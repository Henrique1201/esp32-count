#pragma once

#include <Arduino.h>
#include <Preferences.h>

#include "vote_queue.h"
#include "vote_store.h"

// Pushes queued votes to ecovote-web over HTTPS and polls its active campaign.
// This task never runs in the sensor loop.
class CloudSync {
 public:
  CloudSync(VoteStore &store, VoteQueue &queue) : store_(store), queue_(queue) {}
  bool begin();
  void annotate(QueuedVote &event);

 private:
  static void taskEntry(void *parameter);
  void run();
  int request(const char *method, const String &path, const String &body, String &response);
  void fetchActive();
  void sendPing();
  void flushVotes();
  void currentCampaign(char out[37]);
  static bool parseCampaignId(const String &json, char out[37]);
  static bool formatTimestamp(uint32_t epoch, char out[25]);

  VoteStore &store_;
  VoteQueue &queue_;
  Preferences metadata_;
  portMUX_TYPE campaignMux_ = portMUX_INITIALIZER_UNLOCKED;
  char campaignId_[37]{};
  String macAddress_;
};
