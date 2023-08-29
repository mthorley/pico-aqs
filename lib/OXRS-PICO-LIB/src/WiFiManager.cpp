#include <hardware/flash.h>
#include <hardware/sync.h>
#include <OXRS_LOG.h>
#include <LittleFS.h>
#include <aWOT.h>
#include "WiFIManager.h"

// FIXME:
#define __USE_CREDENTIALS

WiFiServer server(80);
Application app;

#if defined(__USE_CREDENTIALS)
#include "Credentials.h"
#endif

static const char *_LOG_PREFIX = "[WiFiManager] ";

void index(Request &req, Response &res) {
  res.print("Hello Wolrd!");
}

WiFiManager::WiFiManager()
{
}

WiFiManager::~WiFiManager()
{
}

#ifdef __USE_CREDENTIALS
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
#endif

#ifndef __USE_CREDENTIALS
void WiFiManager::writeCredentials(const wifi_credentials_t& creds)
{
    char buf[FLASH_PAGE_SIZE /* /sizeof(char) */];

    // Using fixed position in memory rather than serialisation
    // to reduce likelihood of a read error on changes to the struct
    strcpy(&buf[FLASH_POS_SSID], creds.ssid);
    strcpy(&buf[FLASH_POS_PWD], creds.pwd);

    uint32_t ints = save_and_disable_interrupts();

    // Erase the last sector of the flash
    flash_range_erase((PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE), FLASH_SECTOR_SIZE);

    // Program buf[] into the first page of this sector
    flash_range_program((PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE), (uint8_t *)buf, FLASH_PAGE_SIZE);

    restore_interrupts (ints);
}

void WiFiManager::readCredentials(wifi_credentials_t& creds)
{
// Flash-based address of the last sector
#define FLASH_TARGET_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)

    char *p;
    int addr, value;

    // Compute the memory-mapped address, remembering to include the offset for RAM
    addr = XIP_BASE + FLASH_TARGET_OFFSET;

    p = (char*)addr;
    strncpy(creds.ssid, p+FLASH_POS_SSID, SSID_OR_PWD_LEN);
    strncpy(creds.pwd,  p+FLASH_POS_PWD,  SSID_OR_PWD_LEN);
}

bool WiFiManager::credentialsExist()
{
// FIXME: read flash
    return false;
}

bool WiFiManager::autoConnect(char const *apName, char const *apPassword)
{
    LOG_DEBUG(F("autoConnect"));

    bool res = false;

    // check if credentials exist
    if (credentialsExist()) {
        // attempt to connect
        // if (!attemptToConnect()) 
        {
            // erase credentials

            // fall out to startConfigPortal
            res = startConfigPortal(apName, apPassword);
        }
    }
    else
    {
        // not connected start configportal
        res = startConfigPortal(apName, apPassword);
    }
    return res;
}

bool WiFiManager::startConfigPortal(char const *apName, char const *apPassword)
{
    // go into AP mode
    WiFi.beginAP(apName, apPassword);
    while ( WiFi.status() != WL_CONNECTED ) {;
        delay( 500 );
        Serial.print( "." );
    }

    app.get("/", &index);
    server.begin();

    // scan for networks

    // show portal
    while(true) {
        WiFiClient client = server.available();

        if (client.connected()) {
            Serial.println("client connected");
            app.process(&client);
        }
    }

    return true;
}
#endif
