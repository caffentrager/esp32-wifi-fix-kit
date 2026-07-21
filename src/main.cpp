#include <Arduino.h>
#include <Esp32WifiFix.h>

#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "src/secrets.h missing. Copy src/secrets.h.example to src/secrets.h and fill it in."
#endif

Esp32WifiFix wifiFix;

unsigned long lastStatusPrint = 0;

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n[boot] Esp32WifiFix demo");

  wifiFix.begin();
  wifiFix.beginResilient(WIFI_SSID, WIFI_PASSWORD);
}

void loop() {
  wifiFix.loopResilient();

  if (millis() - lastStatusPrint >= 10000) {
    lastStatusPrint = millis();
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("[status] connected, IP=%s RSSI=%ddBm\n",
                    WiFi.localIP().toString().c_str(), WiFi.RSSI());
    } else {
      Serial.printf("[status] not connected (status=%s)\n",
                    Esp32WifiFix::statusName(WiFi.status()));
    }
  }
}
