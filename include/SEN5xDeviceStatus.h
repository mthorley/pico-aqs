#pragma once
#include <Arduino.h>
#include <bitset>
#include <map>

class SEN5xDeviceStatus {
public:
  enum device_t {
    sen50_sdn_t = 1,
    sen54_sdn_t,
    sen55_sdn_t
  };

  SEN5xDeviceStatus(device_t device)
  {
      setConfig(device);
  };

  // set device status register
  void setRegister(uint32_t _register) 
  {
    this->_register = _register;
  }

  // true if any warning or error bit is set
  bool hasIssue() const 
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

  // return all messages for any type (info, warn, error)
  void getAllMessages(String& msg) const 
  {
    std::bitset<32> b(_register);
    for (auto iter = _pstatusConfig->begin(); iter!=_pstatusConfig->end(); iter++) {
      if (b[iter->second.bit_no]==1) {
        String line(enumTypetoString[iter->second.type]);
        line += ": ";
        line += iter->second.msg;
        msg += line;
      }
    }
  }

private:
  void setConfig(device_t t) 
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

  enum type_t { 
    info=1, warn, error
  };

  inline static const char *enumTypetoString[] = { 
    "Info", 
    "Warn", 
    "Error" 
  };

  typedef struct statusbit {
    statusbit(uint8_t bit_no, String msg, type_t type) 
      : bit_no(bit_no), msg(msg), type(type) {};

    uint8_t bit_no;   // bit number
    String  msg;      // failure message
    type_t  type;     // failure type
  } statusbit_t;

  typedef std::map<const std::string, const statusbit_t> configmap_t;

  // status register configuration 
  // https://sensirion.com/media/documents/6791EFA0/62A1F68F/Sensirion_Datasheet_Environmental_Node_SEN5x.pdf
  inline static const configmap_t statusConfig55 {
    {"fan_speed_range", statusbit_t(21, "Fan speed is too high or too low. Automatically cleared once target speed reached.", type_t::warn)},
    {"fan_cleaning_active", statusbit_t(19, "Automatic fan cleaning in progress.", type_t::info)},
    {"gas_sensor", statusbit_t(7, "Gas sensor error (either VOC and/or NOx).", type_t::error)},
    {"rht_comms_error", statusbit_t(6, "Relative humidity temperature sensor internal communication error.", type_t::error)},
    {"laser", statusbit_t(5, "Laser is switched on and current is out of range.", type_t::error)},
    {"fan_failure", statusbit_t(4, "Fan failure, fan is mechanically blocked or broken.", type_t::error)}
  };

  inline static const configmap_t statusConfig54 {
    {"fan_speed_range", statusbit_t(21, "Fan speed is too high or too low. Automatically cleared once target speed reached.", type_t::warn)},
    {"fan_cleaning_active", statusbit_t(19, "Automatic fan cleaning in progress.", type_t::info)},
    {"gas_sensor", statusbit_t(7, "Gas sensor error for VOC.", type_t::error)},
    {"rht_comms_error", statusbit_t(6, "Relative humidity temperature sensor internal communication error.", type_t::error)},
    {"laser", statusbit_t(5, "Laser is switched on and current is out of range.", type_t::error)},
    {"fan_failure", statusbit_t(4, "Fan failure, fan is mechanically blocked or broken.", type_t::error)}
  };

  inline static const configmap_t statusConfig50 {
    {"fan_speed_range", statusbit_t(21, "Fan speed is too high or too low. Automatically cleared once target speed reached.", type_t::warn)},
    {"fan_cleaning_active", statusbit_t(19, "Automatic fan cleaning in progress.", type_t::info)},
    {"laser", statusbit_t(5, "Laser is switched on and current is out of range.", type_t::error)},
    {"fan_failure", statusbit_t(4, "Fan failure, fan is mechanically blocked or broken.", type_t::error)}
  };

  const configmap_t* _pstatusConfig;
  uint32_t _register;
};
