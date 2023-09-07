#pragma once

#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESP8266mDNS.h>    // yes this is correct
#include <EEPROM.h>         // simulated EEPROM: https://arduino-pico.readthedocs.io/en/latest/eeprom.html

/**
 * Derived from the excellent https://www.smartlab.at/implement-an-esp32-hot-spot-that-runs-a-captive-portal/ and
 * https://stackoverflow.com/questions/54583818/esp-auto-login-accept-message-by-os-with-redirect-to-page-like-public-wifi-port
 */

#define SSID_OR_PWD_LEN   32

typedef struct wifi_credentials_t {
    char ssid[SSID_OR_PWD_LEN];
    char pwd[SSID_OR_PWD_LEN];

/*    void serialise(std::ostream &out) const {
        out << ssid << pwd;
    }

    static wifi_credentials_t deserialise(std::istream &in) {
        wifi_credentials_t c;
        in >> c.ssid >> c.pwd;
        return c;
    }*/
} wifi_credentials_t;

class WiFiManager 
{
public:
    WiFiManager(char const *apName, char const *apPassword);
    ~WiFiManager();

    bool autoConnect();

    // hostname for mDNS to enable http://oxrs.local
    inline static const char *HOSTNAME = "oxrs";

    void saveCredentials(wifi_credentials_t creds);
    void loadCredentials(wifi_credentials_t& creds);

private:
    // state machine states
    typedef enum State {    // _currentState, _previousState;
        START,
        LOAD_CREDENTIALS,   // -> (check for WiFi credentials in EEPROM) ? CONNECT_TO_WLAN : SHOW_PORTAL    // _connectionAttempts reset; setupConfigPortal false; APrunning false;
        CONNECT_TO_WLAN,    // -> (connection successful) ? STOP : RETRY_CONNECTION                         //  exceeded retries, then _accessPointRunning false to reinitalise AP
        RETRY_CONNECTION,   // -> (num connection attempts > 3) ? SHOW_PORTAL : CONNECT_TO_WLAN             // _connectionAttempts++
        SHOW_PORTAL,        // -> (WLAN configured) ? SAVE_CREDS : SHOW_PORTAL
        SAVE_CREDENTIALS,   // -> (save credentials) ? CONNECT_TO_WLAN : error                              // _connectionAttempts reset
        STOP
    } State_t;

    void cycleStateMachine();

    // captive portal methods
    bool startConfigPortal();
    bool redirectToPortal() const;

#ifdef __DEPRECATED__
#define FLASH_POS_SSID    0
#define FLASH_POS_PWD     64
    void writeCredentials(const wifi_credentials_t& creds);
    void readCredentials(wifi_credentials_t& creds);
#endif

    // WiFi credential methods
    bool credentialsExist();
    void clearCredentials();    // FIXME: is this required?
    void saveCredentials();
    void loadCredentials();

    // webserver handler callbacks
    void handleRoot();
    void handleWifi();
    void handleWifiSave();
    void handleNotFound();

    // html helpers
    void sendStandardHeaders();
    void getSignalStrength(String& cssStyle, const int32_t rssi) const;

    int8_t connectWifi();       // connect to wlan

    void getWL_StatusAsString(String& status, const int8_t wlStatus);

// members
    State_t _currentState;              // current state of network configuration state machine
    State_t _previousState;             // previous state of network configuration state machine
    uint8_t _connectionAttempts;        // count of connection attempts
    bool    _setupConfigPortal;         // true if web/dns servers have already been setup for captive portal
    bool    _accessPointRunning;        // true if the AP WiFi is running to support captive portal

    // RSSI thresholds for displaying strength indicators
    inline static const int32_t STRONG_RSSI_THRESHOLD = -55;
    inline static const int32_t MEDIUM_RSSI_THRESHOLD = -80;

    // Access Point network parameters
    static const IPAddress AP_IP;

    // DNS related
    inline static const byte DNS_PORT = 53;

    // WiFi connection consts
    inline static const uint8_t  MAX_CONNECTION_ATTEMPTS = 3;    // max number of attempts to connect to WiFi
    inline static const uint16_t MAX_TIMEOUT_MS          = 10000;        // num of ms to wait until WiFi connection

    // Target WLAN SSID and password
    char _wlanSSID[32];
    char _wlanPassword[32];

    // Access point SSID and password
    char _apSSID[32];
    char _apPassword[32];
};
