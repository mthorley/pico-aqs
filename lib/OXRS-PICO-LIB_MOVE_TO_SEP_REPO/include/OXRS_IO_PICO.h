/**
 * OXRS-IO-Generic-PICO-LIB
 */
#pragma once

#include <ArduinoJson.h>
#include <OXRS_MQTT.h>

/*
Utility class to enable OXRS API, MQTT libraries and
OXRS Admin UI for any RPi PicoW based device.
*/
class OXRS_IO_PICO
{
public:
    OXRS_IO_PICO(bool useOnBoardTempSensor);

    void begin(jsonCallback config, jsonCallback command);
    void loop();

    // Firmware can define the config/commands it supports - for device discovery and adoption
    void setConfigSchema(JsonVariant json);
    void setCommandSchema(JsonVariant json);

    OXRS_MQTT* getMQTT(void);

    static void apiAdoptCallback(JsonVariant json);

    // Helper for publishing to tele/ topic
    void publishTelemetry(JsonVariant telemetry);

    static JsonVariant findNestedKey(JsonObject obj, const String &key);
    inline static const String RESTART_COMMAND = "restart";

    float readOnboardTemperature(bool celsiusNotFahr = true);

private:
    void initialiseRestApi();
    void initialiseNetwork(byte *mac);
    void initialiseMqtt(byte *mac);
    void initialiseWatchdog();
    void initialiseTempSensor();

    static boolean isNetworkConnected();

    // Config helpers
    static void getFirmwareJson(JsonVariant json);
    static void getSystemJson(JsonVariant json);
    static void getNetworkJson(JsonVariant json);
    static void getConfigSchemaJson(JsonVariant json);
    static void getCommandSchemaJson(JsonVariant json);
    static void mergeJson(JsonVariant dst, JsonVariantConst src);

    bool _useOnBoardTempSensor;     // true then log temperature via ADC from Pico onboard sensor
};
