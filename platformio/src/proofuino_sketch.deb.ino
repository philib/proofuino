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
  static NewState* createCooldownState(Temperatures temperatures);
  static NewState* createHoldOnState(Temperatures temperatures);
  static NewState* createHoldOffState(Temperatures temperatures);
  static NewState* createBoostOnState(Temperatures temperatures);
  static NewState* createBoostOffState(Temperatures temperatures);
  static NewState* createErrorState(Temperatures temperatures);
};

class NewState {
public:
  Temperatures temperatures;
  Relay desiredRelayState;
  String state;
  virtual ~NewState() {}  // Virtual destructor
  NewState(Temperatures temperatures, String state, Relay desiredRelayState)
    : temperatures(temperatures), desiredRelayState(desiredRelayState), state(state) {}

  virtual NewState* getNextState(Temperatures temperatures) = 0; // Pure virtual function
};

class StartState : public NewState {
public:
  StartState(Temperatures temperatures): NewState(
    temperatures, "START", OFF){}

  NewState* getNextState(Temperatures temperatures) {
    //Cooldown Transistion
    if(temperatures.TDC > 26.2){
      return StateFactory::createCooldownState(temperatures);
    }
    // Hold Off
    if(temperatures.TDC >= 26 && temperatures.TDC <= 26.2){
      return StateFactory::createHoldOffState(temperatures);
    }
    // Boost On
    if(temperatures.TDC < 26){
      return StateFactory::createBoostOnState(temperatures);
    }
    return StateFactory::createErrorState(temperatures);
  }
};

class CooldownState : public NewState {
public:
  CooldownState(Temperatures temperatures): NewState(
    temperatures, "COOLDOWN", OFF){}

  NewState* getNextState(Temperatures temperatures) {
    if(temperatures.TDC < 26){
      return StateFactory::createHoldOnState(temperatures);
    }
    return StateFactory::createCooldownState(temperatures);
  }
};

class HoldOnState : public NewState {
public:
  HoldOnState(Temperatures temperatures): NewState(
    temperatures, "HOLD_ON", ON){}

  NewState* getNextState(Temperatures temperatures) {
    //Cooldown Transistion
    if(temperatures.TDC > 26.2){
      return StateFactory::createCooldownState(temperatures);
    }
    //Boost On Transistion
    if(temperatures.TDC < 25.8){
      return StateFactory::createBoostOnState(temperatures);
    }
    if(temperatures.TAC >= 26.2){
      return StateFactory::createHoldOffState(temperatures);
    }
    return StateFactory::createHoldOnState(temperatures);
  }
};

class HoldOffState : public NewState {
public:
  HoldOffState(Temperatures temperatures): NewState(
    temperatures, "HOLD_OFF", OFF){}

  NewState* getNextState(Temperatures temperatures) {
    //Cooldown Transistion
    if(temperatures.TDC > 26.2){
      return StateFactory::createCooldownState(temperatures);
    }
    //Boost On Transistion
    if(temperatures.TDC < 26){
      return StateFactory::createBoostOnState(temperatures);
    }
    if(temperatures.TAC <= 25.8){
      return StateFactory::createHoldOnState(temperatures);
    }
    return StateFactory::createHoldOffState(temperatures);
  }
};

class BoostOnState : public NewState {
public:
  BoostOnState(Temperatures temperatures): NewState(
    temperatures, "BOOST_ON", ON){}

  NewState* getNextState(Temperatures temperatures) {
    //Cooldown Transistion
    // due to boost the ambient temperature will be higher and the dough temperature is likely to rise event if the heating is off
    if(temperatures.TDC > 26){
      return StateFactory::createCooldownState(temperatures);
    }
    if(temperatures.TAC >= 32){
      return StateFactory::createBoostOffState(temperatures);
    }
    return StateFactory::createBoostOnState(temperatures);
  }
};

class BoostOffState : public NewState {
public:
  BoostOffState(Temperatures temperatures): NewState(
    temperatures, "BOOST_OFF", OFF){}

  NewState* getNextState(Temperatures temperatures) {
    //Cooldown Transistion
    // due to boost the ambient temperature will be higher and the dough temperature is likely to rise event if the heating is off
    if(temperatures.TDC > 26){
      return StateFactory::createCooldownState(temperatures);
    }
    //Boost On Transistion
    if(temperatures.TAC <= 28){
      return StateFactory::createBoostOnState(temperatures);
    }
    return StateFactory::createBoostOffState(temperatures);
  }
};

class ErrorState : public NewState {
public:
  ErrorState(Temperatures temperatures): NewState(
    temperatures, "ERROR", OFF){}

  NewState* getNextState(Temperatures temperatures) {
    return this;
  }
};

NewState* StateFactory::createStartState(Temperatures temperatures) {
  return new StartState(temperatures);
}

NewState* StateFactory::createCooldownState(Temperatures temperatures) {
  return new CooldownState(temperatures);
}

NewState* StateFactory::createHoldOnState(Temperatures temperatures) {
  return new HoldOnState(temperatures);
}

NewState* StateFactory::createHoldOffState(Temperatures temperatures) {
  return new HoldOffState(temperatures);
}

NewState* StateFactory::createBoostOnState(Temperatures temperatures) {
  return new BoostOnState(temperatures);
}

NewState* StateFactory::createBoostOffState(Temperatures temperatures) {
  return new BoostOffState(temperatures);
}

NewState* StateFactory::createErrorState(Temperatures temperatures) {
  return new ErrorState(temperatures);
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

  updateState(new StartState(Temperatures(0.0, 0.0)));
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

void onConnection(std::function<void()> function)
{
  if (WiFi.status() == WL_CONNECTED)
  {
    function();
  }
  else
  {
    Serial.println("Not connected to WiFi");
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

void updateState(NewState* newState){
  if(currentState == nullptr || newState->state != currentState->state){
    if(currentState == nullptr){
      Serial.println("Starting");
    } else {
      Serial.println("From: " + currentState->state + " To: " + newState->state);
    }
    writeStateToInfluxDB(newState);
  }
  if(currentState != nullptr){
    delete currentState;
  }
  currentState = newState;
}

const int SECONDS = 1000;
const int MINUTES = 60 * SECONDS;

void loop()
{
  onConnection([]()
               {
    ArduinoOTA.handle();
    webSocket.loop();
    server.handleClient(); });

  // #### Saftey NET ####
  Temperatures temperatures = readTemperatures();
  if(temperatures.TAC > 38){
    updateState(new ErrorState(temperatures));
    writeError("Temperature too high");
  }
  Relay relayState = getRelayState();
  if(getRelayState() == ON && lastRelayOn != 0 && millis() - lastRelayOn > 10 * MINUTES){
    updateState(new ErrorState(temperatures));
    writeError("Relay on for too long");
  }

  // sendWebsocket(temperatures, relayState);  

  executeEvery(10 * SECONDS, []() {

    updateState(currentState->getNextState(readTemperatures()));

    if(currentState->state == "ERROR"){
      turnRelayOff();
      return;
    }
    if (currentState->desiredRelayState == ON) {
      turnRelayOn();
    } else {
      turnRelayOff();
    }

    writeTemperaturesToInfluxDB(currentState);

    if(getRelayState() != currentState->desiredRelayState){
      updateState(new ErrorState(readTemperatures()));
      writeError("Relay state does not match desired state");
    }
  });

}

void writeError(String reason){
  Serial.println("Error: " + reason);
  onConnection([&reason]() {

    Point errorData("Error");
    errorData.addField("reason", reason);
    client.writePoint(errorData); 

  });
}

void writeTemperaturesToInfluxDB(NewState* state)
{
  onConnection([&state]() {

    Point sensorData("Sensor");                            // Measurement name: Sensor
    sensorData.addField("box", state->temperatures.TAC);   // Schreibe TAC in die Spalte "box"
    sensorData.addField("bread", state->temperatures.TDC); // Schreibe TDC in die Spalte "bread"
    client.writePoint(sensorData);                         // Datenpunkt in die InfluxDB schreiben

    Point powerData("Power");
    powerData.addField("value", getRelayState() == ON ? 1 : 0);
    client.writePoint(powerData);
    client.writePoint(powerData); 
    
  });
}

void writeStateToInfluxDB(NewState* state)
{
  onConnection([&state]() {

    Point stateData("State");
    stateData.addField("state", state->state);
    client.writePoint(stateData);

  });
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
    updateState(new ErrorState(Temperatures(DEVICE_DISCONNECTED_C, DEVICE_DISCONNECTED_C)));
    writeError("Temperature sensor disconnected");
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
