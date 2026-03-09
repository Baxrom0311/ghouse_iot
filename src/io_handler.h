#pragma once

#include <states.h>
#include "DHT.h"
#include "FastLED.h"
#include "MHZ19.h"

#define FAN_PIN 13
#define MOISTURE_SENSOR 32
#define SOIL_WATER_PUMP 26
#define AIR_WATER_PUMP 25
#define LIGHT_SENSOR 33
#define DHTPIN 4

// MH_RX_PIN 10
// MH_TX_PIN 11

#define DHTTYPE DHT22

#define STRIP_PIN 2
#define NUMLEDS 10

void io_setup();
void io_loop();