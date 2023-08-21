/**
 * OXRS-LOG
 *
 * Logging singleton that allows combinations of different log types 
 * for use in the OXRS eco-system.
 *
 * Logging configuration can be managed via OXRS_API and any admin UI as follows:
 *    Overall log level (e.g. DEBUG, INFO, ERROR etc)
 *    Each logger can be enabled and configured
 *    - Serial   (default)
 *    - MQTT     (state, log topic)
 *    - Syslog   (state, serverIP, servicePort)
 *
 * New loggers can be added by extending the class AbstractLogger.
 */

#pragma once

#include <list>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>

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

/*
 * Singleton log that abstracts underying loggers
 */
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

    /* 
     * Abstrat base class for all loggers
     */
    class AbstractLogger {
    public:
        virtual void log(LogLevel_t level, const __FlashStringHelper* logLine)=0;
        virtual void log(LogLevel_t level, const char* logLine)=0;
        virtual void log(LogLevel_t level, String& logLine)=0;

        // OXRS callbacks
        virtual void onConfig(JsonVariant json)=0;
        virtual void setConfig(JsonVariant json)=0;

        bool isEnabled() const {
            return _enable;
        }

        void setEnable(bool e) {
            _enable = e;
        }

    private:
        bool _enable;   // logger enabled or not
    };

    /*
     * Serial port logger
     */
    class SerialLogger : public AbstractLogger {
    public:
        virtual void log(LogLevel_t level, const __FlashStringHelper* logLine);
        virtual void log(LogLevel_t level, const char* logLine);
        virtual void log(LogLevel_t level, String& logLine);

        virtual void onConfig(JsonVariant json) {};
        virtual void setConfig(JsonVariant json) {};
    };

    /*
     * MQTT logger
     */
    class MQTTLogger : public AbstractLogger {
    public:
        MQTTLogger(PubSubClient& client) :
            _client(client), _topic("") {};

        void setTopic(const char* topic) {
            _topic = topic;
        }

        virtual void log(LogLevel_t level, const __FlashStringHelper* logLine);
        virtual void log(LogLevel_t level, const char* logLine);
        virtual void log(LogLevel_t level, String& logLine);

        virtual void onConfig(JsonVariant json);
        virtual void setConfig(JsonVariant json);

        inline static const char* TOPIC_CONFIG   = "topic";
        inline static const char* MQTTLOG_ENABLE = "mqttlog_enable";

    private:
        PubSubClient& _client;  // MQTT client
        String        _topic;   // log topic
    };

    /*
     * Basic sys logger
     */
    class SysLogger : public AbstractLogger {
    public:
        SysLogger() :
            _hostname(""),      // FIXME: to be derived from network configuration
            _app(FW_SHORT_NAME),
            _server(""),
            _port(514) {};

        virtual void log(LogLevel_t level, const __FlashStringHelper* logLine);
        virtual void log(LogLevel_t level, const char* logLine);
        virtual void log(LogLevel_t level, String& logLine);

        virtual void onConfig(JsonVariant json);
        virtual void setConfig(JsonVariant json);

        inline static const char* SERVER_CONFIG = "server";
        inline static const char* PORT_CONFIG   = "port";
        inline static const char* SYSLOG_ENABLE = "syslog_enable";

        inline static uint16_t MAX_PACKET_SIZE = 256;

        inline static uint8_t PRI_EMERGENCY = 0;
        inline static uint8_t PRI_ALERT     = 1;
        inline static uint8_t PRI_CRITICAL  = 2;
        inline static uint8_t PRI_ERROR     = 3;
        inline static uint8_t PRI_WARNING   = 4;
        inline static uint8_t PRI_NOTICE    = 5;
        inline static uint8_t PRI_INFO      = 6;
        inline static uint8_t PRI_DEBUG     = 7;

        inline static uint8_t FAC_USER   = 1;
        inline static uint8_t FAC_LOCAL0 = 16;
        inline static uint8_t FAC_LOCAL1 = 17;
        inline static uint8_t FAC_LOCAL2 = 18;
        inline static uint8_t FAC_LOCAL3 = 19;
        inline static uint8_t FAC_LOCAL4 = 20;
        inline static uint8_t FAC_LOCAL5 = 21;
        inline static uint8_t FAC_LOCAL6 = 22;
        inline static uint8_t FAC_LOCAL7 = 23;

    private:
        void send(uint8_t* pBuffer, int len);
        uint8_t getSeverity(LogLevel_t level);

        WiFiUDP  _syslogger;    // FIXME: And if this is using ethernet?
        String   _hostname;
        String   _app;
        String   _server;
        uint16_t _port;
    };

    LogLevel_t getLevel() const;
    void setLevel(LogLevel_t level);

    // OXRS callbacks
    void onConfig(JsonVariant json);
    void setConfig(JsonVariant json);

    void setLogLevelCommand(const String& sLogLevel);

    void addLogger(AbstractLogger* pLogger);    // add logger to the list of loggers to use

    void logf(LogLevel_t level, const char* prefix, const char *fmt, ...);
    void log(LogLevel_t level, const char* prefix, const __FlashStringHelper *logEvent);
    void log(LogLevel_t level, const char* prefix, char* logEvent);
    void log(LogLevel_t level, const char* prefix, String& logEvent);

    static JsonVariant findNestedKey(JsonObject obj, const String &key);

private:
    OXRS_LOG();                             // singleton

    LogLevel_t   _currentLevel;             // current logging level of LogLevel_t
    SerialLogger _serial;                   // default Serial logger
    std::list<AbstractLogger*> _loggers;    // list of all loggers to log to
};

extern OXRS_LOG &oxrsLog;
