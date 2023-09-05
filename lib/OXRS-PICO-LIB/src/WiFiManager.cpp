#include <hardware/flash.h>
#include <hardware/sync.h>
#include <OXRS_LOG.h>
#include <LittleFS.h>
#include "WiFiManager.h"
#include "WiFiManagerAssets.h"

/*
Tasks
x update content of pages and OXRS LOGO
x maxlength on inputs
- replace serial with OXRS_LOG
- validation of ssid, pwd for type and length
- unit tests for EEPROM creds
- move WebServer and DNS to pointers so the destructor cleans em up
x template the html pages
o implement TLS
*/

/* hostname for mDNS. Should work at least on windows. Try http://oxrs.local */
const char *myHostname = "oxrs";

// DNS server
const byte DNS_PORT = 53;
DNSServer dnsServer;

// Web server
WebServer server(80);

/* AP network parameters */
IPAddress apIP(192, 168, 42, 1);
IPAddress netMsk(255, 255, 255, 0);

/** Should I connect to WLAN asap? */
boolean connectToWlan;

/** Last time I tried to connect to WLAN */
unsigned long lastConnectTry = 0;

/** Current WLAN status */
unsigned int status = WL_IDLE_STATUS;

// FIXME:
// #define __USE_CREDENTIALS

#if defined(__USE_CREDENTIALS)
#include "Credentials.h"
#endif

static const char *_LOG_PREFIX = "[WiFiManager] ";

WiFiManager::WiFiManager(const char *apName, const char *apPassword) : 
    _wlanSSID(""),
    _wlanPassword(""),
    _setupConfigPortal(false)
{
    strncpy(_apSSID, apName, 32);
    strncpy(_apPassword, apPassword, 32);
// FIXME: Is this in the correct location...
    EEPROM.begin(512);
}

WiFiManager::~WiFiManager()
{
}

// clears credentials in memory only (not in EEPROM emulation)
void WiFiManager::clearWlanCredentials()
{
    strcpy(_wlanSSID, "");
    strcpy(_wlanPassword, "");
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
#ifdef __DEPRECATED__
void WiFiManager::writeCredentials(const wifi_credentials_t &creds)
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

    restore_interrupts(ints);
}

void WiFiManager::readCredentials(wifi_credentials_t &creds)
{
// Flash-based address of the last sector
#define FLASH_TARGET_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)

    char *p;
    int addr, value;

    // Compute the memory-mapped address, remembering to include the offset for RAM
    addr = XIP_BASE + FLASH_TARGET_OFFSET;

    p = (char *)addr;
    strncpy(creds.ssid, p + FLASH_POS_SSID, SSID_OR_PWD_LEN);
    strncpy(creds.pwd, p + FLASH_POS_PWD, SSID_OR_PWD_LEN);
}
#endif

bool WiFiManager::credentialsExist()
{
    // FIXME: read flash
    return false;
}

bool configured = false;

bool WiFiManager::autoConnect()
{
    LOG_DEBUG(F("autoConnect"));

    int8_t res = WL_IDLE_STATUS;
    Serial.println("loading credentials");
    //    loadCredentials();
    if (strlen(_wlanSSID) > 0)
    {
        // attempt to connect to WLAN
        WiFi.begin(_wlanSSID, _wlanPassword);
        res = WiFi.waitForConnectResult();
        Serial.print("res") + Serial.println(res);
    }
    else
    {
        Serial.println("No SSID stored");
    }

    if (res != WL_CONNECTED)
    {
        // create a WiFi AP and captive portal
        startConfigPortal();
        while (!configured)
            loop();
    }

    // FIXME:
    return true;
}

int8_t WiFiManager::connectWifi()
{
//    Serial.println("Connecting as wifi client...");
    LOG_DEBUG(F("Connecting as wifi client"));

    WiFi.disconnect();
    WiFi.begin(_wlanSSID, _wlanPassword);
    int8_t connRes = WiFi.waitForConnectResult();

    LOGF_DEBUG("Connection result %d", connRes);
//    Serial.print("connRes: ");
//    Serial.println(connRes);

    return connRes;
}

void WiFiManager::loop()
{
    if (connectToWlan)
    {
        Serial.println("Connect requested");
        connectToWlan = false;
        int8_t res = connectWifi();
        lastConnectTry = millis();
        if (res == WL_CONNECTED)
            configured = true;
        else
        {
            clearWlanCredentials();
            startConfigPortal();
        }
    }

    {
        unsigned int s = WiFi.status();
        if (s == WL_IDLE_STATUS && millis() > (lastConnectTry + 60000))
        {
            /* If WLAN disconnected and idle try to connect */
            /* Don't set retry time too low as retry interfere the AP operation */
            connectToWlan = true;
        }
        if (status != s)
        { // WLAN status change
            Serial.print("Status: ");
            Serial.println(s);
            status = s;
            if (s == WL_CONNECTED)
            {

                /* Just connected to WLAN */
                Serial.println("");
                Serial.print("Connected to ");
                Serial.println(_wlanSSID);
                Serial.print("IP address: ");
                Serial.println(WiFi.localIP());

                // Setup MDNS responder
                if (!MDNS.begin(myHostname))
                {
                    Serial.println("Error setting up MDNS responder!");
                }
                else
                {
                    Serial.println("mDNS responder started");
                    // Add service to MDNS-SD
                    MDNS.addService("http", "tcp", 80);
                }
            }
            else if (s == WL_NO_SSID_AVAIL)
            {
                WiFi.disconnect();
            }
        }
        if (s == WL_CONNECTED)
        {
            MDNS.update();
        }
    }
    // Do work:
    // DNS
    dnsServer.processNextRequest();
    // HTTP
    server.handleClient();
}

/** IP to String? */
String toStringIp(IPAddress ip)
{
    String res = "";
    for (int i = 0; i < 3; i++)
    {
        res += String((ip >> (8 * i)) & 0xFF) + ".";
    }
    res += String(((ip >> 8 * 3)) & 0xFF);
    return res;
}

/** Is this an IP? */
boolean isIp(String str)
{
    for (size_t i = 0; i < str.length(); i++)
    {
        int c = str.charAt(i);
        if (c != '.' && (c < '0' || c > '9'))
        {
            return false;
        }
    }
    return true;
}

/** 
 * Redirect to captive portal if we got a request for another domain. 
 * Return true in that case so the page handler do not try to handle the request again.
 */
bool WiFiManager::redirectToPortal()
{
    if (!isIp(server.hostHeader()) && server.hostHeader() != (String(myHostname) + ".local"))
    {
        LOG_DEBUG(F("Request redirected to captive portal"));

        server.sendHeader("Location", String("http://") + toStringIp(server.client().localIP()), true);
        server.send(302, "text/plain", "");     // Empty content inhibits Content-length header so we have to close the socket ourselves.
        server.client().stop();                 // Stop is needed because we sent no content length

        return true;
    }
    return false;
}

void WiFiManager::sendStandardHeaders()
{
    server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "-1");
}

/** 
 * Handle root or redirect to captive portal
 */
void WiFiManager::handleRoot()
{
    // If captive portal then redirect instead of displaying the page
    if (redirectToPortal()) { 
        return;
    }

    sendStandardHeaders();
    String html(root);
    html.replace("${oxrs.img}",           oxrs_img);
    html.replace("${firmware.name}",      FW_NAME);
    html.replace("${firmware.shortname}", FW_SHORT_NAME);
    html.replace("${firmware.maker}",     FW_MAKER);
    html.replace("${firmware.version}",   FW_VERSION);
    server.send(200, "text/html", html);
}

void WiFiManager::getSignalStrength(String& signal, int32_t strength)
{
    if (strength <-60)
        signal = "strong";
    else if(strength <-80)
        signal = "medium";
    else
        signal = "weak";
}

/**
 * Show available wifi networks 
 */
void WiFiManager::handleWifi()
{
    sendStandardHeaders();
    String html(wifi);
    html.replace("${oxrs.img}", oxrs_img);
    html.replace("${img.unlock}", unlock);
    html.replace("${img.lock}", lock);

    String networks;
    LOG_DEBUG(F("Network scan start"));
    int n = WiFi.scanNetworks();
    LOG_DEBUG(F("Network scan complete"));

    if (n > 0) {
        for (int i = 0; i < n; i++)
        {
            if ( strlen(WiFi.SSID(i)) > 0 )     // ignore hidden SSIDs
            {
                String network(wifi_network);
                network.replace("${name}", WiFi.SSID(i));

                String signal;
                getSignalStrength(signal, WiFi.RSSI(i));
                network.replace("${signal}", signal);

                network.replace("${network.security}", 
                    WiFi.encryptionType(i) == ENC_TYPE_NONE ? "unlock" : "lock");
                networks += network;
            }
        }
    }
    else {
        networks = F("<tr><td>No networks found</td></tr>");
    }

    html.replace("${networks}", networks);
    server.send(200, "text/html", html);
    server.client().stop();                 // Stop is needed because we sent no content length
}

/** Handle the WLAN save form and redirect to WLAN config page again */
void WiFiManager::handleWifiSave()
{
    LOG_DEBUG(F("wifi save"));

    server.arg("n").toCharArray(_wlanSSID, sizeof(_wlanSSID) - 1);
    server.arg("p").toCharArray(_wlanPassword, sizeof(_wlanPassword) - 1);
    server.sendHeader("Location", "wifi", true);
    sendStandardHeaders();
    server.send(302, "text/plain", ""); // Empty content inhibits Content-length header so we have to close the socket ourselves.
    server.client().stop();             // Stop is needed because we sent no content length
    saveCredentials();
    connectToWlan = strlen(_wlanSSID) > 0; // Request WLAN connect with new credentials if there is a SSID
}

void WiFiManager::saveCredentials()
{
    // save to EEPROM (emulated)
    wifi_credentials_t creds;
    strcpy(creds.ssid, _wlanSSID);
    strcpy(creds.pwd, _wlanPassword);
    EEPROM.put(0, creds);
}

void WiFiManager::loadCredentials()
{
    // load from EEPROM (emulated)
    wifi_credentials_t creds;
    EEPROM.get(0, creds);
    strncpy(_wlanSSID, creds.ssid, 32);
    strncpy(_wlanPassword, creds.pwd, 32);
}

void WiFiManager::handleNotFound()
{
    // If captive portal then redirect instead of displaying the error page
    if (redirectToPortal())
    {
        return;
    }

    String message = F("File Not Found\n\n");
    message += F("URI: ");
    message += server.uri();
    message += F("\nMethod: ");
    message += (server.method() == HTTP_GET) ? "GET" : "POST";
    message += F("\nArguments: ");
    message += server.args();
    message += F("\n");

    for (uint8_t i = 0; i < server.args(); i++)
    {
        message += String(F(" ")) + server.argName(i) + F(": ") + server.arg(i) + F("\n");
    }
    sendStandardHeaders();
    server.send(404, "text/plain", message);
//FIXME: button to get back home?
}

bool WiFiManager::startConfigPortal()
{
    LOG_DEBUG(F("Starting config portal"));

    // go into AP mode
    WiFi.beginAP(_apSSID, _apPassword);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }

    delay(500); // Without delay I've seen the IP address blank
//    Serial.print("AP IP address: ");
//    Serial.println(WiFi.localIP());
    LOGF_DEBUG("AP IP address %s", WiFi.localIP().toString().c_str());

    if (!_setupConfigPortal)
    {
        /* Setup the DNS server redirecting all the domains to the apIP */
        dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
        dnsServer.start(DNS_PORT, "*", apIP);

        /* Setup web pages: root, wifi config pages, SO captive portal detectors and not found. */
        server.on("/", std::bind(&WiFiManager::handleRoot, this));
        server.on("/wifi", std::bind(&WiFiManager::handleWifi, this));
        server.on("/wifisave", std::bind(&WiFiManager::handleWifiSave, this));
        // FIXME: check Android and Windows still have captive portal even with these commented out
        //        server.on("/fwlink", std::bind(&WiFiManager::handleRoot, this));  //Microsoft captive portal. Maybe not needed. Might be handled by notFound handler.
        server.onNotFound(std::bind(&WiFiManager::handleNotFound, this));
        server.begin();             // Web server start
//        Serial.println("HTTP server started");
        LOG_DEBUG(F("HTTP server started"));
        _setupConfigPortal = true;
    }

    //    loadCredentials(); // Load WLAN credentials from network
    //    connect = strlen(_wlanSSID) > 0; // Request WLAN connect if there is a SSID

    return true;
}

#endif
