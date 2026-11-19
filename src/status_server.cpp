#include "status_server.h"

#include <cstdio>
#include <cstring>
#include <WiFi.h>

#include "config.h"

bool StatusServer::begin() {
  return xTaskCreatePinnedToCore(taskEntry, "ecovote-api", 8192, this, 1,
                                 nullptr, 0) == pdPASS;
}

void StatusServer::taskEntry(void *parameter) {
  static_cast<StatusServer *>(parameter)->run();
}

void StatusServer::addCors() {
  server_.sendHeader("Access-Control-Allow-Origin", "*");
  server_.sendHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
  server_.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  server_.sendHeader("Access-Control-Allow-Private-Network", "true");
  server_.sendHeader("Cache-Control", "no-store");
}

void StatusServer::sendStatus() {
  VoteState votes = votes_.snapshot();
  char total[24];
  snprintf(total, sizeof(total), "%llu",
           static_cast<unsigned long long>(votes.a) + votes.b);
  String body = "{\"deviceId\":\"" + deviceId_ + "\",\"votesA\":" +
                String(votes.a) + ",\"votesB\":" + String(votes.b) +
                ",\"total\":" + String(total) + ",\"sequence\":" +
                String(votes.sequence) + ",\"wifiConnected\":" +
                String(WiFi.status() == WL_CONNECTED ? "true" : "false") +
                ",\"ip\":\"" + WiFi.localIP().toString() + "\",\"apIp\":\"" +
                WiFi.softAPIP().toString() + "\",\"rssi\":" + String(WiFi.RSSI()) +
                ",\"uptimeMs\":" + String(millis()) + "}";
  addCors();
  server_.send(200, "application/json; charset=utf-8", body);
}

void StatusServer::run() {
  WiFi.mode(WIFI_AP_STA);
  deviceId_ = WiFi.macAddress();
  deviceId_.replace(":", "");
  String apName = "EcoVote-" + deviceId_.substring(deviceId_.length() - 6);
  if (!WiFi.softAP(apName.c_str(), ECOVOTE_AP_PASSWORD)) {
    Serial.println("Falha ao iniciar ponto de acesso Wi-Fi");
  }
  if (strlen(ECOVOTE_WIFI_SSID)) WiFi.begin(ECOVOTE_WIFI_SSID, ECOVOTE_WIFI_PASSWORD);

  server_.on("/api/v1/status", HTTP_GET, [this]() { sendStatus(); });
  server_.on("/api/v1/status", HTTP_OPTIONS, [this]() {
    addCors();
    server_.send(204);
  });
  server_.onNotFound([this]() {
    addCors();
    server_.send(404, "application/json", "{\"error\":\"not_found\"}");
  });
  server_.begin();
  Serial.printf("AP: %s, API: http://%s/api/v1/status\n", apName.c_str(),
                WiFi.softAPIP().toString().c_str());

  uint32_t lastRetry = millis();
  while (true) {
    server_.handleClient();
    if (strlen(ECOVOTE_WIFI_SSID) && WiFi.status() != WL_CONNECTED &&
        millis() - lastRetry >= 10000) {
      WiFi.reconnect();
      lastRetry = millis();
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
