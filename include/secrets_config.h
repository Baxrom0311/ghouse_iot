#pragma once

#if defined(__has_include)
#if __has_include("secrets.h")
#include "secrets.h"
#else
#ifndef ALLOW_EXAMPLE_SECRETS
#error "Create include/secrets.h from include/secrets.example.h before building firmware. Define ALLOW_EXAMPLE_SECRETS only for documentation/demo builds."
#endif
#include "secrets.example.h"
#endif
#else
#include "secrets.example.h"
#endif

#ifndef MQTT_USERNAME
#define MQTT_USERNAME ""
#endif

#ifndef MQTT_PASSWORD
#define MQTT_PASSWORD ""
#endif

#ifndef MQTT_BROKER_HOST
#ifdef MQTT_BROKER_IP
#define MQTT_BROKER_HOST MQTT_BROKER_IP
#else
#define MQTT_BROKER_HOST "192.168.1.10"
#endif
#endif

#ifndef MQTT_BROKER_PORT
#define MQTT_BROKER_PORT 1883
#endif

#ifndef MQTT_TLS_ENABLED
#define MQTT_TLS_ENABLED 0
#endif

#ifndef MQTT_TLS_HANDSHAKE_TIMEOUT_SEC
#define MQTT_TLS_HANDSHAKE_TIMEOUT_SEC 15
#endif

#ifndef OTA_HOSTNAME
#define OTA_HOSTNAME "agroai-esp32"
#endif

#ifndef OTA_PASSWORD
#define OTA_PASSWORD "change-me-before-flashing"
#endif

#if MQTT_TLS_ENABLED
extern const char MQTT_ROOT_CA[] PROGMEM;
#endif
