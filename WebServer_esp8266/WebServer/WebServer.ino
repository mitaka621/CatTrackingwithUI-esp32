#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <AsyncHTTPRequest_Generic.h>
#include <unordered_set>
#include <NTPClient.h>
#include <WiFiUdp.h>

IPAddress staticIP(192, 168, 0, 9);  // Set the desired static IP address
IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress dns(8, 8, 8, 8);

ESP8266WebServer server(80);

const bool debug = false;

//Device struct config
const int idLength = 50;
const int ipLength = 16;
const int typeLength = 10;
const int numberDevices = 20;

AsyncHTTPRequest requests[numberDevices + 1];

struct Device {
  char id[idLength];
  char ip[ipLength];
  char type[typeLength];
  double distance = -1;
  int avgRSSI = -1;
  int RSSI1m = -1;

  // Constructor that initializes Device with provided values
  Device() {}
  Device(const char* deviceId, const char* deviceIp, const char* deviceType) {

    strncpy(id, deviceId, sizeof(id) - 1);
    id[sizeof(id) - 1] = '\0';  // Ensure null-termination

    strncpy(ip, deviceIp, sizeof(ip) - 1);
    ip[sizeof(ip) - 1] = '\0';  // Ensure null-termination

    strncpy(type, deviceType, sizeof(type) - 1);
    type[sizeof(type) - 1] = '\0';  // Ensure null-termination
  }
};



class DeviceManager {
private:
  Device devices[numberDevices];
  int counter = 0;
public:
  DeviceManager(){};

  int Count() const {
    return counter;
  }

  const Device* GetDevices() const {
    return devices;
  }

  //returns 1 if a device is succesfully added
  //returns 0 if there is a validation error
  //returns -1 if a device is already added
  int AddNewDevice(const char* Id, const char* Ip, const char* Type) {
    if (counter == numberDevices) {
      if (debug)
        Serial.println("There is no more room for more devices!");
      return 0;
    }

    int newIdLength = 0;
    while (Id[newIdLength] != '\0') {
      newIdLength++;
    }

    int newIpLength = 0;
    while (Ip[newIpLength] != '\0') {
      newIpLength++;
    }

    int newTypeLength = 0;
    while (Type[newTypeLength] != '\0') {
      newTypeLength++;
    }

    if (idLength < newIdLength || ipLength < newIpLength || typeLength < newTypeLength) {
      if (debug)
        Serial.println("One of the input args is longer than the declared length!");
      return 0;
    }

    if (!IPAddress::isValid(Ip)) {
      if (debug)
        Serial.println("Cant add device because of invalid IP!");
      return 0;
    }

    switch (isDeviceRegistered(Id, Ip)) {
      case 1:
        if (debug)
          Serial.println("Device already added!");
        return -1;
      case -1:
        removeDevice(Id);

      default:
        //final code executes even after case -1
        devices[counter++] = Device(Id, Ip, Type);
        return 1;
    }
  }

  bool isDeviceRegistered(const char* Id) {
    for (int i = 0; i < counter; i++) {
      if (strcmp(devices[i].id, Id) == 0) {
        return true;
      }
    }
    return false;
  }

  //returns 1 if a device is registered
  //returns 0 if a device is not registered
  //returns -1 if the id is registered but the ip doesnt match
  int isDeviceRegistered(const char* Id, const char* Ip) {
    for (int i = 0; i < counter; i++) {

      if (strcmp(devices[i].id, Id) == 0) {
        if (strcmp(devices[i].ip, Ip) != 0) {
          return -1;
        }
        return 1;
      }
    }
    return 0;
  }



  bool removeDevice(const char* Id) {

    for (int i = 0; i < counter; i++) {
      if (strcmp(devices[i].id, Id) == 0) {

        devices[i] = devices[--counter];
        return true;
      }
    }
    return false;
  }

  void addDistanceAndRSSIToDevice(const char* Id, double distance, int rssi, int rssi1m) {
    for (int i = 0; i < counter; i++) {
      if (strcmp(devices[i].id, Id) == 0) {
        devices[i].distance = distance;
        devices[i].avgRSSI = rssi;
        devices[i].RSSI1m = rssi1m;
        return;
      }
    }
  }

  void add1mRSSI(const char* Id, int newrssi) {
    for (int i = 0; i < counter; i++) {
      if (strcmp(devices[i].id, Id) == 0) {
        devices[i].RSSI1m = newrssi;
        return;
      }
    }
  }

  void resetDevice(const char* Id) {
    for (int i = 0; i < counter; i++) {
      if (strcmp(devices[i].id, Id) == 0) {
        devices[i].distance = -1;
        devices[i].avgRSSI = -1;
        devices[i].RSSI1m = -1;
      }
    }
  }

  void Print() {
    if (debug)
      Serial.println(counter);
    for (int i = 0; i < counter; i++) {
      if (debug)
        Serial.printf("%s(%s) - %s; Distance: %f; RSSI at 1m:%d\n", devices[i].id, devices[i].type, devices[i].ip, devices[i].distance, devices[i].RSSI1m);
    }
  }
};

void addToBlacklistJSON(const char* newIP) {
  const char* filename = "/blacklist.json";

  // Open the existing blacklist file
  File file = LittleFS.open(filename, "r+");

  // Move to the end of the file (before the ']')
  file.seek(file.size() - 1, SeekSet);

  // If not the first IP, add a comma separator
  if (file.size() > 3) {
    file.print(',');
  }

  // Add the new IP to the JSON array
  file.print("\"");
  file.print(newIP);
  file.print("\"]");

  if (debug)
    Serial.println("Added IP to blacklist successfully");
  file.close();
}


String readFile(const String& filePath) {
  File file = LittleFS.open(filePath, "r");
  if (!file) {
    return "";  // Handle file not found or other error
  }

  // Read the content of the file into a String
  String content = file.readString();
  file.close();
  return content;
}


 WiFiUDP ntpUDP;
 NTPClient timeClient(ntpUDP, "pool.ntp.org");
// bool SendNotification(char* title, char* content, int type) {

//   timeClient.update();
//   unsigned long dateTime = timeClient.getEpochTime();

//   JsonDocument doc;
//   doc["dataSource"] = "Cluster0";
//   doc["database"] = "esp8266";
//   doc["collection"] = "Notifications";

//   doc["document"]["title"] = title;
//   doc["document"]["content"] = content;
//   doc["document"]["dateTime"] = dateTime;
//   doc["document"]["isSeen"] = 0;
//   doc["document"]["type"] = type;

//   String body;
//   serializeJson(doc, body);

//   char* url = "https://worldtimeapi.org/api/timezone";

//   requests[numberDevices].onReadyStateChange([](void* optParm, AsyncHTTPRequest* request, int readyState) {
//   if (readyState == readyStateDone) {
//       if (debug)
//         Serial.println(request->responseHTTPString());
//   }
//   });   
//   while (!requests[numberDevices].open("GET", url)) {
//     Serial.println("Wating for reqest to open");
//   }
//   requests[numberDevices].send();         
//     return true;
// }

DeviceManager manager;
bool CalibrationBegin = false;
int expectedRequests = 1;
int currentRequests = 1;
std::unordered_set<std::string> blacklistSet;
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

  //loading blacklist from memory
  File blacklistFile = LittleFS.open("/blacklist.json", "r");

  if (!LittleFS.exists("/blacklist.json")) {
    blacklistFile.close();
    if (debug)
      Serial.println("Failed to open blacklist file. Creating a new one...");

    File file = LittleFS.open("/blacklist.json", "w+");


    file.print("[]");
    if (debug)
      Serial.println("File created");

    file.close();
  }

  JsonDocument blacklistDoc;

  contents = blacklistFile.readString();
  DeserializationError error = deserializeJson(blacklistDoc, contents);
  if (error) {
    if (debug)
      Serial.println("Failed to parse blacklist file");
    if (debug)
      Serial.println(error.f_str());
    blacklistFile.close();
    return;
  }

  if (debug)
    Serial.println(contents);

  blacklistFile.close();

  // Iterate through the JSON array and populate the unordered_set
  for (const auto& element : blacklistDoc.as<JsonArray>()) {
    blacklistSet.insert(element.as<const char*>());
  }

  WiFi.begin(config["ssid"].as<String>().c_str(), config["password"].as<String>().c_str());
  WiFi.config(staticIP, gateway, subnet, dns);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    if (debug)
      Serial.println("Connecting to WiFi...");
  }

  if (debug)
    Serial.println("Connected to WiFi");
  if (debug)
    Serial.print("IP address: ");
  if (debug)
    Serial.println(WiFi.localIP());

  timeClient.begin();
  timeClient.setTimeOffset(7200);

  //endpoints configuration
  server.on("/", HTTPMethod::HTTP_GET, []() {
    server.keepAlive(false);
    IPAddress clientIP = server.client().remoteIP();
    if (blacklistSet.find(clientIP.toString().c_str()) != blacklistSet.end()) {
      if (debug)
        Serial.println("Client IP is blacklisted");
      server.send(403);
      return;
    }

    File file = LittleFS.open("/UI/index.html", "r");
    if (!file) {
      server.send(404, "text/plain", "File not found");
      return;
    }

    server.streamFile(file, "text/html");
    file.close();
  });

  server.on("/styles.css", HTTPMethod::HTTP_GET, []() {
    server.keepAlive(false);
    IPAddress clientIP = server.client().remoteIP();
    if (blacklistSet.find(clientIP.toString().c_str()) != blacklistSet.end()) {
      if (debug)
        Serial.println("Client IP is blacklisted");
      server.send(403);
      return;
    }

    File file = LittleFS.open("/UI/styles.css", "r");
    if (!file) {
      server.send(404, "text/plain", "File not found");
      return;
    }

    server.streamFile(file, "text/css");
    file.close();
  });

  server.on("/editor.js", HTTPMethod::HTTP_GET, []() {
    server.keepAlive(false);
    IPAddress clientIP = server.client().remoteIP();
    if (blacklistSet.find(clientIP.toString().c_str()) != blacklistSet.end()) {
      if (debug)
        Serial.println("Client IP is blacklisted");
      server.send(403);
      return;
    }

    File file = LittleFS.open("/UI/editor.js", "r");
    if (!file) {
      server.send(404, "text/plain", "File not found");
      return;
    }

    server.streamFile(file, "text/javascript");
    file.close();
  });


  server.on("/sidemenu.js", HTTPMethod::HTTP_GET, []() {
    server.keepAlive(false);
    IPAddress clientIP = server.client().remoteIP();
    if (blacklistSet.find(clientIP.toString().c_str()) != blacklistSet.end()) {
      if (debug)
        Serial.println("Client IP is blacklisted");
      server.send(403);
      return;
    }

    File file = LittleFS.open("/UI/sidemenu.js", "r");
    if (!file) {
      server.send(404, "text/javascript", "File not found");
      return;
    }

    server.streamFile(file, "application/javascript");
    file.close();
  });

  server.on("/loadData.js", HTTPMethod::HTTP_GET, []() {
    server.keepAlive(false);
    IPAddress clientIP = server.client().remoteIP();
    if (blacklistSet.find(clientIP.toString().c_str()) != blacklistSet.end()) {
      if (debug)
        Serial.println("Client IP is blacklisted");
      server.send(403);
      return;
    }

    File file = LittleFS.open("/UI/loadData.js", "r");
    if (!file) {
      server.send(404, "text/plain", "File not found");
      return;
    }

    server.streamFile(file, "application/javascript");
    file.close();
  });


  server.on("/resources/cattt.svg", HTTPMethod::HTTP_GET, []() {
    server.keepAlive(false);
    IPAddress clientIP = server.client().remoteIP();
    if (blacklistSet.find(clientIP.toString().c_str()) != blacklistSet.end()) {
      if (debug)
        Serial.println("Client IP is blacklisted");
      server.send(403);
      return;
    }

    File file = LittleFS.open("/UI/resources/cattt.svg", "r");
    if (!file) {
      server.send(404, "text/plain", "File not found");
      return;
    }

    server.streamFile(file, "image/svg+xml");
    file.close();
  });

  server.on("/UI/resources/roomIcon.svg", HTTPMethod::HTTP_GET, []() {
    server.keepAlive(false);
    IPAddress clientIP = server.client().remoteIP();
    if (blacklistSet.find(clientIP.toString().c_str()) != blacklistSet.end()) {
      if (debug)
        Serial.println("Client IP is blacklisted");
      server.send(403);
      return;
    }

    File file = LittleFS.open("/UI/resources/roomIcon.svg", "r");
    if (!file) {
      server.send(404, "text/plain", "File not found");
      return;
    }

    server.streamFile(file, "image/svg+xml");
    file.close();
  });

  server.on("/UI/resources/wallIcon.svg", HTTPMethod::HTTP_GET, []() {
    server.keepAlive(false);
    IPAddress clientIP = server.client().remoteIP();
    if (blacklistSet.find(clientIP.toString().c_str()) != blacklistSet.end()) {
      if (debug)
        Serial.println("Client IP is blacklisted");
      server.send(403);
      return;
    }

    File file = LittleFS.open("/UI/resources/wallIcon.svg", "r");
    if (!file) {
      server.send(404, "text/plain", "File not found");
      return;
    }

    server.streamFile(file, "image/svg+xml");
    file.close();
  });


  server.on("/device", HTTPMethod::HTTP_POST, [&manager]() {
    IPAddress clientIP = server.client().remoteIP();

    if (debug)
      Serial.printf("\n\n---Proccesing new POST /device request from %s---\nraw request body: %s\n", clientIP.toString().c_str(), server.arg("plain").c_str());

    if (blacklistSet.find(clientIP.toString().c_str()) != blacklistSet.end()) {
      if (debug)
        Serial.println("Client IP is blacklisted");
      server.send(403);
      return;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    if (!error) {
      // Extract data from the JSON
      if (!(doc.containsKey("DeviceId") && doc.containsKey("CurrentIP") && doc.containsKey("Type") && doc.size() == 3)) {
        if (debug)
          Serial.println("Invalid body of the request!");
        server.send(400);
        blacklistSet.insert(clientIP.toString().c_str());
        addToBlacklistJSON(clientIP.toString().c_str());
        return;
      }

      if (debug)
        Serial.printf(
          "DeviceId: %s\nClientIP: %s\nType: %s\n", doc["DeviceId"].as<String>().c_str(), doc["CurrentIP"].as<String>().c_str(), doc["Type"].as<String>().c_str());


      switch (manager.AddNewDevice(doc["DeviceId"].as<String>().c_str(), doc["CurrentIP"].as<String>().c_str(), doc["Type"].as<String>().c_str())) {
        case -1:
          server.send(409);
          if (debug)
            Serial.println("Conflicted occured");

          return;
        case 0:
          server.send(400);
          if (debug)
            Serial.printf("Could not add device - %s", doc["DeviceId"].as<String>().c_str());

          return;
        default:
          if (debug)
            Serial.println("Device succesfully added!");
          if (debug)
            Serial.println("Current devices:");
          manager.Print();
          server.send(200);

          ;
      }
    }

    else {
      if (debug)
        Serial.println(error.c_str());
      if (debug)
        Serial.println("Failed to parse JSON");
      server.send(400);
      blacklistSet.insert(clientIP.toString().c_str());
      addToBlacklistJSON(clientIP.toString().c_str());
    }
  });

  server.on("/status", HTTPMethod::HTTP_POST, [&manager]() {
    IPAddress clientIP = server.client().remoteIP();

    if (debug)
      Serial.printf("\n\n---Proccesing new POST /status request from %s---\nraw request body: %s\n", clientIP.toString().c_str(), server.arg("plain").c_str());

    if (blacklistSet.find(clientIP.toString().c_str()) != blacklistSet.end()) {
      if (debug)
        Serial.println("Client IP is blacklisted");
      server.send(403);
      return;
    }

    if (!server.hasArg("plain")) {
      server.send(400);
      if (debug)
        Serial.println("No data arg avalible");
      blacklistSet.insert(clientIP.toString().c_str());
      addToBlacklistJSON(clientIP.toString().c_str());
      return;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain").c_str());
    if (!error) {
      // Extract data from the JSON

      if (!(doc.containsKey("Id") && doc.size() == 1)) {
        if (debug)
          Serial.println("Invalid body of the request!");
        server.send(400);
        blacklistSet.insert(clientIP.toString().c_str());
        addToBlacklistJSON(clientIP.toString().c_str());
        return;
      }
      switch (manager.isDeviceRegistered(doc["Id"].as<String>().c_str(), clientIP.toString().c_str())) {
        case 0:
          if (debug)
            Serial.println("Device is NOT registered!");
          server.send(400);

          return;
        case -1:
          if (debug)
            Serial.println("Device Ip mismatch. It has to be regitered again!");
          manager.removeDevice(doc["Id"].as<String>().c_str());
          server.send(400);

          return;
        default:
          server.send(200);
          if (debug)
            Serial.println("Device is Registered");

          return;
      }

    }

    else {
      if (debug)
        Serial.println(error.c_str());
      if (debug)
        Serial.println("Failed to parse JSON");
      server.send(400);
      blacklistSet.insert(clientIP.toString().c_str());
      addToBlacklistJSON(clientIP.toString().c_str());
      return;
    }
  });


  server.on("/distances", HTTPMethod::HTTP_GET, [&manager, &currentRequests, &expectedRequests]() {
    IPAddress clientIP = server.client().remoteIP();

    if (debug)
      Serial.printf("\n\n---Proccesing new POST /distances request from %s---\nraw request body: %s\n", clientIP.toString().c_str(), server.arg("plain").c_str());

    if (blacklistSet.find(clientIP.toString().c_str()) != blacklistSet.end()) {
      if (debug)
        Serial.println("Client IP is blacklisted");
      server.send(403);
      return;
    }

    JsonDocument doc;
    JsonArray array = doc.to<JsonArray>();

    for (int i = 0; i < manager.Count(); i++) {
      JsonObject obj = array.add<JsonObject>();
      obj["id"] = manager.GetDevices()[i].id;
      obj["type"] = manager.GetDevices()[i].type;
      obj["distance"] = manager.GetDevices()[i].distance;
      obj["avgRSSI"] = manager.GetDevices()[i].avgRSSI;
      obj["rssi1m"] = manager.GetDevices()[i].RSSI1m;
    }

    String jsonString;
    serializeJson(doc, jsonString);
    if (debug)
      Serial.println(jsonString);

    if (jsonString.isEmpty()) {
      server.send(500);
      return;
    }
    server.send(200, "application/json", jsonString);
  });

  server.on("/calibrationStatus", HTTP_POST, [&manager, &CalibrationBegin]() {
    IPAddress clientIP = server.client().remoteIP();

    if (debug)
      Serial.printf("\n\n---Proccesing new POST /calibrate request from %s---\nraw request body: %s\n", clientIP.toString().c_str(), server.arg("plain").c_str());

    if (blacklistSet.find(clientIP.toString().c_str()) != blacklistSet.end()) {
      if (debug)
        Serial.println("Client IP is blacklisted");
      server.send(403);
      return;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    if (!error) {
      // Extract data from the JSON
      if (!(doc.containsKey("Id") && doc.size() == 1)) {
        if (debug)
          Serial.println("Invalid body of the request!");
        server.send(400);
        blacklistSet.insert(clientIP.toString().c_str());
        addToBlacklistJSON(clientIP.toString().c_str());
        return;
      }

      if (manager.isDeviceRegistered(doc["Id"].as<String>().c_str())) {
        CalibrationBegin = true;

        for (int i = 0; i < manager.Count(); i++) {
          if (strcmp(manager.GetDevices()[i].id, doc["Id"].as<String>().c_str()) == 0) {

            char url[50] = "http://";
            strcat(url, manager.GetDevices()[i].ip);
            strcat(url, "/blinkOn");

            Serial.print("Sending a GET /blinkOn request to ");
            Serial.println(url);

            requests[numberDevices].setDebug(false);
            requests[numberDevices].onReadyStateChange([](void* optParm, AsyncHTTPRequest* request, int readyState) {});
            while (!requests[numberDevices].open("GET", url)) {
              Serial.println("Wating for reqest to open");
            }
            requests[numberDevices].send();
            server.send(200);
            return;
          }
        }

        if (debug)
          Serial.println("Error-device cannot be found!");
        CalibrationBegin = false;
        server.send(404);

      } else {
        if (debug)
          Serial.println("Error-device is no longer added!");
        server.send(404);
      }
    } else {
      if (debug)
        Serial.println(error.c_str());
      if (debug)
        Serial.println("Failed to parse JSON");
      server.send(400);
      blacklistSet.insert(clientIP.toString().c_str());
      addToBlacklistJSON(clientIP.toString().c_str());
    }
  });

  server.on("/startCalibration", HTTP_POST, [&manager, &CalibrationBegin]() {
    IPAddress clientIP = server.client().remoteIP();

    if (debug)
      Serial.printf("\n\n---Proccesing new POST /calibrate request from %s---\nraw request body: %s\n", clientIP.toString().c_str(), server.arg("plain").c_str());

    if (blacklistSet.find(clientIP.toString().c_str()) != blacklistSet.end()) {
      if (debug)
        Serial.println("Client IP is blacklisted");
      server.send(403);
      return;
    }

    if (!CalibrationBegin) {
      Serial.println("Cannot start calibration without first invoking POST /calibrationStatus");
      server.send(400);
      return;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    if (!error) {
      // Extract data from the JSON
      if (!(doc.containsKey("Id") && doc.size() == 1)) {
        if (debug)
          Serial.println("Invalid body of the request!");
        server.send(400);
        blacklistSet.insert(clientIP.toString().c_str());
        addToBlacklistJSON(clientIP.toString().c_str());
        return;
      }

      if (!manager.isDeviceRegistered(doc["Id"].as<String>().c_str())) {
        if (debug)
          Serial.println("Error-device is no longer added!");
        server.send(404);
        return;
      }

      for (int i = 0; i < manager.Count(); i++) {
        if (strcmp(manager.GetDevices()[i].id, doc["Id"].as<String>().c_str()) == 0) {

          char url[50] = "http://";
          strcat(url, manager.GetDevices()[i].ip);
          strcat(url, "/beginCalibration");

          Serial.print("Sending a GET /beginCalibration request to ");
          Serial.println(url);

          requests[numberDevices].setDebug(false);
          const char* id = doc["Id"].as<String>().c_str();
          requests[numberDevices].onReadyStateChange([id](void* optParm, AsyncHTTPRequest* request, int readyState) {
            onCalibrationComplete(id, request, readyState);
          });

          while (!requests[numberDevices].open("GET", url)) {
            Serial.println("Wating for requests to open");
          }

          requests[numberDevices].send();
          requests[numberDevices].setTimeout(10);

          server.send(200);
          return;
        }
      }

      Serial.print("Could not locate device!");
      server.send(404);
      return;


    } else {
      if (debug)
        Serial.println(error.c_str());
      if (debug)
        Serial.println("Failed to parse JSON");
      server.send(400);
      blacklistSet.insert(clientIP.toString().c_str());
      addToBlacklistJSON(clientIP.toString().c_str());
    }
  });

  server.on("/stopCalibration", HTTP_POST, [&manager, &CalibrationBegin]() {
    IPAddress clientIP = server.client().remoteIP();

    if (debug)
      Serial.printf("\n\n---Proccesing new POST /stopCalibration request from %s---\nraw request body: %s\n", clientIP.toString().c_str(), server.arg("plain").c_str());

    if (blacklistSet.find(clientIP.toString().c_str()) != blacklistSet.end()) {
      if (debug)
        Serial.println("Client IP is blacklisted");
      server.send(403);
      return;
    }

    if (!CalibrationBegin) {
      Serial.println("Cannot stop calibration if it has not yet began!");
      server.send(405);
      return;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    if (!error) {
      // Extract data from the JSON
      if (!(doc.containsKey("Id") && doc.size() == 1)) {
        if (debug)
          Serial.println("Invalid body of the request!");
        server.send(400);
        blacklistSet.insert(clientIP.toString().c_str());
        addToBlacklistJSON(clientIP.toString().c_str());
        return;
      }

      CalibrationBegin = false;
      if (manager.isDeviceRegistered(doc["Id"].as<String>().c_str())) {

        for (int i = 0; i < manager.Count(); i++) {
          if (strcmp(manager.GetDevices()[i].id, doc["Id"].as<String>().c_str()) == 0) {

            char url[50] = "http://";
            strcat(url, manager.GetDevices()[i].ip);
            strcat(url, "/blinkOff");

            Serial.print("Sending a GET /blinkOff request to ");
            Serial.println(url);

            requests[numberDevices].setDebug(false);
            requests[numberDevices].onReadyStateChange([](void* optParm, AsyncHTTPRequest* request, int readyState) {});

            while (!requests[numberDevices].open("GET", url)) {
              Serial.println("Wating for the request to open");
            }

            requests[numberDevices].send();
            Serial.println("Calibration canceled!");
            server.send(200);
            return;
          }
        }

        if (debug)
          Serial.println("Error-device cannot be found!");
        CalibrationBegin = false;
        server.send(404);

      } else {
        if (debug)
          Serial.println("Error-device is no longer added!");
        server.send(404);
      }
    } else {
      if (debug)
        Serial.println(error.c_str());
      if (debug)
        Serial.println("Failed to parse JSON");
      server.send(400);
      blacklistSet.insert(clientIP.toString().c_str());
      addToBlacklistJSON(clientIP.toString().c_str());
    }
  });

  server.on("/map", HTTP_POST, []() {
    IPAddress clientIP = server.client().remoteIP();

    if (debug)
      Serial.printf("\n\n---Proccesing new POST /map request from %s---\nraw request body: %s\n", clientIP.toString().c_str(), server.arg("plain").c_str());

    if (blacklistSet.find(clientIP.toString().c_str()) != blacklistSet.end()) {
      if (debug)
        Serial.println("Client IP is blacklisted");
      server.send(403);
      return;
    }

    File map = LittleFS.open("/map.json", "w+");
    map.print(server.arg("plain"));
    map.close();

    server.send(200);
  });

  server.on("/map", HTTP_GET, []() {
    IPAddress clientIP = server.client().remoteIP();

    if (debug)
      Serial.printf("\n\n---Proccesing new GET /map request from %s---\nraw request body: %s\n", clientIP.toString().c_str(), server.arg("plain").c_str());

    if (blacklistSet.find(clientIP.toString().c_str()) != blacklistSet.end()) {
      if (debug)
        Serial.println("Client IP is blacklisted");
      server.send(403);
      return;
    }
    if (!LittleFS.exists("/map.json")) {
      server.send(404);
      return;
    }

    File map = LittleFS.open("/map.json", "r");
    server.send(200, "application/json", map);
    map.close();
  });


  server.begin();
  if (debug)
    Serial.println("HTTP server started");
}


void onRequestComplete(const char* Id, AsyncHTTPRequest* request, int readyState) {
  if (readyState == readyStateDone) {
    if(!debug)
    Serial.print(".");
    if (debug)
      Serial.print("Async request completed -> ");

    if (debug)
      Serial.println(request->responseHTTPString());

    if (request->responseHTTPString() == "NOT_CONNECTED" || request->responseHTTPString() == "TIMEOUT") {
      Serial.print("Removing device: ");
      Serial.println(Id);
      manager.removeDevice(Id);
      currentRequests++;
      return;
    }
    if (debug)
      Serial.print("Response: ");
    char* body = request->responseLongText();
    if (debug)
      Serial.println(body);

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, body);
    if (!error) {
      // Extract data from the JSON
      if (!(doc.containsKey("id") && doc.containsKey("avgrssi") && doc.containsKey("distance") && doc.containsKey("rssi1m") && doc.size() == 4)) {
        if (debug)
          Serial.println("Invalid body of the request!");

        return;
      }

      manager.addDistanceAndRSSIToDevice(doc["id"].as<String>().c_str(), doc["distance"].as<double>(), doc["avgrssi"].as<int>(), doc["rssi1m"].as<int>());

      currentRequests++;
    } else {
      currentRequests++;
      if (debug)
        Serial.println(error.c_str());
      if (debug) {
        Serial.print("Failed to parse JSON - ");
        Serial.println(Id);
      }

      return;
    }
  }
}

void onCalibrationComplete(const char* Id, AsyncHTTPRequest* request, int readyState) {
  if (readyState == readyStateDone) {
    if (debug)
      Serial.print("Calibration request completed -> ");

    if (debug)
      Serial.println(request->responseHTTPString());

    if (debug)
      Serial.print("Response: ");
    char* body = request->responseLongText();
    if (debug)
      Serial.println(body);

    if (request->responseHTTPcode() == 404) {
      Serial.println("Couldnt calibrate! Beacon out of range!");
      CalibrationBegin = false;
      return;
    }

    if (request->responseHTTPcode() == 405) {
      Serial.println("Calibration not initialized!");
      return;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, body);
    if (!error) {
      // Extract data from the JSON
      if (!(doc.containsKey("rssi1m") && doc.size() == 1)) {
        if (debug)
          Serial.println("Invalid body of the request!");
        return;
      }

      manager.add1mRSSI(Id, doc["rssi1m"].as<int>());
      CalibrationBegin = false;
      return;


    } else {
      CalibrationBegin = false;
      if (debug)
        Serial.println(error.c_str());
      if (debug) {
        Serial.print("Failed to parse JSON - ");
        Serial.println(Id);
      }

      return;
    }
  }
}



void loop() {
  server.handleClient();

  //if no curretn scan is active begin scanning for the beacon

  if (currentRequests == expectedRequests && currentRequests != 0 && manager.Count() > 0 && !CalibrationBegin) {

    expectedRequests = 0;
    currentRequests = 0;

    if (debug)
      Serial.println("---Starting new scan---");

    if (debug)
      Serial.println(manager.Count());
    for (int i = 0; i < manager.Count(); i++) {
      if (debug)
        Serial.printf("Starting request number %d:\n", i);
      char url[50] = "http://";
      strcat(url, manager.GetDevices()[i].ip);
      strcat(url, "/scan");

      const char* id = manager.GetDevices()[i].id;

      requests[i].setDebug(false);
      requests[i].onReadyStateChange([id](void* optParm, AsyncHTTPRequest* request, int readyState) {
        onRequestComplete(id, request, readyState);
      });
      while (!requests[i].open("GET", url)) {
        Serial.println("Wating for requests to open");
      }
      expectedRequests++;
      requests[i].send();
      requests[i].setTimeout(10);
    }

    if (debug)
      Serial.println("Waiting for all requests to complete.");
  }
}