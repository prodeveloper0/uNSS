"""
클라이언트와 서버 사이의 약속을 지키는지 본다.

응답은 JSON 이 아니라 plain text 다. C++ 클라이언트가 직접 파싱하기 때문에
포맷이 조금만 달라져도 - 따옴표가 붙거나, 줄 구분이 바뀌거나, 대소문자가
달라지거나 - 스위치 쪽에서 조용히 깨진다. 서버 테스트가 잡아줄 수 있는 것은
여기까지이고, 그래서 여기를 잡는다.
"""

UPLOAD = b"PK\x03\x04 pretend this is a save archive"


def upload(client, user, title, payload=UPLOAD):
    """리비전 하나를 끝까지 올리고 revision_id 를 준다."""
    revision_id = client.post(f"/users/{user}/saves/{title}/revisions").text
    client.post(f"/users/{user}/saves/{title}/revisions/{revision_id}", content=payload)
    return revision_id


def test_empty_user_returns_empty_body_not_json(client):
    response = client.get("/users/Nobody/saves")

    assert response.status_code == 200
    assert response.text == ""
    # "[]" 나 "null" 이 오면 클라이언트는 그것을 타이틀 하나로 읽는다.
    assert response.headers["content-type"].startswith("text/plain")


def test_save_list_is_pipe_separated_one_per_line(client):
    first = upload(client, "Soad1337", "0100000000010000")
    second = upload(client, "Soad1337", "010000000000100B")

    lines = client.get("/users/Soad1337/saves").text.splitlines()

    assert sorted(lines) == sorted([
        f"0100000000010000|{first}",
        f"010000000000100B|{second}",
    ])


def test_ids_come_back_uppercase(client):
    # 클라이언트는 타이틀 ID 를 소문자로 보낼 수 있다. 서버는 UPPER 로
    # 정규화해서 저장하고, 그대로 돌려줘야 비교가 어긋나지 않는다.
    revision_id = upload(client, "Soad1337", "010000000000100b")

    body = client.get("/users/Soad1337/saves").text

    assert body == f"010000000000100B|{revision_id}"
    assert revision_id == revision_id.upper()


def test_title_id_lookup_is_case_insensitive(client):
    revision_id = upload(client, "Soad1337", "010000000000100b")

    lower = client.get("/users/Soad1337/saves/010000000000100b/revisions").text
    upper = client.get("/users/Soad1337/saves/010000000000100B/revisions").text

    assert lower == upper == revision_id


def test_users_are_kept_apart(client):
    mine = upload(client, "Soad1337", "0100000000010000")
    yours = upload(client, "Someone", "0100000000010000")

    assert client.get("/users/Soad1337/saves").text == f"0100000000010000|{mine}"
    assert client.get("/users/Someone/saves").text == f"0100000000010000|{yours}"


def test_upload_echoes_the_revision_id(client):
    # 클라이언트는 이 응답으로 업로드가 받아들여졌는지 판단한다.
    revision_id = client.post("/users/Soad1337/saves/0100000000010000/revisions").text
    response = client.post(
        f"/users/Soad1337/saves/0100000000010000/revisions/{revision_id}",
        content=UPLOAD,
    )

    assert response.status_code == 200
    assert response.text == revision_id


def test_download_returns_exactly_what_was_uploaded(client):
    payload = bytes(range(256)) * 8
    revision_id = upload(client, "Soad1337", "0100000000010000", payload)

    response = client.get(
        f"/users/Soad1337/saves/0100000000010000/revisions/{revision_id}/data"
    )

    assert response.status_code == 200
    assert response.content == payload


def test_download_latest_resolves_to_newest_revision(client):
    upload(client, "Soad1337", "0100000000010000", b"old")
    newest = upload(client, "Soad1337", "0100000000010000", b"new")

    response = client.get("/users/Soad1337/saves/0100000000010000/revisions/latest/data")

    assert response.content == b"new"
    assert newest  # 최신 리비전이 실제로 발급됐는지도 같이 본다


def test_download_of_unknown_revision_is_404_not_a_crash(client):
    response = client.get(
        "/users/Soad1337/saves/0100000000010000/revisions/DOES-NOT-EXIST/data"
    )

    assert response.status_code == 404
