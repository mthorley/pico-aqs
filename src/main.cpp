#include <Arduino.h>
#include <WiFi.h>
#include <OXRS_IO_PICO.h>
#include <OXRS_LOG.h>
#include <OXRS_HASS.h>
#include <OXRS_SEN5x.h>

//#define __TESTING

#ifdef __TESTING
#include <WiFIManager.h>
WiFiManager wm;
#endif

// OXRS layer
bool usePicoOnboardTempSensor = true;
OXRS_IO_PICO oxrsPico(usePicoOnboardTempSensor);

// Sensirion air quality sensor
OXRS_SEN5x oxrsSen5x(SEN5x_model_t::SEN55);

// Home assistant discovery config
OXRS_HASS hass(oxrsPico.getMQTT());

// Publish Home Assistant self-discovery config for each sensor
bool hassDiscoveryPublished = false;

// Log prefix for this class
static const char *_LOG_PREFIX = "[main] ";

void jsonConfig(JsonVariant json)
{
    oxrsSen5x.onConfig(json);
 
    // Handle any Home Assistant config
    hass.parseConfig(json);

    LOG_DEBUG(F("jsonConfig complete"));
}

void setConfigSchema()
{
    // Define our config schema
    StaticJsonDocument<4096> json;
    JsonVariant config = json.as<JsonVariant>();

    // Get config schema for Sensirion sensor
    oxrsSen5x.setConfigSchema(config);

    // Add any Home Assistant config
    hass.setConfigSchema(json);

    // Pass our config schema down to the PICO library
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

    // Get command schema for Sensirion sensor
    oxrsSen5x.setCommandSchema(commands);

    // Pass our command schema down to the PICO library
    oxrsPico.setCommandSchema(commands);
}

// FIXME: Move this to OXRS_SEN5x_LIB
void publishHassDiscovery()
{
    if (hassDiscoveryPublished)
        return;

    char topic[64];

    char component[8];
    sprintf(component, "sensor");

    DynamicJsonDocument json(4096);

    char id[32];
    sprintf(id, "temperature");
    hass.getDiscoveryJson(json, id);
    json["name"]         = "Temperature";
    json["dev_cla"]      = "temperature";
    json["unit_of_meas"] = "°C";
    json["stat_t"]       = oxrsPico.getMQTT()->getTelemetryTopic(topic);
    json["val_tpl"]      = "{{ value_json.temp }}";
    json["frc_upd"]      = true;
    hass.publishDiscoveryJson(json, component, id);

    sprintf(id, "humidity");
    hass.getDiscoveryJson(json, id);
    json["name"]         = "Humidity";
    json["dev_cla"]      = "humidity";
    json["unit_of_meas"] = "%";
    json["stat_t"]       = oxrsPico.getMQTT()->getTelemetryTopic(topic);
    json["val_tpl"]      = "{{ value_json.hum }}";
    json["frc_upd"]      = true;
    hass.publishDiscoveryJson(json, component, id);

    sprintf(id, "nitrous_oxide");
    hass.getDiscoveryJson(json, id);
    json["name"]         = "Nitrous Oxide";
    json["dev_cla"]      = "nitrous_oxide";
    json["unit_of_meas"] = "ug/m3";
    json["stat_t"]       = oxrsPico.getMQTT()->getTelemetryTopic(topic);
    json["val_tpl"]      = "{{ value_json.nox }}";
    json["frc_upd"]      = true;
    hass.publishDiscoveryJson(json, component, id);

    sprintf(id, "volatile_organic_compounds");
    hass.getDiscoveryJson(json, id);
    json["name"]         = "Volatile Organic Compounds";
    json["dev_cla"]      = "volatile_organic_compounds";
    json["unit_of_meas"] = "ug/m3";
    json["stat_t"]       = oxrsPico.getMQTT()->getTelemetryTopic(topic);
    json["val_tpl"]      = "{{ value_json.vox }}";
    json["frc_upd"]      = true;
    hass.publishDiscoveryJson(json, component, id);

    sprintf(id, "pm1");
    hass.getDiscoveryJson(json, id);
    json["name"]         = "Particulate Matter pm1.0";
    json["dev_cla"]      = "pm1";
    json["unit_of_meas"] = "ug/m3";
    json["stat_t"]       = oxrsPico.getMQTT()->getTelemetryTopic(topic);
    json["val_tpl"]      = "{{ value_json.pm1p0 }}";
    json["frc_upd"]      = true;
    hass.publishDiscoveryJson(json, component, id);

    sprintf(id, "pm25");
    hass.getDiscoveryJson(json, id);
    json["name"]         = "Particulate Matter pm2.5";
    json["dev_cla"]      = "pm25";
    json["unit_of_meas"] = "ug/m3";
    json["stat_t"]       = oxrsPico.getMQTT()->getTelemetryTopic(topic);
    json["val_tpl"]      = "{{ value_json.pm2p5 }}";
    json["frc_upd"]      = true;
    hass.publishDiscoveryJson(json, component, id);

    sprintf(id, "pm10");
    hass.getDiscoveryJson(json, id);
    json["name"]         = "Particulate Matter pm10.0";
    json["dev_cla"]      = "pm10";
    json["unit_of_meas"] = "ug/m3";
    json["stat_t"]       = oxrsPico.getMQTT()->getTelemetryTopic(topic);
    json["val_tpl"]      = "{{ value_json.pm10p0 }}";
    json["frc_upd"]      = true;

    // Only publish once on boot
    hassDiscoveryPublished = hass.publishDiscoveryJson(json, component, id);
    LOG_DEBUG(F("haas published"));
}

#ifndef __TESTING
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

    // setup Sensirion AQS
    oxrsSen5x.begin(Wire);
}

void loop()
{
    oxrsPico.loop();

    oxrsSen5x.loop();

    DynamicJsonDocument telemetry(4096);
    oxrsSen5x.getTelemetry(telemetry.as<JsonVariant>());
    if (telemetry.size() > 0)
    {
        oxrsPico.publishTelemetry(telemetry);
        if (usePicoOnboardTempSensor) {
            float temperature = oxrsPico.readOnboardTemperature();
            LOGF_INFO("Pico onboard temperature = %.02f %c", temperature, 'C');
        }
    }

    // Check if we need to publish any Home Assistant discovery payloads
    if (hass.isDiscoveryEnabled())
    {
        publishHassDiscovery();
    }
}
#endif

#ifdef __TESTING

void setup() {

    wifi_credentials_t creds;
    strcpy(creds.ssid, "halfife");
    strcpy(creds.pwd, "secret");
//    wm.writeCredentials(creds);

const char *ssid = "OXRS_WiFi";
const char *password = "superhouse";
    wm.startConfigPortal(ssid, password);
}

void loop() {

    bool doOnce = true;

    if (!doOnce) {
        wifi_credentials_t creds;
        wm.readCredentials(creds);
        Serial.println(creds.ssid);
        Serial.println(creds.pwd);
        doOnce = true;
    }
}

#endif
