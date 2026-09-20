# 🎂 Heer Birthday ESP32

A small ESP32-S3 birthday project for **Heer** combining a 0.96-inch OLED display, buzzer melody, RGB lighting, and a phone-friendly web control page.

The project is designed to play a **Happy Birthday** tune while showing synchronized lyrics on the OLED and changing the RGB lighting according to the lyric currently being played.

## 📸 Wiring / Hardware Setup

This is the actual hardware setup used as the visual reference for this project:

![Heer Birthday ESP32 wiring](wiring.jpg)

## ✨ What the project does

- 🎵 Plays a Happy Birthday melody through the buzzer.
- 🖥️ Shows **HAPPY BIRTHDAY HEER** on the OLED when idle.
- 📝 Displays the birthday lyrics and highlights the current word while the song plays.
- 🌈 Changes the RGB LED colour according to the current lyric.
- 🕯️ Supports five optional candle LEDs in the current sketch.
- 🎉 Runs a colourful party-light effect after the song finishes.
- 📱 Provides a web page to control the birthday show from a phone.
- 🔊 Includes a volume slider.
- ⏹️ Includes PLAY and STOP controls.
- 🌈 Shows a slow rainbow effect while waiting for the song to start.

## 🔩 Main Components

| Component | Purpose |
|---|---|
| ESP32-S3 DevKitC-1 | Main controller |
| 0.96-inch OLED | Birthday message and lyrics |
| Passive buzzer | Happy Birthday melody |
| 4-pin RGB LED | Colour effects synchronized with music |
| 330Ω resistors | Current limiting for RGB channels |
| Optional LEDs | Candle effect |

## 🔌 Pin Mapping — Current Arduino Sketch

The uploaded `.ino` file currently defines the following pins:

| Device | Pin | ESP32 GPIO |
|---|---|---:|
| OLED | SDA | GPIO 13 |
| OLED | SCK / SCL | GPIO 14 |
| Buzzer | + | GPIO 7 |
| Buzzer | − | GND |
| RGB red channel | R | GPIO 4 |
| RGB green channel | G | GPIO 5 |
| RGB blue channel | B | GPIO 6 |

The current sketch also contains support for five optional candle LEDs:

| Candle | GPIO |
|---|---:|
| Candle 1 | 15 |
| Candle 2 | 16 |
| Candle 3 | 17 |
| Candle 4 | 18 |
| Candle 5 | 10 |

### RGB LED

The sketch is configured for a **common-anode RGB LED**:

- Common/anode → **3.3V**
- Red → GPIO 4 through resistor
- Green → GPIO 5 through resistor
- Blue → GPIO 6 through resistor

Each RGB colour channel should have its own resistor, typically **220Ω–330Ω**.

Because this is common-anode, the RGB outputs are handled as active-low in the software.

## 🖥️ OLED

OLED connections:

```text
OLED GND  → ESP32 GND
OLED VDD  → ESP32 3V3
OLED SDA  → ESP32 GPIO 13
OLED SCK  → ESP32 GPIO 14
```

The OLED is an I²C display. **SDA and SCL/SCK must use separate GPIOs.**

> Note: an earlier wiring note mentioned both SDA and SCK going to GPIO9. That is not a normal two-wire I²C connection. The current uploaded sketch uses GPIO13 for SDA and GPIO14 for SCL/SCK, so the physical wiring and sketch should match.

## 🔊 Buzzer

```text
Buzzer + → ESP32 GPIO 7
Buzzer - → ESP32 GND
```

The project uses PWM/LEDC to generate the melody.

The melody contains the notes of the Happy Birthday song and controls the buzzer timing without blocking the main program.

## 🎶 Birthday Sequence

### 1. Waiting

OLED:

```text
HAPPY
BIRTHDAY
HEER
```

The RGB light slowly cycles through colours.

### 2. Song starts

The OLED changes to the lyrics:

```text
HAPPY
BIRTHDAY
TO YOU
```

and then:

```text
HAPPY
BIRTHDAY
DEAR HEER
```

The currently spoken/sung word is highlighted.

### 3. RGB synchronization

The RGB colour follows the lyric:

| Lyric | Colour |
|---|---|
| HAPPY | Pink |
| BIRTHDAY | Blue |
| TO | Cyan |
| YOU | Green |
| DEAR | Red |
| HEER | Magenta |

The brightness also pulses with the notes.

### 4. Song finished

The OLED displays a final birthday message and the LEDs enter a colourful party effect.

After a few seconds, the display returns to the ready screen.

## 📱 Web Control

The ESP32 hosts a small web page after connecting to Wi-Fi.

The page provides:

- ▶️ PLAY
- ⏹️ STOP
- 🔊 Volume control
- Birthday-themed interface

The project uses the `ESP32Base` library for the Wi-Fi/base functionality.

## 📚 Required Libraries

The sketch uses:

```cpp
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WebServer.h>
#include <ESP32Base.h>
```

Make sure these libraries are available before compiling.

## 📁 Project Files

```text
Heer_Birthday_ESP32/
├── README.md
├── wiring.jpg
└── Heer_Birthday_ESP32_Lyrics_Music_RGB.ino
```

## 🎁 Project Goal

This is intended as a small physical birthday gift for **Heer**:

**ESP32 + OLED + Buzzer + RGB lighting = a mini interactive Happy Birthday machine.** 🎂✨

The idea is that pressing **PLAY** from a phone starts the complete sequence: music, lyrics, synchronized colours, and candle-style lighting effects.
