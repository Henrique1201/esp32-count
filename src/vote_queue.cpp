#include "vote_queue.h"

#include <cstring>

namespace {
class QueueLock {
 public:
  explicit QueueLock(SemaphoreHandle_t mutex) : mutex_(mutex) {
    xSemaphoreTake(mutex_, portMAX_DELAY);
  }
  ~QueueLock() { xSemaphoreGive(mutex_); }
 private:
  SemaphoreHandle_t mutex_;
};
} // namespace

uint32_t VoteQueue::checksum(const QueuedVote &event) {
  uint32_t hash = 2166136261u;
  auto mix = [&hash](uint8_t byte) { hash = (hash ^ byte) * 16777619u; };
  for (uint8_t shift = 0; shift < 32; shift += 8) mix(event.magic >> shift);
  for (uint8_t shift = 0; shift < 32; shift += 8) mix(event.sequence >> shift);
  for (uint8_t shift = 0; shift < 32; shift += 8) mix(event.epoch >> shift);
  mix(event.option);
  for (char byte : event.campaignId) mix(static_cast<uint8_t>(byte));
  return hash;
}

bool VoteQueue::valid(const QueuedVote &event) {
  return event.magic == kMagic && event.checksum == checksum(event) &&
         (event.option == 'A' || event.option == 'B') &&
         memchr(event.campaignId, '\0', sizeof(event.campaignId)) != nullptr;
}

bool VoteQueue::scanValid() {
  if (!LittleFS.exists(kPath)) return true;
  File file = LittleFS.open(kPath, "r");
  if (!file || file.size() % sizeof(QueuedVote) != 0) return false;
  uint32_t previous = 0;
  QueuedVote event;
  while (file.available()) {
    if (file.read(reinterpret_cast<uint8_t *>(&event), sizeof(event)) != sizeof(event) ||
        !valid(event) || (previous && event.sequence != previous + 1)) return false;
    previous = event.sequence;
  }
  return true;
}

bool VoteQueue::begin() {
  mutex_ = xSemaphoreCreateMutex();
  if (!mutex_) return false;
  if (!LittleFS.begin(false)) {
    Serial.println("Falha ao montar LittleFS; fila de votos indisponivel");
    return false;
  }
  if (!metadata_.begin("ecovote-q", false)) return false;
  acknowledgedSequence_ = metadata_.getUInt("ack", 0);
  ready_ = scanValid();
  if (!ready_) Serial.println("Log de votos invalido; sincronizacao suspensa");
  return ready_;
}

bool VoteQueue::append(QueuedVote event) {
  QueueLock lock(mutex_);
  if (!ready_) return false;
  event.magic = kMagic;
  event.checksum = checksum(event);
  File file = LittleFS.open(kPath, "a");
  if (!file) return false;
  bool written = file.write(reinterpret_cast<const uint8_t *>(&event), sizeof(event)) == sizeof(event);
  file.flush();
  file.close();
  if (!written) ready_ = false; // a partial record requires manual recovery
  return written;
}

void VoteQueue::recover(VoteStore &store) {
  if (!ready_ || !LittleFS.exists(kPath)) return;
  File file = LittleFS.open(kPath, "r");
  QueuedVote event;
  while (file && file.read(reinterpret_cast<uint8_t *>(&event), sizeof(event)) == sizeof(event)) {
    VoteState saved = store.snapshot();
    if (event.sequence <= saved.sequence) continue;
    if (event.sequence != saved.sequence + 1) {
      Serial.println("Lacuna no log de votos; recuperacao suspensa");
      break;
    }
    VoteState restored = store.record(event.option == 'A' ? VoteOption::A : VoteOption::B);
    if (!store.save(restored)) {
      Serial.println("Falha ao recuperar contagem na NVS");
      break;
    }
  }
}

size_t VoteQueue::readBatch(QueuedVote *out, size_t capacity, uint32_t savedSequence,
                            const char *fallbackCampaign) {
  QueueLock lock(mutex_);
  if (!ready_ || !capacity || !LittleFS.exists(kPath)) return 0;
  File file = LittleFS.open(kPath, "r");
  if (!file) return 0;
  size_t count = 0;
  QueuedVote event;
  char campaign[37]{};
  while (file.read(reinterpret_cast<uint8_t *>(&event), sizeof(event)) == sizeof(event)) {
    if (event.sequence <= acknowledgedSequence_) continue;
    if (event.sequence > savedSequence) break;
    const char *effectiveCampaign = event.campaignId[0] ? event.campaignId : fallbackCampaign;
    if (!effectiveCampaign || !effectiveCampaign[0]) break;
    if (count && strcmp(campaign, effectiveCampaign) != 0) break;
    if (!count) strncpy(campaign, effectiveCampaign, sizeof(campaign) - 1);
    if (!event.campaignId[0]) strncpy(event.campaignId, campaign, sizeof(event.campaignId) - 1);
    out[count++] = event;
    if (count == capacity) break;
  }
  return count;
}

bool VoteQueue::acknowledge(uint32_t sequence) {
  QueueLock lock(mutex_);
  if (sequence <= acknowledgedSequence_) return true;
  if (metadata_.putUInt("ack", sequence) != sizeof(uint32_t)) return false;
  acknowledgedSequence_ = sequence;
  clearIfFullyAcknowledged();
  return true;
}

void VoteQueue::clearIfFullyAcknowledged() {
  if (!LittleFS.exists(kPath)) return;
  File file = LittleFS.open(kPath, "r");
  if (!file || file.size() < sizeof(QueuedVote)) return;
  if (!file.seek(file.size() - sizeof(QueuedVote))) return;
  QueuedVote last;
  bool read = file.read(reinterpret_cast<uint8_t *>(&last), sizeof(last)) == sizeof(last);
  file.close();
  if (read && last.sequence <= acknowledgedSequence_) LittleFS.remove(kPath);
}
