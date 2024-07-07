#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <LittleFS.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <esp_now.h>
#include <time.h>
#include <stdio.h>

IPAddress staticIP(192, 168, 0, 9);  // Set the desired static IP address
IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress dns(8, 8, 8, 8);

AsyncWebServer server(80);

const bool debug = true;

const long gmtOffset_sec = 2 * 3600; // Adjust this for standard time (EET, UTC+2)
const int daylightOffset_sec = 1 * 3600; // Additional offset for DST (UTC+3)

const char *ntpServer = "time.google.com";

String GetLocalTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return "";
  }
  char isoDateTime[25];
  strftime(isoDateTime, sizeof(isoDateTime), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);  

  return String(isoDateTime);
}
JsonDocument notificationDoc;
JsonArray notificationsArray= notificationDoc.to<JsonArray>();
bool newNotifications=false;

void PushNotification(String title, String description, int notificationLevel) {
 
  JsonDocument currentNotification;
   
  currentNotification["title"]=title;
  currentNotification["description"]=description;
  currentNotification["notificationLevel"]=notificationLevel;
  currentNotification["dateTime"]=GetLocalTime();

  notificationsArray.add(currentNotification);

  newNotifications=true;
}

JsonDocument registeredDevices;   /*registeredDevices structure
                                    {
                                      deviceId {
                                        char id[idLength];
                                        char ip[ipLength];
                                        char type[typeLength];
                                        double distance = -1;
                                        int avgRSSI = -1;
                                        int RSSI1m = -1;
                                        bool isConnected
                                        bool isExecutingRequest=false,
                                        char requestScanUrl
                                      },
                                      .....
                                    }*/

void OnDataRecv(const esp_now_recv_info_t * esp_now_info, const uint8_t *incomingData, int len) {
  // Create a buffer to hold the incoming JSON data
  char jsonData[len + 1];
  memcpy(jsonData, incomingData, len);
  jsonData[len] = '\0';

  // Deserialize the JSON document
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, jsonData);

  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    return;
  }
  String id(doc["id"].as<const char*>());

  if (!registeredDevices.containsKey(id)) {
    PushNotification("New Device Connected - "+id,"A new reciever device has been detected and is now added to the device manager.",1);
  }else if(!registeredDevices[id]["isConnected"].as<bool>()){
    PushNotification("Device Reconnected - "+id,"A device previously lost connection has now returned online.",1);
  }

  if (doc["avgrssi"].as<int>()==-1&& registeredDevices[id]["BLEFound"].as<bool>()) {
    PushNotification(id+" lost connection to the BLE beacon","A reciever cannot connect to the selected BLE beacon. Connection lost.....",3);
    registeredDevices[id]["BLEFound"]=false;
  }

  registeredDevices[id]=doc;
  registeredDevices[id]["timeRecieved"]=millis();
  if (doc["avgrssi"].as<int>()!=-1) {
    registeredDevices[id]["BLEFound"]=true;
  } 

  if (debug)
  Serial.println("Current device collection:");
  if (debug)
  serializeJsonPretty(registeredDevices, Serial);

  if (!debug)
  Serial.print(".");
}

void setup() {
  Serial.begin(115200);

  if (!LittleFS.begin()) {
    if (debug)
      Serial.println("Failed to mount LittleFS");
    return;
  }

  //loading config from memory
  File configFile = LittleFS.open("/config.json", "r");
  String contents = configFile.readString();

  JsonDocument config;
  deserializeJson(config, contents);
  if (config.isNull()) {
    if (debug)
      Serial.printf("\n\nFailed to load config file! (data/config.json)");
    return;
  }

  if (debug)
    Serial.println();
  if (debug)
    Serial.println("Config loaded:");
  if (debug)
    Serial.println(contents);

  configFile.close();
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(config["ssid"]. as<const char*>(), config["password"]. as<const char*>());
  WiFi.config(staticIP, gateway, subnet, dns);

  int failCounter=0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    if (debug)
      Serial.println("Connecting to WiFi...");

    failCounter++;
    if (failCounter>=20) {
    ESP.restart();
    }
  }
  

  if (debug)
    Serial.println("Connected to WiFi");
  if (debug)
    Serial.print("IP address: ");
  if (debug)
    Serial.println(WiFi.localIP());

  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  //endpoints configuration
  server.serveStatic("/", LittleFS, "/UI/").setDefaultFile("index.html");
  server.serveStatic("/UI/resources/", LittleFS, "/UI/resources/");

  server.on("/distances", HTTP_GET, [&](AsyncWebServerRequest *request) {
    IPAddress clientIP = request->client()->remoteIP();

    if (debug)
      Serial.printf("\n\n---Proccesing new POST /distances request from %s---\nraw request body: %s\n", clientIP.toString().c_str(), request->arg("plain").c_str());

     

    JsonDocument doc;
    JsonArray array = doc.to<JsonArray>();
    for (JsonPair kv : registeredDevices.as<JsonObject>()) {     
      JsonObject obj = array.add<JsonObject>();

      obj["id"] =kv.key().c_str();
      obj["type"] = registeredDevices[kv.key()]["type"].as<const char*>();
      obj["distance"] = registeredDevices[kv.key()]["distance"].as<double>();
      obj["avgRSSI"] =registeredDevices[kv.key()]["avgrssi"].as<int>();
      obj["rssi1m"] = registeredDevices[kv.key()]["rssi1m"].as<int>();
      obj["isConnected"] = registeredDevices[kv.key()]["isConnected"].as<bool>();   
      obj["localIp"]=registeredDevices[kv.key()]["ip"].as<const char*>();
    }

    JsonDocument docToSend;
    docToSend["newNotifications"]=newNotifications;
    docToSend["data"]=doc;

    String jsonString;
    serializeJson(docToSend, jsonString);
    if (debug)
      Serial.println(jsonString);

    if (jsonString.isEmpty()) {
      request->send(500);
      return;
    }
    request->send(200, "application/json", jsonString);
  });

  server.on("/notifications", HTTP_GET, [&](AsyncWebServerRequest *request) {
    IPAddress clientIP = request->client()->remoteIP();

    if (debug)
      Serial.printf("\n\n---Proccesing new POST /notifications request from %s---\n ", clientIP.toString().c_str());

    Serial.println("notifications:");
    serializeJsonPretty(notificationDoc,Serial);

    String jsonString;
    serializeJson(notificationDoc, jsonString);
    if (debug)
      Serial.println(jsonString);

    if (jsonString.isEmpty()) {
      request->send(500);
      return;
    }
    request->send(200, "application/json", jsonString);

    newNotifications=false;
  });

  server.on("/newNotification", HTTP_GET, [&](AsyncWebServerRequest *request) {
    IPAddress clientIP = request->client()->remoteIP();

    if (debug)
      Serial.printf("\n\n---Proccesing new POST /notifications request from %s---\n ", clientIP.toString().c_str());

      JsonDocument latestNotification=notificationsArray[notificationsArray.size() - 1];
      String jsonString;
      serializeJson(latestNotification, jsonString);
      if (debug)
        Serial.println(jsonString);

      if (jsonString.isEmpty()) {
        request->send(500);
        return;
      }
      request->send(200, "application/json", jsonString);

      newNotifications=false;   
  });

  server.on("/map", HTTP_POST, [](AsyncWebServerRequest *request) {
    IPAddress clientIP = request->client()->remoteIP();

    if (debug)
      Serial.printf("\n\n---Proccesing new POST /map request from %s---\nraw request body: %s\n", clientIP.toString().c_str(), request->arg("plain").c_str());

     

    File map = LittleFS.open("/UI/resources/map.json", "w+");
    map.print(request->arg("plain").c_str());
    map.close();

    request->send(200);
  });

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  server.begin();
  
  if (debug)
    Serial.println("HTTP server started"); 

  esp_now_register_recv_cb(OnDataRecv);
}

unsigned long previuosTime = 0;
void loop() {
    unsigned long currentMillis = millis();

    //check for devices which havent supplyied data for over 10 sec every 10 sec
    if (currentMillis - previuosTime >= 10000) {
      Serial.print("Free heap: ");
      Serial.println(ESP.getFreeHeap());
      previuosTime = currentMillis;
      for (JsonPair kv : registeredDevices.as<JsonObject>()) {
        if (registeredDevices[kv.key()]["isConnected"].as<bool>()&&currentMillis-registeredDevices[kv.key()]["timeRecieved"].as<unsigned long>()>10000) {
          Serial.println("Device timeout");
          Serial.println(kv.key().c_str());
          registeredDevices[kv.key()]["isConnected"]=false;
          String id(kv.key().c_str());
          PushNotification("Device Disconnected - "+id,"A reciver has not sent any data for over 10 sec and is now marked as not connected",2);
        }   
      }
    }
}