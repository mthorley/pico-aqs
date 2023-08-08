/*
 * OXRS_HASS.cpp
 * 
 */

#include "Arduino.h"
#include "OXRS_HASS.h"

// Macro for converting env vars to strings
#define STRINGIFY(s) STRINGIFY1(s)
#define STRINGIFY1(s) #s

// Pointer to the MQTT lib so we can get/set config
OXRS_MQTT * _hassMqtt;

OXRS_HASS::OXRS_HASS(OXRS_MQTT * mqtt) 
{
  _hassMqtt = mqtt;

  // Set the defaults
  _discoveryEnabled = false;
  strcpy(_discoveryTopicPrefix, "homeassistant");
}

void OXRS_HASS::setConfigSchema(JsonVariant json)
{
  JsonObject discoveryEnabled = json.createNestedObject("hassDiscoveryEnabled");
  discoveryEnabled["title"] = "Home Assistant Discovery";
  discoveryEnabled["description"] = "Publish Home Assistant discovery config (defaults to 'false').";
  discoveryEnabled["type"] = "boolean";

  JsonObject discoveryTopicPrefix = json.createNestedObject("hassDiscoveryTopicPrefix");
  discoveryTopicPrefix["title"] = "Home Assistant Discovery Topic Prefix";
  discoveryTopicPrefix["description"] = "Prefix for the Home Assistant discovery topic (defaults to 'homeassistant`).";
  discoveryTopicPrefix["type"] = "string";
}

void OXRS_HASS::parseConfig(JsonVariant json)
{
  if (json.containsKey("hassDiscoveryEnabled"))
  {
    _discoveryEnabled = json["hassDiscoveryEnabled"].as<bool>();
  }

  if (json.containsKey("hassDiscoveryTopicPrefix"))
  {
    strcpy(_discoveryTopicPrefix, json["hassDiscoveryTopicPrefix"]);
  }
}

bool OXRS_HASS::isDiscoveryEnabled(void)
{
  return _discoveryEnabled;
}

void OXRS_HASS::getDiscoveryJson(JsonVariant json, char * id)
{
  char uniqueId[64];
  sprintf_P(uniqueId, PSTR("%s_%s"), _hassMqtt->getClientId(), id);
  json["uniq_id"] = uniqueId;
  json["obj_id"] = uniqueId;

  char topic[64];
  json["avty_t"] = _hassMqtt->getLwtTopic(topic);
  json["avty_tpl"] = "{% if value_json.online == true %}online{% else %}offline{% endif %}";

  JsonObject dev = json.createNestedObject("dev");
  dev["name"] = _hassMqtt->getClientId();

#if defined(FW_MAKER)
  json["dev"]["mf"] = FW_MAKER;
#endif
#if defined(FW_NAME)
  json["dev"]["mdl"] = FW_NAME;
#endif
#if defined(FW_VERSION)
  json["dev"]["sw"] = STRINGIFY(FW_VERSION);
#endif

  JsonArray ids = dev.createNestedArray("ids");
  ids.add(_hassMqtt->getClientId());
}

bool OXRS_HASS::publishDiscoveryJson(JsonVariant json, char * component, char * id)
{
  // Exit early if discovery is not enabled
  if (!_discoveryEnabled) { return false; }

  // Check for a null payload and ensure we send an empty JSON object
  // to clear any existing Home Assistant config
  if (json.isNull())
  {
    json = json.to<JsonObject>();
  }

  // Build the discovery topic
  char topic[64];
  sprintf_P(topic, PSTR("%s/%s/%s/%s/config"), _discoveryTopicPrefix, component, _hassMqtt->getClientId(), id);

  // Publish the discovery config (retained)
  return _hassMqtt->publish(json, topic, true);
}
