#include "vote_store.h"

uint32_t VoteStore::checksum(const VoteState &s) {
  return 0xEC0B07E1u ^ s.a ^ (s.b << 7 | s.b >> 25) ^
         (s.sequence << 13 | s.sequence >> 19);
}

bool VoteStore::valid(const SavedVoteState &s) {
  return s.checksum == checksum(s.votes);
}

bool VoteStore::readSlot(const char *key, SavedVoteState &state) {
  return storage_.getBytesLength(key) == sizeof(state) &&
         storage_.getBytes(key, &state, sizeof(state)) == sizeof(state) && valid(state);
}

void VoteStore::begin() {
  if (!storage_.begin("ecovote", false)) {
    Serial.println("Falha ao abrir NVS");
    return;
  }
  SavedVoteState first{}, second{};
  bool hasFirst = readSlot("slot0", first);
  bool hasSecond = readSlot("slot1", second);
  if (hasFirst && (!hasSecond ||
                   static_cast<int32_t>(first.votes.sequence - second.votes.sequence) > 0)) {
    votes_ = first.votes;
    activeSlot_ = 0;
  } else if (hasSecond) {
    votes_ = second.votes;
    activeSlot_ = 1;
  }
  savedSequence_ = votes_.sequence;
}

VoteState VoteStore::snapshot() {
  portENTER_CRITICAL(&mux_);
  VoteState copy = votes_;
  portEXIT_CRITICAL(&mux_);
  return copy;
}

uint32_t VoteStore::savedSequence() {
  portENTER_CRITICAL(&mux_);
  uint32_t copy = savedSequence_;
  portEXIT_CRITICAL(&mux_);
  return copy;
}

VoteState VoteStore::record(VoteOption option) {
  portENTER_CRITICAL(&mux_);
  if (option == VoteOption::A) ++votes_.a;
  else ++votes_.b;
  ++votes_.sequence;
  VoteState copy = votes_;
  portEXIT_CRITICAL(&mux_);
  return copy;
}

bool VoteStore::save(const VoteState &state) {
  SavedVoteState saved{state, checksum(state)};
  uint8_t next = activeSlot_ ^ 1;
  const char *key = next ? "slot1" : "slot0";
  if (storage_.putBytes(key, &saved, sizeof(saved)) == sizeof(saved)) {
    activeSlot_ = next;
    portENTER_CRITICAL(&mux_);
    savedSequence_ = state.sequence;
    portEXIT_CRITICAL(&mux_);
    return true;
  } else {
    Serial.println("Erro ao salvar votos na NVS");
    return false;
  }
}
