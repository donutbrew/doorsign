#pragma once
#include <Arduino.h>

// Uses global display object from doorsign.ino

void drawThickCircle(int cx, int cy, int r, int thickness, uint16_t color) {
  for (int i = 0; i < thickness; i++) {
    display.drawCircle(cx, cy, r - i, color);
  }
}

void drawThickLine(int x0, int y0, int x1, int y1, int thickness, uint16_t color) {
  int half = thickness / 2;

  for (int dx = -half; dx <= half; dx++) {
    for (int dy = -half; dy <= half; dy++) {
      display.drawLine(x0 + dx, y0 + dy, x1 + dx, y1 + dy, color);
    }
  }
}

void drawAvailable(int cx, int cy, int r) {
  // Thick black smiley
  drawThickCircle(cx, cy, r, 3, GxEPD_BLACK);

  int eyeR = max(2, r / 9);
  display.fillCircle(cx - r / 3, cy - r / 4, eyeR, GxEPD_BLACK);
  display.fillCircle(cx + r / 3, cy - r / 4, eyeR, GxEPD_BLACK);

  // thicker smile made of connected segments
  drawThickLine(cx - r / 2, cy + r / 5, cx - r / 4, cy + r / 3, 2, GxEPD_BLACK);
  drawThickLine(cx - r / 4, cy + r / 3, cx, cy + r / 3 + 2, 2, GxEPD_BLACK);
  drawThickLine(cx, cy + r / 3 + 2, cx + r / 4, cy + r / 3, 2, GxEPD_BLACK);
  drawThickLine(cx + r / 4, cy + r / 3, cx + r / 2, cy + r / 5, 2, GxEPD_BLACK);
}

void drawNoSymbol(int cx, int cy, int r) {
  // Thick red no-symbol
  drawThickCircle(cx, cy, r, 5, GxEPD_RED);

  drawThickLine(
    cx - r + 6,
    cy + r - 6,
    cx + r - 6,
    cy - r + 6,
    5,
    GxEPD_RED
  );
}

void drawMeeting(int cx, int cy, int r) {
  int x = cx - PHONE_ICON_W / 2;
  int y = cy - PHONE_ICON_H / 2;

  display.drawBitmap(x, y, phone_icon_56x56, PHONE_ICON_W, PHONE_ICON_H, GxEPD_RED);
}

void drawSoon(int cx, int cy, int r) {
  // Thick clock
  drawThickCircle(cx, cy, r, 4, GxEPD_BLACK);

  // longer hands
  drawThickLine(cx, cy, cx, cy - (3 * r) / 4, 3, GxEPD_BLACK);
  drawThickLine(cx, cy, cx + (2 * r) / 3, cy + r / 4, 3, GxEPD_BLACK);

  display.fillCircle(cx, cy, max(2, r / 9), GxEPD_BLACK);
}

void drawOut(int cx, int cy, int r) {
  uint16_t color = GxEPD_RED;

  // Big dominant triangle (pointing upper-left)
  int tipX = cx - r;
  int tipY = cy - r;

  int baseRightX = cx + r ;
  int baseRightY = cy - r / 4;

  int baseBottomX = cx - r / 4;
  int baseBottomY = cy + r ;

  display.fillTriangle(
    tipX, tipY,
    baseRightX, baseRightY,
    baseBottomX, baseBottomY,
    color
  );

  // Short thick tail (just to suggest direction)
  int tailStartX = cx + r / 2;
  int tailStartY = cy + r / 2;

  int tailEndX = cx - r / 6;
  int tailEndY = cy - r / 6;

  drawThickLine(
    tailStartX, tailStartY,
    tailEndX, tailEndY,
    max(10, r / 3),
    color
  );
}

void drawRemote(int cx, int cy, int r) {
  int x = cx - WIFI_ICON_W / 2;
  int y = cy - WIFI_ICON_H / 2;

  display.drawBitmap(x, y, wifi_icon_56x56, WIFI_ICON_W, WIFI_ICON_H, GxEPD_BLACK);
}

void drawCranky(int cx, int cy, int r) {
  // Angry face
  drawThickCircle(cx, cy, r, 3, GxEPD_BLACK);

  int eyeR = max(2, r / 10);

  // eyes
  display.fillCircle(cx - r / 3, cy - r / 5, eyeR, GxEPD_BLACK);
  display.fillCircle(cx + r / 3, cy - r / 5, eyeR, GxEPD_BLACK);

  // angry eyebrows
  drawThickLine(cx - r / 2, cy - r / 2, cx - r / 5, cy - r / 3, 3, GxEPD_BLACK);
  drawThickLine(cx + r / 5, cy - r / 3, cx + r / 2, cy - r / 2, 3, GxEPD_BLACK);

  // frown
  drawThickLine(cx - r / 2, cy + r / 2, cx - r / 4, cy + r / 3, 3, GxEPD_BLACK);
  drawThickLine(cx - r / 4, cy + r / 3, cx, cy + r / 4, 3, GxEPD_BLACK);
  drawThickLine(cx, cy + r / 4, cx + r / 4, cy + r / 3, 3, GxEPD_BLACK);
  drawThickLine(cx + r / 4, cy + r / 3, cx + r / 2, cy + r / 2, 3, GxEPD_BLACK);
}

void drawStop(int cx, int cy, int r) {
  int x0 = cx - r / 2;
  int x1 = cx + r / 2;
  int x2 = cx + r;
  int x3 = cx + r / 2;
  int x4 = cx - r / 2;
  int x5 = cx - r;

  int y0 = cy - r;
  int y1 = cy - r / 2;
  int y2 = cy + r / 2;
  int y3 = cy + r;
  int y4 = cy + r / 2;
  int y5 = cy - r / 2;

  display.fillTriangle(cx, cy, x0, y0, x1, y0, GxEPD_RED);
  display.fillTriangle(cx, cy, x1, y0, x2, y1, GxEPD_RED);
  display.fillTriangle(cx, cy, x2, y1, x2, y2, GxEPD_RED);
  display.fillTriangle(cx, cy, x2, y2, x3, y3, GxEPD_RED);
  display.fillTriangle(cx, cy, x3, y3, x4, y3, GxEPD_RED);
  display.fillTriangle(cx, cy, x4, y3, x5, y4, GxEPD_RED);
  display.fillTriangle(cx, cy, x5, y4, x5, y5, GxEPD_RED);
  display.fillTriangle(cx, cy, x5, y5, x0, y0, GxEPD_RED);
}

void drawIcon(String icon, int x, int y, int w, int h) {
  int cx = x + w / 2;
  int cy = y + h / 2;
  int r = min(w, h) / 3;

  if (icon == "available") {
    drawAvailable(cx, cy, r);
  } else if (icon == "no") {
    drawNoSymbol(cx, cy, r);
  } else if (icon == "meeting") {
    drawMeeting(cx, cy, r);
  } else if (icon == "soon") {
    drawSoon(cx, cy, r);
  } else if (icon == "out") {
    drawOut(cx, cy, r);
  } else if (icon == "remote" || icon == "telework" || icon == "teleworking") {
    drawRemote(cx, cy, r);
  } else if (icon == "cranky") {
    drawCranky(cx, cy, r);
  } else if (icon == "stop") {
    drawStop(cx, cy, r);
  } else if (icon == "circle") {
    display.fillCircle(cx, cy, r, GxEPD_BLACK);
  }
}