# 저장소 개요

이 저장소는 HTTP 로 통신하는 두 개의 독립적인 컴포넌트를 포함합니다.

- `client/` — Nintendo Switch 홈브류 앱 (C++ / libnx). devkitPro 로 빌드하며 `client.nro` 를 생성합니다.
- `server/` — 세이브 데이터 리비전을 저장하는 Python FastAPI 서비스. 메타데이터는 SQLite, 블롭은 디스크 파일로 저장합니다.
- `resources/` — README 에서 참조하는 스크린샷 / 에셋.

클라이언트와 서버는 **코드나 빌드 도구를 공유하지 않습니다**. 두 개의 별개 프로젝트로 취급하세요.

자세한 내용은 아래 문서를 참고하세요.

- [서버 (명령어 · 아키텍처 · HTTP API)](./SERVER.md)
- [클라이언트 (명령어 · 아키텍처 · 모듈)](./CLIENT.md)
- [클라이언트 ↔ 서버 계약 사항](./PROTOCOL.md)
