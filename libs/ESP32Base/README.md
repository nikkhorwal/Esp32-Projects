# ESP32Base

A small reusable base library for ESP32 projects.

The goal of this library is to keep common ESP32 features out of individual `.ino` files, so each project can focus on its own hardware and application logic.

## Features

- Connects to saved Wi-Fi networks automatically
- Stores up to **5 Wi-Fi networks**
- Scans nearby networks and tries the strongest saved network first
- Falls back to other saved networks if needed
- mDNS hostname:
  - `esp32-s3-board.local`
- OTA firmware updates with ArduinoOTA
- TCP serial/log terminal on port **23**
- Mirrors `Serial0` output to the TCP terminal
- Simple terminal commands for temperature, status and Wi-Fi information
- ESP32 internal temperature access
- Keeps Wi-Fi/OTA/log handling running through `ESP32Base.loop()`

## Library Structure

```text
ESP32Base/
├── ESP32Base.h
├── ESP32Base.cpp
├── library.properties
└── README.md
```

## Version

**1.7.0**

Platform:

```text
ESP32
```

---

# How to Use

Add the library to your Arduino/ESP32 project and include:

```cpp
#include <ESP32Base.h>
```

In `setup()`:

```cpp
void setup() {
  Serial0.begin(115200);

  ESP32Base.begin();
}
```

In `loop()`:

```cpp
void loop() {
  ESP32Base.loop();

  // Your project code
}
```

That's the main pattern.

Your project can then add its own OLED, sensors, LEDs, buzzer, web server, buttons, etc.

---

# Basic Example

```cpp
#include <Arduino.h>
#include <ESP32Base.h>

void setup() {
  Serial0.begin(115200);

  ESP32Base.begin();

  Serial0.println("My ESP32 project started");
}

void loop() {
  ESP32Base.loop();

  // Application code here
}
```

`ESP32Base` handles the common network/OTA/terminal work while your application handles its own hardware.

---

# Wi-Fi

The library looks for saved Wi-Fi credentials stored in the ESP32 `Preferences` namespace:

```text
wifi
```

The credentials use these keys:

```text
ssid1
password1

ssid2
password2

ssid3
password3

ssid4
password4

ssid5
password5
```

Up to **5 networks** can be stored.

At startup the library:

1. Reads the saved networks.
2. Scans nearby Wi-Fi networks.
3. Finds saved networks that are currently visible.
4. Chooses the visible saved network with the strongest RSSI.
5. Attempts to connect.
6. If that connection fails, it tries the other saved networks.

The connection timeout for each attempt is **15 seconds**.

If no saved network is available, `ESP32Base.begin()` returns `false`.

Example:

```cpp
bool connected = ESP32Base.begin();

if (connected) {
  Serial0.println("WiFi connected");
} else {
  Serial0.println("WiFi not connected");
}
```

---

# mDNS

After Wi-Fi connects, the library starts mDNS using:

```text
esp32-s3-board.local
```

So instead of using the IP address, a device on the same network can use:

```text
esp32-s3-board.local
```

The library also advertises:

```text
_arduino._tcp → port 3232
_telnet._tcp  → port 23
```

---

# OTA Updates

The library starts ArduinoOTA automatically after a successful Wi-Fi connection.

This allows supported Arduino IDE/ESP32 workflows to upload new firmware over Wi-Fi instead of using USB every time.

The OTA hostname is:

```text
esp32-s3-board
```

OTA progress and errors are also written through `Serial0`, so they can be seen through the TCP terminal.

---

# TCP Terminal / Logs

ESP32Base starts a TCP server on:

```text
Port 23
```

Connect to:

```text
esp32-s3-board.local:23
```

or:

```text
<ESP32-IP>:23
```

The terminal receives the same output that the library/application writes through:

```cpp
Serial0.println("Hello");
Serial0.print("Value: ");
Serial0.println(value);
```

This is useful when the ESP32 is installed somewhere and you don't want to keep it connected to USB just to see logs.

Only one TCP log client is handled at a time.

---

# Terminal Commands

After connecting to port 23, type:

```text
help
```

Available commands:

```text
help
temp
status
wifi
```

## `help`

Shows the available commands.

## `temp`

Shows the ESP32 internal temperature when supported by the target:

```text
Temperature: xx.x °C
```

## `status`

Shows information such as:

```text
WiFi
IP
mDNS
RSSI
Temperature
OTA state
TCP log port
Uptime
```

## `wifi`

Scans nearby Wi-Fi networks and prints:

```text
SSID
RSSI
Connected network
IP address
```

---

# API

The public API is intentionally small.

## `ESP32Base.begin()`

Starts the common ESP32 services.

```cpp
bool connected = ESP32Base.begin();
```

Returns:

```text
true  → Wi-Fi connected and common services started
false → no Wi-Fi connection
```

## `ESP32Base.loop()`

Must be called continuously from the application's `loop()`.

```cpp
ESP32Base.loop();
```

It keeps:

- TCP terminal handling
- TCP log output
- OTA handling

running.

## `ESP32Base.isConnected()`

Returns the current Wi-Fi connection state.

```cpp
if (ESP32Base.isConnected()) {
  Serial0.println("Online");
}
```

## `ESP32Base.ipAddress()`

Returns the current IP address as a `String`.

```cpp
String ip = ESP32Base.ipAddress();
```

If not connected, it returns an empty string.

## `ESP32Base.temperature()`

Returns the ESP32 internal temperature where supported.

```cpp
float temp = ESP32Base.temperature();
```

The function returns `NAN` when the target does not provide the required temperature function.

---

# Serial0

ESP32Base provides its own `Serial0` wrapper:

```cpp
Serial0.begin(115200);
Serial0.println("Hello");
```

It sends output to the normal hardware serial interface and, when the TCP terminal is connected, also mirrors that output to the TCP client.

This means the same logging code works both through USB serial and the network terminal.

---

# Recommended Project Pattern

For most projects using this library:

```cpp
#include <Arduino.h>
#include <ESP32Base.h>

void setup() {
  Serial0.begin(115200);

  ESP32Base.begin();

  // Initialize your hardware
  // OLED
  // Sensors
  // LEDs
  // Buzzer
  // Web server
}

void loop() {
  ESP32Base.loop();

  // Handle your application
}
```

The important rule is:

> Call `ESP32Base.loop()` regularly.

This allows OTA and the TCP terminal to continue working while your project runs.

---

# When NOT to Use `ESP32Base.begin()`

A project can include the library without starting its Wi-Fi/OTA system.

For example, an offline electronics project may only use the library's `Serial0` compatibility:

```cpp
#include <ESP32Base.h>

void setup() {
  Serial0.begin(115200);
}

void loop() {
  // Offline application
}
```

In that case, do not call:

```cpp
ESP32Base.begin();
```

This is useful for projects that should work completely offline.

---

# Dependencies

ESP32Base uses ESP32/Arduino framework components:

- Arduino ESP32 `WiFi`
- `Preferences`
- `ArduinoOTA`
- `ESPmDNS`
- `Arduino`

No additional third-party library is required for the core ESP32Base functionality.

---

# Design Philosophy

ESP32Base is meant to be a **common foundation**, not the application itself.

Instead of repeating this in every project:

```text
Wi-Fi connection
OTA setup
mDNS setup
TCP logging
Serial handling
Wi-Fi status
Temperature/status commands
```

a project can simply use:

```cpp
#include <ESP32Base.h>

ESP32Base.begin();
ESP32Base.loop();
```

Then the `.ino` file can focus on what makes that particular project different.
