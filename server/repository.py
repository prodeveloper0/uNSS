import sqlite3
import uuid

from abc import ABC, abstractmethod
from typing import Tuple

import aiosqlite

from singleton import singleton


class BaseSaveDataRepository(ABC):
    @abstractmethod
    async def get_latest_revision_by_title(self, user_name: str, title_id: str) -> str:
        """
        유저 귀속 타이틀 세이브 데이터의 최신 리비전 ID 조회
        :param user_name:
        :param title_id:
        :return: Revision ID
        """
        pass

    @abstractmethod
    async def get_revision_status(self, revision_id: str) -> str:
        """
        세이브 데이터 리비전 ID로 세이브 데이터 조회
        :param revision_id:
        :return:
        """
        pass

    @abstractmethod
    async def query_all_latest_revision_by_user(self, user_name: str) -> Tuple[str, str]:
        """
        모든 유저 귀속 타이틀 세이브 데이터의 최신 리비전 ID 조회
        :param user_name:
        :return: [(Title ID, Revision ID), ...]
        """
        pass

    @abstractmethod
    async def begin_new_revision(self, user_name: str, title_id: str) -> str:
        """
        세이브 데이터 리비전 트랜잭션 시작
        트랜잭션이 완료되전까지 최신 리비전으로 적용되지 않아햠
        :param user_name:
        :param title_id:
        :return: Revision ID
        """
        pass

    @abstractmethod
    async def commit_revision(self, revision_id: str):
        """
        세이브 데이터 리비전 트랜잭션 완료
        트랜잭션이 완료되면 최신 리비전으로 적용되어야 함
        :param revision_id:
        :return:
        """
        pass

    @abstractmethod
    async def abandon_revision(self, revision_id: str):
        """
        세이브 데이터 리비전 트랜잭션 폐기
        :param revision_id:
        :return:
        """
        pass


@singleton
class SQLiteSaveDataRepository(BaseSaveDataRepository):
    def __init__(self, db_path: str):
        self.db_path = db_path
        self._init_schema()

    def _init_schema(self):
        script = """
        CREATE TABLE IF NOT EXISTS savedata
        (
            revision_id TEXT NOT NULL PRIMARY KEY,
            user_name TEXT NOT NULL,
            title_id TEXT NOT NULL,
            status CHAR(1) NOT NULL DEFAULT 'P',
            archive_location TEXT DEFAULT NULL,
            created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
            completed_at DATETIME DEFAULT NULL
        );
        
        CREATE INDEX IF NOT EXISTS idx_savedata_user_name ON savedata(user_name);
        CREATE INDEX IF NOT EXISTS idx_savedata_statue ON savedata(status);
        CREATE INDEX IF NOT EXISTS idx_savedata_title_id ON savedata(title_id);
        CREATE INDEX IF NOT EXISTS idx_savedata_created_at ON savedata(completed_at DESC);
        """
        sqlite3.connect(self.db_path).executescript(script).close()

    async def get_latest_revision_by_title(self, user_name: str, title_id: str) -> str:
        # created_at 은 CURRENT_TIMESTAMP 라 초 단위다. 같은 초에 두 번 올리면
        # 순서가 정해지지 않고, SQLite 는 먼저 찾은 것 - 대개 가장 오래된 것 -
        # 을 준다. 복원할 때 지난 세이브를 받게 된다는 뜻이다.
        # rowid 는 INSERT 순서대로 늘어나므로 이것으로 동점을 깬다.
        query = """
        SELECT UPPER(revision_id)
        FROM savedata
        WHERE user_name = ? AND title_id = UPPER(?) AND status = 'C'
        ORDER BY created_at DESC, rowid DESC
        LIMIT 1
        """
        async with aiosqlite.connect(self.db_path) as conn:
            async with conn.execute(query, (user_name, title_id)) as cursor:
                result = await cursor.fetchone()
                return result[0] if result else None

    async def get_revision_status(self, revision_id: str) -> str:
        query = """
        SELECT UPPER(status)
        FROM savedata
        WHERE revision_id = UPPER(?)
        """
        async with aiosqlite.connect(self.db_path) as conn:
            async with conn.execute(query, (revision_id,)) as cursor:
                result = await cursor.fetchone()
                return result[0] if result else None

    async def query_all_latest_revision_by_user(self, user_name: str) -> Tuple[str, str]:
        # GROUP BY ... HAVING MAX(created_at) 은 타이틀마다 한 줄로 줄여주긴
        # 하지만, 그 한 줄이 어느 리비전인지는 정하지 않는다. 같은 초에 두 번
        # 올리면 오래된 쪽이 나왔다 (실측).
        #
        # 타이틀마다 최신 한 줄을 명시적으로 고른다. 동점은 rowid 로 깬다 -
        # get_latest_revision_by_title 과 같은 기준이어야 목록과 개별 조회가
        # 서로 다른 답을 내놓지 않는다.
        query = """
        SELECT UPPER(s.title_id), UPPER(s.revision_id)
        FROM savedata s
        WHERE s.user_name = ? AND s.status = 'C'
          AND s.rowid = (
              SELECT rowid
              FROM savedata
              WHERE user_name = s.user_name AND title_id = s.title_id AND status = 'C'
              ORDER BY created_at DESC, rowid DESC
              LIMIT 1
          )
        """
        async with aiosqlite.connect(self.db_path) as conn:
            async with conn.execute(query, (user_name,)) as cursor:
                return await cursor.fetchall()

    async def begin_new_revision(self, user_name: str, title_id: str) -> str:
        revision_id = str(uuid.uuid4()).upper()
        query = """
        INSERT INTO savedata (
            revision_id, 
            user_name, 
            title_id, 
            status, 
            archive_location
        )
        VALUES (UPPER(?), ?, UPPER(?), 'P', null)
        """
        async with aiosqlite.connect(self.db_path) as conn:
            await conn.execute(query, (revision_id, user_name, title_id))
            await conn.commit()
        return revision_id

    async def commit_revision(self, revision_id: str):
        query = """
        UPDATE savedata
        SET status = 'C', completed_at = CURRENT_TIMESTAMP
        WHERE revision_id = UPPER(?) AND status = 'P'
        """
        async with aiosqlite.connect(self.db_path) as conn:
            await conn.execute(query, (revision_id,))
            await conn.commit()

    async def abandon_revision(self, revision_id: str):
        query = """
        UPDATE savedata
        SET status = 'D', completed_at = CURRENT_TIMESTAMP
        WHERE revision_id = UPPER(?) AND status = 'P'
        """
        async with aiosqlite.connect(self.db_path) as conn:
            await conn.execute(query, (revision_id,))
            await conn.commit()
