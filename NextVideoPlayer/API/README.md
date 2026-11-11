# WebSocket API

## 바로 사용하기

1. 플레이어의 **환경설정 → API** 탭을 엽니다.
2. **켜고 제어화면 열기**를 누릅니다. API 설정이 저장되고 브라우저에 제어화면이 열립니다.
3. 브라우저에서 재생·일시정지·정지·탐색·배속·음량을 조절합니다.

기본 WebSocket 주소는 `ws://127.0.0.1:49152/api/v1`이고 제어화면은 `http://127.0.0.1:49152/`입니다. IP와 포트는 API 탭에서 변경한 뒤 저장합니다. 포트 충돌이나 잘못된 IP로 서버를 시작하지 못하면 API 탭에 상태가 표시되고 일반 영상 재생은 계속 사용할 수 있습니다. 포트가 자동 변경되지는 않습니다.

## 설정 파일

처음 실행할 때 API는 꺼져 있습니다. 설정은 사용자별 `config.ini`에 저장됩니다.

```ini
[API]
Enabled=0
IP=127.0.0.1
Port=49152
```

- 일반 실행은 `%LOCALAPPDATA%\NextVideoPlayer\config.ini`를 사용합니다.
- MSIX 설치본은 해당 패키지의 `LocalState\config.ini`를 사용합니다.
- 여러 플레이어를 제어하려면 각 인스턴스에 별도 설정 파일과 포트를 지정합니다. 파일에 위 API 항목을 넣고 `Enabled=1`로 설정합니다.

```powershell
.\NextVideoPlayer.exe --config C:\PlayerConfig\screen1.ini
.\NextVideoPlayer.exe --config C:\PlayerConfig\screen2.ini
```

기본 IP는 같은 PC에서만 접속 가능합니다. 다른 PC에서 접속하려면 플레이어 PC의 수신 IP로 변경하고 Windows 방화벽에서 해당 포트의 수신을 허용합니다. 현재 API는 인증과 TLS를 제공하지 않으며, 수신 주소에 접근할 수 있는 프로그램이 플레이어를 제어할 수 있습니다. 브라우저 접속은 내장 제어화면과 같은 Origin으로 제한합니다.

## 메시지

UTF-8 JSON 텍스트를 보냅니다. 메시지 종류는 `"type":"all"` 하나입니다. 제어 키를 생략하거나 `null`, `""`를 보내면 해당 현재값을 유지합니다. `false`와 `0`은 실제 설정값으로 처리합니다.

```json
{"type":"all","requestId":"pause-1","playback":"paused"}
```

```json
{"type":"all","speed":1.5,"volume":30,"muted":false}
```

```json
{"type":"all","files":["C:\\Videos\\one.mp4","C:\\Videos\\two.mp4"],"playback":"playing","positionMs":0}
```

| 키 | 허용값과 동작 |
| --- | --- |
| `type` | 필수 문자열 `"all"` |
| `requestId` | 선택 문자열, 최대 128바이트. 응답에서 그대로 돌려줌 |
| `files` | 플레이어 PC에 존재하는 로컬 파일의 절대 경로 배열, 최대 512개. 기존 목록에 추가하고 이 선택 목록의 첫 파일을 재생. 빈 배열은 현재값 유지 |
| `playlistIndex` | 전체 재생목록에서 재생할 항목의 0부터 시작하는 인덱스 |
| `playback` | `"playing"`, `"paused"`, `"stopped"` |
| `positionMs` | 0 이상인 절대 재생 위치, 밀리초. 파일 로딩 중이면 준비 후 적용 |
| `speed` | `0.20`~`16.00`. 버튼의 배속 목록과 관계없이 범위 내 숫자를 지정 |
| `volume` | `0`~`100` 정수 |
| `muted`, `repeat`, `fullscreen`, `topMost` | JSON 불리언 `true` 또는 `false` |
| `subscribe` | 상태 알림 수신 여부. 연결 직후 기본 `true` |
| `executeAtMs` | 변경을 적용할 Unix epoch 밀리초. 앞으로 최대 30초까지 예약 |

`files`는 플레이어 PC의 로컬 파일을 여는 기능입니다. 중복 경로는 기존 목록 처리 규칙에 따라 합쳐집니다. `files`와 `playlistIndex`를 함께 보내면 파일을 추가한 후 전체 목록의 지정 인덱스를 재생합니다.

자막·구간 반복 등의 음수 값이 필요한 별도 명령은 현재 메시지에 포함하지 않습니다. 정의되지 않은 키, 잘못된 자료형, 범위 밖 값, 중복 키를 보내면 오류를 응답합니다. 정지 명령과 함께 보낸 재생 위치는 적용하지 않습니다.

## 조회와 응답

제어 키 없이 보내면 현재 상태를 조회합니다. `subscribe:false`로 상태 알림을 끄고 요청과 응답만 사용할 수 있습니다.

```json
{"type":"all","requestId":"query-1","subscribe":false}
```

응답은 `type`, `requestId`, `ok`, `status`, `state`를 포함합니다. 오류 응답에는 `error.code`와 `error.message`가 들어갑니다. 파싱 단계 오류에는 `state`가 생략될 수 있습니다.

- `status:"applied"`는 명령을 처리했거나 조회를 완료했다는 뜻입니다.
- `status:"accepted"`는 파일 로딩·탐색 또는 예약 실행을 접수했다는 뜻입니다. 영상 준비 완료는 `state.ready`와 `state.loading`으로 확인합니다. 로딩 실패는 `state.mediaError`로 확인합니다.
- 예약 명령은 같은 `requestId`로 접수 응답과 실행 시점의 응답을 각각 보냅니다.
- 상태 알림은 `{"type":"all","event":"state","state":{...}}` 형식이며 최대 약 30Hz로 전송됩니다. 느린 수신자에게는 최신 상태를 보냅니다.

`state`에는 `instanceId`, `version`, `protocolVersion`, `playback`, `file`, `ready`, `loading`, `mediaError`, `positionMs`, `durationMs`, `speed`, `volume`, `muted`, `repeat`, `fullscreen`, `topMost`, `playlistIndex`, `playlistCount`, `playlistRevision`, `timestampMs`가 있습니다. 요청 응답에는 전체 `files` 배열도 포함합니다. 재생목록에 선택 항목이 없으면 응답의 `playlistIndex`는 `-1`입니다.

`executeAtMs`는 UI 타이머 기준으로 처리합니다. 여러 PC의 시계 보정과 영상 동기화는 외부 제어 프로그램이 담당하며 100ms 이내 동기화를 보장하지 않습니다. API 설정 변경·종료 시 아직 실행하지 않은 예약은 취소됩니다.

## 외부 프로그램에서 테스트

추가 모듈 없이 PowerShell 7에서 동봉한 스크립트를 실행할 수 있습니다.

```powershell
pwsh -File .\API\Send-PlayerCommand.ps1
pwsh -File .\API\Send-PlayerCommand.ps1 -Message '{"type":"all","playback":"paused"}'
pwsh -File .\API\Send-PlayerCommand.ps1 -Endpoint ws://127.0.0.1:49153/api/v1 -Message '{"type":"all","speed":2.0}'
```

연결은 최대 8개, 메시지는 1MiB까지 지원합니다. 처리 대기열과 예약 대기열은 크기가 제한되어 있습니다. 과도한 요청은 `busy` 오류를 응답하거나 연결이 종료됩니다.

## 개발과 검증

Visual Studio 2026 C++ 데스크톱 도구, CMake 및 PowerShell 7이 필요합니다. 프로젝트를 빌드하면 `setup_websocket.ps1`이 고정된 소스 아카이브의 SHA256을 검증하고 libwebsockets를 정적 라이브러리로 빌드합니다. 의존성 소스와 빌드 결과는 `artifacts/`에 캐시합니다. 원본과 라이선스는 `ThirdParty/libwebsockets` 안내를 참고하세요.

`App`이 `PlayerControlService`, `UiDispatcher`, `ApiService`를 소유합니다. 네트워크 스레드는 메시지를 파싱하고 제한된 큐에 전달합니다. 모델 변경과 상태 조회는 UI 스레드에서 실행합니다. `ApiServer`는 수신·송신과 연결 수명만 관리합니다.

실행 테스트는 아래 명령으로 수행합니다. 테스트 창은 비활성 Windows 데스크톱에 생성하며 현재 사용자 설정을 변경하지 않습니다.

```powershell
python .\Tests\api_integration.py
python .\Tests\api_integration.py --stress-seconds 60
```
