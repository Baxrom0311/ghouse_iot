#include "DHT.h"

#include <math.h>

DHT::DHT(uint8_t pin, uint8_t type, uint8_t count) {
  (void)count;
  _pin = pin;
  _type = type;
  _maxcycles = microsecondsToClockCycles(1000);
}

void DHT::begin(uint8_t usec) {
  pinMode(_pin, INPUT_PULLUP);
  _lastreadtime = millis() - kMinIntervalMs;
  pullTime = usec;
}

float DHT::readTemperature(bool S, bool force) {
  float value = NAN;
  if (!read(force)) {
    return value;
  }

  switch (_type) {
    case DHT11:
      value = data[2];
      if (data[3] & 0x80) {
        value = -1 - value;
      }
      value += (data[3] & 0x0F) * 0.1f;
      break;
    case DHT12:
      value = data[2] + (data[3] & 0x0F) * 0.1f;
      if (data[2] & 0x80) {
        value *= -1.0f;
      }
      break;
    case DHT22:
    case DHT21:
      value = (((word)(data[2] & 0x7F)) << 8 | data[3]) * 0.1f;
      if (data[2] & 0x80) {
        value *= -1.0f;
      }
      break;
    default:
      break;
  }

  if (S) {
    value = convertCtoF(value);
  }

  return value;
}

float DHT::readHumidity(bool force) {
  float value = NAN;
  if (!read(force)) {
    return value;
  }

  switch (_type) {
    case DHT11:
    case DHT12:
      value = data[0] + data[1] * 0.1f;
      break;
    case DHT22:
    case DHT21:
      value = (((word)data[0]) << 8 | data[1]) * 0.1f;
      break;
    default:
      break;
  }

  return value;
}

bool DHT::read(bool force) {
  const uint32_t now = millis();
  if (!force && ((now - _lastreadtime) < kMinIntervalMs)) {
    return _lastresult;
  }
  _lastreadtime = now;

  data[0] = data[1] = data[2] = data[3] = data[4] = 0;

  pinMode(_pin, INPUT_PULLUP);
  delay(1);

  pinMode(_pin, OUTPUT);
  digitalWrite(_pin, LOW);
  switch (_type) {
    case DHT22:
    case DHT21:
      delayMicroseconds(1100);
      break;
    case DHT11:
    case DHT12:
    default:
      delay(20);
      break;
  }

  uint32_t cycles[80];
  {
    pinMode(_pin, INPUT_PULLUP);
    delayMicroseconds(pullTime);

    InterruptLock lock;

    if (expectPulse(LOW) == kTimeout || expectPulse(HIGH) == kTimeout) {
      _lastresult = false;
      return _lastresult;
    }

    for (int i = 0; i < 80; i += 2) {
      cycles[i] = expectPulse(LOW);
      cycles[i + 1] = expectPulse(HIGH);
    }
  }

  for (int i = 0; i < 40; ++i) {
    const uint32_t lowCycles = cycles[2 * i];
    const uint32_t highCycles = cycles[2 * i + 1];
    if (lowCycles == kTimeout || highCycles == kTimeout) {
      _lastresult = false;
      return _lastresult;
    }

    data[i / 8] <<= 1;
    if (highCycles > lowCycles) {
      data[i / 8] |= 1;
    }
  }

  _lastresult = data[4] == ((data[0] + data[1] + data[2] + data[3]) & 0xFF);
  return _lastresult;
}

uint32_t DHT::expectPulse(bool level) {
#if defined(ESP32)
  const uint32_t start = ESP.getCycleCount();
  while (digitalRead(_pin) == level) {
    if ((uint32_t)(ESP.getCycleCount() - start) >= _maxcycles) {
      return kTimeout;
    }
  }
  return (uint32_t)(ESP.getCycleCount() - start);
#else
#if (F_CPU > 16000000L) || (F_CPU == 0L)
  uint32_t count = 0;
#else
  uint16_t count = 0;
#endif
  while (digitalRead(_pin) == level) {
    if (count++ >= _maxcycles) {
      return kTimeout;
    }
  }
  return count;
#endif
}

float DHT::convertCtoF(float c) { return c * 1.8f + 32.0f; }

float DHT::convertFtoC(float f) { return (f - 32.0f) * 0.55555f; }

float DHT::computeHeatIndex(bool isFahrenheit) {
  return computeHeatIndex(readTemperature(isFahrenheit), readHumidity(),
                          isFahrenheit);
}

float DHT::computeHeatIndex(float temperature, float percentHumidity,
                            bool isFahrenheit) {
  float hi;

  if (!isFahrenheit) {
    temperature = convertCtoF(temperature);
  }

  hi = 0.5f * (temperature + 61.0f + ((temperature - 68.0f) * 1.2f) +
               (percentHumidity * 0.094f));

  if (hi > 79.0f) {
    hi = -42.379f + 2.04901523f * temperature +
         10.14333127f * percentHumidity -
         0.22475541f * temperature * percentHumidity -
         0.00683783f * pow(temperature, 2) -
         0.05481717f * pow(percentHumidity, 2) +
         0.00122874f * pow(temperature, 2) * percentHumidity +
         0.00085282f * temperature * pow(percentHumidity, 2) -
         0.00000199f * pow(temperature, 2) * pow(percentHumidity, 2);

    if ((percentHumidity < 13.0f) && (temperature >= 80.0f) &&
        (temperature <= 112.0f)) {
      hi -= ((13.0f - percentHumidity) * 0.25f) *
            sqrt((17.0f - fabsf(temperature - 95.0f)) * 0.05882f);
    } else if ((percentHumidity > 85.0f) && (temperature >= 80.0f) &&
               (temperature <= 87.0f)) {
      hi += ((percentHumidity - 85.0f) * 0.1f) *
            ((87.0f - temperature) * 0.2f);
    }
  }

  return isFahrenheit ? hi : convertFtoC(hi);
}
