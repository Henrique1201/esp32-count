#pragma once

#include <cstdint>

// One vote per sustained obstruction. The input must return to idle before
// another vote; a short interruption or a held obstruction cannot repeat it.
class SensorGate {
 public:
  bool update(bool active, bool activationEdge, uint32_t nowMs) {
    switch (state_) {
      case State::Armed:
        if (activationEdge && active) {
          sinceMs_ = nowMs;
          state_ = State::Qualifying;
        }
        break;
      case State::Qualifying:
        if (!active) {
          state_ = State::Armed;
        } else if (nowMs - sinceMs_ >= kConfirmMs) {
          state_ = State::Latched;
          return true;
        }
        break;
      case State::Latched:
        if (!active) {
          sinceMs_ = nowMs;
          state_ = State::Releasing;
        }
        break;
      case State::Releasing:
        if (active) {
          state_ = State::Latched;
        } else if (nowMs - sinceMs_ >= kReleaseMs) {
          state_ = State::Armed;
        }
        break;
    }
    return false;
  }

 private:
  enum class State { Armed, Qualifying, Latched, Releasing };
  static constexpr uint32_t kConfirmMs = 40;
  static constexpr uint32_t kReleaseMs = 150;
  State state_ = State::Armed;
  uint32_t sinceMs_ = 0;
};
