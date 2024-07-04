#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <FS.h> // For LittleFS support
#include <LittleFS.h>
#include <AsyncHTTPRequest_Generic.h>
#include <unordered_set>
#include <NTPClient.h>
#include <WiFiUdp.h>


IPAddress staticIP(192, 168, 0, 9);  // Set the desired static IP address
IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress dns(8, 8, 8, 8);

AsyncWebServer server(80);

const bool debug = false;

// Device struct config
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

int requestSetupCounter = 0;
String IdsArray[numberDevices];

SemaphoreHandle_t xMutex;

void onRequestComplete(void* optParm, AsyncHTTPRequest* request, int readyState) {
  if (readyState == readyStateDone) {
    String& Id = *static_cast<String*>(optParm);
    Serial.println(Id);

    if (request->elapsedTime() > 10000 || request->responseHTTPString() == "TIMEOUT") {
      Serial.print("Device disconnected- TIME OUT: ");
      Serial.println(request->elapsedTime());
      Serial.println(Id);

      xSemaphoreTake(xMutex, portMAX_DELAY);
      registeredDevices[Id]["isConnected"] = false;
      registeredDevices[Id]["isExecutingRequest"] = false;
      xSemaphoreGive(xMutex);

      return;
    }

    if (request->responseHTTPString() == "NOT_CONNECTED" || request->responseHTTPString() == "UNKNOWN") {
      if (!WiFi.isConnected()) {
        Serial.println("Not connected!!!!!!");
      }
      
      xSemaphoreTake(xMutex, portMAX_DELAY);
      registeredDevices[Id]["isExecutingRequest"] = false;
      xSemaphoreGive(xMutex);
    } else {
      const char* body = request->responseLongText();
      Serial.println(body);

      xSemaphoreTake(xMutex, portMAX_DELAY);
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, body);

      if (!error) {
        if (doc.containsKey("id") && doc.containsKey("avgrssi") && doc.containsKey("distance") && doc.containsKey("rssi1m") && doc.size() == 4) {
          registeredDevices[Id]["distance"] = doc["distance"];
          registeredDevices[Id]["avgRSSI"] = doc["avgrssi"];
          registeredDevices[Id]["RSSI1m"] = doc["rssi1m"];
        } else {
          Serial.println("Invalid body of the request!");
        }
      } else {
        Serial.print("Failed to parse JSON - ");
        Serial.println(error.c_str());
      }

      registeredDevices[Id]["isExecutingRequest"] = false;
      xSemaphoreGive(xMutex);
    }
    
  }
}

void writeJSONToFile(const char* filename, JsonDocument& jsonDoc) {
    File file = LittleFS.open(filename, FILE_WRITE);
    if (!file) {
        Serial.println("Failed to open file for writing");
        return;
    }

    // Serialize JSON to file
    if (serializeJson(jsonDoc, file) == 0) {
        Serial.println("Failed to write JSON to file");
    }

    file.close();
}

bool readJSONFromFile(const char* filename, JsonDocument& jsonDoc) {
    File file = LittleFS.open(filename, FILE_READ);
    if (!file) {
        Serial.println("Failed to open file for reading");
        return false;
    }

    // Deserialize JSON from file
    DeserializationError error = deserializeJson(jsonDoc, file);
    if (error) {
        Serial.print("Failed to read JSON from file: ");
        Serial.println(error.c_str());
        file.close();
        return false;
    }
    file.close();

    for (JsonPair kv : jsonDoc.as<JsonObject>()) {
      String s(kv.key().c_str());

      IdsArray[requestSetupCounter] = s;
      requests[requestSetupCounter].onReadyStateChange(onRequestComplete, &IdsArray[requestSetupCounter]);

      requestSetupCounter++;
      jsonDoc[kv.key()]["isExecutingRequest"]=false;
    }

    return true;
}

class DeviceManager { 
public:
  DeviceManager(){};

  static int Count() {
    return registeredDevices.size();
  }

  // Returns 1 if a device is successfully added
  // Returns 0 if there is a validation error
  // Returns -1 if a device is already added
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

    String s(Id);
  
    int deviceStatusResult = isDeviceRegistered(Id, Ip);  
    xSemaphoreTake(xMutex, portMAX_DELAY);
    switch (deviceStatusResult) {
      case 1:
        if (debug)
          Serial.println("Device already added!");

        registeredDevices[s]["isConnected"] = true;
        registeredDevices[s]["isExecutingRequest"] = false;
        xSemaphoreGive(xMutex);
        return -1;
      default:       
        String ipStr(Ip);
        String typeStr(Type);

        registeredDevices[s]["ip"] = ipStr;
        registeredDevices[s]["type"] = typeStr;
        registeredDevices[s]["distance"] = -1;
        registeredDevices[s]["avgRSSI"] = -1;
        registeredDevices[s]["RSSI1m"] = -1;
        registeredDevices[s]["isConnected"] = true;
        registeredDevices[s]["isExecutingRequest"] = false;

        size_t ipLength = strlen(Ip);
        size_t urlSize = 7 + ipLength + 7; // "http://" + ip + "/scan\0"

        // Allocate memory for url dynamically
        char* url = (char*)malloc(urlSize);
        if (url == nullptr) {
          Serial.println("Failed to allocate memory for url");
          xSemaphoreGive(xMutex);
          return 0;
        }

        // Construct the URL
        strcpy(url, "http://");
        strcat(url, Ip);
        strcat(url, "/scan");

        registeredDevices[s]["requestScanUrl"] = url;

        writeJSONToFile("/savedDevices.json",registeredDevices);

        if (debug)
          serializeJson(registeredDevices, Serial);
        free(url);

        if (deviceStatusResult == 0) {
          IdsArray[requestSetupCounter] = s;
          requests[requestSetupCounter].onReadyStateChange(onRequestComplete, &IdsArray[requestSetupCounter]);

          requestSetupCounter++;
        }
        xSemaphoreGive(xMutex);
        return 1;
    }
  }

  static bool isDeviceRegistered(const char* Id) {
    xSemaphoreTake(xMutex, portMAX_DELAY);
    String s(Id);
    if (registeredDevices.containsKey(Id)) {
      xSemaphoreGive(xMutex);
      return true;
    }

    xSemaphoreGive(xMutex);
    return false;
  }

  // Returns 1 if a device is registered
  // Returns 0 if a device is not registered
  // Returns -1 if the id is registered but the ip doesn't match
  static int isDeviceRegistered(const char* Id, const char* Ip) {
     xSemaphoreTake(xMutex, portMAX_DELAY);
    String s(Id);
    if (!registeredDevices.containsKey(s)) {
      xSemaphoreGive(xMutex);
      return 0;
    }
    if (strcmp(registeredDevices[s]["ip"].as<const char*>(), Ip) != 0) {
      xSemaphoreGive(xMutex);
      return -1;
    }
    
    xSemaphoreGive(xMutex);
    return 1;
  }

  static void Print() {
    xSemaphoreTake(xMutex, portMAX_DELAY);
    if (debug)
      Serial.println(registeredDevices.size());
    for (JsonPair kv : registeredDevices.as<JsonObject>()) {
      if (debug)
        Serial.printf("%s(%s) - %s; Distance: %f; RSSI at 1m:%d\n", kv.key().c_str(), registeredDevices[kv.key()]["type"].as<const char*>(), registeredDevices[kv.key()]["ip"].as<const char*>(), registeredDevices[kv.key()]["distance"].as<double>(), registeredDevices[kv.key()]["RSSI1m"].as<int>());
    }
    xSemaphoreGive(xMutex);
  }
};

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


void serveStaticFile(AsyncWebServerRequest *request, const String& path, const String& contentType) {
  LittleFS.begin();
  File file = LittleFS.open(path, "r");
  if (!file) {
    request->send(404, "text/plain", "File not found");
  } else {
    request->send(file,path, contentType);
    file.close();
  }
}

bool CalibrationBegin = false;


void setup() {
  Serial.begin(115200);

  xMutex = xSemaphoreCreateMutex();

  if (!LittleFS.begin()) {
    if (debug)
      Serial.println("Failed to mount LittleFS");
    return;
  }

  readJSONFromFile("/savedDevices.json",registeredDevices);

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
  server.serveStatic("/", LittleFS, "/UI/").setDefaultFile("index.html");
  server.serveStatic("/UI/resources/", LittleFS, "/UI/resources/");

  server.on("/device", HTTP_POST, [](AsyncWebServerRequest *request) {
    IPAddress clientIP = request->client()->remoteIP();

    if (debug)
      Serial.printf("\n\n---Processing new POST /device request from %s---\n", clientIP.toString().c_str());
  
  },
  nullptr,
  [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (index == 0) {
      // New request started
      if (debug)
        Serial.println("Receiving request body...");
    }

    // Append data to the body
    String body = String((char*)data);

    if (index + len == total) {
      // All data received
      if (debug)
        Serial.printf("Raw request body: %s\n", body.c_str());

      // Parse JSON from the body
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, body.c_str());
      
      if (!error) {
        // Extract data from the JSON
        if (!(doc.containsKey("DeviceId") && doc.containsKey("CurrentIP") && doc.containsKey("Type") && doc.size() == 3)) {
          if (debug)
            Serial.println("Invalid body of the request!");
          request->send(400);        
          return;
        }

        if (debug)
          Serial.printf(
            "DeviceId: %s\nClientIP: %s\nType: %s\n", doc["DeviceId"].as<const char*>(), doc["CurrentIP"].as<const char*>(), doc["Type"].as<const char*>());

        switch (DeviceManager::AddNewDevice(doc["DeviceId"].as<const char*>(), doc["CurrentIP"].as<const char*>(), doc["Type"].as<const char*>())) {
          case -1:
            request->send(409);
            if (debug)
              Serial.println("Conflict occurred");
            return;
          case 0:
            request->send(400);
            if (debug)
              Serial.printf("Could not add device - %s", doc["DeviceId"].as<const char*>());
            return;
          default:
            if (debug)
              Serial.println("Device successfully added!");
            if (debug)
              Serial.println("Current devices:");
            DeviceManager::Print();
            request->send(200);
        }
      } else {
        if (debug)
          Serial.println(error.c_str());
        if (debug)
          Serial.println("Failed to parse JSON");
        request->send(400);      
      }
    }
  });

  // server.on("/status", HTTP_POST, [&](AsyncWebServerRequest *request) {
  //   IPAddress clientIP = request->client()->remoteIP();

  //   if (debug)
  //     Serial.printf("\n\n---Proccesing new POST /status request from %s---\nraw request body: %s\n", clientIP.toString().c_str(), request->arg("plain").c_str());   

  //   if (!request->hasArg("plain")) {
  //     request->send(400);
  //     if (debug)
  //       Serial.println("No data arg avalible");
       
  //     return;
  //   }

  //   JsonDocument doc;
  //   DeserializationError error = deserializeJson(doc, request->arg("plain").c_str());
  //   if (!error) {
  //     // Extract data from the JSON

  //     if (!(doc.containsKey("Id") && doc.size() == 1)) {
  //       if (debug)
  //         Serial.println("Invalid body of the request!");
  //       request->send(400);         
  //       return;
  //     }
  //     switch (DeviceManager::isDeviceRegistered(doc["Id"]. as<const char*>(), clientIP.toString().c_str())) {
  //       case 0:
  //         if (debug)
  //           Serial.println("Device is NOT registered!");
  //         request->send(400);

  //         return;
  //       case -1:
  //         if (debug)
  //           Serial.println("Device Ip mismatch. It has to be regitered again!");

  //         registeredDevices[doc["Id"].as<const char*>()]["isConnected"]=false;
  //         registeredDevices[doc["Id"].as<const char*>()]["isExecutingRequest"]=false;
  //         request->send(400);

  //         return;
  //       default:
  //         request->send(200);
  //         if (debug)
  //           Serial.println("Device is Registered");
  //           registeredDevices[doc["Id"].as<const char*>()]["isConnected"]=true;
  //           registeredDevices[doc["Id"].as<const char*>()]["isExecutingRequest"]=false;
  //         return;
  //     }

  //   }

  //   else {
  //     if (debug)
  //       Serial.println(error.c_str());
  //     if (debug)
  //       Serial.println("Failed to parse JSON");
  //     request->send(400);
       
  //     return;
  //   }
  // });

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
      obj["avgRSSI"] =registeredDevices[kv.key()]["avgRSSI"].as<int>();
      obj["rssi1m"] = registeredDevices[kv.key()]["RSSI1m"].as<int>();
      obj["isConnected"] = registeredDevices[kv.key()]["isConnected"].as<bool>();   
    }

    String jsonString;
    serializeJson(doc, jsonString);
    if (debug)
      Serial.println(jsonString);

    if (jsonString.isEmpty()) {
      request->send(500);
      return;
    }
    request->send(200, "application/json", jsonString);
  });

  server.on("/calibrationStatus", HTTP_POST, [&](AsyncWebServerRequest *request) {
    IPAddress clientIP = request->client()->remoteIP();

    if (debug)
      Serial.printf("\n\n---Proccesing new POST /calibrate request from %s---\nraw request body: %s\n", clientIP.toString().c_str(), request->arg("plain").c_str());

     

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, request->arg("plain").c_str());

    if (error) {
        if (debug) {
            Serial.println("Failed to parse JSON");
            Serial.println(error.c_str());
        }
        request->send(400);
          
        return;
    }
    
      // Extract data from the JSON
      if (!(doc.containsKey("Id") && doc.size() == 1)) {
        if (debug)
          Serial.println("Invalid body of the request!");
        request->send(400);
          
        return;
      }

      const char* recievedId=doc["Id"];

      if (!DeviceManager::isDeviceRegistered(recievedId)) {
        if (debug) {
            Serial.println("Error: Device is no longer connected or not registered");
        }
       request->send(404);
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
          request->send(500, "text/plain", "Internal Server Error: Memory allocation failed");
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
        request->send(200);
        free(url);

        return;

  });

  server.on("/startCalibration", HTTP_POST, [&](AsyncWebServerRequest *request) {
    IPAddress clientIP = request->client()->remoteIP();

    if (debug)
      Serial.printf("\n\n---Proccesing new POST /calibrate request from %s---\nraw request body: %s\n", clientIP.toString().c_str(), request->arg("plain").c_str());

     

    if (!CalibrationBegin) {
      Serial.println("Cannot start calibration without first invoking POST /calibrationStatus");
      request->send(400);
      return;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, request->arg("plain").c_str());
    if (error) {
        if (debug) {
            Serial.println("Failed to parse JSON");
            Serial.println(error.c_str());
        }
        request->send(400);
          
        return;
    }
      // Extract data from the JSON
      if (!(doc.containsKey("Id") && doc.size() == 1)) {
        if (debug)
          Serial.println("Invalid body of the request!");
        request->send(400);
          
        return;
      }

      const char* recievedId=doc["Id"];
      if (!DeviceManager::isDeviceRegistered(recievedId)) {
        if (debug)
          Serial.println("Error-device is no longer added!");
       request->send(404);
        return;
      }

      const char* savedDeviceIp=registeredDevices[recievedId]["Ip"];

      size_t ipLength = strlen(savedDeviceIp);
      size_t urlSize = 7 + ipLength + 19; // "http://" + ip + "/beginCalibration\0"
      
      // Allocate memory for url dynamically
      char* url = (char*)malloc(urlSize);
      if (url == nullptr) {
        Serial.println("Failed to allocate memory for url");
        request->send(500, "text/plain", "Internal Server Error: Memory allocation failed");
        return;
      }
      
      // Construct the URL
      strcpy(url, "http://");
      strcat(url, savedDeviceIp);
      strcat(url, "/beginCalibration");
      
      Serial.print("Sending a GET /beginCalibration request to ");
      Serial.println(url);

      const char* id = doc["Id"]. as<const char*>();
      requests[numberDevices].onReadyStateChange([id](void* optParm, AsyncHTTPRequest* request, int readyState) {
        onCalibrationComplete(id, request, readyState);
      });

      while (!requests[numberDevices].open("GET", url)) {
        Serial.println("Wating for requests to open");
      }

      requests[numberDevices].send();
      requests[numberDevices].setTimeout(10);

      request->send(200);
      free(url);
  });

  server.on("/stopCalibration", HTTP_POST, [&](AsyncWebServerRequest *request) {
    IPAddress clientIP = request->client()->remoteIP();

    if (debug)
      Serial.printf("\n\n---Proccesing new POST /stopCalibration request from %s---\nraw request body: %s\n", clientIP.toString().c_str(), request->arg("plain").c_str());

     

    if (!CalibrationBegin) {
      Serial.println("Cannot stop calibration if it has not yet began!");
      request->send(405);
      return;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, request->arg("plain").c_str());

    if (error) {
        if (debug) {
            Serial.println("Failed to parse JSON");
            Serial.println(error.c_str());
        }
        request->send(400);
          
        return;
    }

      if (!(doc.containsKey("Id") && doc.size() == 1)) {
        if (debug)
          Serial.println("Invalid body of the request!");
        request->send(400);
          
        return;
      }

      CalibrationBegin = false;

      const char* recievedId=doc["Id"];
      if (!DeviceManager::isDeviceRegistered(recievedId)) {
        if (debug)
          Serial.println("Error-device is no longer added!");
       request->send(404);
        return;
      }

      const char* savedDeviceIp=registeredDevices[recievedId]["Ip"];

      size_t ipLength = strlen(savedDeviceIp);
      size_t urlSize = 7 + ipLength + 11; // "http://" + ip + "/blinkOff\0"
      
      // Allocate memory for url dynamically
      char* url = (char*)malloc(urlSize);
      if (url == nullptr) {
        Serial.println("Failed to allocate memory for url");
        request->send(500, "text/plain", "Internal Server Error: Memory allocation failed");
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
      request->send(200);
      free(url);
  });

  server.on("/map", HTTP_POST, [](AsyncWebServerRequest *request) {
    IPAddress clientIP = request->client()->remoteIP();

    if (debug)
      Serial.printf("\n\n---Proccesing new POST /map request from %s---\nraw request body: %s\n", clientIP.toString().c_str(), request->arg("plain").c_str());

     

    File map = LittleFS.open("/map.json", "w+");
    map.print(request->arg("plain").c_str());
    map.close();

    request->send(200);
  });

  server.on("/map", HTTP_GET, [](AsyncWebServerRequest *request) {
    IPAddress clientIP = request->client()->remoteIP();

    if (debug)
      Serial.printf("\n\n---Proccesing new GET /map request from %s---\nraw request body: %s\n", clientIP.toString().c_str(), request->arg("plain").c_str());

     
    if (!LittleFS.exists("/map.json")) {
     request->send(404);
      return;
    }

    File map = LittleFS.open("/map.json", "r");
    request->send(200, "application/json", map.readString());
    map.close();
  });

  server.begin();
  if (debug)
    Serial.println("HTTP server started");
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
    unsigned long currentMillis = millis();

    // 2-second interval for sending requests
    if (registeredDevices.size() > 0 && !CalibrationBegin && currentMillis - previuosTime >= 2000) {
      previuosTime = currentMillis;

      size_t currentFreeHeap = ESP.getFreeHeap();
      if(debug)
      Serial.print("Free heap: ");
      if(debug)
      Serial.println(currentFreeHeap);
      
      xSemaphoreTake(xMutex, portMAX_DELAY);

      if(debug)
      Serial.println("---Starting new scan---");
      if(debug)
      Serial.println(registeredDevices.size());
      if(debug)
      serializeJsonPretty(registeredDevices, Serial);
      
      int requestCounter = 0;
      for (JsonPair kv : registeredDevices.as<JsonObject>()) {
        if (!registeredDevices.containsKey(kv.key())) {
          requestCounter++;
          continue;
        }
        
        if (registeredDevices[kv.key()]["isExecutingRequest"].as<bool>()) {
          requestCounter++;
          continue;
        }

        // Ensure previous request is properly handled before starting a new one
        if (requests[requestCounter].readyState() == readyStateDone || requests[requestCounter].readyState() == readyStateUnsent) {
          requests[requestCounter].abort();
          if (!requests[requestCounter].open("GET", registeredDevices[kv.key()]["requestScanUrl"].as<const char*>())) {
            Serial.println("Failed to open request");
            continue;
          }

          if(debug)
          Serial.printf("Starting request number %d:\n", requestCounter);
          requests[requestCounter].send();
          registeredDevices[kv.key()]["isExecutingRequest"] = true;
          requestCounter++;
        }      
      }

      xSemaphoreGive(xMutex);
    }
}