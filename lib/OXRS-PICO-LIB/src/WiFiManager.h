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
#define FLASH_POS_SSID    0
#define FLASH_POS_PWD     64

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
// FIXME: NULL password means open network
    WiFiManager(char const *apName, char const *apPassword = NULL);
    ~WiFiManager();

    bool autoConnect();

//private:

    // captive portal methods
    bool startConfigPortal();
    bool redirectToPortal();
    void loop();

    // WiFi credential methods
    bool credentialsExist();

#ifdef __DEPRECATED__
    void writeCredentials(const wifi_credentials_t& creds);
    void readCredentials(wifi_credentials_t& creds);
#endif

    void clearWlanCredentials();
    void saveCredentials();
    void loadCredentials();

    // webserver handler callbacks
    void handleRoot();
    void handleWifi();
    void handleWifiSave();
    void handleNotFound();

    // html helpers
    void sendStandardHeaders();
    void getSignalStrength(String& signal, int32_t strength);

    int8_t connectWifi();       // connect to wlan

    bool _setupConfigPortal;    // setup web/dns servers once if capture portal is reshown

    // Target WLAN SSID and password
    char _wlanSSID[32];
    char _wlanPassword[32];

    // Access point SSID and password
    char _apSSID[32];
    char _apPassword[32];
};
