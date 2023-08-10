#include <OXRS_LOG.h>
#include "WiFIManager.h"

// FIXME:
#define __USE_CREDENTIALS

#if defined(__USE_CREDENTIALS)
#include "Credentials.h"
#endif

static const char *_LOG_PREFIX = "[WiFiManager] ";

WiFiManager::WiFiManager()
{

}

WiFiManager::~WiFiManager()
{

}

bool WiFiManager::autoConnect(char const *apName, char const *apPassword)
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
    return true;
}

#ifndef __USE_CREDENTIALS
bool WiFiManager::setupHostname(bool restart)
{
    if(_hostname == "") {
        LOG_DEBUG(F("No Hostname to set"));
        return false;
    } 
    else {
        LOGF_DEBUG("Setting Hostname: %s", _hostname);
    }

    bool res = true;

    // @note hostname must be set after STA_START
    LOG_DEBUG(F("Setting WiFi hostname"));

    // FIXME: WiFi.setHostname is void and should be bool
    /*res = */WiFi.setHostname(_hostname.c_str());
    #ifdef WM_MDNS
        LOG_DEBUG(F("Setting MDNS hostname, tcp 80"));
        if(MDNS.begin(_hostname.c_str())){
            MDNS.addService("http", "tcp", 80);
        }
    #endif

    LOG_ERROR(F("hostname: set failed!"));

    if(restart && (WiFi.status() == WL_CONNECTED)){
        LOG_DEBUG(F("reconnecting to set new hostname"));
        WiFi.disconnect();
        delay(200); // do not remove, need a delay for disconnect to change status()
    }

    return res;
}

void WiFiManager::_begin()
{
    if(_hasBegun) 
        return;
    _hasBegun = true;
    // _usermode = WiFi.getMode();
}

void WiFiManager::_end()
{
    _hasBegun = false;
    if(_userpersistent) 
        WiFi.persistent(true); // reenable persistent, there is no getter we rely on _userpersistent
    // if(_usermode != WIFI_OFF) WiFi.mode(_usermode);
}

bool WiFiManager::autoConnect(char const *apName, char const *apPassword)
{
    LOG_DEBUG(F("AutoConnect"));

    // bool wifiIsSaved = getWiFiIsSaved();

    setupHostname(true);

    if (_hostname != "")
    {
        // disable wifi if already on
        if (WiFi.getMode() & WIFI_STA)
        {
            WiFi.mode(WIFI_OFF);
            int timeout = millis() + 1200;
            // async loop for mode change
            while (WiFi.getMode() != WIFI_OFF && millis() < timeout)
            {
                delay(0);
            }
        }
    }

    // check if wifi is saved, (has autoconnect) to speed up cp start
    // NOT wifi init safe
    // if(wifiIsSaved){
    _startconn = millis();
    _begin();

    // attempt to connect using saved settings, on fail fallback to AP config portal
    //if (!WiFi.enableSTA(true))
    WiFi.mode(WIFI_STA);
//    {
// handle failure mode Brownout detector etc.
//        LOG_ERROR(F("[FATAL] Unable to enable wifi!"));
//        return false;
//    }

    WiFiSetCountry();

    WiFi.persistent(false);     // disable persistent

    _usermode = WIFI_STA; // When using autoconnect , assume the user wants sta mode on permanently.

    // no getter for autoreconnectpolicy before this
    // https://github.com/esp8266/Arduino/pull/4359
    // so we must force it on else, if not connectimeout then waitforconnectionresult gets stuck endless loop
    WiFi_autoReconnect();

    // if already connected, or try stored connect
    // @note @todo ESP32 has no autoconnect, so connectwifi will always be called unless user called begin etc before
    // @todo check if correct ssid == saved ssid when already connected
    bool connected = false;
    if (WiFi.status() == WL_CONNECTED)
    {
        connected = true;
        LOG_DEBUG(F("AutoConnect: Already Connected"));
        setSTAConfig();
        // @todo not sure if this is safe, causes dup setSTAConfig in connectwifi,
        // and we have no idea WHAT we are connected to
    }

    if (connected || connectWifi(_defaultssid, _defaultpass) == WL_CONNECTED)
    {
// connected
#ifdef WM_DEBUG_LEVEL
        DEBUG_WM(F("AutoConnect: SUCCESS"));
        DEBUG_WM(DEBUG_VERBOSE, F("Connected in"), (String)((millis() - _startconn)) + " ms");
        DEBUG_WM(F("STA IP Address:"), WiFi.localIP());
#endif
        // Serial.println("Connected in " + (String)((millis()-_startconn)) + " ms");
        _lastconxresult = WL_CONNECTED;

        if (_hostname != "")
        {
#ifdef WM_DEBUG_LEVEL
            DEBUG_WM(DEBUG_DEV, F("hostname: STA: "), getWiFiHostname());
#endif
        }
        return true; // connected success
    }

#ifdef WM_DEBUG_LEVEL
    DEBUG_WM(F("AutoConnect: FAILED for "), (String)((millis() - _startconn)) + " ms");
#endif
    // }
    // else {
    // #ifdef WM_DEBUG_LEVEL
    // DEBUG_WM(F("No Credentials are Saved, skipping connect"));
    // #endif
    // }

    // possibly skip the config portal
    if (!_enableConfigPortal)
    {
#ifdef WM_DEBUG_LEVEL
        DEBUG_WM(DEBUG_VERBOSE, F("enableConfigPortal: FALSE, skipping "));
#endif

        return false; // not connected and not cp
    }

    // not connected start configportal
    bool res = startConfigPortal(apName, apPassword);
    return res;
}
#endif
