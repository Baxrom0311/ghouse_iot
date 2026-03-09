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
