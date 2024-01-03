#include <NimBLEDevice.h>

NimBLEScan *pBLEScan;

//mac address for the BLE beacon
const char beaconMAC[] = "e9:65:3c:b5:99:c1";

const int txPower = -54;  // Reference TxPower (RSSI) in dBm at 1 meter distance
const int N = 2;          //const from 2 to 4 for possible accuracy improvment (dependant on device)

const int RSSIsampleSize = 20;
int RSSIArr[RSSIsampleSize];
int arrCount = 0;
double distance;

int compareAscending(const void *a, const void *b) {
  return (*(int *)b - *(int *)a);
}


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
      double avgRSSI = SumRSSI / RSSIsampleSize;
      Serial.println("----------");
      Serial.print("Average RSSI: ");
      Serial.println(avgRSSI);
      Serial.print("Min RSSI: ");
      Serial.println(RSSIArr[0]);
      Serial.print("Max RSSI: ");
      Serial.println(RSSIArr[RSSIsampleSize - 1]);

      for (int a : RSSIArr) {
      Serial.println(a);
      }


      distance = pow(10, (txPower - avgRSSI) / (10.0 * N));
      Serial.print("Distance (estimated): ");
      Serial.println(distance);

      Serial.print("Rounded distance (Real): ");
      Serial.println(round(distance * 2.0) / 2.0);

      pBLEScan->stop();
    }
  }
};


void setup() {
  Serial.begin(115200);
  NimBLEDevice::init("");

  Serial.println("Scanning...");
  pBLEScan = NimBLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks(), true);
  pBLEScan->setMaxResults(0);  // do not store the scan results, use callback only.
}



void loop() {
  if (pBLEScan->isScanning() == false) {
    Serial.println("Scan finished wating 2 sec");
    delay(2000);
    pBLEScan->start(0, nullptr, false);
  }
}
