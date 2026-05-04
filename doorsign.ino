#include <Arduino.h>
#include <SPI.h>
#include <GxEPD2_3C.h>
#include <Fonts/FreeMonoBold9pt7b.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>

// ---------- E-paper pins ----------
#define PIN_EPD_CS   7
#define PIN_EPD_DC   2
#define PIN_EPD_RST  1
#define PIN_EPD_BUSY 10

#define PIN_SPI_SCK  4
#define PIN_SPI_MOSI 3

// ---------- Display ----------
GxEPD2_3C<GxEPD2_213_Z98c, GxEPD2_213_Z98c::HEIGHT> display(
  GxEPD2_213_Z98c(PIN_EPD_CS, PIN_EPD_DC, PIN_EPD_RST, PIN_EPD_BUSY)
);

// ---------- BLE UUIDs ----------
#define SERVICE_UUID        "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"

// ---------- Preset messages ----------
String presets[] = {
  "Dinner is ready",
  "Please come downstairs",
  "Do not disturb"
};

// ---------- Pending display update ----------
String pendingMessage = "";
bool hasPendingMessage = false;

// ---------- Draw message on e-paper ----------
void drawMessage(String msg) {
  Serial.println("Drawing: " + msg);

  display.setRotation(1);

  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);

    display.setTextColor(GxEPD_BLACK);
    display.setFont(&FreeMonoBold9pt7b);

    display.setCursor(10, 30);
    display.print("Message:");

    display.setCursor(10, 65);
    display.print(msg);

    display.setTextColor(GxEPD_RED);
    display.setCursor(10, 105);
    display.print("BLE ready");

  } while (display.nextPage());

  display.hibernate();
}

// ---------- BLE write callback ----------
class MessageCallback : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *characteristic) {
    String value = characteristic->getValue().c_str();
    value.trim();

    if (value.length() == 0) return;

    Serial.println("Received: " + value);

    if (value == "1") {
      pendingMessage = presets[0];
    } else if (value == "2") {
      pendingMessage = presets[1];
    } else if (value == "3") {
      pendingMessage = presets[2];
    } else {
      pendingMessage = value;
    }

    hasPendingMessage = true;
  }
};

// ---------- Restart advertising after disconnect ----------
class ServerCallbacks : public BLEServerCallbacks {
  void onDisconnect(BLEServer *server) {
    Serial.println("Client disconnected; restarting advertising");
    BLEDevice::startAdvertising();
  }
};

void setup() {
  delay(3000);

  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("===== ESP32-C3 E-PAPER BLE MESSENGER BOOT =====");

  // SPI for your custom wiring
  SPI.begin(PIN_SPI_SCK, -1, PIN_SPI_MOSI, PIN_EPD_CS);

  // Initial display setup
  display.init(115200);
  drawMessage("Starting BLE...");

  // BLE setup
  BLEDevice::init("ESP32-EINK-MSG");

  BLEServer *server = BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  BLEService *service = server->createService(SERVICE_UUID);

  BLECharacteristic *messageCharacteristic = service->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_WRITE |
    BLECharacteristic::PROPERTY_WRITE_NR
  );

  messageCharacteristic->setCallbacks(new MessageCallback());
  messageCharacteristic->setValue("Send 1, 2, 3, or text");

  service->start();

  BLEAdvertising *advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(SERVICE_UUID);
  advertising->setScanResponse(true);
  advertising->start();

  Serial.println("BLE advertising as ESP32-EINK-MSG");

  pendingMessage = "BLE ready";
  hasPendingMessage = true;
}

void loop() {
  if (hasPendingMessage) {
    hasPendingMessage = false;

    Serial.println("Updating display from loop...");

    // Important after hibernate
    display.init(115200);

    drawMessage(pendingMessage);

    Serial.println("Display update finished.");
  }

  delay(100);
}