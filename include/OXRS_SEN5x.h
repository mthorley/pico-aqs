#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <SensirionI2CSen5x.h>
#include <Wire.h>
#include "SEN5xDeviceStatus.h"

typedef struct {
    float pm1p0;
    float pm2p5;
    float pm4p0;
    float pm10p0;
    float humidityPercent;
    float tempCelsuis;
    float vocIndex;
    float noxIndex;
} SEN5x_telemetry_t;

class OXRS_SEN5x {
public:
    OXRS_SEN5x(SEN5x_model_t model);

    typedef uint16_t Error_t;

    void begin(TwoWire& wire);
    void loop();

    void getTelemetry(JsonVariant json);

    void onConfig(JsonVariant json);
    void onCommand(JsonVariant json);
    void setConfigSchema(JsonVariant json);
    void setCommandSchema(JsonVariant json);

private:
    inline static const String PUBLISH_TELEMETRY_FREQ     = "publishTelemetrySeconds";
    inline static const String TEMPERATURE_OFFSET         = "temperatureOffsetCelsius";
    inline static const String RESET_COMMAND              = "resetCommand";
    inline static const String FANCLEAN_COMMAND           = "fanCleanCommand";
    inline static const String CLEAR_DEVICESTATUS_COMMAND = "clearDeviceStatusCommand";

    void logError(Error_t error, const __FlashStringHelper* s);
    double round2dp(double d) const;

    Error_t getSerialNumber(String& serialNo);
    Error_t getModuleVersions(String& sensorNameVersion);
    Error_t getMeasurements(SEN5x_telemetry_t& t);
    Error_t refreshDeviceStatus();
    void setTemperatureOffset();
    void initialiseDevice();

    uint32_t _publishTelemetry_ms;
    uint32_t _lastPublishTelemetry_ms;
    float_t  _tempOffset_celsius;

    SensirionI2CSen5x _sensor;
    SEN5x_model_t     _model;
    SEN5xDeviceStatus _deviceStatus;
};
