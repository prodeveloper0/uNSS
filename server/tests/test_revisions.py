"""
리비전 트랜잭션. 여기가 깨지면 반쯤 올라간 세이브가 최신인 척 남는다.

상태는 셋이다:
  P  발급됐고 아직 파일을 받는 중
  C  파일까지 다 받았다 - 이제서야 최신 리비전이 된다
  D  중간에 실패해서 버렸다

'P' 나 'D' 가 목록에 나타나면 클라이언트는 그것을 내려받으려 하고, 없거나
잘린 파일을 세이브 데이터로 되돌려 쓰게 된다.
"""

import sqlite3

import pytest

UPLOAD = b"PK\x03\x04 pretend this is a save archive"


def status_of(work_dir, revision_id):
    conn = sqlite3.connect(work_dir / "metadata.sqlite")
    try:
        row = conn.execute(
            "SELECT status FROM savedata WHERE revision_id = ?", (revision_id,)
        ).fetchone()
    finally:
        conn.close()
    return row[0] if row else None


def test_issued_revision_is_pending_and_invisible(server):
    client, work_dir = server

    revision_id = client.post("/users/Soad1337/saves/0100000000010000/revisions").text

    assert status_of(work_dir, revision_id) == "P"
    # 아직 파일이 없다. 목록에 나오면 클라이언트가 빈 것을 받아간다.
    assert client.get("/users/Soad1337/saves").text == ""


def test_revision_becomes_current_only_after_the_file_arrives(server):
    client, work_dir = server

    revision_id = client.post("/users/Soad1337/saves/0100000000010000/revisions").text
    client.post(
        f"/users/Soad1337/saves/0100000000010000/revisions/{revision_id}",
        content=UPLOAD,
    )

    assert status_of(work_dir, revision_id) == "C"
    assert client.get("/users/Soad1337/saves").text == f"0100000000010000|{revision_id}"


def test_uploading_twice_to_the_same_revision_is_refused(server):
    client, work_dir = server

    revision_id = client.post("/users/Soad1337/saves/0100000000010000/revisions").text
    client.post(
        f"/users/Soad1337/saves/0100000000010000/revisions/{revision_id}",
        content=b"the real save",
    )

    # 이미 끝난 트랜잭션이다. 다시 쓰게 두면 멀쩡한 백업이 덮인다.
    with pytest.raises(ValueError):
        client.post(
            f"/users/Soad1337/saves/0100000000010000/revisions/{revision_id}",
            content=b"garbage",
        )

    assert status_of(work_dir, revision_id) == "C"
    response = client.get(
        f"/users/Soad1337/saves/0100000000010000/revisions/{revision_id}/data"
    )
    assert response.content == b"the real save"


def test_unknown_revision_cannot_be_uploaded_to(server):
    client, _ = server

    with pytest.raises(ValueError):
        client.post(
            "/users/Soad1337/saves/0100000000010000/revisions/MADE-UP-ID",
            content=UPLOAD,
        )


def test_newest_completed_revision_wins(server):
    client, work_dir = server

    first = client.post("/users/Soad1337/saves/0100000000010000/revisions").text
    client.post(
        f"/users/Soad1337/saves/0100000000010000/revisions/{first}", content=b"old"
    )

    second = client.post("/users/Soad1337/saves/0100000000010000/revisions").text
    client.post(
        f"/users/Soad1337/saves/0100000000010000/revisions/{second}", content=b"new"
    )

    # CURRENT_TIMESTAMP 는 초 단위라 같은 초에 두 개가 들어갈 수 있다.
    # 순서를 확실히 하려고 첫 번째를 과거로 밀어둔다.
    conn = sqlite3.connect(work_dir / "metadata.sqlite")
    try:
        conn.execute(
            "UPDATE savedata SET created_at = datetime('now', '-1 hour') WHERE revision_id = ?",
            (first,),
        )
        conn.commit()
    finally:
        conn.close()

    assert client.get("/users/Soad1337/saves/0100000000010000/revisions").text == second
    assert client.get("/users/Soad1337/saves").text == f"0100000000010000|{second}"


def test_same_second_uploads_still_resolve_to_the_newest(server):
    client, _ = server

    # created_at 은 초 단위다. 연달아 올리면 세 개가 같은 시각을 갖는다.
    # 손대지 않은 그대로 - 실제로 일어나는 모양 그대로 - 확인한다.
    last = None
    for payload in (b"first", b"second", b"third"):
        last = client.post("/users/Soad1337/saves/0100000000010000/revisions").text
        client.post(
            f"/users/Soad1337/saves/0100000000010000/revisions/{last}", content=payload
        )

    # 개별 조회와 목록이 같은 답을 내야 한다. 다르면 클라이언트가 목록을 보고
    # 고른 리비전과 실제로 받는 파일이 어긋난다.
    assert client.get("/users/Soad1337/saves/0100000000010000/revisions").text == last
    assert client.get("/users/Soad1337/saves").text == f"0100000000010000|{last}"

    response = client.get("/users/Soad1337/saves/0100000000010000/revisions/latest/data")
    assert response.content == b"third"


def test_a_pending_revision_does_not_hide_the_last_good_one(server):
    client, _ = server

    good = client.post("/users/Soad1337/saves/0100000000010000/revisions").text
    client.post(
        f"/users/Soad1337/saves/0100000000010000/revisions/{good}", content=b"good"
    )

    # 백업이 시작됐다가 (스위치가 꺼지거나 해서) 끝나지 않은 상황.
    client.post("/users/Soad1337/saves/0100000000010000/revisions")

    # 끝난 적 없는 리비전 때문에 멀쩡한 백업이 가려지면 안 된다.
    assert client.get("/users/Soad1337/saves/0100000000010000/revisions").text == good
    assert client.get("/users/Soad1337/saves").text == f"0100000000010000|{good}"


def test_asking_for_a_title_without_any_completed_revision_fails(server):
    client, _ = server

    client.post("/users/Soad1337/saves/0100000000010000/revisions")

    with pytest.raises(ValueError):
        client.get("/users/Soad1337/saves/0100000000010000/revisions")
