#pragma once

#include <Arduino.h>

#include "vote_store.h"

// Drives four chained MAX7219 modules (32 columns by 8 rows) directly over SPI.
// It owns the alternating A/B view, temporary vote feedback and text scrolling.
class MatrixDisplay {
 public:
  void begin();
  void showVote(VoteOption option, const VoteState &votes, uint32_t nowMs);
  void tick(const VoteState &votes, uint32_t nowMs);

 private:
  static constexpr uint8_t kModules = 4;
  static constexpr uint32_t kScreenMs = 1600;
  static constexpr uint32_t kFeedbackMs = 1200;
  static constexpr uint32_t kScrollMs = 120;

  static const uint8_t *glyph(char c);
  void command(uint8_t address, uint8_t data);
  void drawText(const String &message, uint16_t offset);

  uint32_t feedbackUntil_ = 0;
  uint32_t lastScreen_ = 0;
  uint32_t lastScroll_ = 0;
  uint8_t screen_ = 0;
  uint16_t scrollOffset_ = 0;
  bool redraw_ = true;
  bool messageDirty_ = true;
  bool showingFeedback_ = false;
  String message_;
};
