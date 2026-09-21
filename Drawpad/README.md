# ESP32 Drawpad

A small ESP32-S3 drawing pad using a 128x64 SSD1306 OLED and a wireless phone-based drawing interface.

Draw on the phone and the drawing is mirrored live on the OLED over Wi-Fi using WebSocket communication.

## Hardware

- ESP32-S3 DevKitC-1
- 0.96" SSD1306 I2C OLED (128x64)
- Breadboard and jumper wires
- USB-C cable / power source

## Wiring

### OLED

| OLED | ESP32 |
|---|---|
| GND | GND |
| VCC | 3V3 |
| SCL / SCK | GPIO9 |
| SDA | GPIO8 |

### Connection diagram

```text
ESP32-S3                 SSD1306 OLED
────────                 ────────────
3V3  ──────────────────> VCC
GND  ──────────────────> GND
GPIO9 ─────────────────> SCL / SCK
GPIO8 ─────────────────> SDA
```

## Drawing Tools

The phone web interface includes:

- **Pen** — freehand drawing
- **Eraser** — erase pixels
- **Line** — draw straight lines
- **Rectangle** — draw rectangles
- **Circle** — draw circles / ellipses
- **Size** — adjustable drawing size
- **Undo** — undo the previous drawing action
- **Redo** — restore an undone action
- **Clear** — clear the complete canvas

## How It Works

```text
Phone Browser
     │
     │ Wi-Fi / WebSocket
     ▼
 ESP32-S3
     │
     │ I2C
     ▼
 SSD1306 OLED
```

The phone provides a large touch-friendly drawing canvas.

The ESP32 converts the drawing coordinates to the OLED's **128x64 pixel resolution** and updates the display in real time.

## Display

- Controller: SSD1306
- Resolution: 128 × 64
- Interface: I2C
- I2C Address: `0x3C`
- SDA: GPIO8
- SCL: GPIO9

## Software

The project uses:

- ESP32-S3
- Wi-Fi
- WebServer
- WebSocket
- SSD1306 OLED
- Adafruit GFX
- Adafruit SSD1306
- ESP32Base library

## Accessing the Drawpad

After uploading the sketch and connecting the ESP32-S3 to Wi-Fi, open the ESP32 web interface from a phone connected to the same network.

The project can use:

```text
http://esp32-s3-board.local
```

or the ESP32's IP address.

The web interface shows the drawing pad and connection status.

## Notes

The phone drawing area is intentionally much larger than the OLED so it is comfortable to use on a touchscreen.

The actual OLED output is limited to:

```text
128 × 64 pixels
```

No external resistor is required for the basic OLED wiring.

## Project Files

```text
Drawpad/
├── circuit.json
├── code.ino
└── README.md
```

### circuit.json

Contains the project parts and ESP32-to-OLED connections.

### code.ino

Main ESP32-S3 firmware containing the OLED drawing engine, Wi-Fi/WebSocket server and web interface.

### README.md

Project documentation and wiring information.

## Project

**ESP32 Drawpad — Pixel Studio**

A simple electronics project combining:

- ESP32-S3
- OLED graphics
- I2C
- Wi-Fi
- WebSockets
- Touch-friendly web UI
- Real-time drawing
