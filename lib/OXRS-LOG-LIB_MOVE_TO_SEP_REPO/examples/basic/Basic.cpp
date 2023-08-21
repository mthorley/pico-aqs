
#include <OXRS_LOG.h>

static const char *_LOG_PREFIX = "[Basic]";

uint32_t i = 0;

void setup()
{
    oxrsLog.setLevel(LogLevel_t::DEBUG);
    LOG_INFO("setup");
}

void loop()
{
    LOGF_INFO("Loop counter %d", i++);

    if (i % 10) {
        LOG_DEBUG("Another 10");
    }

}
