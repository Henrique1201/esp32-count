#pragma once

// Optional local credentials live in wifi_secrets.h (ignored by Git).
#if __has_include("wifi_secrets.h")
#include "wifi_secrets.h"
#endif

// An empty SSID leaves the device available through its own access point.
#ifndef ECOVOTE_WIFI_SSID
#define ECOVOTE_WIFI_SSID ""
#endif
#ifndef ECOVOTE_WIFI_PASSWORD
#define ECOVOTE_WIFI_PASSWORD ""
#endif

// At least 8 characters for the ESP32 WPA2 access point.
#ifndef ECOVOTE_AP_PASSWORD
#define ECOVOTE_AP_PASSWORD "ecovote123"
#endif

// GPIO assignments for the ESP32 DevKit and four chained MAX7219 modules.
#define ECOVOTE_SENSOR_A_PIN 26
#define ECOVOTE_SENSOR_B_PIN 27
#define ECOVOTE_MATRIX_DIN_PIN 23
#define ECOVOTE_MATRIX_CLK_PIN 18
#define ECOVOTE_MATRIX_CS_PIN 5

// Most obstacle sensor modules pull OUT low while an object is detected.
#define ECOVOTE_SENSOR_ACTIVE_LEVEL LOW
