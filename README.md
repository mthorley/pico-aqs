# Raspberry Pi Pico W Air Quality Sensor

Integration of Sensirion 55 air quality sensor with a Raspberry Pi Pico W.

Features
- OXRS Admin UI via OXRS_MQTT and OXRS_API for PicoW
- Configuration items
  - telemetry publishing frequency
  - offset temperature


## Future works:

```
config
 - celsius / farenheit
```

```
voc thresholds:
  green : 0   - 149
  yellow: 150 - 249
  orange: 250 - 399
  red:    400 - 500

nox thresholds:
  green : 0-19
  yellow: 20-149
  orange: 150-300
  red: 401-500
```

```
Features
- do pmp* as moving averages given noise 
```
