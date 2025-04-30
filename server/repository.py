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
        query = """
        SELECT UPPER(revision_id)
        FROM savedata
        WHERE user_name = ? AND title_id = UPPER(?) AND status = 'C'
        ORDER BY created_at DESC
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
        query = """
        SELECT UPPER(title_id), UPPER(revision_id)
        FROM savedata
        WHERE user_name = ? AND status = 'C'
        GROUP BY title_id
        HAVING MAX(created_at)
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
