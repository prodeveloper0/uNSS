#!/bin/sh
#
# sysmodule 을 먼저 빌드해서 NRO 의 romfs 에 넣은 뒤 클라이언트를 빌드한다.
#
# 순서가 중요하다. romfs 안의 exefs.nsp 는 빌드 시점에 NRO 안으로 들어가므로,
# 모듈을 고치고 이 스크립트를 거치지 않으면 앱은 예전 모듈을 계속 설치한다.
#
# 저장소 전체를 마운트한다. client-sysmodule/source 의 공용 파일들이
# ../../client/source 를 가리키는 심볼릭 링크라서, 하위 폴더만 마운트하면
# 컨테이너 안에서 링크가 끊긴다.

set -e

ROOT=$(cd "$(dirname "$0")" && pwd)
IMAGE=unss-client-builder:latest

run_make()
{
    subdir=$1
    shift
    docker run --rm \
        -v "$ROOT":/uNSS -w "/uNSS/$subdir" \
        -u "$(id -u):$(id -g)" -e HOME=/tmp \
        "$IMAGE" make "$@"
}

echo "==> sysmodule"
run_make client-sysmodule

echo "==> romfs"
mkdir -p "$ROOT/client/romfs"
# Makefile 의 TARGET 은 디렉토리 이름에서 나온다 -> client-sysmodule.nsp
cp "$ROOT/client-sysmodule/client-sysmodule.nsp" "$ROOT/client/romfs/exefs.nsp"
ls -la "$ROOT/client/romfs/exefs.nsp"

echo "==> client"
# NRO 규칙은 elf 와 nacp 에만 걸려 있고 romfs 안의 내용에는 걸려 있지 않다.
# 모듈만 고치고 클라이언트 소스를 건드리지 않으면 make 는 다시 링크할 이유를
# 찾지 못하고, 예전 exefs.nsp 가 든 NRO 가 그대로 남는다 - 이 스크립트가
# 막으려던 바로 그 일이다. 지워서 반드시 다시 만들게 한다.
rm -f "$ROOT/client/client.nro"
run_make client

echo
echo "완료:"
ls -la "$ROOT/client/client.nro"
