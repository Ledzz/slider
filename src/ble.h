#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <map>

// BLE settings
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914c"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a9"

class Ble {
private:
    bool deviceConnected = false;
    bool oldDeviceConnected = false;
    BLEServer *pServer = nullptr;
    BLECharacteristic *pCharacteristic = nullptr;

    // Command registry: name -> handler(args)
    std::map<String, std::function<void(JsonDocument&)> > commandHandlers;

    // ---- Callbacks that can talk to this Ble instance ----
    class ServerCallbacks : public BLEServerCallbacks {
    public:
        explicit ServerCallbacks(Ble *self) : self(self) {
        }

        void onConnect(BLEServer *) override { self->deviceConnected = true; }
        void onDisconnect(BLEServer *) override { self->deviceConnected = false; }

    private:
        Ble *self;
    };

    class CharacteristicCallbacks : public BLECharacteristicCallbacks {
    public:
        explicit CharacteristicCallbacks(Ble *self) : self(self) {
        }

        void onWrite(BLECharacteristic *characteristic) override {
            std::string value = characteristic->getValue();
            if (!value.empty()) {
                String command = String(value.c_str());
                Serial.print("[BLE RX] ");
                Serial.println(command);
                self->handleCommand(command);
            }
        }

    private:
        Ble *self;
    };

    // ---- Command dispatcher ----
    void handleCommand(const String &jsonText) {
        Serial.println("handleCommand");
        Serial.println(jsonText);

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, jsonText);
        if (error) {
            Serial.print("deserializeJson() failed: ");
            Serial.println(error.c_str());
            return;
        }

        const char *cmd = doc["command"] | "";
        if (cmd[0] == '\0') {
            Serial.println("No 'command' field provided.");
            return;
        }


        // User-registered commands
        auto it = commandHandlers.find(String(cmd));
        if (it != commandHandlers.end()) {
            it->second(doc);
        } else {
            Serial.print("Unknown command: ");
            Serial.println(cmd);
        }
    }

public:
    void begin() {
        Serial.println("Starting BLE initialization...");

        BLEDevice::init("Ledzz slider");
        Serial.println("BLE device initialized with name: Ledzz slider");

        pServer = BLEDevice::createServer();
        Serial.println("BLE server created");

        pServer->setCallbacks(new ServerCallbacks(this));
        Serial.println("Server callbacks set");

        BLEService *pService = pServer->createService(SERVICE_UUID);
        Serial.print("BLE service created with UUID: ");
        Serial.println(SERVICE_UUID);

        pCharacteristic = pService->createCharacteristic(
            CHARACTERISTIC_UUID,
            BLECharacteristic::PROPERTY_READ |
            BLECharacteristic::PROPERTY_WRITE |
            BLECharacteristic::PROPERTY_NOTIFY
        );
        Serial.print("Characteristic created with UUID: ");
        Serial.println(CHARACTERISTIC_UUID);
        Serial.println("Characteristic properties: READ | WRITE | NOTIFY");

        pCharacteristic->addDescriptor(new BLE2902()); // Needed for NOTIFY on most clients
        Serial.println("BLE2902 descriptor added to characteristic");

        pCharacteristic->setCallbacks(new CharacteristicCallbacks(this));
        Serial.println("Characteristic callbacks set");

        pService->start();
        Serial.println("BLE service started");

        // Start advertising
        BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
        Serial.println("BLE advertising object obtained");

        pAdvertising->addServiceUUID(SERVICE_UUID);
        Serial.println("Service UUID added to advertising");

        pAdvertising->setScanResponse(true);
        Serial.println("Scan response enabled");

        pAdvertising->setMinPreferred(0x06);
        pAdvertising->setMinPreferred(0x12);
        Serial.println("Advertising intervals set (min preferred: 0x06, 0x12)");

        BLEDevice::startAdvertising();
        Serial.println("BLE advertising started");

        Serial.println("BLE ready. Send {\"command\":\"ping\"} or a custom command.");
        Serial.println("=== BLE initialization complete ===");
    }

    // Call this regularly from loop()
    void poll() {
        if (!deviceConnected && oldDeviceConnected) {
            vTaskDelay(500); // give the BT stack time
            if (pServer) pServer->startAdvertising();
            oldDeviceConnected = deviceConnected;
        }
        if (deviceConnected && !oldDeviceConnected) {
            oldDeviceConnected = deviceConnected;
        }
    }

    // Register a command: the callback gets the "args" object (or empty object)
    void addCommand(const String &name, const std::function<void(JsonDocument&)>& callback) {
        commandHandlers[name] = callback;
    }

    // Convenience: send a notification back to the client
    void notify(const String &s) {
        if (pCharacteristic && deviceConnected) {
            pCharacteristic->setValue(s.c_str());
            pCharacteristic->notify();
        }
    }
};
