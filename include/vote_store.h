#pragma once

#include <Arduino.h>
#include <Preferences.h>

enum class VoteOption { A, B };

struct VoteState {
  uint32_t a = 0;
  uint32_t b = 0;
  uint32_t sequence = 0;
};

// Owns the live count and the two alternating NVS records. The HTTP task
// reads snapshots while the sensor loop records votes.
class VoteStore {
 public:
  void begin();
  VoteState record(VoteOption option);
  bool save(const VoteState &state);
  VoteState snapshot();
  uint32_t savedSequence();

 private:
  struct SavedVoteState {
    VoteState votes;
    uint32_t checksum;
  };

  static uint32_t checksum(const VoteState &state);
  static bool valid(const SavedVoteState &state);
  bool readSlot(const char *key, SavedVoteState &state);

  Preferences storage_;
  VoteState votes_;
  uint32_t savedSequence_ = 0;
  uint8_t activeSlot_ = 0;
  portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
};
