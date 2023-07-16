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
class OXRS_IO_PICO {
public:
  static void apiAdoptCallback(JsonVariant json);

  static void setConfigSchema(JsonVariant json);
  static void setCommandSchema(JsonVariant json);

  static void begin(jsonCallback config, jsonCallback command);
  static void loop();

  static void publishTelemetry(JsonVariant telemetry);

  inline static const String RESTART_COMMAND = "restart";

private:
  static void initialiseRestApi();
  static void initialiseNetwork(byte* mac);
  static void initialiseMqtt(byte *mac);

  static boolean isNetworkConnected();

  // config helpers
  static void getFirmwareJson(JsonVariant json);
  static void getSystemJson(JsonVariant json);
  static void getNetworkJson(JsonVariant json);
  static void getConfigSchemaJson(JsonVariant json);
  static void getCommandSchemaJson(JsonVariant json);
  static void mergeJson(JsonVariant dst, JsonVariantConst src);

};
