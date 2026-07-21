#include "Esp32WifiFix.h"

#include <esp_wifi.h>

namespace {
void onWifiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
    Serial.printf("[wifi] event: disconnected, reason=%d\n",
                  info.wifi_sta_disconnected.reason);
  }
}
}  // namespace

void Esp32WifiFix::begin() {
  WiFi.mode(WIFI_STA);
  WiFi.onEvent(onWifiEvent);

  // A board that previously ran different firmware (or connected to a
  // different AP) can leave a stale channel/BSSID cached in NVS that
  // blocks fresh auth. Clear it on every boot instead of trusting it.
  WiFi.disconnect(true, true);
  delay(200);

  // Some routers broadcast their 2.4GHz radio at 40MHz channel width
  // (HT40). The ESP32 STA then fails association with AUTH_EXPIRE
  // (disconnect reason 2) even though the SSID/password are correct and
  // the network shows up in scans. Forcing 20MHz avoids that failure
  // mode entirely.
  esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT20);
}

void Esp32WifiFix::scanAndPrint() {
  Serial.println("[wifi] scanning nearby networks...");
  int n = WiFi.scanNetworks();
  if (n == 0) {
    Serial.println("[wifi] scan: found nothing");
  } else {
    for (int i = 0; i < n; i++) {
      Serial.printf("[wifi] scan %2d: SSID='%s' RSSI=%d ch=%d enc=%d\n", i,
                     WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.channel(i),
                     (int)WiFi.encryptionType(i));
    }
  }
  Serial.println("[wifi] scan done ---");
}

const char* Esp32WifiFix::statusName(wl_status_t status) {
  switch (status) {
    case WL_IDLE_STATUS:
      return "IDLE";
    case WL_NO_SSID_AVAIL:
      return "NO_SSID_AVAIL";
    case WL_SCAN_COMPLETED:
      return "SCAN_DONE";
    case WL_CONNECTED:
      return "CONNECTED";
    case WL_CONNECT_FAILED:
      return "CONNECT_FAILED";
    case WL_CONNECTION_LOST:
      return "CONN_LOST";
    case WL_DISCONNECTED:
      return "DISCONNECTED";
    default:
      return "UNKNOWN";
  }
}

bool Esp32WifiFix::connect(const char* ssid, const char* password,
                            unsigned long timeoutMs) {
  scanAndPrint();

  WiFi.begin(ssid, password);
  Serial.printf("[wifi] connecting to %s\n", ssid);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
    delay(500);
    Serial.printf("[wifi] status=%s (%lus)\n", statusName(WiFi.status()),
                  (millis() - start) / 1000);
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(
        "[wifi] FAILED. Checklist: SSID/password correct? 2.4GHz band "
        "(ESP32 doesn't support 5GHz)? Router's 2.4GHz channel width set "
        "to 20MHz, not 40MHz/auto?");
    return false;
  }

  Serial.printf("[wifi] connected. IP = %s\n",
                WiFi.localIP().toString().c_str());
  return true;
}

void Esp32WifiFix::beginResilient(const char* ssid, const char* password,
                                   unsigned long retryIntervalMs) {
  _ssid = ssid;
  _password = password;
  _retryIntervalMs = retryIntervalMs;
  Serial.printf("[wifi] (background) connecting to %s\n", ssid);
  WiFi.disconnect();
  WiFi.begin(_ssid, _password);
  _lastAttemptMs = millis();
}

bool Esp32WifiFix::loopResilient() {
  bool connectedNow = WiFi.status() == WL_CONNECTED;

  if (connectedNow && !_wasConnected) {
    _wasConnected = true;
    Serial.printf("[wifi] connected. IP = %s\n",
                  WiFi.localIP().toString().c_str());
    return true;
  }

  if (!connectedNow) {
    _wasConnected = false;
    if (millis() - _lastAttemptMs >= _retryIntervalMs) {
      Serial.printf("[wifi] disconnected (status=%s) -- retrying\n",
                    statusName(WiFi.status()));
      WiFi.disconnect();
      WiFi.begin(_ssid, _password);
      _lastAttemptMs = millis();
    }
  }

  return false;
}
