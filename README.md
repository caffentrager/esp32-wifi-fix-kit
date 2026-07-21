[English](README.md) | [한국어](README.ko.md)

# ESP32 WiFi Fix Kit

Three ESP32-C3 boards, three separate PlatformIO projects, the same WiFi
connection failures hit and fixed over and over. This repo pulls those
fixes into one reusable library instead of re-solving them a fourth time.

## Problems this fixes

| Symptom | Likely cause | Fix |
|---|---|---|
| `WiFi.begin()` loops forever, stuck at `WL_DISCONNECTED` (6) with disconnect reason `2` (`AUTH_EXPIRE`) on every attempt — even though the SSID/password are correct and the network shows up in `WiFi.scanNetworks()` | Most likely a **router-side anti-flood block temporarily blacklisting the board's MAC address** after repeated rapid reconnect attempts (self-clears after a cooldown, no code/config change needed). A 2.4GHz-set-to-40MHz-channel-width (HT40) router setting is commonly cited elsewhere for this same symptom, but in our own re-test the router was already confirmed at 20MHz via its admin panel and the failure still happened identically — so treat HT40 as a secondary suspect, not a confirmed cause. Full timeline in [readai.md](readai.md). | `esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT20)` is applied defensively (cheap, and a documented fix for the HT40 case elsewhere) — but the fix that actually matters is **not hammering reconnects**: `loopResilient()` backs off exponentially instead of retrying on a fixed fast interval, so it can't re-trigger or extend a MAC lockout. |
| Auth intermittently fails after reflashing the same board with different firmware/credentials | The previous firmware's WiFi state (old channel/BSSID) is still cached in NVS and interferes with the new association | `WiFi.disconnect(true, true)` (erases the saved config, not just a disconnect) before every `WiFi.begin()` |
| Firmware hangs forever in `setup()` if WiFi never connects — blocks unrelated peripherals (LCD, web server) that don't even need WiFi | A blocking `while (WiFi.status() != WL_CONNECTED) {}` loop with no escape | Non-blocking connect: start everything else regardless, retry WiFi from `loop()` with backoff |

> **Diagnostic tip:** disconnect reason `2` (`AUTH_EXPIRE`) fires at the raw 802.11
> open-system-auth stage, *before* the PSK is ever checked — so it's never a
> wrong-password problem. A bad password instead fails later, at the 4-way handshake.
> Don't waste time re-typing credentials for a reason-2 failure.

Full debugging narrative (symptoms, dead ends, what evidence ruled out the
channel-width theory) is in [readai.md](readai.md).

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
