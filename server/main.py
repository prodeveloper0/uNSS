import click
import uvicorn

from fastapi import FastAPI, HTTPException
from starlette.requests import Request
from starlette.responses import FileResponse, PlainTextResponse

from repository import SQLiteSaveDataRepository
from service import SaveDataService

app = FastAPI()


repo = SQLiteSaveDataRepository("metadata.sqlite")
service = SaveDataService(repo)


@app.get("/users/{user_name}/saves")
async def query_all_user_save_data(user_name: str):
    value = "\n".join(
        [
            f"{item.title_id}|{item.revision_id}"
            for item in await service.query_all_latest_revision_by_user(user_name)
        ]
    )
    return PlainTextResponse(value)


@app.post("/users/{user_name}/saves/{title_id}/revisions")
async def issue_save_data_revision_id(user_name: str, title_id: str):
    return PlainTextResponse((await service.begin_new_revision(user_name, title_id)).revision_id)


@app.post("/users/{user_name}/saves/{title_id}/revisions/{revision_id}")
async def upload_save_data_revision(user_name: str, title_id: str, revision_id: str, request: Request):
    stream = request.stream()

    async def read_stream(size: int | None):
        try:
            return await stream.__anext__()
        except StopAsyncIteration:
            return None

    await service.process_file(revision_id, read_stream)
    return PlainTextResponse(revision_id)


@app.get("/users/{user_name}/saves/{title_id}/revisions")
async def get_revisions(user_name: str, title_id: str):
    revision = await service.get_latest_revision_by_title(user_name, title_id)
    return PlainTextResponse(revision.revision_id)


@app.get("/users/{user_name}/saves/{title_id}/revisions/{revision_id}/data")
async def download_save_data_revision(user_name: str, title_id: str, revision_id: str):
    if revision_id == "latest" or not revision_id:
        revision_id = (await service.get_latest_revision_by_title(user_name, title_id)).revision_id

    try:
        file_path = service.get_save_data_path(revision_id)
    except FileNotFoundError:
        raise HTTPException(status_code=404, detail="File not found")
    return FileResponse(file_path, media_type="application/octet-stream", filename=f"{revision_id}.sar")


@click.command()
@click.option("--host", default="0.0.0.0", help="Host to run the server on")
@click.option("--port", default=8989, help="Port to run the server on")
def main(host: str, port: int):
    uvicorn.run("main:app", host=host, port=port)


if __name__ == "__main__":
    main()
