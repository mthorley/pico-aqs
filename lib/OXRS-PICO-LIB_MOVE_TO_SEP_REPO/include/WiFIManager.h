#pragma once
#include <WiFi.h>

/**
 * This is *heavily* based on the excellent works of https://github.com/tzapu/WiFiManager/tree/master
 * but tailored to Pico and simplified and is very much WIP.
 */

class WiFiManager 
{
public:
    WiFiManager();
    ~WiFiManager();

    bool autoConnect(char const *apName, char const *apPassword = NULL);
    bool setupHostname(bool restart);


private:

    void _begin();
    void _end();
    bool          _hasBegun               = false; // flag wm loaded,unloaded
    bool          _userpersistent         = true;  // users preffered persistence to restore

    String        _hostname               = "";    // hostname for esp8266 for dhcp, and or MDNS

    // options flags
    unsigned long _configPortalTimeout    = 0; // ms close config portal loop if set (depending on  _cp/webClientCheck options)
    unsigned long _connectTimeout         = 0; // ms stop trying to connect to ap if set
    unsigned long _saveTimeout            = 0; // ms stop trying to connect to ap on saves, in case bugs in esp waitforconnectresult

    unsigned long _configPortalStart      = 0; // ms config portal start time (updated for timeouts)
    unsigned long _webPortalAccessed      = 0; // ms last web access time
    uint8_t       _lastconxresult         = WL_IDLE_STATUS; // store last result when doing connect operations
    int           _numNetworks            = 0; // init index for numnetworks wifiscans
    unsigned long _lastscan               = 0; // ms for timing wifi scans
    unsigned long _startscan              = 0; // ms for timing wifi scans
    unsigned long _startconn              = 0; // ms for timing wifi connects
    WiFiMode_t    _usermode               = WIFI_STA; // Default user mode

};
