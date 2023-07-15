/**
 * 
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "OXRS_SEN5x.h"
#include "SEN5xDeviceStatus.h"
#include "OXRS_LOG.h"

static const char* _LOG_PREFIX = "[OXRS_SEN5x] ";

void OXRS_SEN5x::begin(TwoWire& wire)
{
    // assumes Wire.begin() has been called prior
    _sensor.begin(Wire);

    Error_t error = _sensor.deviceReset();
    if (error) {
        logError(error, F("Error trying to execute deviceReset():"));
    }

// Print SEN55 module information if i2c buffers are large enough
#ifdef USE_PRODUCT_INFO
//FIXME: 
// [OXRS_SEN5x] [ERROR] Error trying to execute getSerialNumber():  Can't execute this command on this board, internal I2C buffer is too small
    String serialNo;
    error = getSerialNumber(serialNo);
    if (!error) 
        LOG_INFO(serialNo);

// [OXRS_SEN5x] [ERROR] Error trying to execute getProductName(): Can't execute this command on this board, internal I2C buffer is too small
    String moduleVersions;
    error = getModuleVersions(moduleVersions);
    if (!error)
        LOG_INFO(moduleVersions);
#endif

    initialiseDevice();
}

void OXRS_SEN5x::initialiseDevice() {
    // Adjust tempOffset to account for additional temperature offsets
    // exceeding the SEN module's self heating.
    setTemperatureOffset();

    // Start Measurement
    Error_t error = _sensor.startMeasurement();
    if (error) {
        logError(error, F("Error trying to execute startMeasurement():"));
    }
}

OXRS_SEN5x::Error_t OXRS_SEN5x::getMeasurements(SEN5x_telemetry_t& t)
{
    return _sensor.readMeasuredValues(t.pm1p0, t.pm2p5, t.pm4p0, t.pm10p0,
        t.humidityPercent, t.tempCelsuis, t.vocIndex, t.noxIndex);
}

void OXRS_SEN5x::loop()
{
    // must be done post begin
    float current_offset;
    Error_t error = _sensor.getTemperatureOffsetSimple(current_offset);
    if (error) {
        logError(error, F("Failed to getTemperatureOffsetSimple():"));
    }
    else {
        if (current_offset != _tempOffset_celsius) {
            // tempoffset has changed, update sensor
            setTemperatureOffset();
        }
    }
}

// Set temperature offset
void OXRS_SEN5x::setTemperatureOffset()
{
    Error_t error = _sensor.setTemperatureOffsetSimple(_tempOffset_celsius);
    if (error) {
        logError(error, F("Error trying to execute setTemperatureOffsetSimple():"));
    } else {
        LOGF_DEBUG("Set temperature offset: %.02f celsius (SEN54/55 only)", _tempOffset_celsius);
    }
}

double OXRS_SEN5x::round2dp(double value) const {
   return (int)(value * 100 + 0.5) / 100.0;
}

// Get telemetry from AQS
void OXRS_SEN5x::getTelemetry(JsonVariant json)
{   
  // Do not publish if telemetry has been disabled
  if (_publishTelemetry_ms == 0)
    return;

  // Check if time passed is enough to publish
  if ((millis() - _lastPublishTelemetry_ms) > _publishTelemetry_ms)
  {
    LOG_DEBUG(F("Taking measurements"));
    SEN5x_telemetry_t t;
    Error_t error = getMeasurements(t);
    if (error) {
        logError(error, F("Failed to get measurements"));
    } 
    else {
// TODO: Filter these attributes by the sensor model
// From sensirion i2c README
// - SEN50 (only particulate matter signals available)
// - SEN54 (no NOx signal available)
// - SEN55 (full feature set)

        json["pm1p0"]  = round2dp(t.pm1p0);
        json["pm2p5"]  = round2dp(t.pm2p5);
        json["pm4p0"]  = round2dp(t.pm4p0);
        json["pm10p0"] = round2dp(t.pm10p0);
        json["hum"]    = round2dp(t.humidityPercent);
        json["temp"]   = round2dp(t.tempCelsuis);
        json["vox"]    = round2dp(t.vocIndex);
        json["nox"]    = round2dp(t.noxIndex);

        String jsonAsString;
        serializeJson(json, jsonAsString);
        LOGF_DEBUG("%s", jsonAsString.c_str());
    }

    // Reset timer
    _lastPublishTelemetry_ms = millis();

// FIXME: check device status should be before getMeasurements && only if no issues should getMeasurements proceed?
    // check devicestatus register
    checkDeviceStatus();
  }
}

OXRS_SEN5x::Error_t OXRS_SEN5x::getSerialNumber(String& serialNo)
{
    unsigned char serialNumber[32];
    uint8_t serialNumberSize = 32;

    Error_t error = _sensor.getSerialNumber(serialNumber, serialNumberSize);
    if (error) {
        logError(error, F("Error trying to execute getSerialNumber():"));
        return error;
    }
    else {
        serialNo = (char*)serialNumber;
        return 0;
    }
}

OXRS_SEN5x::Error_t OXRS_SEN5x::getModuleVersions(String& sensorNameVersion)
{
    unsigned char productName[32];
    uint8_t productNameSize = 32;

    Error_t error = _sensor.getProductName(productName, productNameSize);
    if (error) {
        logError(error, F("Error trying to execute getProductName():"));
        return error;
    }

    uint8_t firmwareMajor;
    uint8_t firmwareMinor;
    bool firmwareDebug;
    uint8_t hardwareMajor;
    uint8_t hardwareMinor;
    uint8_t protocolMajor;
    uint8_t protocolMinor;
    error = _sensor.getVersion(firmwareMajor, firmwareMinor, firmwareDebug,
        hardwareMajor, hardwareMinor, protocolMajor, protocolMinor);
    if (error) {
        logError(error, F("Error trying to execute getVersion():"));
        return error;
    }

    char buf[256];
    sprintf_P(buf, PSTR("ProductName: %s, Firmware: %d.%d, Hardware: %d.%d"),
        productName, firmwareMajor, firmwareMinor, hardwareMajor, hardwareMinor);
    sensorNameVersion = buf;
    return 0;
}

// Check device status register
OXRS_SEN5x::Error_t OXRS_SEN5x::checkDeviceStatus()
{
    // read device status register values
    uint32_t reg;
    Error_t error = _sensor.readDeviceStatus(reg);
    if (error) {
        logError(error, F("Failed to read device status:"));
        return error;
    }

    // parse register bits
    SEN5xDeviceStatus ds(SEN5xDeviceStatus::device_t::sen55_sdn_t); // FIXME: can we autodetect the device?
    ds.setRegister(reg);
    ds.logStatus();
    return 0;
}

void OXRS_SEN5x::onConfig(JsonVariant json) 
{
    if (json.containsKey(PUBLISH_TELEMETRY_FREQ))
    {
        _publishTelemetry_ms = json[PUBLISH_TELEMETRY_FREQ].as<uint32_t>() * 1000L;
        LOGF_INFO("Set config publish telemetry ms to %" PRIu32 "", _publishTelemetry_ms);
    }
    if (json.containsKey(TEMPERATURE_OFFSET))
    {
        _tempOffset_celsius = json[TEMPERATURE_OFFSET].as<float_t>();
        LOGF_INFO("Set config temperature offset degrees to %.02f", _tempOffset_celsius);
    }
}

void OXRS_SEN5x::onCommand(JsonVariant json)
{
    if (json.containsKey(RESET_COMMAND) && json[RESET_COMMAND].as<bool>())
    {
        LOG_INFO(F("Resetting sensor"));
        Error_t error = _sensor.deviceReset();
        if (error)
            logError(error, F("Error reseting sensor"));
        initialiseDevice();
        return;
    }
    if (json.containsKey(FANCLEAN_COMMAND) && json[FANCLEAN_COMMAND].as<bool>())
    {
        LOG_INFO(F("Fanclean command"));
        Error_t error = _sensor.startFanCleaning();
        if (error)
            logError(error, F("Error starting fan cleaning"));
        return;
    }
    else {
//            if (fancleaning)
//                cancel fan cleaning
    }
}

void OXRS_SEN5x::setCommandSchema(JsonVariant command)
{
  JsonObject resetsensor = command.createNestedObject(RESET_COMMAND);
  resetsensor["title"] = "Reset Sen5x Sensor";
  resetsensor["type"] = "boolean";

  JsonObject fanclean = command.createNestedObject(FANCLEAN_COMMAND);
  fanclean["title"] = "Start Fan Cleaning (default is weekly).";
  fanclean["type"] = "boolean";
// TODO
    // add cleardevicestatus
}

void OXRS_SEN5x::setConfigSchema(JsonVariant config) 
{
  JsonObject publishTelemetry = config.createNestedObject(PUBLISH_TELEMETRY_FREQ);
  publishTelemetry["title"] = "Publish Telemetry Frequency (seconds)";
  publishTelemetry["description"] = \
    "How often to publish telemetry from the air quality sensor attached to your device \
(defaults to 10 seconds, setting to 0 disables telemetry reports). \
Must be a number between 0 and 86400 (i.e. 1 day).";
  publishTelemetry["type"] = "integer";
  publishTelemetry["minimum"] = 0;
  publishTelemetry["maximum"] = 86400;

  JsonObject temperatureOffset = config.createNestedObject(TEMPERATURE_OFFSET);
  temperatureOffset["title"] = "Temperature offset (°C)";
  temperatureOffset["description"] = \
    "Temperature offset in Celsuis. Default 0. Must be a number between -10 and 10.";
  temperatureOffset["type"] = "integer";
  temperatureOffset["minimum"] = -10;
  temperatureOffset["maximum"] = 10;
}

void OXRS_SEN5x::logError(Error_t error, const __FlashStringHelper* s)
{
    char errorMessage[256];
    errorToString(error, errorMessage, 256);
    LOGF_ERROR("%s %s", s, errorMessage);
}
