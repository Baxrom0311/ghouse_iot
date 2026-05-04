# Subscribe topics (control/settings)
## {id}/{device}/control
Turning on/off device by sending 1 or 0 
- ### `{id}/soil_water_pump/control`

- ### `{id}/air_water_pump/control`

- ### `{id}/led/control`
    Controls the plain LED relay output. Send `1` to turn it on and `0` to turn it off.

- ### `{id}/fan/control`

- ### `{id}/ping`
    Send 1 here and get OK
## {id}/{device}/settings
Setting parameters to auto-control
```json
{
    "max":50,
    "min":2,
}
```
- ### `{id}/humidity/settings`

- ### `{id}/temperature/settings`

- ### `{id}/moisture/settings`

- ### `{id}/air/settings`

OK if controller is working
# Publish topics (state)
## {id}/{device}/state
Controller sends responses here
- ### `{id}/log`
    If controller got `{id}/pingcontrol`, it in response sends `OK`

- ### `{id}/state`
    One of these
```json
{
  "fan": "0 | 1",
  "led": "0 | 1",
  "soil_water_pump": "0 | 1",
  "air_water_pump": "0 | 1",
  "air": "400–2000",
  "light": "0–100",
  "humidity": "0–100",
  "temperature": "-128–127",
  "moisture": "0–100"
}

```

# Fan behavior
- `GPIO25` controls the fan output in [`src/io_handler.h`](/Users/baxrom/ish_full/dimlom_ishi/smartgreenhouse/IotAgroAi/src/io_handler.h).
- Manual fan control comes from `{id}/fan/control`, but it only works when AI mode is off through `{id}/mode/ai = 0`.
- In AI mode, fan turns on when temperature goes above `max_temperature`.
- In AI mode, fan turns off when temperature goes below `min_temperature`.
- If `MQ135_ENABLED` is set to `1` and the air sensor is working, fan also turns on above `max_air_co2` and turns off only after both temperature and air readings drop below their `min_*` thresholds.

# MQ135 air sensor
- The current build uses MQ135 analog output on `GPIO34` (`MQ135_PIN`).
- The published MQTT/API field stays `air`, so backend and frontend do not need changes.
- MQ135 is not a calibrated CO2 sensor. The firmware maps analog raw readings from `MQ135_CLEAN_AIR_RAW` to `MQ135_POLLUTED_AIR_RAW` into an approximate `400-2000` air value for automation.
- If your MQ135 module analog output can exceed 3.3V, use a voltage divider before connecting it to ESP32 `GPIO34`.

# ST7735S TFT display
- The current build uses a 1.8" 128x160 RGB TFT with ST7735S controller.
- Default wiring is configured in [`src/display_handler.h`](/Users/baxrom/ish_full/dimlom_ishi/smartgreenhouse/IotAgroAi/src/display_handler.h): `CS=GPIO5`, `DC=GPIO21`, `RST=GPIO22`, `MOSI=GPIO19`, `SCLK=GPIO18`, `BL=GPIO4`.
- `MOSI` is not on `GPIO23` because `GPIO23` is already used by the DHT sensor.
- If the display colors or orientation look wrong, change `TFT_INIT_TAB` or `TFT_ROTATION` in `src/display_handler.h`.

# 12V relay wiring
- Do not connect a 12V fan directly to ESP32 `GPIO25`.
- Use a relay module or MOSFET driver between ESP32 and the 12V fan supply.
- Typical relay wiring: `GPIO25 -> IN`, `ESP32 GND -> relay GND`, `relay VCC -> 5V or module-rated VCC`, `12V+ -> relay COM`, `relay NO -> fan +`, `fan - -> 12V-`.
- Keep the 12V fan power separate from the ESP32 GPIO. Grounds should be common only where the relay module/driver requires it.
- Many relay boards are active-low. If your relay turns on when `GPIO25` is LOW, set `FAN_ACTIVE_LOW` to `1` in [`src/io_handler.h`](/Users/baxrom/ish_full/dimlom_ishi/smartgreenhouse/IotAgroAi/src/io_handler.h).

# LED relay wiring
- The current build uses a plain LED through a relay on `GPIO26` (`LED_RELAY_PIN`).
- The MQTT/API device name stays `led`, so existing `{id}/led/control` commands still work.
- Many relay boards are active-low. If your LED relay turns on when `GPIO26` is HIGH, set `LED_RELAY_ACTIVE_LOW` to `0` in [`src/io_handler.h`](/Users/baxrom/ish_full/dimlom_ishi/smartgreenhouse/IotAgroAi/src/io_handler.h).
- The previous FastLED strip code is still present. Set `LED_DRIVER_RELAY` to `0` to use the old `STRIP_PIN`/`NUMLEDS` implementation.
