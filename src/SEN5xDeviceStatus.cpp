#include "SEN5xDeviceStatus.h"
#include "OXRS_LOG.h"

static const char* _LOG_PREFIX = "[SEN5xDeviceStatus] ";

SEN5xDeviceStatus::SEN5xDeviceStatus(SEN5x_model_t model)
{
    setConfig(model);
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

bool SEN5xDeviceStatus::isFanCleaningActive() const {
  std::bitset<32> b(_register);
  return b[FANCLEANING_BIT];
}

void SEN5xDeviceStatus::logStatus() const
{
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
                break;
              case warn:
                LOG_WARN(msg);
                break;
              case info:
                LOG_INFO(msg);
                break;
            }
        }
    }
}

void SEN5xDeviceStatus::setConfig(SEN5x_model_t model)
{
    switch(model) {
      case SEN50:
        _pstatusConfig = &statusConfig50;
      case SEN54:
        _pstatusConfig = &statusConfig54;
      default:
        _pstatusConfig = &statusConfig55;
    }
}
