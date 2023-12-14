#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <InfluxDbClient.h>
#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>
#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>

enum State {
  START,
  HOLD_ON,
  HOLD_OFF,
  BOOST_ON,
  BOOST_OFF,
  ERROR
};

class Temperatures {
  public:
    float TAC;
    float TDC;

    Temperatures(float tac, float tdc)
      : TAC(tac), TDC(tdc) {}
};

OneWire oneWire;
DallasTemperature sensors(&oneWire);

InfluxDBClient client("http://192.168.178.46:8086", "brot");

WiFiClient wifiClient;
Adafruit_MQTT_Client mqtt(&wifiClient, "192.168.178.46", 1234);
Adafruit_MQTT_Publish mqttPublish = Adafruit_MQTT_Publish(&mqtt, "cmnd/tasmota_417AA1/power");

ESP8266WebServer server(80);

const double TAD = 32.0;
const double TDD = 26.0;
State currentState = START;
const double OFFSET = 0.5;
Temperatures temperatures = Temperatures(0.0, 0.0);

void setup() {
  Serial.begin(115200);
  ArduinoOTA.begin();
  setupWifi();
  setupMqtt();
  setupServer();
}

void loop() {
  server.handleClient();
  ArduinoOTA.handle();
  executeEvery(10000, []() {
    temperatures = readTemperatures(D2);
    State newState = getState(currentState, temperatures.TAC, TAD, temperatures.TDC, TDD, OFFSET);

    if (newState != currentState) {
      sendMQTTMessage(newState);
      writeStateToInfluxDB(newState);
    }
    writeTemperaturesToInfluxDB(temperatures.TAC, temperatures.TDC, newState);

    currentState = newState;
  });
}

// Funktion zur Bestimmung des nächsten Zustands basierend auf den gegebenen Parametern
State getState(State currentState, float TAC, float TAD, float TDC, float TDD, float Offset) {
  // Wenn der aktuelle Zustand "START" ist
  if (currentState == START) {
    if (TDC < (TDD - Offset)) {
      return BOOST_ON;
    } else if (TDC >= (TDD - Offset) && TDC < TDD) {
      return HOLD_ON;
    } else if (TDC >= (TDD - Offset) && TDC > TDD) {
      return HOLD_OFF;
    }
  }
  // Wenn der aktuelle Zustand "HOLD_ON" oder "HOLD_OFF" ist
  else if (currentState == HOLD_ON || currentState == HOLD_OFF) {
    if (TAC >= (TDD + Offset)) {
      return HOLD_OFF;
    } else if (TAC <= (TDD - Offset)) {
      return HOLD_ON;
    } else if (TDC < (TDD - Offset)) {
      return BOOST_ON;
    }
  }
  // Wenn der aktuelle Zustand "BOOST_ON" oder "BOOST_OFF" ist
  else if (currentState == BOOST_ON || currentState == BOOST_OFF) {
    if (TAC >= (TAD + Offset)) {
      return BOOST_OFF;
    } else if (TAC <= (TAD - Offset)) {
      return BOOST_ON;
    } else if (TDC >= (TDD + Offset)) {
      return HOLD_OFF;
    }
  }
  // Rückkehr des aktuellen Zustands, wenn keine Bedingung erfüllt ist
  return currentState;
}

void sendMQTTMessage(State state) {
  if (!mqtt.connected()) {
    mqtt.disconnect();
    delay(1000);
    mqtt.connect();
  }

  if (mqtt.connected()) {
    if (state == HOLD_ON || state == BOOST_ON) {
      const char* message = "ON";
      mqttPublish.publish(message);
    } else {
      const char* message = "OFF";
      mqttPublish.publish(message);
    }
    Serial.println("Message sent!");
  } else {
    Serial.println("MQTT not connected!");
  }
}

void writeTemperaturesToInfluxDB(float TAC, float TDC, State state) {
  Point sensorData("Sensor");  // Measurement name: Sensor

  // Schreiben der Daten in die InfluxDB
  sensorData.addField("box", TAC);    // Schreibe TAC in die Spalte "box"
  sensorData.addField("bread", TDC);  // Schreibe TDC in die Spalte "bread"

  client.writePoint(sensorData);  // Datenpunkt in die InfluxDB schreiben

  Point powerData("Power");
  if (state == BOOST_ON || state == HOLD_ON) {
    powerData.addField("value", 1);
    client.writePoint(powerData);
  } else {
    powerData.addField("value", 0);
  }
  client.writePoint(powerData);
}

void writeStateToInfluxDB(State state) {
  Point stateData("State");
  stateData.addField("state", stateToString(state));
  client.writePoint(stateData);
}

// Funktion zur Konvertierung des State Enums in einen String
String stateToString(State state) {
  switch (state) {
    case START:
      return "START";
    case HOLD_ON:
      return "HOLD_ON";
    case HOLD_OFF:
      return "HOLD_OFF";
    case BOOST_ON:
      return "BOOST_ON";
    case BOOST_OFF:
      return "BOOST_OFF";
    case ERROR:
      return "ERROR";
    default:
      return "UNKNOWN_STATE";  // Für den Fall, dass ein unbekannter Zustand übergeben wird
  }
}

Temperatures readTemperatures(int pin) {
  oneWire.begin(pin);
  sensors.begin();

  float TDC = 0.0;
  float TAC = 0.0;

  sensors.requestTemperatures();

  float tempTAC = sensors.getTempCByIndex(0);
  float tempTDC = sensors.getTempCByIndex(1);
  if (tempTAC != DEVICE_DISCONNECTED_C && tempTDC != DEVICE_DISCONNECTED_C) {
    TDC = tempTDC;
    TAC = tempTAC;
    if (currentState == ERROR) {
      currentState = START;
    }
  } else {
    currentState = ERROR;
  }

  return Temperatures(TAC, TDC);
}

void setupWifi() {
  // Create an instance of WiFiManager
  WiFiManager wifiManager;

  // Uncomment the following line for debug output
  // wifiManager.setDebugOutput(true);

  // Try to connect to WiFi with saved credentials
  if (!wifiManager.autoConnect("ProofuinoAP")) {
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

void setupServer() {
  if (!LittleFS.begin()) {
    Serial.println("Failed to mount LittleFS");
    return;
  }
  server.on("/", HTTP_GET, []() {
    File file = LittleFS.open("/index.html", "r");
    if (!file) {
      Serial.println("Failed to open file");
      server.send(404, "text/plain", "File not found");
      return;
    }
    server.streamFile(file, "text/html");
    file.close();
  });
  server.on("/temperature", HTTP_GET, []() {
    server.send(200, "text/plain", String(temperatures.TAC));
  });
  server.begin();
}

void setupMqtt() {
  if (!mqtt.connected()) {
    mqtt.disconnect();
    delay(1000);
    mqtt.connect();
  }

  if (!mqtt.connected()) {
    Serial.println("Failed to connect to MQTT. Check server and credentials");
  } else {
    Serial.println("Connected to MQTT broker");
  }
}

unsigned long previousMillis = 0;

// Higher Order Function für zeitgesteuerte Ausführung
void executeEvery(unsigned long interval, void (*function)()) {
  unsigned long currentMillis = millis();

  if (previousMillis == 0 || currentMillis - previousMillis >= interval) {
    // Speichern der aktuellen Zeit für das nächste Intervall
    previousMillis = currentMillis;

    // Aufruf der übergebenen Funktion
    (*function)();
  }
}
