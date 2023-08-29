#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

class OXRS_TIME {
public:
    OXRS_TIME();

    inline static const String DEFAULT_NTP_SERVER1 = "pool.ntp.org";
    inline static const String DEFAULT_NTP_SERVER2 = "time.nist.gov";

    void begin();

    static void setConfig(JsonVariant json);

private:

    // ntp endpoints
    String _ntpServer1;
    String _ntpServer2;
};
