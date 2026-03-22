#ifndef DHT_H
#define DHT_H

#include "Arduino.h"

static const uint8_t DHT11{11};
static const uint8_t DHT12{12};
static const uint8_t DHT21{21};
static const uint8_t DHT22{22};
static const uint8_t AM2301{21};

class DHT {
 public:
  DHT(uint8_t pin, uint8_t type, uint8_t count = 6);

  void begin(uint8_t usec = 55);
  float readTemperature(bool S = false, bool force = false);
  float readHumidity(bool force = false);
  bool read(bool force = false);

  float convertCtoF(float);
  float convertFtoC(float);
  float computeHeatIndex(bool isFahrenheit = true);
  float computeHeatIndex(float temperature, float percentHumidity,
                         bool isFahrenheit = true);

 private:
  static constexpr uint32_t kMinIntervalMs = 2000;
  static constexpr uint32_t kTimeout = UINT32_MAX;

  uint8_t data[5]{};
  uint8_t _pin;
  uint8_t _type;
  uint32_t _lastreadtime{0};
  uint32_t _maxcycles{0};
  bool _lastresult{false};
  uint8_t pullTime{55};

  uint32_t expectPulse(bool level);
};

class InterruptLock {
 public:
  InterruptLock() {
#if !defined(ARDUINO_ARCH_NRF52)
    noInterrupts();
#endif
  }

  ~InterruptLock() {
#if !defined(ARDUINO_ARCH_NRF52)
    interrupts();
#endif
  }
};

#endif
