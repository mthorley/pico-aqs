#include "OXRS_IO_PICO.h"

#include <WiFi.h>
#include <LittleFS.h>
#include <OXRS_MQTT.h>
#include <MqttLogger.h>
#include "OXRS_API.h"

#include "Credentials.h" // FIXME:

// Supported firmware config and command schemas
DynamicJsonDocument _fwConfigSchema(JSON_CONFIG_MAX_SIZE);
DynamicJsonDocument _fwCommandSchema(JSON_CONFIG_MAX_SIZE);

// OXRS
WiFiServer _server(80);
WiFiClient _client;

// MQTT client
PubSubClient _mqttClient(_client);
OXRS_MQTT _mqtt(_mqttClient);

// REST API
OXRS_API _api(_mqtt);

// Logging (topic updated once MQTT connects successfully)
MqttLogger _logger(_mqttClient, "log", MqttLoggerMode::MqttAndSerial);

// MQTT callbacks wrapped by _mqttConfig/_mqttCommand
jsonCallback _onConfig;
jsonCallback _onCommand;

void _apiAdoptCallback(JsonVariant json) {
  OXRS_IO_PICO::apiAdoptCallback(json);
}

void _mqttConnected() {
  // MqttLogger doesn't copy the logging topic to an internal
  // buffer so we have to use a static array here
  static char logTopic[64];
  _logger.setTopic(_mqtt.getLogTopic(logTopic));

  DynamicJsonDocument json(JSON_ADOPT_MAX_SIZE);
  _mqtt.publishAdopt(_api.getAdopt(json.as<JsonVariant>()));

  _logger.println("[pico] mqtt connected");
}

void _mqttDisconnected(int state) {
  // Log the disconnect reason
  // See https://github.com/knolleary/pubsubclient/blob/2d228f2f862a95846c65a8518c79f48dfc8f188c/src/PubSubClient.h#L44
  switch (state)
  {
  case MQTT_CONNECTION_TIMEOUT:
    _logger.println(F("[pico] mqtt connection timeout"));
    break;
  case MQTT_CONNECTION_LOST:
    _logger.println(F("[pico] mqtt connection lost"));
    break;
  case MQTT_CONNECT_FAILED:
    _logger.println(F("[pico] mqtt connect failed"));
    break;
  case MQTT_DISCONNECTED:
    _logger.println(F("[pico] mqtt disconnected"));
    break;
  case MQTT_CONNECT_BAD_PROTOCOL:
    _logger.println(F("[pico] mqtt bad protocol"));
    break;
  case MQTT_CONNECT_BAD_CLIENT_ID:
    _logger.println(F("[pico] mqtt bad client id"));
    break;
  case MQTT_CONNECT_UNAVAILABLE:
    _logger.println(F("[pico] mqtt unavailable"));
    break;
  case MQTT_CONNECT_BAD_CREDENTIALS:
    _logger.println(F("[pico] mqtt bad credentials"));
    break;
  case MQTT_CONNECT_UNAUTHORIZED:
    _logger.println(F("[pico] mqtt unauthorised"));
    break;
  }
}

void _mqttConfig(JsonVariant json) {
  // Pass on to the firmware callback
  if (_onConfig) {
    _onConfig(json);
  }
}

void _mqttCommand(JsonVariant json) {
  // Core restart command
  if (json.containsKey("restart") && json["restart"].as<bool>()) {
    rp2040.restart();
  }

  // Pass on to the firmware callback
  if (_onCommand) {
    _onCommand(json);
  }
}

void _mqttCallback(char *topic, uint8_t *payload, unsigned int length) {
  int state = _mqtt.receive(topic, payload, length);
  switch (state)
  {
    case MQTT_RECEIVE_ZERO_LENGTH:
      _logger.println("[picow] empty mqtt payload received");
      break;
    case MQTT_RECEIVE_JSON_ERROR:
      _logger.println("[picow] failed to deserialise mqtt json payload");
      break;
    case MQTT_RECEIVE_NO_CONFIG_HANDLER:
      _logger.println("[picow] no mqtt config handler");
      break;
    case MQTT_RECEIVE_NO_COMMAND_HANDLER:
      _logger.println("[picow] no mqtt command handler");
      break;
  }
}

void OXRS_IO_PICO::initialiseMqtt(byte *mac) {
  // NOTE: this must be called *before* initialising the REST API since
  // that will load MQTT config from file, which has precendence

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

void OXRS_IO_PICO::initialiseRestApi() {
  // NOTE: this must be called *after* initialising MQTT since that sets
  // the default client id, which has lower precedence that MQTT
  // settings stored in file and loaded by the API

  // Set up REST API
  _api.begin();

  // Register callbacks
  _api.onAdopt(_apiAdoptCallback);

  // Start listenging
  _server.begin();
}

boolean OXRS_IO_PICO::isNetworkConnected() {
  return WiFi.status() == WL_CONNECTED;
}

// FIXME: rely on wifi manager and config
void network_connect() {
  int status;
  while (status != WL_CONNECTED) {

      Serial.print("Attempting to connect to WPA SSID: ");
      Serial.println(WIFI_SSID);

      // Connect to WPA/WPA2 network:
      status = WiFi.begin(WIFI_SSID, WIFI_PASSWORD); // Set these credentials

      // wait to connect:
      delay(5000);
  }

  if (status == WL_CONNECTED) {
    Serial.println("Connected to");
    Serial.println(WIFI_SSID);
  }

  Serial.println(WiFi.localIP());
  Serial.println(WiFi.macAddress());
}

void OXRS_IO_PICO::initialiseNetwork(byte* mac) {
  
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

void OXRS_IO_PICO::loop() {

  // check network connection
  if (isNetworkConnected()) {
    
    // handle mqtt messages
    _mqtt.loop();

    // handle api requests
    WiFiClient client = _server.available();
    _api.loop(&client);
  }
}

void OXRS_IO_PICO::publishTelemetry(JsonVariant telemetry) {
    // publish something to OXRS_
    if (isNetworkConnected) {
      _mqtt.publishTelemetry(telemetry);
    }
}

// json helper
void OXRS_IO_PICO::mergeJson(JsonVariant dst, JsonVariantConst src) {
  if (src.is<JsonObjectConst>()) {
    for (JsonPairConst kvp : src.as<JsonObjectConst>()) {
      if (dst[kvp.key()]) {
        mergeJson(dst[kvp.key()], kvp.value());
      }
      else {
        dst[kvp.key()] = kvp.value();
      }
    }
  }
  else {
    dst.set(src);
  }
}

// FIXME:
void OXRS_IO_PICO::getFirmwareJson(JsonVariant json) {
  JsonObject firmware = json.createNestedObject("firmware");
  
  firmware["name"]      = "Air Quality Sensor";
  firmware["shortName"] = "AQS";
  firmware["maker"]     = "RaspberryPi";
  firmware["version"]   = "0.1";
}

void OXRS_IO_PICO::getSystemJson(JsonVariant json) {
  JsonObject system = json.createNestedObject("system");

// FIXME:
  system["heapUsedBytes"] = rp2040.getUsedHeap();
  system["heapFreeBytes"] = rp2040.getFreeHeap();
  system["heapMaxAllocBytes"] = rp2040.getTotalHeap();
  system["flashChipSizeBytes"] = 0; // TBD

  system["sketchSpaceUsedBytes"] = 0;
  system["sketchSpaceTotalBytes"] = 0;

  FSInfo64 fs;
  LittleFS.info64(fs);
  system["fileSystemUsedBytes"] = fs.usedBytes;
  system["fileSystemTotalBytes"] = fs.totalBytes;
}

void OXRS_IO_PICO::getNetworkJson(JsonVariant json) {
  JsonObject network = json.createNestedObject("network");

  network["mode"] = "wifi";
  network["ip"] = WiFi.localIP();

  byte mac[6];
  WiFi.macAddress(mac);
  char mac_display[18];
  sprintf_P(mac_display, PSTR("%02X:%02X:%02X:%02X:%02X:%02X"), mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  network["mac"] = mac_display;
}

void OXRS_IO_PICO::getConfigSchemaJson(JsonVariant json) {
  JsonObject configSchema = json.createNestedObject("configSchema");

  // config schema metadata
  configSchema["$schema"] = JSON_SCHEMA_VERSION;
  configSchema["title"] = "Air Quality Sensor - Sensirion 5x";
  configSchema["type"] = "object";

  JsonObject properties = configSchema.createNestedObject("properties");

  // Firmware config schema (if any)
  if (!_fwConfigSchema.isNull()) {
    mergeJson(properties, _fwConfigSchema.as<JsonVariant>());
  }
}

void OXRS_IO_PICO::setConfigSchema(JsonVariant json) {
  _fwConfigSchema.clear();
  mergeJson(_fwConfigSchema.as<JsonVariant>(), json);
}

void OXRS_IO_PICO::getCommandSchemaJson(JsonVariant json) {

  JsonObject commandSchema = json.createNestedObject("commandSchema");

  // command schema metadata
  commandSchema["$schema"] = JSON_SCHEMA_VERSION;
  commandSchema["title"] = "pico";
  commandSchema["type"] = "object";

  JsonObject properties = commandSchema.createNestedObject("properties");

  // Firware command schema (if any)
  if (!_fwCommandSchema.isNull()) {
    OXRS_IO_PICO::mergeJson(properties, _fwCommandSchema.as<JsonVariant>());
  }

  // Restart command
  JsonObject restart = properties.createNestedObject("restart");
  restart["title"] = "Restart";
  restart["type"] = "boolean";
}

void OXRS_IO_PICO::setCommandSchema(JsonVariant json) {
  _fwCommandSchema.clear();
  mergeJson(_fwCommandSchema.as<JsonVariant>(), json);
}

void OXRS_IO_PICO::apiAdoptCallback(JsonVariant json) {
  getFirmwareJson(json);
  getSystemJson(json);
  getNetworkJson(json);
  getConfigSchemaJson(json);
  getCommandSchemaJson(json);
}

void OXRS_IO_PICO::begin(jsonCallback config, jsonCallback command) {

  // Get our firmware details
  DynamicJsonDocument json(128);
  getFirmwareJson(json.as<JsonVariant>());

  // Log firmware details
  _logger.print("[pico]");
  serializeJson(json, _logger);
  _logger.println();

  // We wrap the callbacks so we can intercept messages intended for the PICO_SEN
  _onConfig = config;
  _onCommand = command;

  // setup network and obtain an IP address
  byte mac[6];
  initialiseNetwork(mac);

  // setup MQTT (dont attempt to connect yet)
  initialiseMqtt(mac);

  // setup the REST API
  initialiseRestApi();
}
