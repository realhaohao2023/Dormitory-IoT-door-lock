#ifndef  WIFI_MQTT_H
#define  WIFI_MQTT_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <PubSubClient.h> //PubSubClient 作者 Nick O’Leary
#include <HTTPClient.h>
#include <ESP32Servo.h>
#include <ArduinoJson.h> // 引入ArduinoJson库

void Wifi_Connect();
void mqttCheckConnect();
void mqttcallback(char *topic, byte *payload, unsigned int length);
void judgemqttconnect();

void processJsonMessage(char *message);
void publock();

#endif // ! WIFI_MQTT_H
