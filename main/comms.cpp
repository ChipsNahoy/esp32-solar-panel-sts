#include "comms.h"
#include "utils.h"
#include "sensors.h"
#include "motor.h"
#include "timekeeping.h"

// Wi-Fi Credentials
const char* AP_SSID = "ESP32_Debug_Mode";
const char* AP_PASSWORD = "debugmode";

// MQTT Message Identifier
const char* GROUP_ID    = "solar_panel_TA";           
const int   OWNER_ID    = 212100207;                  
const char* CATEGORY    = "Electrical";               
const char* KAFKA_TOPIC = "212100207/solar_panel_TA";
const char* USE_KAFKA   = "true";

// Web Server
WebServer server(80);

WiFiClient espClient; 
PubSubClient client(espClient);

// HTML
void handleRoot() {
  bool wifiOK = (WiFi.status() == WL_CONNECTED);
  String html = R"rawliteral(
    <!DOCTYPE html>
    <html>
    <head>
        <meta charset="utf-8">
        <title>ESP32 Mode Selection</title>
        <style>
            body { font-family: Arial, sans-serif; text-align: center; padding: 50px; }
            h1 { color: #333; }
            #clock { font-size: 1.5em; margin: 10px; }
            button { padding: 10px 20px; font-size: 18px; margin: 10px; cursor: pointer; }
            #wifiForm { display: none; margin-top: 20px; }
            input { margin: 10px; padding: 8px; font-size: 16px; }
            #setForm { display: none; margin-top: 20px; }
        </style>
    </head>
    <body>
        <h1>Select Operating Mode</h1>
          <div id="clock">--:--:--</div>
          <div><small>Source: <span id="source">?</span></small></div>
        <button onclick="showWifiForm()">Wi-Fi Mode</button>
        <button onclick="adjustPanelTilt()">Adjust Panel Tilt</button>
        <button onclick="showSetForm()">Set Time Manually</button>
    )rawliteral";

  if (wifiOK) {
    html += R"rawliteral(
      <button onclick="syncTime()">Sync RTC to NTP</button>
    )rawliteral";
  }
        
  html += R"rawliteral(
        <div id="wifiForm">
            <h2>Enter Wi-Fi Credentials</h2>
            <input id="ssid" type="text" placeholder="Wi-Fi SSID" required>
            <input id="password" type="password" placeholder="Wi-Fi Password" required>
            <button onclick="submitWifi()">Submit</button>
        </div>

        <div id="setForm">
          <h3>Manual Time Set</h3>
          <form onsubmit="setTime(); return false;">
            <input id="fyear"  type="number" placeholder="YYYY" required>
            <input id="fmonth" type="number" placeholder="MM"  required>
            <input id="fday"   type="number" placeholder="DD"  required><br>
            <input id="fhour"  type="number" placeholder="hh"  required>
            <input id="fmin"   type="number" placeholder="mm"  required>
            <input id="fsec"   type="number" placeholder="ss"  required><br>
            <button type="submit">Apply</button>
          </form>
        </div>

        <br><button onclick="location.href='/reboot'">Reboot Now</button>

        <script>
            // Poll /getTime every second
            setInterval(fetchTime, 1000);
            function fetchTime() {
              fetch('/getTime')
                .then(r => r.json())
                .then(j => {
                  document.getElementById('clock').textContent = j.timestamp;
                  document.getElementById('source').textContent = j.source;
                });
            }

            function syncTime() {
              fetch('/syncTime')
                .then(r => r.json())
                .then(j => alert(j.message));
            }

            function showSetForm() {
              document.getElementById('setForm').style.display = 'block';
            }

            function setTime() {
              const y = document.getElementById('fyear').value,
                    mo = document.getElementById('fmonth').value,
                    d = document.getElementById('fday').value,
                    h = document.getElementById('fhour').value,
                    mi = document.getElementById('fmin').value,
                    s = document.getElementById('fsec').value;
              fetch(`/setTime?year=${y}&month=${mo}&day=${d}&hour=${h}&minute=${mi}&second=${s}`, { method: 'POST' })
                .then(r => r.json())
                .then(j => alert(j.message));
            }

            function showWifiForm() {
                document.getElementById('wifiForm').style.display = 'block';
            }

            function submitWifi() {
                const ssid = document.getElementById('ssid').value;
                const password = document.getElementById('password').value;
                
                if (!ssid || !password) {
                    alert('Please enter both SSID and password.');
                    return;
                }
                const url = `/setMode?mode=WiFi&ssid=${encodeURIComponent(ssid)}&password=${encodeURIComponent(password)}`;
                window.location.href = url;
            }

            function adjustPanelTilt() {
              fetch('/adjustTilt')
                .then(response => response.text())
                .then(data => alert(data));
            }
        </script>
    </body>
    </html>
    )rawliteral";

    server.send(200, "text/html", html);
}

void handleSetMode() {
  if (!server.hasArg("mode") 
   || server.arg("mode") != "WiFi"
   || !server.hasArg("ssid")
   || !server.hasArg("password")) {
    server.send(400, "text/plain", "Invalid Request");
    return;
  }

  String ssid     = server.arg("ssid");
  String password = server.arg("password");

  // 1) Switch into dual AP+STA mode and kick off connection
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASSWORD);      // keep AP up
  WiFi.begin(ssid.c_str(), password.c_str());

  // 2) Wait up to 30s for connection
  unsigned long start = millis();
  bool connected = false;
  while (millis() - start < 30000) {
    if (WiFi.status() == WL_CONNECTED) {
      connected = true;
      break;
    }
    delay(500);
  }

  // 3a) Success → save creds & prompt for reboot
  if (connected) {
    preferences.begin("esp32_config", false);
    preferences.putString("ssid", ssid);
    preferences.putString("password", password);
    preferences.putString("mode", "WiFi");
    preferences.putBool("justDebugged", true);
    preferences.end();

    String html = R"rawliteral(
    <!DOCTYPE html>
    <html>
      <body style="font-family:Arial,sans-serif;text-align:center">
        <h2>Wi-Fi OK: )rawliteral" + ssid + R"rawliteral(</h2>
        <form action="/setMqtt" method="POST">
          <p>MQTT Server: <input name="server"   placeholder="e.g. broker.hivemq.com"></p>
          <p>Port:         <input name="port"     placeholder="1883"></p>
          <p>Topic:        <input name="topic"    placeholder="solar_panel/data"></p>
          <button type="submit">Test & Save MQTT</button>
        </form>
        <p>Or press debug button to stay here.</p>
      </body>
    </html>
    )rawliteral";

    server.send(200, "text/html", html);
    return;
  }

  // 3b) Failure → go back to form with error
  WiFi.softAP(AP_SSID, AP_PASSWORD);  // re‑enable AP only
  String html = R"rawliteral(
    <!DOCTYPE html>
    <html>
    <head><title>Wi-Fi Failed</title></head>
    <body style="font-family:Arial,sans-serif;text-align:center;">
      <h2>Failed to connect to )rawliteral"
    + ssid +
    R"rawliteral(</h2>
      <p>Please <a href="/">try again</a> with different credentials.</p>
    </body>
    </html>
  )rawliteral";

  server.send(200, "text/html", html);
}

void handleSetMqtt() {
  // Validate
  if (!server.hasArg("server") ||
      !server.hasArg("port")   ||
      !server.hasArg("topic")) {
    server.send(400, "text/plain", "Invalid MQTT parameters");
    return;
  }
  String mServer = server.arg("server");
  int    mPort   = server.arg("port").toInt();
  String mTopic  = server.arg("topic");
  Serial.print("Received: MQTT-Server:"); Serial.print(mServer); 
  Serial.print(" MQTT-Port:"); Serial.print(mPort);
  Serial.print(" MQTT-Topic:"); Serial.println(mPort);

  // Test connection
  WiFiClient testNet;
  PubSubClient testClient(testNet);
  testClient.setServer(mServer.c_str(), mPort);

  unsigned long start = millis();
  bool ok = false;
  while (millis() - start < 5000) {
    if (testClient.connect("ESP32_Debug_Test")) {
      Serial.println("MQTT connection success!!");
      ok = true;
      testClient.disconnect();
      break;
    }
    delay(500);
  }

  if (ok) {
    preferences.begin("esp32_config", false);
    preferences.putString("mqtt_server", mServer);
    preferences.putUInt("mqtt_port",   mPort);
    preferences.putString("mqtt_topic", mTopic);
    preferences.end();

    // Success page
    String html = R"rawliteral(
      <!DOCTYPE html><html><body style="font-family:Arial,sans-serif;text-align:center">
        <h2>MQTT OK!</h2>
        <p>Server: )rawliteral" + mServer +
      R"rawliteral(<br>Port: )rawliteral" + String(mPort) +
      R"rawliteral(<br>Topic: )rawliteral" + mTopic +
      R"rawliteral(</p>
        <button onclick="location.href='/reboot'">Reboot Now</button>
        <p>Or press debug button to stay here.</p>
      </body></html>
    )rawliteral";
    server.send(200, "text/html", html);
  } else {
    // Failure → ask again
    String html = R"rawliteral(
      <!DOCTYPE html><html><body style="font-family:Arial,sans-serif;text-align:center">
        <h2>Failed to reach MQTT broker</h2>
        <p><a href="/">Start Over</a> or <a href="javascript:history.back()">Try Again</a></p>
      </body></html>
    )rawliteral";
    server.send(200, "text/html", html);
  }
}

void handleReboot() {
  server.send(200, "text/plain", "Rebooting now…");
  preferences.begin("esp32_config", false);
  preferences.putBool("justDebugged", true);
  preferences.end();
  delay(1000);
  ESP.restart();
}

void handleGetTime() {
  char buf[20];
  const char* src;
  if (rtc_state) {
    DateTime dt = rtc.now();
    src = "DS3231";
    snprintf(buf, sizeof(buf),
             "%04d-%02d-%02d %02d:%02d:%02d",
             dt.year(), dt.month(), dt.day(),
             dt.hour(), dt.minute(), dt.second());
  } else {
    struct tm tm;  
    if (!getLocalTime(&tm)) {
      strcpy(buf, "0000-00-00 00:00:00");
    } else {
      src = "Internal";
      snprintf(buf, sizeof(buf),
               "%04d-%02d-%02d %02d:%02d:%02d",
               tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday,
               tm.tm_hour, tm.tm_min, tm.tm_sec);
    }
  }
  String json = String("{\"timestamp\":\"") + buf + 
                String("\",\"source\":\"") + src + "\"}";
  server.send(200, "application/json", json);
}

void handleSyncTime() {
  bool ok = sync_RTC_to_NTP();
  String msg = ok ? "RTC synced to NTP" : "RTC sync failed";
  String json = String("{\"success\":") + (ok?"true":"false") + 
                String(",\"message\":\"") + msg + "\"}";
  server.send(200, "application/json", json);
}

void handleSetTime() {
  int yr  = server.arg("year").toInt();
  int mo  = server.arg("month").toInt();
  int day = server.arg("day").toInt();
  int hr  = server.arg("hour").toInt();
  int mi  = server.arg("minute").toInt();
  int se  = server.arg("second").toInt();

  bool ok;
  if (rtc_state) {
    rtc.adjust(DateTime(yr, mo, day, hr, mi, se));
    ok = true;
  } else {
    // set ESP32’s internal clock
    struct tm tm = {};
    tm.tm_year = yr - 1900;
    tm.tm_mon  = mo - 1;
    tm.tm_mday = day;
    tm.tm_hour = hr;
    tm.tm_min  = mi;
    tm.tm_sec  = se;
    time_t t = mktime(&tm);
    struct timeval tv = { .tv_sec = t, .tv_usec = 0 };
    ok = (settimeofday(&tv, NULL) == 0);
  }

  String msg = ok ? "Time updated successfully" : "Failed to set time";
  String json = String("{\"success\":") + (ok?"true":"false") +
                String(",\"message\":\"") + msg + "\"}";
  server.send(200, "application/json", json);
}

void handleAdjustTilt() {
  setup_motor();
  adjust_panel_tilt();
  server.send(200, "text/plain", "Panel adjustment completed.");
}

bool connectWiFi() {
  preferences.begin("esp32_config", false);
  unsigned int counter = preferences.getUInt("wifiRebootCounter", 0);
  counter++;
  int retry_attempt = 0;
  Serial.print("Connecting to WiFi...");
  String ssid = preferences.getString("ssid");
  String password = preferences.getString("password");
  preferences.end();
  if (ssid.isEmpty() || password.isEmpty()) {
    Serial.println("Key 'ssid' not found in NVS! Booting into Debug Mode");
    back_to_debug();
  }
  Serial.print("Connecting to "); Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    if (counter > 2) {
      Serial.println("Failed to reconnect to WiFi after rebooting 2 times, continuing without WiFi...");
      preferences.putUInt("wifiRebootCounter", 0);
      preferences.end();
      return false;
    }
    if (retry_attempt > 5) {
      Serial.println("Failed to reconnect to WiFi 5 times, rebooting ESP...");
      preferences.putUInt("wifiRebootCounter", counter);
      preferences.end();
      ESP.restart();
    }
    delay(500);
    Serial.print(".");
    retry_attempt++;
  }
  preferences.putUInt("wifiRebootCounter", 0);
  preferences.end();
  Serial.println("\nWiFi Connected");
  return true;
}

bool connectMQTT() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Not connected to WiFi, skipping MQTT...");
    return false;
  }
  preferences.begin("esp32_config", false);
  unsigned int counter = preferences.getUInt("mqttRebootCounter", 0);
  counter++;
  int retry_attempt = 0;
  while (!client.connected()) {
    if (counter > 2) {
      Serial.println("Failed to connect to MQTT broker after rebooting 2 times, skipping MQTT...");
      preferences.putUInt("mqttRebootCounter", 0);
      preferences.end();
      return false;
    }
    if (retry_attempt > 5) {
      Serial.println("Failed to connect to MQTT broker 5 times, rebooting ESP...");
      preferences.putUInt("mqttRebootCounter", counter);
      preferences.end();
      ESP.restart();
    }
    Serial.print("Connecting to MQTT...");
    if (client.connect("ESP32_Client")) {
      Serial.println("Connected!");
    } else {
      retry_attempt++;
      Serial.print("Failed, rc=");
      Serial.print(client.state());
      Serial.println(" Retrying in 500ms...");
      delay(500);
    }
  }
  preferences.end();
  return true;
}

bool publishData() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Not connected to WiFi, skipping data transfer...");
    return false;
  }
  preferences.begin("esp32_config", false);
  String MQTT_TOPIC = preferences.getString("mqtt_topic");
  preferences.end();
  int retry_attempt = 0;
  sensor_data data = prepData();
  if (data.l_uv < 0) {
    Serial.print("Left UV Problem, analog value: ");
    Serial.println(ads.readADC(0));
    addError("Left UV Problem");
  }
  if (data.r_uv < 0) {
    Serial.print("Right UV Problem, analog value: ");
    Serial.println(ads.readADC(1));
    addError("Right UV Problem");
  }
  String deviceId = WiFi.macAddress();
  char payload[512];
  int len = snprintf(payload, sizeof(payload),
    "{"
      "\"device_id\":\"%s\","
      "\"group_id\":\"%s\","
      "\"owner_id\":%d,"
      "\"category\":\"%s\","
      "\"values\":{"
        "\"l_uv\":%.2f,"
        "\"r_uv\":%.2f,"
        "\"v1\":%.2f,"
        "\"i1\":%.2f,"
        "\"v2\":%.2f,"
        "\"i2\":%.2f,"
        "\"roll\":%.2f,"
        "\"pitch\":%.2f,"
        "\"espTemperature\":%.2f,"
        "\"error\":\"%s\""
      "},"
      "\"timestamp_sensor\":\"%s\","
      "\"kafka\":%s,"
      "\"kafka_topic\":\"%s\""
    "}",
    deviceId.c_str(),
    GROUP_ID,
    OWNER_ID,
    CATEGORY,
    data.l_uv,
    data.r_uv,
    data.v1,
    data.i1,
    data.v2,
    data.i2,
    data.roll,
    data.pitch,
    data.espTemperature,
    errorMsg.isEmpty() ? "OK" : errorMsg.c_str(),
    data.datetime,
    USE_KAFKA,
    KAFKA_TOPIC
  );

  Serial.printf("Payload length = %d bytes\n", len);
  if (len < 0 || len >= int(sizeof(payload))) {
    Serial.println("Payload too large!");
    return false;
  }

  Serial.println("Data to be Published: " + String(payload));
  client.loop();
  while (!client.publish(MQTT_TOPIC.c_str(), payload)) {
    if (retry_attempt > 3) {
      Serial.print("Publish failed, state=");
      Serial.println(client.state());
      Serial.println("Failed to publish through MQTT 3 times, skipping data transfer...");
      return false;
    }
    Serial.println("Failed to publish through MQTT, trying again...");
    retry_attempt++;
  }
  
  unsigned long t0 = millis();
  while (millis() - t0 < 200) {
    client.loop();
    delay(1);
  }

  Serial.println("Successfully published data through MQTT!");
  return true;
}

