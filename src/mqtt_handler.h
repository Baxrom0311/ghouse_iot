#pragma once

#include <PicoMQTT.h>
#include <states.h>
#include <ArduinoJson.h>
#include <secrets_config.h>

#define MQTT_IP MQTT_BROKER_IP
#define MQTT_ID MQTT_CLIENT_ID

// #define

void mqtt_setup();
void mqtt_loop();
