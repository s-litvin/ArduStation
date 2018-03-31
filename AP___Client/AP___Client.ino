#include <DHT.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h> 
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <ArduinoJson.h>
#include <CapacitiveSensor.h>

/* Set these to your desired credentials. */
// Uncomment whatever type you're using!
#define DHTTYPE DHT11   // DHT 11 
#define DHTPIN 2        // GPIO2
DHT dht(DHTPIN, DHTTYPE, 11);

CapacitiveSensor   cs_12_13 = CapacitiveSensor(12,13);  // GPIO 12 & 13

const char *ap_name = "ArduStation";
const char *password = "qazxswedc";
String host = "take-my-bit.ml";
String ssid = "";
String pass = "";
String port = "80";
String token = "ch=1";
unsigned long last_ping = 0;
int ping_intrv = 15000;
String page_title = "Страница управления ArduStation";
String page_content = "Content";
char css[] = "<style>.c1{position:absolute;left:15vw;right:15vw;background-color:#dedede;padding:20px;}ul.hr li{display:inline;margin-right:5px;}ul li{list-style-type:none;background-color:#ccc;border:1px solid #aaa;padding:10px;}ul.vert li{margin:4px;}a{color:black;text-decoration:none;}body{font-family:arial;}input{padding:9px;font-size:18px;line-height:18px;border:0;margin:0;}input[type=submit]{width:100%;}.t_l{text-align:left;}.t_r{text-align:right;}table{border:0;width:100%;}select{font-size:14px;line-height:14px;padding:9px}</style>";
String status_message = "&status=Hello! I run and sends the data. Your esp8266.";

bool stop_wifi = false;

int allowed_gpio[] = {2,4,5,12,13,14,15,16};

int ssid_addr[]        = {0, 32}; // адрес, смещение
int pass_addr[]        = {32, 32};
int url_addr[]         = {64, 64};
int port_addr[]        = {128, 8};
int token_addr[]       = {136, 32}; // {136, 16}


ESP8266WebServer server(80);

void sendPage()
{
    sendHead();
    server.sendContent(page_content);
    sendTail();
}
void sendHead()
{
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html", "");
    server.sendContent("<html><head><meta charset=\"utf-8\"><title>" + page_title + "</title>" + css + "</head>");
    server.sendContent("<body>");
}

void sendTail()
{
    server.sendContent("</body></html>");
}


// http://192.168.4.1
void handleRoot() {
  page_title = "Страница управления ArduStation";
  String status =  (WiFi.status() != WL_CONNECTED) ? "<span style=\"color:red\">disconnected</span>" : "<span style=\"color:green\">connected</span>";
  String ssid  = getFromEEPROM(ssid_addr);
  IPAddress tmp_ip = WiFi.localIP();
  String ip(String(tmp_ip[0]) + "." + String(tmp_ip[1]) + "." + String(tmp_ip[2]) + "." + String(tmp_ip[3]));
  page_content = "<table>";
  page_content += "<tr><td width=\"50%\" class=\"t_r\">Точка доступа: </td><td class=\"t_l\"><b>" + ssid + "</b></td></tr>";
  page_content += "<tr><td width=\"50%\" class=\"t_r\">Состояние: </td><td class=\"t_l\"><b>" + status + "</b></td></tr>";
  page_content += "<tr><td width=\"50%\" class=\"t_r\">Local IP: </td><td class=\"t_l\"><b><a href=\"http://" + ip + "\">" + ip + "</a></b></td></tr>"; 
  tmp_ip = WiFi.softAPIP();
  ip = (String(tmp_ip[0]) + "." + String(tmp_ip[1]) + "." + String(tmp_ip[2]) + "." + String(tmp_ip[3]));
  page_content += "<tr><td width=\"50%\" class=\"t_r\">Access point IP: </td><td class=\"t_l\"><b><a href=\"http://" + ip + "\">" + ip + "</a></b></td></tr>"; 
  page_content +="</table>";
  page_content += "<script src='https://cdnjs.cloudflare.com/ajax/libs/p5.js/0.6.0/p5.js'></script><script src=""></script>";
  sendPage();
}

void handleJs() {
  page_content = "";
  page_content += "<script src=\"https://code.jquery.com/jquery-3.3.1.min.js\" integrity=\"sha256-FgpCb/KJQlLNfOu91ta32o/NMZxltwRo8QtmkMRdAu8=\" crossorigin=\"anonymous\"></script><script src=\"https://cdnjs.cloudflare.com/ajax/libs/p5.js/0.6.0/p5.js\"></script><script src=\"https://github.com/shpikyliak/robot/blob/master/robot.js\" type=\"text/javascript\"></script>";
  sendPage();
}

/////////////////////////// EEPROM FUNCTIONS //////////////////////
void clearEEPROM(int addr[2])
{
  for (int i = addr[0]; i < (addr[0]+addr[1]); i++) { EEPROM.write(i, 0); }
  EEPROM.commit();
}

void saveSSIDAndPass(String new_ssid, String new_pass) 
{
  if (saveToEEPROM(new_ssid, ssid_addr)) Serial.println("SSID " + new_ssid + " SAVED");
  if (saveToEEPROM(new_pass, pass_addr)) Serial.println("PASS " + new_pass + " SAVED");
}
boolean saveToEEPROM(String value, int addr[2])
{
  clearEEPROM(addr);
  Serial.println("Pushing '" + value + "' to EEPROM " + String(addr[0]) + ".." + String(addr[0]+addr[1]));
  if (value.length() < addr[1]) {
    for (int i = 0; i < value.length(); i++) { EEPROM.write(i+addr[0], value[i]); }
    EEPROM.commit();
    return true;
  } else return false;
}
String getFromEEPROM(int addr[2])
{
  String result = "";
  int c = 0;
  int pointer = addr[0];
  while (pointer < (addr[0]+addr[1]) && pointer != 512) {
    c = EEPROM.read(pointer);
    if (c != 0) { 
      result += char(c);
      pointer++;
    } else pointer = 512;
  }
  Serial.println("from EEPROM "+String(addr[0]) + ".." +String(addr[0]+addr[1])+": " + result);  
  return result;
}
/////////////////////////////// END EEPROM FUNCTIONS ///////////////////////////////

/////////////////////////////// Wi-Fi CLIENT ////////////////////////////
void StartWiFi() 
{
  delay(200);
  Serial.println("Starting Wi-Fi.....");
  ssid = getFromEEPROM(ssid_addr);
  pass = getFromEEPROM(pass_addr);
  //pass = "324423";
  host = getFromEEPROM(url_addr);
  port = getFromEEPROM(port_addr);
  token = getFromEEPROM(token_addr);
  
  if (ssid != "" ) {
    char s[ssid.length()]; ssid.toCharArray(s, ssid.length()+1);
    char p[pass.length()]; 
    if (pass.length() > 0) pass.toCharArray(p, pass.length()+1);
    else pass.toCharArray(p, pass.length());
    Serial.print("Access point name:"); Serial.println(s);
    Serial.print("Pass:"); Serial.println(p);
    WiFi.begin(s, p);
    int count = 0;
    while (WiFi.status() != WL_CONNECTED && count++ < 17) {
      delay(1000); Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("WiFi connected. IP address: " + WiFi.localIP());
    } else Serial.println("Connecting to '" + ssid + "' with pass '" + pass + "' FAILED");
  }
}

void StopWiFi()
{
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
  }
}
/////////////////////////////// END Wi-Fi CLIENT ////////////////////////////////
String GPIOSelect(String param_name) 
{
  String str = "<select name=\"" + param_name + "\">";
  String tmp_gpio = "";
  for(int i=0; i < 8; i++) {
    tmp_gpio = String(allowed_gpio[i]);
    str += "<option value=\"" + tmp_gpio + "\">" + tmp_gpio + "</option>";
  }
  str += "</select>";
  return str;
}

void setup() {
  delay(1000);
  EEPROM.begin(512);
	delay(2000);
	Serial.begin(115200);
  delay(100);   
	Serial.print("Configuring access point...");
	WiFi.softAP(ap_name, password);

	IPAddress myIP = WiFi.softAPIP();
	Serial.print("AP IP address: ");
	Serial.println(myIP);
 
	server.on("/", handleRoot);
  server.on("/control", handleJs);
  server.on("/settings", []() {
    // http://<ip address>/settings?ssid=SSID&pass=12345678
    String s = server.arg("ssid");
    String p = server.arg("pass");
    page_title = "Settings";
    if (s != "") {
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
          if (getEncryptionType(WiFi.encryptionType(thisNet)) ==  "None") options += "*";
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
  delay(3500);

  if (WiFi.status() == WL_CONNECTED && !stop_wifi) {
      WiFiClient client;
      char site[host.length()]; host.toCharArray(site, host.length()+1);
      if (!client.connect(site, 80)) {
        Serial.println("connection failed to " + host);
        return;
      }
  }
}

void loop() {
	server.handleClient();
}

