#include <Arduino.h>
#include <esp_idf_version.h>
#include <esp_task_wdt.h>

#include "matrix_display.h"
#include "sensor_input.h"
#include "status_server.h"
#include "vote_store.h"

namespace {
VoteStore votes;
SensorInput sensors;
MatrixDisplay display;
StatusServer api(votes);

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
  display.begin();
  display.tick(votes.snapshot(), millis());
  sensors.begin();
  if (!api.begin()) Serial.println("Falha ao iniciar tarefa da API");
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
