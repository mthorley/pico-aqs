#include <OXRS_LOG.h>

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

OXRS_LOG::LogLevel_t OXRS_LOG::getLevel()
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
    logLine.concat(_currentLevel);
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
