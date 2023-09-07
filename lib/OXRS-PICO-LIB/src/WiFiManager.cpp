#include <hardware/flash.h>
#include <hardware/sync.h>
#include <OXRS_LOG.h>
#include <LittleFS.h>
#include <WiFiManager.h>
#include <WiFiManagerAssets.h>

/*
Tasks
x update content of pages and OXRS LOGO
x maxlength on inputs
- fix lock icon size
- writing of creds to EEPROM and retrieval
- replace serial with OXRS_LOG
- unit tests for EEPROM creds
- validation of ssid, pwd for type and length
- move WebServer and DNS to pointers so the destructor cleans em up
x template the html pages
o implement TLS
*/

// DNS server
DNSServer dnsServer;

// Web server
WebServer server(80);

// FIXME:
// #define __USE_CREDENTIALS

#if defined(__USE_CREDENTIALS)
#include "Credentials.h"
#endif

static const char *_LOG_PREFIX = "[WiFiManager] ";

// AccessPoint network config
const IPAddress WiFiManager::AP_IP = IPAddress(192,168,42,1);

WiFiManager::WiFiManager(const char *apName, const char *apPassword) :
    _wlanSSID(""),
    _wlanPassword(""),
    _setupConfigPortal(false),
    _accessPointRunning(false),
    _connectionAttempts(0),
    _currentState(State_t::START),
    _previousState(State_t::STOP)
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
void WiFiManager::clearCredentials()
{
    strcpy(_wlanSSID, "");
    strcpy(_wlanPassword, "");
}

void WiFiManager::saveCredentials(wifi_credentials_t creds)
{
    EEPROM.put(0, creds);
}

void WiFiManager::loadCredentials(wifi_credentials_t& creds)
{
    EEPROM.get(0, creds);
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
    return false;   // FIXME: how will we know?
}

void WiFiManager::cycleStateMachine()
{
    if (_previousState != _currentState)
        LOGF_DEBUG("Current state %d", _currentState);

    switch(_currentState) {
        case START:
            // move to LOAD_CREDENTIALS
            _currentState = LOAD_CREDENTIALS;
            // initialise state variables
            _accessPointRunning = false;
            _setupConfigPortal = false;
            _connectionAttempts = 0;
            break;

        case LOAD_CREDENTIALS:
            // attempt to load WiFi credentials from EEPROM
            if (credentialsExist()) {
                LOG_DEBUG(F("Credentials exist"));
                loadCredentials();
                _currentState = CONNECT_TO_WLAN;
            }
            else {
                LOG_DEBUG(F("Credentials do not exist"));
                _currentState = SHOW_PORTAL;
            }
            break;

        case SHOW_PORTAL:
            // spin up captive portal
            startConfigPortal();        // portal wifi page form submit will move the state to SAVE_CREDENTIALS
            break;

        case SAVE_CREDENTIALS:
            saveCredentials();
            _connectionAttempts = 0;
            _currentState = CONNECT_TO_WLAN;
            break;

        case CONNECT_TO_WLAN:
            if (connectWifi() != WL_CONNECTED) {
                _connectionAttempts++;
                delay(500);             // give it some time
            }
            else {
                _currentState = STOP;
            }

            if (_connectionAttempts > MAX_CONNECTION_ATTEMPTS-1) {
                _accessPointRunning = false;    // reinitialise AP
                _currentState = SHOW_PORTAL;
            }
            break;

        case STOP:
            // done
            break;
    }

    // if the config portal has been setup, handle dns and web requests
    if (_setupConfigPortal) {
        dnsServer.processNextRequest();
        server.handleClient();
    }

    if (_previousState != _currentState)
        LOGF_DEBUG("Transitioned to state %d", _currentState);

    _previousState = _currentState;
}

bool WiFiManager::autoConnect()
{
    // iterate through state machine until we have configured WiFi
    while (_currentState != State_t::STOP)
        cycleStateMachine();
    return true;
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

//---------------------------------------------------

/**
 * Convert WL_STATUS into String for logging.
 */
void WiFiManager::getWL_StatusAsString(String& status, const int8_t wlStatus)
{
    switch (wlStatus)
    {
        case WL_IDLE_STATUS:
            status = "WL_IDLE_STATUS";
            break;
        case WL_NO_SSID_AVAIL:
            status = "WL_NO_SSID_AVAIL";
            break;
        case WL_SCAN_COMPLETED:
            status = "WL_SCAN_COMPLETED";
            break;
        case WL_CONNECTED:
            status = "WL_CONNECTED";
            break;
        case WL_CONNECT_FAILED:
            status = "WL_CONNECT_FAILED";
            break;
        case WL_CONNECTION_LOST:
            status = "WL_CONNECTION_LOST";
            break;
        case WL_DISCONNECTED:
            status = "WL_DISCONNECTED";
            break;
        default:
            status = "Unknown";
            break;
    }
}

/**
 * Connect to configured WLAN WiFi
 *
 * @return int8_t connection result
 */
int8_t WiFiManager::connectWifi()
{
    LOGF_DEBUG("Connecting as wifi client to SSID %s", _wlanSSID);

    WiFi.disconnect();
    WiFi.begin(_wlanSSID, _wlanPassword);
    int8_t res = WiFi.waitForConnectResult(MAX_TIMEOUT_MS);

    String s;
    getWL_StatusAsString(s, res);
    LOGF_DEBUG("Connection result %s", s.c_str());

    return res;
}

/**
 * Create access point and register and serve web content for
 * the captive portal.
 */
bool WiFiManager::startConfigPortal()
{
    if (!_accessPointRunning) {
        // Go into AP mode, configure IP (so DNS can use it)
        WiFi.config(AP_IP);
        WiFi.beginAP(_apSSID, _apPassword);
        while (WiFi.status() != WL_CONNECTED)
        {
            delay(500);
            Serial.print(".");  // waiting
        }

        delay(500);             // wait for AP to stabalise

        String ip = WiFi.localIP().toString();
        LOGF_DEBUG("AccessPoint IP address %s", ip.c_str());
        _accessPointRunning = true;
    }

    // only setup config portal once
    if (!_setupConfigPortal)
    {
        LOG_DEBUG(F("Starting config portal"));

        // Setup the DNS server redirecting all domains to the AP_IP
        dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
        dnsServer.start(DNS_PORT, "*", AP_IP);

        // Setup web pages: root, wifi config and not found
        server.on("/", std::bind(&WiFiManager::handleRoot, this));
        server.on("/wifi", std::bind(&WiFiManager::handleWifi, this));
        server.on("/wifisave", std::bind(&WiFiManager::handleWifiSave, this));
        server.onNotFound(std::bind(&WiFiManager::handleNotFound, this));
        server.begin();             // Web server start

        LOG_DEBUG(F("HTTP server started"));
        _setupConfigPortal = true;
    }

    return _setupConfigPortal;
}

/** 
 * Redirect to captive portal if we got a request for another domain. 
 * Return true in that case so the page handler does not try to handle the request again.
 */
bool WiFiManager::redirectToPortal() const
{
    if (!isIp(server.hostHeader()) && server.hostHeader() != (String(HOSTNAME) + ".local"))
    {
        LOG_DEBUG(F("Request redirected to captive portal"));

        IPAddress ip = server.client().localIP();
        server.sendHeader("Location", String("http://") + ip.toString(), true);
        server.send(302, "text/plain", "");     // Empty content inhibits Content-length header so we have to close the socket ourselves.
        server.client().stop();                 // Stop is needed because we sent no content length

        return true;
    }
    return false;
}

/**
 * Send standard HTTP headers
 */
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

/**
 * Converts RSSI into a CSS style to indicate WiFi network signal strength.
 */
void WiFiManager::getSignalStrength(String& cssStyle, const int32_t rssi) const
{
    if (rssi < STRONG_RSSI_THRESHOLD)
        cssStyle = "strong";
    else if(rssi < MEDIUM_RSSI_THRESHOLD)
        cssStyle = "medium";
    else
        cssStyle = "weak";
}

/**
 * Show available wifi networks.
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
        for (int i = 0; i < n; i++) {
            if ( strlen(WiFi.SSID(i)) > 0 )     // ignore hidden SSIDs
            {
                String network(wifi_network);
                network.replace("${name}", WiFi.SSID(i));

                String signal;
                getSignalStrength(signal, WiFi.RSSI(i));
                network.replace("${signal}", signal);

                network.replace("${network.security}", WiFi.encryptionType(i) == ENC_TYPE_NONE ? "unlock" : "lock");
                networks += network;
            }
        }
    }
    else {
        networks = F("<tr><td>No networks found</td></tr>");
    }

    WiFi.scanDelete();

    html.replace("${networks}", networks);
    server.send(200, "text/html", html);
    server.client().stop();                 // Stop is needed because we sent no content length
}

/**
 * Process the WiFi save form, save credentials and redirect to WiFi config page again. 
 */
void WiFiManager::handleWifiSave()
{
    LOG_DEBUG(F("Handle WiFi save"));

    server.arg("n").toCharArray(_wlanSSID, sizeof(_wlanSSID) - 1);
    server.arg("p").toCharArray(_wlanPassword, sizeof(_wlanPassword) - 1);
    server.sendHeader("Location", "wifi", true);
    sendStandardHeaders();
    server.send(302, "text/plain", "");     // Empty content inhibits Content-length header so we have to close the socket ourselves.
    server.client().stop();                 // Stop is needed because we sent no content length
    
    _currentState = State_t::SAVE_CREDENTIALS;      // force next step in state machine
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

#endif
