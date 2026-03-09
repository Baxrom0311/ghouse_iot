#include <io_handler.h>
#include <display_handler.h>
#include <math.h>

DHT dht(DHTPIN, DHTTYPE);
CRGB leds[NUMLEDS];
#define RXD2 16
#define TXD2 17
HardwareSerial mySerial(2);
MHZ19 myMHZ19;

void io_setup();
void io_loop();
void turn_led(bool newState);
void turn_fan(bool newState);
void turn_soil_water_pump(bool newState);
void turn_air_water_pump(bool newState);
void command_handler();
void ai_handler();
void update_sensors();
void enqueue_state_update();
void check_humidity();
void check_fan();
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
    digitalWrite(FAN_PIN, LOW);

    mySerial.begin(9600, SERIAL_8N1, RXD2, TXD2);
    delay(1000);
    myMHZ19.begin(mySerial);
    myMHZ19.setRange(10000);
    // myMHZ19.autoCalibration();

    FastLED.addLeds<WS2811, STRIP_PIN, GRB>(leds, NUMLEDS);
    FastLED.clear();
    FastLED.show();

    dht.begin();
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
        lcd.clear();
        lcd.setCursor(0, 0);
        if (agro_settings.ai_mode)
            lcd.print("AI mode");
        else
            lcd.print("Manual mode");
        lcd.setCursor(0, 1);
        lcd.print("T:");
        if (!agro_state.sensors_ready)
            lcd.print("N");
        else
            lcd.print(agro_state.temperature);
        lcd.print(" H:");
        if (!agro_state.sensors_ready)
            lcd.print("N");
        else
            lcd.print(agro_state.humidity);
        lcd.print(" A:");
        if (!agro_state.sensors_ready)
            lcd.print("N");
        else
            lcd.print(agro_state.air_co2);
        lcd.print(" M:");
        if (!agro_state.sensors_ready)
            lcd.print("N");
        else
            lcd.print(agro_state.soil_moisture);
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

    int CO2 = myMHZ19.getCO2();
    if (CO2 > 0)
    {
        agro_state.air_co2 = CO2;
    }

    int moisture = constrain(map(analogRead(MOISTURE_SENSOR), 0, 4096, 0, 100), 0, 100);
    log_i("Moisture %d", moisture);
    agro_state.soil_moisture = moisture;
    int light = analogRead(LIGHT_SENSOR);
    agro_state.light = constrain(map(light, 0, 4096, 0, 100), 0, 100);
    agro_state.sensors_ready = humidity_ok && temperature_ok;
    if (!agro_state.sensors_ready)
    {
        log_w("Skipping AI updates until DHT readings recover");
    }
    enqueue_state_update();
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
    digitalWrite(FAN_PIN, newState);
    agro_state.fan = newState;
    log_i("Fan state changed to %d", agro_state.fan);
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
    bool should_turn_on = agro_state.air_co2 > agro_settings.max_air_co2 ||
                          agro_state.temperature > agro_settings.max_temperature;
    bool should_turn_off = agro_state.air_co2 < agro_settings.min_air_co2 &&
                           agro_state.temperature < agro_settings.min_temperature;

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
