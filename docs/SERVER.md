# 서버 (Python / FastAPI)

## 명령어

`server/` 디렉토리에서 실행합니다.

```bash
pip install -r requirements.txt                      # 의존성 설치
python main.py --host 0.0.0.0 --port 8989            # click 엔트리포인트 (uvloop 사용)
./run-linux.sh                                       # 대안: `uvicorn main:app ...` 직접 실행
./tests.sh <function> [args...]                      # 127.0.0.1:8989 대상 curl 기반 수동 API 스모크 테스트
./build-docker.sh <tag>                              # docker build . --tag unss-server:<tag>
docker compose up                                    # docker-compose.yaml 사용 (unss-server:20250501 이미지 필요)
pyinstaller --name "uNSS-Server" main.py --onefile   # Windows 단일 바이너리 (build-win32.bat)
```

자동화된 테스트 스위트는 없습니다. `tests.sh` 는 실행 중인 서버에 대해 사용할 수 있는 curl 헬퍼 모음입니다 (`query_all_user_save_data`, `issue_save_data_revision_id`, `get_latest_save_data_revision_id`, `upload_save_data_revision`, `download_save_data_revision`).

## 아키텍처

계층형 구조입니다.

- `main.py` — FastAPI 라우트
- `service.py` — `SaveDataService` (도메인 로직)
- `repository.py` — `BaseSaveDataRepository` / `aiosqlite` 기반 `SQLiteSaveDataRepository`
- `model.py` — `TitleRevision` pydantic 모델
- `singleton.py` — `@singleton` 클래스 데코레이터. `SaveDataService` 와 `SQLiteSaveDataRepository` 모두 이 데코레이터를 사용하므로 사실상 프로세스 전역 인스턴스입니다.

### 핵심 개념 — 리비전 트랜잭션

세이브 데이터 업로드는 `savedata` 테이블의 `status` 컬럼으로 추적되는 2 단계 오퍼레이션입니다.

1. `begin_new_revision` 이 `status='P'` (pending) 행을 삽입하고 새 UUID `revision_id` 를 반환합니다.
2. `process_file` 이 요청 본문을 `savedata/<revision_id>` 로 스트리밍한 뒤, 성공 시 `commit_revision` (`status='C'`), 실패 시 `abandon_revision` (`status='D'`) 을 호출합니다.
3. `status='C'` 인 행만 `get_latest_revision_by_title` / `query_all_latest_revision_by_user` 에서 조회됩니다.

### 저장 위치

- 블롭 파일: `server/savedata/<revision_id>` (디렉토리는 자동 생성)
- SQLite 메타데이터: `metadata.sqlite`. 스키마는 `SQLiteSaveDataRepository.__init__` 에서 멱등하게 생성됩니다.
- 두 경로 모두 `docker-compose.yaml` 에서 볼륨으로 마운트됩니다.

### 주의: 대문자 정규화

모든 `title_id` 및 `revision_id` 값은 SQL 에서 `UPPER(...)` 로 대문자 정규화됩니다. 새 쿼리를 추가할 때 반드시 이 규칙을 지켜야 하며, 그렇지 않으면 조회가 조용히 실패합니다.

## HTTP API

응답은 JSON 이 아닌 **plain text** 입니다. C++ 클라이언트가 이를 직접 파싱하므로 포맷을 바꾸지 마세요. 모든 라우트는 `main.py` 에 정의되어 있습니다.

| 메서드 | 경로 | 설명 |
|---|---|---|
| `GET`  | `/users/{user}/saves` | 커밋된 최신 리비전 전체를 `title_id\|revision_id` 형식의 줄바꿈 구분 텍스트로 반환 |
| `POST` | `/users/{user}/saves/{title_id}/revisions` | 새 리비전 트랜잭션 시작, 새 `revision_id` 반환 |
| `POST` | `/users/{user}/saves/{title_id}/revisions/{revision_id}` | 블롭 스트리밍 업로드. 성공/실패에 따라 자동 commit/abandon |
| `GET`  | `/users/{user}/saves/{title_id}/revisions` | 해당 타이틀의 최신 커밋 리비전 ID 반환 |
| `GET`  | `/users/{user}/saves/{title_id}/revisions/{revision_id}/data` | 블롭 다운로드. `revision_id=latest` 는 최신 커밋 리비전으로 치환됨 |
