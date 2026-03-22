#pragma once

#include <states.h>
#include "DHT.h"
#include "FastLED.h"
#include "MHZ19.h"

#define FAN_PIN 13
#define SOIL_WATER_PUMP 26
#define AIR_WATER_PUMP 25
// Set to 1 if your relay module turns ON when the input pin goes LOW.
#define FAN_ACTIVE_LOW 0
#define FAN_ON_LEVEL (FAN_ACTIVE_LOW ? LOW : HIGH)
#define FAN_OFF_LEVEL (FAN_ACTIVE_LOW ? HIGH : LOW)

#define MOISTURE_SENSOR 32
#define LIGHT_SENSOR 33
#define DHTPIN 27
#define DHT_ENABLED 1

// MH_RX_PIN 10
// MH_TX_PIN 11

#define MHZ19_ENABLED 1

#define DHTTYPE DHT22

#define STRIP_PIN 23
#define NUMLEDS 10

void io_setup();
void io_loop();
