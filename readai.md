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

1. **AUTH_EXPIRE (reason 2) — most likely a router MAC lockout, not (only)
   channel width.** This one has a real timeline worth preserving in full,
   because the first hypothesis reached in this project turned out to be
   incomplete, and the stale version of it is still what's written in the
   sibling projects' own code comments and `research.md` (they were never
   updated — this file is the corrected version; trust this over those if
   they ever disagree).

   - **2026-07-19, `usage-validation/`:** hit `WiFi.begin()` looping
     forever with `WL_DISCONNECTED` (6), correct SSID/password, network
     visible in scan. Router's 2.4GHz channel width happened to be set to
     `40MHz` (HT40); switching it to `20MHz` on the router connected
     immediately on the first try. Concluded: HT40 causes ESP32
     `AUTH_EXPIRE` failures, a fix known elsewhere in the ESP32 community
     too. This is a real, well-documented ESP32 compatibility issue in
     general — the mistake was concluding it was *the* explanation for
     every recurrence.
   - **2026-07-20, `wifi-connect-test/`:** built specifically to
     re-confirm the fix on a second minimal sketch, same board, same AP.
     Before touching anything, checked the router's admin panel and
     screenshotted it: 2.4GHz was **already** at 20MHz (`b,g,n`,
     WPA2PSK+AES, no 802.1x) — i.e. the HT40 condition from three days
     earlier no longer applied at all. Ran it anyway: `WiFi.onEvent` fired
     `reason=2` (`AUTH_EXPIRE`) on every attempt, identically to the
     earlier failure, for several retry cycles. Then, several minutes
     later, with **zero code or router-config changes**, the very next
     attempt connected on the first try (`status=3`).
   - **Revised conclusion:** the channel-width fix on 2026-07-19 likely
     worked for an unrelated reason (or coincidentally, if a lockout had
     already expired by the time it was tried) — the failure mode itself
     is better explained by the router's anti-flood / brute-force
     protection temporarily blacklisting the board's MAC address
     (`1C:DB:D4:3B:E2:88` in the observed case) after repeated rapid
     reconnect attempts made *while debugging*, self-clearing after a
     cooldown period. This is a hypothesis, not something confirmed via
     router logs (consumer router admin UI didn't expose that), but it
     fits the evidence better than channel width does: channel width was
     already correct and the symptom persisted anyway.
   - **What this means for the code in this repo:**
     `esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT20)` is still applied
     in `begin()` — it's a zero-cost, well-documented fix for genuine HT40
     cases elsewhere, no reason to remove it. But it should not be trusted
     as *the* fix for a reason-2 failure, and the library's actual
     defense against the lockout theory is `beginResilient()` /
     `loopResilient()`'s **exponential backoff** (starts at
     `retryIntervalMs`, doubles per consecutive failure up to
     `maxRetryIntervalMs`): a fixed fast retry interval (the original
     `usage-validation` pattern retried every 15s, forever) is exactly the
     "hammering reconnects" behavior that plausibly extends or re-triggers
     a lockout, so it was changed to back off instead of copying that
     pattern as-is.
   - **Diagnostic aside worth keeping:** disconnect reason `2`
     (`AUTH_EXPIRE`) fires at the raw 802.11 open-system-auth stage,
     before the PSK is ever checked — so it is categorically not a
     wrong-password symptom. A wrong password instead fails later, at the
     4-way handshake, with a different disconnect reason. Registering
     `WiFi.onEvent()` to print the reason code (as `begin()` does here) is
     the fast way to tell these apart instead of re-typing credentials to
     rule it out.

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
   pattern (fixed 15s retry interval, non-blocking); this repo kept the
   non-blocking shape but changed the fixed interval to backoff, per #1.

## API surface (`lib/Esp32WifiFix/Esp32WifiFix.h`)

- `begin()` — call once, applies fixes #1 (defensively) and #2. Must run
  before either `connect()` or `beginResilient()`.
- `connect(ssid, password, timeoutMs=30000)` — blocking, fix #3 not
  applied (by design — this is the "I want it simple and my sketch has
  nothing else to do" path). Prints a scan + status trace; on timeout,
  prints a checklist instead of just returning false silently.
- `beginResilient(ssid, password, retryIntervalMs=15000, maxRetryIntervalMs=120000)`
  + `loopResilient()` — non-blocking pair implementing fix #3.
  `loopResilient()` must be called every `loop()` iteration; it returns
  `true` exactly once, on the edge where the connection comes up (useful
  for triggering one-time post-connect work like an initial data poll).
  Retry interval doubles on each consecutive failure, capped at
  `maxRetryIntervalMs`, and resets to `retryIntervalMs` on the next
  successful connect.
- `statusName(wl_status_t)` / `scanAndPrint()` — static diagnostic helpers,
  used internally by `connect()` but also public for use in custom
  reconnect logic.

## Gotchas not obvious from the code

- `esp_wifi_set_bandwidth` silently no-ops (or can crash) if called before
  `esp_wifi` is initialized — that's why `begin()`'s internal ordering
  (`WiFi.mode` → `WiFi.onEvent` → `WiFi.disconnect(true,true)` → delay(200)
  → `esp_wifi_set_bandwidth`) matters and shouldn't be reordered casually.
  This exact sequence is what was confirmed working on real hardware in
  `wifi-connect-test`, independent of whether HT20 itself was the actual
  fix for any given failure (see #1).
- **`research.md` and `wifi-connect-test/src/main.cpp`'s code comment in
  the sibling `claude-desktop-buddy` repo still state the original,
  superseded HT40-only explanation as settled fact** — they were written
  on 2026-07-19 and never updated after the 2026-07-20 re-test falsified
  the "channel width alone explains it" reading. If those files and this
  one ever seem to disagree, this file (and the auto-memory project note
  it was extracted from) has the corrected account.
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
