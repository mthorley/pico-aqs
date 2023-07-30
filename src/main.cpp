#include <Arduino.h>
#include <WiFi.h>

#include "HAIntegration.h"

#include <OXRS_IO_PICO.h>
#include <OXRS_LOG.h>
#include "OXRS_SEN5x.h"

HAIntegration integration;

// OXRS layer
OXRS_IO_PICO oxrsPico;

// Sensirion air quality sensor
OXRS_SEN5x oxrsSen5x(SEN5x_model_t::SEN55);

static const char *_LOG_PREFIX = "[main] ";

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
    oxrsPico.setConfigSchema(config);
}

void jsonCommand(JsonVariant json)
{
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
    oxrsPico.setCommandSchema(commands);
}

void setup()
{
    Serial.begin();

    Wire.begin();

    delay(250); // Give the serial terminal a chance to connect, if present

    // jsonConfig and jsonCommand are callbacks invoked when the admin API/UI updates
    oxrsPico.begin(jsonConfig, jsonCommand);

    // set up config/command schema for self discovery and adoption
    setConfigSchema();
    setCommandSchema();

    // add any custom apis
    // addCustomApis();

    integration.configure();

    // setup Sensirion AQS
    oxrsSen5x.begin(Wire);
}

void loop()
{
    integration.loop();

    oxrsPico.loop();

    oxrsSen5x.loop();

    DynamicJsonDocument telemetry(4096);
    oxrsSen5x.getTelemetry(telemetry.as<JsonVariant>());
    if (telemetry.size() > 0)
    {
        oxrsPico.publishTelemetry(telemetry);
    }
}
