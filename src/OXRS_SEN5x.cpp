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
        logError(error, "Error trying to execute deviceReset():");
    }

// Print SEN55 module information if i2c buffers are large enough
#ifdef USE_PRODUCT_INFO
    printSerialNumber();
    printModuleVersions();
#endif

    // Adjust tempOffset to account for additional temperature offsets
    // exceeding the SEN module's self heating.
    setTemperatureOffset();

    // Start Measurement
    error = _sensor.startMeasurement();
    if (error) {
        logError(error, "Error trying to execute startMeasurement():");
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
        logError(error, "Failed to getTemperatureOffsetSimple");
    }
    else {
        if (current_offset != _tempOffset_celsius) {
            setTemperatureOffset();
        }
    }
}

void OXRS_SEN5x::setTemperatureOffset() {
    Error_t error = _sensor.setTemperatureOffsetSimple(_tempOffset_celsius);
    if (error) {
        logError(error, "Error trying to execute setTemperatureOffsetSimple():");
    } else {
        LOGF_DEBUG("Set temperature offset: %.02f celsius (SEN54/55 only)", _tempOffset_celsius);
    }
}

void OXRS_SEN5x::logTelemetry(SEN5x_telemetry_t& t)
{
    LOGF_DEBUG("MassConcentrationPm1p0:%.02f, MassConcentrationPm2p5:%.02f, MassConcentrationPm4p0:%.02f, MassConcentrationPm10p0:%.02f",
        t.pm1p0, t.pm2p5, t.pm4p0, t.pm10p0);
}

void OXRS_SEN5x::printToSerial(SEN5x_telemetry_t& t)
{
       Serial.print("MassConcentrationPm1p0:");
        Serial.print(t.pm1p0);
        Serial.print("\t");
        Serial.print("MassConcentrationPm2p5:");
        Serial.print(t.pm2p5);
        Serial.print("\t");
        Serial.print("MassConcentrationPm4p0:");
        Serial.print(t.pm4p0);
        Serial.print("\t");
        Serial.print("MassConcentrationPm10p0:");
        Serial.print(t.pm10p0);
        Serial.print("\t");
        Serial.print("AmbientHumidity:");
        if (isnan(t.humidityPercent)) {
            Serial.print("n/a");
        } else {
            Serial.print(t.humidityPercent);
        }
        Serial.print("\t");
        Serial.print("AmbientTemperature:");
        if (isnan(t.tempCelsuis)) {
            Serial.print("n/a");
        } else {
            Serial.print(t.tempCelsuis);
        }
        Serial.print("\t");
        Serial.print("VocIndex:");
        if (isnan(t.vocIndex)) {
            Serial.print("n/a");
        } else {
            Serial.print(t.vocIndex);
        }
        Serial.print("\t");
        Serial.print("NoxIndex:");
        if (isnan(t.noxIndex)) {
            Serial.println("n/a");
        } else {
            Serial.println(t.noxIndex);
        } 
}

void OXRS_SEN5x::getTelemetry(JsonVariant json)
{
  // Do not publish if telemetry has been disabled
  if (_publishTelemetry_ms == 0)
    return;

  // Check if time passed is enough to publish
  if ((millis() - _lastPublishTelemetry_ms) > _publishTelemetry_ms)
  {
    LOG_DEBUG(F("Measuring"));
    SEN5x_telemetry_t t;
    Error_t error = getMeasurements(t);
    if (error) {
        logError(error, "Failed to get measurements");
    } 
    else {
      const char* fmt = "%.2f";
      char buf[50];
      sprintf(buf, fmt, t.pm1p0); json["pm1p0"] = buf;
      sprintf(buf, fmt, t.pm2p5); json["pm2p5"] = buf;
      sprintf(buf, fmt, t.pm4p0); json["pm4p0"]  = buf;
      sprintf(buf, fmt, t.pm10p0); json["pm10p0"] = buf;
      sprintf(buf, fmt, t.humidityPercent); json["hum"] = buf;
      sprintf(buf, fmt, t.tempCelsuis); json["temp"] = buf;
      sprintf(buf, fmt, t.vocIndex); json["vox"] = buf;
      sprintf(buf, fmt, t.noxIndex); json["nox"] = buf;
      printToSerial(t);
    }

    // Reset timer
    _lastPublishTelemetry_ms = millis();

    checkDeviceStatus();
  }
}

void OXRS_SEN5x::printModuleVersions() {

    unsigned char productName[32];
    uint8_t productNameSize = 32;

    Error_t error = _sensor.getProductName(productName, productNameSize);

    if (error) {
        logError(error, "Error trying to execute getProductName(): ");
    } else {
        Serial.print("ProductName:");
        Serial.println((char*)productName);
    }

    uint8_t firmwareMajor;
    uint8_t firmwareMinor;
    bool firmwareDebug;
    uint8_t hardwareMajor;
    uint8_t hardwareMinor;
    uint8_t protocolMajor;
    uint8_t protocolMinor;

    error = _sensor.getVersion(firmwareMajor, firmwareMinor, firmwareDebug,
                             hardwareMajor, hardwareMinor, protocolMajor,
                             protocolMinor);
    if (error) {
        logError(error, "Error trying to execute getVersion(): ");
    } else {
        Serial.print("Firmware: ");
        Serial.print(firmwareMajor);
        Serial.print(".");
        Serial.print(firmwareMinor);
        Serial.print(", ");

        Serial.print("Hardware: ");
        Serial.print(hardwareMajor);
        Serial.print(".");
        Serial.println(hardwareMinor);
    }
}

void OXRS_SEN5x::printSerialNumber() {
    uint16_t error;
    char errorMessage[256];
    unsigned char serialNumber[32];
    uint8_t serialNumberSize = 32;

    error = _sensor.getSerialNumber(serialNumber, serialNumberSize);
    if (error) {
        logError(error, "Error trying to execute getSerialNumber(): ");
    } else {
        Serial.print("SerialNumber:");
        Serial.println((char*)serialNumber);
    }
}

// check device status register
void OXRS_SEN5x::checkDeviceStatus() 
{
    // read device status register values
    uint32_t reg;
    _sensor.readDeviceStatus(reg);

    // parse register bits
    SEN5xDeviceStatus ds(SEN5xDeviceStatus::device_t::sen55_sdn_t); // FIXME: autodetect this
    ds.setRegister(reg);
    if (ds.hasIssue()) {
        String msg;
        ds.getAllMessages(msg);
        Serial.print(msg);
    }
    else {
        LOG_DEBUG(F("Device status ok"));
    }
}

void OXRS_SEN5x::onConfig(JsonVariant json) 
{
    if (json.containsKey(PUBLISH_TELEMETERY_SECONDS))
    {
        _publishTelemetry_ms = json[PUBLISH_TELEMETERY_SECONDS].as<uint32_t>() * 1000L;
        LOGF_INFO("Set config publish telemetry ms to %" PRIu32 "", _publishTelemetry_ms);
    }
    if (json.containsKey(TEMPERATURE_OFFSET))
    {
        _tempOffset_celsius = json[TEMPERATURE_OFFSET].as<float_t>();
        LOGF_INFO("Set config temperature offset degrees to %.02f", _tempOffset_celsius);
    }
}

void OXRS_SEN5x::setConfigSchema(JsonVariant config) 
{
  JsonObject publishTelemetry = config.createNestedObject(PUBLISH_TELEMETERY_SECONDS);
  publishTelemetry["title"] = "Publish Telemetry Frequency (seconds)";
  publishTelemetry["description"] = \
    "How often to publish telemetry from the air quality sensor attached to your device \
(defaults to 10 seconds, setting to 0 disables telemetry reports). \
Must be a number between 0 and 86400 (i.e. 1 day).";
  publishTelemetry["type"] = "integer";
  publishTelemetry["minimum"] = 0;
  publishTelemetry["maximum"] = 86400;

  JsonObject temperatureOffset = config.createNestedObject(TEMPERATURE_OFFSET);
  temperatureOffset["title"] = "Temperature offset (C)";
  temperatureOffset["description"] = \
    "Temperature offset in Celsuis. Default 0.";
  temperatureOffset["type"] = "integer";
  temperatureOffset["minimum"] = -10;
  temperatureOffset["maximum"] = 10;
}

void OXRS_SEN5x::logError(Error_t error, const char* s)
{
    char errorMessage[256];
    errorToString(error, errorMessage, 256);
    LOGF_ERROR("%s %s", s, errorMessage);
}
