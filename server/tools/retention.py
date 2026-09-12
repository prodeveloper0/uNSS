#!/usr/bin/env python3
"""Safe uNSS revision retention.

Default is a read-only dry run. --apply never unlinks data: it marks selected
rows R (retained in SQLite) and atomically moves their blobs into a timestamped
quarantine directory beside savedata. Stop the server before --apply.
"""

import argparse
import collections
import datetime as dt
import os
from pathlib import Path, PurePosixPath
import shutil
import sqlite3
import sys
import zipfile


def safe_member(name: str) -> bool:
    if not name or "\\" in name or name.startswith("/"):
        return False
    path = PurePosixPath(name)
    return not path.is_absolute() and ".." not in path.parts


def validate_archive(path: Path) -> tuple[bool, str]:
    if not path.is_file():
        return False, "missing blob"
    try:
        with zipfile.ZipFile(path) as archive:
            infos = archive.infolist()
            if not infos:
                return False, "empty ZIP"
            names: set[str] = set()
            has_save = False
            for info in infos:
                name = info.filename
                if info.is_dir():
                    return False, "directory entry"
                if not safe_member(name):
                    return False, "unsafe path"
                if name in names:
                    return False, "duplicate path"
                names.add(name)
                has_save = has_save or name.startswith("saves/")
            if not has_save:
                return False, "no saves/ entry"
            bad = archive.testzip()
            if bad is not None:
                return False, f"CRC/decompression failure: {bad}"
    except (OSError, zipfile.BadZipFile, RuntimeError) as exc:
        return False, f"bad ZIP: {exc}"
    return True, "valid"


def parse_time(value: str | None) -> dt.datetime:
    if not value:
        return dt.datetime.min
    return dt.datetime.fromisoformat(value)


def select(rows, save_dir: Path, keep_latest: int, daily_days: int, now: dt.datetime):
    by_title = collections.defaultdict(list)
    invalid = []
    for row in rows:
        valid, reason = validate_archive(save_dir / row[1])
        item = {"row": row, "valid": valid, "reason": reason,
                "time": parse_time(row[6] or row[5])}
        if valid:
            by_title[(row[2], row[3])].append(item)
        else:
            invalid.append(item)

    keep_ids = set()
    cutoff = now - dt.timedelta(days=daily_days)
    for items in by_title.values():
        items.sort(key=lambda x: (x["time"], x["row"][0]), reverse=True)
        keep_ids.update(x["row"][1] for x in items[:keep_latest])

        daily = {}
        monthly = {}
        for item in items:
            stamp = item["time"]
            if stamp >= cutoff:
                daily.setdefault(stamp.date(), item)
            else:
                monthly.setdefault((stamp.year, stamp.month), item)
        keep_ids.update(x["row"][1] for x in daily.values())
        keep_ids.update(x["row"][1] for x in monthly.values())

    valid_items = [x for items in by_title.values() for x in items]
    remove = invalid + [x for x in valid_items if x["row"][1] not in keep_ids]
    keep = [x for x in valid_items if x["row"][1] in keep_ids]
    return keep, remove, invalid


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--base", type=Path, default=Path("."))
    parser.add_argument("--keep-latest", type=int, default=10)
    parser.add_argument("--daily-days", type=int, default=30)
    parser.add_argument("--apply", action="store_true")
    args = parser.parse_args()
    if args.keep_latest < 1 or args.daily_days < 0:
        parser.error("retention values must be positive")

    base = args.base.resolve()
    db_path = base / "metadata.sqlite"
    save_dir = base / "savedata"
    conn = sqlite3.connect(f"file:{db_path}?mode=ro", uri=True)
    rows = conn.execute("""
        SELECT rowid, revision_id, user_name, UPPER(title_id), status,
               created_at, completed_at
        FROM savedata WHERE status = 'C'
    """).fetchall()
    conn.close()

    now = dt.datetime.now(dt.UTC).replace(tzinfo=None)
    keep, remove, invalid = select(rows, save_dir, args.keep_latest,
                                   args.daily_days, now)
    remove_bytes = sum((save_dir / x["row"][1]).stat().st_size
                       for x in remove if (save_dir / x["row"][1]).is_file())
    print(f"completed={len(rows)} keep={len(keep)} quarantine={len(remove)} "
          f"invalid={len(invalid)} bytes={remove_bytes}")

    grouped = collections.defaultdict(lambda: [0, 0, 0, 0])
    for item in keep:
        grouped[(item["row"][2], item["row"][3])][0] += 1
    for item in remove:
        data = grouped[(item["row"][2], item["row"][3])]
        data[1] += 1
        data[3] += (save_dir / item["row"][1]).stat().st_size if (save_dir / item["row"][1]).is_file() else 0
    for item in invalid:
        grouped[(item["row"][2], item["row"][3])][2] += 1
    for (user, title), (kept, removed, bad, size) in sorted(grouped.items()):
        if removed:
            print(f"{user}|{title}: keep={kept} quarantine={removed} "
                  f"invalid={bad} bytes={size}")

    if not args.apply:
        print("DRY RUN ONLY; no database or blob was changed")
        return 0

    stamp = dt.datetime.now(dt.UTC).strftime("%Y%m%dT%H%M%SZ")
    backup_db = base / f"metadata.sqlite.before-retention-{stamp}"
    quarantine = base / f"savedata-retention-quarantine-{stamp}"
    quarantine.mkdir(mode=0o700)
    shutil.copy2(db_path, backup_db)

    moved = []
    try:
        for item in remove:
            revision = item["row"][1]
            source = save_dir / revision
            if source.exists():
                destination = quarantine / revision
                os.replace(source, destination)
                moved.append((destination, source))

        conn = sqlite3.connect(db_path)
        try:
            conn.execute("BEGIN IMMEDIATE")
            conn.executemany("UPDATE savedata SET status='R' "
                             "WHERE revision_id=? AND status='C'",
                             [(x["row"][1],) for x in remove])
            conn.commit()
        finally:
            conn.close()
    except Exception:
        for source, destination in reversed(moved):
            if source.exists():
                os.replace(source, destination)
        raise

    print(f"APPLIED; database backup: {backup_db}")
    print(f"Quarantine (rollback data, not deleted): {quarantine}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
