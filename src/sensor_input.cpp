#include "sensor_input.h"

#include "config.h"

SensorInput *SensorInput::instance_ = nullptr;

void IRAM_ATTR SensorInput::onA() {
  portENTER_CRITICAL_ISR(&instance_->edgeMux_);
  instance_->edges_ |= 1;
  portEXIT_CRITICAL_ISR(&instance_->edgeMux_);
}

void IRAM_ATTR SensorInput::onB() {
  portENTER_CRITICAL_ISR(&instance_->edgeMux_);
  instance_->edges_ |= 2;
  portEXIT_CRITICAL_ISR(&instance_->edgeMux_);
}

void SensorInput::begin() {
  instance_ = this;
  pinMode(ECOVOTE_SENSOR_A_PIN, INPUT_PULLUP);
  pinMode(ECOVOTE_SENSOR_B_PIN, INPUT_PULLUP);
  int edge = ECOVOTE_SENSOR_ACTIVE_LEVEL == LOW ? FALLING : RISING;
  attachInterrupt(digitalPinToInterrupt(ECOVOTE_SENSOR_A_PIN), onA, edge);
  attachInterrupt(digitalPinToInterrupt(ECOVOTE_SENSOR_B_PIN), onB, edge);
}

SensorEvents SensorInput::poll(uint32_t nowMs) {
  portENTER_CRITICAL(&edgeMux_);
  uint8_t pending = edges_;
  edges_ = 0;
  portEXIT_CRITICAL(&edgeMux_);
  bool activeA = digitalRead(ECOVOTE_SENSOR_A_PIN) == ECOVOTE_SENSOR_ACTIVE_LEVEL;
  bool activeB = digitalRead(ECOVOTE_SENSOR_B_PIN) == ECOVOTE_SENSOR_ACTIVE_LEVEL;
  return {gateA_.update(activeA, pending & 1, nowMs),
          gateB_.update(activeB, pending & 2, nowMs)};
}
