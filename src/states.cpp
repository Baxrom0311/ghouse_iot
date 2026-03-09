#include <Preferences.h>
#include <states.h>

QueueHandle_t mqttToIo;
QueueHandle_t ioToMqtt;

States agro_state;

SensorSettings agro_settings;

namespace
{
Preferences preferences;
constexpr const char *PREFERENCES_NAMESPACE = "agroai";
}

void load_settings()
{
    preferences.begin(PREFERENCES_NAMESPACE, true);
    agro_settings.ai_mode = preferences.getBool("ai_mode", agro_settings.ai_mode);
    agro_settings.min_humidity = preferences.getInt(
        "min_humidity", agro_settings.min_humidity
    );
    agro_settings.max_humidity = preferences.getInt(
        "max_humidity", agro_settings.max_humidity
    );
    agro_settings.min_temperature = preferences.getInt(
        "min_temperature", agro_settings.min_temperature
    );
    agro_settings.max_temperature = preferences.getInt(
        "max_temperature", agro_settings.max_temperature
    );
    agro_settings.min_moisture = preferences.getInt(
        "min_moisture", agro_settings.min_moisture
    );
    agro_settings.max_moisture = preferences.getInt(
        "max_moisture", agro_settings.max_moisture
    );
    agro_settings.min_air_co2 = preferences.getInt(
        "min_air_co2", agro_settings.min_air_co2
    );
    agro_settings.max_air_co2 = preferences.getInt(
        "max_air_co2", agro_settings.max_air_co2
    );
    agro_settings.min_light = preferences.getInt(
        "min_light", agro_settings.min_light
    );
    agro_settings.max_light = preferences.getInt(
        "max_light", agro_settings.max_light
    );
    preferences.end();
}

void save_ai_mode()
{
    preferences.begin(PREFERENCES_NAMESPACE, false);
    preferences.putBool("ai_mode", agro_settings.ai_mode);
    preferences.end();
}

void save_sensor_settings()
{
    preferences.begin(PREFERENCES_NAMESPACE, false);
    preferences.putInt("min_humidity", agro_settings.min_humidity);
    preferences.putInt("max_humidity", agro_settings.max_humidity);
    preferences.putInt("min_temperature", agro_settings.min_temperature);
    preferences.putInt("max_temperature", agro_settings.max_temperature);
    preferences.putInt("min_moisture", agro_settings.min_moisture);
    preferences.putInt("max_moisture", agro_settings.max_moisture);
    preferences.putInt("min_air_co2", agro_settings.min_air_co2);
    preferences.putInt("max_air_co2", agro_settings.max_air_co2);
    preferences.putInt("min_light", agro_settings.min_light);
    preferences.putInt("max_light", agro_settings.max_light);
    preferences.end();
}
