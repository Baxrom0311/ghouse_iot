#pragma once

#include <PicoMQTT.h>
#include <states.h>
#include <ArduinoJson.h>
#include <secrets_config.h>

#define MQTT_IP MQTT_BROKER_IP

void mqtt_setup();
void mqtt_loop();
