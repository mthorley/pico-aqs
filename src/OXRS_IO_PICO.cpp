/**
 * OXRS-IO-Generic-PICOW-LIB
 * TODO: Confirm naming convention here.
 */

#include "hardware/watchdog.h"
#include <WiFi.h>
#include <LittleFS.h>
#include <OXRS_MQTT.h>
#include "OXRS_API.h"
#include <OXRS_LOG.h>
#include "OXRS_IO_PICO.h"

// #define __WATCHDOG

#include "Credentials.h" // FIXME:

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
OXRS_LOG::MQTTLogger _mqttLogger(_mqttClient, "log"); // Logging (topic updated once MQTT connects successfully)
OXRS_LOG::SysLogger  _sysLogger("picow", "192.168.3.26");

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
    // Process log config changes
    JsonVariant jvLoglevel = OXRS_IO_PICO::findNestedKey(json, "loglevel");
    if (!jvLoglevel.isNull()) {
        String sLoglevel(jvLoglevel.as<String>().c_str());
        OXRS_IO_PICO::setLogLevelComamnd(sLoglevel);
    }

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

OXRS_IO_PICO::OXRS_IO_PICO() 
{

};

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

// FIXME: rely on wifi manager and config
void network_connect()
{
    int status;
    while (status != WL_CONNECTED)
    {

        LOGF_DEBUG("Attempting to connect to WPA SSID: %s", WIFI_SSID);

        // Connect to WPA/WPA2 network:
        status = WiFi.begin(WIFI_SSID, WIFI_PASSWORD); // Set these credentials

        // wait to connect:
        delay(5000);
    }

    if (status == WL_CONNECTED)
    {
        LOGF_INFO("Connected to WPA SSID: %s", WIFI_SSID);
    }

    LOGF_INFO("Device IPv4: %s MAC: %s", WiFi.localIP().toString().c_str(), WiFi.macAddress().c_str());
}

void OXRS_IO_PICO::initialiseNetwork(byte *mac)
{

    LOG_DEBUG(F("initialiseNetwork"));
    /*  WiFi.macAddress(mac);

      // Format the MAC address for logging
      char mac_display[18];
      sprintf_P(mac_display, PSTR("%02X:%02X:%02X:%02X:%02X:%02X"), mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
      _logger.println(mac_display);

      WiFi.mode(WIFI_STA);*/

    // FIXME:
    /*  WiFiManager wm;

      wm.setConfigPortalTimeout(WM_CONFIG_PORTAL_TIMEOUT_S);

      if (!wm.autoConnect("OXRS_WiFi", "superhouse")) {
        _logger.println("Failed to connect");
      }

      IPAddress ipAddress = WiFi.localIP();
    */
    network_connect();
}

void OXRS_IO_PICO::loop()
{
    // check network connection
    if (isNetworkConnected())
    {
        // handle mqtt messages
        _mqtt.loop();

        // handle api requests
        WiFiClient client = _server.available();
        _api.loop(&client);
    }

#ifdef __WATCHDOG
    watchdog_update();
#endif
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

void OXRS_IO_PICO::getConfigSchemaLogging(JsonVariant json)
{
    // Log level
    JsonObject logConfig = json.createNestedObject("logging");
    logConfig["title"]   = "Logging";
    JsonObject logProps  = logConfig.createNestedObject("properties");

    JsonObject logLevel  = logProps.createNestedObject("loglevel");
    logLevel["title"]    = "Log Level";
    logLevel["default"]  = oxrsLog.getLevel();

    JsonArray logEnum = logLevel.createNestedArray("enum");
    logEnum.add("DEBUG");
    logEnum.add("INFO");
    logEnum.add("WARN");
    logEnum.add("ERROR");
    logEnum.add("FATAL");
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
    getConfigSchemaLogging(props);
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

void OXRS_IO_PICO::setLogLevelCommand(const String& sLogLevel)
{
    if (sLogLevel=="DEBUG")
        oxrsLog.setLevel(OXRS_LOG::LogLevel_t::DEBUG);
    if (sLogLevel=="INFO")
        oxrsLog.setLevel(OXRS_LOG::LogLevel_t::INFO);
    if (sLogLevel=="WARN")
        oxrsLog.setLevel(OXRS_LOG::LogLevel_t::WARN);
    if (sLogLevel=="ERROR")
        oxrsLog.setLevel(OXRS_LOG::LogLevel_t::ERROR);
    if (sLogLevel=="FATAL")
        oxrsLog.setLevel(OXRS_LOG::LogLevel_t::FATAL);
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

void OXRS_IO_PICO::begin(jsonCallback config, jsonCallback command)
{
// FIXME: depends on config
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
}
