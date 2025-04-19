#include "sensors.h"
#include "motor.h"
#include "timekeeping.h"
#include "comms.h"
#include "utils.h"

void setup() {
  Serial.begin(115200);

  pinMode(debugButtonPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(debugButtonPin), handleDebugButton, FALLING);

  Wire.begin();

  preferences.begin("esp32_config", false);
  String savedMode = preferences.getString("mode", "DEBUG");
  Serial.println("Saved Mode: " + savedMode);
  preferences.end();
  setup_suite();

  if (savedMode == "DEBUG") {
      // Start in Debug Mode (Wi-Fi AP + Web Server)
      Serial.println("Starting in Debug Mode...");
      WiFi.softAP(AP_SSID, AP_PASSWORD);
      Serial.print("AP IP Address: ");
      Serial.println(WiFi.softAPIP());
      if (MDNS.begin("debug")) {
        Serial.println("mDNS responder started at http://debug.local");
      } else {
        Serial.println("Error starting mDNS");
      }
      server.on("/", handleRoot);
      server.on("/setMode", HTTP_GET, handleSetMode);
      server.on("/setMqtt", HTTP_POST, handleSetMqtt);
      server.on("/getTime",  HTTP_GET,  handleGetTime);
      server.on("/syncTime", HTTP_GET,  handleSyncTime);
      server.on("/setTime",  HTTP_POST, handleSetTime);
      server.on("/reboot", HTTP_GET, handleReboot);
      server.on("/adjustTilt", HTTP_GET, handleAdjustTilt);
      server.begin(); 
  } else if (savedMode == "WiFi") {             
      Serial.println("Starting in Wi-Fi Mode...");
      if (!connectWiFi()) {
        Serial.println("Not connected to WiFi, skipping MQTT and data transfer...");
      } else {
        sync_RTC_to_NTP();
        preferences.begin("esp32_config", false);
        String MQTT_BROKER = preferences.getString("mqtt_server");
        unsigned int MQTT_PORT = preferences.getUInt("mqtt_port");
        preferences.end();
        if (MQTT_BROKER.isEmpty() || isnan(MQTT_PORT)) {
          Serial.println("Key 'mqtt_server' not found in NVS! Booting into Debug Mode");
          back_to_debug();
        }
        client.setServer(MQTT_BROKER.c_str(), MQTT_PORT);
        if (connectMQTT()) {
          client.loop();
          process_routine();
        }
      }
  }
}

void loop() {
  wifi_mode_t m = WiFi.getMode();
  if ( (m == WIFI_MODE_AP) || (m == WIFI_MODE_APSTA) ) {
    server.handleClient();
  }
}
