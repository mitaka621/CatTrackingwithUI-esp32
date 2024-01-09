#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <LittleFS.h>
#include <AsyncHTTPRequest_Generic.h>
#include <unordered_set>

IPAddress staticIP(192, 168, 0, 9);  // Set the desired static IP address
IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0);

ESP8266WebServer server(80);

const bool debug = true;

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

  void addDistanceAndRSSIToDevice(const char* Id, double distance, int rssi) {
    for (int i = 0; i < counter; i++) {
      if (strcmp(devices[i].id, Id) == 0) {
        devices[i].distance = distance;
        devices[i].avgRSSI = rssi;
        return;
      }
    }
  }
  void resetDevice(const char* Id) {
    for (int i = 0; i < counter; i++) {
      if (strcmp(devices[i].id, Id) == 0) {
        devices[i].distance = -1;
        devices[i].avgRSSI = -1;
      }
    }
  }

  void Print() {
    if (debug)
      Serial.println(counter);
    for (int i = 0; i < counter; i++) {
      if (debug)
        Serial.printf("%s(%s) - %s; Distance: %f\n", devices[i].id, devices[i].type, devices[i].ip, devices[i].distance);
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
  if (debug)
    Serial.printf("File:\n%s\n", LittleFS.open("/blacklist.json", "r").readString().c_str());
}


DeviceManager manager;
int expectedRequests = 0;
int currentRequests = 0;
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
  WiFi.config(staticIP, gateway, subnet);

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
    File file = LittleFS.open("/UI/index.html", "r");
    if (!file) {
      server.send(404, "text/plain", "File not found");
      return;
    }

    server.streamFile(file, "text/html");
    file.close();
  });

  server.on("/styles.css", HTTPMethod::HTTP_GET, []() {
    File file = LittleFS.open("/UI/styles.css", "r");
    if (!file) {
      server.send(404, "text/plain", "File not found");
      return;
    }

    server.streamFile(file, "text/css");
    file.close();
  });

  server.on("/editor.js", HTTPMethod::HTTP_GET, []() {
    File file = LittleFS.open("/UI/editor.js", "r");
    if (!file) {
      server.send(404, "text/plain", "File not found");
      return;
    }

    server.streamFile(file, "text/css");
    file.close();
  });

  server.on("/editor.js", HTTPMethod::HTTP_GET, []() {
    File file = LittleFS.open("/UI/editor.js", "r");
    if (!file) {
      server.send(404, "text/plain", "File not found");
      return;
    }

    server.streamFile(file, "application/javascript");
    file.close();
  });

  server.on("/sidemenu.js", HTTPMethod::HTTP_GET, []() {
    File file = LittleFS.open("/UI/sidemenu.js", "r");
    if (!file) {
      server.send(404, "text/plain", "File not found");
      return;
    }

    server.streamFile(file, "application/javascript");
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

  server.on("/status", HTTPMethod::HTTP_GET, [&manager]() {
    IPAddress clientIP = server.client().remoteIP();

    if (debug)
      Serial.printf("\n\n---Proccesing new GET /status request from %s---\nraw request body: %s\n", clientIP.toString().c_str(), server.arg("data").c_str());

    if (blacklistSet.find(clientIP.toString().c_str()) != blacklistSet.end()) {
      if (debug)
        Serial.println("Client IP is blacklisted");
      server.send(403);
      return;
    }

    if (!server.hasArg("data")) {
      server.send(400);
      if (debug)
        Serial.println("No data arg avalible");
      blacklistSet.insert(clientIP.toString().c_str());
      addToBlacklistJSON(clientIP.toString().c_str());
      return;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, server.arg("data"));
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


  //first this endpoint has to be called (doesnt return nothing) and then /distances to get all of the calculated distances from all devices
  server.on("/start/scan", HTTPMethod::HTTP_GET, [&manager]() {
    expectedRequests = 0;
    currentRequests = 0;

    IPAddress clientIP = server.client().remoteIP();
    if (debug)
      Serial.printf("\n\n---Proccesing new GET /start/scan request from %s---\n", clientIP.toString().c_str());

    if (debug)
      Serial.println(manager.Count());
    if (manager.Count() == 0) {
      server.send(405, "text/plain", "Error no devices.");
      return;
    }
    for (int i = 0; i < manager.Count(); i++) {
      if (debug)
        Serial.printf("Starting request number %d:\n", i);
      char url[50] = "http://";
      strcat(url, manager.GetDevices()[i].ip);
      strcat(url, "/scan");

      requests[i].setDebug(false);
      requests[i].onReadyStateChange(onRequestComplete);
      if (requests[i].open("GET", url)) {
        expectedRequests++;
        requests[i].send();
        requests[i].setTimeout(10);
      } else {
        if (debug)
          Serial.print("Error with device: ");
        if (debug)
          Serial.println(manager.GetDevices()[i].id);
      }
    }

    server.send(200);
    if (debug)
      Serial.println("Waiting for all requests to complete.");
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

    if (currentRequests == expectedRequests && currentRequests != 0) {
      currentRequests = 0;
      expectedRequests = 0;


      JsonDocument doc;
      JsonArray array = doc.to<JsonArray>();

      for (int i = 0; i < manager.Count(); i++) {
        JsonObject obj = array.add<JsonObject>();
        obj["id"] = manager.GetDevices()[i].id;
        obj["type"] = manager.GetDevices()[i].type;
        obj["distance"] = manager.GetDevices()[i].distance;
        obj["avgRSSI"] = manager.GetDevices()[i].avgRSSI;

        manager.resetDevice(manager.GetDevices()[i].id);
      }

      String jsonString;
      serializeJson(doc, jsonString);
      if (debug)
        Serial.println(jsonString);
      server.send(200, "application/json", jsonString);

      return;
    }


    if (debug)
      Serial.printf("Still processing requests: %d/%d\n\n", currentRequests, expectedRequests);
    server.send(202);
  });



  server.begin();
  if (debug)
    Serial.println("HTTP server started");

  // manager.AddNewDevice("1", "123.123.123.123", "ESP32");
  // manager.AddNewDevice("2", "123.123.123.123", "nekvodrugo");
  // manager.AddNewDevice("3", "123.123.123.123", "nekvodrugo");
  // if(debug)
  //Serial.println(manager.isDeviceRegistered("1", "123.123.123.123"));
  // if(debug)
  //Serial.println(manager.isDeviceRegistered("1", "13.123.123.14"));
  // if(debug)
  //Serial.println(manager.isDeviceRegistered("5", "13.123.123.14"));
}

void onRequestComplete(void* optParm, AsyncHTTPRequest* request, int readyState) {
  (void)optParm;


  if (readyState == readyStateDone) {
    if (debug)
      Serial.print("Async request completed -> ");
    currentRequests++;
    if (debug)
      Serial.println(request->responseHTTPString());
    if (debug)
      Serial.print("Response: ");
    String body = request->responseText();
    if (debug)
      Serial.println(body);

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, body);
    if (!error) {
      // Extract data from the JSON
      if (!(doc.containsKey("id") && doc.containsKey("avgrssi") && doc.containsKey("distance") && doc.size() == 3)) {
        if (debug)
          Serial.println("Invalid body of the request!");

        return;
      }

      manager.addDistanceAndRSSIToDevice(doc["id"].as<String>().c_str(), doc["distance"].as<double>(), doc["avgrssi"].as<int>());
      server.send(200);


    } else {
      if (debug)
        Serial.println(error.c_str());
      if (debug)
        Serial.println("Failed to parse JSON");

      return;
    }
  }
}

int64_t previuosTime = 0;
void loop() {
  server.handleClient();
}