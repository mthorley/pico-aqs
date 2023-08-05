#include <OXRS_LOG.h>

static const char *_LOG_PREFIX = "[OXRS_LOG] ";

OXRS_LOG& oxrsLog = OXRS_LOG::getInstance();

OXRS_LOG::OXRS_LOG() : 
    _currentLevel(DEBUG)
{
    _loggers.push_back(&_serial);
};

OXRS_LOG& OXRS_LOG::getInstance() {
    static OXRS_LOG instance;
    return instance;
}

OXRS_LOG::LogLevel_t OXRS_LOG::getLevel() const
{
    return _currentLevel;
}

void OXRS_LOG::addLogger(AbstractLogger* pLogger)
{
    _loggers.push_back(pLogger);
}

void OXRS_LOG::setLevel(LogLevel_t level)
{
    _currentLevel = level;
    String logLine("Changed log level to ");
    logLine.concat(levelStr[_currentLevel]);
    log(INFO, "[OXRS_LOG] ", logLine);
}

// FIXME: use static char buffer to avoid memory fragmentation
void OXRS_LOG::logf(LogLevel_t level, const char* prefix, const char *fmt, ...)
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

void OXRS_LOG::log(LogLevel_t level, const char* prefix, const __FlashStringHelper *logEvent)
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

void OXRS_LOG::log(LogLevel_t level, const char* prefix, char* logEvent)
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

void OXRS_LOG::log(LogLevel_t level, const char* prefix, String& logEvent)
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

JsonVariant OXRS_LOG::findNestedKey(JsonObject obj, const String &key)
{
    JsonVariant foundObject = obj[key];
    if (!foundObject.isNull())
        return foundObject;

    for (JsonPair pair : obj)
    {
        JsonVariant nestedObject = findNestedKey(pair.value(), key);
        if (!nestedObject.isNull())
            return nestedObject;
    }

    return JsonVariant();
}

void OXRS_LOG::setLogLevelCommand(const String& sLogLevel)
{
    if (sLogLevel=="DEBUG")
        oxrsLog.setLevel(OXRS_LOG::LogLevel_t::DEBUG);
    if (sLogLevel=="INFO")
        oxrsLog.setLevel(OXRS_LOG::LogLevel_t::INFO);
    if (sLogLevel=="WARN")
        oxrsLog.setLevel(OXRS_LOG::LogLevel_t::WARN);
    if (sLogLevel=="ERROR")
        oxrsLog.setLevel(OXRS_LOG::LogLevel_t::ERROR);
    if (sLogLevel=="FATAL")
        oxrsLog.setLevel(OXRS_LOG::LogLevel_t::FATAL);
}

void OXRS_LOG::onConfig(JsonVariant json) {

    // Process log config changes
    JsonVariant jvLoglevel = findNestedKey(json, "loglevel");
    if (!jvLoglevel.isNull()) {
        String sLoglevel(jvLoglevel.as<String>().c_str());
        setLogLevelCommand(sLoglevel);
    }

    // iterate through all loggers for onConfig
    for (std::list<AbstractLogger*>::iterator iter=_loggers.begin(); iter != _loggers.end(); ++iter) {
        (*iter)->onConfig(json);
    }
}

void OXRS_LOG::setConfig(JsonVariant json)
{
    // Logging section
    JsonObject logConfig = json.createNestedObject("logging");
    logConfig["title"]   = "Logging";
    JsonObject logProps  = logConfig.createNestedObject("properties");

    // Log level
    JsonObject logLevel  = logProps.createNestedObject("loglevel");
    logLevel["title"]    = "Log Level";
    logLevel["description"] = "Applies to all loggers.";
    logLevel["default"]  = oxrsLog.getLevel();

    JsonArray logLevelEnum = logLevel.createNestedArray("enum");
    logLevelEnum.add("DEBUG");
    logLevelEnum.add("INFO");
    logLevelEnum.add("WARN");
    logLevelEnum.add("ERROR");
    logLevelEnum.add("FATAL");

    // iterate through all loggers to get config
    for (std::list<AbstractLogger*>::iterator iter=_loggers.begin(); iter != _loggers.end(); ++iter) {
        (*iter)->setConfig(logProps);
    }
}

// Serial logger
void OXRS_LOG::SerialLogger::log(LogLevel_t level, const __FlashStringHelper* logLine)
{
    Serial.println(logLine);
}

void OXRS_LOG::SerialLogger::log(LogLevel_t level, const char* logLine)
{
    Serial.println(logLine);
}

void OXRS_LOG::SerialLogger::log(LogLevel_t level, String& logLine)
{
    Serial.println(logLine);
}

// Mqtt Logger
void OXRS_LOG::MQTTLogger::log(LogLevel_t level, const __FlashStringHelper* logLine)
{
    if (_client.connected() && isEnabled()) {
        String s(logLine);
        _client.publish(_topic.c_str(), s.c_str());
    }
}

void OXRS_LOG::MQTTLogger::log(LogLevel_t level, const char* logLine)
{
    if (_client.connected() && isEnabled()) {
        _client.publish(_topic.c_str(), logLine);
    }
}

void OXRS_LOG::MQTTLogger::log(LogLevel_t level, String& logLine)
{
    if (_client.connected() && isEnabled()) {
        _client.publish(_topic.c_str(), logLine.c_str());
    }
}

void OXRS_LOG::MQTTLogger::setConfig(JsonVariant json)
{
    JsonObject mqttlog = json.createNestedObject("mqttlog");
    mqttlog["title"] = "MQTT Log";
    JsonObject mqttlogProps = mqttlog.createNestedObject("properties");
    JsonObject enable = mqttlogProps.createNestedObject(MQTTLOG_ENABLE);
    enable["type"]    = "boolean";
    enable["title"]   = "Enable";

    JsonObject deps = mqttlog.createNestedObject("dependencies");
    JsonObject mqttlogEnable = deps.createNestedObject(MQTTLOG_ENABLE);
    JsonArray oneOfArr = mqttlogEnable.createNestedArray("oneOf");
    JsonObject obj = oneOfArr.createNestedObject();
    JsonObject enableProps = obj.createNestedObject("properties");
    JsonObject mqttlogEnableProps = enableProps.createNestedObject(MQTTLOG_ENABLE);
    JsonArray arrEnum = mqttlogEnableProps.createNestedArray("enum");
    arrEnum.add(true);

    JsonObject serverProps = enableProps.createNestedObject(TOPIC_CONFIG);
    serverProps["type"]    = "string";
    serverProps["title"]   = "Topic";
    serverProps["description"] = "";
    serverProps["default"] = _topic;

    JsonArray required = obj.createNestedArray("required");
    required.add(TOPIC_CONFIG);

    obj = oneOfArr.createNestedObject();
    enableProps = obj.createNestedObject("properties");
    mqttlogEnableProps = enableProps.createNestedObject(MQTTLOG_ENABLE);
    arrEnum = mqttlogEnableProps.createNestedArray("enum");
    arrEnum.add(false);
}

void OXRS_LOG::MQTTLogger::onConfig(JsonVariant json)
{
    // Process log config changes
    JsonVariant jvEnable = findNestedKey(json, MQTTLOG_ENABLE);
    if (!jvEnable.isNull()) {
        bool enable = jvEnable.as<bool>();

        if (enable) {
            JsonVariant jvTopic = findNestedKey(json, TOPIC_CONFIG);
            if (!jvTopic.isNull()) {
                _topic = jvTopic.as<String>();
            }
        }

        if (!isEnabled() && enable) {
            setEnable(enable);
            LOG_DEBUG(F("MqttLog enabled"));
        }
        if (isEnabled() && !enable) {
            LOG_DEBUG(F("MqttLog disabled"));
            setEnable(enable);
        }
    }
}


// Syslog
void OXRS_LOG::SysLogger::setConfig(JsonVariant json)
{
    JsonObject syslog = json.createNestedObject("syslog");
    syslog["title"] = "Sys Log";
    JsonObject syslogProps = syslog.createNestedObject("properties");
    JsonObject enable = syslogProps.createNestedObject(SYSLOG_ENABLE);
    enable["type"]    = "boolean";
    enable["title"]   = "Enable";

    JsonObject deps = syslog.createNestedObject("dependencies");
    JsonObject syslogEnable = deps.createNestedObject(SYSLOG_ENABLE);
    JsonArray oneOfArr = syslogEnable.createNestedArray("oneOf");
    JsonObject obj = oneOfArr.createNestedObject();
    JsonObject enableProps = obj.createNestedObject("properties");
    JsonObject syslogEnableProps = enableProps.createNestedObject(SYSLOG_ENABLE);
    JsonArray arrEnum = syslogEnableProps.createNestedArray("enum");
    arrEnum.add(true);

    JsonObject serverProps = enableProps.createNestedObject(SERVER_CONFIG);
    serverProps["type"]    = "string";
    serverProps["title"]   = "Server";
    serverProps["description"] = "";

    JsonObject portProps = enableProps.createNestedObject(PORT_CONFIG);
    portProps["type"]    = "integer";
    portProps["title"]   = "Port";
    portProps["description"] = "";
    portProps["default"] = 514;

    JsonArray required = obj.createNestedArray("required");
    required.add("port");

    obj = oneOfArr.createNestedObject();
    enableProps = obj.createNestedObject("properties");
    syslogEnableProps = enableProps.createNestedObject(SYSLOG_ENABLE);
    arrEnum = syslogEnableProps.createNestedArray("enum");
    arrEnum.add(false);
}

void OXRS_LOG::SysLogger::log(LogLevel_t level, const __FlashStringHelper* logLine)
{
    if (!isEnabled())
        return;

    uint8_t facility = FAC_LOCAL0;
    uint8_t severity = getSeverity(level);
    uint8_t priority = (8 * facility) + severity;

    // This is a unit8 instead of a char because that's what udp.write() wants
    uint8_t buffer[MAX_PACKET_SIZE];
    String s(logLine);
    int len = snprintf((char*)buffer, MAX_PACKET_SIZE, "<%d>%s %s: %s", priority, _hostname.c_str(), _app.c_str(), s);

    // Send the raw UDP packet
    send(buffer, len);
}

void OXRS_LOG::SysLogger::log(LogLevel_t level, const char* logLine)
{
    if (!isEnabled())
        return;

    uint8_t facility = FAC_LOCAL0;
    uint8_t severity = getSeverity(level);
    uint8_t priority = (8 * facility) + severity;

    // This is a unit8 instead of a char because that's what udp.write() wants
    uint8_t buffer[MAX_PACKET_SIZE];
    int len = snprintf((char*)buffer, MAX_PACKET_SIZE, "<%d>%s %s: %s", priority, _hostname.c_str(), _app.c_str(), logLine);

    // Send the raw UDP packet
    send(buffer, len);
}

void OXRS_LOG::SysLogger::log(LogLevel_t level, String& logLine)
{
    if (!isEnabled())
        return;

    uint8_t facility = FAC_LOCAL0;
    uint8_t severity = getSeverity(level);
    uint8_t priority = (8 * facility) + severity;

    // This is a unit8 instead of a char because that's what udp.write() wants
    uint8_t buffer[MAX_PACKET_SIZE];
    int len = snprintf((char*)buffer, MAX_PACKET_SIZE, "<%d>%s %s: %s", priority, _hostname.c_str(), _app.c_str(), logLine.c_str());

    // Send the raw UDP packet
    send(buffer, len);
}

void OXRS_LOG::SysLogger::send(uint8_t* pBuffer, int len) 
{
    _syslogger.beginPacket(_server.c_str(), _port);
    _syslogger.write(pBuffer, len);
    _syslogger.endPacket();
}

uint8_t OXRS_LOG::SysLogger::getSeverity(LogLevel_t level)
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

void OXRS_LOG::SysLogger::onConfig(JsonVariant json)
{
    // Process log config changes
    JsonVariant jvEnable = findNestedKey(json, SYSLOG_ENABLE);
    if (!jvEnable.isNull()) {
        bool enable = jvEnable.as<bool>();

        if (enable) {
            JsonVariant jvServer = findNestedKey(json, SERVER_CONFIG);
            if (!jvServer.isNull()) {
                _server = jvServer.as<String>();
            }

            JsonVariant jvPort = findNestedKey(json, PORT_CONFIG);
            if (!jvPort.isNull()) {
                _port = jvPort.as<uint16_t>();
            }
        }

        if (!isEnabled() && enable) {
            setEnable(enable);
            LOG_DEBUG(F("Syslog enabled"));
        }
        if (isEnabled() && !enable) {
            LOG_DEBUG(F("Syslog disabled"));
            setEnable(enable);
        }
    }
}
