#include <NimBLEDevice.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <ArduinoJson.h>

const char *DeviceId = "2";
const char *DeviceType = "ESP32";


const char *ssid = "ditoge03";
const char *password = "mitko111";

const char *ServerAddress = "http://192.168.0.9/";

//mac address for the BLE beacon
const char beaconMAC[] = "e9:65:3c:b5:99:c1";

const int txPower = -44;  // Reference TxPower (RSSI) in dBm at 1 meter distance
const int N = 2;          //const from 2 to 4 for accuracy

NimBLEScan *pBLEScan;
const int RSSIsampleSize = 20;
int RSSIArr[RSSIsampleSize];
int arrCount = 0;
double distance;


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
      Serial.print("Distance (estimated): ");
      Serial.println(distance);

      Serial.print("Rounded distance (Real): ");
      roundedDistance = round(distance * 2.0) / 2.0;
      Serial.println(roundedDistance);

      pBLEScan->stop();
    }
  }
};



bool isConnected = false;
void ConnectToMainServer() {
  if (WiFi.status() == WL_CONNECTED) {

    HTTPClient http;

    // Create a JSON object
    StaticJsonDocument<100> jsonDoc;  // Adjust the size according to your JSON structure

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


    // Create a JSON object
    StaticJsonDocument<100> jsonDoc;  // Adjust the size according to your JSON structure

    // Populate the JSON object with key-value pairs
    jsonDoc["Id"] = DeviceId;


    // Serialize the JSON object to a string
    String jsonString;
    serializeJson(jsonDoc, jsonString);

    char fullURL[50];
    strcpy(fullURL, ServerAddress);
    strcat(fullURL, "status");
    strcat(fullURL, "?data=");
    strcat(fullURL, jsonString.c_str());


    Serial.printf("Sending a GET request to %s\n", fullURL);
    http.begin(fullURL);
    int httpResponseCode = http.GET();

     
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
bool isScanning=false;
void setup() {
  Serial.begin(115200);
  NimBLEDevice::init("");

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

  server.on("/scan", HTTP_GET, [&distance, &avgRSSI,&isScanning]() {
    isScanning=true;
    Serial.println("Proccesing new GET /scan");
    
    pBLEScan->start(0, nullptr, false);

    int64_t startTime = esp_timer_get_time();
    while (pBLEScan->isScanning() == true) {
      Serial.println("Collecting data from ble beacon...");
      if (esp_timer_get_time() - startTime >= 5000000) {
        Serial.println("Failed to get RSSI from beacon. Out of range!");
        roundedDistance = -1;
        avgRSSI = -1;
        break;
      }
      delay(200);
    }

    // Create a JSON object
    JsonDocument jsonDoc;
    // Add data to the JSON object
    jsonDoc["distance"] = roundedDistance;
    jsonDoc["avgrssi"] = avgRSSI;
    jsonDoc["id"] = DeviceId;
    // Serialize JSON to a string
    String jsonString;
    serializeJson(jsonDoc, jsonString);

    // Send JSON response
    server.send(200, "application/json", jsonString);
     
    isScanning=false;
  });

  server.begin();
}



int64_t previuosTime = 0;

void loop() {
  server.handleClient();
  if (!isConnected) {
    ConnectToMainServer();
    return;
  }

  //if a minute has passed check if the main server is still active
  if (isScanning==false&& esp_timer_get_time() - previuosTime >= 60000000) {
    previuosTime = esp_timer_get_time();
    CheckServerStatus();
  }
}
