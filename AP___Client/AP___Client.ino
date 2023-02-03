#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>

/* Set these to your desired credentials. */

const char * ap_name = "WiFiBot";
const char * password = "qazxswedc";

unsigned long previousMillis1 = 0;
unsigned long previousMillis2 = 0;
unsigned long previousMillisWiFi = 0;
long interval1 = 10000;
long interval2 = 20000;

String host1 = "api.thingspeak.com";
String host1_params = "/";
String host2 = "api.telegram.org";

String ssid = "";
String pass = "";

unsigned long last_ping = 0;
int ping_intrv = 15000;
String page_title = "Страница управления Wi-Fi bot";
String page_content = "Content";
char css[] = "<style>.s_b{background-color:#eee;padding:5px}.p_p{padding-top:35px}.c1{position:absolute;left:15vw;right:15vw;background-color:#dedede;padding:20px;}ul.hr li{display:inline;margin-right:5px;}ul li{list-style-type:none;background-color:#ccc;border:1px solid #aaa;padding:10px;}ul.vert li{margin:4px;}a{color:black;text-decoration:none;}body{font-family:arial;}input{padding:4px;font-size:14px;line-height:18px;border:1;margin:0;}input[type=submit]{width:50%;}.t_l{text-align:left;}.t_r{text-align:right;}table{border:0;width:100%;}select{font-size:14px;line-height:14px;padding:9px}</style>";
String status_message = "&status=Hello! I run and sends the data. Your esp8266.";

bool stop_wifi = false;
bool just_started = true;

int ssid_addr[] = {0, 32}; // адрес, смещение
int pass_addr[] = {32, 32};
int url_addr1[] = {64, 32};
int url_addr2[] = {96, 64};
int chat_id_addr[] = {160, 14};
int url_addr1_params[] = {174, 64};
int url_addr1_timer[] = {238, 16};
int url_addr2_timer[] = {254, 16};


ESP8266WebServer server(80);

void sendPage() {
  sendHead();
  server.sendContent(page_content);
  sendTail();
}
void sendHead() {
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", "");
  server.sendContent("<html><head><meta charset=\"utf-8\"><title>" + page_title + "</title>" + css + "</head>");
  server.sendContent("<body><div><span class=\"s_b\"><a href=\"/\">Home</a></span><span class=\"s_b\"><a href=\"/settings\">Settings</a></span><span class=\"s_b\"><a href=\"/events\">Events</a></span></div>");
}

void sendTail() {
  server.sendContent("</body></html>");
}

// http://192.168.4.1
void handleRoot() {
  page_title = "Страница управления ArduStation";
  String status = (WiFi.status() != WL_CONNECTED) ? "<span style=\"color:red\">offline</span>" : "<span style=\"color:green\">online</span>";
  String ssid = getFromEEPROM(ssid_addr);
  IPAddress tmp_ip = WiFi.localIP();
  String ip(String(tmp_ip[0]) + "." + String(tmp_ip[1]) + "." + String(tmp_ip[2]) + "." + String(tmp_ip[3]));
  page_content = "<table>";
  page_content += "<tr><td width=\"50%\" class=\"t_r\">Точка доступа: </td><td class=\"t_l\"><b>" + ssid + "</b></td></tr>";
  page_content += "<tr><td width=\"50%\" class=\"t_r\">Состояние: </td><td class=\"t_l\"><b>" + status + "</b></td></tr>";
  page_content += "<tr><td width=\"50%\" class=\"t_r\">Local IP: </td><td class=\"t_l\"><b><a href=\"http://" + ip + "\">" + ip + "</a></b></td></tr>";
  tmp_ip = WiFi.softAPIP();
  ip = (String(tmp_ip[0]) + "." + String(tmp_ip[1]) + "." + String(tmp_ip[2]) + "." + String(tmp_ip[3]));
  page_content += "<tr><td width=\"50%\" class=\"t_r\">Access point IP: </td><td class=\"t_l\"><b><a href=\"http://" + ip + "\">" + ip + "</a></b></td></tr>";
  page_content += "</table>";
  sendPage();
}

void handleEvents ()
{
    page_title = "Webhooks";

    String url1 = server.arg("url1");
    if (url1 != "") {
      host1 = url1;
      saveToEEPROM(host1, url_addr1);
    } else {
      host1 = getFromEEPROM(url_addr1);
    }

    String url1_params = server.arg("url1_params");
    if (url1_params != "") {
      host1_params = url1_params;
      saveToEEPROM(host1_params, url_addr1_params);
    } else {
      host1_params = getFromEEPROM(url_addr1_params);
    }

    String url2 = server.arg("url2");
    if (url2 != "") {
      host2 = url2;
      saveToEEPROM(host2, url_addr2);
    } else {
      host2 = getFromEEPROM(url_addr2);
    }

    String chatId = server.arg("chat_id");
    if (chatId != "") {
      saveToEEPROM(String(chatId), chat_id_addr);
    } else {
      chatId = getFromEEPROM(chat_id_addr);
    }

    String host1_timer = server.arg("url1_timer");
    if (host1_timer != "") {
      saveToEEPROM(String(host1_timer), url_addr1_timer);
      interval1 = host1_timer.toInt() * 1000;
    } else {
      host1_timer = getFromEEPROM(url_addr1_timer);
    }

    String host2_timer = server.arg("url2_timer");
    if (host2_timer != "") {
      saveToEEPROM(String(host2_timer), url_addr2_timer);
      interval2 = host2_timer.toInt() * 1000;
    } else {
      host2_timer = getFromEEPROM(url_addr2_timer);
    }

    sendHead();
    server.sendContent("<form method=\"post\">");
    server.sendContent("<p class=\"p_p\">Web webhook URL:</p>http:// <input type=\"text\" name=\"url1\" value=\"" + host1 + "\"> / <input type=\"text\" name=\"url1_params\" value=\"" + host1_params + "\"> ⏱️: <input type=\"text\" name=\"url1_timer\" size=6 value=\"" + host1_timer + "\">(sec)");
    server.sendContent("<p class=\"p_p\">Telegram bot API token:</p><input type=\"text\" name=\"url2\" value=\"" + host2 + "\"> Chat id:<input type=\"text\" name=\"chat_id\" value=\"" + chatId + "\"> ⏱️: <input type=\"text\" name=\"url2_timer\" size=6 value=\"" + host2_timer + "\">(sec)");
    server.sendContent("<p class=\"p_p\"><input type=\"submit\" value=\"SAVE\"></p></form>");
    sendTail();
}

void checkCron()
{
  if (WiFi.status() != WL_CONNECTED || stop_wifi) {
    return;
  }
  
  // Timer 1 for HTTP
  unsigned long currentMillis1 = millis();
  if (just_started || (currentMillis1 - previousMillis1 >= interval1 && interval1 > 5000)) {
    Serial.println("Cron 1 triggered");
    previousMillis1 = currentMillis1;

    WiFiClient client;
    if (!client.connect(String(getFromEEPROM(url_addr1)), 80)) {
      Serial.println("connection failed to " + String(getFromEEPROM(url_addr1)));
    } else {
      client.print(String("GET ") + "/" + String(getFromEEPROM(url_addr1_params)) + " HTTP/1.1\r\n" +
                  "Host: " + String(getFromEEPROM(url_addr1)) + "\r\n" + 
                  "Connection: close\r\n\r\n");
      Serial.println("Request " + String(getFromEEPROM(url_addr1_params)) +" to " + String(getFromEEPROM(url_addr1)) + "  sent! )))");
      delay(500);
    }
  }

  // Timer 2 for Telegram
  unsigned long currentMillis2 = millis();
  if (just_started || (currentMillis2 - previousMillis2 >= interval2 && interval2 > 5000)) {
    Serial.println("Cron 2 triggered");
    previousMillis2 = currentMillis2;

    WiFiClientSecure client2;

    client2.setInsecure();
    if (!client2.connect("api.telegram.org", 443)) {
      Serial.println("connection failed to telegram");
    } else {
      HTTPClient http;
      http.begin(client2, String("https://api.telegram.org/") + String(getFromEEPROM(url_addr2)) + "/sendMessage");
      http.addHeader("Content-Type", "application/json");

      String httpRequestData = "{\"chat_id\": \"" + String(getFromEEPROM(chat_id_addr)) + "\", \"text\": \"🟢...\", \"disable_notification\": true}";
      if (just_started) {
        httpRequestData = "{\"chat_id\": \"" + String(getFromEEPROM(chat_id_addr)) + "\", \"text\": \"🕺🎉 Electricity is back!... \", \"disable_notification\": true}";
      }

      int httpResponseCode = http.POST(httpRequestData);

      Serial.print("Telegram response code: ");
      Serial.println(httpResponseCode);
      String payload = http.getString();
      Serial.println(payload); 
      http.end();

      delay(100);
    }
  }

  just_started = false;
}

void reconnectWiFi()
{
  if (WiFi.status() != WL_CONNECTED && !stop_wifi) {
    // Timer for WiFi reconnecting
    unsigned long currentMillisWiFi = millis();
    if (currentMillisWiFi - previousMillisWiFi >= 30000) {
      previousMillisWiFi = currentMillisWiFi;
      StartWiFi();
    }
  }

  if (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_BUILTIN, HIGH);
  } else {
    digitalWrite(LED_BUILTIN, LOW);      
  }
}

/////////////////////////// EEPROM FUNCTIONS //////////////////////
void clearEEPROM(int addr[2]) {
  for (int i = addr[0]; i < (addr[0] + addr[1]); i++) {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();
}

void saveSSIDAndPass(String new_ssid, String new_pass) {
  if (saveToEEPROM(new_ssid, ssid_addr)) Serial.println("SSID " + new_ssid + " SAVED");
  if (saveToEEPROM(new_pass, pass_addr)) Serial.println("PASS " + new_pass + " SAVED");
}
boolean saveToEEPROM(String value, int addr[2]) {
  clearEEPROM(addr);
  Serial.println("Pushing '" + value + "' to EEPROM " + String(addr[0]) + ".." + String(addr[0] + addr[1]));
  if (value.length() < addr[1]) {
    for (int i = 0; i < value.length(); i++) {
      EEPROM.write(i + addr[0], value[i]);
    }
    EEPROM.commit();
    return true;
  } else {
    Serial.println("Not pushed to EEPROM");
    return false;
  }
}
String getFromEEPROM(int addr[2]) {
  String result = "";
  int c = 0;
  int pointer = addr[0];
  while (pointer < (addr[0] + addr[1]) && pointer != 512) {
    c = EEPROM.read(pointer);
    if (c != 0) {
      result += char(c);
      pointer++;
    } else pointer = 512;
  }
  Serial.println("from EEPROM " + String(addr[0]) + ".." + String(addr[0] + addr[1]) + ": " + result);
  return result;
}
/////////////////////////////// END EEPROM FUNCTIONS ///////////////////////////////

/////////////////////////////// Wi-Fi CLIENT ////////////////////////////
void StartWiFi() {
  delay(200);
  Serial.println("Starting Wi-Fi.....");
  ssid = getFromEEPROM(ssid_addr);
  pass = getFromEEPROM(pass_addr);

  host1 = getFromEEPROM(url_addr1);
  host2 = getFromEEPROM(url_addr2);

  if (ssid != "") {
    char s[ssid.length()];
    ssid.toCharArray(s, ssid.length() + 1);
    char p[pass.length()];
    if (pass.length() > 0) pass.toCharArray(p, pass.length() + 1);
    else pass.toCharArray(p, pass.length());
    Serial.print("Access point name:");
    Serial.println(s);
    Serial.print("Pass:");
    Serial.println(p);
    WiFi.begin(s, p);
    int count = 0;
    while (WiFi.status() != WL_CONNECTED && count++ < 17) {
      delay(500);
      Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println(WiFi.localIP());
    } else Serial.println("Connecting to WiFi FAILED");
  }
}

void StopWiFi() {
  stop_wifi = true;
  if (WiFi.status() == WL_CONNECTED) WiFi.disconnect();
  Serial.println("WiFi disconnected");
}
String getEncryptionType(int thisType) {
  // read the encryption type and print out the name:
  switch (thisType) {
  case ENC_TYPE_WEP:
    return "WEP";
    break;
  case ENC_TYPE_TKIP:
    return "WPA";
    break;
  case ENC_TYPE_CCMP:
    return "WPA2";
    break;
  case ENC_TYPE_NONE:
    return "None";
    break;
  case ENC_TYPE_AUTO:
    return "Auto";
    break;
  default:
    return "WPA";
  }
}
/////////////////////////////// END Wi-Fi CLIENT ////////////////////////////////

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);

  delay(200);
  EEPROM.begin(512);
  delay(200);
  
  interval1 = String(getFromEEPROM(url_addr1_timer)).toInt() * 1000;
  interval2 = String(getFromEEPROM(url_addr2_timer)).toInt() * 1000;

  Serial.begin(115200);
  delay(100);
  Serial.println("");
  Serial.print("Configuring access point...");
  
  WiFi.softAP(ap_name, password);

  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);
  WiFi.softAP(ap_name, password);

  server.on("/", handleRoot);
  server.on("/events", handleEvents);
  server.on("/settings", []() {
    // http://<ip address>/settings?ssid=SSID&pass=12345678
    String s = server.arg("ssid");
    String p = server.arg("pass");
    page_title = "Settings";
    if (s != "") {
      Serial.println("Savingg");
      saveSSIDAndPass(s, p);
      s = getFromEEPROM(ssid_addr);
      p = getFromEEPROM(pass_addr);
      page_content = "Новая точка доступа " + s + " сохранена!";
      sendPage();
      StartWiFi();
    } else {
      int numSsid = WiFi.scanNetworks();
      String options = "<select name=ssid>";
      if (numSsid != -1) {
        for (int thisNet = 0; thisNet < numSsid; thisNet++) {
          String str(WiFi.SSID(thisNet));
          options += "<option value=\"" + str + "\">";
          if (getEncryptionType(WiFi.encryptionType(thisNet)) == "None") options += "*";
          options += " " + str + " (" + String(WiFi.RSSI(thisNet)) + "dB)";
          options += "</option>";
        }
      } else options += "<option></option>";
      options += "</select>";
      page_content = "<form method=\"post\"><table><tr><td width=\"50%\" class=\"t_r\"><small>Имя точки доступа<small></td><td rowspan=2 class=\"t_l\">" + options + "</td></tr><tr><td class=\"t_r\"><small>Acces point name</small></td></tr><tr><td colspan=2>&nbsp</td></tr><tr><td width=\"50%\" class=\"t_r\"><small>Пароль<small></td><td rowspan=2 class=\"t_l\"><input type=\"password\" name=\"pass\"></td></tr><tr><td class=\"t_r\"><small>Password</small></td></tr><tr><td colspan=2>&nbsp</td></tr><tr><td></td><td class=\"t_l\"><input type=\"submit\" value=\"SAVE\"></td></tr></table></form>";
      sendPage();
    }
  });

  server.begin();

  Serial.println("HTTP server started");
  StartWiFi();

  delay(1500);

}

void loop() {
  
    server.handleClient();

    checkCron();

    reconnectWiFi();
  
}
