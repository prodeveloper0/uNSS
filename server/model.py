from pydantic import BaseModel


class TitleRevision(BaseModel):
    title_id: str
    revision_id: str
