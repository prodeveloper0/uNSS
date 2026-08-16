FROM python:3.12-slim

# Which account the server runs as. Override at build time if the host uses
# different ids:  docker build --build-arg UID=1001 --build-arg GID=1001 .
ARG UID=1000
ARG GID=100

WORKDIR /app

# Opens nothing on the host — it only records the port in the image metadata.
# Reverse proxies read it from there: Traefik picks it up automatically as
# long as exactly one port is declared, so no port label is needed on the
# container.
EXPOSE 8989

COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt
COPY . /app

# The database path is relative, so metadata.sqlite lives in /app — and SQLite
# writes its journal *next to* the database. Without write access to the
# directory itself (not just the file), every write fails with
# "unable to open database file", even when the file is mounted correctly.
RUN chown -R ${UID}:${GID} /app

# Do not run as root. Bind-mounted metadata.sqlite and savedata/ must be owned
# by the same ids on the host, or the server cannot write to them.
USER ${UID}:${GID}

ENTRYPOINT ["python3", "main.py"]
