#include <WiFi.h>
#include <PubSubClient.h>
#include "DHT20.h"
#include "Wire.h"
#include <ArduinoJson.h>

#define LED_PIN 2 
bool ledState = false;

const char* WIFI_SSID = "LamCoffee";
const char* WIFI_PASS = "999999999";

const char* MQTT_SERVER = "app.coreiot.io";
const int MQTT_PORT = 1883;
const char* ACCESS_TOKEN = "YWbAVwOqlX1AUG3CHBgy";

DHT20 dht20;
WiFiClient espClient;
PubSubClient client(espClient);

bool wifiConnected = false;
bool mqttConnected = false;

void InitWiFi() {
  Serial.print("Connecting to WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Wifi connected!");
}

const bool reconnect() {
  const wl_status_t status = WiFi.status();
  if (status == WL_CONNECTED) {
    return true;
  }
  InitWiFi();
  return true;
}

void wifiTask(void *pvParameters) {
  while (1) {
    wifiConnected = reconnect();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  String messageTemp;

  for (unsigned int i = 0; i < length; i++) {
    messageTemp += (char)payload[i];
  }

  Serial.print("Received [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(messageTemp);

  if (String(topic) == "v1/devices/me/attributes") {
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, messageTemp);

    if (!error && doc.containsKey("led")) {
      ledState = doc["led"];
      digitalWrite(LED_PIN, ledState ? HIGH : LOW);

      // Gửi trạng thái đèn hiện tại về telemetry
      String response = "{\"led\":" + String(ledState ? "true" : "false") + "}";
      client.publish("v1/devices/me/telemetry", response.c_str());

      Serial.println("LED state updated from server: " + String(ledState));
    }
  }
}

void serverTask(void *pvParameters) {
  while (1) {
    if (!wifiConnected) {
      mqttConnected = false;
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }

    if (!client.connected()) {
      mqttConnected = false;
      Serial.print("Connecting to server...");
      client.setServer(MQTT_SERVER, MQTT_PORT);
      client.setCallback(callback);
      if (!client.connect("ESP32_Client", ACCESS_TOKEN, NULL)) {
        Serial.println("Failed to connect");
        vTaskDelay(pdMS_TO_TICKS(2000));
        continue;
      }
      Serial.println("MQTT connected!");
      client.subscribe("v1/devices/me/attributes"); 
      mqttConnected = true;
    }
    client.loop();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void SensorTask(void *pvParameters) {
  Wire.begin();
  dht20.begin();
  while (1) {
    if (!mqttConnected) {
      vTaskDelay(pdMS_TO_TICKS(10000));
      continue;
    }
    dht20.read();
    float temp = dht20.getTemperature();
    float humi = dht20.getHumidity();
    if (isnan(temp) || isnan(humi)) {
      Serial.println("Failed to read from DHT20 sensor!");
    } else {
      Serial.print("Temperature: ");
      Serial.print(temp);
      Serial.print(" °C, Humidity: ");
      Serial.print(humi);
      Serial.println(" %");
      String payload = "{\"temperature\":" + String(temp) + ", \"humidity\":" + String(humi) + "}";
      if (client.publish("v1/devices/me/telemetry", payload.c_str())) {
        Serial.println("Data sent: " + payload);
      } else {
        Serial.println("Failed to send data!");
      }
    }
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  InitWiFi();

  xTaskCreate(wifiTask, "WiFi Task", 2048, NULL, 2, NULL);
  xTaskCreate(serverTask, "Server Task", 4096, NULL, 2, NULL);
  xTaskCreate(SensorTask, "Sensor Task", 4096, NULL, 2, NULL);
}

void loop() {
  
}