[English](README.md) | [한국어](README.ko.md)

# ESP32 WiFi Fix Kit

ESP32-C3 보드 3개, 서로 다른 PlatformIO 프로젝트 3개인데, 매번 똑같은 WiFi 연결
문제를 겪고 매번 다시 고쳤다. 이 레포는 그 해결책들을 네 번째로 또 반복해서
풀지 않도록 재사용 가능한 라이브러리 하나로 모아둔 것이다.

## 해결하는 문제들

| 증상 | 원인 | 해결책 |
|---|---|---|
| SSID가 맞고 `WiFi.scanNetworks()`에도 보이는데 `WiFi.begin()`이 끝없이 돌고 `WiFi.status()`가 `WL_DISCONNECTED`(6)에서 멈춤 | 공유기 2.4GHz 대역이 채널폭 40MHz(HT40)로 브로드캐스트 중. ESP32 STA가 `AUTH_EXPIRE`(disconnect reason 2)로 계속 인증 실패 | 연결 전에 20MHz로 강제: `esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT20)` |
| 같은 보드에 다른 펌웨어/인증정보로 재플래시한 뒤 인증이 간헐적으로 실패 | 이전 펌웨어의 WiFi 상태(오래된 채널/BSSID)가 NVS에 남아있다가 새 연결 시도를 방해 | 매 `WiFi.begin()` 전에 `WiFi.disconnect(true, true)` (단순 연결 해제가 아니라 저장된 설정까지 삭제) |
| WiFi가 끝내 연결되지 않으면 `setup()`에서 영원히 멈춤 — WiFi랑 무관한 다른 주변장치(LCD, 웹서버)까지 같이 멈춤 | 탈출구 없는 블로킹 루프 `while (WiFi.status() != WL_CONNECTED) {}` | 논블로킹 연결: WiFi 여부와 상관없이 나머지는 먼저 다 띄우고, `loop()`에서 일정 간격으로 WiFi만 재시도 |

전체 디버깅 서사(증상, 막다른 길들, 실제로 해결한 공유기 설정)는
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
