#include <io_handler.h>
#include <display_handler.h>
#include <math.h>

DHT dht(DHTPIN, DHTTYPE);
CRGB leds[NUMLEDS];
#define RXD2 16
#define TXD2 17
HardwareSerial mySerial(2);
MHZ19 myMHZ19;
bool mhz19_available = true;
uint32_t mhz19_retry_after = 0;

void io_setup();
void io_loop();
void turn_led(bool newState);
void turn_fan(bool newState);
void turn_soil_water_pump(bool newState);
void turn_air_water_pump(bool newState);
void command_handler();
void ai_handler();
void update_sensors();
void read_co2_sensor();
void update_display_page();
void enqueue_state_update();
void check_humidity();
void check_fan();
bool fan_has_co2_reading();
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

void io_setup()
{
    pinMode(FAN_PIN, OUTPUT);
    pinMode(SOIL_WATER_PUMP, OUTPUT);
    pinMode(AIR_WATER_PUMP, OUTPUT);
    digitalWrite(SOIL_WATER_PUMP, LOW);
    digitalWrite(AIR_WATER_PUMP, LOW);
    digitalWrite(FAN_PIN, FAN_OFF_LEVEL);

#if MHZ19_ENABLED
    mySerial.begin(9600, SERIAL_8N1, RXD2, TXD2);
    delay(1000);
    myMHZ19.begin(mySerial);
    myMHZ19.setRange(10000);
    if (myMHZ19.errorCode != RESULT_OK)
    {
        mhz19_available = false;
        mhz19_retry_after = millis() + 60000;
        log_w("MH-Z19 init failed with error=%d, CO2 reads paused for 60s", myMHZ19.errorCode);
    }
    // myMHZ19.autoCalibration();
#else
    mhz19_available = false;
    log_i("MH-Z19 sensor is disabled in io_handler.h");
#endif

    FastLED.addLeds<WS2811, STRIP_PIN, GRB>(leds, NUMLEDS);
    FastLED.clear();
    FastLED.show();

#if DHT_ENABLED
    dht.begin();
#else
    log_i("DHT sensor is disabled in io_handler.h");
#endif
    log_i("IO setup complete. Commands and state changes will be printed to serial.");
}
Command recieved_command;
uint32_t sensors_tmr;
uint32_t display_update;
uint32_t ai_check;
void io_loop()
{
    command_handler();
    ai_handler();
    if (millis() - sensors_tmr >= 5000)
    {
        sensors_tmr = millis();
        update_sensors();
    }
    if (millis() - display_update >= 5000)
    {
        display_update = millis();
        update_display_page();
    }
}
void command_handler()
{
    if (xQueueReceive(mqttToIo, &recieved_command, 0) == pdTRUE)
    {
        log_i(
            "Applying command %s with value=%d",
            command_type_name(recieved_command.type),
            recieved_command.value);
        if (agro_settings.ai_mode)
        {
            log_w("Manual command ignored because AI mode is enabled");
            return;
        }
        switch (recieved_command.type)
        {
        case CommandType::CMD_AIR_WATER_PUMP:
        {
            turn_air_water_pump(recieved_command.value != 0);

            break;
        }
        case CommandType::CMD_FAN:
        {
            turn_fan(recieved_command.value != 0);

            break;
        }
        case CommandType::CMD_SOIL_WATER_PUMP:
        {
            turn_soil_water_pump(recieved_command.value != 0);

            break;
        }
        case CommandType::CMD_LED:
        {
            turn_led(recieved_command.value != 0);
            break;
        }
        }
    }
}

void enqueue_state_update()
{
    Callback callback;
    callback.type = CallbackType::CLB_UPDATE;
    if (xQueueSend(ioToMqtt, &callback, 0) != pdTRUE)
    {
        log_w("State update queue is full");
    }
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
    }
    else
    {
        log_w("Failed to read temperature from DHT");
    }

    if (humidity_ok)
    {
        agro_state.humidity = (int)h;
    }
    else
    {
        log_w("Failed to read humidity from DHT");
    }
#else
    bool humidity_ok = false;
    bool temperature_ok = false;
#endif

    read_co2_sensor();

    int moisture_raw = analogRead(MOISTURE_SENSOR);
    int moisture = constrain(map(moisture_raw, 0, 4096, 0, 100), 0, 100);
    agro_state.soil_moisture = moisture;
    int light_raw = analogRead(LIGHT_SENSOR);
    agro_state.light = constrain(map(light_raw, 0, 4096, 0, 100), 0, 100);
    log_i(
        "Moisture raw=%d pct=%d | Light raw=%d pct=%d",
        moisture_raw,
        agro_state.soil_moisture,
        light_raw,
        agro_state.light);
    agro_state.sensors_ready = humidity_ok && temperature_ok;
    if (DHT_ENABLED && !agro_state.sensors_ready)
    {
        log_w("Skipping AI updates until DHT readings recover");
    }
    enqueue_state_update();
}
void update_display_page()
{
    char line0[17];
    char line1[17];

    snprintf(
        line0,
        sizeof(line0),
        "%s M:%3d L:%3d",
        agro_settings.ai_mode ? "AI" : "MAN",
        agro_state.soil_moisture,
        agro_state.light);

#if DHT_ENABLED
#if MHZ19_ENABLED
    if (agro_state.sensors_ready)
    {
        snprintf(
            line1,
            sizeof(line1),
            "T:%2d H:%2d C:%3d",
            agro_state.temperature,
            agro_state.humidity,
            agro_state.air_co2);
    }
    else
    {
        snprintf(line1, sizeof(line1), "T:-- H:-- C:--");
    }
#else
    if (agro_state.sensors_ready)
    {
        snprintf(
            line1,
            sizeof(line1),
            "T:%2d H:%2d CO2OFF",
            agro_state.temperature,
            agro_state.humidity);
    }
    else
    {
        snprintf(line1, sizeof(line1), "T:-- H:-- CO2OFF");
    }
#endif
#else
#if MHZ19_ENABLED
    snprintf(line1, sizeof(line1), "DHT OFF C:%4d", agro_state.air_co2);
#else
    snprintf(line1, sizeof(line1), "DHT OFF CO2OFF");
#endif
#endif

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(line0);
    lcd.setCursor(0, 1);
    lcd.print(line1);
}
void read_co2_sensor()
{
#if !MHZ19_ENABLED
    return;
#else
    if (!mhz19_available && millis() < mhz19_retry_after)
    {
        return;
    }

    int co2 = myMHZ19.getCO2();
    if (myMHZ19.errorCode == RESULT_OK && co2 > 0)
    {
        agro_state.air_co2 = co2;
        mhz19_available = true;
        return;
    }

    if (mhz19_available)
    {
        log_w("MH-Z19 read failed with error=%d, pausing CO2 retries for 60s", myMHZ19.errorCode);
    }

    mhz19_available = false;
    mhz19_retry_after = millis() + 60000;
#endif
}
bool fan_has_co2_reading()
{
#if MHZ19_ENABLED
    return mhz19_available && agro_state.air_co2 > 0;
#else
    return false;
#endif
}
void turn_led(bool newState)
{
    // digitalWrite(LED_PIN, newState);
    fill_solid(leds, NUMLEDS, CHSV(0, 0, newState ? 255 : 0));
    FastLED.show();
    agro_state.led = newState;
    log_i("Turned led %d", agro_state.led);
    enqueue_state_update();
}
void turn_fan(bool newState)
{
    digitalWrite(FAN_PIN, newState ? FAN_ON_LEVEL : FAN_OFF_LEVEL);
    agro_state.fan = newState;
    log_i(
        "Fan state changed to %d (pin=%d active_low=%d)",
        agro_state.fan,
        newState ? FAN_ON_LEVEL : FAN_OFF_LEVEL,
        FAN_ACTIVE_LOW);
    enqueue_state_update();
}
void turn_soil_water_pump(bool newState)
{
    digitalWrite(SOIL_WATER_PUMP, newState);
    agro_state.soil_water_pump = newState;
    log_i("Soil water pump state changed to %d", agro_state.soil_water_pump);
    enqueue_state_update();
}
void turn_air_water_pump(bool newState)
{
    digitalWrite(AIR_WATER_PUMP, newState);
    agro_state.air_water_pump = newState;
    log_i("Air water pump state changed to %d", agro_state.air_water_pump);
    enqueue_state_update();
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
    bool co2_ready = fan_has_co2_reading();
    bool should_turn_on = agro_state.temperature > agro_settings.max_temperature;
    bool should_turn_off = agro_state.temperature < agro_settings.min_temperature;

    if (co2_ready)
    {
        should_turn_on = should_turn_on || agro_state.air_co2 > agro_settings.max_air_co2;
        should_turn_off = should_turn_off && agro_state.air_co2 < agro_settings.min_air_co2;
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
    if (!agro_settings.ai_mode || !agro_state.sensors_ready)
    {
        return;
    }

    if (millis() - ai_check >= 2000)
    {
        ai_check = millis();
        check_moisture();
        check_light();
        check_humidity();
        check_fan();
    }
}
