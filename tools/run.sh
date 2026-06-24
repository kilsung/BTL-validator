#!/bin/bash
# BACnet BTL 검증 도구 실행 스크립트 (Linux/macOS)
# 사용법:
#   ./run.sh
#   PORT=5001 ./run.sh
#   BACNET_PORT=47809 ./run.sh
#   ./run.sh --port 5001 --bacnet-port 47809

set -e
cd "$(dirname "$0")"

PYTHON=$(command -v python3 || command -v python)
if [ -z "$PYTHON" ]; then
    echo "❌  Python 실행 파일이 없습니다. 설치 후 다시 실행하세요."
    exit 1
fi

exec "$PYTHON" run.py "$@"
