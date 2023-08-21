#pragma once
#include <WiFi.h>

/**
 * This is *heavily* based on the excellent works of https://github.com/tzapu/WiFiManager/tree/master
 * but tailored to Pico and simplified and is very much WIP.
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
    WiFiManager();
    ~WiFiManager();

    bool autoConnect(char const *apName, char const *apPassword = NULL);
    bool startConfigPortal(char const *apName, char const *apPassword);

//private:

    // WiFi credential methods
    bool credentialsExist();
    void writeCredentials(const wifi_credentials_t& creds);
    void readCredentials(wifi_credentials_t& creds);
};
