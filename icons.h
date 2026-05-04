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
  // Red telephone handset, stylized as a chunky diagonal receiver
  int x0 = cx - r + 2;
  int y0 = cy + r / 3;
  int x1 = cx + r - 2;
  int y1 = cy - r / 3;

  // main diagonal body
  drawThickLine(x0, y0, x1, y1, max(7, r / 3), GxEPD_RED);

  // larger handset ends
  display.fillRoundRect(x0 - r / 5, y0 - r / 4, r / 2, r / 2, 5, GxEPD_RED);
  display.fillRoundRect(x1 - r / 3, y1 - r / 4, r / 2, r / 2, 5, GxEPD_RED);

  // carve a small white inner gap to make it read less like a blob
  drawThickLine(
    cx - r / 3,
    cy + r / 9,
    cx + r / 3,
    cy - r / 9,
    3,
    GxEPD_WHITE
  );
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
  // Very thick red diagonal arrow pointing upper-left
  int tailX = cx + r;
  int tailY = cy + r;
  int headX = cx - r;
  int headY = cy - r;

  drawThickLine(tailX, tailY, headX + r / 3, headY + r / 3, max(8, r / 3), GxEPD_RED);

  // arrow head
  display.fillTriangle(
    headX, headY,
    headX + r, headY + r / 5,
    headX + r / 5, headY + r,
    GxEPD_RED
  );
}

void drawRemote(int cx, int cy, int r) {
  // Chunky WiFi symbol: dot + three wedge-like arcs

  int dotR = max(3, r / 8);
  display.fillCircle(cx, cy + r / 2, dotR, GxEPD_BLACK);

  // inner arc
  drawThickLine(cx - r / 3, cy + r / 4, cx, cy, 4, GxEPD_BLACK);
  drawThickLine(cx, cy, cx + r / 3, cy + r / 4, 4, GxEPD_BLACK);

  // middle arc
  drawThickLine(cx - (2 * r) / 3, cy, cx - r / 4, cy - r / 4, 4, GxEPD_BLACK);
  drawThickLine(cx - r / 4, cy - r / 4, cx, cy - r / 3, 4, GxEPD_BLACK);
  drawThickLine(cx, cy - r / 3, cx + r / 4, cy - r / 4, 4, GxEPD_BLACK);
  drawThickLine(cx + r / 4, cy - r / 4, cx + (2 * r) / 3, cy, 4, GxEPD_BLACK);

  // outer arc
  drawThickLine(cx - r, cy - r / 4, cx - r / 2, cy - r / 2, 4, GxEPD_BLACK);
  drawThickLine(cx - r / 2, cy - r / 2, cx, cy - (2 * r) / 3, 4, GxEPD_BLACK);
  drawThickLine(cx, cy - (2 * r) / 3, cx + r / 2, cy - r / 2, 4, GxEPD_BLACK);
  drawThickLine(cx + r / 2, cy - r / 2, cx + r, cy - r / 4, 4, GxEPD_BLACK);
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