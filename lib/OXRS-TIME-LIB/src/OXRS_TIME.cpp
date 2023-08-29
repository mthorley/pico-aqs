#include "OXRS_TIME.h"
#include <WiFi.h>

// NTP
NTPClass NTP;

OXRS_TIME::OXRS_TIME() :
    _ntpServer1(DEFAULT_NTP_SERVER1),
    _ntpServer2(DEFAULT_NTP_SERVER2) 
{}

void OXRS_TIME::begin()
{
    NTP.begin(_ntpServer1.c_str(), _ntpServer2.c_str());
}

void OXRS_TIME::setConfig(JsonVariant json)
{
    JsonObject timeConfig = json.createNestedObject("time");
    timeConfig["title"]   = "Time";
    JsonObject timeProps  = timeConfig.createNestedObject("properties");

    JsonObject ntpServer1 = timeProps.createNestedObject("ntp1");
    ntpServer1["title"]   = "NTP Server 1";
    ntpServer1["type"]    = "string";
    ntpServer1["format"] = "hostname";
    ntpServer1["default"] = DEFAULT_NTP_SERVER1;

    JsonObject ntpServer2 = timeProps.createNestedObject("ntp2");
    ntpServer2["title"]   = "NTP Server 2";
    ntpServer2["type"]    = "string";
    ntpServer2["format"] = "hostname";
    ntpServer2["default"] = DEFAULT_NTP_SERVER2;

    JsonObject tz = timeProps.createNestedObject("tz");
    tz["title"]   = "Timezone";
    tz["type"]    = "integer";
    tz["description"] = "\
-13..+13 = set timezone offset from UTC in hours.\
-13:00..+13:00 = set timezone offset from UTC in hours and minutes.\
99 = use timezone configured with TimeDst and TimeStd";
    tz["default"] = 99;

/*
H,W,M,D,h,T
H = hemisphere (0 = northern hemisphere / 1 = southern hemisphere)
W = week (0 = last week of month, 1..4 = first .. fourth)
M = month (1..12)
D = day of week (1..7 1 = Sunday 7 = Saturday)
h = hour (0..23) in local time
T = time zone (-780..780) (offset from UTC in MINUTES - 780min / 60min=13hrs)
*/
    JsonObject timeStd = timeProps.createNestedObject("timestd");
    timeStd["title"]   = "TimeStd";
    timeStd["type"]    = "string";
    timeStd["default"] = "1,1,4,1,3,600";

    JsonObject timeDst = timeProps.createNestedObject("timedst");
    timeDst["title"]   = "TimeDst";
    timeDst["type"]    = "string";
    timeDst["default"] = "1,1,10,1,2,660";
}
