#include "config.h"
#include "StateManager.h"
#include "RelayManager.h"
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <InfluxDbClient.h>
#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

StateManager *stateManager;
RelayManager *relayManager;
OneWire oneWire;
DallasTemperature sensors(&oneWire);
InfluxDBClient client(INFLUXDB_URL, DATABASE);
WiFiClient wifiClient;
ESP8266WebServer server(80);
WiFiManager wifiManager;

const int SECONDS = 1000;
const int MINUTES = 60 * SECONDS;

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

Temperatures readTemperatures(std::function<void()> onError)
{
  oneWire.begin(D2);
  sensors.begin();

  float dough = 0.0;
  float box = 0.0;

  sensors.requestTemperatures();

  float tempTAC = sensors.getTempCByIndex(0);
  float tempTDC = sensors.getTempCByIndex(1);
  if (tempTAC != DEVICE_DISCONNECTED_C && tempTDC != DEVICE_DISCONNECTED_C)
  {
    dough = tempTDC;
    box = tempTAC;
  }
  else
  {
    onError();
  }
  return Temperatures(box, dough);
}

void loop()
{
  wifiManager.process();
  onConnection([]()
               {
    ArduinoOTA.handle();
    server.handleClient(); });

  executeEvery(10 * SECONDS, []()
               {
                 std::function<void()> onError = []()
                 {
                   stateManager->setErrorState("Error reading temperatures");
                 };
                 Temperatures temperatures = readTemperatures(onError);
                 logSensorData(temperatures);
                 stateManager->process(temperatures); });
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

void logSensorData(Temperatures temperatures)
{
  onConnection([&temperatures]()
               {
    Point sensorData("Sensor");
    sensorData.addField("box", temperatures.box.getValue());
    sensorData.addField("bread", temperatures.dough.getValue());
    client.writePoint(sensorData);

    Point powerData("Power");
    powerData.addField("value", relayManager->getRelayState() == RelayManager::State::ON ? 1 : 0);
    client.writePoint(powerData);
    client.writePoint(powerData); });
}

void logState(String state)
{
  onConnection([&state]()
               {
    Point stateData("StateManagerState");
    stateData.addField("state", state);
    client.writePoint(stateData); });
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
  server.on("/status", HTTP_GET, []()
            { 
              StaticJsonDocument<200> jsonDoc;
              jsonDoc["state"] = stateManager->getStateStringified();

              jsonDoc["config"]["targetTemperature"] = stateManager->getDesiredDoughTemperature();

              jsonDoc["sensors"]["relay"] = (relayManager->getRelayState() == RelayManager::State::ON ? "ON" : "OFF");
              jsonDoc["sensors"]["temperatures"]["box"] = stateManager->getTemperatures().box.getValue();
              jsonDoc["sensors"]["temperatures"]["dough"] = stateManager->getTemperatures().dough.getValue();
              String response;
              serializeJson(jsonDoc, response);
              server.send(200, "application/json", response); });

  server.on("/config", HTTP_POST, []()
            {
                String body = server.arg("plain");
                StaticJsonDocument<200> jsonDoc;
                deserializeJson(jsonDoc, body);
                stateManager->setDesiredDoughTemperature(jsonDoc["targetTemperature"]);
                server.send(200, "application/json", ""); });

  server.on("/on", HTTP_POST, []()
            {
                  stateManager->restart();
                server.send(200, "application/json", ""); });
  server.on("/off", HTTP_POST, []()
            {
                  stateManager->pause();
                server.send(200, "application/json", ""); });
  // must be at the end to not override other routes
  server.serveStatic("/", LittleFS, "/");
  server.begin();
}

void setup()
{
  Serial.begin(115200);

  setupOTA();
  setupWifi();
  setupServer();

  std::function<void(Relay, String)> onStateChange = [](Relay newRelayState, String state)
  {
    logState(state);
    if (newRelayState == ON)
    {
      relayManager->turnOn();
    }
    else
    {
      relayManager->turnOff();
    } };

  std::function<void(String)> onError = [](String error)
  {
    writeError(error);
  };

  float desiredDoughTemperature = 26.0;
  relayManager = new RelayManager(D1);
  stateManager = new StateManager(desiredDoughTemperature, onStateChange,
                                  onError);
}