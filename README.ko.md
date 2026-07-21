[English](README.md) | [한국어](README.ko.md)

# ESP32 WiFi Fix Kit

ESP32-C3 보드 3개, 서로 다른 PlatformIO 프로젝트 3개인데, 매번 똑같은 WiFi 연결
문제를 겪고 매번 다시 고쳤다. 이 레포는 그 해결책들을 네 번째로 또 반복해서
풀지 않도록 재사용 가능한 라이브러리 하나로 모아둔 것이다.

## 해결하는 문제들

| 증상 | 유력한 원인 | 해결책 |
|---|---|---|
| SSID/비밀번호가 맞고 `WiFi.scanNetworks()`에도 보이는데 `WiFi.begin()`이 끝없이 돌고 disconnect reason이 매번 `2`(`AUTH_EXPIRE`), `WiFi.status()`는 `WL_DISCONNECTED`(6)에서 멈춤 | 가장 유력한 원인은 **공유기의 anti-flood(연결 시도 남용 차단) 기능이 반복적인 빠른 재연결 시도 때문에 보드의 MAC 주소를 일시적으로 블랙리스트에 올린 것** (코드/설정 변경 없이도 쿨다운 후 저절로 풀림). 2.4GHz 채널폭이 40MHz(HT40)로 설정된 것이 같은 증상의 원인으로 흔히 언급되지만, 실제 재검증에서는 공유기 관리자 화면으로 이미 20MHz임을 확인한 상태에서도 동일하게 실패했다 — 그래서 HT40은 확정 원인이 아니라 2차 용의자 정도로 취급할 것. 전체 타임라인은 [readai.md](readai.md) 참고. | `esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT20)`는 예방 차원에서 적용해둠(비용이 거의 없고, 다른 곳에서는 실제로 HT40 문제를 고치는 문서화된 방법이기도 함) — 하지만 진짜 중요한 해결책은 **재연결을 몰아치지 않는 것**이다: `loopResilient()`는 고정된 빠른 간격 대신 지수 백오프로 재시도해서, MAC 락아웃을 다시 유발하거나 연장시키지 않도록 한다. |
| 같은 보드에 다른 펌웨어/인증정보로 재플래시한 뒤 인증이 간헐적으로 실패 | 이전 펌웨어의 WiFi 상태(오래된 채널/BSSID)가 NVS에 남아있다가 새 연결 시도를 방해 | 매 `WiFi.begin()` 전에 `WiFi.disconnect(true, true)` (단순 연결 해제가 아니라 저장된 설정까지 삭제) |
| WiFi가 끝내 연결되지 않으면 `setup()`에서 영원히 멈춤 — WiFi랑 무관한 다른 주변장치(LCD, 웹서버)까지 같이 멈춤 | 탈출구 없는 블로킹 루프 `while (WiFi.status() != WL_CONNECTED) {}` | 논블로킹 연결: WiFi 여부와 상관없이 나머지는 먼저 다 띄우고, `loop()`에서 백오프하며 WiFi만 재시도 |

> **진단 팁:** disconnect reason `2`(`AUTH_EXPIRE`)는 PSK를 확인하기도 전,
> 원시 802.11 open-system-auth 단계에서 발생한다 — 즉 비밀번호가 틀려서
> 나는 에러가 절대 아니다. 비밀번호가 틀리면 그보다 늦은 4-way handshake
> 단계에서 실패한다. reason 2 실패에 비밀번호를 다시 확인하느라 시간
> 쓰지 말 것.

전체 디버깅 서사(증상, 막다른 길들, 채널폭 가설을 기각하게 만든 근거)는
[readai.md](readai.md)에 있다.

## 사용법

`lib/Esp32WifiFix/`를 본인 PlatformIO 프로젝트의 `lib/` 폴더에 복사하면 된다.

```cpp
#include <Esp32WifiFix.h>

Esp32WifiFix wifiFix;

void setup() {
  Serial.begin(115200);
  wifiFix.begin();  // NVS 캐시 정리 + HT20 강제

  // 옵션 A -- 블로킹, 가장 단순. setup()에서만 쓰는 스케치라면 충분.
  if (!wifiFix.connect(WIFI_SSID, WIFI_PASSWORD)) {
    // wifiFix.connect()가 이미 체크리스트를 출력했음
  }

  // 옵션 B -- 논블로킹. 스케치의 다른 부분(디스플레이, 서버, 센서)이 WiFi
  // 끊김 때문에 멈추면 안 되는 경우 권장.
  wifiFix.beginResilient(WIFI_SSID, WIFI_PASSWORD);
}

void loop() {
  wifiFix.loopResilient();  // 옵션 A만 쓴다면 호출해도 아무 효과 없음
  // ... 나머지 loop 코드
}
```

이 레포의 `src/main.cpp`는 그대로 빌드 가능한 예제(옵션 B)다. `src/secrets.h`만
만들어두면(아래 참고) `pio run`으로 바로 빌드된다.

## 하드웨어 참고사항

- ESP32-C3(SuperMini 클론, devkitm-1)에서 테스트함.
- ESP32는 5GHz를 지원하지 않는다 — 연결하려는 AP/SSID가 2.4GHz로 나오는지
  먼저 확인할 것.
- ESP32-C3 SuperMini 클론은 "Espressif USB JTAG/serial debug unit"으로
  잡히는 네이티브 USB 전용 보드라, 시리얼 포트가 제대로 잡히려면
  `platformio.ini`에 `-DARDUINO_USB_MODE=1 -DARDUINO_USB_CDC_ON_BOOT=1`이
  필요하다 — 이 레포의 `platformio.ini`에는 이미 설정되어 있음.

## 인증정보

`src/secrets.h`는 gitignore되어 있다. 빌드 전에 `src/secrets.h.example`을
`src/secrets.h`로 복사하고 본인 SSID/비밀번호를 채워넣을 것.

## 출처

[claude-desktop-buddy-diy](https://github.com/caffentrager/claude-desktop-buddy-diy)
작업 중 만들었던 세 개의 형제 프로젝트에서 뽑아냄: `wifi-connect-test`,
`usage-validation`, `claude-token-viewer`.

## 라이선스

MIT -- [LICENSE](LICENSE) 참고.
