#include "bt_control.h"
#include <NimBLEDevice.h>

// A3B8C4F4-1298-D5A4-5191-0A0D7DEA7C0A: IF_B7
// <0100> 0203 112A C019 1124 4301 0253 01F4 A144 30


NimBLEAdvertising* pAdvertising;
static NimBLECharacteristic* notifyChar;
static NimBLECharacteristic* writeChar;

class CmdCallback : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* c) {
        std::string val = c->getValue();
        if (val.empty()) return;

        uint8_t cmd = val[0];
        Serial.printf("Received command: 0x%02X\n", cmd);

    //     switch (cmd) {
    //         case 0x02:
    //             Serial.println("TARE command received");
    //             // TODO: tare logic
    //             break;

    //         case 0x03:
    //             Serial.println("UNIT CHANGE command received");
    //             // TODO: unit change logic
    //             break;

    //         case 0x04:
    //             Serial.println("HOLD command received");
    //             // TODO: hold logic
    //             break;

    //         default:
    //             Serial.printf("Unknown command: 0x%02X\n", cmd);
    //     }
     }
};

void init_bluetooth(void){
    NimBLEDevice::init("IF_B7");

    // Server wird benötigt, auch wenn du keine Services nutzt
    NimBLEServer* server = NimBLEDevice::createServer();

    // Advertising-Objekt nur EINMAL holen
    pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->setMinInterval(50);
    pAdvertising->setMaxInterval(100);

    NimBLEAdvertisementData advData;
    advData.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
    advData.setName("IF_B7");
    advData.addServiceUUID("A3B8C4F4-1298-D5A4-5191-0A0D7DEA7C0A");

    // Start-Payload
    std::string payload;
    payload.push_back(0x01);
    payload.push_back(0x00);
    payload.push_back(0x02);
    payload.push_back(0x03);
    payload.push_back(0x11);
    payload.push_back(0x2A);
    payload.push_back(0xC0);
    payload.push_back(0x19);
    payload.push_back(0x11);
    payload.push_back(0x24);
    payload.push_back(0x43);
    payload.push_back(0x01);
    payload.push_back(0x00);
    payload.push_back(0x00);
    payload.push_back(0x01);
    payload.push_back(0xF4);
    payload.push_back(0xA1);
    payload.push_back(0x44);
    payload.push_back(0x30);

    advData.setManufacturerData(payload);

    pAdvertising->setAdvertisementData(advData);
    pAdvertising->start();

    Serial.println("BLE Advertising started");
}


uint32_t nextBtAdd = 0;
void bluetooth_update_scale_value(float scValue) {
    uint32_t now = millis();
    //if ((now < nextBtAdd)|| WifiClientConnected) return;
    if (now < nextBtAdd) return;

    nextBtAdd = now + 100;
    //if(DEBUG >= 1){
        Serial.printf("BT send %.2f\r\n", scValue);
   // }    
    if(scValue < 0) scValue = 0;
    uint16_t raw = scValue * 100;

    NimBLEAdvertisementData advData;

    // Nur Manufacturer-Data aktualisieren!
    std::string payload;
    payload.push_back(0x01);
    payload.push_back(0x00);
    payload.push_back(0x02);
    payload.push_back(0x03);
    payload.push_back(0x11);
    payload.push_back(0x2A);
    payload.push_back(0xC0);
    payload.push_back(0x19);
    payload.push_back(0x11);
    payload.push_back(0x24);
    payload.push_back(0x43);
    payload.push_back(0x01);
    payload.push_back((raw >> 8) & 0xFF);
    payload.push_back(raw & 0xFF);
    payload.push_back(0x01);
    payload.push_back(0xF4);
    payload.push_back(0xA1);
    payload.push_back(0x44);
    payload.push_back(0x30);

    advData.setManufacturerData(payload);

    // Advertising läuft bereits → nur Daten ersetzen
    pAdvertising->setAdvertisementData(advData);
}