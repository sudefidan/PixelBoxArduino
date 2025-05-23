#include "ble_control.h"

// Global variables
BLEServer *pServer = NULL;
BLECharacteristic *pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;
unsigned long lastNotificationTime = 0;
String lastNotificationMessage = "";
const unsigned long NOTIFICATION_DEBOUNCE_TIME = 300; // milliseconds


// Server callbacks
class MyServerCallbacks : public BLEServerCallbacks
{
    void onConnect(BLEServer *pServer)
    {
        deviceConnected = true;
        Serial.println("BLE Client connected");
    }

    void onDisconnect(BLEServer *pServer)
    {
        deviceConnected = false;
        Serial.println("BLE Client disconnected");
    }
};

// Characteristic callbacks
class MyCallbacks : public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic *pCharacteristic)
    {
        // Get the value as a std::string
        String rawValue = pCharacteristic->getValue().c_str();
        std::string value = std::string(rawValue.c_str());

        if (value.length() > 0)
        {
            Serial.print("Received command: ");
            for (int i = 0; i < value.length(); i++)
            {
                Serial.print(value[i]);
            }
            Serial.println();

            // Call the external command handler function
            handleBLECommand(value);
        }
    }
};

// Initialise BLE
bool initialiseBLE(const char *deviceName)
{
    Serial.println("Starting BLE initialisation!");

    // Create the BLE Device
    BLEDevice::init(deviceName);

    // Create the BLE Server
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks()); // Set callback for incoming data

    // Create the BLE Service
    BLEService *pService = pServer->createService(SERVICE_UUID);

    // Create a BLE Characteristic
    pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ |
            BLECharacteristic::PROPERTY_WRITE |
            BLECharacteristic::PROPERTY_NOTIFY);

    // Create a BLE Descriptor
    pCharacteristic->addDescriptor(new BLE2902());
    pCharacteristic->setCallbacks(new MyCallbacks());

    // Set initial value
    pCharacteristic->setValue("PixelBox Ready!");

    // Start the service
    pService->start();

    // Start advertising
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06); // helps with iPhone connections issue
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();

    Serial.println("BLE advertising started.");
    return true;
}

// Send notification to connected clients
void notifyBLEClients(const char *message)
{
    if (!pServer || !pCharacteristic)
    {
        Serial.println("BLE server or characteristic not initialised");
        return;
    }

    // Check if this is the same message we just sent
    if (lastNotificationMessage == message) {
        unsigned long currentTime = millis();
        // If the same message was sent recently, skip it
        if (currentTime - lastNotificationTime < NOTIFICATION_DEBOUNCE_TIME) {
            Serial.printf("Skipping duplicate notification: %s\n", message);
            return;
        }
    }

    if (deviceConnected)
    {
        pCharacteristic->setValue(message);
        pCharacteristic->notify();
        Serial.printf("Notification sent: %s\n", message);


        // Track this notification
        lastNotificationTime = millis();
        lastNotificationMessage = message;
    }
    else
    {
        Serial.println("No BLE clients connected. Skipping notification.");
    }
}

// Check BLE connection status and handle reconnections
void checkBLEStatus()
{
    // Disconnection handling
    if (!deviceConnected && oldDeviceConnected)
    {
        delay(500);                  // Give the Bluetooth stack time to get ready
        pServer->startAdvertising(); // Restart advertising
        Serial.println("Started advertising again");
        oldDeviceConnected = deviceConnected;
    }

    // Connection handling
    if (deviceConnected && !oldDeviceConnected)
    {
        oldDeviceConnected = deviceConnected;
    }
}