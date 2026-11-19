#include <Arduino.h>
#include <esp_idf_version.h>
#include <esp_task_wdt.h>

#include "matrix_display.h"
#include "cloud_sync.h"
#include "sensor_input.h"
#include "status_server.h"
#include "vote_queue.h"
#include "vote_store.h"

namespace {
VoteStore votes;
VoteQueue queue;
SensorInput sensors;
MatrixDisplay display;
StatusServer api(votes);
CloudSync cloud(votes, queue);
bool queueReady = false;

void enableWatchdog() {
#if ESP_IDF_VERSION_MAJOR >= 5
  esp_task_wdt_config_t config{};
  config.timeout_ms = 5000;
  config.idle_core_mask = 0;
  config.trigger_panic = true;
  esp_task_wdt_init(&config);
#else
  esp_task_wdt_init(5, true);
#endif
  esp_task_wdt_add(nullptr);
}

void countVote(VoteOption option, uint32_t nowMs) {
  VoteState current = votes.record(option);
  display.showVote(option, current, nowMs);
  QueuedVote event{};
  event.sequence = current.sequence;
  event.option = option == VoteOption::A ? 'A' : 'B';
  cloud.annotate(event);
  if (queueReady && !queue.append(event)) {
    Serial.println("Falha ao colocar voto na fila offline");
  }
  votes.save(current);
  Serial.printf("Voto %c: A=%lu B=%lu\n", option == VoteOption::A ? 'A' : 'B',
                static_cast<unsigned long>(current.a),
                static_cast<unsigned long>(current.b));
}
} // namespace

void setup() {
  Serial.begin(115200);
  enableWatchdog();
  votes.begin();
  queueReady = queue.begin();
  if (queueReady) queue.recover(votes);
  display.begin();
  display.tick(votes.snapshot(), millis());
  sensors.begin();
  if (!api.begin()) Serial.println("Falha ao iniciar tarefa da API");
  if (queueReady) cloud.begin();
}

void loop() {
  uint32_t now = millis();
  SensorEvents events = sensors.poll(now);
  if (events.a) countVote(VoteOption::A, now);
  if (events.b) countVote(VoteOption::B, now);
  display.tick(votes.snapshot(), millis());
  esp_task_wdt_reset();
  delay(1);
}
