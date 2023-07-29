#pragma once
#include <list>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <Arduino.h>

#define LOG_ERROR(s)    Logger().log(OXRS_LOG::LogLevel_t::ERROR, _LOG_PREFIX, s)
#define LOG_INFO(s)     Logger().log(OXRS_LOG::LogLevel_t::INFO, _LOG_PREFIX, s)
#define LOG_WARN(s)     Logger().log(OXRS_LOG::LogLevel_t::WARN, _LOG_PREFIX, s)
#define LOG_FATAL(s)    Logger().log(OXRS_LOG::LogLevel_t::FATAL, _LOG_PREFIX, s)
#define LOG_DEBUG(s)    Logger().log(OXRS_LOG::LogLevel_t::DEBUG, _LOG_PREFIX, s)

#define LOGF_ERROR(fmt, ...)    Logger().logf(OXRS_LOG::LogLevel_t::ERROR, _LOG_PREFIX, fmt, __VA_ARGS__)
#define LOGF_INFO(fmt, ...)     Logger().logf(OXRS_LOG::LogLevel_t::INFO, _LOG_PREFIX, fmt, __VA_ARGS__)
#define LOGF_WARN(fmt, ...)     Logger().logf(OXRS_LOG::LogLevel_t::WARN, _LOG_PREFIX, fmt, __VA_ARGS__)
#define LOGF_FATAL(fmt, ...)    Logger().logf(OXRS_LOG::LogLevel_t::FATAL, _LOG_PREFIX, fmt, __VA_ARGS__)
#define LOGF_DEBUG(fmt, ...)    Logger().logf(OXRS_LOG::LogLevel_t::DEBUG, _LOG_PREFIX, fmt, __VA_ARGS__)

#define ISLOG_DEBUG     (Logger().getLevel()==OXRS_LOG::LogLevel_t::DEBUG)

// Abstracts underying loggers: mqtt, serial, loki, syslog etc
class OXRS_LOG {
public:
    enum LogLevel_t {
        ALL=0,      // all log levels
        DEBUG,      // fine grained information events useful to debug an application
        INFO,       // coarse grained information events useful to monitor an application
        WARN,       // designates potentially harmful or impending failure situations
        ERROR,      // designates error events but may still allow the application to continue
        FATAL,      // application likely to fail
        OFF,
    };

    inline static const char *levelStr[] = { " ", "[DEBUG] ", "[INFO] ", "[WARN] ", "[ERROR] ", "[FATAL] ", " " };

    class AbstractLogger {
    public:
        virtual void log(LogLevel_t level, const __FlashStringHelper* logLine)=0;
        virtual void log(LogLevel_t level, const char* logLine)=0;
        virtual void log(LogLevel_t level, String& logLine)=0;
    };

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

    OXRS_LOG() : _currentLevel(DEBUG)
    {
        _loggers.push_back(&_serial);
    };

    LogLevel_t getLevel() {
        return _currentLevel;
    }

    void setLevel(LogLevel_t level)
    {
        _currentLevel = level;
        String logLine("Changed log level to ");
        logLine.concat(_currentLevel);
        log(INFO, "[OXRS_LOG] ", logLine);
    }

    // FIXME: use static char buffer to avoid memory fragmentation
    void logf(LogLevel_t level, const char* prefix, const char *fmt, ...)
    {
        if (level < _currentLevel)
            return;

        size_t initialLen = strlen(fmt);
        char *logEvent = new char[initialLen + 1];

        // get length of logEvent
        va_list args;
        va_start(args, fmt);
        size_t len = vsnprintf(logEvent, initialLen + 1, fmt, args);
        if (len > initialLen) {
            // extend length for logEvent
            delete[] logEvent;
            logEvent = new char[len + 1];
            vsnprintf(logEvent, len + 1, fmt, args);
        }
        va_end(args);

        log(level, prefix, logEvent);

        delete[] logEvent;
    }

    void log(LogLevel_t level, const char* prefix, const __FlashStringHelper *logEvent)
    {
        if (level >= _currentLevel) {
            String logLine(prefix);
            logLine.concat(levelStr[level]);
            logLine.concat(logEvent);
            for (std::list<AbstractLogger*>::iterator iter=_loggers.begin(); iter != _loggers.end(); ++iter) {
                (*iter)->log(level, logLine);
            }
        }
    }

    void log(LogLevel_t level, const char* prefix, char* logEvent)
    {
        if (level >= _currentLevel) {
            String logLine(prefix);
            logLine.concat(levelStr[level]);
            logLine.concat(logEvent);
            for (std::list<AbstractLogger*>::iterator iter=_loggers.begin(); iter != _loggers.end(); ++iter) {
                (*iter)->log(level, logLine);
            }
        }
    }

    void log(LogLevel_t level, const char* prefix, String& logEvent)
    {
        if (level >= _currentLevel) {
            String logLine(prefix);
            logLine.concat(levelStr[level]);
            logLine.concat(logEvent);
            for (std::list<AbstractLogger*>::iterator iter=_loggers.begin(); iter != _loggers.end(); ++iter) {
                (*iter)->log(level, logLine);
            }
        }
    }

private:
    LogLevel_t   _currentLevel;
    SerialLogger _serial;
    //SysLogger _sysLog;    // https://github.com/arcao/Syslog/blob/master/src/Syslog.cpp
    //MqttLogger _mqttLog;
    //LokiLogger _lokiLog;  // https://github.com/grafana/loki-arduino
    std::list<AbstractLogger*> _loggers;
};

namespace {
    OXRS_LOG& Logger() {
        static OXRS_LOG logger;
        return logger;
    };
}
