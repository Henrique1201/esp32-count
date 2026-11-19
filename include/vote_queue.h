#pragma once

#include <Arduino.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <freertos/semphr.h>

#include "vote_store.h"

struct QueuedVote {
  uint32_t magic = 0;
  uint32_t sequence = 0;
  uint32_t epoch = 0; // UTC seconds, or 0 if NTP was not available
  char option = 'A';
  char campaignId[37]{}; // empty when no campaign was known at capture time
  uint32_t checksum = 0;
};

// Append-only durable outbox. The acknowledgement cursor is committed only
// after /sync replies 200. A fully acknowledged log can be removed safely.
class VoteQueue {
 public:
  bool begin();
  bool append(QueuedVote event);
  void recover(VoteStore &store);
  size_t readBatch(QueuedVote *out, size_t capacity, uint32_t savedSequence,
                   const char *fallbackCampaign);
  bool acknowledge(uint32_t sequence);

 private:
  static constexpr uint32_t kMagic = 0xEC0E2026u;
  static constexpr const char *kPath = "/votes.bin";
  static uint32_t checksum(const QueuedVote &event);
  static bool valid(const QueuedVote &event);
  bool scanValid();
  void clearIfFullyAcknowledged();

  Preferences metadata_;
  SemaphoreHandle_t mutex_ = nullptr;
  uint32_t acknowledgedSequence_ = 0;
  bool ready_ = false;
};
