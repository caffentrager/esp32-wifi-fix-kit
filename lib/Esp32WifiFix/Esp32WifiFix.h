#pragma once

#include <Arduino.h>
#include <WiFi.h>

// Consolidated fixes for the WiFi connection failures that kept recurring
// across separate ESP32-C3 projects. See the repo README for the story
// behind each one.
class Esp32WifiFix {
 public:
  // Call once from setup(), before connect()/beginResilient(). Resets STA
  // mode, wipes any stale NVS wifi cache left by a previous firmware/AP,
  // and forces 20MHz channel width.
  void begin();

  // Blocking connect with a timeout. Prints a network scan and a
  // status-code trace to Serial; on failure, prints a troubleshooting
  // checklist instead of hanging silently. Returns true if connected
  // within timeoutMs.
  bool connect(const char* ssid, const char* password,
               unsigned long timeoutMs = 30000);

  // Non-blocking variant: kicks off one connection attempt and returns
  // immediately, so setup() can continue configuring peripherals that
  // don't depend on WiFi.
  void beginResilient(const char* ssid, const char* password,
                       unsigned long retryIntervalMs = 15000);

  // Call every loop(). Retries in the background on retryIntervalMs
  // whenever disconnected. Returns true exactly once, on the iteration
  // where the connection transitions from down to up.
  bool loopResilient();

  static void scanAndPrint();
  static const char* statusName(wl_status_t status);

 private:
  const char* _ssid = nullptr;
  const char* _password = nullptr;
  unsigned long _retryIntervalMs = 15000;
  unsigned long _lastAttemptMs = 0;
  bool _wasConnected = false;
};
