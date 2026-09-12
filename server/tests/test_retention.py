import sqlite3
import subprocess
import sys
from datetime import datetime, timedelta
from pathlib import Path
import zipfile


def make_archive(path: Path, payload: bytes = b"save"):
    with zipfile.ZipFile(path, "w") as archive:
        archive.writestr("saves/data.bin", payload)


def make_database(path: Path):
    with sqlite3.connect(path) as db:
        db.execute("""
            CREATE TABLE savedata (
                revision_id TEXT PRIMARY KEY,
                user_name TEXT NOT NULL,
                title_id TEXT NOT NULL,
                status CHAR(1) NOT NULL,
                archive_location TEXT,
                created_at DATETIME NOT NULL,
                completed_at DATETIME
            )
        """)


def run_tool(tool: Path, base: Path, apply: bool = False):
    command = [sys.executable, str(tool), "--base", str(base)]
    if apply:
        command.append("--apply")
    return subprocess.run(command, check=True, capture_output=True, text=True)


def test_dry_run_and_apply_are_rollback_safe(tmp_path):
    tool = Path(__file__).parents[1] / "tools" / "retention.py"
    save_dir = tmp_path / "savedata"
    save_dir.mkdir()
    db_path = tmp_path / "metadata.sqlite"
    make_database(db_path)

    # Fifteen revisions in one old month: newest ten satisfy keep-latest and
    # the monthly snapshot is already among them, so five should be retained
    # only in quarantine. One invalid completed revision is quarantined too.
    start = datetime(2024, 1, 1)
    with sqlite3.connect(db_path) as db:
        for index in range(15):
            revision = f"VALID-{index:02d}"
            stamp = (start + timedelta(hours=index)).isoformat(" ")
            db.execute(
                "INSERT INTO savedata VALUES (?, 'user', 'TITLE', 'C', NULL, ?, ?)",
                (revision, stamp, stamp),
            )
            make_archive(save_dir / revision, str(index).encode())

        db.execute(
            "INSERT INTO savedata VALUES "
            "('EMPTY', 'user', 'TITLE', 'C', NULL, ?, ?)",
            (start.isoformat(" "), start.isoformat(" ")),
        )
        with zipfile.ZipFile(save_dir / "EMPTY", "w"):
            pass

        db.execute(
            "INSERT INTO savedata VALUES "
            "('PENDING', 'user', 'TITLE', 'P', NULL, ?, NULL)",
            (start.isoformat(" "),),
        )
        (save_dir / "PENDING").write_bytes(b"in progress")

    dry_run = run_tool(tool, tmp_path)
    assert "keep=10 quarantine=6 invalid=1" in dry_run.stdout
    with sqlite3.connect(db_path) as db:
        assert db.execute("SELECT count(*) FROM savedata WHERE status='C'").fetchone()[0] == 16
    assert len(list(save_dir.iterdir())) == 17

    applied = run_tool(tool, tmp_path, apply=True)
    assert "APPLIED" in applied.stdout
    with sqlite3.connect(db_path) as db:
        assert db.execute("SELECT count(*) FROM savedata WHERE status='C'").fetchone()[0] == 10
        assert db.execute("SELECT count(*) FROM savedata WHERE status='R'").fetchone()[0] == 6
        assert db.execute("SELECT count(*) FROM savedata WHERE status='P'").fetchone()[0] == 1

    quarantines = list(tmp_path.glob("savedata-retention-quarantine-*"))
    backups = list(tmp_path.glob("metadata.sqlite.before-retention-*"))
    assert len(quarantines) == 1
    assert len(backups) == 1
    assert len(list(quarantines[0].iterdir())) == 6
    assert len(list(save_dir.iterdir())) == 11
