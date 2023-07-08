#include "SEN5xDeviceStatus.h"
#include "OXRS_LOG.h"

static const char* _LOG_PREFIX = "[SEN5xDeviceStatus] ";

SEN5xDeviceStatus::SEN5xDeviceStatus(device_t device)
{
    setConfig(device);
};

// set device status register
void SEN5xDeviceStatus::setRegister(uint32_t _register)
{
    this->_register = _register;
}

// true if any warning or error bit is set
bool SEN5xDeviceStatus::hasIssue() const
{
    std::bitset<32> b(_register);
    bool warnOrError = false;

    // iterate through all status config bits
    for (auto iter = _pstatusConfig->begin(); iter!=_pstatusConfig->end(); iter++) {
        if (iter->second.type==warn || iter->second.type==error)
        warnOrError |= b[iter->second.bit_no];
    }
    return warnOrError;
}

void SEN5xDeviceStatus::logStatus()
{
    bool warnOrError = false;
    std::bitset<32> b(_register);

    // iterate through all status config bits
    for (auto iter = _pstatusConfig->begin(); iter != _pstatusConfig->end(); iter++)
    {
        // is status bit set?
        if (b[iter->second.bit_no] == 1)
        {
            String msg(iter->second.msg);
            switch (iter->second.type) {
              case error:
                LOG_ERROR(msg);
                warnOrError = true;
                break;
              case warn:
                LOG_WARN(msg);
                warnOrError = true;
                break;
              case info:
                LOG_INFO(msg);
                break;
            }
        }
    }

    if (!warnOrError)
        LOG_INFO(F("Device status ok"));
}

void SEN5xDeviceStatus::setConfig(device_t t)
{
    switch(t) {
      case sen50_sdn_t:
        _pstatusConfig = &statusConfig50;
      case sen54_sdn_t:
        _pstatusConfig = &statusConfig54;
      default:
        _pstatusConfig = &statusConfig55;
    }
}
