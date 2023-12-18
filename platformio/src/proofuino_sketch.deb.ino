#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <InfluxDbClient.h>
#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>


enum Relay {
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

const double TAD = 32.0;
const double TDD = 26.0;
const double OFFSET = 0.5;
const double TDD_OFFSET = 0.5;

class NewState;

class StateFactory {
public:
  static NewState* createStartState(Temperatures temperatures);
  static NewState* createCooldownState(Temperatures temperatures, NewState* previousState);
  static NewState* createHoldOnState(Temperatures temperatures, NewState* previousState);
  static NewState* createHoldOffState(Temperatures temperatures, NewState* previousState);
  static NewState* createBoostOnState(Temperatures temperatures, NewState* previousState);
  static NewState* createBoostOffState(Temperatures temperatures, NewState* previousState);
  static NewState* createErrorState(Temperatures temperatures, NewState* previousState, String reason = "");
};

class NewState {
public:
  Temperatures temperatures;
  Relay desiredRelayState;
  NewState* previousState;
  String state;
  NewState(Temperatures temperatures, NewState* previousState, String state, Relay desiredRelayState)
    : temperatures(temperatures), desiredRelayState(desiredRelayState), previousState(previousState), state(state) {}

  virtual NewState* getNextState(Temperatures temperatures) = 0; // Pure virtual function
};

class StartState : public NewState {
public:
  StartState(Temperatures temperatures): NewState(
    temperatures, nullptr, "START", OFF){}

  NewState* getNextState(Temperatures temperatures) {
    //Cooldown Transistion
    if(temperatures.TDC > 26.2){
      return StateFactory::createCooldownState(temperatures, this);
    }
    // Hold Off
    if(temperatures.TDC >= 26 && temperatures.TDC <= 26.2){
      return StateFactory::createHoldOffState(temperatures, this);
    }
    // Boost On
    if(temperatures.TDC < 26){
      return StateFactory::createBoostOnState(temperatures, this);
    }
    return StateFactory::createErrorState(temperatures, this);
  }
};

class CooldownState : public NewState {
public:
  CooldownState(Temperatures temperatures, NewState* previousState): NewState(
    temperatures, previousState, "COOLDOWN", OFF){}

  NewState* getNextState(Temperatures temperatures) {
    if(temperatures.TDC < 26){
      return StateFactory::createHoldOnState(temperatures, this);
    }
    return StateFactory::createCooldownState(temperatures, this);
  }
};

class HoldOnState : public NewState {
public:
  HoldOnState(Temperatures temperatures, NewState* previousState): NewState(
    temperatures, previousState, "HOLD_ON", ON){}

  NewState* getNextState(Temperatures temperatures) {
    //Cooldown Transistion
    if(temperatures.TDC > 26.2){
      return StateFactory::createCooldownState(temperatures, this);
    }
    //Boost On Transistion
    if(temperatures.TDC < 26){
      return StateFactory::createBoostOnState(temperatures, this);
    }
    if(temperatures.TAC >= 26.2){
      return StateFactory::createHoldOffState(temperatures, this);
    }
    return StateFactory::createHoldOnState(temperatures, this);
  }
};

class HoldOffState : public NewState {
public:
  HoldOffState(Temperatures temperatures, NewState* previousState): NewState(
    temperatures, previousState, "HOLD_OFF", OFF){}

  NewState* getNextState(Temperatures temperatures) {
    //Cooldown Transistion
    if(temperatures.TDC > 26.2){
      return StateFactory::createCooldownState(temperatures, this);
    }
    //Boost On Transistion
    if(temperatures.TDC < 26){
      return StateFactory::createBoostOnState(temperatures, this);
    }
    if(temperatures.TAC <= 25.8){
      return StateFactory::createHoldOnState(temperatures, this);
    }
    return StateFactory::createHoldOffState(temperatures, this);
  }
};

class BoostOnState : public NewState {
public:
  BoostOnState(Temperatures temperatures, NewState* previousState): NewState(
    temperatures, previousState, "BOOST_ON", ON){}

  NewState* getNextState(Temperatures temperatures) {
    //Cooldown Transistion
    // due to boost the ambient temperature will be higher and the dough temperature is likely to rise event if the heating is off
    if(temperatures.TDC > 26){
      return StateFactory::createCooldownState(temperatures, this);
    }
    if(temperatures.TAC >= 32){
      return StateFactory::createBoostOffState(temperatures, this);
    }
    return StateFactory::createBoostOnState(temperatures, this);
  }
};

class BoostOffState : public NewState {
public:
  BoostOffState(Temperatures temperatures, NewState* previousState): NewState(
    temperatures, previousState, "BOOST_OFF", OFF){}

  NewState* getNextState(Temperatures temperatures) {
    //Cooldown Transistion
    // due to boost the ambient temperature will be higher and the dough temperature is likely to rise event if the heating is off
    if(temperatures.TDC > 26){
      return StateFactory::createCooldownState(temperatures, this);
    }
    //Boost On Transistion
    if(temperatures.TAC <= 28){
      return StateFactory::createBoostOnState(temperatures, this);
    }
    return StateFactory::createBoostOffState(temperatures, this);
  }
};

class ErrorState : public NewState {
public:
  String reason;
  ErrorState(Temperatures temperatures, NewState* previousState, String reason = ""): NewState(
    temperatures, previousState, "ERROR", OFF){}

  NewState* getNextState(Temperatures temperatures) {
    return this;
  }
};

NewState* StateFactory::createStartState(Temperatures temperatures) {
  return new StartState(temperatures);
}

NewState* StateFactory::createCooldownState(Temperatures temperatures, NewState* previousState) {
  return new CooldownState(temperatures, previousState);
}

NewState* StateFactory::createHoldOnState(Temperatures temperatures, NewState* previousState) {
  return new HoldOnState(temperatures, previousState);
}

NewState* StateFactory::createHoldOffState(Temperatures temperatures, NewState* previousState) {
  return new HoldOffState(temperatures, previousState);
}

NewState* StateFactory::createBoostOnState(Temperatures temperatures, NewState* previousState) {
  return new BoostOnState(temperatures, previousState);
}

NewState* StateFactory::createBoostOffState(Temperatures temperatures, NewState* previousState) {
  return new BoostOffState(temperatures, previousState);
}

NewState* StateFactory::createErrorState(Temperatures temperatures, NewState* previousState, String reason) {
  return new ErrorState(temperatures, previousState, reason);
}

const int RELAY_PIN = D1;

OneWire oneWire;
DallasTemperature sensors(&oneWire);

InfluxDBClient client("http://192.168.178.47:8086", "brot");

WiFiClient wifiClient;

ESP8266WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

NewState* currentState;

void setup() {
  Serial.begin(115200);

  setupOTA();
  setupWifi();
  setupServer();
  webSocket.begin();

  pinMode(RELAY_PIN, OUTPUT);

  currentState = new StartState(Temperatures(0.0, 0.0));
  writeStateToInfluxDB(currentState);
}

unsigned long previousMillis = 0;

// Higher Order Function für zeitgesteuerte Ausführung
void executeEvery(unsigned long interval, std::function<void()> function)
{
  unsigned long currentMillis = millis();

  if (previousMillis == 0 || currentMillis - previousMillis >= interval)
  {
    // Speichern der aktuellen Zeit für das nächste Intervall
    previousMillis = currentMillis;

    // Aufruf der übergebenen Funktion
    function();
  }
}

unsigned long lastRelayOn = 0;

void turnRelayOff() {
  lastRelayOn = 0;
  digitalWrite(RELAY_PIN, LOW);
}

void turnRelayOn() {
  lastRelayOn = millis();
  digitalWrite(RELAY_PIN, HIGH);
}

Relay getRelayState() {
  return digitalRead(RELAY_PIN) == HIGH ? ON : OFF;
}

const int SECONDS = 1000;
const int MINUTES = 60 * SECONDS;

void loop()
{
  server.handleClient();
  webSocket.loop();
  ArduinoOTA.handle();

  // #### Saftey NET ####
  Temperatures temperatures = readTemperatures();
  if(temperatures.TAC > 38){
    currentState = new ErrorState(temperatures, currentState, "Safety shutdown");
  }
  Relay relayState = getRelayState();
  if(getRelayState() == ON && lastRelayOn != 0 && millis() - lastRelayOn > 10 * MINUTES){
    currentState = new ErrorState(temperatures, currentState, "Safety shutdown");
  }

  sendWebsocket(temperatures, relayState);  

  executeEvery(10 * SECONDS, []() {

    Temperatures temperatures = readTemperatures();
    NewState* newState = currentState->getNextState(temperatures);

    if(newState->state == "ERROR"){
      turnRelayOff();
      if (currentState->state != "ERROR") {
        writeStateToInfluxDB(newState);
      }
      currentState = newState;
      return;
    }
    if (newState->state != currentState->state) {
      if (newState->desiredRelayState == ON) {
        turnRelayOn();
      } else {
        turnRelayOff();
      }
      writeStateToInfluxDB(newState);
      currentState = newState; 
    }
    writeTemperaturesToInfluxDB();

    if(getRelayState() != currentState->desiredRelayState){
      currentState = new ErrorState(readTemperatures(), currentState, "Relay state does not match desired state");
    }
  });

}

void writeTemperaturesToInfluxDB()
{
  Temperatures sensorReadings = readTemperatures();
  Point sensorData("Sensor"); // Measurement name: Sensor
  sensorData.addField("box", sensorReadings.TAC);   // Schreibe TAC in die Spalte "box"
  sensorData.addField("bread", sensorReadings.TDC); // Schreibe TDC in die Spalte "bread"
  client.writePoint(sensorData); // Datenpunkt in die InfluxDB schreiben

  Point powerData("Power");
  powerData.addField("value", getRelayState() == ON ? 1 : 0);
  client.writePoint(powerData);
  client.writePoint(powerData);
}

void writeStateToInfluxDB(NewState* state)
{
  Point stateData("State");
  stateData.addField("state", state->state);
  client.writePoint(stateData);
}

void sendWebsocket(Temperatures temperatures, Relay relayState){
  // Create a JSON document
  DynamicJsonDocument doc(1024);

  // Populate the JSON document
  doc["state"] = currentState->getNextState(temperatures)->state;
  doc["temperature"]["tac"] = temperatures.TAC;
  doc["temperature"]["tdc"] = temperatures.TDC;
  doc["relay"] = relayState == ON ? "ON" : "OFF";
  String message;
serializeJson(doc,message);
  webSocket.broadcastTXT(message);
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
  if (tempTAC != DEVICE_DISCONNECTED_C && tempTDC != DEVICE_DISCONNECTED_C) {
    TDC = tempTDC;
    TAC = tempTAC;
  }
  else {
    currentState = new ErrorState(Temperatures(DEVICE_DISCONNECTED_C, DEVICE_DISCONNECTED_C), currentState, "Temperature sensor disconnected");
  }

  return Temperatures(TAC, TDC);
}

void setupOTA() {
  ArduinoOTA.onEnd([]() {
    ESP.restart();
  });
  ArduinoOTA.begin();
}

void setupWifi() {
  // Create an instance of WiFiManager
  WiFiManager wifiManager;
  // set timeout of autoConnect funtion. will return, no matter if connection is successful or not
  wifiManager.setConfigPortalTimeout(1 * MINUTES);

  // Try to connect to WiFi with saved credentials
  bool connected = wifiManager.autoConnect("ProofuinoAP");
  if (!connected) {
    Serial.println("Failed to connect and hit timeout");
    delay(3000);
    // Reset and try again or do whatever
    ESP.reset();
    delay(5000);
  }

  // If connected, display network information
  Serial.println("Connected to Wi-Fi!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
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
  server.on("/temperature", HTTP_GET, []()
            { server.send(200, "text/plain", String(currentState->temperatures.TAC)); });
  server.begin();
}
