#include <Arduino.h>
#include <SPI.h>
#include <vector>
#include <Preferences.h>
#include "esp_sleep.h"

#include <GxEPD2_3C.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>

// ---------- Power mode ----------
#define ENABLE_DEEP_SLEEP false

// Only used when ENABLE_DEEP_SLEEP is true
#define BLE_WAKE_WINDOW_MS 60000

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
#define SERVICE_UUID                 "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define WRITE_CHARACTERISTIC_UUID    "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define ICONS_CHARACTERISTIC_UUID    "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
#define PRESETS_CHARACTERISTIC_UUID  "6E400004-B5A3-F393-E0A9-E50E24DCCA9E"
#define STATUS_CHARACTERISTIC_UUID   "6E400005-B5A3-F393-E0A9-E50E24DCCA9E"

BLECharacteristic *presetListCharacteristic = nullptr;
BLECharacteristic *iconListCharacteristic = nullptr;
BLECharacteristic *statusCharacteristic = nullptr;

bool bleClientConnected = false;

// ---------- State ----------
String pendingMessage = "";
bool hasPendingMessage = false;
String lastDrawnMessage = "";

int buttonSelectedPreset = 0;
unsigned long lastButtonPressTime = 0;
bool buttonSelectionPending = false;

#if ENABLE_DEEP_SLEEP
unsigned long awakeStartedAt = 0;
bool skipFirstButtonPressAfterWake = true;
#endif

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

// ---------- Persistence ----------
String presetKey(int index) {
  return "p" + String(index + 1);
}

void saveLastMessage(String msg) {
  prefs.putString("lastMsg", msg);
  Serial.println("Saved last displayed message.");
}

String loadLastMessage() {
  return prefs.getString("lastMsg", presets[0]);
}

void loadSavedPresets() {
  for (int i = 0; i < NUM_PRESETS; i++) {
    presets[i] = prefs.getString(presetKey(i).c_str(), presets[i]);
  }

  Serial.println("Loaded presets:");
  for (int i = 0; i < NUM_PRESETS; i++) {
    Serial.println(String(i + 1) + ": " + presets[i]);
  }
}

void savePreset(int index) {
  if (index < 0 || index >= NUM_PRESETS) return;

  prefs.putString(presetKey(index).c_str(), presets[index]);
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

// ---------- Metadata helpers ----------
String cleanPresetLabel(String msg) {
  ParsedMessage parsed = parseMessage(msg);
  String text = parsed.text;

  text.replace("\\n", " ");
  text.replace("\\r", " ");
  text.replace("\n", " ");
  text.replace("\r", " ");

  text.replace("[big]", "");
  text.replace("[/big]", "");
  text.replace("[med]", "");
  text.replace("[/med]", "");
  text.replace("[small]", "");
  text.replace("[/small]", "");

  text.replace("{{", "");
  text.replace("}}", "");
  text.replace("{", "");
  text.replace("}", "");

  text.replace("|", "/");

  while (text.indexOf("  ") >= 0) {
    text.replace("  ", " ");
  }

  text.trim();

  if (text.length() > 28) {
    text = text.substring(0, 28);
  }

  return text;
}

String buildIconListString() {
  return "available|meeting|no|out|soon|remote|cranky|stop|circle";
}

String buildPresetListString() {
  String out = "";

  for (int i = 0; i < NUM_PRESETS; i++) {
    if (i > 0) out += "|";
    out += String(i + 1);
    out += "|";
    out += cleanPresetLabel(presets[i]);
  }

  return out;
}

void refreshMetadataCharacteristics() {
  if (iconListCharacteristic != nullptr) {
    iconListCharacteristic->setValue(buildIconListString().c_str());
  }

  if (presetListCharacteristic != nullptr) {
    String presetList = buildPresetListString();
    presetListCharacteristic->setValue(presetList.c_str());

    if (bleClientConnected) {
      presetListCharacteristic->notify();
    }
  }

  if (statusCharacteristic != nullptr) {
    statusCharacteristic->setValue(pendingMessage.c_str());

    if (bleClientConnected) {
      statusCharacteristic->notify();
    }
  }
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

int totalLinesHeight(std::vector<TextLine> &lines) {
  int totalHeight = 0;
  for (auto &line : lines) totalHeight += line.height;
  return totalHeight;
}

bool anyLineTooWide(std::vector<TextLine> &lines, int maxWidth) {
  for (auto &line : lines) {
    if (line.width > maxWidth) return true;
  }
  return false;
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
    int totalHeight = totalLinesHeight(lines);

    for (int shrinkPass = 0; shrinkPass < 2; shrinkPass++) {
      bool tooTall = totalHeight > display.height() - 4;
      bool tooWide = anyLineTooWide(lines, textMaxWidth);

      if (!tooTall && !tooWide) break;

      Serial.println(
        String("Message too large; shrinking text. tooTall=") +
        String(tooTall) +
        " tooWide=" +
        String(tooWide)
      );

      parsed.text = shrinkMarkupSizes(parsed.text);
      lines = layoutText(parsed.text, textMaxWidth);
      totalHeight = totalLinesHeight(lines);
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

      int x = textX + max(0, (textMaxWidth - line.width) / 2);

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

// ---------- Sleep ----------
#if ENABLE_DEEP_SLEEP
void goToSleep() {
  Serial.println("Entering deep sleep.");
  delay(100);

  BLEDevice::deinit(true);
  delay(100);

  esp_sleep_enable_ext0_wakeup((gpio_num_t)BUTTON_PIN, 0);
  delay(100);

  esp_deep_sleep_start();
}

void handleSleepTimer() {
  if (millis() - awakeStartedAt > BLE_WAKE_WINDOW_MS) {
    goToSleep();
  }
}
#endif

// ---------- Button ----------
void handleButton() {
#if ENABLE_DEEP_SLEEP
  if (skipFirstButtonPressAfterWake) {
    if (digitalRead(BUTTON_PIN) == HIGH) {
      skipFirstButtonPressAfterWake = false;
      Serial.println("Wake button released; button control enabled.");
    }
    return;
  }
#endif

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
bool parsePresetRecall(String value, int &indexOut) {
  value.trim();

  for (int i = 0; i < value.length(); i++) {
    if (!isDigit(value[i])) return false;
  }

  int slot = value.toInt();
  if (slot < 1 || slot > NUM_PRESETS) return false;

  indexOut = slot - 1;
  return true;
}

bool parsePresetSet(String value, int &indexOut, String &messageOut) {
  if (!value.startsWith("SET")) return false;

  int colon = value.indexOf(':');
  if (colon < 4) return false;

  String slotText = value.substring(3, colon);
  for (int i = 0; i < slotText.length(); i++) {
    if (!isDigit(slotText[i])) return false;
  }

  int slot = slotText.toInt();
  if (slot < 1 || slot > NUM_PRESETS) return false;

  indexOut = slot - 1;
  messageOut = value.substring(colon + 1);
  return true;
}

class MessageCallback : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *characteristic) {
    String value = characteristic->getValue().c_str();
    value.trim();

    if (value.length() == 0) return;

#if ENABLE_DEEP_SLEEP
    awakeStartedAt = millis();
#endif

    Serial.println("Received: " + value);

    int presetIndex = -1;
    String newPresetMessage = "";

    if (parsePresetRecall(value, presetIndex)) {
      pendingMessage = presets[presetIndex];
      buttonSelectedPreset = presetIndex;
    } else if (parsePresetSet(value, presetIndex, newPresetMessage)) {
      presets[presetIndex] = newPresetMessage;
      savePreset(presetIndex);
      refreshMetadataCharacteristics();

      pendingMessage = presets[presetIndex];
      buttonSelectedPreset = presetIndex;
    } else if (value == "RESETPRESETS") {
      prefs.clear();
      loadSavedPresets();
      refreshMetadataCharacteristics();

      Serial.println("Cleared saved presets and last message. Reboot to reload defaults.");
      pendingMessage = "{{Presets reset}}";
    } else {
      pendingMessage = value;
    }

    hasPendingMessage = true;
  }
};

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *server) {
    bleClientConnected = true;
    Serial.println("Client connected");

#if ENABLE_DEEP_SLEEP
    awakeStartedAt = millis();
#endif
  }

  void onDisconnect(BLEServer *server) {
    bleClientConnected = false;
    Serial.println("Client disconnected; restarting advertising");
    BLEDevice::startAdvertising();

#if ENABLE_DEEP_SLEEP
    awakeStartedAt = millis();
#endif
  }
};

void setupBLE() {
  BLEDevice::deinit(true);
  delay(300);

  BLEDevice::init("ESP32-EINK-MSG");

  BLEServer *server = BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  BLEService *service = server->createService(SERVICE_UUID);

  BLECharacteristic *writeCharacteristic = service->createCharacteristic(
    WRITE_CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_WRITE |
    BLECharacteristic::PROPERTY_WRITE_NR
  );

  writeCharacteristic->setCallbacks(new MessageCallback());
  writeCharacteristic->setValue("Send preset number, SETn:text, or custom text");

  iconListCharacteristic = service->createCharacteristic(
    ICONS_CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ
  );

  presetListCharacteristic = service->createCharacteristic(
    PRESETS_CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_NOTIFY
  );

  statusCharacteristic = service->createCharacteristic(
    STATUS_CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_NOTIFY
  );

  refreshMetadataCharacteristics();

  service->start();

  BLEAdvertising *advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(SERVICE_UUID);
  advertising->setScanResponse(true);
  advertising->setMinPreferred(0x06);
  advertising->setMinPreferred(0x12);

  BLEDevice::startAdvertising();

  Serial.println("BLE advertising as ESP32-EINK-MSG");
}

void setup() {
  delay(3000);

  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("===== ESP32-C3 E-PAPER BLE MESSENGER BOOT =====");

#if ENABLE_DEEP_SLEEP
  Serial.println("Power mode: deep sleep enabled");
  awakeStartedAt = millis();

  esp_sleep_wakeup_cause_t wakeCause = esp_sleep_get_wakeup_cause();
  if (wakeCause == ESP_SLEEP_WAKEUP_EXT0) {
    Serial.println("Woke from button press.");
    skipFirstButtonPressAfterWake = true;
  } else {
    Serial.println("Cold boot or reset.");
    skipFirstButtonPressAfterWake = false;
  }
#else
  Serial.println("Power mode: always on");
#endif

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  prefs.begin("doorsign", false);
  loadSavedPresets();

  SPI.begin(PIN_SPI_SCK, -1, PIN_SPI_MOSI, PIN_EPD_CS);

  display.init(115200);

  setupBLE();

  pendingMessage = loadLastMessage();
  lastDrawnMessage = "";
  hasPendingMessage = true;
}

void loop() {
  handleButton();

  if (hasPendingMessage) {
    hasPendingMessage = false;

    Serial.println("Updating display from loop...");
    display.init(115200);
    drawMessage(pendingMessage);
    saveLastMessage(pendingMessage);
    lastDrawnMessage = pendingMessage;
    refreshMetadataCharacteristics();
    Serial.println("Display update finished.");

#if ENABLE_DEEP_SLEEP
    awakeStartedAt = millis();
#endif
  }

#if ENABLE_DEEP_SLEEP
  handleSleepTimer();
#endif

  delay(20);
}