#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>

/* Set these to your desired credentials. */

const char * ap_name = "ArduStation";
const char * password = "qazxswedc";

unsigned long previousMillis = 0;
const long interval = 300000;

String host1 = "api.thingspeak.com";
String host2 = "api.telegram.org";

String ssid = "";
String pass = "";

unsigned long last_ping = 0;
int ping_intrv = 15000;
String page_title = "Страница управления ArduStation";
String page_content = "Content";
char css[] = "<style>.c1{position:absolute;left:15vw;right:15vw;background-color:#dedede;padding:20px;}ul.hr li{display:inline;margin-right:5px;}ul li{list-style-type:none;background-color:#ccc;border:1px solid #aaa;padding:10px;}ul.vert li{margin:4px;}a{color:black;text-decoration:none;}body{font-family:arial;}input{padding:9px;font-size:18px;line-height:18px;border:0;margin:0;}input[type=submit]{width:100%;}.t_l{text-align:left;}.t_r{text-align:right;}table{border:0;width:100%;}select{font-size:14px;line-height:14px;padding:9px}</style>";
String status_message = "&status=Hello! I run and sends the data. Your esp8266.";

bool stop_wifi = false;

int ssid_addr[] = {0, 32}; // адрес, смещение
int pass_addr[] = {32, 32};
int url_addr1[] = {64, 160};
int url_addr2[] = {224, 220};
int chat_id_addr[] = {444, 16};


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
  server.sendContent("<body>");
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

    String url2 = server.arg("url2");
    if (url2 != "") {
      host2 = url2;
      saveToEEPROM(host2, url_addr2);
    } else {
      host2 = getFromEEPROM(url_addr2);
    }

    String chatId = server.arg("chat_id");
    if (chatId != "") {
      saveToEEPROM(chatId, chat_id_addr);
    } else {
      chatId = getFromEEPROM(chat_id_addr);
    }

    sendHead();
    server.sendContent("<form method=\"post\">");
    server.sendContent("<p>HTTP://<input type=\"text\" name=\"url1\" value=\"" + host1 + "\"></p>");
    server.sendContent("<p>Telegram bot API token: <input type=\"text\" name=\"url2\" value=\"" + host2 + "\"> Chat id:<input type=\"text\" name=\"chat_id\" value=\"" + chatId + "\"></p>");
    server.sendContent("<input type=\"submit\" value=\"SAVE\"></form>");
    sendTail();
}

void checkCron()
{
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    Serial.println("Cron triggered");
    previousMillis = currentMillis;

    if (WiFi.status() == WL_CONNECTED && !stop_wifi) {
      
      WiFiClientSecure client2;

      client2.setInsecure();
      if (!client2.connect("api.telegram.org", 443)) {
        Serial.println("connection failed to telegram");
        return;
      }

      HTTPClient http;
      http.begin(client2, String("https://api.telegram.org/") + String(getFromEEPROM(url_addr2)) + "/getUpdates");
      int httpResponseCode = http.GET();
      Serial.println(httpResponseCode);
      String payload = http.getString();
      Serial.println(payload); 

      http.begin(client2, String("https://api.telegram.org/") + String(getFromEEPROM(url_addr2)) + "/sendMessage");
      http.addHeader("Content-Type", "application/json");
      String httpRequestData = "{\"chat_id\": \"" + String(getFromEEPROM(chat_id_addr)) + "\", \"text\": \"Online 📡\", \"disable_notification\": true}";           
      httpResponseCode = http.POST(httpRequestData);
      Serial.print("Telegram response code: ");
      Serial.println(httpResponseCode);
      payload = http.getString();
      Serial.println(payload); 
      http.end();

      delay(100);

      // if (!client2.connect("http://" + getFromEEPROM(url_addr1), 80)) {
      //   Serial.print("connection failed to http");
      //   return;
      // }

      // http.begin(client2, "http://" + String(getFromEEPROM(url_addr1)));
      // httpResponseCode = http.GET();
      // Serial.print("HTTP Response code: ");
      // Serial.println(httpResponseCode);
      // payload = http.getString();
      // Serial.println(payload); 
      // http.end();
    }
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
  } else return false;
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
      delay(1000);
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
  delay(200);
  EEPROM.begin(512);
  delay(500);
  Serial.begin(115200);
  delay(100);
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
}
