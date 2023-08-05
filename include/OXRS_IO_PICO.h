/**
 * OXRS-IO-Generic-PICO-LIB
 */
#pragma once

#include <ArduinoJson.h>
#include <OXRS_MQTT.h>

/*
Utility class to enable OXRS API, MQTT libraries and
OXRS Admin UI for any PICOw based device.
*/
class OXRS_IO_PICO
{
public:
    OXRS_IO_PICO();

    static void apiAdoptCallback(JsonVariant json);

    void setConfigSchema(JsonVariant json);
    void setCommandSchema(JsonVariant json);

    void begin(jsonCallback config, jsonCallback command);
    void loop();

    void publishTelemetry(JsonVariant telemetry);

    static JsonVariant findNestedKey(JsonObject obj, const String &key);
    static void setLogLevelCommand(const String& level);

    inline static const String RESTART_COMMAND     = "restart";

private:
    void initialiseRestApi();
    void initialiseNetwork(byte *mac);
    void initialiseMqtt(byte *mac);
    void initialiseWatchdog();

    static boolean isNetworkConnected();

    // config helpers
    static void getFirmwareJson(JsonVariant json);
    static void getSystemJson(JsonVariant json);
    static void getNetworkJson(JsonVariant json);
    static void getConfigSchemaJson(JsonVariant json);
    static void getCommandSchemaJson(JsonVariant json);
    static void mergeJson(JsonVariant dst, JsonVariantConst src);
};
