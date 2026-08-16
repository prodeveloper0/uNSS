"""
테스트마다 서버를 통째로 새로 세운다.

repository 와 service 는 싱글턴이고, main 은 임포트되는 순간 모듈 수준에서
"metadata.sqlite" 를 만든다. 즉 상태가 프로세스에 붙어 있다. 그래서 임시
디렉토리로 옮겨 간 뒤 모듈을 다시 임포트한다 - 싱글턴이 클로저에 인스턴스를
들고 있으므로, 모듈을 새로 읽으면 인스턴스도 새로 생긴다.

이렇게 하지 않으면 테스트끼리 같은 DB 와 savedata/ 를 공유하고, 실행 순서에
따라 결과가 달라진다.
"""

import sys
from pathlib import Path

import pytest
from fastapi.testclient import TestClient

SERVER_DIR = Path(__file__).resolve().parent.parent

# 서버 모듈은 서로를 평범한 최상위 이름으로 임포트한다 (from repository import ...).
if str(SERVER_DIR) not in sys.path:
    sys.path.insert(0, str(SERVER_DIR))


@pytest.fixture()
def server(tmp_path, monkeypatch):
    """(TestClient, 작업 디렉토리) 를 준다."""

    monkeypatch.chdir(tmp_path)

    for name in ("main", "service", "repository", "singleton", "model"):
        sys.modules.pop(name, None)

    import main  # noqa: E402  - chdir 뒤에 임포트해야 DB 가 임시 디렉토리에 생긴다

    with TestClient(main.app) as client:
        yield client, tmp_path


@pytest.fixture()
def client(server):
    return server[0]
