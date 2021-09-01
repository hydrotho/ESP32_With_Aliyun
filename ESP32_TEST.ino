#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFi.h>

#include "Aliyun_MQTT.h"
#include "DHT11.h"

#define DHT11_PIN D2
#define BUTTON_PIN D3
#define ALARM_PIN D7
#define RELAY_PIN D9

const char *wifi_ssid = "! wifi_ssid !";
const char *wifi_password = "! wifi_password !";

const char *product_key = "! product_key !";
const char *device_name = "! device_name !";
const char *device_secret = "! device_secret !";
const char *aliyun_region = "cn-shanghai";  //华东2（上海）

const char *sub_topic = "! /sys/${product_key}/${device_name}/thing/service/property/set !";
const char *pub_topic = "! /sys/${product_key}/${device_name}/thing/event/property/post !";

String air_identifier = "! air_identifier !";
String pause_identifier = "! pause_identifier !";
String pause_set_time_identifier = "! pause_set_time_identifier !";
String pause_count_down_identifier = "! pause_count_down_identifier !";
String alarm_identifier = "! alarm_identifier !";
String hum_identifier = "! hum_identifier !";
String temp_identifier = "! temp_identifier !";

WiFiClient esp32_client;
PubSubClient client(esp32_client);
DHT11 dht11;

uint8_t air_status = 0;
uint8_t alarm_status = 0;
bool pause_status = false;
uint16_t pause_cnt = 0;
uint16_t pause_set = 1;

void UpdateAirStatus() {
  if (alarm_status) {
    if (!pause_status) {
      digitalWrite(RELAY_PIN, HIGH);
      air_status = 1;
    } else {
      digitalWrite(RELAY_PIN, LOW);
      air_status = 0;
    }
  } else {
    digitalWrite(RELAY_PIN, LOW);
    air_status = 0;
  }
}

void UpdateButton() {
  if (digitalRead(BUTTON_PIN) == HIGH) {
    delay(20);
    if (digitalRead(BUTTON_PIN) == HIGH) {
      pause_status = !pause_status;
      while (digitalRead(BUTTON_PIN) == HIGH);
      Serial.print("PauseStatus: ");
      Serial.println(pause_status);
      if (pause_status)
        pause_cnt = 30;
      else
        pause_cnt = 0;
      UpdateAirStatus();
      client.publish(pub_topic,
                     ("{\"id\":12345,\"params\":{\"" + pause_identifier + "\":" + (int) pause_status + ",\""
                         + air_identifier + "\":" + air_status + ",\"" + pause_count_down_identifier + "\":" + pause_cnt
                         + "},\"method\":\"thing.event.property.post\"}").c_str());
    }
  }
}

//温度19℃至27℃，湿度20%至85%
void IsComfortable() {
  if (dht11.temperature < 19 || dht11.temperature > 27 || dht11.temperature < 20 || dht11.temperature > 85) {
    digitalWrite(ALARM_PIN, HIGH);
    alarm_status = 1;
  } else {
    digitalWrite(ALARM_PIN, LOW);
    alarm_status = 0;
  }
  UpdateAirStatus();
}

void ConnectWifi() {
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(wifi_ssid);

  WiFi.begin(wifi_ssid, wifi_password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void Callback(char *topic, byte *payload, unsigned int length) {
  Serial.print("Recevice [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char) payload[i]);
  }
  Serial.println();

  StaticJsonBuffer<500> jsonBuffer;
  JsonObject &root = jsonBuffer.parseObject((const char *) payload);
  if (!root.success()) {
    Serial.println("parseObject() failed.");
    return;
  }
  if (root["params"][pause_set_time_identifier]) {
    pause_set = root["params"][pause_set_time_identifier];
    client.publish(pub_topic,
                   ("{\"id\":12345,\"params\":{\"" + pause_set_time_identifier + "\":" + pause_set
                       + "},\"method\":\"thing.event.property.post\"}").c_str());
    return;
  }
  pause_status = root["params"][pause_identifier];
  if (pause_status)
    pause_cnt = pause_set;
  else
    pause_cnt = 0;
  UpdateAirStatus();
  client.publish(pub_topic,
                 ("{\"id\":12345,\"params\":{\"" + pause_identifier + "\":" + (int) pause_status + ",\""
                     + air_identifier + "\":" + air_status + ",\"" + pause_count_down_identifier + "\":" + pause_cnt
                     + ",\"" + pause_set_time_identifier + "\":" + pause_set
                     + "},\"method\":\"thing.event.property.post\"}").c_str());
}

void MqttCheckConnect() {
  while (!connectAliyunMQTT(client, product_key, device_name, device_secret, aliyun_region)) {
  }

  Serial.println("MQTT connected successfully.");
  client.subscribe(sub_topic);
  Serial.println("Subscribe done.");
}

//////////

void setup() {
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(ALARM_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT);

  Serial.begin(115200);
  while (!Serial)
    continue;

  ConnectWifi();

  client.setKeepAlive(120);
  client.setBufferSize(1024);
  client.setCallback(Callback);

  MqttCheckConnect();
}

uint8_t tempTime = 55;

void loop() {
  UpdateButton();

  if (!client.connected()) {
    MqttCheckConnect();
  }

  if (tempTime > 60) {
    tempTime = 0;

    if (pause_cnt != 0)
      --pause_cnt;
    else
      pause_status = false;

    dht11.Read(DHT11_PIN);
    Serial.print("CurrentTemperature: ");
    Serial.println(dht11.temperature);
    Serial.print("CurrentHumidity: ");
    Serial.println(dht11.humidity);

    IsComfortable();

    Serial.print("AlarmStatus: ");
    Serial.println(alarm_status);
    Serial.print("PauseStatus: ");
    Serial.println(pause_status);
    Serial.print("PauseCountDown: ");
    Serial.println(pause_cnt);
    Serial.print("AirStatus: ");
    Serial.println(air_status);

    client.publish(pub_topic,
                   ("{\"id\":12345,\"params\":{\"" + temp_identifier + "\":" + dht11.temperature + ",\""
                       + hum_identifier + "\":" + dht11.humidity + ",\"" + alarm_identifier + "\":" + alarm_status
                       + ",\"" + pause_identifier + "\":" + (int) pause_status + ",\"" + air_identifier + "\":"
                       + air_status + ",\"" + pause_count_down_identifier + "\":" + pause_cnt
                       + "},\"method\":\"thing.event.property.post\"}").c_str());
  } else {
    ++tempTime;
    delay(1000);
  }

  client.loop();
}