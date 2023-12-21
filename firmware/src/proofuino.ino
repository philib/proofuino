#include "config.h"
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <InfluxDbClient.h>
#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>

enum Relay
{
  ON,
  OFF
};

class Temperatures
{
public:
  float TAC;
  float TDC;

  Temperatures(float tac, float tdc)
      : TAC(tac), TDC(tdc) {}
};

class State;

class StateFactory
{
public:
  static State *createStartState(Temperatures temperatures);
  static State *createCooldownState(Temperatures temperatures);
  static State *createHoldOnState(Temperatures temperatures);
  static State *createHoldOffState(Temperatures temperatures);
  static State *createBoostOnState(Temperatures temperatures);
  static State *createBoostOffState(Temperatures temperatures);
  static State *createErrorState(Temperatures temperatures);
};

class State
{
public:
  Temperatures temperatures;
  Relay desiredRelayState;
  String state;
  virtual ~State() {} // Virtual destructor
  State(Temperatures temperatures, String state, Relay desiredRelayState)
      : temperatures(temperatures), desiredRelayState(desiredRelayState), state(state) {}

  virtual State *getNextState(Temperatures temperatures) = 0; // Pure virtual function
};

class StartState : public State
{
public:
  StartState(Temperatures temperatures) : State(
                                              temperatures, "START", OFF) {}

  State *getNextState(Temperatures temperatures)
  {
    // Cooldown Transistion
    if (temperatures.TDC > 26.2)
    {
      return StateFactory::createCooldownState(temperatures);
    }
    // Hold Off
    if (temperatures.TDC >= 26 && temperatures.TDC <= 26.2)
    {
      return StateFactory::createHoldOffState(temperatures);
    }
    // Boost On
    if (temperatures.TDC < 26)
    {
      return StateFactory::createBoostOnState(temperatures);
    }
    return StateFactory::createErrorState(temperatures);
  }
};

class CooldownState : public State
{
public:
  CooldownState(Temperatures temperatures) : State(
                                                 temperatures, "COOLDOWN", OFF) {}

  State *getNextState(Temperatures temperatures)
  {
    if (temperatures.TDC < 26)
    {
      return StateFactory::createHoldOnState(temperatures);
    }
    return StateFactory::createCooldownState(temperatures);
  }
};

class HoldOnState : public State
{
public:
  HoldOnState(Temperatures temperatures) : State(
                                               temperatures, "HOLD_ON", ON) {}

  State *getNextState(Temperatures temperatures)
  {
    // Cooldown Transistion
    if (temperatures.TDC > 26.2)
    {
      return StateFactory::createCooldownState(temperatures);
    }
    // Boost On Transistion
    if (temperatures.TDC < 25.8)
    {
      return StateFactory::createBoostOnState(temperatures);
    }
    if (temperatures.TAC >= 26.2)
    {
      return StateFactory::createHoldOffState(temperatures);
    }
    return StateFactory::createHoldOnState(temperatures);
  }
};

class HoldOffState : public State
{
public:
  HoldOffState(Temperatures temperatures) : State(
                                                temperatures, "HOLD_OFF", OFF) {}

  State *getNextState(Temperatures temperatures)
  {
    // Cooldown Transistion
    if (temperatures.TDC > 26.2)
    {
      return StateFactory::createCooldownState(temperatures);
    }
    // Boost On Transistion
    if (temperatures.TDC < 26)
    {
      return StateFactory::createBoostOnState(temperatures);
    }
    if (temperatures.TAC <= 25.8)
    {
      return StateFactory::createHoldOnState(temperatures);
    }
    return StateFactory::createHoldOffState(temperatures);
  }
};

class BoostOnState : public State
{
public:
  BoostOnState(Temperatures temperatures) : State(
                                                temperatures, "BOOST_ON", ON) {}

  State *getNextState(Temperatures temperatures)
  {
    // Cooldown Transistion
    //  due to boost the ambient temperature will be higher and the dough temperature is likely to rise event if the heating is off
    if (temperatures.TDC > 26)
    {
      return StateFactory::createCooldownState(temperatures);
    }
    if (temperatures.TAC >= 32)
    {
      return StateFactory::createBoostOffState(temperatures);
    }
    return StateFactory::createBoostOnState(temperatures);
  }
};

class BoostOffState : public State
{
public:
  BoostOffState(Temperatures temperatures) : State(
                                                 temperatures, "BOOST_OFF", OFF) {}

  State *getNextState(Temperatures temperatures)
  {
    // Cooldown Transistion
    //  due to boost the ambient temperature will be higher and the dough temperature is likely to rise event if the heating is off
    if (temperatures.TDC > 26)
    {
      return StateFactory::createCooldownState(temperatures);
    }
    // Boost On Transistion
    if (temperatures.TAC <= 28)
    {
      return StateFactory::createBoostOnState(temperatures);
    }
    return StateFactory::createBoostOffState(temperatures);
  }
};

class ErrorState : public State
{
public:
  ErrorState(Temperatures temperatures) : State(
                                              temperatures, "ERROR", OFF) {}

  State *getNextState(Temperatures temperatures)
  {
    return this;
  }
};

State *StateFactory::createStartState(Temperatures temperatures)
{
  return new StartState(temperatures);
}

State *StateFactory::createCooldownState(Temperatures temperatures)
{
  return new CooldownState(temperatures);
}

State *StateFactory::createHoldOnState(Temperatures temperatures)
{
  return new HoldOnState(temperatures);
}

State *StateFactory::createHoldOffState(Temperatures temperatures)
{
  return new HoldOffState(temperatures);
}

State *StateFactory::createBoostOnState(Temperatures temperatures)
{
  return new BoostOnState(temperatures);
}

State *StateFactory::createBoostOffState(Temperatures temperatures)
{
  return new BoostOffState(temperatures);
}

State *StateFactory::createErrorState(Temperatures temperatures)
{
  return new ErrorState(temperatures);
}

const int RELAY_PIN = D1;

OneWire oneWire;
DallasTemperature sensors(&oneWire);

InfluxDBClient client(INFLUXDB_URL, DATABASE);

WiFiClient wifiClient;

ESP8266WebServer server(80);

State *currentState;
WiFiManager wifiManager;

unsigned long previousMillis = 0;

void executeEvery(unsigned long interval, std::function<void()> function)
{
  unsigned long currentMillis = millis();

  if (previousMillis == 0 || currentMillis - previousMillis >= interval)
  {
    previousMillis = currentMillis;
    function();
  }
}

void onConnection(std::function<void()> function)
{
  if (WiFi.status() == WL_CONNECTED)
  {
    function();
  }
}

unsigned long lastRelayOn = 0;

void turnRelayOff()
{
  lastRelayOn = 0;
  digitalWrite(RELAY_PIN, LOW);
}

void turnRelayOn()
{
  if (getRelayState() == OFF)
  {
    lastRelayOn = millis();
  }
  digitalWrite(RELAY_PIN, HIGH);
}

Relay getRelayState()
{
  return digitalRead(RELAY_PIN) == HIGH ? ON : OFF;
}

void updateState(State *newState)
{
  if (currentState == nullptr || newState->state != currentState->state)
  {
    if (currentState == nullptr)
    {
      Serial.println("Starting");
    }
    else
    {
      Serial.println("From: " + currentState->state + " To: " + newState->state);
    }
    writeStateToInfluxDB(newState);
  }
  if (currentState != nullptr)
  {
    delete currentState;
  }
  currentState = newState;
}

const int SECONDS = 1000;
const int MINUTES = 60 * SECONDS;

void loop()
{
  wifiManager.process();
  onConnection([]()
               {
    ArduinoOTA.handle();
    server.handleClient(); });

  executeEvery(10 * SECONDS, []()
               {
    Temperatures temperatures = readTemperatures();
    Relay relayState = getRelayState();

    bool isTemperatureToHigh = temperatures.TAC > 38;
    bool isRelayOnForTooLong = relayState == ON && lastRelayOn != 0 && millis() - lastRelayOn > 10 * MINUTES;
    if(isTemperatureToHigh || isRelayOnForTooLong){
      updateState(new ErrorState(temperatures));
      isTemperatureToHigh ? writeError("Temperature too high") : writeError("Relay on for too long");
      return;
    }

    updateState(currentState->getNextState(temperatures));
    Serial.println("State: " + currentState->state + " | Relay: " + (relayState == ON ? "ON" : "OFF") + " | TAC: " + temperatures.TAC + " | TDC: " + temperatures.TDC);

    if(currentState->state == "ERROR"){
      turnRelayOff();
      return;
    }
    if (currentState->desiredRelayState == ON) {
      turnRelayOn();
    } else {
      turnRelayOff();
    }

    writeTemperaturesToInfluxDB(currentState); });
}

void writeError(String reason)
{
  Serial.println("Error: " + reason);
  onConnection([&reason]()
               {
                 Point errorData("Error");
                 errorData.addField("reason", reason);
                 client.writePoint(errorData); });
}

void writeTemperaturesToInfluxDB(State *state)
{
  onConnection([&state]()
               {
    Point sensorData("Sensor");
    sensorData.addField("box", state->temperatures.TAC);
    sensorData.addField("bread", state->temperatures.TDC);
    client.writePoint(sensorData);

    Point powerData("Power");
    powerData.addField("value", getRelayState() == ON ? 1 : 0);
    client.writePoint(powerData);
    client.writePoint(powerData); });
}

void writeStateToInfluxDB(State *state)
{
  onConnection([&state]()
               {
    Point stateData("State");
    stateData.addField("state", state->state);
    client.writePoint(stateData); });
}

Temperatures readTemperatures()
{
  oneWire.begin(D2);
  sensors.begin();

  float TDC = 0.0;
  float TAC = 0.0;

  sensors.requestTemperatures();

  float tempTAC = sensors.getTempCByIndex(0);
  float tempTDC = sensors.getTempCByIndex(1);
  if (tempTAC != DEVICE_DISCONNECTED_C && tempTDC != DEVICE_DISCONNECTED_C)
  {
    TDC = tempTDC;
    TAC = tempTAC;
  }
  else
  {
    updateState(new ErrorState(Temperatures(DEVICE_DISCONNECTED_C, DEVICE_DISCONNECTED_C)));
    writeError("Temperature sensor disconnected");
  }

  return Temperatures(TAC, TDC);
}

void setupOTA()
{
  ArduinoOTA.onEnd([]()
                   { ESP.restart(); });
  ArduinoOTA.begin();
}

void setupWifi()
{
  // wifiManager.resetSettings(); // for debugging
  wifiManager.setConfigPortalBlocking(false);
  wifiManager.setConfigPortalTimeout(60);
  wifiManager.autoConnect("ProofuinoAP");
  MDNS.begin("proofuino");
}

void setupServer()
{
  if (!LittleFS.begin())
  {
    Serial.println("Failed to mount LittleFS");
    return;
  }
  server.on("/", HTTP_GET, []()
            {
    File file = LittleFS.open("/index.html", "r");
    if (!file) {
      Serial.println("Failed to open file");
      server.send(404, "text/plain", "File not found");
      return;
    }
    server.streamFile(file, "text/html");
    file.close(); });
  server.on("/status", HTTP_GET, []()
            { 
    String response = "State: " + currentState->state + "\nHeatmat: " + (getRelayState() == ON ? "ON" : "OFF") + "\nBox: " + currentState->temperatures.TAC + "\nDough: " + currentState->temperatures.TDC;
    server.send(200, "text/plain", response); });
  server.begin();
}

void setup()
{
  Serial.begin(115200);

  setupOTA();
  setupWifi();
  setupServer();

  pinMode(RELAY_PIN, OUTPUT);

  updateState(new StartState(Temperatures(0.0, 0.0)));
}