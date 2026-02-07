#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266mDNS.h>
#include <DNSServer.h>
#include <Updater.h> // <--- ДОДАНО: Бібліотека для прошивки

// ================= НАЛАШТУВАННЯ =================
const char * ap_name = "MyBot";
const char * password = "qazxswedc"; 

// ================= ЗМІННІ =================
unsigned long previousMillis1 = 0;
unsigned long previousMillis2 = 0;
unsigned long previousMillisWiFi = 0;
unsigned long lastNetworkActivity = 0;
const long NETWORK_DELAY = 3000;
long interval1 = 10000;
long interval2 = 20000;

String host1 = "api.thingspeak.com";
String host1_params = "/";
String host2 = "api.telegram.org"; 

String ssid = "";
String pass = "";

String page_title = "Wi-Fi Beacon Control";
bool stop_wifi = false;
bool just_started = true;

// ================= КАРТА EEPROM =================
int ssid_addr[] = {0, 32};
int pass_addr[] = {32, 32};
int url_addr1[] = {64, 32};
int url_addr2[] = {96, 64};
int chat_id_addr[] = {160, 32};
int url_addr1_params[] = {192, 64};
int url_addr1_timer[] = {256, 16};
int url_addr2_timer[] = {272, 16};

// ================= ОБ'ЄКТИ =================
ESP8266WebServer server(80);
DNSServer dnsServer; 

// ================= HTML & CSS =================
const char PAGE_HEAD[] PROGMEM = R"raw(
<!DOCTYPE html>
<html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>MyBot Control</title>
<style>
body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif; background-color: #f2f4f8; color: #333; margin: 0; padding: 20px; display: flex; flex-direction: column; align-items: center; }
.card { background: white; max-width: 500px; width: 100%; border-radius: 12px; box-shadow: 0 4px 15px rgba(0,0,0,0.1); padding: 20px; margin-bottom: 20px; box-sizing: border-box; }
h2 { margin-top: 0; color: #0056b3; text-align: center; }
.btn-group { display: flex; justify-content: space-between; margin-bottom: 20px; flex-wrap: wrap; gap: 5px; }
.big_button { flex: 1; min-width: 60px; height: 70px; border: none; border-radius: 10px; font-size: 2em; cursor: pointer; color: white; transition: 0.1s; }
.big_button:active { transform: scale(0.95); }
.btn-home { background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); }
.btn-wifi { background: linear-gradient(135deg, #2af598 0%, #009efd 100%); }
.btn-events { background: linear-gradient(135deg, #ff9a9e 0%, #fecfef 99%, #fecfef 100%); }
.btn-update { background: linear-gradient(135deg, #f093fb 0%, #f5576c 100%); } /* <--- Стиль для кнопки оновлення */
.btn-reboot { background: #ff6b6b; font-size: 1.2em; height: 50px; width: 100%; margin-top: 10px; color: white; border: none; border-radius: 8px; cursor: pointer;}
input[type=text], input[type=password], select { width: 100%; padding: 12px; margin: 8px 0; border: 1px solid #ddd; border-radius: 6px; box-sizing: border-box; font-size: 16px; }
input[type=submit], .file-btn { width: 100%; background-color: #007bff; color: white; padding: 14px 20px; border: none; border-radius: 6px; cursor: pointer; font-size: 18px; font-weight: bold; margin-top: 10px;}
input[type=file] { padding: 10px; background: #eee; border-radius: 6px; width: 100%; box-sizing: border-box; }
.status-row { display: flex; justify-content: space-between; padding: 10px 0; border-bottom: 1px solid #eee; }
.status-label { font-weight: bold; color: #555; }
.status-val { color: #000; }
a { text-decoration: none; display:contents; }
.note { font-size: 0.8em; color: #777; margin-top: 5px; display: block;}
</style></head><body>
<div class="card">
  <div class="btn-group">
    <a href="/"><button class="big_button btn-home">🏠</button></a>
    <a href="/settings"><button class="big_button btn-wifi">📶</button></a>
    <a href="/events"><button class="big_button btn-events">⏱️</button></a>
    <a href="/firmware"><button class="big_button btn-update">💾</button></a> </div>
)raw";

const char PAGE_FOOT[] PROGMEM = R"raw(
</div></body></html>
)raw";

// ================= ДОПОМІЖНІ ФУНКЦІЇ =================

void sendPage(String content) {
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  server.send(200, "text/html", String(PAGE_HEAD) + content + String(PAGE_FOOT));
}

String getFromEEPROM(int addr[2]);
boolean saveToEEPROM(String value, int addr[2]);
void saveSSIDAndPass(String new_ssid, String new_pass);

// ================= ОБРОБНИКИ СТОРІНОК =================

void handleRoot() {
  String status = (WiFi.status() != WL_CONNECTED) 
                  ? "<span style='color:red; font-weight:bold'>OFFLINE ❌</span>" 
                  : "<span style='color:green; font-weight:bold'>ONLINE 🟢</span>";
  
  String current_ssid = getFromEEPROM(ssid_addr);
  IPAddress local_ip = WiFi.localIP();
  IPAddress ap_ip = WiFi.softAPIP();

  String html = "<h2>System Status</h2>";
  html += "<div class='status-row'><span class='status-label'>Target WiFi:</span><span class='status-val'>" + current_ssid + "</span></div>";
  html += "<div class='status-row'><span class='status-label'>Status:</span><span class='status-val'>" + status + "</span></div>";
  html += "<div class='status-row'><span class='status-label'>Local IP:</span><span class='status-val'><a href='http://" + local_ip.toString() + "'>" + local_ip.toString() + "</a></span></div>";
  html += "<div class='status-row'><span class='status-label'>AP IP:</span><span class='status-val'>" + ap_ip.toString() + "</span></div>";
  html += "<div class='status-row'><span class='status-label'>mDNS:</span><span class='status-val'><a href='http://mybot.local'>http://mybot.local</a></span></div>";
  
  html += "<br><form action='/reboot' method='POST'><button class='btn-reboot'>🔄 REBOOT SYSTEM</button></form>";
  
  sendPage(html);
}

void handleEvents() {
    if (server.method() == HTTP_POST) {
       if (server.hasArg("url1")) saveToEEPROM(server.arg("url1"), url_addr1);
       if (server.hasArg("url1_params")) saveToEEPROM(server.arg("url1_params"), url_addr1_params);
       if (server.hasArg("url1_timer")) {
          saveToEEPROM(server.arg("url1_timer"), url_addr1_timer);
          interval1 = server.arg("url1_timer").toInt() * 1000;
       }
       if (server.hasArg("url2")) saveToEEPROM(server.arg("url2"), url_addr2);
       if (server.hasArg("chat_id")) saveToEEPROM(server.arg("chat_id"), chat_id_addr);
       if (server.hasArg("url2_timer")) {
          saveToEEPROM(server.arg("url2_timer"), url_addr2_timer);
          interval2 = server.arg("url2_timer").toInt() * 1000;
       }
    }

    host1 = getFromEEPROM(url_addr1);
    host1_params = getFromEEPROM(url_addr1_params);
    String t1 = getFromEEPROM(url_addr1_timer);

    host2 = getFromEEPROM(url_addr2);
    String cid = getFromEEPROM(chat_id_addr);
    String t2 = getFromEEPROM(url_addr2_timer);

    String html = "<h2>Webhooks & Timers</h2>";
    html += "<form method='post'>";
    html += "<h3>📡 ThingSpeak (HTTP)</h3>";
    html += "<span class='note'>Host</span><input type='text' name='url1' value='" + host1 + "'>";
    html += "<span class='note'>Params</span><input type='text' name='url1_params' value='" + host1_params + "'>";
    html += "<span class='note'>Interval (sec)</span><input type='text' name='url1_timer' value='" + t1 + "'>";
    html += "<hr>";
    html += "<h3>✈️ Telegram (HTTPS)</h3>";
    html += "<span class='note'>Bot Token (Add 'bot' manually!)</span>";
    html += "<input type='text' name='url2' placeholder='bot123456:ABC...' value='" + host2 + "'>";
    html += "<span class='note'>Chat ID</span><input type='text' name='chat_id' value='" + cid + "'>";
    html += "<span class='note'>Interval (sec)</span><input type='text' name='url2_timer' value='" + t2 + "'>";
    html += "<input type='submit' value='SAVE SETTINGS'></form>";
    sendPage(html);
}

void handleSettings() {
    String s = server.arg("ssid");
    if (s != "") {
      saveSSIDAndPass(s, server.arg("pass"));
      sendPage("<h2>Saved!</h2><p>Rebooting...</p>");
      delay(1000);
      ESP.restart();
    } else {
      int numSsid = WiFi.scanNetworks();
      String options = "<select name='ssid'>";
      if (numSsid != -1) {
        for (int i = 0; i < numSsid; i++) options += "<option value=\"" + WiFi.SSID(i) + "\">" + WiFi.SSID(i) + " (" + String(WiFi.RSSI(i)) + "dB)</option>";
      }
      options += "</select>";
      String html = "<h2>WiFi Connection</h2><form method='post'>";
      html += "<span class='note'>Select Network</span>" + options;
      html += "<span class='note'>Password</span><input type='password' name='pass'>";
      html += "<input type='submit' value='CONNECT & REBOOT'></form>";
      sendPage(html);
    }
}

// === НОВА ФУНКЦІЯ: Сторінка оновлення ===
void handleFirmware() {
  String html = "<h2>Firmware Update</h2>";
  html += "<p>Upload <b>.bin</b> file.</p>";
  // Важливо: enctype='multipart/form-data' для передачі файлів
  html += "<form method='POST' action='/update' enctype='multipart/form-data'>";
  html += "<input type='file' name='update' accept='.bin'>";
  html += "<input type='submit' value='🚀 FLASH FIRMWARE' onclick='this.value=\"Wait...\";'>";
  html += "</form>";
  html += "<p class='note'>⚠️ Do not turn off power!</p>";
  sendPage(html);
}

void handleReboot() {
  server.send(200, "text/html", "<h2>Rebooting...</h2>");
  delay(1000);
  ESP.restart();
}

void handleNotFound() { handleRoot(); }

// ================= CRON & LOGIC =================

void checkCron() {
  if (WiFi.status() != WL_CONNECTED || stop_wifi) return;
  
  unsigned long currentMillis = millis();

  // --- Cron 1: ThingSpeak ---
  // Перевіряємо, чи настав час
  if (just_started || (currentMillis - previousMillis1 >= interval1 && interval1 > 5000)) {
    previousMillis1 = currentMillis; // Одразу оновлюємо таймер, щоб не спрацювало двічі
    
    Serial.println("\n>>> Cron 1: ThingSpeak Start");
    
    WiFiClient client;
    String h1 = getFromEEPROM(url_addr1);
    
    if (client.connect(h1, 80)) {
      client.print(String("GET ") + getFromEEPROM(url_addr1_params) + " HTTP/1.1\r\nHost: " + h1 + "\r\nConnection: close\r\n\r\n");
      
      unsigned long timeout = millis();
      while (client.available() == 0) {
        if (millis() - timeout > 2000) break;
        delay(10);
      }
      client.stop();
      Serial.println(">>> Cron 1: Done");
    } else {
      Serial.println(">>> Cron 1: Connect Failed");
    }

    // ВАЖЛИВО: Робимо паузу, щоб стек WiFi очистився перед наступним запитом
    delay(2000); 
  }

  // Оновлюємо currentMillis після затримки, щоб другий таймер був точнішим
  currentMillis = millis();

  // --- Cron 2: Telegram ---
  if (just_started || (currentMillis - previousMillis2 >= interval2 && interval2 > 5000)) {
    previousMillis2 = currentMillis; // Оновлюємо таймер
    
    Serial.println("\n>>> Cron 2: Telegram Start");

    WiFiClientSecure client2;
    client2.setInsecure();
    
    if (client2.connect("api.telegram.org", 443)) {
      HTTPClient http;
      String url = "https://api.telegram.org/" + getFromEEPROM(url_addr2) + "/sendMessage";
      
      http.begin(client2, url);
      http.addHeader("Content-Type", "application/json");

      String msg = just_started ? "🕺🎉 I am back!" : "🟢...";
      // Додаємо timestamp до повідомлення, щоб Telegram не кешував однакові повідомлення
      String json = "{\"chat_id\": \"" + getFromEEPROM(chat_id_addr) + "\", \"text\": \"" + msg + "\", \"disable_notification\": true}";
      
      int httpCode = http.POST(json);
      http.end();
      client2.stop();
      
      if (httpCode > 0) {
        just_started = false;
        Serial.printf(">>> Cron 2: Done (Code %d)\n", httpCode);
      } else {
        Serial.printf(">>> Cron 2: Failed (Error: %s)\n", http.errorToString(httpCode).c_str());
      }
    } else {
      Serial.println(">>> Cron 2: Connect Failed");
    }
  }
}

void reconnectWiFi() {
  if (WiFi.status() != WL_CONNECTED && !stop_wifi) {
    if (millis() - previousMillisWiFi >= 60000) {
      previousMillisWiFi = millis();
      WiFi.begin(getFromEEPROM(ssid_addr), getFromEEPROM(pass_addr));
    }
    digitalWrite(LED_BUILTIN, HIGH);
  } else {
    digitalWrite(LED_BUILTIN, LOW);      
  }
}

// ================= EEPROM HELPERS =================

void clearEEPROM(int addr[2]) {
  for (int i = addr[0]; i < (addr[0] + addr[1]); i++) EEPROM.write(i, 0);
  EEPROM.commit();
}

boolean saveToEEPROM(String value, int addr[2]) {
  clearEEPROM(addr);
  if (value.length() < addr[1]) {
    for (int i = 0; i < value.length(); i++) EEPROM.write(i + addr[0], value[i]);
    EEPROM.commit();
    return true;
  }
  return false;
}

String getFromEEPROM(int addr[2]) {
  String result = "";
  for (int i = addr[0]; i < (addr[0] + addr[1]); i++) {
    char c = EEPROM.read(i);
    if (c != 0 && c != 255) result += c; else break;
  }
  return result;
}

void saveSSIDAndPass(String new_ssid, String new_pass) {
  saveToEEPROM(new_ssid, ssid_addr);
  saveToEEPROM(new_pass, pass_addr);
}

// ================= SETUP & LOOP =================

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH); 

  Serial.begin(115200);
  EEPROM.begin(512);
  delay(500);

  String t1 = getFromEEPROM(url_addr1_timer);
  String t2 = getFromEEPROM(url_addr2_timer);
  if (t1.length() > 0) interval1 = t1.toInt() * 1000;
  if (t2.length() > 0) interval2 = t2.toInt() * 1000;

  WiFi.softAP(ap_name, password);
  dnsServer.start(53, "*", WiFi.softAPIP());
  if (MDNS.begin("mybot")) Serial.println("MDNS started");

  server.on("/", handleRoot);
  server.on("/settings", handleSettings);
  server.on("/events", handleEvents);
  server.on("/firmware", handleFirmware); // <--- Кнопка для юзера
  server.on("/reboot", handleReboot);
  server.onNotFound(handleNotFound);

  // === ОБРОБНИК ПРОШИВКИ (Системна частина) ===
  server.on("/update", HTTP_POST, []() {
    // 1. Відповідь браузеру після завершення
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK! REBOOTING...");
    ESP.restart();
  }, []() {
    // 2. Процес завантаження файлу
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      WiFiUDP::stopAll(); // Зупиняємо зайве
      uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
      if (!Update.begin(maxSketchSpace)) { 
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) {
        Serial.printf("Update Success: %u\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });

  server.begin();
  Serial.println("HTTP server started");

  String saved_ssid = getFromEEPROM(ssid_addr);
  if (saved_ssid.length() > 1) {
    WiFi.begin(saved_ssid, getFromEEPROM(pass_addr));
  }
}

void loop() {
  dnsServer.processNextRequest(); 
  MDNS.update();                  
  server.handleClient();          
  checkCron();                    
  reconnectWiFi();                
}
