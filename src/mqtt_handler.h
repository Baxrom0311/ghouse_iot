#pragma once

#include <PicoMQTT.h>
#include <states.h>
#include <ArduinoJson.h>
#include <secrets_config.h>
#if MQTT_TLS_ENABLED
#include <WiFiClientSecure.h>
#endif

void mqtt_setup();
void mqtt_loop();
