# Subscribe topics (control/settings)
## {id}/{device}/control
Turning on/off device by sending 1 or 0 
- ### `{id}/soil_water_pump/control`

- ### `{id}/air_water_pump/control`

- ### `{id}/led/control`

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
  "air": "400–10000",
  "light": "0–100",
  "humidity": "0–100",
  "temperature": "-128–127",
  "moisture": "0–100"
}

```

# Fan behavior
- `GPIO13` controls the fan output in [`src/io_handler.h`](/Users/baxrom/ish_full/dimlom_ishi/smartgreenhouse/IotAgroAi/src/io_handler.h).
- Manual fan control comes from `{id}/fan/control`, but it only works when AI mode is off through `{id}/mode/ai = 0`.
- In AI mode, fan turns on when temperature goes above `max_temperature`.
- In AI mode, fan turns off when temperature goes below `min_temperature`.
- If `MHZ19_ENABLED` is set to `1` and the CO2 sensor is working, fan also turns on above `max_air_co2` and turns off only after both temperature and CO2 drop below their `min_*` thresholds.

# 12V relay wiring
- Do not connect a 12V fan directly to ESP32 `GPIO13`.
- Use a relay module or MOSFET driver between ESP32 and the 12V fan supply.
- Typical relay wiring: `GPIO13 -> IN`, `ESP32 GND -> relay GND`, `relay VCC -> 5V or module-rated VCC`, `12V+ -> relay COM`, `relay NO -> fan +`, `fan - -> 12V-`.
- Keep the 12V fan power separate from the ESP32 GPIO. Grounds should be common only where the relay module/driver requires it.
- Many relay boards are active-low. If your relay turns on when `GPIO13` is LOW, set `FAN_ACTIVE_LOW` to `1` in [`src/io_handler.h`](/Users/baxrom/ish_full/dimlom_ishi/smartgreenhouse/IotAgroAi/src/io_handler.h).
