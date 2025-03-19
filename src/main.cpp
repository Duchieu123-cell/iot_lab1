#include <WiFi.h>
#include <PubSubClient.h>
#include "DHT20.h"
#include "Wire.h"

const char* WIFI_SSID = "ACLAB-IOT";
const char* WIFI_PASS = "12345678";

const char* MQTT_SERVER = "app.coreiot.io";
const int MQTT_PORT = 1883;
const char* ACCESS_TOKEN = "8qixx7tv7gqd0iw800vz";  

DHT20 dht20;
WiFiClient espClient;
PubSubClient client(espClient);

bool wifiConnected = false;
bool mqttConnected = false;

void InitWiFi() {
  Serial.print("Connecting to WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) 
  {
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
  while(1) {
    wifiConnected = reconnect();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void serverTask(void *pvParameters) {
  while(1)
  {
    if (!wifiConnected) 
    {
      mqttConnected = false;
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }

    if (!client.connected()) 
    {
      mqttConnected = false;
      Serial.print("Connecting to server...");
      client.setServer(MQTT_SERVER, MQTT_PORT);
      if (!client.connect("ESP32_Client", ACCESS_TOKEN, NULL)) 
      {
        Serial.println("Failed to connect");
        vTaskDelay(pdMS_TO_TICKS(10));
        continue;
      } 
      Serial.println("MQTT connected!");
      mqttConnected = true;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void SensorTask(void *pvParameters) {
  Wire.begin(21, 22);
  dht20.begin();
  while(1)
  {
    if(!mqttConnected)
    {
      vTaskDelay(pdMS_TO_TICKS(10000));
      continue;
    }
    dht20.read();
    float temp = dht20.getTemperature();
    float humi = dht20.getHumidity();
    if (isnan(temp) || isnan(humi)) 
    {
      Serial.println("Failed to read from DHT20 sensor!");
    } 
    else 
    {
      Serial.print("Temperature: ");
      Serial.print(temp);
      Serial.print(" °C, Humidity: ");
      Serial.print(humi);
      Serial.println(" %");
      String payload = "{\"temperature\":" + String(temp) + ", \"humidity\":" + String(humi) + "}";
      if (client.publish("v1/devices/me/telemetry", payload.c_str())) 
      {
        Serial.println("Data sent: " + payload);
      } 
      else 
      {
        Serial.println("Failed to send data!");
      }
    }
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}

void setup() {
  Serial.begin(115200);
  InitWiFi();

  xTaskCreate(wifiTask, "WiFi Task", 2048, NULL, 2, NULL);
  xTaskCreate(serverTask, "Server Task", 4096, NULL, 2, NULL);
  xTaskCreate(SensorTask, "Sensor Task", 4096, NULL, 2, NULL);
}

void loop() {

}