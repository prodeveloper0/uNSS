# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 저장소 구조 (요약)

- `client/` — Nintendo Switch 홈브류 앱 (C++ / libnx, devkitPro 빌드)
- `server/` — Python FastAPI 기반 세이브 데이터 동기화 서버 (SQLite 메타데이터 + 디스크 블롭)
- `resources/` — README 에서 참조하는 에셋

클라이언트와 서버는 코드 / 빌드 도구를 공유하지 않는 별개 프로젝트입니다.

## 상세 문서

작업 대상에 따라 아래 문서를 먼저 읽으세요.

- [`docs/OVERVIEW.md`](./docs/OVERVIEW.md) — 저장소 전체 구조 개요
- [`docs/SERVER.md`](./docs/SERVER.md) — 서버 빌드/실행 명령어, 계층형 아키텍처, 리비전 트랜잭션 모델, HTTP API 스펙
- [`docs/CLIENT.md`](./docs/CLIENT.md) — 클라이언트 빌드/배포 명령어, TUI 이벤트 루프, `source/` 모듈 구성, SD 카드 런타임 경로
- [`docs/PROTOCOL.md`](./docs/PROTOCOL.md) — 클라이언트 ↔ 서버 계약 사항 (사용자 식별, 업로드/응답 포맷 등 깨지기 쉬운 규약)

## 작업 시 유의사항

- 서버 API 응답은 JSON 이 아닌 plain text 입니다. C++ 클라이언트가 직접 파싱하므로 포맷을 임의로 바꾸지 마세요 (`docs/PROTOCOL.md` 참고).
- 서버 SQL 은 `title_id` / `revision_id` 를 `UPPER(...)` 로 정규화합니다. 새 쿼리 추가 시 이 규칙을 지키지 않으면 조회가 조용히 실패합니다 (`docs/SERVER.md` 참고).
- 세이브 업로드는 2 단계 리비전 트랜잭션 (`P` → `C` / `D`) 입니다. 새 엔드포인트 추가 시 이 상태 머신을 깨지 않도록 주의하세요 (`docs/SERVER.md` 참고).
