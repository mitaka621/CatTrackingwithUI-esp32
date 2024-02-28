#include <NimBLEDevice.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include "FS.h"
#include <LittleFS.h>

#define FORMAT_LITTLEFS_IF_FAILED true

const char *DeviceId = "on couch";
const char *DeviceType = "ESP32";


const char *ssid = "ditoge03";
const char *password = "mitko111";

const char *ServerAddress = "http://192.168.0.9/";

//mac address for the BLE beacon
//const char beaconMAC[] = "e9:65:3c:b5:99:c1"; //big boy beacon
const char beaconMAC[] = "d0:3e:ed:1e:9a:9b"; //small beacon 5.0

int txPower = -44;  // Reference TxPower (RSSI) in dBm at 1 meter distance
const int N = 2;    //const from 2 to 4 for accuracy

NimBLEScan *pBLEScan;
const int RSSIsampleSize = 1;
int RSSIArr[RSSIsampleSize];
int arrCount = 0;
double distance;

#define LED 2

int compareAscending(const void *a, const void *b) {
  return (*(int *)b - *(int *)a);
}

double roundedDistance = -1;
double avgRSSI = -1;
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice *advertisedDevice) {
    if (strcmp(beaconMAC, advertisedDevice->getAddress().toString().c_str()) == 0) {
      int rssi = advertisedDevice->getRSSI();
      if (rssi != 0) {
        RSSIArr[arrCount] = rssi;
        arrCount++;
      }
    }

    if (arrCount == RSSIsampleSize) {  //takes 'RSSIsampleSize' samples of the RSSI value
      arrCount = 0;
      int SumRSSI = 0;

      qsort(RSSIArr, RSSIsampleSize, sizeof(int), compareAscending);
      for (int i = 0; i < RSSIsampleSize; i++) {
        SumRSSI += RSSIArr[i];
      }
      avgRSSI = SumRSSI / RSSIsampleSize;
      // Serial.println("----------");
      // Serial.print("Average RSSI: ");
      // Serial.println(avgRSSI);
      // Serial.print("Min RSSI: ");
      // Serial.println(RSSIArr[0]);
      // Serial.print("Max RSSI: ");
      // Serial.println(RSSIArr[RSSIsampleSize - 1]);



      distance = pow(10, (txPower - avgRSSI) / (10.0 * N));
      //Serial.print("Distance (estimated): ");
      //Serial.println(distance);

      // Serial.print("Rounded distance (Real): ");
      roundedDistance = round(distance * 2.0) / 2.0;
      //Serial.println(roundedDistance);

      pBLEScan->stop();
    }
  }
};



bool isConnected = false;
void ConnectToMainServer() {
  if (WiFi.status() == WL_CONNECTED) {

    HTTPClient http;

    // Create a JSON object
    JsonDocument jsonDoc;  // Adjust the size according to your JSON structure

    // Populate the JSON object with key-value pairs
    jsonDoc["DeviceId"] = DeviceId;
    jsonDoc["CurrentIP"] = WiFi.localIP().toString();
    jsonDoc["Type"] = DeviceType;

    // Serialize the JSON object to a string
    String jsonString;
    serializeJson(jsonDoc, jsonString);

    char fullURL[50];
    strcpy(fullURL, ServerAddress);
    strcat(fullURL, "device");

    Serial.printf("Sending a POST request to %s\n", fullURL);
    http.begin(fullURL);
    http.addHeader("Content-Type", "application/json");
    int httpResponseCode = http.POST(jsonString);


    if (httpResponseCode == 200) {
      Serial.println("Device registered successfully!");
      isConnected = true;
      return;
    }
    if (httpResponseCode == 409) {
      Serial.println("Device already added!");
      isConnected = true;
      return;
    }
    Serial.print("Failed to conect to server - status code ");
    Serial.println(httpResponseCode);

    http.end();
  } else {
    Serial.println("Not connected to WiFi");
  }
}

void CheckServerStatus() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    JsonDocument jsonDoc;

    jsonDoc["Id"] = DeviceId;

    char jsonString[200];
    serializeJson(jsonDoc, jsonString);

    char fullURL[300];
    strcpy(fullURL, ServerAddress);
    strcat(fullURL, "status");


    String str(fullURL);
    Serial.printf("Sending a POST request to %s\n", fullURL);
    http.begin(fullURL);
    int httpResponseCode = http.POST(jsonString);


    if (httpResponseCode == 200) {
      Serial.println("Status OK");
      return;
    }

    Serial.print("Status NOT OK. Trying to register device again... - status code ");
    Serial.println(httpResponseCode);
    isConnected = false;

    http.end();
  } else {
    Serial.println("Not connected to WiFi");
  }
}



WebServer server(80);

bool isScanning = false;
bool isLEDOn = false;
void setup() {
  pinMode(LED, OUTPUT);
  Serial.begin(115200);
  NimBLEDevice::init("");

  if(!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)){
        Serial.println("LittleFS Mount Failed");
        return;
    }

  File configFile = LittleFS.open("/config.json", "r");

  if (!LittleFS.exists("/config.json")) {
    configFile.close();
    Serial.println("Failed to open config file. Creating a new one...");

    File file = LittleFS.open("/config.json", "w+");
    file.print("{}");
    Serial.println("File created");

    file.close();
  } else {
    JsonDocument config;
    deserializeJson(config, configFile);
    Serial.println(configFile.readString());
    if (config.containsKey("txpower")) {
      txPower = config["txpower"].as<int>();
      Serial.print("Loaded txpower from memory: ");
      Serial.println(txPower);
    }
    configFile.close();
  }

  pBLEScan = NimBLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks(), true);
  pBLEScan->setMaxResults(0);  // do not store the scan results, use callback only.


  WiFi.begin(ssid, password);
  WiFi.setAutoConnect(true);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }

  Serial.println("WiFi connected");
  server.on("/scan", HTTP_GET, [&distance, &avgRSSI, &isScanning, &txPower]() {
    //return defualt values when calibration is in progress (LED is blinking)
    if (isLEDOn) {
      JsonDocument jsonDoc;
      // Add data to the JSON object
      jsonDoc["distance"] = -1;
      jsonDoc["avgrssi"] = -1;
      jsonDoc["id"] = DeviceId;
      jsonDoc["rssi1m"] = txPower;
      // Serialize JSON to a string
      char jsonString[200];
      serializeJson(jsonDoc, jsonString);

      // Send JSON response
      server.send(200, "application/json", jsonString);
      return;
    }

    isScanning = true;
    //Serial.println();
    //Serial.println("Proccesing new GET /scan");

    pBLEScan->start(0, nullptr, false);

    int64_t startTime = esp_timer_get_time();
    while (pBLEScan->isScanning() == true) {
      if (esp_timer_get_time() - startTime >= 5000000) {
        Serial.println("Failed to get RSSI from beacon. Out of range!");
        roundedDistance = -1;
        avgRSSI = -1;
        break;
      }
    }

    // Create a JSON object
    JsonDocument jsonDoc;
    // Add data to the JSON object
    jsonDoc["distance"] = roundedDistance;
    jsonDoc["avgrssi"] = avgRSSI;
    jsonDoc["id"] = DeviceId;
    jsonDoc["rssi1m"] = txPower;
    // Serialize JSON to a string
    char jsonString[600];
    serializeJson(jsonDoc, jsonString);

    Serial.print(".");
    server.send(200, "application/json", jsonString);

    isScanning = false;
  });

  server.on("/beginCalibration", HTTP_GET, [&avgRSSI, &txPower]() {
    if (!isLEDOn) {
      Serial.println("Cannot begin calibration without first getting response 200 from main server /calibrationStatus");
      server.send(405);
      return;
    }
    isLEDOn = false;
    digitalWrite(LED, HIGH);

    isScanning = true;
    Serial.println();
    Serial.println("Proccesing new GET /beginCalibration");

    pBLEScan->start(0, nullptr, false);

    int64_t startTime = esp_timer_get_time();
    Serial.println("Collecting data from ble beacon...");
    while (pBLEScan->isScanning() == true) {
      if (esp_timer_get_time() - startTime >= 5000000) {
        Serial.println("Failed to get RSSI from beacon. Out of range!");
        server.send(404);
        digitalWrite(LED, LOW);
        return;
      }
    }

    txPower = avgRSSI;

    JsonDocument newtxPower;
    newtxPower["txpower"] = txPower;
    File file = LittleFS.open("/config.json", "r+");
    serializeJson(newtxPower, file);
    file.close();

    // Create a JSON object
    JsonDocument jsonDoc;
    // Add data to the JSON object
    jsonDoc["rssi1m"] = avgRSSI;
    // Serialize JSON to a string
    char jsonString[100];
    serializeJson(jsonDoc, jsonString);

    // Send JSON response
    server.send(200, "application/json", jsonString);

    isScanning = false;

    server.send(200);
    digitalWrite(LED, LOW);
  });

  server.on("/blinkOn", HTTP_GET, [&isLEDOn]() {
    isLEDOn = true;
    server.send(200);
  });
  server.on("/blinkOff", HTTP_GET, [&isLEDOn]() {
    isLEDOn = false;
    digitalWrite(LED, LOW);
    server.send(200);
  });



  server.begin();
  Serial.print("Server started - ");
  Serial.println(WiFi.localIP());

  Serial.println("Handeling /scan requests (.):");
}



int64_t previuosTime = 0;
int64_t blinkerTimer = 0;
void loop() {
  server.handleClient();
  if (!isConnected) {
    isLEDOn = false;
    digitalWrite(LED, LOW);

    ConnectToMainServer();
    return;
  }

  //if a minute has passed check if the main server is still active
  if (!isScanning && esp_timer_get_time() - previuosTime >= 60000000) {
    previuosTime = esp_timer_get_time();
    CheckServerStatus();
  }


  //start the onboard LED
  if (isLEDOn) {
    if (esp_timer_get_time() - blinkerTimer >= 1000000) {
      blinkerTimer = esp_timer_get_time();
      digitalWrite(LED, HIGH);
      delay(10);
    } else
      digitalWrite(LED, LOW);
  }
}
