#include <Arduino.h>
#include <SPI.h>
#include <vector>

#include <GxEPD2_3C.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold18pt7b.h>

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

// ---------- Presets ----------
String presets[] = {
  "[big]Dinner[/big]\nis {ready}",
  "Please come {downstairs}",
  "[big]{Do not disturb}[/big]"
};

String pendingMessage = "";
bool hasPendingMessage = false;

// ---------- Text structs ----------
struct Segment {
  String text;
  String size;
  bool red;
};

struct TextLine {
  std::vector<Segment> segments;
  int width;
  int height;
};

// ---------- Function prototypes ----------
void setFontBySize(String size);
int lineHeightForSize(String size);
int textWidth(String text, String size);
void addSegmentToLine(TextLine &line, String text, String size, bool red);
std::vector<TextLine> layoutText(String msg, int maxWidth);
void drawMessage(String msg);

// ---------- Text sizing ----------
void setFontBySize(String size) {
  if (size == "big") {
    display.setFont(&FreeMonoBold18pt7b);
  } else if (size == "med") {
    display.setFont(&FreeMonoBold12pt7b);
  } else {
    display.setFont(&FreeMonoBold9pt7b);
  }
}

int lineHeightForSize(String size) {
  if (size == "big") return 34;
  if (size == "med") return 24;
  return 18;
}

// ---------- Measure text ----------
int textWidth(String text, String size) {
  setFontBySize(size);

  int16_t tbx, tby;
  uint16_t tbw, tbh;
  display.getTextBounds(text, 0, 0, &tbx, &tby, &tbw, &tbh);

  return tbw;
}

// ---------- Add segment to line ----------
void addSegmentToLine(TextLine &line, String text, String size, bool red) {
  if (text.length() == 0) return;

  int w = textWidth(text, size);

  Segment s;
  s.text = text;
  s.size = size;
  s.red = red;

  line.segments.push_back(s);
  line.width += w;
  line.height = max(line.height, lineHeightForSize(size));
}

// ---------- Layout rich wrapped text ----------
std::vector<TextLine> layoutText(String msg, int maxWidth) {
  std::vector<TextLine> lines;

  String size = "med";   // default text size
  bool red = false;

  TextLine currentLine;
  currentLine.width = 0;
  currentLine.height = lineHeightForSize(size);

  String token = "";

  auto flushToken = [&]() {
    if (token.length() == 0) return;

    int tokenWidth = textWidth(token, size);
    int spaceWidth = textWidth(" ", size);

    bool needsSpace = currentLine.segments.size() > 0;
    int addedWidth = tokenWidth + (needsSpace ? spaceWidth : 0);

    if (needsSpace && currentLine.width + addedWidth > maxWidth) {
      lines.push_back(currentLine);

      currentLine.segments.clear();
      currentLine.width = 0;
      currentLine.height = lineHeightForSize(size);
      needsSpace = false;
    }

    if (needsSpace) {
      addSegmentToLine(currentLine, " ", size, red);
    }

    addSegmentToLine(currentLine, token, size, red);
    token = "";
  };

  auto newLine = [&]() {
    flushToken();

    if (currentLine.segments.size() > 0) {
      lines.push_back(currentLine);
    }

    currentLine.segments.clear();
    currentLine.width = 0;
    currentLine.height = lineHeightForSize(size);
  };

  for (int i = 0; i < msg.length(); i++) {
    if (msg.substring(i).startsWith("[big]")) {
      flushToken();
      size = "big";
      i += 4;
      continue;
    }

    if (msg.substring(i).startsWith("[/big]")) {
      flushToken();
      size = "med";
      i += 5;
      continue;
    }

    if (msg.substring(i).startsWith("[med]")) {
      flushToken();
      size = "med";
      i += 4;
      continue;
    }

    if (msg.substring(i).startsWith("[/med]")) {
      flushToken();
      size = "med";
      i += 5;
      continue;
    }

    if (msg.substring(i).startsWith("[small]")) {
      flushToken();
      size = "small";
      i += 6;
      continue;
    }

    if (msg.substring(i).startsWith("[/small]")) {
      flushToken();
      size = "med";
      i += 7;
      continue;
    }

    char c = msg[i];

    if (c == '{') {
      flushToken();
      red = true;
    } else if (c == '}') {
      flushToken();
      red = false;
    } else if (c == '\n') {
      newLine();
    } else if (c == ' ') {
      flushToken();
    } else {
      token += c;
    }
  }

  flushToken();

  if (currentLine.segments.size() > 0) {
    lines.push_back(currentLine);
  }

  return lines;
}

// ---------- Draw message ----------
void drawMessage(String msg) {
  Serial.println("Drawing: " + msg);

  display.setRotation(1);

  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);

    int margin = 10;
    int maxWidth = display.width() - 2 * margin;

    std::vector<TextLine> lines = layoutText(msg, maxWidth);

    int totalHeight = 0;
    for (auto &line : lines) {
      totalHeight += line.height;
    }

    int y = (display.height() - totalHeight) / 2;

    for (auto &line : lines) {
      int x = (display.width() - line.width) / 2;

      for (auto &seg : line.segments) {
        setFontBySize(seg.size);

        display.setTextColor(seg.red ? GxEPD_RED : GxEPD_BLACK);
        display.setCursor(x, y);
        display.print(seg.text);

        x += textWidth(seg.text, seg.size);
      }

      y += line.height;
    }

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

// ---------- BLE reconnect ----------
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