#include "cloud_sync.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <ctime>

#include "config.h"

namespace {
constexpr uint32_t kActivePollMs = 60000;
constexpr uint32_t kPingMs = 300000;
constexpr uint32_t kSyncMs = 3000;
constexpr size_t kBatchSize = 20;
constexpr time_t kMinimumValidTime = 1704067200; // 2024-01-01 UTC

bool isUuid(const String &value) {
  if (value.length() != 36) return false;
  for (size_t i = 0; i < 36; ++i) {
    char c = value[i];
    if (i == 8 || i == 13 || i == 18 || i == 23) {
      if (c != '-') return false;
    } else if (!isxdigit(static_cast<unsigned char>(c))) return false;
  }
  return true;
}
} // namespace

bool CloudSync::begin() {
  if (!ECOVOTE_DEVICE_API_KEY[0] ||
      (!ECOVOTE_BACKEND_CA_CERT[0] && !ECOVOTE_ALLOW_INSECURE_TLS)) {
    Serial.println("Cloud desabilitada: configure API key e certificado CA");
    return false;
  }
  if (!metadata_.begin("ecovote-cloud", false)) return false;
  String cached = metadata_.getString("campaign", "");
  if (isUuid(cached)) cached.toCharArray(campaignId_, sizeof(campaignId_));
  return xTaskCreatePinnedToCore(taskEntry, "ecovote-cloud", 12288, this, 1,
                                 nullptr, 0) == pdPASS;
}

void CloudSync::annotate(QueuedVote &event) {
  time_t now = time(nullptr);
  event.epoch = now >= kMinimumValidTime ? static_cast<uint32_t>(now) : 0;
  currentCampaign(event.campaignId);
}

void CloudSync::currentCampaign(char out[37]) {
  portENTER_CRITICAL(&campaignMux_);
  memcpy(out, campaignId_, sizeof(campaignId_));
  portEXIT_CRITICAL(&campaignMux_);
}

void CloudSync::taskEntry(void *parameter) {
  static_cast<CloudSync *>(parameter)->run();
}

int CloudSync::request(const char *method, const String &path,
                       const String &body, String &response) {
  WiFiClientSecure client;
  if (ECOVOTE_BACKEND_CA_CERT[0]) client.setCACert(ECOVOTE_BACKEND_CA_CERT);
  else client.setInsecure(); // explicit development opt-in only
  HTTPClient http;
  http.setTimeout(5000);
  if (!http.begin(client, String(ECOVOTE_BACKEND_BASE_URL) + path)) return -1;
  http.addHeader("x-device-api-key", ECOVOTE_DEVICE_API_KEY);
  http.addHeader("Content-Type", "application/json");
  int status = strcmp(method, "GET") == 0 ? http.GET() : http.POST(body);
  if (status > 0) response = http.getString();
  http.end();
  return status;
}

bool CloudSync::parseCampaignId(const String &json, char out[37]) {
  int key = json.indexOf("\"campaign_id\"");
  if (key < 0) return false;
  int colon = json.indexOf(':', key + 13);
  int quote = colon < 0 ? -1 : json.indexOf('"', colon + 1);
  int end = quote < 0 ? -1 : json.indexOf('"', quote + 1);
  if (end < 0) return false;
  String value = json.substring(quote + 1, end);
  if (!isUuid(value)) return false;
  value.toCharArray(out, 37);
  return true;
}

void CloudSync::fetchActive() {
  String response;
  int status = request("GET", "/active?mac_address=" + macAddress_, "", response);
  char id[37]{};
  if (status == 200 && parseCampaignId(response, id)) {
    portENTER_CRITICAL(&campaignMux_);
    memcpy(campaignId_, id, sizeof(id));
    portEXIT_CRITICAL(&campaignMux_);
    metadata_.putString("campaign", id);
  } else if (status == 404) {
    portENTER_CRITICAL(&campaignMux_);
    campaignId_[0] = '\0';
    portEXIT_CRITICAL(&campaignMux_);
    metadata_.remove("campaign");
  } else if (status != 200) {
    Serial.printf("Consulta de campanha falhou: HTTP %d\n", status);
  }
}

void CloudSync::sendPing() {
  String body = "{\"mac_address\":\"" + macAddress_ + "\",\"ip_address\":\"" +
                WiFi.localIP().toString() + "\",\"wifi_signal_dbm\":" +
                String(WiFi.RSSI()) + "}";
  String response;
  int status = request("POST", "/ping", body, response);
  if (status != 200) Serial.printf("Ping falhou: HTTP %d\n", status);
}

bool CloudSync::formatTimestamp(uint32_t epoch, char out[25]) {
  time_t raw = epoch ? epoch : time(nullptr);
  if (raw < kMinimumValidTime) return false;
  tm utc{};
  if (!gmtime_r(&raw, &utc)) return false;
  return strftime(out, 25, "%Y-%m-%dT%H:%M:%SZ", &utc) > 0;
}

void CloudSync::flushVotes() {
  char fallbackCampaign[37]{};
  currentCampaign(fallbackCampaign);
  QueuedVote batch[kBatchSize];
  size_t count = queue_.readBatch(batch, kBatchSize, store_.savedSequence(),
                                  fallbackCampaign);
  if (!count) return;
  String body = "{\"mac_address\":\"" + macAddress_ + "\",\"campaign_id\":\"" +
                String(batch[0].campaignId) + "\",\"votes\":[";
  for (size_t i = 0; i < count; ++i) {
    char timestamp[25];
    if (!formatTimestamp(batch[i].epoch, timestamp)) return;
    if (i) body += ',';
    body += "{\"selected_option\":\"";
    body += batch[i].option;
    body += "\",\"timestamp\":\"";
    body += timestamp;
    body += "\"}";
  }
  body += "]}";
  String response;
  int status = request("POST", "/sync", body, response);
  if (status == 200 && response.indexOf("\"success\":true") >= 0) {
    if (!queue_.acknowledge(batch[count - 1].sequence)) {
      Serial.println("Falha ao confirmar lote de votos na NVS");
    }
  } else {
    Serial.printf("Sincronizacao falhou: HTTP %d\n", status);
  }
}

void CloudSync::run() {
  configTime(0, 0, "pool.ntp.org", "a.st1.ntp.br");
  uint32_t lastActive = millis() - kActivePollMs;
  uint32_t lastPing = millis() - kPingMs;
  uint32_t lastSync = millis() - kSyncMs;
  while (true) {
    uint32_t now = millis();
    if (WiFi.status() == WL_CONNECTED && time(nullptr) >= kMinimumValidTime) {
      if (!macAddress_.length()) macAddress_ = WiFi.macAddress();
      if (now - lastActive >= kActivePollMs) {
        fetchActive();
        lastActive = now;
      }
      if (now - lastPing >= kPingMs) {
        sendPing();
        lastPing = now;
      }
      if (now - lastSync >= kSyncMs) {
        flushVotes();
        lastSync = now;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(250));
  }
}
