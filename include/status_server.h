#pragma once

#include <Arduino.h>
#include <WebServer.h>

#include "vote_store.h"

// Read-only HTTP API. Its own FreeRTOS task keeps slow HTTP clients and Wi-Fi
// reconnect attempts away from the sensor/display loop.
class StatusServer {
 public:
  explicit StatusServer(VoteStore &votes) : votes_(votes), server_(80) {}
  bool begin();

 private:
  static void taskEntry(void *parameter);
  void run();
  void addCors();
  void sendStatus();

  VoteStore &votes_;
  WebServer server_;
  String deviceId_;
};
