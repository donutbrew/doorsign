#include <Arduino.h>
#include <SPI.h>
#include <vector>
#include <Preferences.h>

#include <GxEPD2_3C.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>

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

// ---------- Button ----------
#define BUTTON_PIN 21

Preferences prefs;

GxEPD2_3C<GxEPD2_213_Z98c, GxEPD2_213_Z98c::HEIGHT> display(
  GxEPD2_213_Z98c(PIN_EPD_CS, PIN_EPD_DC, PIN_EPD_RST, PIN_EPD_BUSY)
);

#include "bitmap_icons.h"
#include "icons.h"
#include "presets.h"

// ---------- BLE UUIDs ----------
#define SERVICE_UUID        "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"

// ---------- State ----------
String pendingMessage = "";
bool hasPendingMessage = false;

int buttonSelectedPreset = 0;
unsigned long lastButtonPressTime = 0;
bool buttonSelectionPending = false;

// ---------- Structs ----------
struct Segment {
  String text;
  String size;
  bool red;
  bool redBox;
};

struct TextLine {
  std::vector<Segment> segments;
  int width;
  int height;
};

struct ParsedMessage {
  String icon;
  String text;
};

// ---------- Preset persistence ----------
void loadSavedPresets() {
  presets[0] = prefs.getString("p1", presets[0]);
  presets[1] = prefs.getString("p2", presets[1]);
  presets[2] = prefs.getString("p3", presets[2]);

  Serial.println("Loaded presets:");
  Serial.println("1: " + presets[0]);
  Serial.println("2: " + presets[1]);
  Serial.println("3: " + presets[2]);
}

void savePreset(int index) {
  if (index == 0) prefs.putString("p1", presets[0]);
  if (index == 1) prefs.putString("p2", presets[1]);
  if (index == 2) prefs.putString("p3", presets[2]);

  Serial.println("Saved preset " + String(index + 1));
}

// ---------- Text sizing ----------
void setFontBySize(String size) {
  if (size == "big") display.setFont(&FreeSansBold18pt7b);
  else if (size == "med") display.setFont(&FreeSansBold12pt7b);
  else display.setFont(&FreeSansBold9pt7b);
}

int lineHeightForSize(String size) {
  setFontBySize(size);

  int16_t tbx, tby;
  uint16_t tbw, tbh;
  display.getTextBounds("Ag", 0, 0, &tbx, &tby, &tbw, &tbh);

  return tbh + 1;
}

int baselineOffsetForSize(String size) {
  setFontBySize(size);

  int16_t tbx, tby;
  uint16_t tbw, tbh;
  display.getTextBounds("Ag", 0, 0, &tbx, &tby, &tbw, &tbh);

  return -tby;
}

int textWidth(String text, String size) {
  if (text == " ") {
    if (size == "big") return 10;
    if (size == "med") return 8;
    return 6;
  }

  setFontBySize(size);

  int16_t tbx, tby;
  uint16_t tbw, tbh;
  display.getTextBounds(text, 0, 0, &tbx, &tby, &tbw, &tbh);

  return tbw;
}

// ---------- Message parser ----------
ParsedMessage parseMessage(String msg) {
  ParsedMessage result;
  result.icon = "none";
  result.text = msg;

  if (msg.startsWith("ICON:")) {
    int pipe = msg.indexOf('|');
    if (pipe > 5) {
      result.icon = msg.substring(5, pipe);
      result.text = msg.substring(pipe + 1);
      result.icon.trim();
    }
  }

  return result;
}

// ---------- Layout ----------
void addSegmentToLine(TextLine &line, String text, String size, bool red, bool redBox) {
  if (text.length() == 0) return;

  int w = textWidth(text, size);

  Segment s;
  s.text = text;
  s.size = size;
  s.red = red;
  s.redBox = redBox;

  line.segments.push_back(s);

  if (redBox && text != " ") {
    line.width += w + 10;
    line.height = max(line.height, lineHeightForSize(size) + 6);
  } else {
    line.width += w;
    line.height = max(line.height, lineHeightForSize(size));
  }
}

std::vector<TextLine> layoutText(String msg, int maxWidth) {
  msg.replace("\\n", "\n");
  msg.replace("\\r", "\n");
  msg.replace("\r", "\n");

  msg.replace("’", "'");
  msg.replace("‘", "'");
  msg.replace("“", "\"");
  msg.replace("”", "\"");

  std::vector<TextLine> lines;

  String size = "med";
  bool red = false;
  bool redBox = false;

  TextLine currentLine;
  currentLine.width = 0;
  currentLine.height = lineHeightForSize(size);

  String token = "";

  auto flushToken = [&]() {
    if (token.length() == 0) return;

    int tokenWidth = textWidth(token, size);
    int spaceWidth = textWidth(" ", size);
    int boxExtra = redBox ? 10 : 0;

    bool needsSpace = currentLine.segments.size() > 0;
    int addedWidth = tokenWidth + boxExtra + (needsSpace ? spaceWidth : 0);

    if (needsSpace && currentLine.width + addedWidth > maxWidth) {
      lines.push_back(currentLine);
      currentLine.segments.clear();
      currentLine.width = 0;
      currentLine.height = lineHeightForSize(size);
      needsSpace = false;
    }

    if (needsSpace) addSegmentToLine(currentLine, " ", size, false, false);

    addSegmentToLine(currentLine, token, size, red, redBox);
    token = "";
  };

  auto newLine = [&]() {
    flushToken();

    if (currentLine.segments.size() > 0) {
      lines.push_back(currentLine);
    } else {
      TextLine blankLine;
      blankLine.width = 0;
      blankLine.height = lineHeightForSize(size);
      lines.push_back(blankLine);
    }

    currentLine.segments.clear();
    currentLine.width = 0;
    currentLine.height = lineHeightForSize(size);
  };

  for (int i = 0; i < msg.length(); i++) {
    if (msg.substring(i).startsWith("[big]")) {
      flushToken(); size = "big"; i += 4; continue;
    }
    if (msg.substring(i).startsWith("[/big]")) {
      flushToken(); size = "med"; i += 5; continue;
    }
    if (msg.substring(i).startsWith("[med]")) {
      flushToken(); size = "med"; i += 4; continue;
    }
    if (msg.substring(i).startsWith("[/med]")) {
      flushToken(); size = "med"; i += 5; continue;
    }
    if (msg.substring(i).startsWith("[small]")) {
      flushToken(); size = "small"; i += 6; continue;
    }
    if (msg.substring(i).startsWith("[/small]")) {
      flushToken(); size = "med"; i += 7; continue;
    }
    if (msg.substring(i).startsWith("{{")) {
      flushToken(); redBox = true; i += 1; continue;
    }
    if (msg.substring(i).startsWith("}}")) {
      flushToken(); redBox = false; i += 1; continue;
    }

    char c = msg[i];

    if (c == '{') {
      flushToken(); red = true;
    } else if (c == '}') {
      flushToken(); red = false;
    } else if (c == '\n') {
      newLine();
    } else if (c == ' ') {
      if (redBox) token += c;
      else flushToken();
    } else {
      token += c;
    }
  }

  flushToken();

  if (currentLine.segments.size() > 0) lines.push_back(currentLine);

  return lines;
}

String shrinkMarkupSizes(String msg) {
  msg.replace("[big]", "[med]");
  msg.replace("[/big]", "[/med]");
  msg.replace("[med]", "[small]");
  msg.replace("[/med]", "[/small]");
  return msg;
}

// ---------- Draw message ----------
void drawMessage(String msg) {
  Serial.println("Drawing: " + msg);

  ParsedMessage parsed = parseMessage(msg);
  bool hasIcon = parsed.icon != "none";

  display.setRotation(1);

  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);

    int margin = 8;
    int iconWidth = hasIcon ? display.width() / 4 : 0;
    int textX = hasIcon ? iconWidth + margin : margin;
    int textMaxWidth = display.width() - textX - margin;

    if (hasIcon) {
      drawIcon(parsed.icon, 0, 0, iconWidth, display.height());
    }

    std::vector<TextLine> lines = layoutText(parsed.text, textMaxWidth);

    int totalHeight = 0;
    for (auto &line : lines) totalHeight += line.height;

    if (totalHeight > display.height() - 4) {
      Serial.println("Message too tall; shrinking text.");
      parsed.text = shrinkMarkupSizes(parsed.text);
      lines = layoutText(parsed.text, textMaxWidth);

      totalHeight = 0;
      for (auto &line : lines) totalHeight += line.height;
    }

    String firstLineSize = "med";
    if (lines.size() > 0 && lines[0].segments.size() > 0) {
      firstLineSize = lines[0].segments[0].size;
    }

    int topY = (totalHeight > display.height() - 4)
      ? 2
      : (display.height() - totalHeight) / 2;

    int y = topY + baselineOffsetForSize(firstLineSize);

    for (auto &line : lines) {
      if (y > display.height() + 12) break;

      int x = textX + (textMaxWidth - line.width) / 2;

      for (auto &seg : line.segments) {
        setFontBySize(seg.size);

        int segWidth = textWidth(seg.text, seg.size);

        if (seg.text == " ") {
          x += segWidth;
          continue;
        }

        if (seg.redBox) {
          int padX = 5;
          int padY = 4;

          int16_t tbx, tby;
          uint16_t tbw, tbh;

          display.getTextBounds(seg.text, x, y, &tbx, &tby, &tbw, &tbh);

          display.fillRoundRect(
            tbx - padX,
            tby - padY,
            tbw + padX * 2,
            tbh + padY * 2,
            4,
            GxEPD_RED
          );

          display.setTextColor(GxEPD_WHITE);
        } else {
          display.setTextColor(seg.red ? GxEPD_RED : GxEPD_BLACK);
        }

        display.setCursor(x, y);
        display.print(seg.text);

        x += segWidth + (seg.redBox ? 10 : 0);
      }

      y += line.height;
    }

  } while (display.nextPage());

  display.hibernate();
}

// ---------- Button ----------
void handleButton() {
  static bool wasPressed = false;

  bool pressed = digitalRead(BUTTON_PIN) == LOW;

  if (pressed && !wasPressed) {
    unsigned long now = millis();

    if (now - lastButtonPressTime > 180) {
      buttonSelectedPreset = (buttonSelectedPreset + 1) % NUM_PRESETS;
      buttonSelectionPending = true;
      lastButtonPressTime = now;

      Serial.println("Button selected preset: " + String(buttonSelectedPreset + 1));
    }
  }

  wasPressed = pressed;

  if (buttonSelectionPending && millis() - lastButtonPressTime > 600) {
    buttonSelectionPending = false;

    pendingMessage = presets[buttonSelectedPreset];
    hasPendingMessage = true;

    Serial.println("Button committed preset: " + String(buttonSelectedPreset + 1));
  }
}

// ---------- BLE ----------
class MessageCallback : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *characteristic) {
    String value = characteristic->getValue().c_str();
    value.trim();

    if (value.length() == 0) return;

    Serial.println("Received: " + value);

    if (value == "1") {
      pendingMessage = presets[0];
      buttonSelectedPreset = 0;
    } else if (value == "2") {
      pendingMessage = presets[1];
      buttonSelectedPreset = 1;
    } else if (value == "3") {
      pendingMessage = presets[2];
      buttonSelectedPreset = 2;
    } else if (value.startsWith("SET1:")) {
      presets[0] = value.substring(5);
      savePreset(0);
      pendingMessage = presets[0];
      buttonSelectedPreset = 0;
    } else if (value.startsWith("SET2:")) {
      presets[1] = value.substring(5);
      savePreset(1);
      pendingMessage = presets[1];
      buttonSelectedPreset = 1;
    } else if (value.startsWith("SET3:")) {
      presets[2] = value.substring(5);
      savePreset(2);
      pendingMessage = presets[2];
      buttonSelectedPreset = 2;
    } else if (value == "RESETPRESETS") {
      prefs.clear();
      Serial.println("Cleared saved presets. Reboot to reload defaults.");
      pendingMessage = "{{Presets reset}}";
    } else {
      pendingMessage = value;
    }

    hasPendingMessage = true;
  }
};

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
  Serial.println("===== ESP32-C3 E-PAPER BLE MESSENGER V2 BOOT =====");

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  prefs.begin("doorsign", false);
  loadSavedPresets();

  SPI.begin(PIN_SPI_SCK, -1, PIN_SPI_MOSI, PIN_EPD_CS);

  display.init(115200);

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

  buttonSelectedPreset = 0;
  pendingMessage = presets[0];
  hasPendingMessage = true;
}

void loop() {
  handleButton();

  if (hasPendingMessage) {
    hasPendingMessage = false;

    Serial.println("Updating display from loop...");
    display.init(115200);
    drawMessage(pendingMessage);
    Serial.println("Display update finished.");
  }

  delay(20);
}