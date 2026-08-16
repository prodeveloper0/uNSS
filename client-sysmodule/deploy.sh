#!/bin/sh
#
# 빌드한 sysmodule 을 스위치의 올바른 위치로 올린다.
#
#   ./deploy.sh 192.168.1.40
#   ./deploy.sh 192.168.1.40:21
#
# Atmosphere 는 atmosphere/contents/<PROGRAM_ID>/ 만 쳐다본다.
# 폴더 이름이 곧 프로그램 ID 이므로 config.json 에서 직접 읽어온다.
# 손으로 옮기면 ID 가 어긋나기 쉬워서 스크립트로 고정한다.

set -e

HOSTPORT=$1
if [ -z "$HOSTPORT" ]; then
    echo "usage: $0 <switch-ip>[:port]" >&2
    exit 1
fi

HOST=$(echo "$HOSTPORT" | cut -d':' -f1)
PORT=$(echo "$HOSTPORT" | cut -d':' -f2 -s)
PORT=${PORT:-21}

# Makefile 의 TARGET 은 디렉토리 이름에서 나온다 -> client-sysmodule.nsp
NSP=client-sysmodule.nsp
if [ ! -f "$NSP" ]; then
    echo "$NSP 이 없다. 먼저 make 를 실행할 것." >&2
    exit 1
fi

# "program_id": "0x0100000000554E53" -> 0100000000554E53
PROGRAM_ID=$(sed -n 's/.*"program_id"[^"]*"0x\([0-9A-Fa-f]*\)".*/\1/p' config.json | head -n 1)
if [ -z "$PROGRAM_ID" ]; then
    echo "config.json 에서 program_id 를 찾지 못했다." >&2
    exit 1
fi

BASE="ftp://$HOST:$PORT/ams_contents:/$PROGRAM_ID"
FTPOPTS="--connect-timeout 15 --user anonymous:anonymous --ftp-create-dirs -sS"

echo "-> $HOST:$PORT  (program id $PROGRAM_ID)"

# exefs.nsp 라는 이름이어야 한다. client.nsp 그대로 올리면 인식하지 않는다.
curl $FTPOPTS -T "$NSP" "$BASE/exefs.nsp"
echo "   exefs.nsp"

# 빈 파일이면 된다. 이게 있어야 부팅 시 자동 실행된다.
TMPFLAG=$(mktemp)
curl $FTPOPTS -T "$TMPFLAG" "$BASE/flags/boot2.flag"
rm -f "$TMPFLAG"
echo "   flags/boot2.flag"

echo "-> 재부팅하면 적용된다."
