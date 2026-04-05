#include <mqtt_handler.h>
#include <display_handler.h>

#if MQTT_TLS_ENABLED
namespace
{
WiFiClientSecure mqtt_transport;
}

PicoMQTT::Client mqtt_client(
    mqtt_transport,
    MQTT_BROKER_HOST,
    MQTT_BROKER_PORT,
    MQTT_CLIENT_ID,
    MQTT_USERNAME,
    MQTT_PASSWORD);
#else
PicoMQTT::Client mqtt_client(
    MQTT_BROKER_HOST,
    MQTT_BROKER_PORT,
    MQTT_CLIENT_ID,
    MQTT_USERNAME,
    MQTT_PASSWORD);
#endif

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
void topic_id_update(const char *topic, const char *payload);

void callback_handler();
void log_topic_payload(const char *topic, const char *payload);

namespace
{
String topic_mode_ai;
String topic_soil_water_pump_control;
String topic_air_water_pump_control;
String topic_led_control;
String topic_fan_control;
String topic_temperature_settings;
String topic_humidity_settings;
String topic_moisture_settings;
String topic_light_settings;
String topic_air_settings;
String topic_ping;
String topic_state;
String topic_log;
String topic_topic_id_update;

void configure_mqtt_transport()
{
#if MQTT_TLS_ENABLED
    mqtt_transport.setHandshakeTimeout(MQTT_TLS_HANDSHAKE_TIMEOUT_SEC);
    mqtt_transport.setCACert(MQTT_ROOT_CA);
#endif
}

String build_topic(const char *suffix)
{
    return agro_mqtt_topic_id + suffix;
}

void refresh_topics()
{
    topic_mode_ai = build_topic("/mode/ai");
    topic_soil_water_pump_control = build_topic("/soil_water_pump/control");
    topic_air_water_pump_control = build_topic("/air_water_pump/control");
    topic_led_control = build_topic("/led/control");
    topic_fan_control = build_topic("/fan/control");
    topic_temperature_settings = build_topic("/temperature/settings");
    topic_humidity_settings = build_topic("/humidity/settings");
    topic_moisture_settings = build_topic("/moisture/settings");
    topic_light_settings = build_topic("/light/settings");
    topic_air_settings = build_topic("/air/settings");
    topic_ping = build_topic("/ping");
    topic_state = build_topic("/state");
    topic_log = build_topic("/log");
    topic_topic_id_update = build_topic("/system/topic_id");
}
} // namespace

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
    configure_mqtt_transport();
    log_i(
        "MQTT connect host=%s port=%u tls=%d",
        MQTT_BROKER_HOST,
        static_cast<unsigned>(MQTT_BROKER_PORT),
        MQTT_TLS_ENABLED);
    refresh_topics();
    mqtt_client.subscribe(topic_mode_ai.c_str(), ai_mode);

    mqtt_client.subscribe(topic_soil_water_pump_control.c_str(), soil_water_pump_control);
    mqtt_client.subscribe(topic_air_water_pump_control.c_str(), air_water_pump_control);
    mqtt_client.subscribe(topic_led_control.c_str(), led_control);
    mqtt_client.subscribe(topic_fan_control.c_str(), fan_control);

    mqtt_client.subscribe(topic_temperature_settings.c_str(), temperature_settings);
    mqtt_client.subscribe(topic_humidity_settings.c_str(), humidity_settings);
    mqtt_client.subscribe(topic_moisture_settings.c_str(), water_settings);
    mqtt_client.subscribe(topic_light_settings.c_str(), light_settings);
    mqtt_client.subscribe(topic_air_settings.c_str(), air_settings);

    mqtt_client.subscribe(topic_ping.c_str(), ping_mqtt);
    mqtt_client.subscribe(topic_topic_id_update.c_str(), topic_id_update);

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
    mqtt_client.publish(topic_log.c_str(), "OK");
}
void ai_mode(const char *topic, const char *payload)
{
    log_topic_payload(topic, payload);
    int new_mode = atoi(payload);
    agro_settings.ai_mode = new_mode != 0;
    save_ai_mode();
    log_i("AI mode changed to %d", agro_settings.ai_mode);
}
void topic_id_update(const char *topic, const char *payload)
{
    log_topic_payload(topic, payload);
    String new_topic_id;
    String update_token;
    String current_topic_id = agro_mqtt_topic_id;
    JsonDocument update_doc;
    DeserializationError update_error = deserializeJson(update_doc, payload);
    if (!update_error)
    {
        if (update_doc["topic_id"].is<const char *>())
        {
            new_topic_id = update_doc["topic_id"].as<const char *>();
        }
        if (update_doc["token"].is<const char *>())
        {
            update_token = update_doc["token"].as<const char *>();
        }
    }
    else
    {
        new_topic_id = payload;
    }
    new_topic_id.trim();
    update_token.trim();
    if (new_topic_id.isEmpty())
    {
        log_w("Ignoring empty MQTT topic id update");
        return;
    }
    if (new_topic_id.indexOf('/') >= 0 || new_topic_id.indexOf('+') >= 0 || new_topic_id.indexOf('#') >= 0)
    {
        log_w("Ignoring invalid MQTT topic id update=%s", new_topic_id.c_str());
        return;
    }
    if (new_topic_id == agro_mqtt_topic_id)
    {
        log_i("MQTT topic id already set to %s", new_topic_id.c_str());
        return;
    }

    agro_mqtt_topic_id = new_topic_id;
    save_mqtt_topic_id();

    String ack_topic = new_topic_id + "/system/topic_id/ack";
    String ack_payload;
    if (!update_token.isEmpty())
    {
        JsonDocument ack_doc;
        ack_doc["topic_id"] = new_topic_id;
        ack_doc["token"] = update_token;
        serializeJson(ack_doc, ack_payload);
    }
    else
    {
        ack_payload = new_topic_id;
    }
    bool ack_published = mqtt_client.publish(
        ack_topic.c_str(),
        ack_payload.c_str(),
        true);
    log_i(
        "Published topic id ack=%d on %s with payload=%s",
        ack_published,
        ack_topic.c_str(),
        ack_payload.c_str());

    if (!ack_published)
    {
        log_w("Topic id update aborted because ack publish failed");
        agro_mqtt_topic_id = current_topic_id;
        save_mqtt_topic_id();
        return;
    }

    log_i("Persisted new MQTT topic id=%s. Restarting to resubscribe.", agro_mqtt_topic_id.c_str());
    delay(250);
    delay(500);
    ESP.restart();
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

    if (agro_state.air_co2_ready)
    {
        doc[F("air")] = agro_state.air_co2;
    }
    else
    {
        doc[F("air")] = nullptr;
    }
    doc[F("light")] = agro_state.light;
    if (agro_state.humidity_ready)
    {
        doc[F("humidity")] = agro_state.humidity;
    }
    else
    {
        doc[F("humidity")] = nullptr;
    }
    if (agro_state.temperature_ready)
    {
        doc[F("temperature")] = agro_state.temperature;
    }
    else
    {
        doc[F("temperature")] = nullptr;
    }
    doc[F("moisture")] = agro_state.soil_moisture;
    doc[F("fan")] = agro_state.fan;
    doc[F("led")] = agro_state.led;
    doc[F("soil_water_pump")] = agro_state.soil_water_pump;
    doc[F("air_water_pump")] = agro_state.air_water_pump;
    doc[F("ai_mode")] = agro_settings.ai_mode;

    String output;

    doc.shrinkToFit(); // optional

    serializeJson(doc, output);
    mqtt_client.publish(topic_state.c_str(), output);
}
void pub_fan_state()
{
    mqtt_client.publish(topic_state.c_str(), "fan:" + String(agro_state.fan));
}
void pub_led_state()
{
    mqtt_client.publish(topic_state.c_str(), "led:" + String(agro_state.led));
}
void pub_soil_water_pump_state()
{
    mqtt_client.publish(topic_state.c_str(), "soil_water_pump:" + String(agro_state.soil_water_pump));
}
void pub_air_water_pump_state()
{
    mqtt_client.publish(topic_state.c_str(), "air_water_pump:" + String(agro_state.air_water_pump));
}

void pub_air_state()
{
    mqtt_client.publish(topic_state.c_str(), "air:" + String(agro_state.air_co2));
}
void pub_light_state()
{
    mqtt_client.publish(topic_state.c_str(), "light:" + String(agro_state.light));
}
void pub_humidity_state()
{
    mqtt_client.publish(topic_state.c_str(), "humidity:" + String(agro_state.humidity));
}
void pub_temperature_state()
{
    mqtt_client.publish(topic_state.c_str(), "temperature:" + String(agro_state.temperature));
}
void pub_moisture_state()
{
    mqtt_client.publish(topic_state.c_str(), "moisture:" + String(agro_state.soil_moisture));
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
