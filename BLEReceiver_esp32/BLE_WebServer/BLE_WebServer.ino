#include <NimBLEDevice.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include "FS.h"
#include <LittleFS.h>
#include <esp_now.h>

#define FORMAT_LITTLEFS_IF_FAILED true

#define RSSIsampleSize 10  // Adjust as needed
#define CalibratingRssiSampleSize 30  // Adjust as needed
#define SCAN_INTERVAL 10 //10 milisecs
#define LED 2

const char *DeviceId = "psu";
const char *DeviceType = "ESP32";


const char *ssid = "ditoge03";
const char *password = "mitko111";

uint8_t broadcastAddress[] = {0xA0, 0xA3, 0xB3, 0x2F, 0x91, 0x24};

//mac address for the BLE beacon
//const  char* beaconMAC = "e9:65:3c:b5:99:c1"; //big boy beacon
const char* beaconMAC = "d0:3e:ed:1e:9a:9b";  //small beacon 5.0
int RSSIArr[RSSIsampleSize];
int arrCount = -1;
int avgRSSI = -1;
double roundedDistance = -1;
double distance = 0;
int txPower = -59;  // Example Tx Power value, adjust as needed
const double N = 3.0;  // Example environment factor, adjust as needed

//Kalman filtering
float x = 0.0;       // Initial state estimate
float P = 1.0;       // Initial estimate error covariance
float Q = 1e-2;      // Initial process noise covariance
float R = 1e-2;       // Initial measurement noise covariance
float K;             // Kalman gain
float prevRSSI = 0;   // Previous RSSI measurement
float variance = 0;   // Variance estimate
float alpha = 0.001;  // Smoothing factor for variance
float varianceThreshold = 1.0;  // Threshold to trigger parameter adjustment
int windowSize= 3;

NimBLEScan* pBLEScan;

QueueHandle_t scanResultQueue;

struct ScanResult {
  int rssi;
  NimBLEAddress address;
};

int64_t lastScanResultTime=0;

//runs on core 0
void scanTask(void *pvParameters) {
  for(;;) {
    Serial.println("Starting BLE scan...");
    pBLEScan->start(0, nullptr, false);

    while (pBLEScan->isScanning()) {
      if (esp_timer_get_time() - lastScanResultTime >= 30000000) {
        Serial.println("Failed to get RSSI from beacon. Out of range!");
        roundedDistance = -1;
        avgRSSI = -1;
        pBLEScan->stop();
        break;
      }
      delay(10);  // Small delay to avoid watchdog trigger
    }

    vTaskDelay(SCAN_INTERVAL / portTICK_PERIOD_MS);  // Delay for SCAN_INTERVAL before next scan
  }
}

int compareAscending(const void *a, const void *b) {
  return (*(int *)b - *(int *)a);
}

//runs on core 0
void handleScanResult(void *pvParameters) {
  ScanResult result;
  for(;;) {
    if (xQueueReceive(scanResultQueue, &result, portMAX_DELAY) == pdTRUE) {     
      if (result.rssi != 0) {
        RSSIArr[arrCount] = result.rssi;
        arrCount++;
      }

      if (arrCount == RSSIsampleSize) {  // Takes 'RSSIsampleSize' samples of the RSSI value
        pBLEScan->stop();
        arrCount = 0;

        qsort(RSSIArr, RSSIsampleSize, sizeof(int), compareAscending);
        float movingAvgRSSI[RSSIsampleSize];
        float window[windowSize];
        int windowCount = 0;

        for (int i = 0; i < RSSIsampleSize; ++i) {
            window[windowCount % windowSize] = RSSIArr[i];
            windowCount++;

            int size = std::min(windowCount, windowSize);
            float sum = 0;
            for (int j = 0; j < size; ++j) {
                sum += window[j];
            }
            movingAvgRSSI[i] = sum / size;
        }

        // Apply Kalman Filter
        float kalmanFilteredRSSI[RSSIsampleSize];
        float x_est = movingAvgRSSI[0];
        float P = 1.0;

        for (int k = 0; k < RSSIsampleSize; ++k) {
            // Prediction
            float x_pred = x_est;
            float P_pred = P + Q;

            // Update
            float K = P_pred / (P_pred + R);
            x_est = x_pred + K * (movingAvgRSSI[k] - x_pred);
            P = (1 - K) * P_pred;

            kalmanFilteredRSSI[k] = x_est;
        }

        // Get the median of the filtered RSSI values
        qsort(kalmanFilteredRSSI, RSSIsampleSize, sizeof(float), compareAscending);
        float medianRSSI = kalmanFilteredRSSI[RSSIsampleSize / 2];
        avgRSSI=medianRSSI;
        Serial.println(avgRSSI);
        // Calculate the distance based on the median RSSI value
        distance = pow(10, (txPower - medianRSSI) / (10.0 * N));
        roundedDistance = round(distance * 2.0) / 2.0;

        Serial.println(roundedDistance);
      }
    }
  }
}

bool isCalibrating=false;
int calibrationRssiArr[CalibratingRssiSampleSize];
int calibrationCounter=0;
//runs on core 1
class MyAdvertisedDeviceCallbacks : public NimBLEAdvertisedDeviceCallbacks {
  void onResult(NimBLEAdvertisedDevice* advertisedDevice) override {
    if (strcmp(beaconMAC, advertisedDevice->getAddress().toString().c_str()) == 0) {
      int rssi = advertisedDevice->getRSSI();
      if (rssi != 0) {
        lastScanResultTime=esp_timer_get_time();
        if (isCalibrating) {
          calibrationRssiArr[calibrationCounter++]=rssi;
          Serial.printf("Calibration progress %d/%d\n", calibrationCounter,CalibratingRssiSampleSize);
          return;
        }
        ScanResult result;
        result.rssi = rssi;
        result.address = advertisedDevice->getAddress();
        xQueueSend(scanResultQueue, &result, portMAX_DELAY);
      }
    }
  }
};

WebServer server(80);

bool isScanning = false;
bool isLEDOn = false;
int64_t previuosScan = 0;
void setup() {
  pinMode(LED, OUTPUT);
  Serial.begin(115200);


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

  if (LittleFS.exists("/latestData.json")) {
    File savedData = LittleFS.open("/latestData.json", "r");
    JsonDocument data;
    deserializeJson(data, savedData);
    if (data.containsKey("distance")) {
      distance=data["distance"].as<int>();    
    }
    if (data.containsKey("avgrssi")) {
      avgRSSI=data["avgrssi"].as<int>();    
    }    
    savedData.close();
  } 

  NimBLEDevice::init("ESP32_BLE");
  pBLEScan = NimBLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks(), true);
  pBLEScan->setMaxResults(0);  // Do not store the scan results, use callback only

  scanResultQueue = xQueueCreate(10, sizeof(ScanResult));

  xTaskCreatePinnedToCore(
    scanTask,   /* Task function */
    "scanTask", /* Name of the task */
    10000,      /* Stack size of the task */
    NULL,       /* Parameter of the task */
    1,          /* Priority of the task */
    NULL,       /* Task handle to keep track of created task */
    0);         /* Core where the task should run */

  // Create the task to handle scan results on core 0
  xTaskCreatePinnedToCore(
    handleScanResult,   /* Task function */
    "handleScanResult", /* Name of the task */
    10000,              /* Stack size of the task */
    NULL,               /* Parameter of the task */
    1,                  /* Priority of the task */
    NULL,               /* Task handle to keep track of created task */
    0);                 /* Core where the task should run */

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int failCounter=0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
      Serial.println("Connecting to WiFi...");

    failCounter++;
    if (failCounter>=20) {
    ESP.restart();
    }
  }

  Serial.println("WiFi connected");

  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_peer_info_t peerInfo;
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  peerInfo.ifidx = WIFI_IF_STA;

  // Add peer
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }

  server.on("/", HTTP_GET, []() {
    server.send(200);
  });

  server.on("/beginCalibration", HTTP_GET, [&]() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    if (!isLEDOn) {
      Serial.println("Cannot begin calibration without first getting response 200 from main server /calibrationStatus");
      server.send(405);
      return;
    }
    isLEDOn = false;
    digitalWrite(LED, HIGH);

    isScanning = true;
    isCalibrating=true;
    Serial.println();
    Serial.println("Proccesing new GET /beginCalibration");
    
    
    
    pBLEScan->start(0, nullptr, false);

    int64_t startTime = esp_timer_get_time();
    Serial.println("Collecting data from ble beacon...");
    while (calibrationCounter<CalibratingRssiSampleSize) {
      if (esp_timer_get_time() - startTime >= 20000000&&calibrationCounter==0) {
        Serial.println("Failed to get RSSI from beacon. Out of range!");
        pBLEScan->stop();
        calibrationCounter=0;
        isCalibrating=false;
        isScanning = false;
        server.send(404);
        digitalWrite(LED, LOW);
        return;
      }
    }
    
    pBLEScan->stop();
    calibrationCounter=0;
    isCalibrating=false;
    isScanning = false;

    qsort(calibrationRssiArr, CalibratingRssiSampleSize, sizeof(int), compareAscending);

    Serial.print("Calibration result: ");
    Serial.println(calibrationRssiArr[CalibratingRssiSampleSize/2]);
    txPower=calibrationRssiArr[CalibratingRssiSampleSize/2];

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

  server.on("/blinkOn", HTTP_GET, [&]() {
    isLEDOn = true;
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200);
  });
  server.on("/blinkOff", HTTP_GET, [&]() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    isLEDOn = false;
    digitalWrite(LED, LOW);
    server.send(200);
  });

  server.begin();
  Serial.print("Server started - ");
  Serial.println(WiFi.localIP());

  Serial.println("Sent data packet successfully (.):");
}

void SendDataESPNow() {
  JsonDocument jsonDoc;

  jsonDoc["distance"] = roundedDistance;
  jsonDoc["avgrssi"] = avgRSSI;
  jsonDoc["id"] = DeviceId;
  jsonDoc["rssi1m"] = txPower;
  jsonDoc["isConnected"]=true;
  jsonDoc["ip"]=WiFi.localIP().toString();
  jsonDoc["type"]="ESP32";

  size_t jsonSize = measureJson(jsonDoc) + 1;

  char* jsonData = new char[jsonSize];

  serializeJson(jsonDoc, jsonData, jsonSize);

  // Send JSON data via ESP-NOW
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *)jsonData, jsonSize);
  if (result == ESP_OK) {
    Serial.print(".");
  } else {
    Serial.println("Error sending the data. Saving data and restarting.....");
    Serial.println(esp_err_to_name(result));

    if(!isLEDOn){
    File file = LittleFS.open("/latestData.json", "w");
    serializeJson(jsonDoc, file);
    file.close();
    ESP.restart();
    } 
  }

  delete[] jsonData;
}

int64_t blinkerTimer = 0;
int64_t previousSend = 0;
void loop() {
  server.handleClient();

  //send new package every 1 sec
  if (!isLEDOn&& esp_timer_get_time() - previousSend >= 1000000) {
    if(WiFi.status() != WL_CONNECTED){
      Serial.println("Wifi lost connection");
      WiFi.disconnect();
      WiFi.begin(ssid, password);
    }
    previousSend = esp_timer_get_time();
    SendDataESPNow();
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
