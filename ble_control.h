#ifndef BLE_CONTROL_H
#define BLE_CONTROL_H

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "Arduino.h"
#include <string>

// BLE UUIDs - make sure to match these with your app
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// BLE server and characteristic pointers
extern BLEServer *pServer;
extern BLECharacteristic *pCharacteristic;
extern bool deviceConnected;

// External reference to command handler function
extern void handleBLECommand(const std::string &command);

// Function declarations
bool initialiseBLE(const char *deviceName = "ESP32Camera");
void notifyBLEClients(const char *message);
void setupBLECallbacks();
void checkBLEStatus();

#endif // BLE_CONTROL_H