#include <mqtt_handler.h>
#include <display_handler.h>
#include <ctype.h>

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
void bulk_settings(const char *topic, const char *payload);
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
String topic_settings;
String topic_ping;
String topic_state;
String topic_log;
String topic_status;
String topic_command_ack;
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

bool valid_topic_id(const String &topic_id)
{
    if (topic_id.isEmpty() || topic_id.length() > 64)
    {
        return false;
    }
    if (topic_id.indexOf('/') >= 0 || topic_id.indexOf('+') >= 0 || topic_id.indexOf('#') >= 0)
    {
        return false;
    }
    for (size_t i = 0; i < topic_id.length(); i++)
    {
        char c = topic_id.charAt(i);
        if (!isalnum(c) && c != '-' && c != '_')
        {
            return false;
        }
    }
    return true;
}

bool valid_bounds(int min_value, int max_value, int lower_limit, int upper_limit)
{
    return min_value >= lower_limit &&
           max_value <= upper_limit &&
           max_value > min_value;
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
    topic_settings = build_topic("/settings");
    topic_ping = build_topic("/ping");
    topic_state = build_topic("/state");
    topic_log = build_topic("/log");
    topic_status = build_topic("/status");
    topic_command_ack = build_topic("/command/ack");
    topic_topic_id_update = build_topic("/system/topic_id");
}
} // namespace

void log_topic_payload(const char *topic, const char *payload)
{
    log_i("Received MQTT topic=%s payload=%s", topic, payload);
}

String payload_command_id(JsonDocument &doc)
{
    if (doc["command_id"].is<const char *>())
    {
        return doc["command_id"].as<const char *>();
    }
    return "";
}

int payload_value(const char *payload, JsonDocument &doc)
{
    DeserializationError error = deserializeJson(doc, payload);
    if (error)
    {
        return atoi(payload);
    }

    if (doc["value"].is<const char *>())
    {
        return atoi(doc["value"].as<const char *>());
    }
    if (doc["value"].is<int>())
    {
        return doc["value"].as<int>();
    }

    return atoi(payload);
}

void set_command_id(Command &command, const String &command_id)
{
    snprintf(command.command_id, sizeof(command.command_id), "%s", command_id.c_str());
}

void publish_command_ack(
    const String &command_id,
    const char *status = "acknowledged",
    const char *message = "applied")
{
    if (command_id.isEmpty())
    {
        return;
    }

    JsonDocument doc;
    doc["command_id"] = command_id;
    doc["status"] = status;
    doc["message"] = message;

    String output;
    serializeJson(doc, output);
    bool ok = mqtt_client.publish(topic_command_ack.c_str(), output.c_str(), 1);
    log_i(
        "Published command ack=%d command_id=%s status=%s",
        ok,
        command_id.c_str(),
        status);
}

bool queue_device_command(
    const char *payload,
    CommandType command_type,
    const char *command_name)
{
    JsonDocument doc;
    String command_id;
    Command command;
    command.type = command_type;
    command.value = payload_value(payload, doc);
    command_id = payload_command_id(doc);
    set_command_id(command, command_id);
    if (xQueueSend(mqttToIo, &command, 0) != pdTRUE)
    {
        log_w("Failed to queue %s command", command_name);
        publish_command_ack(command_id, "failed", "queue full");
        return false;
    }
    return true;
}

void apply_single_bounds_setting(
    const char *payload,
    int &min_value,
    int &max_value,
    const char *setting_name,
    int lower_limit,
    int upper_limit)
{
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error)
    {
        log_i("error parsing JSON");
        return;
    }

    String command_id = payload_command_id(doc);
    int next_max = doc["max"] | max_value;
    int next_min = doc["min"] | min_value;
    if (!valid_bounds(next_min, next_max, lower_limit, upper_limit))
    {
        log_w("Ignoring invalid %s settings min=%d max=%d", setting_name, next_min, next_max);
        publish_command_ack(command_id, "failed", "invalid settings");
        return;
    }

    max_value = next_max;
    min_value = next_min;
    save_sensor_settings();
    log_i("%s settings updated min=%d max=%d", setting_name, min_value, max_value);
    publish_command_ack(command_id);
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
    mqtt_client.subscribe(topic_settings.c_str(), bulk_settings);

    mqtt_client.subscribe(topic_ping.c_str(), ping_mqtt);
    mqtt_client.subscribe(topic_topic_id_update.c_str(), topic_id_update);

    mqtt_client.will.topic = topic_status;
    mqtt_client.will.payload = "offline";
    mqtt_client.will.qos = 1;
    mqtt_client.will.retain = true;

    mqtt_client.connected_callback = []
    {
        mqtt_client.publish(topic_status.c_str(), "online", 1, true);
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
    mqtt_client.publish(topic_log.c_str(), "OK", 1);
}
void ai_mode(const char *topic, const char *payload)
{
    log_topic_payload(topic, payload);
    JsonDocument doc;
    int new_mode = payload_value(payload, doc);
    String command_id = payload_command_id(doc);
    agro_settings.ai_mode = new_mode != 0;
    save_ai_mode();
    log_i("AI mode changed to %d", agro_settings.ai_mode);
    publish_command_ack(command_id);
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
    if (!valid_topic_id(new_topic_id))
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
        1);
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
    queue_device_command(payload, CommandType::CMD_SOIL_WATER_PUMP, "soil water pump");
}
void air_water_pump_control(const char *topic, const char *payload)
{
    log_topic_payload(topic, payload);
    queue_device_command(payload, CommandType::CMD_AIR_WATER_PUMP, "air water pump");
}
void led_control(const char *topic, const char *payload)
{
    log_topic_payload(topic, payload);
    queue_device_command(payload, CommandType::CMD_LED, "LED");
}
void fan_control(const char *topic, const char *payload)
{
    log_topic_payload(topic, payload);
    queue_device_command(payload, CommandType::CMD_FAN, "fan");
}

void temperature_settings(const char *topic, const char *payload)
{
    log_topic_payload(topic, payload);
    apply_single_bounds_setting(
        payload,
        agro_settings.min_temperature,
        agro_settings.max_temperature,
        "temperature",
        -20,
        80);
}
void humidity_settings(const char *topic, const char *payload)
{
    log_topic_payload(topic, payload);
    apply_single_bounds_setting(
        payload,
        agro_settings.min_humidity,
        agro_settings.max_humidity,
        "humidity",
        0,
        100);
}
void water_settings(const char *topic, const char *payload)
{
    log_topic_payload(topic, payload);
    apply_single_bounds_setting(
        payload,
        agro_settings.min_moisture,
        agro_settings.max_moisture,
        "moisture",
        0,
        100);
}
void light_settings(const char *topic, const char *payload)
{
    log_topic_payload(topic, payload);
    apply_single_bounds_setting(
        payload,
        agro_settings.min_light,
        agro_settings.max_light,
        "light",
        0,
        100);
}
void air_settings(const char *topic, const char *payload)
{
    log_topic_payload(topic, payload);
    apply_single_bounds_setting(
        payload,
        agro_settings.min_air_co2,
        agro_settings.max_air_co2,
        "air",
        0,
        10000);
}

bool update_bounds(
    JsonVariantConst setting,
    int &min_value,
    int &max_value,
    const char *setting_name,
    int lower_limit,
    int upper_limit)
{
    if (setting.isNull())
    {
        return false;
    }

    int next_min = setting["min"] | min_value;
    int next_max = setting["max"] | max_value;
    if (!valid_bounds(next_min, next_max, lower_limit, upper_limit))
    {
        log_w(
            "Ignoring invalid %s bulk settings min=%d max=%d",
            setting_name,
            next_min,
            next_max);
        return false;
    }

    min_value = next_min;
    max_value = next_max;
    return true;
}

void bulk_settings(const char *topic, const char *payload)
{
    log_topic_payload(topic, payload);
    JsonDocument doc;

    DeserializationError error = deserializeJson(doc, payload);
    if (error)
    {
        log_i("error parsing JSON");
        return;
    }
    String command_id = payload_command_id(doc);

    bool changed = false;
    changed = update_bounds(
                  doc["temperature"],
                  agro_settings.min_temperature,
                  agro_settings.max_temperature,
                  "temperature",
                  -20,
                  80) ||
              changed;
    changed = update_bounds(
                  doc["humidity"],
                  agro_settings.min_humidity,
                  agro_settings.max_humidity,
                  "humidity",
                  0,
                  100) ||
              changed;
    changed = update_bounds(
                  doc["moisture"],
                  agro_settings.min_moisture,
                  agro_settings.max_moisture,
                  "moisture",
                  0,
                  100) ||
              changed;
    changed = update_bounds(
                  doc["light"],
                  agro_settings.min_light,
                  agro_settings.max_light,
                  "light",
                  0,
                  100) ||
              changed;
    changed = update_bounds(
                  doc["air"],
                  agro_settings.min_air_co2,
                  agro_settings.max_air_co2,
                  "air",
                  0,
                  10000) ||
              changed;

    if (changed)
    {
        save_sensor_settings();
        log_i("Bulk sensor settings updated");
    }
    publish_command_ack(command_id);
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
    mqtt_client.publish(topic_state.c_str(), output, 1, true);
}

void callback_handler()
{
    Callback received_callback;
    if (xQueueReceive(ioToMqtt, &received_callback, 0) == pdTRUE)
    {
        if (received_callback.type == CallbackType::CLB_UPDATE)
        {
            pub_states();
        }
        else if (received_callback.type == CallbackType::CLB_COMMAND_ACK)
        {
            publish_command_ack(
                String(received_callback.command_id),
                received_callback.status,
                received_callback.message);
        }
    }
}
