/*
 * OXRS_HASS.h
 * 
 */

#ifndef OXRS_HASS_H
#define OXRS_HASS_H

#include <OXRS_MQTT.h>                    // For MQTT pub/sub

class OXRS_HASS
{
  public:
    OXRS_HASS(OXRS_MQTT * mqtt);

    void setConfigSchema(JsonVariant json);
    void parseConfig(JsonVariant json);

    bool isDiscoveryEnabled(void);

    void getDiscoveryJson(JsonVariant json, char * id);
    bool publishDiscoveryJson(JsonVariant json, char * component, char * id);

  private:
    bool _discoveryEnabled;
    char _discoveryTopicPrefix[32];
};

#endif