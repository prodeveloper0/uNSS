# 클라이언트 (Switch 홈브류, C++ / libnx)

## 명령어

`client/` 디렉토리에서 실행합니다.

```bash
export DEVKITPRO=<path to>/devkitpro     # 필수. 설정하지 않으면 Makefile 이 에러로 중단됨
make                                     # client.nro (+ client.elf, client.nacp) 생성
make clean
./deploy.sh <host:port> [projectname]    # nxlink/ftpd 가 실행 중인 Switch 로 client.nro 를 FTP 업로드
```

Makefile 은 표준 devkitPro libnx 템플릿으로, `source/` 하위의 `.cpp` / `.c` / `.s` 파일을 자동 검출합니다. 링크 의존성은 `-lSDL2_ttf -lSDL2` (GUI 렌더링), `-lnx`, `curl-config` 플래그 (portlibs 의 curl) 등입니다. `APP_TITLE=uNSS`, `APP_AUTHOR=prodeveloper0`.

## Docker 로 빌드 (호스트 SDK 격리)

호스트에 devkitPro 를 설치하고 싶지 않을 때 사용합니다. `client/Dockerfile` 이 `devkitpro/devkita64` 공식 이미지를 베이스로 `switch-curl`, `switch-sdl2`, `switch-sdl2_ttf` 등 portlib 을 추가한 빌더 이미지를 정의하며, 소스는 볼륨 마운트로 주입되므로 산출물 (`client.nro` 등) 은 호스트 작업 디렉토리에 그대로 생성됩니다.

```bash
cd client/
docker compose run --rm builder              # make (기본 명령)
docker compose run --rm cleaner               # make clean
docker compose run --rm builder bash         # 대화형 쉘 진입
```

첫 실행 시 이미지가 빌드되며, 이후에는 캐시된 이미지를 재사용합니다. `Dockerfile` 혹은 portlib 의존성이 바뀌면 `docker compose build builder` 로 재빌드하세요. `deploy.sh` 는 여전히 호스트에서 실행합니다 (FTP 업로드 대상 네트워크 접근이 필요하므로 컨테이너화하지 않음).

빌드 산출물의 소유권은 호스트 사용자와 일치하도록 `docker-compose.yaml` 에서 `user: "${UID:-1000}:${GID:-1000}"` 로 실행됩니다. zsh 는 `$UID` / `$GID` 를 자동으로 노출하므로 그대로 동작하며, bash 사용자는 `$GID` 가 비어있을 수 있어 `GID=$(id -g) docker compose run --rm builder` 와 같이 호출하거나 `client/.env` 에 `GID=...` 를 기록해두세요.

## 아키텍처

`source/main.cpp` 는 설정 로드 → SDL2 GUI 초기화 → 메인 화면 표시 순서로 동작합니다.

- 메인 화면 (`MainScreen`) 에서 계정을 해석한 뒤 Push / Pull 메뉴를 표시.
- `A` → 선택한 메뉴 실행 (Push 또는 Pull).
- `-` → 계정 전환 (full application mode: psel, applet mode: 내장 계정 선택 화면).
- `+` → 종료.

## GUI 구조

SDL2 + SDL2_ttf 기반의 직접 렌더링 GUI 입니다. Screen 스택으로 화면을 관리합니다.

```
gui::App (SDL2 초기화, 메인 루프, Screen 스택 관리)
  ├── MainScreen     — 메인 메뉴 (계정 정보, Push/Pull 버튼)
  ├── AccountScreen  — 내장 계정 선택 화면 (applet mode 용)
  └── ProgressScreen — Push/Pull 진행 로그 화면
```

- `gui::Renderer` — SDL2 렌더러 래퍼. Switch 시스템 폰트 (한글 포함) 를 `plGetSharedFontByType(PlSharedFontType_KO)` 로 로드.
- `gui::Screen` — 화면 인터페이스. `update(kDown)` 과 `render(r)` 를 구현.
- `gui::App` — 싱글톤. `pushScreen()` / `popScreen()` 으로 화면 전환.

### ProgressScreen 의 동기 실행 모델

Push/Pull 작업은 메인 스레드에서 동기 실행됩니다 (libnx 서비스가 스레드 안전하지 않으므로). 로그 콜백에서 `renderNow()` 를 호출하여 각 타이틀 처리마다 화면을 갱신합니다.

## 모듈 구성

`source/` 하위 헤더/구현 페어:

| 모듈 | 역할 |
|---|---|
| `gui/Gui` | SDL2/SDL_ttf 초기화, `Renderer` (텍스트·도형 그리기), `App` (메인 루프·Screen 스택), `Screen` 인터페이스 |
| `gui/MainScreen` | 메인 화면. 계정 해석 (`resolveAccount`), Push/Pull 메뉴, 계정 전환 |
| `gui/AccountScreen` | 내장 계정 선택 화면. `probeAccounts` 로 계정 목록 표시 |
| `gui/ProgressScreen` | Push/Pull 진행 로그 화면. 동기 실행 + 로그 콜백에서 화면 갱신 |
| `account` | 현재 Switch 사용자 계정 (uid, nickname) 결정. `getCurrentAccount(Account*, const AccountResolveOptions&)` 가 launch context 에 따라 preselected user / psel 애플릿 / ini 닉네임 매칭 중 하나로 계정을 선택함 (자세한 전략은 아래 "계정 결정 전략" 절 참고) |
| `title` | 설치된 타이틀 열거, 타이틀 이름 조회 |
| `savedata` | Switch 세이브 데이터 FS 마운트/읽기/쓰기 (`archiveAllSaveData`, `restoreSaveData` 등). 성공 시 `SAVEDATA_OK` 반환 |
| `fileio`, `zipio` (+ 번들된 `miniz.c/h`) | 파일시스템 헬퍼 및 zip 아카이브 처리 |
| `http` | libcurl 래퍼 |
| `remote` | `IRemoteStoreIO` 인터페이스. `HTTPRemoteStore` 가 FastAPI 엔드포인트를 호출하여 push/pull 구현 |
| `ini` | 헤더-온리 설정 파서. `Config gl_Config("sdmc:/uNSS/config.ini")` 로 사용. `[remote] enabled` / `serverUrl` 이 원격 동기화 경로를 게이팅하고, `[account] defaultAccountName` / `useProfileSelector` 가 계정 결정 전략에 주입됨 |
| `utils` | 기타 헬퍼 (`padding`, `recursiveMkdir`, ...) |

## Switch SD 카드 상의 런타임 경로

- `sdmc:/uNSS/config.ini` — 사용자 편집 가능한 설정 파일. 형식은 README 참고.
- `sdmc:/uNSS/saves/` — 업로드 전 / 다운로드 후 아카이브된 세이브 블롭의 스테이징 디렉토리.

## 계정 결정 전략 (`source/account.cpp`)

`getCurrentAccount` 는 아래 순서로 uid 를 찾습니다.

1. `options.useProfileSelector == false` → 2·3 을 건너뛰고 곧바로 4 로 점프.
2. `appletGetAppletType()` 으로 launch context 를 분기.
   - **Full application mode** (`AppletType_Application` / `AppletType_SystemApplication`) 일 때만 3 을 시도.
   - 그 외 (library applet = applet mode 등) 는 HOS 의 정상 경로를 우회하고 4 로 fallback.
3. `pselShowUserSelector(&uid, &settings)` — psel library applet 으로 선택창을 띄움. **library applet 은 다른 library applet 을 런칭할 수 없으므로** applet mode 에서는 호출 자체가 실패하고, 그래서 2 에서 applet mode 를 먼저 걸러낸다.
4. `findUserByNickname(options.defaultAccountName, &uid)` — `accountListAllUsers` 결과를 순회하며 `AccountProfileBase::nickname` 이 `defaultAccountName` 과 **정확히** 일치하는 첫 사용자를 선택. `defaultAccountName` 이 빈 문자열이면 실패.

4 까지 와서도 uid 를 얻지 못하면 `getCurrentAccount` 는 `-1` 을 반환합니다.

### 새 `[account]` 키를 추가할 때

- 기본값은 **`main.cpp::initConfig()`** 에서 `.has(...)` 로 등록한다. `ini.hpp` 는 write 기능이 없어 SD 카드의 `config.ini` 에 자동 기록되지는 않고, 런타임 default 로만 동작한다.
- GUI 에서 사용하는 쪽은 `MainScreen::resolveAccount()` 에서 `config["account"][...]` 로 읽어 분기한다. `account.cpp` 가 `gl_Config` 전역에 직접 접근하지 않는 현재 분리를 깨지 말 것.
- README 의 "Configuration" 섹션, 본 문서의 모듈 표 / 결정 전략 절을 함께 갱신한다.

