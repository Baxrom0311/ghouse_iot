#pragma once

#include <states.h>
#include "DHT.h"
#include "FastLED.h"

#define FAN_PIN 25
#define SOIL_WATER_PUMP 27
#define AIR_WATER_PUMP 14
#define LED_RELAY_PIN 26
// Set to 1 if your relay module turns ON when the input pin goes LOW.
#define FAN_ACTIVE_LOW 1
#define SOIL_WATER_PUMP_ACTIVE_LOW 1
#define AIR_WATER_PUMP_ACTIVE_LOW 1
#define LED_RELAY_ACTIVE_LOW 1
#define FAN_OFF_USES_INPUT_MODE 1
#define SOIL_WATER_PUMP_OFF_USES_INPUT_MODE 1
#define AIR_WATER_PUMP_OFF_USES_INPUT_MODE 1
#define LED_RELAY_OFF_USES_INPUT_MODE 1
#define FAN_ON_LEVEL (FAN_ACTIVE_LOW ? LOW : HIGH)
#define FAN_OFF_LEVEL (FAN_ACTIVE_LOW ? HIGH : LOW)
#define SOIL_WATER_PUMP_ON_LEVEL (SOIL_WATER_PUMP_ACTIVE_LOW ? LOW : HIGH)
#define SOIL_WATER_PUMP_OFF_LEVEL (SOIL_WATER_PUMP_ACTIVE_LOW ? HIGH : LOW)
#define AIR_WATER_PUMP_ON_LEVEL (AIR_WATER_PUMP_ACTIVE_LOW ? LOW : HIGH)
#define AIR_WATER_PUMP_OFF_LEVEL (AIR_WATER_PUMP_ACTIVE_LOW ? HIGH : LOW)
#define LED_RELAY_ON_LEVEL (LED_RELAY_ACTIVE_LOW ? LOW : HIGH)
#define LED_RELAY_OFF_LEVEL (LED_RELAY_ACTIVE_LOW ? HIGH : LOW)
#define WATER_PUMP_MAX_RUNTIME_MS 120000UL

#define MOISTURE_SENSOR 32

// LDR module DO/D0 digital output is wired to ESP32 D33/GPIO33.
#define LIGHT_SENSOR 33
#define LIGHT_SENSOR_DIGITAL 1
#define LIGHT_SENSOR_DIGITAL_INPUT_MODE INPUT_PULLUP
// If the displayed light value is inverted, change this to LOW.
#define LIGHT_SENSOR_DIGITAL_LIGHT_LEVEL HIGH
#define ANALOG_SENSOR_MIN_VALID_RAW 1
#define ANALOG_SENSOR_MAX_VALID_RAW 4094

#define DHTPIN 23
#define DHT_ENABLED 1

#define MQ135_PIN 34
#define MQ135_ENABLED 1
#define MQ135_CLEAN_AIR_RAW 300
#define MQ135_POLLUTED_AIR_RAW 2500
#define MQ135_MIN_PPM 400
#define MQ135_MAX_PPM 2000

// Change to DHT11 if your module is not DHT22.
#define DHTTYPE DHT11

#define STRIP_PIN 23
#define NUMLEDS 10

// Keep the old FastLED strip implementation available, but drive the current
// plain LED through a relay by default.
#define LED_DRIVER_RELAY 1

#if !LED_DRIVER_RELAY && DHTPIN == STRIP_PIN
#error "DHTPIN conflicts with STRIP_PIN when FastLED strip mode is enabled."
#endif

void io_setup();
void io_loop();
