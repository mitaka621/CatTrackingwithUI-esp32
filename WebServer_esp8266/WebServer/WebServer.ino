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

const bool debug = true;

//Device struct config
const int idLength = 50;
const int ipLength = 16;
const int typeLength = 10;
const int numberDevices = 20;

AsyncHTTPRequest requests[numberDevices + 1];

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

class DeviceManager {
public:
  DeviceManager(){};

  static int Count() {
    return registeredDevices.size();
  }

  //returns 1 if a device is succesfully added
  //returns 0 if there is a validation error
  //returns -1 if a device is already added
  static int AddNewDevice(const char* Id, const char* Ip, const char* Type) {
    if (registeredDevices.size() == numberDevices) {
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

    String s(Id);
    switch (isDeviceRegistered(Id, Ip)) {
      case 1:
        if (debug)
          Serial.println("Device already added!");

        registeredDevices[s]["isConnected"]=true;
        registeredDevices[s]["isExecutingRequest"]=false;
        return -1;
      default:

        
        String ipStr(Ip);
        String typeStr(Type);

        registeredDevices[s]["ip"]=ipStr;
        registeredDevices[s]["type"]=typeStr;
        registeredDevices[s]["distance"]=-1;
        registeredDevices[s]["avgRSSI"]=-1;
        registeredDevices[s]["RSSI1m"]=-1;
        registeredDevices[s]["isConnected"]=true;
        registeredDevices[s]["isExecutingRequest"]=false;
        
        size_t ipLength = strlen(Ip);
        size_t urlSize = 7 + ipLength + 7; // "http://" + ip + "/scan\0"
        
        // Allocate memory for url dynamically
        char* url = (char*)malloc(urlSize);
        if (url == nullptr) {
          Serial.println("Failed to allocate memory for url");
          return 0;
        }
        
        // Construct the URL
        strcpy(url, "http://");
        strcat(url, Ip);
        strcat(url, "/scan");

        registeredDevices[s]["requestScanUrl"]=url;

        serializeJson(registeredDevices, Serial);
        return 1;
    }
  }

  static bool isDeviceRegistered(const char* Id) {
    String s(Id);
    if(registeredDevices.containsKey(Id)&&registeredDevices[s]["isConnected"].as<bool>()){
      return true;
    }

    return false;
  }

  //returns 1 if a device is registered
  //returns 0 if a device is not registered
  //returns -1 if the id is registered but the ip doesnt match
  static int isDeviceRegistered(const char* Id, const char* Ip) {
    String s(Id);
    if (!registeredDevices.containsKey(s)) {
      return 0;
    }
    if (strcmp(registeredDevices[s]["ip"].as<const char*>(), Ip) != 0) {
      return -1;
    }

    return 1;
  }

  static void Print() {
    if (debug)
      Serial.println(registeredDevices.size());
    for (JsonPair kv : registeredDevices.as<JsonObject>()) {
      if (debug)
        Serial.printf("%s(%s) - %s; Distance: %f; RSSI at 1m:%d\n", kv.key().c_str(), registeredDevices[kv.key()]["type"].as<const char*>(), registeredDevices[kv.key()]["ip"].as<const char*>(), registeredDevices[kv.key()]["distance"].as<double>(), registeredDevices[kv.key()]["RSSI1m"].as<int>());
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


bool CalibrationBegin = false;
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

  WiFi.begin(config["ssid"]. as<const char*>(), config["password"]. as<const char*>());
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


  server.on("/device", HTTPMethod::HTTP_POST, [&registeredDevices]() {
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
          "DeviceId: %s\nClientIP: %s\nType: %s\n", doc["DeviceId"]. as<const char*>(), doc["CurrentIP"]. as<const char*>(), doc["Type"]. as<const char*>());


      switch (DeviceManager::AddNewDevice(doc["DeviceId"]. as<const char*>(), doc["CurrentIP"]. as<const char*>(), doc["Type"]. as<const char*>())) {
        case -1:
          server.send(409);
          if (debug)
            Serial.println("Conflicted occured");

          return;
        case 0:
          server.send(400);
          if (debug)
            Serial.printf("Could not add device - %s", doc["DeviceId"]. as<const char*>());

          return;
        default:
          if (debug)
            Serial.println("Device succesfully added!");
          if (debug)
            Serial.println("Current devices:");
          DeviceManager::Print();
          server.send(200);
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

  server.on("/status", HTTPMethod::HTTP_POST, [&registeredDevices]() {
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
      switch (DeviceManager::isDeviceRegistered(doc["Id"]. as<const char*>(), clientIP.toString().c_str())) {
        case 0:
          if (debug)
            Serial.println("Device is NOT registered!");
          server.send(400);

          return;
        case -1:
          if (debug)
            Serial.println("Device Ip mismatch. It has to be regitered again!");

          registeredDevices[doc["Id"].as<const char*>()]["isConnected"]=false;
          registeredDevices[doc["Id"].as<const char*>()]["isExecutingRequest"]=false;
          server.send(400);

          return;
        default:
          server.send(200);
          if (debug)
            Serial.println("Device is Registered");
            registeredDevices[doc["Id"].as<const char*>()]["isConnected"]=true;
            registeredDevices[doc["Id"].as<const char*>()]["isExecutingRequest"]=false;
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


  server.on("/distances", HTTPMethod::HTTP_GET, [&registeredDevices]() {
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

    for (JsonPair kv : registeredDevices.as<JsonObject>()) {
      JsonObject obj;
      obj["id"] =kv.key().c_str();
      obj["type"] = registeredDevices[kv.key()]["type"].as<const char*>();
      obj["distance"] = registeredDevices[kv.key()]["distance"].as<double>();
      obj["avgRSSI"] =registeredDevices[kv.key()]["avgRSSI"].as<int>();
      obj["rssi1m"] = registeredDevices[kv.key()]["RSSI1m"].as<int>();
      obj["isConnected"] = registeredDevices[kv.key()]["isConnected"].as<bool>();

      array.add<JsonObject>(obj);
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

  server.on("/calibrationStatus", HTTP_POST, [&registeredDevices, &CalibrationBegin]() {
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

    if (error) {
        if (debug) {
            Serial.println("Failed to parse JSON");
            Serial.println(error.c_str());
        }
        server.send(400);
        blacklistSet.insert(clientIP.toString().c_str());
        addToBlacklistJSON(clientIP.toString().c_str());
        return;
    }
    
      // Extract data from the JSON
      if (!(doc.containsKey("Id") && doc.size() == 1)) {
        if (debug)
          Serial.println("Invalid body of the request!");
        server.send(400);
        blacklistSet.insert(clientIP.toString().c_str());
        addToBlacklistJSON(clientIP.toString().c_str());
        return;
      }

      const char* recievedId=doc["Id"];

      if (!DeviceManager::isDeviceRegistered(recievedId)) {
        if (debug) {
            Serial.println("Error: Device is no longer connected or not registered");
        }
        server.send(404);
        return;
      } 

        CalibrationBegin = true;

        const char* savedDeviceIp=registeredDevices[recievedId]["Ip"];

        size_t ipLength = strlen(savedDeviceIp);
        size_t urlSize = 7 + ipLength + 8; // "http://" + ip + "/blinkOn\0"
        
        // Allocate memory for url dynamically
        char* url = (char*)malloc(urlSize);
        if (url == nullptr) {
          Serial.println("Failed to allocate memory for url");
          server.send(500, "text/plain", "Internal Server Error: Memory allocation failed");
          return;
        }
        
        // Construct the URL
        strcpy(url, "http://");
        strcat(url, savedDeviceIp);
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
        free(url);

        return;

  });

  server.on("/startCalibration", HTTP_POST, [&registeredDevices, &CalibrationBegin]() {
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
    if (error) {
        if (debug) {
            Serial.println("Failed to parse JSON");
            Serial.println(error.c_str());
        }
        server.send(400);
        blacklistSet.insert(clientIP.toString().c_str());
        addToBlacklistJSON(clientIP.toString().c_str());
        return;
    }
      // Extract data from the JSON
      if (!(doc.containsKey("Id") && doc.size() == 1)) {
        if (debug)
          Serial.println("Invalid body of the request!");
        server.send(400);
        blacklistSet.insert(clientIP.toString().c_str());
        addToBlacklistJSON(clientIP.toString().c_str());
        return;
      }

      const char* recievedId=doc["Id"];
      if (!DeviceManager::isDeviceRegistered(recievedId)) {
        if (debug)
          Serial.println("Error-device is no longer added!");
        server.send(404);
        return;
      }

      const char* savedDeviceIp=registeredDevices[recievedId]["Ip"];

      size_t ipLength = strlen(savedDeviceIp);
      size_t urlSize = 7 + ipLength + 19; // "http://" + ip + "/beginCalibration\0"
      
      // Allocate memory for url dynamically
      char* url = (char*)malloc(urlSize);
      if (url == nullptr) {
        Serial.println("Failed to allocate memory for url");
        server.send(500, "text/plain", "Internal Server Error: Memory allocation failed");
        return;
      }
      
      // Construct the URL
      strcpy(url, "http://");
      strcat(url, savedDeviceIp);
      strcat(url, "/beginCalibration");
      
      Serial.print("Sending a GET /beginCalibration request to ");
      Serial.println(url);

      requests[numberDevices].setDebug(false);
      const char* id = doc["Id"]. as<const char*>();
      requests[numberDevices].onReadyStateChange([id](void* optParm, AsyncHTTPRequest* request, int readyState) {
        onCalibrationComplete(id, request, readyState);
      });

      while (!requests[numberDevices].open("GET", url)) {
        Serial.println("Wating for requests to open");
      }

      requests[numberDevices].send();
      requests[numberDevices].setTimeout(10);

      server.send(200);
      free(url);
  });

  server.on("/stopCalibration", HTTP_POST, [&registeredDevices, &CalibrationBegin]() {
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

    if (error) {
        if (debug) {
            Serial.println("Failed to parse JSON");
            Serial.println(error.c_str());
        }
        server.send(400);
        blacklistSet.insert(clientIP.toString().c_str());
        addToBlacklistJSON(clientIP.toString().c_str());
        return;
    }

      if (!(doc.containsKey("Id") && doc.size() == 1)) {
        if (debug)
          Serial.println("Invalid body of the request!");
        server.send(400);
        blacklistSet.insert(clientIP.toString().c_str());
        addToBlacklistJSON(clientIP.toString().c_str());
        return;
      }

      CalibrationBegin = false;

      const char* recievedId=doc["Id"];
      if (!DeviceManager::isDeviceRegistered(recievedId)) {
        if (debug)
          Serial.println("Error-device is no longer added!");
        server.send(404);
        return;
      }

      const char* savedDeviceIp=registeredDevices[recievedId]["Ip"];

      size_t ipLength = strlen(savedDeviceIp);
      size_t urlSize = 7 + ipLength + 11; // "http://" + ip + "/blinkOff\0"
      
      // Allocate memory for url dynamically
      char* url = (char*)malloc(urlSize);
      if (url == nullptr) {
        Serial.println("Failed to allocate memory for url");
        server.send(500, "text/plain", "Internal Server Error: Memory allocation failed");
        return;
      }
      
      // Construct the URL
      strcpy(url, "http://");
      strcat(url, savedDeviceIp);
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
      free(url);
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


void onRequestComplete(const char* Id, AsyncHTTPRequest* request, int readyState,const char* Ip) {
  String s(Id);
  if (request->elapsedTime()>10000) {
    Serial.print("Removing device - TIME OUT: ");
    Serial.println(request->elapsedTime());
    Serial.println(Id);
  
    registeredDevices[s]["isConnected"]=false;
    registeredDevices[s]["isExecutingRequest"]=false;
    return;
  }
  if (readyState == readyStateDone) {
    if(!debug)
    Serial.print(".");
    if (debug)
      Serial.print("Async request completed -> ");

    if (debug)
      Serial.println(request->responseHTTPString());

    if (request->responseHTTPString() == "NOT_CONNECTED" || request->responseHTTPString() == "TIMEOUT") {
      if (!WiFi.isConnected()) {
      Serial.print("Not connected!!!!!!");
      }       
      registeredDevices[s]["isExecutingRequest"]=false;
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
        registeredDevices[s]["isExecutingRequest"]=false;
        return;
      }

      double distance=doc["distance"];
      int avgRSSI=doc["avgrssi"];
      int rssi1m=doc["rssi1m"];

      Serial.println(avgRSSI);

      registeredDevices[s]["distance"]=distance;
      registeredDevices[s]["avgRSSI"]=avgRSSI;
      registeredDevices[s]["RSSI1m"]=rssi1m;
    } else {
        
      if (debug)
        Serial.println(error.c_str());
      if (debug) {
        Serial.print("Failed to parse JSON - ");
        Serial.println(Id);
      }    
    }
    registeredDevices[s]["isExecutingRequest"]=false;
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

      String s(Id);
      registeredDevices[s]["RSSI1m"]=doc["rssi1m"].as<int>();

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


unsigned long previuosTime = 0;
void loop() {
  server.handleClient();

  //if no curretn scan is active begin scanning for the beacon
  unsigned long currentMillis = millis();

  if ( DeviceManager::Count() > 0 && !CalibrationBegin && currentMillis - previuosTime >= 2000) {
    previuosTime=currentMillis;

    if (debug)
      Serial.println("---Starting new scan---");
    if (debug)
      Serial.println(DeviceManager::Count());
    if(debug)
      serializeJsonPretty(registeredDevices, Serial);

    int requestCounter=0;
    for (JsonPair kv : registeredDevices.as<JsonObject>()) {
      if (!DeviceManager::isDeviceRegistered(kv.key().c_str())) {
        requestCounter++;
        continue;
      }

      if (registeredDevices[kv.key()]["isExecutingRequest"].as<bool>()) { // if true currently a request is executing to the given device
        requestCounter++;
        continue;
      }else{
        requests[requestCounter].abort();
      }

      if (debug)
        Serial.printf("Starting request number %d:\n", requestCounter);

      requests[requestCounter].onReadyStateChange([=,&registeredDevices](void* optParm, AsyncHTTPRequest* request, int readyState) {
        onRequestComplete(kv.key().c_str(), request, readyState,registeredDevices[kv.key()]["ip"].as<const char*>() );
      });
      
      while (!requests[requestCounter].open("GET", registeredDevices[kv.key()]["requestScanUrl"].as<const char*>())) {
        Serial.println("Wating for requests to open");
      }
             
      requests[requestCounter].send();
      registeredDevices[kv.key()]["isExecutingRequest"]=true;

      requestCounter++;
    }
  }
}