/**
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <OXRS_LOG.h>
#include <OXRS_SEN5x.h>
#include <SEN5xDeviceStatus.h>

// Refer https://github.com/Sensirion/arduino-i2c-sen5x/blob/master/examples/exampleUsage/exampleUsage.ino
#define MAXBUF_REQUIREMENT 48
#if (defined(I2C_BUFFER_LENGTH) &&                 \
     (I2C_BUFFER_LENGTH >= MAXBUF_REQUIREMENT)) || \
    (defined(BUFFER_LENGTH) && BUFFER_LENGTH >= MAXBUF_REQUIREMENT)
#define USE_PRODUCT_INFO
#endif

static const char *_LOG_PREFIX = "[OXRS_SEN5x] ";

OXRS_SEN5x::OXRS_SEN5x(SEN5x_model_t model) :
    _model(model),
    _deviceStatus(model),
    _publishTelemetry_ms(DEFAULT_PUBLISH_TELEMETRY_MS),
    _tempOffset_celsius(DEFAULT_TEMP_OFFSET_C),
    _lastPublishTelemetry_ms(0),
    _deviceReady(false)
{
};

void OXRS_SEN5x::begin(TwoWire &wire)
{
    // assumes Wire.begin() has been called prior
    _sensor.begin(Wire);

    Error_t error = _sensor.deviceReset();
    if (error)
    {
        logError(error, F("Error trying to execute deviceReset():"));
        return;
    }

// Print SEN5x module information if i2c buffers are large enough
#ifdef USE_PRODUCT_INFO
    String serialNo;
    error = getSerialNumber(serialNo);
    if (!error)
        LOG_INFO(serialNo);

    String moduleVersions;
    error = getModuleVersions(moduleVersions);
    if (!error)
        LOG_INFO(moduleVersions);
#endif

    initialiseDevice();

    _deviceReady = true;
}

void OXRS_SEN5x::initialiseDevice()
{
    if (_model != SEN50)
    {
        // Adjust tempOffset to account for additional temperature offsets
        // exceeding the SEN module's self heating.
        setTemperatureOffset();
    }

    // Start Measurement
    Error_t error = _sensor.startMeasurement();
    if (error)
    {
        logError(error, F("Error trying to execute startMeasurement():"));
    }
}

OXRS_SEN5x::Error_t OXRS_SEN5x::getMeasurements(SEN5x_telemetry_t &t)
{
    return _sensor.readMeasuredValues(t.pm1p0, t.pm2p5, t.pm4p0, t.pm10p0,
        t.humidityPercent, t.tempCelsuis, t.vocIndex, t.noxIndex);
}

void OXRS_SEN5x::loop()
{
    if (!_deviceReady)
        return;

    if (_model != SEN50)
    { // must be done post begin
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
}

// Set temperature offset
void OXRS_SEN5x::setTemperatureOffset()
{
    Error_t error = _sensor.setTemperatureOffsetSimple(_tempOffset_celsius);
    if (error)
    {
        logError(error, F("Error trying to execute setTemperatureOffsetSimple():"));
    }
    else
    {
        LOGF_DEBUG("Set temperature offset: %.02f celsius", _tempOffset_celsius);
    }
}

double OXRS_SEN5x::round2dp(float value) const
{
    return (std::isnan(value)) ? 0 : (int)(value * 100 + 0.5) / 100.0;
}

void OXRS_SEN5x::telemetryAsJson(const SEN5x_telemetry_t& t, JsonVariant json) const
{
    // Refer https://sensirion.com/media/documents/6791EFA0/62A1F68F/Sensirion_Datasheet_Environmental_Node_SEN5x.pdf
    // as to which models have which capabilities
    json["pm1p0"] = round2dp(t.pm1p0);
    json["pm2p5"] = round2dp(t.pm2p5);
    json["pm4p0"] = round2dp(t.pm4p0);
    json["pm10p0"] = round2dp(t.pm10p0);

    if (_model != SEN50)
    {
        json["hum"] = round2dp(t.humidityPercent);
        json["temp"] = round2dp(t.tempCelsuis);
        json["vox"] = round2dp(t.vocIndex);
    }

    if (_model == SEN55)
        json["nox"] = round2dp(t.noxIndex); 
}

// Get telemetry from AQS
void OXRS_SEN5x::getTelemetry(JsonVariant json)
{
    // Do not publish if telemetry has been disabled
    if (_publishTelemetry_ms == 0) {
        LOG_INFO(F("Telemetry disabled"));
        return;
    }

    // Check if time passed is enough to publish
    if ((millis() - _lastPublishTelemetry_ms) > _publishTelemetry_ms) {
        // check device status
        Error_t error = refreshDeviceStatus();
        if (error) {
            logError(error, F("Failed to referesh device status:"));
            _lastPublishTelemetry_ms = millis();
            return;
        }

        if (_deviceStatus.hasIssue()) {
            _deviceStatus.logStatus();
        }

        // check device is dataready
        bool dataReady;
        error = _sensor.readDataReady(dataReady);
        if (error) {
            logError(error, F("Failed to get dataready state"));
            _lastPublishTelemetry_ms = millis();
            return;
        }

        if (!dataReady) {
            if (_deviceStatus.isFanCleaningActive())
                LOG_DEBUG(F("Device data not ready as fan cleaning active"));
            else
                LOG_DEBUG(F("Device data not ready"));
            _lastPublishTelemetry_ms = millis();
            return;
        }

        LOG_DEBUG(F("Taking measurements"));
        SEN5x_telemetry_t t;
        error = getMeasurements(t);
        if (error) {
            logError(error, F("Failed to get measurements"));
            _lastPublishTelemetry_ms = millis();
            return;
        }

        telemetryAsJson(t, json);

        // Reset timer
        _lastPublishTelemetry_ms = millis();
    }
}

OXRS_SEN5x::Error_t OXRS_SEN5x::getSerialNumber(String &serialNo)
{
    unsigned char serialNumber[32];
    uint8_t serialNumberSize = 32;

    Error_t error = _sensor.getSerialNumber(serialNumber, serialNumberSize);
    if (error)
    {
        logError(error, F("Error trying to execute getSerialNumber():"));
        return error;
    }
    else
    {
        serialNo = (char *)serialNumber;
        return 0;
    }
}

OXRS_SEN5x::Error_t OXRS_SEN5x::getModuleVersions(String &sensorNameVersion)
{
    unsigned char productName[32];
    uint8_t productNameSize = 32;

    Error_t error = _sensor.getProductName(productName, productNameSize);
    if (error)
    {
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

    if (error)
    {
        logError(error, F("Error trying to execute getVersion():"));
        return error;
    }

    char buf[256];
    sprintf_P(buf, PSTR("ProductName: %s, Firmware: %d.%d, Hardware: %d.%d"),
              productName, firmwareMajor, firmwareMinor, hardwareMajor, hardwareMinor);
    sensorNameVersion = buf;
    return 0;
}

OXRS_SEN5x::Error_t OXRS_SEN5x::refreshDeviceStatus()
{
    // read device status register values
    uint32_t reg;
    Error_t error = _sensor.readDeviceStatus(reg);
    if (error) {
        return error;
    }

    // parse register bits
    _deviceStatus.setRegister(reg);
    return 0;
}

JsonVariant OXRS_SEN5x::findNestedKey(JsonObject obj, const String &key) const
{
    JsonVariant foundObject = obj[key];
    if (!foundObject.isNull())
        return foundObject;

    for (JsonPair pair : obj)
    {
        JsonVariant nestedObject = findNestedKey(pair.value(), key);
        if (!nestedObject.isNull())
            return nestedObject;
    }

    return JsonVariant();
}

void OXRS_SEN5x::onConfig(JsonVariant json)
{
    JsonVariant publishTelemetryFreq = findNestedKey(json, PUBLISH_TELEMETRY_FREQ_CONFIG);
    if (!publishTelemetryFreq.isNull())
    {
        _publishTelemetry_ms = publishTelemetryFreq.as<uint32_t>() * 1000L;
        LOGF_INFO("Set config publish telemetry ms to %" PRIu32 "", _publishTelemetry_ms);
    }

    if (_model != SEN50)
    {
        JsonVariant tempOffset = findNestedKey(json, TEMPERATURE_OFFSET_CONFIG);
        if (!tempOffset.isNull()) {
            _tempOffset_celsius = tempOffset.as<float_t>();
            LOGF_INFO("Set config temperature offset degrees to %.02f", _tempOffset_celsius);
        }
    }
}

void OXRS_SEN5x::resetSensor()
{
    LOG_INFO(F("Resetting sensor"));
    // reset sensor
    Error_t error = _sensor.deviceReset();
    if (error)
        logError(error, F("Error reseting sensor"));

    // reinitialise device
    initialiseDevice();
}

void OXRS_SEN5x::fanClean()
{
    LOG_INFO(F("Fanclean command"));
    // check device status
    Error_t error = refreshDeviceStatus();
    if (error)
    {
        logError(error, F("Failed to referesh device status:"));
        return;
    }

    if (!_deviceStatus.isFanCleaningActive())
    {
        Error_t error = _sensor.startFanCleaning();
        error ? logError(error, F("Error starting fan cleaning")) : LOG_INFO(F("Fan cleaning started"));
// TODO: capture time of fan cleaning request so we can ensure its done once a week esp if the device is being turned on/off a lot like during development
    }
    else
    {
        LOG_DEBUG(F("Fan cleaning already active"));
    }
}

void OXRS_SEN5x::clearDeviceStatus()
{
    LOG_INFO(F("Clear device status"));
    // only clear if device status bits set

    // check device status
    Error_t error = refreshDeviceStatus();
    if (error)
    {
        logError(error, F("Failed to referesh device status:"));
        return;
    }

    if (_deviceStatus.hasIssue())
    {
        uint32_t deviceStatus;
        error = _sensor.readAndClearDeviceStatus(deviceStatus);
        error ? logError(error, F("Error clearing device status")) : LOG_INFO(F("Device status cleared"));
    }
    else
    {
        LOG_DEBUG(F("No device status bit set"));
    }
}

void OXRS_SEN5x::onCommand(JsonVariant json)
{
    if (json.containsKey(RESET_COMMAND) && json[RESET_COMMAND].as<bool>()) {
        resetSensor();
        return;
    }

    if (json.containsKey(FANCLEAN_COMMAND) && json[FANCLEAN_COMMAND].as<bool>()) {
        fanClean();
        return;
    }

    if (json.containsKey(CLEAR_DEVICESTATUS_COMMAND) && json[CLEAR_DEVICESTATUS_COMMAND].as<bool>()) {
        clearDeviceStatus();
        return;
    }
}

void OXRS_SEN5x::setCommandSchema(JsonVariant command)
{
    JsonObject resetSensor = command.createNestedObject(RESET_COMMAND);
    resetSensor["title"]   = "Reset SEN5x Sensor";
    resetSensor["type"]    = "boolean";

    JsonObject fanClean = command.createNestedObject(FANCLEAN_COMMAND);
    fanClean["title"]   = "Start Fan Cleaning (default is weekly)";
    fanClean["type"]    = "boolean";

    JsonObject clearDevice = command.createNestedObject(CLEAR_DEVICESTATUS_COMMAND);
    clearDevice["title"]   = "Clear DeviceStatus Register";
    clearDevice["type"]    = "boolean";
}

String OXRS_SEN5x::getModelName() const
{
    // FIXME: autodetect via product and serial number, but for now
    switch (_model)
    {
    case SEN5x_model_t::SEN50:
        return "Sensirion SEN50-SDN-T";
    case SEN5x_model_t::SEN54:
        return "Sensirion SEN54-SDN-T";
    case SEN5x_model_t::SEN55:
        return "Sensirion SEN55-SDN-T";
    default:
        return "Unknown";
    }
}

void OXRS_SEN5x::setConfigSchema(JsonVariant config)
{
    JsonObject model     = config.createNestedObject("model");
    model["title"]       = "Model";
    model["description"] = "Connected air quality sensor model.";
    model["type"]        = "string";
    model["default"]     = getModelName();
    model["readOnly"]    = true;

    JsonObject publishTelemetry = config.createNestedObject(PUBLISH_TELEMETRY_FREQ_CONFIG);
    publishTelemetry["title"]   = "Publish Telemetry Frequency (seconds)";
    publishTelemetry["description"] =
        "How often to publish telemetry from the air quality sensor \
(setting to 0 disables telemetry capture). Must be a number between 0 and 86400 (i.e. 1 day).";
    publishTelemetry["type"]    = "integer";
    publishTelemetry["minimum"] = 0;
    publishTelemetry["maximum"] = 86400;
    publishTelemetry["default"] = (uint32_t)_publishTelemetry_ms / 1000.0;

    if (_model != SEN50)
    {
        JsonObject temperatureOffset = config.createNestedObject(TEMPERATURE_OFFSET_CONFIG);
        temperatureOffset["title"]   = "Temperature Offset (°C)";
        temperatureOffset["description"] = "Temperature offset in Celsuis. Default 0. Must be a number between -10 and 10.";
        temperatureOffset["type"]    = "integer";
        temperatureOffset["minimum"] = -10;
        temperatureOffset["maximum"] = 10;
        temperatureOffset["default"] = _tempOffset_celsius;
    }

    /*    JsonObject lastFanClean = config.createNestedObject("lastFanClean");
        lastFanClean["title"] = "Last Fan Clean";
        lastFanClean["description"] = "Datetime of last fan clean.";
        lastFanClean["type"] = "string";
        lastFanClean["format"] = "date-time";
        lastFanClean["readOnly"] = "true";*/
}

void OXRS_SEN5x::logError(Error_t error, const __FlashStringHelper *s)
{
    char errorMessage[256];
    errorToString(error, errorMessage, 256);
    LOGF_ERROR("%s %s", s, errorMessage);
}
