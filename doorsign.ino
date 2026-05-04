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

GxEPD2_3C<GxEPD2_213_Z98c, GxEPD2_213_Z98c::HEIGHT> display(
  GxEPD2_213_Z98c(PIN_EPD_CS, PIN_EPD_DC, PIN_EPD_RST, PIN_EPD_BUSY)
);

// ---------- BLE UUIDs ----------
#define SERVICE_UUID        "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"

// ---------- Presets ----------
String presets[] = {
  "Dinner is {ready}",
  "Please come {downstairs}",
  "{Do not disturb}"
};

String pendingMessage = "";
bool hasPendingMessage = false;

// ---------- Wrapped text with {red spans} ----------
void printWrappedRichText(String msg, int x, int y, int maxWidth, int lineHeight) {
  int cursorX = x;
  int cursorY = y;
  bool red = false;

  display.setFont(&FreeMonoBold9pt7b);
  display.setTextColor(GxEPD_BLACK);

  String token = "";

  for (int i = 0; i <= msg.length(); i++) {
    char c = (i < msg.length()) ? msg[i] : ' ';

    bool flush =
      c == ' ' || c == '\n' || c == '{' || c == '}' || i == msg.length();

    if (flush && token.length() > 0) {
      int16_t tbx, tby;
      uint16_t tbw, tbh;
      display.getTextBounds(token, cursorX, cursorY, &tbx, &tby, &tbw, &tbh);

      if (cursorX != x && cursorX + tbw > x + maxWidth) {
        cursorX = x;
        cursorY += lineHeight;
      }

      if (cursorY > display.height() - 8) return;

      display.setTextColor(red ? GxEPD_RED : GxEPD_BLACK);
      display.setCursor(cursorX, cursorY);
      display.print(token);

      cursorX += tbw + 8;
      token = "";
    }

    if (c == '{') {
      red = true;
    } else if (c == '}') {
      red = false;
    } else if (c == '\n') {
      cursorX = x;
      cursorY += lineHeight;
    } else if (c == ' ') {
      // word separator already handled by cursorX spacing
    } else if (i < msg.length()) {
      token += c;
    }
  }
}

// ---------- Draw message ----------
void drawMessage(String msg) {
  Serial.println("Drawing: " + msg);

  display.setRotation(1);

  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);

    display.setFont(&FreeMonoBold9pt7b);

    display.setTextColor(GxEPD_BLACK);
    display.setCursor(10, 22);
    display.print("Message:");

    printWrappedRichText(msg, 10, 52, display.width() - 20, 18);

  } while (display.nextPage());

  display.hibernate();
}

// ---------- BLE callback ----------
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
    } else if (value.startsWith("SET1:")) {
      presets[0] = value.substring(5);
      pendingMessage = presets[0];
    } else if (value.startsWith("SET2:")) {
      presets[1] = value.substring(5);
      pendingMessage = presets[1];
    } else if (value.startsWith("SET3:")) {
      presets[2] = value.substring(5);
      pendingMessage = presets[2];
    } else {
      pendingMessage = value;
    }

    hasPendingMessage = true;
  }
};

// ---------- Re-advertise after disconnect ----------
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

  SPI.begin(PIN_SPI_SCK, -1, PIN_SPI_MOSI, PIN_EPD_CS);

  display.init(115200);
  drawMessage("Starting {BLE}...");

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
  messageCharacteristic->setValue("Send 1, 2, 3, SET1:text, or text");

  service->start();

  BLEAdvertising *advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(SERVICE_UUID);
  advertising->setScanResponse(true);
  advertising->start();

  Serial.println("BLE advertising as ESP32-EINK-MSG");

  pendingMessage = "{BLE ready}";
  hasPendingMessage = true;
}

void loop() {
  if (hasPendingMessage) {
    hasPendingMessage = false;

    Serial.println("Updating display from loop...");
    display.init(115200);
    drawMessage(pendingMessage);
    Serial.println("Display update finished.");
  }

  delay(100);
}