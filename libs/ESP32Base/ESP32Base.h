#ifndef ESP32BASE_H
#define ESP32BASE_H

#include <Arduino.h>

class ESP32BaseSerialClass : public Print {
public:
  void begin(unsigned long baud);
  void end();
  operator bool() const;
  size_t write(uint8_t c) override;
  size_t write(const uint8_t *buffer, size_t size) override;
  using Print::write;
private:
  bool started = false;
};

extern ESP32BaseSerialClass ESP32BaseSerial;
#define Serial0 ESP32BaseSerial

class ESP32BaseClass {
public:
  bool begin();
  void loop();
  bool isConnected();
  String ipAddress();
  float temperature();
};

extern ESP32BaseClass ESP32Base;

#endif
