곰플레이어의 광고 난발 및 유료화와 windows media player legacy 의 반복영상중단 문제로 인해 화가 나서 개발하는 동영상 플레이어

C++ WinAPI 기반의 MVP 패턴으로 개발 (Model-View-Presenter)

MSI·MSIX 빌드 및 Microsoft Store/GitHub Release 배포 준비: [패키징 안내](Packaging/README.md)

libmpv / libVLC 같은 오픈소스 미디어 코어를 연동해 빠르게 완성도 높은 플레이어 구축
FFmpeg + Direct2D/D3D11 + PeekMessage 루프 구축
GPU 가속을 통한 배속 지원 가능해야함

- libmpv (LGPLv2.1+ / ISC)
- FFmpeg (LGPLv2.1+)
- libass (ISC)
- libplacebo (LGPLv2.1+)
- uchardet (MPL 1.1 / LGPLv2.1+)
- [cJSON v1.7.19](https://github.com/DaveGamble/cJSON/tree/v1.7.19) (MIT, `ThirdParty/cJSON` 소스 포함)


## 향후 개발 항목 (TODO)

1. 하단 컨트롤 바 UI (Direct2D 렌더링)
   - 탐색 바(Seek Bar) 마우스 클릭/드래그 이동 및 재생 시간(`현재 시간 / 전체 시간`) 표시
   - 재생/일시정지, 정지, 볼륨 슬라이더/음소거, 배속 조절, 반복 재생 버튼
   - 전체화면 시 마우스 커서 위치에 따른 컨트롤 바 자동 숨김/표시
2. 단축키 및 마우스 조작 (곰플레이어 스타일)
   - `좌/우 방향키`: 구간 이동 (`±5초`, `설정창에서 설정 가능하게`)
   - `상/하 방향키` 및 `마우스 휠`: 볼륨 조절
   - `Enter` 또는 `화면 더블클릭`: 전체화면 전환 / `ESC`: 전체화면 해제
   - `[`, `]`, `\`: 구간 반복(A-B Repeat) 시작/종료/해제
3. 자막 제어
   - 자막 파일(`.smi`, `.srt`, `.ass`) 드래그 앤 드롭 로드 및 표시 토글
   - 자막 싱크(`±0.5초`) 및 글자 크기/위치 조절
4. 재생목록 및 편의 기능
   - 재생목록(Playlist) 관리
   - 항상 위(TopMost) 고정, 화면 캡처, 마지막 볼륨/설정값 저장