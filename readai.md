# readai.md — context for an AI assistant picking this up cold

## What this is

A small PlatformIO/Arduino library (`lib/Esp32WifiFix/`) that consolidates
fixes for three distinct ESP32 WiFi connection failures, extracted on
2026-07-21 from three separate sibling projects that had each independently
hit and solved (or partially solved) these problems:

- `~/Project/claude-desktop-buddy/wifi-connect-test/` — a standalone
  connectivity-test sketch; source of the HT20/HT40 fix and the NVS-clear
  fix (both in its `setup()`).
- `~/Project/claude-desktop-buddy/usage-validation/` — a usage-monitor
  sketch; source of the non-blocking background-reconnect pattern
  (`loop()`-driven retry, doesn't block the web server or LCD).
- `~/Project/claude-token-viewer/` — an LCD usage-viewer sketch; the most
  "naive" of the three (blocking connect, no HT20 fix, no NVS clear, hangs
  forever with "WiFi FAILED" on the LCD if the initial connect fails). Kept
  as a data point on what breaks without the fixes, not copied wholesale.

This repo is the "do it once, correctly" version so the next ESP32 project
doesn't reintroduce any of the three failure modes.

## The three root causes (shorthand)

1. **HT40 vs HT20 channel width.** Some home routers set the 2.4GHz radio
   to 40MHz channel width for extra throughput. The ESP32 Arduino WiFi
   stack's STA association with such an AP fails repeatedly with
   `AUTH_EXPIRE` (`WiFi.onEvent` disconnect reason `2`), even though the
   SSID/password are correct and the network is visible in
   `WiFi.scanNetworks()`. This is a well-known ESP32 compatibility gotcha,
   not specific to any one router brand. Fix: `esp_wifi_set_bandwidth
   (WIFI_IF_STA, WIFI_BW_HT20)` — must be called after `WiFi.mode(WIFI_STA)`
   (which initializes the underlying `esp_wifi` driver) and before
   `WiFi.begin()`.

2. **Stale NVS WiFi cache.** The ESP32 WiFi driver persists connection
   state (channel, BSSID, etc.) to NVS (flash) across reboots and even
   across reflashes with different firmware. A board that previously
   associated with a different AP, or the same AP under different
   conditions, can have this stale state interfere with a fresh connection
   attempt. Fix: `WiFi.disconnect(true, true)` — the two `true` args erase
   the stored config and disable STA, as opposed to a bare `WiFi.disconnect()`
   which only drops the current association. Call this once at the start of
   every boot, before `WiFi.begin()`.

3. **Blocking connect with no escape hatch.** The naive pattern —
   `while (WiFi.status() != WL_CONNECTED) { delay(500); }` with no timeout,
   or a timeout that then just spins forever (`while(true) delay(1000);`) —
   means any peripheral setup that comes after it in `setup()` never runs
   if WiFi is unavailable, even if that peripheral has nothing to do with
   networking (e.g. an LCD showing local sensor data). Fix: treat WiFi as
   an independently-retried background concern
   (`beginResilient()`/`loopResilient()` here) rather than a setup
   precondition. `usage-validation`'s `main.cpp` was the origin of this
   pattern (15s retry interval, non-blocking).

## API surface (`lib/Esp32WifiFix/Esp32WifiFix.h`)

- `begin()` — call once, applies fixes #1 and #2. Must run before either
  `connect()` or `beginResilient()`.
- `connect(ssid, password, timeoutMs=30000)` — blocking, fix #3 not
  applied (by design — this is the "I want it simple and my sketch has
  nothing else to do" path). Prints a scan + status trace; on timeout,
  prints a checklist instead of just returning false silently.
- `beginResilient(ssid, password, retryIntervalMs=15000)` +
  `loopResilient()` — non-blocking pair implementing fix #3.
  `loopResilient()` must be called every `loop()` iteration; it returns
  `true` exactly once, on the edge where the connection comes up (useful
  for triggering one-time post-connect work like an initial data poll).
- `statusName(wl_status_t)` / `scanAndPrint()` — static diagnostic helpers,
  used internally by `connect()` but also public for use in custom
  reconnect logic.

## Gotchas not obvious from the code

- `esp_wifi_set_bandwidth` silently no-ops (or can crash) if called before
  `esp_wifi` is initialized — that's why `begin()`'s internal ordering
  (`WiFi.mode` → `WiFi.onEvent` → `WiFi.disconnect(true,true)` → delay(200)
  → `esp_wifi_set_bandwidth`) matters and shouldn't be reordered casually.
  This exact sequence is what was confirmed working on real hardware in
  `wifi-connect-test`.
- The HT40 issue is a **router-side** setting, not fixable from firmware
  alone in the general case — forcing `WIFI_BW_HT20` on the ESP32 side
  works because it makes the ESP32 negotiate 20MHz regardless of what the
  router advertises, but if a given AP absolutely requires 40MHz-only
  (rare), this workaround won't help. Wasn't hit in practice; noting the
  theoretical edge.
- ESP32-C3 SuperMini clones need `-DARDUINO_USB_MODE=1
  -DARDUINO_USB_CDC_ON_BOOT=1` (already in this repo's `platformio.ini`) or
  they won't enumerate a usable serial port at all — easy to misdiagnose
  as a WiFi/board problem when it's actually a build-flags problem.
- **Never commit `src/secrets.h`.** It's gitignored here, but the sibling
  projects this was extracted from each have their own local
  `src/secrets.h` containing real home WiFi credentials — those files were
  read during extraction to confirm the working pattern but their contents
  were deliberately never copied into this repo. Only `secrets.h.example`
  (placeholder values) ships here.

## Non-goals

This kit only covers **STA-mode connection reliability**. It does not
cover: AP/SoftAP mode, WiFiManager-style captive-portal provisioning,
OTA, or the HTTP/web-server code that happened to live alongside the WiFi
code in the source projects — that's app-specific and was intentionally
left out of `lib/Esp32WifiFix/`.
