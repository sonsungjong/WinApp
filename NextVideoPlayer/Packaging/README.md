# NextVideoPlayer MSI / MSIX 패키징

이 구성은 패키지를 로컬에서 빌드하고 검증한다. 스토어 등록, 인증서 설치,
GitHub 저장소 생성 및 릴리스 공개는 자동 실행하지 않는다.

## 준비

- Windows 10 2004 이상 또는 Windows 11, x64
- Visual Studio 2026의 C++ 데스크톱 개발 워크로드, v145 도구 집합
- Windows 10/11 SDK의 MakeAppx, SignTool
- PowerShell 7 (`pwsh`)

MSI는 Windows Installer automation API와 Windows의 MakeCab으로 만든다.
별도의 Visual Studio 확장이나 WiX 설치는 필요하지 않다.

프로젝트 폴더에서 실행한다.

```powershell
pwsh -NoProfile -File .\Packaging\Build-Packages.ps1 -Target All
```

`-Target MSI`, `-Target MSIX`로 하나만 만들 수도 있다. 실행 파일은 Release/x64로
별도 빌드하고, 필요한 SDK와 폰트는 기존 `setup_sdk.bat`가 준비한다.
패키징에 필요한 Vulkan runtime ZIP은 공식 LunarG 서버에서 내려받고
`Package.config.json`의 고정 SHA256을 확인한다. 캐시는 `artifacts/packaging/cache`다.

## 결과와 버전

`Version.h`가 실행 파일, MSI, MSIX의 공통 버전이다. 초기 버전은 `1.0.0.0`이다.
MSI 업그레이드를 위해 배포마다 major/minor/patch 중 하나를 올린다.
Store 제출용 첫 번째 필드는 1 이상, 네 번째 필드는 0으로 유지한다.
첫 공개 배포 이후 `Package.config.json`의 `MsiUpgradeCode`는 변경하지 않는다.

```text
artifacts/packages/1.0.0/
  NextVideoPlayer-1.0.0-x64.msi
  NextVideoPlayer-1.0.0-x64.msix
  SHA256SUMS.txt
  build-info.json
```

`build-info.json`은 파일 목록, 해시, 로컬 검증 경로를 담은 개발용 기록이다.
GitHub Release에는 설치 파일과 체크섬을 첨부하면 된다.
생성물과 다운로드 캐시는 Git에서 제외된다.

## MSI: GitHub Release용

- 현재 사용자에게 `%LOCALAPPDATA%\Programs\NextVideoPlayer`에 설치한다.
- 관리자 권한, 전역 폰트 설치, 설치 중 다운로드가 필요하지 않다.
- 시작 메뉴 바로가기와 Windows 앱 목록의 제거 기능을 등록한다.
- 버전이 올라가면 같은 UpgradeCode의 이전 MSI를 교체하고 다운그레이드는 차단한다.
- 설정은 `%LOCALAPPDATA%\NextVideoPlayer\config.ini`에 별도로 저장한다.
  업그레이드·제거 시 설치 프로그램이 사용자 설정을 지우지 않는다.
- 기본 빌드는 미서명이다. 공개 배포용 코드 서명은 아래 옵션으로 적용할 수 있다.

같은 버전의 공개 릴리스 파일을 덮어쓰지 말고, 새 버전과 새 릴리스로 배포한다.

## MSIX: Microsoft Store용

`Package.config.json`에 다음 Partner Center 등록 정보를 반영했다.

| 항목 | 등록값 |
| --- | --- |
| Package/Identity/Name | `SUNGJONG.NextVideoPlayer` |
| Package/Identity/Publisher | `CN=2F982184-7E58-4E48-8591-3878B830CF0B` |
| Package/Properties/PublisherDisplayName | `SUNGJONG` |

저장된 등록 정보로 스토어용 패키지를 생성한다.

```powershell
pwsh -NoProfile -File .\Packaging\Build-Packages.ps1 -Target MSIX -ForStore
```

결과는 `artifacts/packages/<버전>/store/`에 생성된다.
스토어용 패키지와 체크섬·검증 기록을 일반 MSI/MSIX 결과와 구분해 보관한다.

다른 앱/게시자 정보로 빌드하려면 명령행에서 세 값을 지정할 수 있다.

```powershell
pwsh -NoProfile -File .\Packaging\Build-Packages.ps1 -Target MSIX -ForStore `
  -StoreIdentityName 'Partner Center의 Package/Identity/Name' `
  -StorePublisher 'CN=Partner Center의 게시자 ID' `
  -PublisherDisplayName 'Partner Center의 게시자 표시 이름'
```

세 값은 표시 이름을 임의로 추측하지 말고 등록 정보에서 그대로 복사한다.
`-ForStore`는 설정 파일의 값 또는 명령행 값을 검증하며 개발용 임시 ID는 거부한다.
Store는 제출된 MSIX를 서명하므로 Store 제출에 개인 서명 인증서는 필요하지 않다.
이 프로젝트는 Win32 full-trust 앱을 그대로 패키징하며 여러 플레이어 인스턴스를 허용한다.
Store 심사 시 `runFullTrust`는 Win32 창·재생 엔진을 실행하는 데 필요하다고 설명한다.

MSIX 설정은 `%LOCALAPPDATA%\Packages\<PackageFamilyName>\LocalState\config.ini`에 저장한다.
설치 경로에는 설정이나 로그를 쓰지 않는다. 기본 미서명 MSIX는 직접 설치용이 아니다.
로컬 설치 테스트는 신뢰된 개발 인증서로 서명한 별도 패키지를 준비해서 진행한다.
이 스크립트가 인증서를 만들거나 신뢰 저장소에 등록하지는 않는다.

스토어용 결과를 검증할 때는 해당 폴더를 지정한다.

```powershell
pwsh -NoProfile -File .\Packaging\Test-Packages.ps1 -PackageDirectory .\artifacts\packages\1.0.0\store
```

## 선택적 코드 서명

사용자 인증서 저장소에 준비한 코드 서명 인증서의 thumbprint를 전달한다.
개인 키나 PFX 비밀번호를 소스 또는 설정 파일에 적지 않는다.

```powershell
pwsh -NoProfile -File .\Packaging\Build-Packages.ps1 -Target MSI `
  -CertificateThumbprint '인증서 THUMBPRINT' `
  -TimestampUrl '인증서 제공업체의 RFC3161 타임스탬프 URL'
```

서명 대상은 staging 폴더의 미서명 실행 파일·DLL과 결과 패키지다.
이미 유효한 공급자 서명이 있는 DLL은 유지한다.
MSIX를 서명할 때는 인증서의 Subject가 MSIX Publisher와 일치해야 한다.

## 검증

```powershell
pwsh -NoProfile -File .\Packaging\Test-Packages.ps1
```

- Windows Installer의 관리 추출(`/a /qn`)로 MSI를 풀고 원본 SHA256과 비교
- MakeAppx로 MSIX를 풀고 파일, ID, 버전, 다중 인스턴스 선언 확인
- 사용자 설정 경로, 기존 설정 이전, 기존 파일 보존, 저장·복원, 동시 초기화 확인
- 배포 폴더에서 VC++ runtime, Vulkan loader, libmpv를 실제 로드하여 확인

테스트는 플레이어 창이나 영상을 실행하지 않고, 앱을 사용자 PC에 설치·등록하지 않는다.
검증 파일과 로그는 `artifacts/packaging/verification`에 남는다.
최종 배포 전 깨끗한 별도 Windows PC/VM에서 MSI 설치·업그레이드·제거와
서명된 MSIX 설치·재생·업데이트를 확인한다.

MSI의 설치·업그레이드·제거를 로컬에서 검증하려면 다음 통합 테스트를 명시적으로 실행한다.
테스트 전용 제품·컴포넌트 GUID와 시작 메뉴 폴더를 사용하며, 무인 설치 후 finally에서 제거한다.
실제 NextVideoPlayer 제품 ID를 설치하거나 플레이어를 실행하지 않는다.

```powershell
pwsh -NoProfile -File .\Packaging\Test-MsiLifecycle.ps1
```

## 별도 GitHub 배포 저장소

1. 소스 공개 결정과 별도로 배포용 공개 저장소를 만든다.
2. `ReleaseRepository.README.md`를 소개 페이지의 출발점으로 사용한다.
3. 버전 태그(예: `v1.0.0`)로 Release를 만들고 `.msi`와 `SHA256SUMS.txt`를 첨부한다.
4. 미리 보기 버전은 pre-release로 표시하고 다운로드 링크를 소개 글과 Store 페이지에 연결한다.

소스가 비공개라면 개발 저장소의 전체 이력을 배포 저장소에 push하지 않는다.

## 포함 파일과 라이선스

배포 payload는 실행 파일, libmpv, x64 VC++ runtime DLL, Vulkan loader,
나눔스퀘어 폰트, 아이콘, 외부 라이브러리 고지로 제한한다.
기존 `config.ini`, PDB, 강의 자료, SDK 헤더·LIB·예제 소스는 포함하지 않는다.
구버전 실행 파일 옆의 `config.ini`는 새 사용자 설정 파일이 없을 때만 복사하며 원본은 유지한다.

`licenses/`에 cJSON MIT, 폰트 OFL, Vulkan runtime의 원본 고지와 기존 `LICENSE.txt`,
실제 DLL 해시·SDK 다운로드 주소를 포함한 `COMPONENT-SOURCES.txt`를 넣는다.
후자는 LGPL 대응 소스 제공을 자동으로 완료하는 문서가 아니다.
공개 배포 전 사용한 libmpv/FFmpeg 및 포함 의존성의 정확한 소스·패치·빌드 정보와
재배포 조건을 준비해야 한다. NextVideoPlayer 자체 소스의 공개 여부·라이선스는 별도 결정한다.

공식 문서:
- [MSIX 패키징 도구](https://learn.microsoft.com/en-us/windows/msix/package/create-app-package-with-makeappx-tool)
- [Microsoft Store의 Win32 배포 방식](https://learn.microsoft.com/en-us/windows/apps/distribute-through-store/how-to-distribute-your-win32-app-through-microsoft-store)
- [Windows Installer automation](https://learn.microsoft.com/en-us/windows/win32/msi/installer-object)
- [MSI 비관리자 설치](https://learn.microsoft.com/en-us/windows/win32/msi/authoring-packages-without-the-uac-dialog-box)
- [Vulkan runtime 다운로드](https://vulkan.lunarg.com/sdk/home/)
