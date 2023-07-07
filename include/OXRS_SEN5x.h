#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <SensirionI2CSen5x.h>
#include <Wire.h>

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
    OXRS_SEN5x() : 
        _publishTelemetry_ms(10000),
        _tempOffset_celsius(0),
        _lastPublishTelemetry_ms(0) {};

    typedef uint16_t Error_t;

    void begin(TwoWire& wire);
    void loop();

    void getTelemetry(JsonVariant json);

    void onConfig(JsonVariant json);
    void setConfigSchema(JsonVariant json);

    void printToSerial(SEN5x_telemetry_t& t);

    void checkDeviceStatus();
    void printModuleVersions();
    void printSerialNumber();

private:
    inline static const String PUBLISH_TELEMETERY_SECONDS   = "publishTelemetrySeconds";
    inline static const String TEMPERATURE_OFFSET           = "temperatureOffsetCelsius";

    void logTelemetry(SEN5x_telemetry_t& t);
    void logError(Error_t error, const char* s);

    void setTemperatureOffset();
    Error_t getMeasurements(SEN5x_telemetry_t& t);

    uint32_t _publishTelemetry_ms;
    uint32_t _lastPublishTelemetry_ms;
    float_t  _tempOffset_celsius;

    SensirionI2CSen5x _sensor;
};
