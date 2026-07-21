[English](README.md) | [한국어](README.ko.md)

# ESP32 WiFi Fix Kit

Three ESP32-C3 boards, three separate PlatformIO projects, the same WiFi
connection failures hit and fixed over and over. This repo pulls those
fixes into one reusable library instead of re-solving them a fourth time.

## Problems this fixes

| Symptom | Cause | Fix |
|---|---|---|
| `WiFi.begin()` loops forever, `WiFi.status()` stuck at `WL_DISCONNECTED` (6) — even though the SSID is correct and shows up in `WiFi.scanNetworks()` | Router's 2.4GHz radio is broadcasting at 40MHz channel width (HT40). The ESP32 STA repeatedly fails association with `AUTH_EXPIRE` (disconnect reason 2). | Force 20MHz before connecting: `esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT20)` |
| Auth intermittently fails after reflashing the same board with different firmware/credentials | The previous firmware's WiFi state (old channel/BSSID) is still cached in NVS and interferes with the new association | `WiFi.disconnect(true, true)` (erases the saved config, not just a disconnect) before every `WiFi.begin()` |
| Firmware hangs forever in `setup()` if WiFi never connects — blocks unrelated peripherals (LCD, web server) that don't even need WiFi | A blocking `while (WiFi.status() != WL_CONNECTED) {}` loop with no escape | Non-blocking connect: start everything else regardless, retry WiFi from `loop()` on an interval |

Full debugging narrative (symptoms, dead ends, the router setting that
actually fixed it) is in [readai.md](readai.md).

## Usage

Copy `lib/Esp32WifiFix/` into your own PlatformIO project's `lib/` folder.

```cpp
#include <Esp32WifiFix.h>

Esp32WifiFix wifiFix;

void setup() {
  Serial.begin(115200);
  wifiFix.begin();  // clears stale NVS cache, forces HT20

  // Option A -- blocking, simplest, fine for setup()-only sketches:
  if (!wifiFix.connect(WIFI_SSID, WIFI_PASSWORD)) {
    // wifiFix.connect() already printed a troubleshooting checklist
  }

  // Option B -- non-blocking, recommended if anything else in the sketch
  // (display, server, sensors) shouldn't be blocked by WiFi being down:
  wifiFix.beginResilient(WIFI_SSID, WIFI_PASSWORD);
}

void loop() {
  wifiFix.loopResilient();  // no-op if you used Option A instead
  // ... rest of your loop
}
```

`src/main.cpp` in this repo is a complete runnable example (Option B) --
build it directly with `pio run` once `src/secrets.h` exists (see below).

## Hardware notes

- Tested on ESP32-C3 (SuperMini clones and devkitm-1).
- ESP32 does not support 5GHz -- confirm the AP/SSID you're targeting is
  broadcasting on 2.4GHz.
- ESP32-C3 SuperMini clones enumerate as "Espressif USB JTAG/serial debug
  unit" and need `-DARDUINO_USB_MODE=1 -DARDUINO_USB_CDC_ON_BOOT=1` in
  `platformio.ini` to get a working serial port -- already set in this
  repo's `platformio.ini`.

## Credentials

`src/secrets.h` is gitignored. Copy `src/secrets.h.example` to
`src/secrets.h` and fill in your own SSID/password before building.

## Source

Extracted from three sibling projects built while working on
[claude-desktop-buddy-diy](https://github.com/caffentrager/claude-desktop-buddy-diy):
`wifi-connect-test`, `usage-validation`, and `claude-token-viewer`.

## License

MIT -- see [LICENSE](LICENSE).
