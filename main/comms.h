#pragma once

// Communication Libraries
#include <Wire.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <PubSubClient.h>
#define MQTT_MAX_PACKET_SIZE 512
#define MQTT_KEEPALIVE 60

extern const char* AP_SSID;
extern const char* AP_PASSWORD;

extern WebServer server;
extern WiFiClient espClient;
extern PubSubClient client;

void handleRoot();
void handleSetMode();
void handleSetMqtt();
void handleReboot();
void handleGetTime();
void handleSyncTime();
void handleSetTime();
void handleAdjustTilt();
bool connectWiFi();
bool connectMQTT();
bool publishData();