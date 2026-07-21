[English](README.md) | [한국어](README.ko.md)

# ESP32 WiFi Fix Kit

ESP32-C3 보드 3개, 서로 다른 PlatformIO 프로젝트 3개인데, 매번 똑같은 WiFi 연결
문제를 겪고 매번 다시 고쳤다. 이 레포는 그 해결책들을 네 번째로 또 반복해서
풀지 않도록 재사용 가능한 라이브러리 하나로 모아둔 것이다.

## 해결하는 문제들

| 증상 | 유력한 원인 | 해결책 |
|---|---|---|
| SSID/비밀번호가 맞고 `WiFi.scanNetworks()`에도 보이는데 `WiFi.begin()`이 매번 실패하고 disconnect reason이 항상 `2`(`AUTH_EXPIRE`), `WiFi.status()`는 `WL_DISCONNECTED`(6)에서 멈춤. **게다가 서로 무관한 여러 네트워크(다른 공유기, 게스트 SSID, 핸드폰/PC 핫스팟)에 대해 전부 동일하게 실패** | 일부 ESP32-C3 SuperMini/클론 보드의 **실제 RF 하드웨어 결함**: 안테나 임피던스 매칭이 어긋나 있어서 기본 TX 출력(19.5dBm)이 반사되어 인증 과정 자체를 깨뜨림. [arduino-esp32 #6767](https://github.com/espressif/arduino-esp32/issues/6767)에서 확인됨 — 공유기 설정도, 소프트웨어 버그도 아님. (공유기의 anti-flood MAC 차단이 처음 세운 가설이었고 단독으로는 비슷한 증상을 낼 수 있지만, 한 번도 본 적 없는 새 네트워크에서도 똑같이 실패하는 건 설명이 안 됨 — 정정된 전체 타임라인은 [readai.md](readai.md) 참고.) | `WiFi.begin()` **직후에** `WiFi.setTxPower(WIFI_POWER_8_5dBm)` 호출 (순서 중요 — begin() 전에 호출하면 STA 인터페이스가 아직 안 떠서 조용히 무시됨). 이건 범위를 희생하는 우회책이지 하드웨어 수리가 아님 — 정상 보드보다 안정적으로 붙는 거리가 짧아질 수 있음. |
| 같은 보드에 다른 펌웨어/인증정보로 재플래시한 뒤 인증이 간헐적으로 실패 | 이전 펌웨어의 WiFi 상태(오래된 채널/BSSID)가 NVS에 남아있다가 새 연결 시도를 방해 | 매 `WiFi.begin()` 전에 `WiFi.disconnect(true, true)` (단순 연결 해제가 아니라 저장된 설정까지 삭제) |
| WiFi가 끝내 연결되지 않으면 `setup()`에서 영원히 멈춤 — WiFi랑 무관한 다른 주변장치(LCD, 웹서버)까지 같이 멈춤 | 탈출구 없는 블로킹 루프 `while (WiFi.status() != WL_CONNECTED) {}` | 논블로킹 연결: WiFi 여부와 상관없이 나머지는 먼저 다 띄우고, `loop()`에서 백오프하며 WiFi만 재시도 |
| `WiFi.status()`가 `WL_DISCONNECTED`가 아니라 `WL_NO_SSID_AVAIL`로 나옴 | 위와는 다른 문제 — 보드가 스캔에서 그 SSID 자체를 못 찾은 것. 보통 2.4GHz/5GHz 대역 불일치 (핸드폰 핫스팟과 Windows 모바일 핫스팟 둘 다 기본값이 5GHz/자동인 경우가 흔함) | AP를 2.4GHz 전용으로 강제 설정 — ESP32는 어떤 모드에서도 5GHz를 지원하지 않음 |

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
  wifiFix.begin();  // NVS 캐시 정리 + HT20 강제 + 이벤트 로깅 설정

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
- 일부 ESP32-C3 SuperMini/클론 보드는 기본(19.5dBm) TX 출력으로는 WiFi
  인증 자체를 완료 못 하는 실제 안테나 매칭 결함이 있다 — 위 `AUTH_EXPIRE`
  항목과 [arduino-esp32 #6767](https://github.com/espressif/arduino-esp32/issues/6767)
  참고. `connect()`/`beginResilient()`가 이미 `WIFI_POWER_8_5dBm` 우회책을
  적용하고 있음 — 그래도 계속 `reason=2`가 뜬다면, 해당 보드의 결함이
  TX 출력을 낮추는 정도로는 해결 안 될 만큼 심할 수 있어서 하드웨어 수리가
  필요할 수 있다.

## 인증정보

`src/secrets.h`는 gitignore되어 있다. 빌드 전에 `src/secrets.h.example`을
`src/secrets.h`로 복사하고 본인 SSID/비밀번호를 채워넣을 것.

## 출처

[claude-desktop-buddy-diy](https://github.com/caffentrager/claude-desktop-buddy-diy)
작업 중 만들었던 세 개의 형제 프로젝트에서 뽑아냄: `wifi-connect-test`,
`usage-validation`, `claude-token-viewer`.

## 라이선스

MIT -- [LICENSE](LICENSE) 참고.
