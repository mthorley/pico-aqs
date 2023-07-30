#pragma once
#include <list>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFiUdp.h>

#define LOG_ERROR(s)  oxrsLog.log(OXRS_LOG::LogLevel_t::ERROR, _LOG_PREFIX, s)
#define LOG_INFO(s)   oxrsLog.log(OXRS_LOG::LogLevel_t::INFO,  _LOG_PREFIX, s)
#define LOG_WARN(s)   oxrsLog.log(OXRS_LOG::LogLevel_t::WARN,  _LOG_PREFIX, s)
#define LOG_FATAL(s)  oxrsLog.log(OXRS_LOG::LogLevel_t::FATAL, _LOG_PREFIX, s)
#define LOG_DEBUG(s)  oxrsLog.log(OXRS_LOG::LogLevel_t::DEBUG, _LOG_PREFIX, s)

#define LOGF_ERROR(fmt, ...)  oxrsLog.logf(OXRS_LOG::LogLevel_t::ERROR, _LOG_PREFIX, fmt, __VA_ARGS__)
#define LOGF_INFO(fmt, ...)   oxrsLog.logf(OXRS_LOG::LogLevel_t::INFO,  _LOG_PREFIX, fmt, __VA_ARGS__)
#define LOGF_WARN(fmt, ...)   oxrsLog.logf(OXRS_LOG::LogLevel_t::WARN,  _LOG_PREFIX, fmt, __VA_ARGS__)
#define LOGF_FATAL(fmt, ...)  oxrsLog.logf(OXRS_LOG::LogLevel_t::FATAL, _LOG_PREFIX, fmt, __VA_ARGS__)
#define LOGF_DEBUG(fmt, ...)  oxrsLog.logf(OXRS_LOG::LogLevel_t::DEBUG, _LOG_PREFIX, fmt, __VA_ARGS__)

#define ISLOG_DEBUG     (oxrsLog.getLevel()==OXRS_LOG::LogLevel_t::DEBUG)

#define MAX_PACKET_SIZE 256

#define PRI_EMERGENCY 0
#define PRI_ALERT     1
#define PRI_CRITICAL  2
#define PRI_ERROR     3
#define PRI_WARNING   4
#define PRI_NOTICE    5
#define PRI_INFO      6
#define PRI_DEBUG     7

#define FAC_USER   1
#define FAC_LOCAL0 16
#define FAC_LOCAL1 17
#define FAC_LOCAL2 18
#define FAC_LOCAL3 19
#define FAC_LOCAL4 20
#define FAC_LOCAL5 21
#define FAC_LOCAL6 22
#define FAC_LOCAL7 23

// Singleton that abstracts underying loggers: mqtt, serial, loki, syslog etc
class OXRS_LOG {
public:
    static OXRS_LOG &getInstance();

    // prevent copy construction
    OXRS_LOG(const OXRS_LOG& ) = delete;
    OXRS_LOG &operator=(const OXRS_LOG& ) = delete;

    enum LogLevel_t {
        ALL=0,      // all log levels
        DEBUG,      // fine grained information events useful to debug an application
        INFO,       // coarse grained information events useful to monitor an application
        WARN,       // designates potentially harmful or impending failure situations
        ERROR,      // designates error events but may still allow the application to continue
        FATAL,      // application likely to fail
        OFF,
    };

    inline static const char *levelStr[] = {
        " ",
        "[DEBUG] ",
        "[INFO] ",
        "[WARN] ",
        "[ERROR] ",
        "[FATAL] ",
        " "
    };

    // base class
    class AbstractLogger {
    public:
        virtual void log(LogLevel_t level, const __FlashStringHelper* logLine)=0;
        virtual void log(LogLevel_t level, const char* logLine)=0;
        virtual void log(LogLevel_t level, String& logLine)=0;
    };

    // Serial port logger
    class SerialLogger : public AbstractLogger {
    public:
        virtual void log(LogLevel_t level, const __FlashStringHelper* logLine) {
            Serial.println(logLine);
        }
        virtual void log(LogLevel_t level, const char* logLine) {
            Serial.println(logLine);
        }
        virtual void log(LogLevel_t level, String& logLine) {
            Serial.println(logLine);
        }
    };

    // MQTT logger
    class MQTTLogger : public AbstractLogger {
    public:
        MQTTLogger(PubSubClient& client, const char *topic) :
            _client(client), _topic(topic) {};

        void setTopic(const char* topic) {
            _topic = topic;
        }

        virtual void log(LogLevel_t level, const __FlashStringHelper* logLine) {
            if (_client.connected()) {
                String s(logLine);
                _client.publish(_topic.c_str(), s.c_str());
            }
        }
        virtual void log(LogLevel_t level, const char* logLine) {
            if (_client.connected()) {
                _client.publish(_topic.c_str(), logLine);
            }
        }
        virtual void log(LogLevel_t level, String& logLine) {
            if (_client.connected()) {
                _client.publish(_topic.c_str(), logLine.c_str());
            }
        }

    private:
        PubSubClient& _client;  // MQTT client
        String        _topic;   // log topic
    };

    class SysLogger : public AbstractLogger {
    public:
        SysLogger(const char* hostname, const char* server, uint16_t port=514) :
            _hostname(hostname),
            _app(FW_SHORT_NAME),
            _server(server),
            _port(port) {};

        virtual void log(LogLevel_t level, const __FlashStringHelper* logLine) {
            uint8_t priority = getPriority(level);

            // This is a unit8 instead of a char because that's what udp.write() wants
            uint8_t buffer[MAX_PACKET_SIZE];
            String s(logLine);
            int len = snprintf((char*)buffer, MAX_PACKET_SIZE, "<%d>%s %s: %s", priority, _hostname.c_str(), _app.c_str(), s);

            // Send the raw UDP packet
            send(buffer, len);
        }
        virtual void log(LogLevel_t level, const char* logLine) {
            uint8_t priority = getPriority(level);

            // This is a unit8 instead of a char because that's what udp.write() wants
            uint8_t buffer[MAX_PACKET_SIZE];
            int len = snprintf((char*)buffer, MAX_PACKET_SIZE, "<%d>%s %s: %s", priority, _hostname.c_str(), _app.c_str(), logLine);

            // Send the raw UDP packet
            send(buffer, len);
        }
        virtual void log(LogLevel_t level, String& logLine) {
            uint8_t priority = getPriority(level);

            // This is a unit8 instead of a char because that's what udp.write() wants
            uint8_t buffer[MAX_PACKET_SIZE];
            int len = snprintf((char*)buffer, MAX_PACKET_SIZE, "<%d>%s %s: %s", priority, _hostname.c_str(), _app.c_str(), logLine.c_str());

            // Send the raw UDP packet
            send(buffer, len);
        }

    private:
        void send(uint8_t* pBuffer, int len) 
        {
            _syslogger.beginPacket(_server.c_str(), _port);
            _syslogger.write(pBuffer, len);
            _syslogger.endPacket();
        }

        uint8_t getPriority(LogLevel_t level)
        {
            switch (level) {
                case DEBUG:
                    return PRI_DEBUG;
                case INFO:
                    return PRI_INFO;
                case FATAL:
                    return PRI_CRITICAL;
                case ERROR:
                    return PRI_ERROR;
                case WARN:
                    return PRI_WARNING;
                default:
                    return PRI_INFO;
            }
        }

        WiFiUDP  _syslogger;
        String   _hostname;
        String   _app;
        String   _server;
        uint16_t _port;
    };

    /*class LokiLogger : public AbstractLogger {
    public:
        virtual void log(LogLevel_t level, const __FlashStringHelper* logLine) {
            // map stream as log, label as device name
            //{
            //    stream: "log",
            //    label: "Sensirion55x-<clientID>"
            //    msg: logEvent
            //}
        }
        virtual void log(LogLevel_t level, const char* logLine) {
            //Serial.println(s);
        }
        virtual void log(LogLevel_t level, String& logLine) {
            //Serial.println(s);
        }
    };*/

    LogLevel_t getLevel();
    void setLevel(LogLevel_t level);
    void addLogger(AbstractLogger* pLogger);

    void logf(LogLevel_t level, const char* prefix, const char *fmt, ...);
    void log(LogLevel_t level, const char* prefix, const __FlashStringHelper *logEvent);
    void log(LogLevel_t level, const char* prefix, char* logEvent);
    void log(LogLevel_t level, const char* prefix, String& logEvent);

private:
    OXRS_LOG();

    LogLevel_t   _currentLevel;
    SerialLogger _serial;
    //SysLogger _sysLog;    // https://github.com/arcao/Syslog/blob/master/src/Syslog.cpp
    //LokiLogger _lokiLog;  // https://github.com/grafana/loki-arduino
    std::list<AbstractLogger*> _loggers;
};

extern OXRS_LOG &oxrsLog;
