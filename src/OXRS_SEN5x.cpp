#include "OXRS_SEN5x.h"
#include "SEN5xDeviceStatus.h"

void OXRS_SEN5x::begin(TwoWire& wire) {

    // assumes Wire.begin() has been called prior
    _sensor.begin(Wire);

    uint16_t error;
    char errorMessage[256];
    error = _sensor.deviceReset();
    if (error) {
        Serial.print("Error trying to execute deviceReset(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
    }

// Print SEN55 module information if i2c buffers are large enough
#ifdef USE_PRODUCT_INFO
    printSerialNumber();
    printModuleVersions();
#endif

    // set a temperature offset in degrees celsius
    // Note: supported by SEN54 and SEN55 sensors
    // By default, the temperature and humidity outputs from the sensor
    // are compensated for the modules self-heating. If the module is
    // designed into a device, the temperature compensation might need
    // to be adapted to incorporate the change in thermal coupling and
    // self-heating of other device components.
    //
    // A guide to achieve optimal performance, including references
    // to mechanical design-in examples can be found in the app note
    // “SEN5x – Temperature Compensation Instruction” at www.sensirion.com.
    // Please refer to those application notes for further information
    // on the advanced compensation settings used
    // in `setTemperatureOffsetParameters`, `setWarmStartParameter` and
    // `setRhtAccelerationMode`.
    //
    // Adjust tempOffset to account for additional temperature offsets
    // exceeding the SEN module's self heating.
    float tempOffset = 0.0;
    error = _sensor.setTemperatureOffsetSimple(tempOffset);
    if (error) {
        Serial.print("Error trying to execute setTemperatureOffsetSimple(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
    } else {
        Serial.print("Temperature Offset set to ");
        Serial.print(tempOffset);
        Serial.println(" deg. Celsius (SEN54/SEN55 only");
    }

    // Start Measurement
    error = _sensor.startMeasurement();
    if (error) {
        Serial.print("Error trying to execute startMeasurement(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
    }
}

uint16_t OXRS_SEN5x::measure(SEN5x_telemetry_t& t) 
{
    uint16_t error = _sensor.readMeasuredValues(
        t.pm1p0, t.pm2p5, t.pm4p0, t.pm10p0, t.humidityPercent, t.tempCelsuis, t.vocIndex, t.noxIndex);
    return error;
}

void OXRS_SEN5x::loop() 
{
    // must be done post begin
    float current_offset;
    _sensor.getTemperatureOffsetSimple(current_offset);
    if (current_offset != _tempOffset_celsius) {
        _sensor.setTemperatureOffsetSimple(_tempOffset_celsius);
        Serial.println("updated temp");
    }
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
    Serial.println("measure");
    SEN5x_telemetry_t t;
    uint16_t error = measure(t);
    const char* fmt = "%.2f";
    if (!error) {
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
    else {
      Serial.println("error");
    }
    // Reset timer
    _lastPublishTelemetry_ms = millis();

    checkDeviceStatus();
  }
}

void OXRS_SEN5x::printModuleVersions() {
    uint16_t error;
    char errorMessage[256];

    unsigned char productName[32];
    uint8_t productNameSize = 32;

    error = _sensor.getProductName(productName, productNameSize);

    if (error) {
        Serial.print("Error trying to execute getProductName(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
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
        Serial.print("Error trying to execute getVersion(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
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
        Serial.print("Error trying to execute getSerialNumber(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
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
    SEN5xDeviceStatus ds(SEN5xDeviceStatus::device_t::sen55_sdn_t);
    ds.setRegister(reg);
    if (ds.hasIssue()) {
        String msg;
        ds.getAllMessages(msg);
        Serial.print(msg);
    }
    else {
        Serial.println("ok");
    }
}

void OXRS_SEN5x::onConfig(JsonVariant json) 
{
    if (json.containsKey(PUBLISH_TELEMETERY_SECONDS))
    {
        _publishTelemetry_ms = json[PUBLISH_TELEMETERY_SECONDS].as<uint32_t>() * 1000L;
        Serial.println(_publishTelemetry_ms);
    }
    if (json.containsKey(TEMPERATURE_OFFSET))
    {
        _tempOffset_celsius = json[TEMPERATURE_OFFSET].as<int8_t>();
        Serial.println(_tempOffset_celsius);
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
