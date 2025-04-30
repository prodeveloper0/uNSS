#!/bin/sh

PROJECTNAME=$(basename "$PWD")
HOSTPORT=$1
FILE=$PROJECTNAME.nro
TARGET_PATH=switch/$PROJECTNAME
REMOTE_PATH=$TARGET_PATH/$PROJECTNAME.nro

HOST=$(echo "$HOSTPORT" | cut -d':' -f1)
PORT=$(echo "$HOSTPORT" | cut -d':' -f2)

ftp -inv "$HOST" "$PORT" <<EOF
user anonymous anonymous
binary
mkdir $TARGET_PATH
cd $TARGET_PATH
delete $FILE
put $FILE
bye
EOF