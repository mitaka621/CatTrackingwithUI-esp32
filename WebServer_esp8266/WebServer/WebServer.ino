#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <LittleFS.h>
#include <AsyncHTTPRequest_Generic.h>

const char* ssid = "ditoge03";
const char* password = "mitko111";

IPAddress staticIP(192, 168, 0, 9);  // Set the desired static IP address
IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0);

ESP8266WebServer server(80);

const int ledPin = 2;  // D2 pin on ESP8266

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
      Serial.println("One of the input args is longer than the declared length!");
      return 0;
    }

    if (!IPAddress::isValid(Ip)) {
      Serial.println("Cant add device because of invalid IP!");
      return 0;
    }

    switch (isDeviceRegistered(Id, Ip)) {
      case 1:
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
  void resetDevice(const char* Id)
  {
    for (int i = 0; i < counter; i++) {
      if (strcmp(devices[i].id, Id) == 0) {
          devices[i].distance=-1;
          devices[i].avgRSSI=-1;
      }
    }
  }

  void Print() {
    Serial.println(counter);
    for (int i = 0; i < counter; i++) {
      Serial.printf("%s(%s) - %s; Distance: %f\n", devices[i].id, devices[i].type, devices[i].ip, devices[i].distance);
    }
  }
};


DeviceManager manager;
int expectedRequests = 0;
int currentRequests = 0;
void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  WiFi.config(staticIP, gateway, subnet);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }

  Serial.println("Connected to WiFi");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  server.on("/device", HTTPMethod::HTTP_POST, [&manager]() {
    IPAddress clientIP = server.client().remoteIP();

    Serial.printf("\n\n---Proccesing new POST /device request from %s---\nraw request body: %s\n", clientIP.toString().c_str(), server.arg("plain").c_str());

    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    if (!error) {
      // Extract data from the JSON
      if (!(doc.containsKey("DeviceId") && doc.containsKey("CurrentIP") && doc.containsKey("Type") && doc.size() == 3)) {
        Serial.println("Invalid body of the request!");
        server.send(400);
        doc.garbageCollect();
        return;
      }

      Serial.printf(
        "DeviceId: %s\nClientIP: %s\nType: %s\n", doc["DeviceId"].as<String>().c_str(), doc["CurrentIP"].as<String>().c_str(), doc["Type"].as<String>().c_str());


      switch (manager.AddNewDevice(doc["DeviceId"].as<String>().c_str(), doc["CurrentIP"].as<String>().c_str(), doc["Type"].as<String>().c_str())) {
        case -1:
          server.send(409);
          Serial.println("Conflicted occured");
          doc.garbageCollect();
          return;
        case 0:
          server.send(400);
          Serial.printf("Could not add device - %s", doc["DeviceId"].as<String>().c_str());
          doc.garbageCollect();
          return;
        default:
          Serial.println("Device succesfully added!");
          Serial.println("Current devices:");
          manager.Print();
          server.send(200);
          doc.garbageCollect();
          ;
      }
    }

    else {
      Serial.println(error.c_str());
      Serial.println("Failed to parse JSON");
      server.send(400);
      doc.garbageCollect();
      return;
    }
  });

  server.on("/status", HTTPMethod::HTTP_GET, [&manager]() {
    IPAddress clientIP = server.client().remoteIP();

    Serial.printf("\n\n---Proccesing new GET /status request from %s---\nraw request body: %s\n", clientIP.toString().c_str(), server.arg("data").c_str());


    if (!server.hasArg("data")) {
      server.send(400);
      Serial.println("No data arg avalible");

      return;
    }

    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, server.arg("data"));
    if (!error) {
      // Extract data from the JSON

      if (!(doc.containsKey("Id") && doc.size() == 1)) {
        Serial.println("Invalid body of the request!");
        server.send(400);
        doc.garbageCollect();
        return;
      }
      switch (manager.isDeviceRegistered(doc["Id"].as<String>().c_str(), clientIP.toString().c_str())) {
        case 0:
          Serial.println("Device is NOT registered!");
          server.send(400);
          doc.garbageCollect();
          return;
        case -1:
          Serial.println("Device Ip mismatch. It has to be regitered again!");
          manager.removeDevice(doc["Id"].as<String>().c_str());
          server.send(400);
          doc.garbageCollect();
          return;
        default:
          server.send(200);
          Serial.println("Device is Registered");
          doc.garbageCollect();
          return;
      }
      if (!manager.isDeviceRegistered(doc["Id"].as<String>().c_str(), clientIP.toString().c_str())) {
        Serial.println("Device is NOT registered!");
        server.send(400);
        doc.garbageCollect();
        return;
      }
    }

    else {
      Serial.println(error.c_str());
      Serial.println("Failed to parse JSON");
      server.send(400);
      doc.garbageCollect();
      return;
    }
  });

  
  //first this endpoint has to be called (doesnt return nothing) and then /distances to get all of the calculated distances from all devices
  server.on("/start/scan", HTTPMethod::HTTP_GET, [&manager]() {
    expectedRequests = 0;
    currentRequests = 0;

    IPAddress clientIP = server.client().remoteIP();
    Serial.printf("\n\n---Proccesing new GET /start/scan request from %s---\n", clientIP.toString().c_str());

    Serial.println(manager.Count());
    if (manager.Count()==0) {
    server.send(405, "text/plain", "Error no devices.");
    return;
    }
    for (int i = 0; i < manager.Count(); i++) {
      Serial.printf("Starting request number %d:\n", i);
      char url[50] = "http://";
      strcat(url, manager.GetDevices()[i].ip);
      strcat(url, "/scan");

      requests[i].setDebug(false);
      requests[i].onReadyStateChange(onRequestComplete);
      if (requests[i].open("GET", url)) {
        expectedRequests++;
        requests[i].send();
      } else {
        Serial.print("Error with device: ");
        Serial.println(manager.GetDevices()[i].id);
      }
    }

    server.send(200);
    Serial.println("Waiting for all requests to complete.");

 
  });

  server.on("/distances", HTTPMethod::HTTP_GET, [&manager, &currentRequests, &expectedRequests]() {
    IPAddress clientIP = server.client().remoteIP();

    Serial.printf("\n\n---Proccesing new POST /distances request from %s---\nraw request body: %s\n", clientIP.toString().c_str(), server.arg("plain").c_str());

    if (currentRequests == expectedRequests && currentRequests != 0) {
      currentRequests=0;
      expectedRequests=0;

      const int capacity = 20 * numberDevices;
      StaticJsonDocument<capacity> doc;
      JsonArray array = doc.to<JsonArray>();

      for (int i = 0; i < manager.Count(); i++) {
        JsonObject obj = array.createNestedObject();
        obj["id"] = manager.GetDevices()[i].id;
        obj["type"] = manager.GetDevices()[i].type;
        obj["distance"] = manager.GetDevices()[i].distance;
        obj["avgRSSI"] = manager.GetDevices()[i].avgRSSI;

        manager.resetDevice(manager.GetDevices()[i].id);
      }

      String jsonString;
      serializeJson(doc, jsonString);
      Serial.println(jsonString);
      server.send(200, "application/json", jsonString);
      doc.garbageCollect();
      return;
    }


    Serial.printf("Still processing requests: %d/%d\n\n", currentRequests, expectedRequests);
    server.send(202);
  });
  server.begin();
  Serial.println("HTTP server started");

  // manager.AddNewDevice("1", "123.123.123.123", "ESP32");
  // manager.AddNewDevice("2", "123.123.123.123", "nekvodrugo");
  // manager.AddNewDevice("3", "123.123.123.123", "nekvodrugo");
  // Serial.println(manager.isDeviceRegistered("1", "123.123.123.123"));
  // Serial.println(manager.isDeviceRegistered("1", "13.123.123.14"));
  // Serial.println(manager.isDeviceRegistered("5", "13.123.123.14"));
}

void onRequestComplete(void* optParm, AsyncHTTPRequest* request, int readyState) {
  (void)optParm;

  
  if (readyState == readyStateDone) {
    Serial.print("Async request completed -> ");
    currentRequests++;
    Serial.println(request->responseHTTPString());
    Serial.print("Response: ");
    String body = request->responseText();
    Serial.println(body);

    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, body);
    if (!error) {
      // Extract data from the JSON
      if (!(doc.containsKey("id") && doc.containsKey("avgrssi") && doc.containsKey("distance") && doc.size() == 3)) {
        Serial.println("Invalid body of the request!");
        doc.garbageCollect();
        return;
      }

      manager.addDistanceAndRSSIToDevice(doc["id"].as<String>().c_str(), doc["distance"].as<double>(), doc["avgrssi"].as<int>());
      server.send(200);
      doc.garbageCollect();

    } else {
      Serial.println(error.c_str());
      Serial.println("Failed to parse JSON");
      doc.garbageCollect();
      return;
    }
  }
}

void loop() {
  server.handleClient();
}