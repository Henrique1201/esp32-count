#include "matrix_display.h"

#include <SPI.h>

#include "config.h"

const uint8_t *MatrixDisplay::glyph(char c) {
  // Each glyph is five columns wide. Bit 0 is the first LED row.
  static const uint8_t font[][5] = {
      {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0
      {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
      {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
      {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3
      {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
      {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
      {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
      {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
      {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
      {0x06, 0x49, 0x49, 0x29, 0x1E}, // 9
      {0x7E, 0x11, 0x11, 0x11, 0x7E}, // A
      {0x7F, 0x49, 0x49, 0x49, 0x36}, // B
      {0x00, 0x36, 0x36, 0x00, 0x00}, // :
      {0x00, 0x00, 0x00, 0x00, 0x00}, // space
  };
  if (c >= '0' && c <= '9') return font[c - '0'];
  if (c == 'A') return font[10];
  if (c == 'B') return font[11];
  if (c == ':') return font[12];
  return font[13];
}

void MatrixDisplay::command(uint8_t address, uint8_t data) {
  digitalWrite(ECOVOTE_MATRIX_CS_PIN, LOW);
  for (uint8_t i = 0; i < kModules; ++i) {
    SPI.transfer(address);
    SPI.transfer(data);
  }
  digitalWrite(ECOVOTE_MATRIX_CS_PIN, HIGH);
}

void MatrixDisplay::begin() {
  pinMode(ECOVOTE_MATRIX_CS_PIN, OUTPUT);
  digitalWrite(ECOVOTE_MATRIX_CS_PIN, HIGH);
  SPI.begin(ECOVOTE_MATRIX_CLK_PIN, -1, ECOVOTE_MATRIX_DIN_PIN, ECOVOTE_MATRIX_CS_PIN);
  command(0x0F, 0); // display test off
  command(0x09, 0); // no BCD decode
  command(0x0B, 7); // all eight rows
  command(0x0A, 3); // moderate brightness
  command(0x0C, 1); // normal operation
  for (uint8_t row = 1; row <= 8; ++row) command(row, 0);
}

void MatrixDisplay::drawText(const String &message, uint16_t offset) {
  uint8_t columns[32]{};
  for (uint8_t x = 0; x < 32; ++x) {
    uint32_t source = static_cast<uint32_t>(x) + offset;
    uint32_t character = source / 6;
    if (character < message.length() && source % 6 < 5) {
      columns[x] = glyph(message[character])[source % 6];
    }
  }
  for (uint8_t row = 0; row < 8; ++row) {
    digitalWrite(ECOVOTE_MATRIX_CS_PIN, LOW);
    for (int module = kModules - 1; module >= 0; --module) {
      uint8_t bits = 0;
      for (uint8_t col = 0; col < 8; ++col) {
        if (columns[module * 8 + col] & (1u << row)) bits |= 1u << (7 - col);
      }
      SPI.transfer(row + 1);
      SPI.transfer(bits);
    }
    digitalWrite(ECOVOTE_MATRIX_CS_PIN, HIGH);
  }
}

void MatrixDisplay::showVote(VoteOption option, const VoteState &votes, uint32_t nowMs) {
  screen_ = option == VoteOption::B ? 1 : 0;
  feedbackUntil_ = nowMs + kFeedbackMs;
  lastScreen_ = nowMs;
  scrollOffset_ = 0;
  redraw_ = true;
  messageDirty_ = true;
  tick(votes, nowMs); // acknowledge the vote before the flash write
}

void MatrixDisplay::tick(const VoteState &votes, uint32_t nowMs) {
  bool feedback = static_cast<int32_t>(nowMs - feedbackUntil_) < 0;
  if (feedback != showingFeedback_) {
    showingFeedback_ = feedback;
    messageDirty_ = true;
    redraw_ = true;
  }
  if (!feedback && nowMs - lastScreen_ >= kScreenMs) {
    screen_ ^= 1;
    lastScreen_ = nowMs;
    scrollOffset_ = 0;
    redraw_ = true;
    messageDirty_ = true;
  }
  if (messageDirty_) {
    if (votes.a < 10 && votes.b < 10 && !feedback) {
      message_ = "A" + String(votes.a) + " B" + String(votes.b);
    } else {
      message_ = String(screen_ ? "B:" : "A:") + String(screen_ ? votes.b : votes.a);
    }
    messageDirty_ = false;
  }
  const uint16_t width = message_.length() * 6;
  if (width > 32 && nowMs - lastScroll_ >= kScrollMs) {
    lastScroll_ = nowMs;
    scrollOffset_ = scrollOffset_ < width - 32 ? scrollOffset_ + 1 : 0;
    redraw_ = true;
  }
  if (redraw_) {
    drawText(message_, scrollOffset_);
    redraw_ = false;
  }
}
