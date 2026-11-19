#include "sensor_gate.h"
#include <cassert>

int main() {
  SensorGate gate;
  SensorGate other;
  assert(!gate.update(true, true, 0));
  assert(!gate.update(false, false, 10)); // noise
  assert(!gate.update(true, true, 100));
  assert(!other.update(true, true, 110));
  assert(!gate.update(true, false, 139));
  assert(gate.update(true, false, 140));
  assert(other.update(true, false, 150)); // independent duto
  assert(!gate.update(true, true, 500)); // held beam
  assert(!gate.update(false, false, 600));
  assert(!gate.update(true, true, 650)); // brief release
  assert(!gate.update(false, false, 700));
  assert(!gate.update(false, false, 849));
  assert(!gate.update(false, false, 850));
  assert(!gate.update(true, true, 900));
  assert(gate.update(true, false, 940));
}
