#!/bin/bash
# =============================================================================
# BACnet Gateway Verification Allure Report Viewer (Docker-based)
# Location: verification/view_report.sh
# =============================================================================
# Usage:
#   ./verification/view_report.sh [결과폴더경로] [포트번호]
# Examples:
#   ./verification/view_report.sh ./verification/results/20260901T063059Z
#   ./verification/view_report.sh                               # 최신 결과 자동 실행
# =============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
GATEWAY_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
PROJECT_DIR_NAME="$(basename "$GATEWAY_ROOT")"
DOCKER_IMAGE="bacnet-gateway-test:latest"
DOCKERFILE="$SCRIPT_DIR/docker/Dockerfile"
PORT="${2:-5050}"

# 1. Docker availability check
if ! command -v docker >/dev/null 2>&1; then
    echo "[ERROR] Docker가 시스템에 설치되어 있지 않거나 PATH에 없습니다." >&2
    exit 1
fi

# 2. Ensure Docker image exists and has allure installed
ensure_docker_image() {
    if ! docker image inspect "$DOCKER_IMAGE" >/dev/null 2>&1 || ! docker run --rm --entrypoint which "$DOCKER_IMAGE" allure >/dev/null 2>&1; then
        echo "======================================================="
        echo " [Docker] Allure가 포함된 $DOCKER_IMAGE 이미지를 빌드합니다..."
        echo "======================================================="
        docker build -t "$DOCKER_IMAGE" -f "$DOCKERFILE" "$GATEWAY_ROOT"
    fi
}

# 3. Determine target results directory
TARGET_DIR="$1"
if [ -z "$TARGET_DIR" ]; then
    LATEST_DIR=$(ls -td "$GATEWAY_ROOT/verification/results"/20* 2>/dev/null | head -n 1 || true)
    if [ -n "$LATEST_DIR" ] && [ -d "$LATEST_DIR" ]; then
        TARGET_DIR="$LATEST_DIR"
        echo "[INFO] 대상 폴더가 지정되지 않아 가장 최신 결과 폴더를 사용합니다:"
        echo "       $TARGET_DIR"
    else
        echo "[ERROR] 조회할 테스트 결과 폴더가 없습니다." >&2
        echo "        사용법: $0 <결과_디렉터리_경로>" >&2
        exit 1
    fi
fi

# Resolve relative path from GATEWAY_ROOT for Docker container path
if [ -d "$TARGET_DIR" ]; then
    ABS_TARGET_DIR="$(cd "$TARGET_DIR" && pwd)"
else
    echo "[ERROR] 지정된 결과 디렉터리가 존재하지 않습니다: $TARGET_DIR" >&2
    exit 1
fi

# Convert host absolute path to container path
CONTAINER_TARGET_DIR="/workspace/$PROJECT_DIR_NAME/${ABS_TARGET_DIR#$GATEWAY_ROOT/}"

# Detect host IP address for user convenience
HOST_IP=$(hostname -I 2>/dev/null | awk '{print $1}')
[ -z "$HOST_IP" ] && HOST_IP="127.0.0.1"

# 3.5. Port conflict check and interactive resolution
check_and_resolve_port() {
    local target_port="$1"

    # 포트 사용 여부 확인 (소켓 바인드 시도)
    local in_use=0
    if ! python3 -c "import socket, sys; s = socket.socket(socket.AF_INET, socket.SOCK_STREAM); s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1);
try:
    s.bind(('', $target_port))
    s.close()
    sys.exit(0)
except OSError:
    sys.exit(1)
" 2>/dev/null; then
        in_use=1
    fi

    if [ "$in_use" -eq 1 ]; then
        local pids
        pids=$(lsof -ti :"$target_port" 2>/dev/null || fuser "$target_port/tcp" 2>/dev/null || true)
        pids=$(echo "$pids" | tr '\n' ' ' | xargs)

        echo ""
        echo "[WARN] 포트 $target_port 가 이미 다른 프로세스에서 사용 중입니다."
        if [ -n "$pids" ]; then
            echo "       - 점유 프로세스 PID: $pids"
            ps -fp $pids 2>/dev/null || true
        fi

        local kill_confirmed=0
        if [ -t 0 ]; then
            read -r -p "[질문] 기존 프로세스를 종료하고 $target_port 포트로 실행하시겠습니까? (y/N): " answer
            case "$answer" in
                [yY]|[yY][eE][sS])
                    kill_confirmed=1
                    ;;
                *)
                    kill_confirmed=0
                    ;;
            esac
        else
            echo "[INFO] 비대화형(non-interactive) 환경이므로 대체 포트를 검색합니다."
            kill_confirmed=0
        fi

        if [ "$kill_confirmed" -eq 1 ]; then
            if [ -n "$pids" ]; then
                echo "[INFO] 기존 프로세스(PID: $pids)를 종료합니다..."
                kill $pids 2>/dev/null || true
                sleep 0.5
                local remaining_pids
                remaining_pids=$(lsof -ti :"$target_port" 2>/dev/null || fuser "$target_port/tcp" 2>/dev/null || true)
                remaining_pids=$(echo "$remaining_pids" | tr '\n' ' ' | xargs)
                if [ -n "$remaining_pids" ]; then
                    echo "[WARN] 정상 종료되지 않아 강제 종료(SIGKILL)합니다..."
                    kill -9 $remaining_pids 2>/dev/null || true
                    sleep 0.5
                fi
            fi

            # 포트 해제 여부 재확인
            if python3 -c "import socket, sys; s = socket.socket(socket.AF_INET, socket.SOCK_STREAM); s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1);
try:
    s.bind(('', $target_port))
    s.close()
    sys.exit(0)
except OSError:
    sys.exit(1)
" 2>/dev/null; then
                echo "[INFO] 포트 $target_port 가 성공적으로 확보되었습니다."
                PORT="$target_port"
                return 0
            else
                echo "[WARN] 포트 $target_port 가 즉시 해제되지 않아 대체 포트를 검색합니다."
            fi
        fi

        # 종료를 선택하지 않았거나 해제되지 않은 경우 대체 포트 탐색
        local next_port
        next_port=$(python3 -c "import socket, sys
for p in range($target_port + 1, $target_port + 50):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        s.bind(('', p))
        s.close()
        print(p)
        sys.exit(0)
    except OSError:
        s.close()
        continue
sys.exit(1)
" 2>/dev/null || true)

        if [ -n "$next_port" ]; then
            echo "[INFO] 기존 프로세스를 유지하고 대체 포트 $next_port 로 웹 서버를 시작합니다."
            PORT="$next_port"
        else
            echo "[ERROR] 가용한 대체 포트를 찾을 수 없습니다 ($((target_port + 1))..$((target_port + 50)))." >&2
            exit 1
        fi
    fi
}

check_and_resolve_port "$PORT"

echo ""
echo "======================================================="
echo "   BACnet Gateway Allure Report Viewer"
echo "======================================================="
echo " - Report Path   : $ABS_TARGET_DIR"
echo ""
echo " [웹 브라우저 접속 주소]"
echo "  * 로컬 PC 접속  : http://localhost:$PORT"
if [ "$HOST_IP" != "127.0.0.1" ]; then
    echo "  * 원격/네트워크 : http://$HOST_IP:$PORT"
fi
echo ""
echo " [안내] 위 주소를 웹 브라우저 주소창에 입력하여 접속하십시오."
echo "        (서버 종료: Ctrl + C)"
echo "======================================================="
echo ""

ensure_docker_image

# 고유한 컨테이너 이름 부여 및 종료 트랩 등록
CONTAINER_NAME="allure_viewer_${PORT}_$$"

cleanup() {
    # 중복 호출 방지
    trap - INT TERM EXIT
    echo ""
    echo "[INFO] Allure 리포트 서버를 종료합니다 (포트 $PORT 정리 중)..."
    docker stop -t 1 "$CONTAINER_NAME" >/dev/null 2>&1 || true
    docker rm -f "$CONTAINER_NAME" >/dev/null 2>&1 || true

    # 포트 잔여 프로세스 최종 확인 및 정리
    local pids
    pids=$(lsof -ti :"$PORT" 2>/dev/null || fuser "$PORT/tcp" 2>/dev/null || true)
    pids=$(echo "$pids" | tr '\n' ' ' | xargs)
    if [ -n "$pids" ]; then
        kill $pids 2>/dev/null || true
        sleep 0.3
        local remaining
        remaining=$(lsof -ti :"$PORT" 2>/dev/null || fuser "$PORT/tcp" 2>/dev/null || true)
        remaining=$(echo "$remaining" | tr '\n' ' ' | xargs)
        if [ -n "$remaining" ]; then
            kill -9 $remaining 2>/dev/null || true
        fi
    fi
    echo "[INFO] 포트 $PORT 가 정상적으로 해제되었습니다."
}

trap cleanup INT TERM EXIT

docker run --rm -it --name "$CONTAINER_NAME" --init --net=host --privileged \
    --user "$(id -u):$(id -g)" \
    -e PYTHONDONTWRITEBYTECODE=1 \
    -v "$GATEWAY_ROOT/..":/workspace \
    -w "/workspace/$PROJECT_DIR_NAME" \
    --entrypoint bash \
    "$DOCKER_IMAGE" \
    -c "
        rm -rf /tmp/allure-results /tmp/allure-report
        mkdir -p /tmp/allure-results

        # 1. Smart copy: Copy results and optimize heavy evidence files (>1MB)
        python3 -c \"from pathlib import Path; from verification.scripts.tc_framework import prepare_allure_results; prepare_allure_results(Path('$CONTAINER_TARGET_DIR'), Path('/tmp/allure-results'))\"

        # 2. Generate Allure report
        allure generate /tmp/allure-results -o /tmp/allure-report --clean >/dev/null 2>&1

        # 3. Patch Allure frontend assets: Replace 'Open attachment in new tab' with 'Download' and link raw attachments
        python3 -c \"from pathlib import Path; from verification.scripts.tc_framework import patch_allure_report; patch_allure_report(Path('/tmp/allure-report'), Path('$CONTAINER_TARGET_DIR'))\"

        # 4. Start HTTP Server with explicit signal handling & socket cleanup
        exec python3 -c \"import functools, http.server, socketserver, sys, signal
class NoCacheHandler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header('Cache-Control', 'no-store, no-cache, must-revalidate, max-age=0')
        self.send_header('Pragma', 'no-cache')
        self.send_header('Expires', '0')
        super().end_headers()
handler = functools.partial(NoCacheHandler, directory='/tmp/allure-report')
socketserver.TCPServer.allow_reuse_address = True
requested_port = int('$PORT')
httpd = None
actual_port = requested_port
for p in range(requested_port, requested_port + 20):
    try:
        httpd = socketserver.TCPServer(('', p), handler)
        actual_port = p
        break
    except OSError as e:
        if e.errno == 98:
            continue
        raise
if not httpd:
    sys.exit(f'[ERROR] 가용한 포트를 찾을 수 없습니다 ({requested_port}..{requested_port+19})')
if actual_port != requested_port:
    print(f'\n[WARN] {requested_port} 포트가 이미 사용 중입니다.')
    print(f'       대체 포트 {actual_port} 로 웹 서버를 시작합니다!')
    print(f' >> 로컬 접속: http://localhost:{actual_port}')
    if '$HOST_IP' != '127.0.0.1':
        print(f' >> 원격 접속: http://$HOST_IP:{actual_port}')
    print('=======================================================\n')

def sig_handler(signum, frame):
    sys.exit(0)

signal.signal(signal.SIGINT, sig_handler)
signal.signal(signal.SIGTERM, sig_handler)

try:
    httpd.serve_forever()
except (KeyboardInterrupt, SystemExit):
    pass
finally:
    try:
        httpd.server_close()
    except Exception:
        pass\"
    "

