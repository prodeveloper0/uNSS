import os
from typing import Sequence, Callable, Awaitable

from model import TitleRevision
from repository import BaseSaveDataRepository
from singleton import singleton


@singleton
class SaveDataService:
    BASEDIR = "savedata"

    def __init__(self, repo: BaseSaveDataRepository):
        self._repo = repo
        os.makedirs(self.BASEDIR, exist_ok=True)

    async def get_latest_revision_by_title(self, user_name: str, title_id: str) -> TitleRevision:
        """
        유저 귀속 타이틀 세이브 데이터의 최신 리비전 ID 조회
        :param user_name:
        :param title_id:
        :return: TitleRevision
        """
        revision_id = await self._repo.get_latest_revision_by_title(user_name, title_id)
        if not revision_id:
            raise ValueError(f"No revision found for title {title_id}")
        return TitleRevision(title_id=title_id, revision_id=revision_id)

    async def query_all_latest_revision_by_user(self, user_name: str) -> Sequence[TitleRevision]:
        """
        모든 유저 귀속 타이틀 세이브 데이터의 최신 리비전 ID 조회
        :param user_name:
        :return: [TitleRevision, ...]
        """
        revisions = await self._repo.query_all_latest_revision_by_user(user_name)
        return [TitleRevision(title_id=title_id, revision_id=revision_id) for title_id, revision_id in revisions]

    async def begin_new_revision(self, user_name: str, title_id: str) -> TitleRevision:
        """
        세이브 데이터 리비전 트랜잭션 시작
        트랜잭션이 완료되전까지 최신 리비전으로 적용되지 않아햠
        :param user_name:
        :param title_id:
        :return: TitleRevision
        """
        revision_id = await self._repo.begin_new_revision(user_name, title_id)
        return TitleRevision(title_id=title_id, revision_id=revision_id)

    async def commit_revision(self, revision_id: str):
        """
        세이브 데이터 리비전 트랜잭션 완료
        트랜잭션이 완료되면 최신 리비전으로 적용되어야 함
        :param revision_id:
        :return:
        """
        await self._repo.commit_revision(revision_id)

    async def abandon_revision(self, revision_id: str):
        """
        세이브 데이터 리비전 트랜잭션 폐기
        :param revision_id:
        :return:
        """
        await self._repo.abandon_revision(revision_id)

    async def process_file(self, revision_id: str, read: Callable[[int | None], Awaitable[bytes]]):
        """
        세이브 데이터 처리 및 트랜잭션 처리
        :param revision_id:
        :param read:
        :return:
        """
        revision_status = await self._repo.get_revision_status(revision_id)
        if revision_status != 'P':
            raise ValueError(f"Revision {revision_id} is not in progress")

        try:
            save_data_path = self.get_save_data_path(revision_id, ignore_file_exists=True)
            with open(save_data_path, "wb") as f:
                while True:
                    data = await read(None)
                    if data is None:
                        break
                    f.write(data)
        except Exception as e:
            await self.abandon_revision(revision_id)
            raise e
        else:
            await self.commit_revision(revision_id)

    def get_save_data_path(self, revision_id: str, ignore_file_exists: bool = False) -> str:
        """
        세이브 데이터 파일 경로 가져오기
        :param revision_id:
        :param ignore_file_exists:
        :return:
        """
        filepath = os.path.join(self.BASEDIR, revision_id)
        if not os.path.exists(filepath) and not ignore_file_exists:
            raise FileNotFoundError(f"File {filepath} not found")
        return filepath
