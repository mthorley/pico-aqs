#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <SensirionI2CSen5x.h>
#include <Wire.h>
#include "SEN5xDeviceStatus.h"

/*
 * OXRS firmware supporting Sensirion 5x (SEN50, SEN54, SEN55) air quality sensors.
 * Refer https://www.sensirion.com/media/documents/6791EFA0/62A1F68F/Sensirion_Datasheet_Environmental_Node_SEN5x.pdf
 */

// Struct capturing measurements of SEN5x sensor.
// Note PM4.0 and PM10.0 are statistically generated and not measured: refer
// https://sensirion.com/media/documents/B7AAA101/61653FB8/Sensirion_Particulate_Matter_AppNotes_Specification_Statement.pdf
typedef struct {
    float pm1p0;                // particulate matter PM1.0 µm
    float pm2p5;                // particulate matter PM2.5 µm
    float pm4p0;                // particulate matter PM4.0 µm
    float pm10p0;               // particulate matter PM10.0 µm
    float humidityPercent;      // relative humidity  %
    float tempCelsuis;          // temperature        °C
    float vocIndex;             // volatile organic compound index 1-500
    float noxIndex;             // nitrous oxide index 1-500
} SEN5x_telemetry_t;

class OXRS_SEN5x {
public:
    OXRS_SEN5x(SEN5x_model_t model);

    typedef uint16_t Error_t;

    void begin(TwoWire& wire);
    void loop();

    void getTelemetry(JsonVariant json);

    // OXRS ecosystem
    void onConfig(JsonVariant json);
    void onCommand(JsonVariant json);
    void setConfigSchema(JsonVariant json);
    void setCommandSchema(JsonVariant json);

private:
    // defaults
    inline static const uint32_t DEFAULT_PUBLISH_TELEMETRY_MS = 10000;
    inline static const int8_t   DEFAULT_TEMP_OFFSET_C        = 0;

    // OXRS config items
    inline static const String PUBLISH_TELEMETRY_FREQ_CONFIG  = "publishTelemetrySeconds";
    inline static const String TEMPERATURE_OFFSET_CONFIG      = "temperatureOffsetCelsius";

    // OXRS command items
    inline static const String RESET_COMMAND                  = "resetCommand";
    inline static const String FANCLEAN_COMMAND               = "fanCleanCommand";
    inline static const String CLEAR_DEVICESTATUS_COMMAND     = "clearDeviceStatusCommand";

    void logError(Error_t error, const __FlashStringHelper* s);
    double round2dp(float d) const;
    JsonVariant findNestedKey(JsonObject obj, const String& key) const;
    String getModelName() const;

    Error_t getSerialNumber(String& serialNo);
    Error_t getModuleVersions(String& sensorNameVersion);
    Error_t getMeasurements(SEN5x_telemetry_t& t);
    Error_t refreshDeviceStatus();

    void telemetryAsJson(const SEN5x_telemetry_t& t, JsonVariant json) const;

    // commands
    void resetSensor();
    void fanClean();
    void clearDeviceStatus();

    void setTemperatureOffset();
    void initialiseDevice();

    uint32_t _publishTelemetry_ms;          // how often publish
    uint32_t _lastPublishTelemetry_ms;      // last time published since start
    float_t  _tempOffset_celsius;           // sensor temperature offset

    SensirionI2CSen5x _sensor;              // i2c library
    SEN5x_model_t     _model;               // sensor model
    SEN5xDeviceStatus _deviceStatus;        // sensor device status
    bool              _deviceReady;         // device connected and successfully reset
};
