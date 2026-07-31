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
run_make client

echo
echo "완료:"
ls -la "$ROOT/client/client.nro"
