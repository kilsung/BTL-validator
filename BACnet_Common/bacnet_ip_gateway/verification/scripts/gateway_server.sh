#!/bin/bash

# 인자가 2개 미만으로 전달된 경우 사용법 출력 후 종료
if [ $# -lt 2 ]; then
    echo "사용법: $0 <시리얼포트> <슬레이브ID> [보드레이트] [--delay 초] [추가옵션...]"
    echo "예시: $0 /dev/ttySLAVE 2 19200"
    echo "      $0 /dev/ttySLAVE 1 19200 --delay 0.1"
    echo "      $0 /dev/ttySLAVE 1 19200 -d 0.05 -v"
    exit 1
fi

PORT=$1
SLAVE_ID=$2
BAUDRATE=${3:-19200}

# 앞선 3개의 기본 인자를 제거하고 남은 추가 옵션(예: -v)을 뒤로 전달합니다.
shift 3 2>/dev/null || shift $# 2>/dev/null

# 스크립트 파일이 위치한 디렉토리 경로 계산하여 modbus_rtu_slave.py 실행
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# Replace the wrapper shell with the simulator process.  The managed runner
# owns and waits for this PID; keeping an extra shell parent here can leave the
# Python simulator alive after a test case has been stopped and the next case
# may then connect to a stale control socket.
exec python3 -u "$SCRIPT_DIR/modbus_rtu_slave.py" "$PORT" "$SLAVE_ID" --baudrate "$BAUDRATE" "$@"
