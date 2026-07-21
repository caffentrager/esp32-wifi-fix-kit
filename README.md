[English](README.md) | [한국어](README.ko.md)

# ESP32 WiFi Fix Kit

Three ESP32-C3 boards, three separate PlatformIO projects, the same WiFi
connection failures hit and fixed over and over. This repo pulls those
fixes into one reusable library instead of re-solving them a fourth time.

## Problems this fixes

| Symptom | Likely cause | Fix |
|---|---|---|
| `WiFi.begin()` fails every single attempt, stuck at `WL_DISCONNECTED` (6) with disconnect reason `2` (`AUTH_EXPIRE`) — even though the SSID/password are correct, the network shows up in `WiFi.scanNetworks()`, **and this reproduces identically against multiple unrelated networks** (a different router, a guest SSID, a phone/PC hotspot) | A genuine **RF hardware defect on some ESP32-C3 SuperMini/clone boards**: bad antenna impedance matching reflects the default 19.5dBm TX power back into the radio and corrupts the auth exchange. Confirmed via [arduino-esp32 #6767](https://github.com/espressif/arduino-esp32/issues/6767) - not a router setting, not a software bug. (A router-side anti-flood MAC lockout was our first hypothesis and can produce similar symptoms in isolation, but doesn't explain failing against a brand-new network that's never seen the board before - see [readai.md](readai.md) for the full corrected timeline.) | `WiFi.setTxPower(WIFI_POWER_8_5dBm)` called **immediately after** `WiFi.begin()` (order matters - calling it before is a silent no-op, since the STA interface isn't up yet). This is a range-limiting workaround, not a hardware repair - expect a smaller reliable radius than a non-defective board. |
| Auth intermittently fails after reflashing the same board with different firmware/credentials | The previous firmware's WiFi state (old channel/BSSID) is still cached in NVS and interferes with the new association | `WiFi.disconnect(true, true)` (erases the saved config, not just a disconnect) before every `WiFi.begin()` |
| Firmware hangs forever in `setup()` if WiFi never connects — blocks unrelated peripherals (LCD, web server) that don't even need WiFi | A blocking `while (WiFi.status() != WL_CONNECTED) {}` loop with no escape | Non-blocking connect: start everything else regardless, retry WiFi from `loop()` with backoff |
| `WiFi.status()` reports `WL_NO_SSID_AVAIL` rather than `WL_DISCONNECTED` | Different problem from the row above - the board's scan never saw the SSID at all, usually a 2.4GHz/5GHz band mismatch (phone hotspots and Windows Mobile Hotspot both commonly default to 5GHz/auto) | Force the AP to 2.4GHz-only; ESP32 doesn't support 5GHz in any mode |

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
  wifiFix.begin();  // clears stale NVS cache, forces HT20, sets up event logging

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
- Some ESP32-C3 SuperMini/clone boards have a real antenna-matching
  defect that makes them unable to complete WiFi auth at full (19.5dBm)
  TX power -- see the `AUTH_EXPIRE` row above and
  [arduino-esp32 #6767](https://github.com/espressif/arduino-esp32/issues/6767).
  `connect()`/`beginResilient()` already apply the `WIFI_POWER_8_5dBm`
  workaround; if you're still seeing `reason=2` after that, the defect on
  your specific unit may be bad enough to need a hardware rework, not just
  lower TX power.

## Credentials

`src/secrets.h` is gitignored. Copy `src/secrets.h.example` to
`src/secrets.h` and fill in your own SSID/password before building.

## Source

Extracted from three sibling projects built while working on
[claude-desktop-buddy-diy](https://github.com/caffentrager/claude-desktop-buddy-diy):
`wifi-connect-test`, `usage-validation`, and `claude-token-viewer`.

## License

MIT -- see [LICENSE](LICENSE).
