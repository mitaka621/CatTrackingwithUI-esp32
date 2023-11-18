#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

//0000feaa-0000-1000-8000-00805f9b34fb - Holy-IOT
//0000ffe0-0000-1000-8000-00805f9b34fb - iTAG
const char *deviceNameToScan = "Holy-IOT"; //Your beacon name
//static BLEUUID serviceUUID("0000feaa-0000-1000-8000-00805f9b34fb"); //Your beacon UUID

const int txPower = -65;  // Reference TxPower (RSSI) in dBm at 1 meter distance
const int N = 3; //const from 2 to 4 for accuracy

BLEScan *pBLEScan;
const int RSSIsampleSize = 20;
int RSSIArr[RSSIsampleSize];
int arrCount = 0;
double distance;


int compareAscending(const void *a, const void *b) {
  return (*(int *)b - *(int *)a);
}

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    //Locate Device by UUID
    // if (advertisedDevice.haveServiceUUID() && advertisedDevice.getServiceUUID().equals(serviceUUID)) {
    //   int rssi = advertisedDevice.getRSSI();
    //   Serial.print("RSSI: ");
    //   Serial.println(rssi);
    //   RSSIArr[arrCount] = rssi;
    //   arrCount++;
    //   // Calculate distance estimation based on RSSI (this is just an approximation)
    // }

    //Locate Device by Name
    if (advertisedDevice.haveName() && strcmp(advertisedDevice.getName().c_str(), deviceNameToScan) == 0) {
      int rssi = advertisedDevice.getRSSI();
      // Serial.print("RSSI of device '");
      // Serial.print(deviceNameToScan);
      // Serial.print("': ");
      // Serial.println(rssi);
      if (rssi != 0) {
        RSSIArr[arrCount] = rssi;
        arrCount++;
      }

      if (arrCount == RSSIsampleSize) {  //takes 'RSSIsampleSize' samples of the RSSI
        arrCount = 0;
        int SumRSSI = 0;

        qsort(RSSIArr, RSSIsampleSize, sizeof(int), compareAscending);
        for (int i = 0; i < RSSIsampleSize; i++) {
          SumRSSI += RSSIArr[i];
        }
        double avgRSSI = SumRSSI / RSSIsampleSize;
        Serial.println("----------");
        Serial.print("Average RSSI: ");
        Serial.println(avgRSSI);
        Serial.print("Min RSSI: ");
        Serial.println(RSSIArr[0]);
        Serial.print("Max RSSI: ");
        Serial.println(RSSIArr[RSSIsampleSize - 1]);

        
        distance = pow(10, (txPower - avgRSSI) / (10.0 * N));
        Serial.print("Distance (estimated): ");
        Serial.println(distance);

        Serial.print("Rounded distance (Real): ");
        Serial.println(round(distance * 2.0) / 2.0);

        
      }
      //Serial.println(arrCount);
    }
  }
};

void setup() {
  Serial.begin(115200);
  Serial.println("Starting BLE scan...");

  BLEDevice::init("");

  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks(), true);

  // Set scan parameters
  pBLEScan->setInterval(100);     // Scan interval (in 0.625 ms units)
  pBLEScan->setWindow(99);        // Scan window (in 0.625 ms units)
  pBLEScan->setActiveScan(true);  // Active scan (required for low latency mode)
                                  // Start the scan
  pBLEScan->start(0);             // Scan for 5 seconds, adjust as needed
}

void loop() {
  // Other tasks or code to execute within the loop
}
