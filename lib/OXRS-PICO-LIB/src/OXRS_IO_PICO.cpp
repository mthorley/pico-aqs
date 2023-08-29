/**
 * OXRS-IO-Generic-PICOW-LIB
 * TODO: Confirm naming convention here.
 */

#include "hardware/watchdog.h"
#include "hardware/adc.h"
#include <WiFi.h>
#include <LittleFS.h>
#include <OXRS_MQTT.h>
#include <OXRS_API.h>
#include <OXRS_LOG.h>
#include <OXRS_IO_PICO.h>
#include <OXRS_TIME.h>
#include "WiFIManager.h"

// #define __WATCHDOG

static const char *_LOG_PREFIX = "[OXRS_IO_PICO] ";

// Supported firmware config and command schemas
DynamicJsonDocument _fwConfigSchema(JSON_CONFIG_MAX_SIZE);
DynamicJsonDocument _fwCommandSchema(JSON_CONFIG_MAX_SIZE);

// OXRS
WiFiServer _server(80);
WiFiClient _client;

// MQTT client
PubSubClient _mqttClient(_client);
OXRS_MQTT    _mqtt(_mqttClient);

// REST API
OXRS_API _api(_mqtt);

// Logging
OXRS_LOG::MQTTLogger _mqttLogger(_mqttClient);   // Logging (topic updated once MQTT connects successfully)
OXRS_LOG::SysLogger  _sysLogger;                 // Updated on config

// Time
OXRS_TIME oxrsTime;

// MQTT callbacks wrapped by _mqttConfig/_mqttCommand
jsonCallback _onConfig;
jsonCallback _onCommand;

void _apiAdoptCallback(JsonVariant json)
{
    OXRS_IO_PICO::apiAdoptCallback(json);
}

void _mqttConnected()
{
    // update log topic
    static char logTopic[64];
    _mqttLogger.setTopic(_mqtt.getLogTopic(logTopic));

    DynamicJsonDocument json(JSON_ADOPT_MAX_SIZE);
    _mqtt.publishAdopt(_api.getAdopt(json.as<JsonVariant>()));

    LOG_INFO(F("mqtt connected"));
}

void _mqttDisconnected(int state)
{
    // Log the disconnect reason
    // See https://github.com/knolleary/pubsubclient/blob/2d228f2f862a95846c65a8518c79f48dfc8f188c/src/PubSubClient.h#L44
    switch (state)
    {
    case MQTT_CONNECTION_TIMEOUT:
        LOG_DEBUG(F("mqtt connection timeout"));
        break;
    case MQTT_CONNECTION_LOST:
        LOG_DEBUG(F("mqtt connection lost"));
        break;
    case MQTT_CONNECT_FAILED:
        LOG_DEBUG(F("mqtt connect failed"));
        break;
    case MQTT_DISCONNECTED:
        LOG_DEBUG(F("mqtt disconnected"));
        break;
    case MQTT_CONNECT_BAD_PROTOCOL:
        LOG_DEBUG(F("mqtt bad protocol"));
        break;
    case MQTT_CONNECT_BAD_CLIENT_ID:
        LOG_DEBUG(F("mqtt bad client id"));
        break;
    case MQTT_CONNECT_UNAVAILABLE:
        LOG_DEBUG(F("mqtt unavailable"));
        break;
    case MQTT_CONNECT_BAD_CREDENTIALS:
        LOG_DEBUG(F("mqtt bad credentials"));
        break;
    case MQTT_CONNECT_UNAUTHORIZED:
        LOG_DEBUG(F("mqtt unauthorised"));
        break;
    }
}

void _mqttConfig(JsonVariant json)
{
    // parse any logging config updates
    oxrsLog.onConfig(json);

    // Pass on to the firmware callback
    if (_onConfig)
    {
        _onConfig(json);
    }
}

void _mqttCommand(JsonVariant json)
{
    // Core restart command
    if (json.containsKey(OXRS_IO_PICO::RESTART_COMMAND) && json[OXRS_IO_PICO::RESTART_COMMAND].as<bool>())
    {
        rp2040.restart();
    }

    // Pass on to the firmware callback
    if (_onCommand)
    {
        _onCommand(json);
    }
}

void _mqttCallback(char *topic, uint8_t *payload, unsigned int length)
{
    int state = _mqtt.receive(topic, payload, length);
    switch (state)
    {
    case MQTT_RECEIVE_ZERO_LENGTH:
        LOG_DEBUG(F("empty mqtt payload received"));
        break;
    case MQTT_RECEIVE_JSON_ERROR:
        LOG_ERROR(F("failed to deserialise mqtt json payload"));
        break;
    case MQTT_RECEIVE_NO_CONFIG_HANDLER:
        LOG_DEBUG(F("no mqtt config handler"));
        break;
    case MQTT_RECEIVE_NO_COMMAND_HANDLER:
        LOG_DEBUG(F("no mqtt command handler"));
        break;
    }
}

OXRS_IO_PICO::OXRS_IO_PICO(bool useOnBoardTempSensor=false) : 
    _useOnBoardTempSensor(useOnBoardTempSensor) {};

void OXRS_IO_PICO::initialiseMqtt(byte *mac)
{
    // NOTE: this must be called *before* initialising the REST API since
    // that will load MQTT config from file, which has precendence
    LOG_DEBUG(F("initialiseMqtt"));

    // Set the default ClientID to last 3 bytes of the MAC address
    char clientId[32];
    sprintf_P(clientId, PSTR("%02x%02x%02x"), mac[3], mac[4], mac[5]);
    _mqtt.setClientId(clientId);

    // Register callbacks
    _mqtt.onConnected(_mqttConnected);
    _mqtt.onDisconnected(_mqttDisconnected);
    _mqtt.onConfig(_mqttConfig);
    _mqtt.onCommand(_mqttCommand);

    // start listening for MQTT messages
    _mqttClient.setCallback(_mqttCallback);
}

void OXRS_IO_PICO::initialiseRestApi()
{
    // NOTE: this must be called *after* initialising MQTT since that sets
    // the default client id, which has lower precedence that MQTT
    // settings stored in file and loaded by the API
    LOG_DEBUG(F("initialiseRestApi"));

    // Set up REST API
    _api.begin();

    // Register callbacks
    _api.onAdopt(_apiAdoptCallback);

    // Start listenging
    _server.begin();
}

boolean OXRS_IO_PICO::isNetworkConnected()
{
    return WiFi.status() == WL_CONNECTED;
}

void OXRS_IO_PICO::initialiseNetwork(byte *mac)
{
    LOG_DEBUG(F("initialiseNetwork"));
    WiFi.macAddress(mac);

    // Format the MAC address for logging
    char mac_display[18];
    sprintf_P(mac_display, PSTR("%02X:%02X:%02X:%02X:%02X:%02X"), mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    LOG_INFO(mac_display);

    // Ensure we are in the correct WiFi mode
//    WiFi.mode(WIFI_STA);

    // Connect using saved creds, or start captive portal if none found
    // NOTE: Blocks until connected or the portal is closed
    WiFiManager wm;
    bool success = wm.autoConnect("OXRS_WiFi", "superhouse");
    String s = success ? WiFi.localIP().toString() : IPAddress(0, 0, 0, 0).toString();
    LOGF_INFO("network %s", s.c_str());
}

void OXRS_IO_PICO::initialiseTempSensor()
{
    if (!_useOnBoardTempSensor)
        return;

    // Initialize hardware AD converter, enable onboard temperature sensor and
    // select its channel (do this once for efficiency, but beware that this
    // is a global operation).
    adc_init();
    adc_set_temp_sensor_enabled(true);
    adc_select_input(4);
}

/* 
 * Get onboard temperature of Pico via ADC.
 * References for this implementation: raspberry-pi-pico-c-sdk.pdf, Section '4.1.1. hardware_adc'
 * pico-examples/adc/adc_console/adc_console.c 
 */
float OXRS_IO_PICO::readOnboardTemperature(bool celsiusNotFahr)
{
    /* 12-bit conversion, assume max value == ADC_VREF == 3.3 V */
    const float conversionFactor = 3.3f / (1 << 12);

    float adc = (float)adc_read() * conversionFactor;
    float tempC = 27.0f - (adc - 0.706f) / 0.001721f;

    return celsiusNotFahr ? tempC : tempC * 9 / 5 + 32;
}

void OXRS_IO_PICO::loop()
{
    // check network connection
    if (isNetworkConnected())
    {
        // handle mqtt messages
        int res = _mqtt.loop();
        if (ISLOG_DEBUG) 
        {
            if (res == MQTT_RECONNECT_FAILED)
                LOG_DEBUG(F("Mqtt reconnect failed"));
            if (res == MQTT_RECONNECT_BACKING_OFF)
                LOG_DEBUG(F("Mqtt reconnect backing off"));
        }

        // handle api requests
        WiFiClient client = _server.available();
        _api.loop(&client);
    }

#ifdef __WATCHDOG
    watchdog_update();
#endif
}

OXRS_MQTT* OXRS_IO_PICO::getMQTT()
{
    return &_mqtt;
}

void OXRS_IO_PICO::publishTelemetry(JsonVariant telemetry)
{
    // publish something to OXRS_
    if (isNetworkConnected)
    {
        _mqtt.publishTelemetry(telemetry);
    }
}

// json helper
void OXRS_IO_PICO::mergeJson(JsonVariant dst, JsonVariantConst src)
{
    if (src.is<JsonObjectConst>())
    {
        for (JsonPairConst kvp : src.as<JsonObjectConst>()) {
            if (dst[kvp.key()]) {
                mergeJson(dst[kvp.key()], kvp.value());
            }
            else {
                dst[kvp.key()] = kvp.value();
            }
        }
    }
    else
    {
        dst.set(src);
    }
}

JsonVariant OXRS_IO_PICO::findNestedKey(JsonObject obj, const String &key)
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

void OXRS_IO_PICO::getFirmwareJson(JsonVariant json)
{
    JsonObject firmware = json.createNestedObject("firmware");

    firmware["name"]      = FW_NAME;
    firmware["shortName"] = FW_SHORT_NAME;
    firmware["maker"]     = FW_MAKER;
    firmware["version"]   = FW_VERSION;

#if defined(FW_GITHUB_URL)
    firmware["githubUrl"] = FW_GITHUB_URL;
#endif
}

void OXRS_IO_PICO::getSystemJson(JsonVariant json)
{
    JsonObject system = json.createNestedObject("system");

    system["heapUsedBytes"]      = rp2040.getUsedHeap();
    system["heapFreeBytes"]      = rp2040.getFreeHeap();
    system["heapMaxAllocBytes"]  = rp2040.getTotalHeap();
    system["flashChipSizeBytes"] = PICO_FLASH_SIZE_BYTES;

    // FIXME:
    system["sketchSpaceUsedBytes"]  = 0;
    system["sketchSpaceTotalBytes"] = 0;

    FSInfo64 fs;
    LittleFS.info64(fs);
    system["fileSystemUsedBytes"]  = fs.usedBytes;
    system["fileSystemTotalBytes"] = fs.totalBytes;
}

void OXRS_IO_PICO::getNetworkJson(JsonVariant json)
{
    JsonObject network = json.createNestedObject("network");

    network["mode"]    = "wifi";
    network["ip"]      = WiFi.localIP();
    network["rssi"]    = WiFi.RSSI();
    network["channel"] = WiFi.channel();
    // TODO: network["hostname"] = WiFi.hostname();

    byte mac[6];
    WiFi.macAddress(mac);
    char mac_display[18];
    sprintf_P(mac_display, PSTR("%02X:%02X:%02X:%02X:%02X:%02X"), mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    network["mac"] = mac_display;
}

void OXRS_IO_PICO::getConfigSchemaJson(JsonVariant json)
{
    JsonObject configSchema = json.createNestedObject("configSchema");

    // Config schema metadata
    configSchema["$schema"] = JSON_SCHEMA_VERSION;
    configSchema["title"]   = FW_SHORT_NAME;
    configSchema["type"]    = "object";

    JsonObject props = configSchema.createNestedObject("properties");

    // Firmware config schema (if any)
    if (!_fwConfigSchema.isNull())
    {
        mergeJson(props, _fwConfigSchema.as<JsonVariant>());
    }

    // Append other config
    oxrsLog.setConfig(props);
    oxrsTime.setConfig(props);
}

void OXRS_IO_PICO::setConfigSchema(JsonVariant json)
{
    _fwConfigSchema.clear();
    mergeJson(_fwConfigSchema.as<JsonVariant>(), json);
}

void OXRS_IO_PICO::getCommandSchemaJson(JsonVariant json)
{
    JsonObject commandSchema = json.createNestedObject("commandSchema");

    // Command schema metadata
    commandSchema["$schema"] = JSON_SCHEMA_VERSION;
    commandSchema["title"]   = FW_SHORT_NAME;
    commandSchema["type"]    = "object";

    JsonObject properties = commandSchema.createNestedObject("properties");

    // Firware command schema (if any)
    if (!_fwCommandSchema.isNull())
    {
        OXRS_IO_PICO::mergeJson(properties, _fwCommandSchema.as<JsonVariant>());
    }

    // Restart command
    JsonObject restart = properties.createNestedObject(RESTART_COMMAND);
    restart["title"]   = "Restart Pico";
    restart["type"]    = "boolean";
}

void OXRS_IO_PICO::setCommandSchema(JsonVariant json)
{
    _fwCommandSchema.clear();
    mergeJson(_fwCommandSchema.as<JsonVariant>(), json);
}

void OXRS_IO_PICO::apiAdoptCallback(JsonVariant json)
{
    getFirmwareJson(json);
    getSystemJson(json);
    getNetworkJson(json);
    getConfigSchemaJson(json);
    getCommandSchemaJson(json);
}

void OXRS_IO_PICO::initialiseWatchdog()
{
#ifdef __WATCHDOG
    if (watchdog_caused_reboot())
    {
        LOG_INFO(F("Watchdog caused reboot"));
        // update state.json file with datetime and reboot cause and inc reboot count
    }

    // Enable the watchdog, requiring the watchdog to be updated every ~8s or the chip will reboot
    watchdog_enable(0x7FFFFF, 1);
#endif
}

void OXRS_IO_PICO::initialiseTime()
{
    oxrsTime.begin();
}

void OXRS_IO_PICO::begin(jsonCallback config, jsonCallback command)
{
    // add MQTT and other logging
    oxrsLog.addLogger(&_sysLogger);
    oxrsLog.addLogger(&_mqttLogger);

    LOG_DEBUG(F("begin"));

    // get our firmware details
    DynamicJsonDocument json(256);
    getFirmwareJson(json.as<JsonVariant>());

    // log firmware details
    String jsonAsString;
    serializeJson(json, jsonAsString);
    LOGF_DEBUG("%s", jsonAsString.c_str());

    // wrap the callbacks so we can intercept messages
    // intended for the underlying device or sensor
    _onConfig = config;
    _onCommand = command;

    // setup network and obtain an IP address
    byte mac[6];
    initialiseNetwork(mac);

    // setup MQTT (dont attempt to connect yet)
    initialiseMqtt(mac);

    // setup the REST API
    initialiseRestApi();

    // setup watchdog
    initialiseWatchdog();

    // setup onboard temp sensor
    initialiseTempSensor();

    // setup ntp/tz
    initialiseTime();
}
