# ESP32 BLE E-Ink Message Display (Version 1)

## Overview

A Bluetooth-controlled message display using an ESP32-C3 and a tri-color
(black/white/red) e-paper screen. Messages are sent from a phone via BLE
and rendered with rich text formatting including sizes, colors, and
boxed highlights.

------------------------------------------------------------------------

## Hardware Requirements

### Core Components

-   ESP32-C3 Zero (or compatible ESP32-C3 board)
-   WeAct 2.13" Tri-Color E-Paper Display (Black/White/Red, SPI)

### Optional

-   Jumper wires (female-to-female)
-   Breadboard or soldered headers

------------------------------------------------------------------------

## Wiring (ESP32-C3 → E-Paper)

VCC → 3V3\
GND → GND\
SCL → GPIO 4\
SDA → GPIO 3\
CS → GPIO 7\
DC → GPIO 2\
RES → GPIO 1\
BUSY → GPIO 10

Note: SCL = SPI Clock, SDA = MOSI

------------------------------------------------------------------------

## Software Setup

### Arduino IDE

-   Install ESP32 board support
-   Select board: ESP32C3 Dev Module

### Libraries

Install via Library Manager: - GxEPD2 - Adafruit GFX Library - ESP32 BLE
Arduino

### Settings

-   USB CDC On Boot: Enabled
-   Upload Speed: 115200+
-   Press RESET if display does not update after upload

------------------------------------------------------------------------

## BLE Usage

### Device Name

ESP32-EINK-MSG

### UUIDs

Service: 6E400001-B5A3-F393-E0A9-E50E24DCCA9E\
Characteristic: 6E400002-B5A3-F393-E0A9-E50E24DCCA9E

### Apps

-   nRF Connect (iOS/Android)

### Sending Messages

-   Connect to device
-   Find writable characteristic
-   Send UTF-8 text

------------------------------------------------------------------------

## Text Formatting

### Line Breaks

Use: `\n`{=tex}

Example: Dinner is ready`\nCome `{=tex}downstairs

------------------------------------------------------------------------

### Text Sizes

\[big\]Large\[/big\]\
\[med\]Medium\[/med\]\
\[small\]Small\[/small\]

Default: med

------------------------------------------------------------------------

### Red Text

{This text is red}

------------------------------------------------------------------------

### Red Box (White Text)

{{This text is white in a red box}}

Spaces are preserved inside double braces.

------------------------------------------------------------------------

### Combined Example

\[big\]WT{F}\[/big\]`\n{{Clint's Office}}`{=tex}`\n[small]`{=tex}Why are
you standing there\[/small\]

------------------------------------------------------------------------

## Presets

### Recall

1\
2\
3

### Set

SET1:Message\
SET2:Message\
SET3:Message

------------------------------------------------------------------------

## Behavior Notes

-   Text is centered horizontally and vertically
-   Messages auto-shrink if too tall
-   Extra lines may be omitted if still too large
-   E-paper refresh is slow (normal)

------------------------------------------------------------------------

## Key Functions

drawMessage(msg): renders message\
layoutText(msg, width): parses and wraps text\
shrinkMarkupSizes(msg): reduces size if needed\
setFontBySize(size): sets font\
lineHeightForSize(size): calculates spacing

------------------------------------------------------------------------

## Startup Behavior

-   BLE advertising starts
-   Preset 1 is displayed automatically

------------------------------------------------------------------------

## Tips

-   Press RESET if needed after upload
-   Use UTF-8 strings only
-   Smart punctuation is normalized automatically

------------------------------------------------------------------------

## Version 1 Features

-   BLE message input
-   Rich text formatting
-   Word wrapping
-   Centered layout
-   Red text and boxed highlights
-   Presets (3 slots)
-   Auto font scaling

------------------------------------------------------------------------

## Version 2 Ideas

-   Icons on left side
-   Buttons for local control
-   Deep sleep mode
-   Custom fonts

