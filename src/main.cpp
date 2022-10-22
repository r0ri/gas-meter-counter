#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <secrets.h>

WiFiClient espClient;
PubSubClient client(espClient);

const char* ssid = SECRET_SSID;  // Enter your passwords in the secrets.h file. You can use the secrets.h.template file for this purpose.
const char* password = SECRET_PASS;
const char* mqttServer = "192.168.178.69";
const unsigned int mqttPort = 1883;
const char* mqttUser = SECRET_MQTT_USER;
const char* mqttPass = SECRET_MQTT_PASS;

String mqttClientId = "iot-GasReed";
const int capacity = JSON_OBJECT_SIZE(1);

const unsigned int reedPin = 13; // D7
const unsigned int ledPin = 12; // D6

unsigned int meterCount = 0;

int reedState;
int statCounter; 
int ledState = HIGH;
int lastReedState = LOW;

// the following variables are unsigned longs because the time, measured in
// milliseconds, will quickly become a bigger number than can be stored in an int.
unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled
unsigned long debounceDelay = 50;    // the debounce time; increase if the output flickers

void initWifi(){  // Start WiFi-Connection
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
}

void publish(int count) {
  StaticJsonDocument<capacity> doc;
  doc["count"] = count;
  char out[128];
  serializeJson(doc, out);
  client.publish("test/gasmeter/accumulated", out, true);
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    if (!client.connect(mqttClientId.c_str(), mqttUser, mqttPass)) {
      delay(500);
    }
  }
}

void initialConnectAndSub() {
  // Loop until we're reconnected
  while (!client.connected()) {
    if (client.connect(mqttClientId.c_str(), mqttUser, mqttPass)) {
      client.subscribe("test/gasmeter/accumulated");
    } else {
      delay(500);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  if (strcmp(topic,"test/gasmeter/accumulated")==0) {
    String meterCountStr;
    StaticJsonDocument<capacity> doc;
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    for (int i=0;i<length;i++) {
      meterCountStr += (char)payload[i];
      Serial.print((char)payload[i]);
    }
    Serial.println();
    deserializeJson(doc, meterCountStr);
    meterCount = doc["count"];
    client.unsubscribe("test/gasmeter/accumulated");
    Serial.printf("Set initial value of meterCount to %d.\n", meterCount);
  }
}

void setup() {
  Serial.begin(115200);
  initWifi();
  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);
  Serial.print("RSSI: ");
  Serial.println(WiFi.RSSI());

  initialConnectAndSub();

  pinMode(reedPin, INPUT);
  pinMode(ledPin, OUTPUT);

  digitalWrite(ledPin, ledState);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  int reading = digitalRead(reedPin);
  if (reading != lastReedState) {
    // reset the debouncing timer
    lastDebounceTime = millis();
  }
  if ((millis() - lastDebounceTime) > debounceDelay) {
    // whatever the reading is at, it's been there for longer than the debounce
    // delay, so take it as the actual current state:

    // if the button state has changed:
    if (reading != reedState) {
      reedState = reading;

      // only toggle the LED if the new button state is HIGH
      if (reedState == HIGH) {
        ledState = !ledState;
        meterCount++;
        // Serial.printf("Counter: %d \n", meterCount);
        publish(meterCount);
        // client.publish("test/gasmeter/accumulated", String(meterCount).c_str(), true);
        int publishResult = client.publish("test/gasmeter/pulse", String(1).c_str());
        if (!publishResult) {
          Serial.println("Publish to MQTT failed");
        }
      }
    }
  }

  // set the LED:
  digitalWrite(ledPin, ledState);

  // save the reading. Next time through the loop, it'll be the lastReedState:
  lastReedState = reading;
}