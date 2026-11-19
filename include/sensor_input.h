#pragma once

#include <Arduino.h>

#include "sensor_gate.h"

struct SensorEvents {
  bool a = false;
  bool b = false;
};

// ISRs only latch activation edges. The normal loop validates the signal,
// so no counting, flash write or display I/O happens inside an interrupt.
class SensorInput {
 public:
  void begin();
  SensorEvents poll(uint32_t nowMs);

 private:
  static void IRAM_ATTR onA();
  static void IRAM_ATTR onB();
  static SensorInput *instance_;

  portMUX_TYPE edgeMux_ = portMUX_INITIALIZER_UNLOCKED;
  volatile uint8_t edges_ = 0;
  SensorGate gateA_;
  SensorGate gateB_;
};
