#include <mqtt_handler.h>
#include <display_handler.h>

PicoMQTT::Client mqtt_client(MQTT_IP);

void soil_water_pump_control(const char *topic, const char *payload);
void air_water_pump_control(const char *topic, const char *payload);
void led_control(const char *topic, const char *payload);
void fan_control(const char *topic, const char *payload);
void temperature_settings(const char *topic, const char *payload);
void humidity_settings(const char *topic, const char *payload);
void water_settings(const char *topic, const char *payload);
void light_settings(const char *topic, const char *payload);
void air_settings(const char *topic, const char *payload);
void ping_mqtt(const char *topic, const char *payload);
void ai_mode(const char *topic, const char *payload);

void callback_handler();
void log_topic_payload(const char *topic, const char *payload);

void log_topic_payload(const char *topic, const char *payload)
{
    log_i("Received MQTT topic=%s payload=%s", topic, payload);
}

void mqtt_setup();
void mqtt_loop();
void mqtt_main(void *p)
{
    mqtt_setup();
    for (;;)
    {
        mqtt_loop();
        vTaskDelay(1);
    }
}
void mqtt_setup()
{
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Connecting to");
    lcd.setCursor(0, 1);
    lcd.print("MQTT");
    delay(500);
    mqtt_client.subscribe(MQTT_ID "/mode/ai", ai_mode);

    mqtt_client.subscribe(MQTT_ID "/soil_water_pump/control", soil_water_pump_control);
    mqtt_client.subscribe(MQTT_ID "/air_water_pump/control", air_water_pump_control);
    mqtt_client.subscribe(MQTT_ID "/led/control", led_control);
    mqtt_client.subscribe(MQTT_ID "/fan/control", fan_control);

    mqtt_client.subscribe(MQTT_ID "/temperature/settings", temperature_settings);
    mqtt_client.subscribe(MQTT_ID "/humidity/settings", humidity_settings);
    mqtt_client.subscribe(MQTT_ID "/moisture/settings", water_settings);
    mqtt_client.subscribe(MQTT_ID "/light/settings", light_settings);
    mqtt_client.subscribe(MQTT_ID "/air/settings", air_settings);

    mqtt_client.subscribe(MQTT_ID "/ping", ping_mqtt);

    mqtt_client.connected_callback = []
    {
        lcd.clear();
        lcd.setCursor(6, 0);
        lcd.print("MQTT");
        lcd.setCursor(0, 1);
        lcd.print("Connected");
        log_i("Connected to mqtt");
    };
    mqtt_client.begin();
}
void mqtt_loop()
{
    mqtt_client.loop();
    callback_handler();
}
void ping_mqtt(const char *topic, const char *payload)
{
    log_topic_payload(topic, payload);
    mqtt_client.publish(MQTT_ID "/log", "OK");
}
void ai_mode(const char *topic, const char *payload)
{
    log_topic_payload(topic, payload);
    int new_mode = atoi(payload);
    agro_settings.ai_mode = new_mode != 0;
    save_ai_mode();
    log_i("AI mode changed to %d", agro_settings.ai_mode);
}
void soil_water_pump_control(const char *topic, const char *payload)
{
    log_topic_payload(topic, payload);
    Command command;
    command.type = CommandType::CMD_SOIL_WATER_PUMP;
    command.value = atoi(payload);
    if (xQueueSend(mqttToIo, &command, 0) != pdTRUE)
    {
        log_w("Failed to queue soil water pump command");
    }
}
void air_water_pump_control(const char *topic, const char *payload)
{
    log_topic_payload(topic, payload);
    Command command;
    command.type = CommandType::CMD_AIR_WATER_PUMP;
    command.value = atoi(payload);
    if (xQueueSend(mqttToIo, &command, 0) != pdTRUE)
    {
        log_w("Failed to queue air water pump command");
    }
}
void led_control(const char *topic, const char *payload)
{
    log_topic_payload(topic, payload);
    Command command;
    command.type = CommandType::CMD_LED;
    command.value = atoi(payload);
    if (xQueueSend(mqttToIo, &command, 0) != pdTRUE)
    {
        log_w("Failed to queue LED command");
    }
}
void fan_control(const char *topic, const char *payload)
{
    log_topic_payload(topic, payload);
    Command command;
    command.type = CommandType::CMD_FAN;
    command.value = atoi(payload);
    if (xQueueSend(mqttToIo, &command, 0) != pdTRUE)
    {
        log_w("Failed to queue fan command");
    }
}

void temperature_settings(const char *topic, const char *payload)
{
    log_topic_payload(topic, payload);
    JsonDocument doc;

    DeserializationError error = deserializeJson(doc, payload);
    if (error)
    {
        log_i("error parsing JSON");
        return;
    }
    agro_settings.max_temperature = doc["max"];
    agro_settings.min_temperature = doc["min"];
    save_sensor_settings();
    log_i(
        "Temperature settings updated min=%d max=%d",
        agro_settings.min_temperature,
        agro_settings.max_temperature);
}
void humidity_settings(const char *topic, const char *payload)
{
    log_topic_payload(topic, payload);
    JsonDocument doc;

    DeserializationError error = deserializeJson(doc, payload);
    if (error)
    {
        log_i("error parsing JSON");
        return;
    }
    agro_settings.max_humidity = doc["max"];
    agro_settings.min_humidity = doc["min"];
    save_sensor_settings();
    log_i(
        "Humidity settings updated min=%d max=%d",
        agro_settings.min_humidity,
        agro_settings.max_humidity);
}
void water_settings(const char *topic, const char *payload)
{
    log_topic_payload(topic, payload);
    JsonDocument doc;

    DeserializationError error = deserializeJson(doc, payload);
    if (error)
    {
        log_i("error parsing JSON");
        return;
    }
    agro_settings.max_moisture = doc["max"];
    agro_settings.min_moisture = doc["min"];
    save_sensor_settings();
    log_i(
        "Moisture settings updated min=%d max=%d",
        agro_settings.min_moisture,
        agro_settings.max_moisture);
}
void light_settings(const char *topic, const char *payload)
{
    log_topic_payload(topic, payload);
    JsonDocument doc;

    DeserializationError error = deserializeJson(doc, payload);
    if (error)
    {
        log_i("error parsing JSON");
        return;
    }
    agro_settings.max_light = doc["max"];
    agro_settings.min_light = doc["min"];
    save_sensor_settings();
    log_i(
        "Light settings updated min=%d max=%d",
        agro_settings.min_light,
        agro_settings.max_light);
}
void air_settings(const char *topic, const char *payload)
{
    log_topic_payload(topic, payload);
    JsonDocument doc;

    DeserializationError error = deserializeJson(doc, payload);
    if (error)
    {
        log_i("error parsing JSON");
        return;
    }
    agro_settings.max_air_co2 = doc["max"];
    agro_settings.min_air_co2 = doc["min"];
    save_sensor_settings();
    log_i(
        "Air settings updated min=%d max=%d",
        agro_settings.min_air_co2,
        agro_settings.max_air_co2);
}

void pub_states()
{
    JsonDocument doc;

    doc[F("air")] = agro_state.air_co2;
    doc[F("light")] = agro_state.light;
    doc[F("humidity")] = agro_state.humidity;
    doc[F("temperature")] = agro_state.temperature;
    doc[F("moisture")] = agro_state.soil_moisture;
    doc[F("fan")] = agro_state.fan;
    doc[F("led")] = agro_state.led;
    doc[F("soil_water_pump")] = agro_state.soil_water_pump;
    doc[F("air_water_pump")] = agro_state.air_water_pump;
    doc[F("ai_mode")] = agro_settings.ai_mode;

    String output;

    doc.shrinkToFit(); // optional

    serializeJson(doc, output);
    mqtt_client.publish(MQTT_ID "/state", output);
}
void pub_fan_state()
{
    mqtt_client.publish(MQTT_ID "/state", "fan:" + String(agro_state.fan));
}
void pub_led_state()
{
    mqtt_client.publish(MQTT_ID "/state", "led:" + String(agro_state.led));
}
void pub_soil_water_pump_state()
{
    mqtt_client.publish(MQTT_ID "/state", "soil_water_pump:" + String(agro_state.soil_water_pump));
}
void pub_air_water_pump_state()
{
    mqtt_client.publish(MQTT_ID "/state", "air_water_pump:" + String(agro_state.air_water_pump));
}

void pub_air_state()
{
    mqtt_client.publish(MQTT_ID "/state", "air:" + String(agro_state.air_co2));
}
void pub_light_state()
{
    mqtt_client.publish(MQTT_ID "/state", "light:" + String(agro_state.light));
}
void pub_humidity_state()
{
    mqtt_client.publish(MQTT_ID "/state", "humidity:" + String(agro_state.humidity));
}
void pub_temperature_state()
{
    mqtt_client.publish(MQTT_ID "/state", "temperature:" + String(agro_state.temperature));
}
void pub_moisture_state()
{
    mqtt_client.publish(MQTT_ID "/state", "moisture:" + String(agro_state.soil_moisture));
}

void callback_handler()
{
    Callback recieved_callback;
    if (xQueueReceive(ioToMqtt, &recieved_callback, 0) == pdTRUE)
    {
        if (recieved_callback.type == CallbackType::CLB_UPDATE)
        {
            pub_states();
        }
    }
}
