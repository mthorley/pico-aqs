/*

Refactoring steps:
- moving average for pmxyz measurements
- add commands reset device, fan cleaning etc
- Extend AQSSensiron to deal with Sensiron devices below Sen5x where telemetry is missing

*/
#include <Arduino.h>
#include <WiFi.h>

#include "HAIntegration.h"
#include "Credentials.h"

#include <OXRS_IO_PICO.h>
#include "OXRS_SEN5x.h"
#include "OXRS_LOG.h"

HAIntegration integration;

// Sensirion air quality sensor
OXRS_SEN5x oxrsSen5x;

static const char* _LOG_PREFIX = "[main] ";

void jsonConfig(JsonVariant json) 
{
  oxrsSen5x.onConfig(json);
  LOG_DEBUG(F("jsonConfig complete"));
}

// template
void setConfigSchema() 
{
  // Define our config schema
  StaticJsonDocument<4096> json;
  JsonVariant config = json.as<JsonVariant>();

  // get config schema for Sensirion sensor
  oxrsSen5x.setConfigSchema(config);

  // pass our config schema down to the PICO library
  OXRS_IO_PICO::setConfigSchema(config);
}

void jsonCommand(JsonVariant json) {
  oxrsSen5x.onCommand(json);
  LOG_DEBUG(F("jsonCommand complete"));
}

void setCommandSchema()
{
  // Define our command schema
  StaticJsonDocument<4096> json;
  JsonVariant commands = json.as<JsonVariant>();

  // get command schema for Sensirion sensor
  oxrsSen5x.setCommandSchema(commands);

  // pass our command schema down to the PICO library
  OXRS_IO_PICO::setCommandSchema(commands);
}

void setup()
{
  Serial.begin();

  Wire.begin();

  Logger().setLevel(OXRS_LOG::DEBUG);

  delay(250); //Give the serial terminal a chance to connect, if present

  // jsonConfig and jsonCommand are callbacks invoked when the admin API/UI updates
  OXRS_IO_PICO::begin(jsonConfig, jsonCommand);

  // set up config/command schema for self discovery and adoption
  setConfigSchema();
  setCommandSchema();

  // add any custom apis
  //addCustomApis();

  integration.configure();

  // setup Sensirion AQS
  oxrsSen5x.begin(Wire);
}

void loop()
{
  integration.loop();

  OXRS_IO_PICO::loop();

  oxrsSen5x.loop();

  DynamicJsonDocument telemetry(4096);
  oxrsSen5x.getTelemetry(telemetry.as<JsonVariant>());
  if (telemetry.size() > 0)
  {
      OXRS_IO_PICO::publishTelemetry(telemetry);
  }
}
