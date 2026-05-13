#include <io_handler.h>
#include <display_handler.h>
#include <math.h>

DHT dht(DHTPIN, DHTTYPE);
#if !LED_DRIVER_RELAY
CRGB leds[NUMLEDS];
#endif

void io_setup();
void io_loop();
void turn_led(bool newState);
void turn_fan(bool newState);
void turn_soil_water_pump(bool newState);
void turn_air_water_pump(bool newState);
void command_handler();
void ai_handler();
void update_sensors();
void read_air_sensor();
void update_display_page();
void enqueue_state_update();
void enqueue_command_ack(const Command &command, const char *status, const char *message);
void check_humidity();
void check_fan();
void enforce_pump_runtime_limits();
bool fan_has_co2_reading();
bool analog_reading_ready(int raw);
void set_relay_output(
    uint8_t pin,
    bool active_low,
    bool state,
    bool off_uses_input_mode);
const char *command_type_name(CommandType type);

const char *command_type_name(CommandType type)
{
    switch (type)
    {
    case CommandType::CMD_LED:
        return "led";
    case CommandType::CMD_SOIL_WATER_PUMP:
        return "soil_water_pump";
    case CommandType::CMD_AIR_WATER_PUMP:
        return "air_water_pump";
    case CommandType::CMD_FAN:
        return "fan";
    }
    return "unknown";
}

bool analog_reading_ready(int raw)
{
    return raw >= ANALOG_SENSOR_MIN_VALID_RAW && raw <= ANALOG_SENSOR_MAX_VALID_RAW;
}

void set_relay_output(
    uint8_t pin,
    bool active_low,
    bool state,
    bool off_uses_input_mode)
{
    if (state)
    {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, active_low ? LOW : HIGH);
        return;
    }

    if (active_low && off_uses_input_mode)
    {
        // Some 5V low-level relay boards do not fully turn off from a 3.3V HIGH.
        // Releasing the pin to high impedance lets the module's pull-up return it to idle.
        digitalWrite(pin, HIGH);
        pinMode(pin, INPUT);
        return;
    }

    pinMode(pin, OUTPUT);
    digitalWrite(pin, active_low ? HIGH : LOW);
}

void io_setup()
{
    set_relay_output(
        FAN_PIN,
        FAN_ACTIVE_LOW,
        false,
        FAN_OFF_USES_INPUT_MODE);
    set_relay_output(
        SOIL_WATER_PUMP,
        SOIL_WATER_PUMP_ACTIVE_LOW,
        false,
        SOIL_WATER_PUMP_OFF_USES_INPUT_MODE);
    set_relay_output(
        AIR_WATER_PUMP,
        AIR_WATER_PUMP_ACTIVE_LOW,
        false,
        AIR_WATER_PUMP_OFF_USES_INPUT_MODE);
    set_relay_output(
        LED_RELAY_PIN,
        LED_RELAY_ACTIVE_LOW,
        false,
        LED_RELAY_OFF_USES_INPUT_MODE);
    log_i(
        "Relay polarity fan=%d soil=%d air=%d led=%d",
        FAN_ACTIVE_LOW,
        SOIL_WATER_PUMP_ACTIVE_LOW,
        AIR_WATER_PUMP_ACTIVE_LOW,
        LED_RELAY_ACTIVE_LOW);
    log_i(
        "Output pins fan=%d soil=%d air=%d led=%d strip=%d",
        FAN_PIN,
        SOIL_WATER_PUMP,
        AIR_WATER_PUMP,
        LED_RELAY_PIN,
        STRIP_PIN);
    log_i(
        "Relay off mode fan=%s soil=%s air=%s led=%s",
        FAN_OFF_USES_INPUT_MODE ? "input" : "drive",
        SOIL_WATER_PUMP_OFF_USES_INPUT_MODE ? "input" : "drive",
        AIR_WATER_PUMP_OFF_USES_INPUT_MODE ? "input" : "drive",
        LED_RELAY_OFF_USES_INPUT_MODE ? "input" : "drive");

#if MQ135_ENABLED
    pinMode(MQ135_PIN, INPUT);
#if defined(ARDUINO_ARCH_ESP32)
    analogSetPinAttenuation(MQ135_PIN, ADC_11db);
#endif
    log_i("MQ135 air sensor enabled on analog pin=%d", MQ135_PIN);
#else
    agro_state.air_co2_ready = false;
    log_i("MQ135 air sensor is disabled in io_handler.h");
#endif

#if defined(ARDUINO_ARCH_ESP32)
    analogSetPinAttenuation(MOISTURE_SENSOR, ADC_11db);
#if !LIGHT_SENSOR_DIGITAL
    analogSetPinAttenuation(LIGHT_SENSOR, ADC_11db);
#endif
#endif
#if LIGHT_SENSOR_DIGITAL
    pinMode(LIGHT_SENSOR, LIGHT_SENSOR_DIGITAL_INPUT_MODE);
    log_i(
        "Sensor pins moisture=%d light=%d mode=digital light_level=%d",
        MOISTURE_SENSOR,
        LIGHT_SENSOR,
        LIGHT_SENSOR_DIGITAL_LIGHT_LEVEL);
#else
    log_i("Analog sensor pins moisture=%d light=%d", MOISTURE_SENSOR, LIGHT_SENSOR);
#endif

#if LED_DRIVER_RELAY
    log_i("LED driver relay enabled on pin=%d", LED_RELAY_PIN);
#else
    FastLED.addLeds<WS2811, STRIP_PIN, GRB>(leds, NUMLEDS);
    FastLED.clear();
    FastLED.show();
    log_i("FastLED strip enabled on pin=%d leds=%d", STRIP_PIN, NUMLEDS);
#endif

#if DHT_ENABLED
    dht.begin();
    log_i("DHT enabled on pin=%d type=%d", DHTPIN, DHTTYPE);
#else
    log_i("DHT sensor is disabled in io_handler.h");
#endif
    log_i("IO setup complete. Commands and state changes will be printed to serial.");
}
Command received_command;
uint32_t sensors_tmr;
uint32_t display_update;
uint32_t ai_check;
uint32_t soil_water_pump_started_at;
uint32_t air_water_pump_started_at;
void io_loop()
{
    command_handler();
    ai_handler();
    enforce_pump_runtime_limits();
    if (millis() - sensors_tmr >= SENSOR_PUBLISH_INTERVAL_MS)
    {
        sensors_tmr = millis();
        update_sensors();
    }
    if (millis() - display_update >= DISPLAY_REFRESH_INTERVAL_MS)
    {
        display_update = millis();
        update_display_page();
    }
}
void command_handler()
{
    for (uint8_t processed = 0; processed < IO_COMMANDS_PER_LOOP; processed++)
    {
        if (xQueueReceive(mqttToIo, &received_command, 0) != pdTRUE)
        {
            return;
        }

        log_i(
            "Applying command %s with value=%d",
            command_type_name(received_command.type),
            received_command.value);
        if (agro_settings.ai_mode)
        {
            log_w("Manual command ignored because AI mode is enabled");
            enqueue_command_ack(received_command, "failed", "ai mode enabled");
            continue;
        }
        switch (received_command.type)
        {
        case CommandType::CMD_AIR_WATER_PUMP:
        {
            turn_air_water_pump(received_command.value != 0);

            break;
        }
        case CommandType::CMD_FAN:
        {
            turn_fan(received_command.value != 0);

            break;
        }
        case CommandType::CMD_SOIL_WATER_PUMP:
        {
            turn_soil_water_pump(received_command.value != 0);

            break;
        }
        case CommandType::CMD_LED:
        {
            turn_led(received_command.value != 0);
            break;
        }
        }
        enqueue_command_ack(received_command, "acknowledged", "applied");
    }
}

void enqueue_command_ack(const Command &command, const char *status, const char *message)
{
    if (command.command_id[0] == '\0')
    {
        return;
    }

    Callback callback;
    callback.type = CallbackType::CLB_COMMAND_ACK;
    snprintf(callback.command_id, sizeof(callback.command_id), "%s", command.command_id);
    snprintf(callback.status, sizeof(callback.status), "%s", status);
    snprintf(callback.message, sizeof(callback.message), "%s", message);

    if (xQueueSend(ioToMqtt, &callback, 0) != pdTRUE)
    {
        log_w("Command ack queue is full for command_id=%s", command.command_id);
    }
}

void enqueue_state_update()
{
    if (state_update_queued)
    {
        return;
    }

    Callback callback;
    callback.type = CallbackType::CLB_UPDATE;
    if (xQueueSend(ioToMqtt, &callback, 0) != pdTRUE)
    {
        log_w("State update queue is full");
        return;
    }

    state_update_queued = true;
}

void update_sensors()
{
#if DHT_ENABLED
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    bool humidity_ok = !isnan(h);
    bool temperature_ok = !isnan(t);

    if (temperature_ok)
    {
        agro_state.temperature = (int)t;
        agro_state.temperature_ready = true;
    }
    else
    {
        agro_state.temperature_ready = false;
        log_w("Failed to read temperature from DHT");
    }

    if (humidity_ok)
    {
        agro_state.humidity = (int)h;
        agro_state.humidity_ready = true;
    }
    else
    {
        agro_state.humidity_ready = false;
        log_w("Failed to read humidity from DHT");
    }
#else
    bool humidity_ok = false;
    bool temperature_ok = false;
    agro_state.humidity_ready = false;
    agro_state.temperature_ready = false;
#endif

    read_air_sensor();

    int moisture_raw = analogRead(MOISTURE_SENSOR);
    agro_state.soil_moisture_ready = analog_reading_ready(moisture_raw);
    if (agro_state.soil_moisture_ready)
    {
        // Typical capacitive moisture sensors output high values when dry and low values when wet.
        int moisture = constrain(map(moisture_raw, 4096, 0, 0, 100), 0, 100);
        agro_state.soil_moisture = moisture;
    }
    else
    {
        log_w("Moisture sensor raw=%d outside valid range", moisture_raw);
    }

#if LIGHT_SENSOR_DIGITAL
    int light_level = digitalRead(LIGHT_SENSOR);
    agro_state.light_ready = true;
    agro_state.light = light_level == LIGHT_SENSOR_DIGITAL_LIGHT_LEVEL ? 100 : 0;
    log_i(
        "Moisture raw=%d pct=%d | Light level=%d pct=%d",
        moisture_raw,
        agro_state.soil_moisture,
        light_level,
        agro_state.light);
#else
    int light_raw = analogRead(LIGHT_SENSOR);
    agro_state.light_ready = analog_reading_ready(light_raw);
    if (agro_state.light_ready)
    {
        agro_state.light = constrain(map(light_raw, 0, 4096, 0, 100), 0, 100);
    }
    else
    {
        log_w("Light sensor raw=%d outside valid range", light_raw);
    }
    log_i(
        "Moisture raw=%d pct=%d | Light raw=%d pct=%d",
        moisture_raw,
        agro_state.soil_moisture,
        light_raw,
        agro_state.light);
#endif
    agro_state.sensors_ready =
        agro_state.humidity_ready && agro_state.temperature_ready;
    if (DHT_ENABLED && !agro_state.sensors_ready)
    {
        log_w("Humidity/temperature automations paused until DHT readings recover");
    }
    enqueue_state_update();
}
void update_display_page()
{
    char line[32];

    lcd.clear();
    lcd.setTextColor(ST77XX_CYAN);
    lcd.setCursor(0, 0);
    lcd.print("AgroAi");

    // WiFi/MQTT status indikatori
    lcd.setCursor(8, 0);
    if (wifi_connected && mqtt_connected)
    {
        lcd.setTextColor(ST77XX_GREEN);
        lcd.print("ON");
    }
    else if (wifi_connected)
    {
        lcd.setTextColor(ST77XX_YELLOW);
        lcd.print("WF");
    }
    else
    {
        lcd.setTextColor(ST77XX_RED);
        lcd.print("OF");
    }

    lcd.setTextColor(agro_settings.ai_mode ? ST77XX_GREEN : ST77XX_YELLOW);
    lcd.setCursor(0, 1);
    lcd.print(agro_settings.ai_mode ? "Mode:AI" : "Mode:MAN");

    lcd.setTextColor(ST77XX_WHITE);
    char moisture_text[8];
    char light_text[8];
    if (agro_state.soil_moisture_ready)
    {
        snprintf(moisture_text, sizeof(moisture_text), "%d%%", agro_state.soil_moisture);
    }
    else
    {
        snprintf(moisture_text, sizeof(moisture_text), "-");
    }
    if (agro_state.light_ready)
    {
        snprintf(light_text, sizeof(light_text), "%d%%", agro_state.light);
    }
    else
    {
        snprintf(light_text, sizeof(light_text), "-");
    }
#if DHT_ENABLED
    char temperature_text[8];
    char humidity_text[8];
    if (agro_state.temperature_ready)
    {
        snprintf(temperature_text, sizeof(temperature_text), "%d", agro_state.temperature);
    }
    else
    {
        snprintf(temperature_text, sizeof(temperature_text), "-");
    }
    if (agro_state.humidity_ready)
    {
        snprintf(humidity_text, sizeof(humidity_text), "%d", agro_state.humidity);
    }
    else
    {
        snprintf(humidity_text, sizeof(humidity_text), "-");
    }
    snprintf(
        line,
        sizeof(line),
        "T:%s H:%s",
        temperature_text,
        humidity_text);
#else
    snprintf(line, sizeof(line), "T:- H:-");
#endif
    lcd.setCursor(0, 2);
    lcd.print(line);

    snprintf(line, sizeof(line), "M:%s L:%s", moisture_text, light_text);
    lcd.setCursor(0, 3);
    lcd.print(line);

#if MQ135_ENABLED
    if (agro_state.air_co2_ready)
    {
        snprintf(line, sizeof(line), "Air:%dppm", agro_state.air_co2);
    }
    else
    {
        snprintf(line, sizeof(line), "Air:-");
    }
#else
    snprintf(line, sizeof(line), "Air:-");
#endif
    lcd.setCursor(0, 4);
    lcd.print(line);

    lcd.setTextColor(ST77XX_MAGENTA);
    snprintf(
        line,
        sizeof(line),
        "F%d S%d A%d L%d",
        agro_state.fan ? 1 : 0,
        agro_state.soil_water_pump ? 1 : 0,
        agro_state.air_water_pump ? 1 : 0,
        agro_state.led ? 1 : 0);
    lcd.setCursor(0, 5);
    lcd.print(line);

    lcd.setTextColor(ST77XX_WHITE);
}
void read_air_sensor()
{
#if MQ135_ENABLED
    int raw = analogRead(MQ135_PIN);
    int normalized_raw = constrain(
        raw,
        MQ135_CLEAN_AIR_RAW,
        MQ135_POLLUTED_AIR_RAW);
    agro_state.air_co2 = map(
        normalized_raw,
        MQ135_CLEAN_AIR_RAW,
        MQ135_POLLUTED_AIR_RAW,
        MQ135_MIN_PPM,
        MQ135_MAX_PPM);
    agro_state.air_co2_ready = true;
    log_i("MQ135 raw=%d air_estimate=%d", raw, agro_state.air_co2);
#else
    agro_state.air_co2_ready = false;
#endif
}
bool fan_has_co2_reading()
{
#if MQ135_ENABLED
    return agro_state.air_co2_ready && agro_state.air_co2 > 0;
#else
    return false;
#endif
}
void turn_led(bool newState)
{
#if LED_DRIVER_RELAY
    set_relay_output(
        LED_RELAY_PIN,
        LED_RELAY_ACTIVE_LOW,
        newState,
        LED_RELAY_OFF_USES_INPUT_MODE);
#else
    fill_solid(leds, NUMLEDS, CHSV(0, 0, newState ? 255 : 0));
    FastLED.show();
#endif
    agro_state.led = newState;
    log_i(
        "LED state changed to %d (relay=%d pin=%d level=%d off_mode=%s)",
        agro_state.led,
        LED_DRIVER_RELAY,
        LED_DRIVER_RELAY ? LED_RELAY_PIN : STRIP_PIN,
        newState ? LED_RELAY_ON_LEVEL : LED_RELAY_OFF_LEVEL,
        LED_RELAY_OFF_USES_INPUT_MODE ? "input" : "drive");
    enqueue_state_update();
}
void turn_fan(bool newState)
{
    set_relay_output(
        FAN_PIN,
        FAN_ACTIVE_LOW,
        newState,
        FAN_OFF_USES_INPUT_MODE);
    agro_state.fan = newState;
    log_i(
        "Fan state changed to %d (pin=%d active_low=%d off_mode=%s)",
        agro_state.fan,
        newState ? FAN_ON_LEVEL : FAN_OFF_LEVEL,
        FAN_ACTIVE_LOW,
        FAN_OFF_USES_INPUT_MODE ? "input" : "drive");
    enqueue_state_update();
}
void turn_soil_water_pump(bool newState)
{
    bool old_state = agro_state.soil_water_pump;
    set_relay_output(
        SOIL_WATER_PUMP,
        SOIL_WATER_PUMP_ACTIVE_LOW,
        newState,
        SOIL_WATER_PUMP_OFF_USES_INPUT_MODE);
    agro_state.soil_water_pump = newState;
    if (newState && !old_state)
    {
        soil_water_pump_started_at = millis();
    }
    else if (!newState)
    {
        soil_water_pump_started_at = 0;
    }
    log_i(
        "Soil water pump state changed to %d (pin=%d active_low=%d off_mode=%s)",
        agro_state.soil_water_pump,
        newState ? SOIL_WATER_PUMP_ON_LEVEL : SOIL_WATER_PUMP_OFF_LEVEL,
        SOIL_WATER_PUMP_ACTIVE_LOW,
        SOIL_WATER_PUMP_OFF_USES_INPUT_MODE ? "input" : "drive");
    enqueue_state_update();
}
void turn_air_water_pump(bool newState)
{
    bool old_state = agro_state.air_water_pump;
    set_relay_output(
        AIR_WATER_PUMP,
        AIR_WATER_PUMP_ACTIVE_LOW,
        newState,
        AIR_WATER_PUMP_OFF_USES_INPUT_MODE);
    agro_state.air_water_pump = newState;
    if (newState && !old_state)
    {
        air_water_pump_started_at = millis();
    }
    else if (!newState)
    {
        air_water_pump_started_at = 0;
    }
    log_i(
        "Air water pump state changed to %d (pin=%d active_low=%d off_mode=%s)",
        agro_state.air_water_pump,
        newState ? AIR_WATER_PUMP_ON_LEVEL : AIR_WATER_PUMP_OFF_LEVEL,
        AIR_WATER_PUMP_ACTIVE_LOW,
        AIR_WATER_PUMP_OFF_USES_INPUT_MODE ? "input" : "drive");
    enqueue_state_update();
}
void enforce_pump_runtime_limits()
{
    if (
        agro_state.soil_water_pump &&
        soil_water_pump_started_at != 0 &&
        millis() - soil_water_pump_started_at > WATER_PUMP_MAX_RUNTIME_MS)
    {
        log_w("Soil water pump runtime exceeded %lu ms, emergency stopping", WATER_PUMP_MAX_RUNTIME_MS);
        turn_soil_water_pump(false);
    }

    if (
        agro_state.air_water_pump &&
        air_water_pump_started_at != 0 &&
        millis() - air_water_pump_started_at > WATER_PUMP_MAX_RUNTIME_MS)
    {
        log_w("Air water pump runtime exceeded %lu ms, emergency stopping", WATER_PUMP_MAX_RUNTIME_MS);
        turn_air_water_pump(false);
    }
}
void check_moisture()
{
    if (!agro_state.soil_water_pump && agro_state.soil_moisture < agro_settings.min_moisture)
    {
        turn_soil_water_pump(true);
    }
    else if (agro_state.soil_water_pump && agro_state.soil_moisture > agro_settings.max_moisture)
    {
        turn_soil_water_pump(false);
    }
}
void check_light()
{
    if (!agro_state.led && agro_state.light < agro_settings.min_light)
    {
        turn_led(true);
    }
    else if (agro_state.led && agro_state.light > agro_settings.max_light)
    {
        turn_led(false);
    }
}
void check_humidity()
{
    if (!agro_state.humidity_ready)
    {
        return;
    }
    if (!agro_state.air_water_pump && agro_state.humidity < agro_settings.min_humidity)
    {
        turn_air_water_pump(true);
    }
    else if (agro_state.air_water_pump && agro_state.humidity > agro_settings.max_humidity)
    {
        turn_air_water_pump(false);
    }
}
void check_fan()
{
    bool temperature_ready = agro_state.temperature_ready;
    bool co2_ready = fan_has_co2_reading();
    if (!temperature_ready && !co2_ready)
    {
        return;
    }

    bool should_turn_on = false;
    bool should_turn_off = false;

    if (temperature_ready)
    {
        if (agro_state.temperature > agro_settings.max_temperature) should_turn_on = true;
        if (agro_state.temperature < agro_settings.min_temperature) should_turn_off = true;
    }

    if (co2_ready)
    {
        if (agro_state.air_co2 > agro_settings.max_air_co2) should_turn_on = true;
        if (agro_state.air_co2 < agro_settings.min_air_co2) should_turn_off = true;
    }

    // Resolve conflicts (e.g., Temp < min but CO2 > max)
    if (should_turn_on && should_turn_off)
    {
        if (temperature_ready && agro_state.temperature < agro_settings.min_temperature)
        {
            // Freeze protection: temperature is too low, so DO NOT turn on the fan,
            // even if CO2 is high.
            should_turn_on = false;
        }
        else
        {
            // Otherwise, we prioritize cooling / exhausting over turning off
            should_turn_off = false;
        }
    }

    if (!agro_state.fan && should_turn_on)
    {
        turn_fan(true);
    }
    else if (agro_state.fan && should_turn_off)
    {
        turn_fan(false);
    }
}
void ai_handler()
{
    if (!agro_settings.ai_mode)
    {
        return;
    }

    if (millis() - ai_check >= AI_CHECK_INTERVAL_MS)
    {
        ai_check = millis();
        
        // Moiture check (always has reading from analogRead, but we could add range check)
        check_moisture();
        
        // Light check
        check_light();
        
        // Fail-safe for humidity
        if (agro_state.humidity_ready)
        {
            check_humidity();
        }
        else if (agro_state.air_water_pump)
        {
            log_w("Humidity sensor not ready, emergency stopping air water pump");
            turn_air_water_pump(false);
        }

        // Fail-safe for fan (temperature or CO2)
        if (agro_state.temperature_ready || fan_has_co2_reading())
        {
            check_fan();
        }
        else if (agro_state.fan)
        {
            log_w("Temperature and CO2 sensors not ready, emergency stopping fan");
            turn_fan(false);
        }
    }
}
