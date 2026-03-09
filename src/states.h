#pragma once

#include <Arduino.h>

struct States
{
    int humidity = 0;
    int temperature = 0;
    int air_co2 = 0;
    int soil_moisture = 0;
    int light = 0;

    bool soil_water_pump = false;
    bool air_water_pump = false;
    bool led = false;
    bool fan = false;
    bool sensors_ready = false;
};
struct SensorSettings
{
    int min_humidity = 40;
    int max_humidity = 70;

    int min_temperature = 18;
    int max_temperature = 28;

    int min_moisture = 35;
    int max_moisture = 70;

    int min_air_co2 = 400;
    int max_air_co2 = 1200;

    int min_light = 20;
    int max_light = 60;

    bool ai_mode = true;
};

enum class CommandType
{
    CMD_LED,
    CMD_SOIL_WATER_PUMP,
    CMD_AIR_WATER_PUMP,
    CMD_FAN,
};
struct Command
{
    CommandType type;
    int value;
};

enum class CallbackType
{
    CLB_UPDATE
};
struct Callback
{
    CallbackType type;
};

extern QueueHandle_t mqttToIo;
extern QueueHandle_t ioToMqtt;
extern States agro_state;
extern SensorSettings agro_settings;

void load_settings();
void save_ai_mode();
void save_sensor_settings();
