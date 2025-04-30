#!/bin/bash

BASE_URL="http://127.0.0.1:8989"

# 1. 모든 사용자 저장 데이터를 조회
function query_all_user_save_data() {
  USER_NAME=$1
  curl -X GET "$BASE_URL/users/$USER_NAME/saves"
}

# 2. 새로운 저장 데이터 리비전 ID 발급
function issue_save_data_revision_id() {
  USER_NAME=$1
  TITLE_ID=$2
  curl -X POST "$BASE_URL/users/$USER_NAME/saves/$TITLE_ID/revisions"
}

# 3. 특정 타이틀의 최신 리비전 ID 조회
function get_latest_save_data_revision_id() {
  USER_NAME=$1
  TITLE_ID=$2
  curl -X GET "$BASE_URL/users/$USER_NAME/saves/$TITLE_ID/revisions"
}

# 4. 저장 데이터 리비전 업로드
function upload_save_data_revision() {
  USER_NAME=$1
  TITLE_ID=$2
  REVISION_ID=$3
  FILE_PATH=$4
  curl -X POST -H "Content-Type: application/octet-stream" "$BASE_URL/users/$USER_NAME/saves/$TITLE_ID/revisions/$REVISION_ID" --data-binary "@${FILE_PATH}"
}

# 5. 저장 데이터 리비전 다운로드.
function download_save_data_revision() {
  USER_NAME=$1
  TITLE_ID=$2
  REVISION_ID=$3
  curl -X GET "$BASE_URL/users/$USER_NAME/saves/$TITLE_ID/revisions/$REVISION_ID" -O
}

# 스크립트 실행 로직
if [[ $# -lt 1 ]]; then
  echo "사용법: $0 <함수명> [매개변수...]"
  exit 1
fi

FUNCTION_NAME=$1
shift

# 함수 실행
case $FUNCTION_NAME in
  query_all_user_save_data)
    query_all_user_save_data "$@"
    ;;
  issue_save_data_revision_id)
    issue_save_data_revision_id "$@"
    ;;
  get_latest_save_data_revision_id)
    get_latest_save_data_revision_id "$@"
    ;;
  upload_save_data_revision)
    upload_save_data_revision "$@"
    ;;
  download_save_data_revision)
    download_save_data_revision "$@"
    ;;
  *)
    echo "알 수 없는 함수: $FUNCTION_NAME"
    echo "사용 가능한 함수: query_all_user_save_data, issue_save_d ata_revision_id, get_latest_save_data_revision_id, upload_save_data_revision, download_save_data_revision"
    exit 1
    ;;
esac