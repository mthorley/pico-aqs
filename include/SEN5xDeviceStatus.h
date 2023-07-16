#pragma once
#include <Arduino.h>
#include <bitset>
#include <map>

typedef enum  {
    SEN50 = 1,
    SEN54,
    SEN55
} SEN5x_model_t;

class SEN5xDeviceStatus {
public:
  SEN5xDeviceStatus(SEN5x_model_t model);

  void setRegister(uint32_t _register);
  bool hasIssue() const;
  void logStatus() const;
  bool isFanCleaningActive() const;

private:
  void setConfig(SEN5x_model_t model);

  enum type_t {
    info=0,
    warn,
    error
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

  inline static uint8_t FANSPEED_BIT    = 21;
  inline static uint8_t FANCLEANING_BIT = 19;
  inline static uint8_t GASSENSOR_BIT   = 7;
  inline static uint8_t RHT_BIT         = 6;
  inline static uint8_t LASER_BIT       = 5;
  inline static uint8_t FANFAILURE_BIT  = 4;

  // status register configuration
  // https://sensirion.com/media/documents/6791EFA0/62A1F68F/Sensirion_Datasheet_Environmental_Node_SEN5x.pdf
  inline static const configmap_t statusConfig55 {
    {"fan_speed_range", statusbit_t(FANSPEED_BIT, "Fan speed is too high or too low. Automatically cleared once target speed reached.", type_t::warn)},
    {"fan_cleaning_active", statusbit_t(FANCLEANING_BIT, "Automatic fan cleaning in progress.", type_t::info)},
    {"gas_sensor", statusbit_t(GASSENSOR_BIT, "Gas sensor error (either VOC and/or NOx).", type_t::error)},
    {"rht_comms_error", statusbit_t(RHT_BIT, "Relative humidity temperature sensor internal communication error.", type_t::error)},
    {"laser", statusbit_t(LASER_BIT, "Laser is switched on and current is out of range.", type_t::error)},
    {"fan_failure", statusbit_t(FANFAILURE_BIT, "Fan failure, fan is mechanically blocked or broken.", type_t::error)}
  };

  inline static const configmap_t statusConfig54 {
    {"fan_speed_range", statusbit_t(FANSPEED_BIT, "Fan speed is too high or too low. Automatically cleared once target speed reached.", type_t::warn)},
    {"fan_cleaning_active", statusbit_t(FANCLEANING_BIT, "Automatic fan cleaning in progress.", type_t::info)},
    {"gas_sensor", statusbit_t(GASSENSOR_BIT, "Gas sensor error for VOC.", type_t::error)},
    {"rht_comms_error", statusbit_t(RHT_BIT, "Relative humidity temperature sensor internal communication error.", type_t::error)},
    {"laser", statusbit_t(LASER_BIT, "Laser is switched on and current is out of range.", type_t::error)},
    {"fan_failure", statusbit_t(FANFAILURE_BIT, "Fan failure, fan is mechanically blocked or broken.", type_t::error)}
  };

  inline static const configmap_t statusConfig50 {
    {"fan_speed_range", statusbit_t(FANSPEED_BIT, "Fan speed is too high or too low. Automatically cleared once target speed reached.", type_t::warn)},
    {"fan_cleaning_active", statusbit_t(FANCLEANING_BIT, "Automatic fan cleaning in progress.", type_t::info)},
    {"laser", statusbit_t(LASER_BIT, "Laser is switched on and current is out of range.", type_t::error)},
    {"fan_failure", statusbit_t(FANFAILURE_BIT, "Fan failure, fan is mechanically blocked or broken.", type_t::error)}
  };

  const configmap_t* _pstatusConfig;
  uint32_t _register;
};
