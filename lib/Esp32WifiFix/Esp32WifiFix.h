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
  // and forces 20MHz channel width (a cheap defensive fix for one
  // possible cause of AUTH_EXPIRE). See readai.md: the reason=2 recurrence
  // we actually hit in our own testing was a board-side antenna-impedance-
  // matching defect (arduino-esp32 #6767), which begin() can't fix by
  // itself -- that needs the WIFI_POWER_8_5dBm TX-power workaround applied
  // in connect()/beginResilient().
  void begin();

  // Blocking connect with a timeout. Prints a network scan and a
  // status-code trace to Serial; on failure, prints a troubleshooting
  // checklist instead of hanging silently. Returns true if connected
  // within timeoutMs.
  bool connect(const char* ssid, const char* password,
               unsigned long timeoutMs = 30000);

  // Non-blocking variant: kicks off one connection attempt and returns
  // immediately, so setup() can continue configuring peripherals that
  // don't depend on WiFi. Retries start at retryIntervalMs and double on
  // each consecutive failure up to maxRetryIntervalMs -- still generally
  // good practice against transient network issues, but not the fix for
  // the reason=2 AUTH_EXPIRE recurrence in readai.md: that turned out to
  // be a board-side antenna-impedance-matching defect (arduino-esp32
  // #6767), and no amount of backoff helps without the WIFI_POWER_8_5dBm
  // TX-power workaround this function applies on every (re)connect
  // attempt.
  void beginResilient(const char* ssid, const char* password,
                       unsigned long retryIntervalMs = 15000,
                       unsigned long maxRetryIntervalMs = 120000);

  // Call every loop(). Retries in the background, backing off on repeated
  // failures, whenever disconnected. Returns true exactly once, on the
  // iteration where the connection transitions from down to up.
  bool loopResilient();

  static void scanAndPrint();
  static const char* statusName(wl_status_t status);

 private:
  const char* _ssid = nullptr;
  const char* _password = nullptr;
  unsigned long _retryIntervalMs = 15000;
  unsigned long _maxRetryIntervalMs = 120000;
  unsigned long _currentRetryIntervalMs = 15000;
  unsigned long _lastAttemptMs = 0;
  bool _wasConnected = false;
};
