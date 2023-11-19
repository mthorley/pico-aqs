# OXRS-IO-PICO Library

Based on the excellent [OXRS ecosystem](https://oxrs.io/), this library provides scaffolding support for API and MQTT integration for Raspberry Pi Pico W based micro controllers.

Sensors or other devices connected to the PicoW can use this library to obtain key device management features.

## Key Features

- [Captive portal](https://github.com/mthorley/wifimanager-pico) to allow configuration of the device to a target WiFi network
- Support for [OXRS AdminUI](https://github.com/OXRS-IO/OXRS-IO-AdminUI-WEB-APP) and API based automation of configuration
- OTA updates via web UI
- MQTT configuration via web UI
- Logging integration with OXRS_LOG library
- Watchdog
- Use of onboard Pico temperature sensor

### Admin UI

The admin UI is a static HTML site stored on a client machine that integrates with the OXRS_API to configure the device. Options available are shown below.

![Alt text](./docs/oxrs-admin-ui.png)

### OTA Updates

Over the Air updates can be performed using the AdminUI. LittleFS filesystem is used to store the uploaded firmware and therefore must be large enough to handle the firmware size. This can be configured as follows in the `platform.ini`

```
board_build.filesystem_size = 0.5m
```

### MQTT Configuration

MQTT configuration can be performed using the AdminUI. Note that TLS is not currently supported.

### Watchdog

### Logging via OXRS_LOG

## Development

This section outlines implementation choices and how to extend the library for future use.
