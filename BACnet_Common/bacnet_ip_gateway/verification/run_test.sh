#!/bin/bash
# =============================================================================
# BACnet Gateway & vport Verification Runner (Docker-based Portable Interface)
# Location: verification/run_test.sh
# =============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
GATEWAY_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
ES_ROOT="$(cd "$GATEWAY_ROOT/.." && pwd)"
PROJECT_DIR_NAME="$(basename "$GATEWAY_ROOT")"
CONFIG_FILE="$SCRIPT_DIR/test_config.json"
DOCKER_IMAGE="bacnet-gateway-test:latest"
DOCKERFILE="$SCRIPT_DIR/docker/Dockerfile"

# Normalize absolute paths under ES_ROOT to relative paths based on GATEWAY_ROOT
normalize_path() {
    local target="$1"
    [ -z "$target" ] && return
    if [[ "$target" != /* ]]; then
        echo "$target"
        return
    fi
    if [[ "$target" = "$GATEWAY_ROOT"* ]] || [[ "$target" = "$ES_ROOT"* ]]; then
        local rel
        rel="$(python3 -c "import os; print(os.path.relpath('$target', '$GATEWAY_ROOT'))" 2>/dev/null || echo "$target")"
        if [ -n "$rel" ]; then
            echo "$rel"
            return
        fi
    fi
    echo "$target"
}

# Check docker availability
if ! command -v docker >/dev/null 2>&1; then
    echo "[ERROR] Docker가 시스템에 설치되어 있지 않거나 PATH에 없습니다." >&2
    exit 1
fi

# Load test_config.json using python3
load_config() {
    python3 -c "
import json, sys
try:
    with open('$CONFIG_FILE', 'r', encoding='utf-8') as f:
        cfg = json.load(f)
    print('DEFAULT_MODE=' + str(cfg.get('default_mode', 'managed')))
    
    # Common
    common = cfg.get('common', {})
    print('TPI_PATH=' + str(common.get('tpi_path', './model/cdu/[BTL]cdu.tpi')))
    print('PICS_DOCX_PATH=' + str(common.get('pics_docx_path', './model/cdu/[BTL]FUNC_CHECKLIST_CDU.docx')))
    print('DEFAULT_SUITE=' + str(common.get('suite', 'quick')))
    print('DEVICE_ID=' + str(common.get('device_id', 1200)))
    print('EXPECTED_RESPONSE_TIME_MS=' + str(common.get('expected_response_time_ms', 200)))
    gw_cfg = common.get('gateway_config_dir') or cfg.get('managed', {}).get('gateway_config_dir') or cfg.get('live', {}).get('gateway_config_dir') or './conf'
    vp_cfg = common.get('vport_config_path') or cfg.get('managed', {}).get('vport_config_path') or cfg.get('live', {}).get('vport_config_path') or '../vport_release/vport.cfg'
    print('GATEWAY_CONFIG_DIR=' + str(gw_cfg))
    print('VPORT_CONFIG_PATH=' + str(vp_cfg))

    # Managed
    managed = cfg.get('managed', {})
    binaries = managed.get('binaries', {})
    print('GATEWAY_BIN=' + str(binaries.get('gateway_bin', './bacnet_ip_gateway')))
    print('VPORT_BIN=' + str(binaries.get('vport_bin', '../vport_release/vport')))
    print('VPORT_TEST_BIN=' + str(binaries.get('vport_test_bin', '../vport_release/vport_test')))
    print('UNIT_TEST_BIN=' + str(binaries.get('unit_test_bin', './verification/unit/run_tests')))
    print('MANAGED_BBMD_AUTO=' + str(managed.get('bbmd_auto', True)))

    # Live
    live = cfg.get('live', {})
    print('LIVE_TARGET_IP=' + str(live.get('target_ip', '192.168.1.100')))
    print('LIVE_TARGET_PORT=' + str(live.get('target_port', 47808)))
    print('LIVE_ALLOW_MUTATION=' + str(live.get('allow_live_mutation', False)))
    print('LIVE_BBMD_ENDPOINT=' + str(live.get('bbmd_endpoint', '')))
    print('LIVE_SSH_ENABLED=' + str(live.get('ssh_enabled', False)))
    print('LIVE_SSH_USER=' + str(live.get('ssh_user', 'root')))
    print('LIVE_SSH_PORT=' + str(live.get('ssh_port', 22)))
except Exception as e:
    sys.exit(1)
" 2>/dev/null
}

if [ -f "$CONFIG_FILE" ]; then
    eval "$(load_config)"
    GATEWAY_CONFIG_DIR="$(normalize_path "$GATEWAY_CONFIG_DIR")"
    VPORT_CONFIG_PATH="$(normalize_path "$VPORT_CONFIG_PATH")"
else
    DEFAULT_MODE="managed"
    GATEWAY_CONFIG_DIR="./conf"
    VPORT_CONFIG_PATH="../vport_release/vport.cfg"
    TPI_PATH="./model/cdu/[BTL]cdu.tpi"
    DEFAULT_SUITE="full"
    DEVICE_ID=1200
    EXPECTED_RESPONSE_TIME_MS=200

    GATEWAY_BIN="./bacnet_ip_gateway"
    VPORT_BIN="../vport_release/vport"
    VPORT_TEST_BIN="../vport_release/vport_test"
    UNIT_TEST_BIN="./verification/unit/run_tests"
    MANAGED_BBMD_AUTO=True

    LIVE_TARGET_IP="192.168.1.100"
    LIVE_TARGET_PORT=47808
    LIVE_ALLOW_MUTATION=False
    LIVE_BBMD_ENDPOINT=""
    LIVE_SSH_ENABLED=False
    LIVE_SSH_USER="root"
    LIVE_SSH_PORT=22
fi

# Save current user inputs to test_config.json
save_config() {
    python3 -c "
import json, sys
cfg_path = '$CONFIG_FILE'
try:
    try:
        with open(cfg_path, 'r', encoding='utf-8') as f:
            cfg = json.load(f)
    except Exception:
        cfg = {}

    cfg['default_mode'] = 'managed'
    common = cfg.setdefault('common', {})
    if '$SUITE':
        common['suite'] = '$SUITE'
    if '$GATEWAY_CONFIG_DIR':
        common['gateway_config_dir'] = '$GATEWAY_CONFIG_DIR'
    if '$VPORT_CONFIG_PATH':
        common['vport_config_path'] = '$VPORT_CONFIG_PATH'
    if '$TPI_PATH':
        common['tpi_path'] = '$TPI_PATH'
    if '$PICS_DOCX_PATH':
        common['pics_docx_path'] = '$PICS_DOCX_PATH'

    managed = cfg.setdefault('managed', {})
    managed.pop('gateway_config_dir', None)
    managed.pop('vport_config_path', None)

    live = cfg.setdefault('live', {})
    live.pop('gateway_config_dir', None)
    live.pop('vport_config_path', None)
    live.pop('ssh_pass', None)  # 보안: 비밀번호는 파일에 영구 저장하지 않음
    if '$MODE' == 'live':
        if '$TARGET_IP':
            live['target_ip'] = '$TARGET_IP'
        if '$TARGET_PORT':
            live['target_port'] = int('$TARGET_PORT')
        live['allow_live_mutation'] = ('$LIVE_MUTATION_STATUS' == '허용 (제어/쓰기 TC 포함)')
        if '$LIVE_BBMD_VAL' and '$LIVE_BBMD_VAL' != '내장 BBMD 자동 구동':
            live['bbmd_endpoint'] = '$LIVE_BBMD_VAL'
        live['ssh_enabled'] = ('$SAVED_SSH_ENABLED' == 'True')
        live['ssh_user'] = '$CURRENT_SSH_USER'
        live['ssh_port'] = int('$CURRENT_SSH_PORT')

    with open(cfg_path, 'w', encoding='utf-8') as f:
        json.dump(cfg, f, indent=2, ensure_ascii=False)
        f.write('\n')
except Exception as e:
    pass
" 2>/dev/null || true
}

# Ensure Docker image exists or build it
ensure_docker_image() {
    if ! docker image inspect "$DOCKER_IMAGE" >/dev/null 2>&1; then
        echo "======================================================="
        echo " [Docker] $DOCKER_IMAGE 이미지가 없어 새로 빌드합니다..."
        echo "======================================================="
        docker build -t "$DOCKER_IMAGE" -f "$DOCKERFILE" "$GATEWAY_ROOT"
    fi
}

# Build target directory using host toolchain or fallback to Docker
build_dir() {
    local target_dir="$1"
    local make_target="$2"
    local desc="$3"

    if [ ! -d "$target_dir" ] || [ ! -f "$target_dir/Makefile" ]; then
        return 1
    fi

    echo "======================================================="
    echo " [Auto-Build] $desc 자동 빌드를 시작합니다..."
    echo "  - 디렉터리: $target_dir"
    [ -n "$make_target" ] && echo "  - 타깃    : $make_target"
    echo "======================================================="

    if command -v make >/dev/null 2>&1 && command -v g++ >/dev/null 2>&1; then
        echo "[Build] 호스트 툴체인(g++/make)을 사용하여 빌드합니다."
        if [ -n "$make_target" ]; then
            make -C "$target_dir" "$make_target"
        else
            make -C "$target_dir"
        fi
    else
        echo "[Build] 호스트에 C++ 컴파일러가 없어 Docker 컨테이너 환경에서 빌드를 수행합니다..."
        ensure_docker_image
        local rel_path
        rel_path="$(python3 -c "import os; print(os.path.relpath('$target_dir', '$GATEWAY_ROOT/..'))" 2>/dev/null || true)"
        if [ -z "$rel_path" ]; then
            rel_path="$PROJECT_DIR_NAME"
        fi
        docker run --rm \
            --entrypoint /bin/bash \
            --user "$(id -u):$(id -g)" \
            -v "$GATEWAY_ROOT/..":/workspace \
            -w "/workspace/$rel_path" \
            "$DOCKER_IMAGE" \
            -c "make $make_target"
    fi
    echo "[Auto-Build] $desc 빌드 완료."
    echo ""
}

# Check and auto-build required binaries if source Makefile exists
ensure_binaries() {
    # 1. Gateway binary
    local need_gw_build=0
    if [ ! -f "$GATEWAY_ROOT/$GATEWAY_BIN" ]; then
        need_gw_build=1
    elif [ -d "$GATEWAY_ROOT/src" ] && [ -n "$(find "$GATEWAY_ROOT/src" -type f -newer "$GATEWAY_ROOT/$GATEWAY_BIN" 2>/dev/null)" ]; then
        need_gw_build=1
    fi
    if [ "$need_gw_build" = "1" ]; then
        if [ -f "$GATEWAY_ROOT/Makefile" ]; then
            echo "[Notice] '$GATEWAY_BIN' 바이너리가 없거나 소스 코드가 변경되었습니다. 빌드를 진행합니다."
            build_dir "$GATEWAY_ROOT" "" "bacnet_ip_gateway" || true
        fi
    fi

    # 2. vport / vport_test binary (vport_release)
    local vport_dir
    vport_dir="$(cd "$GATEWAY_ROOT/$(dirname "$VPORT_BIN")" 2>/dev/null && pwd || true)"
    if [ ! -f "$GATEWAY_ROOT/$VPORT_BIN" ] || [ ! -f "$GATEWAY_ROOT/$VPORT_TEST_BIN" ]; then
        if [ -n "$vport_dir" ] && [ -f "$vport_dir/Makefile" ]; then
            echo "[Notice] vport / vport_test 바이너리가 없습니다. $vport_dir 소스에서 빌드를 진행합니다."
            build_dir "$vport_dir" "all" "vport & vport_test" || true
        fi
    fi

    # 3. Unit test binary
    if [ ! -f "$GATEWAY_ROOT/$UNIT_TEST_BIN" ]; then
        if [ -f "$GATEWAY_ROOT/Makefile" ]; then
            echo "[Notice] '$UNIT_TEST_BIN' 바이너리가 없습니다. 소스에서 빌드를 진행합니다."
            build_dir "$GATEWAY_ROOT" "$UNIT_TEST_BIN" "unit tests" || true
        fi
    fi
}

# If CLI arguments are provided, bypass interactive mode
if [ $# -gt 0 ]; then
    # Auto-build binaries if not explicitly running live mode
    if [[ ! "$*" =~ "--mode live" ]] && [[ ! "$*" =~ "-m live" ]]; then
        ensure_binaries
    fi
    ensure_docker_image
    SSH_CLI_VOL=""
    [ -d "$HOME/.ssh" ] && SSH_CLI_VOL="-v $HOME/.ssh:/root/.ssh:ro -v $HOME/.ssh:$HOME/.ssh:ro"
    USER_MAP_OPT=""
    [ -f "/etc/passwd" ] && USER_MAP_OPT="-v /etc/passwd:/etc/passwd:ro -v /etc/group:/etc/group:ro"
    exec docker run --rm -it --net=host --privileged \
        --user "$(id -u):$(id -g)" \
        -e PYTHONDONTWRITEBYTECODE=1 \
        -e SSH_PASS="${SSH_PASS:-}" \
        -v "$GATEWAY_ROOT/..":/workspace \
        -w "/workspace/$PROJECT_DIR_NAME" \
        $SSH_CLI_VOL \
        $USER_MAP_OPT \
        "$DOCKER_IMAGE" "$@"
fi

# Interactive Menu
echo "======================================================="
echo "   BACnet Gateway & vport Verification Runner"
echo "======================================================="
echo ""

# 1. Mode selection
DEFAULT_MODE_NUM=1
[ "$DEFAULT_MODE" = "live" ] && DEFAULT_MODE_NUM=2

echo "[1] 테스트 모드를 선택하십시오:"
echo "    1) Managed (PC 로컬 가상 시뮬레이션 - 전체 자동 검증) [기본값]"
echo "    2) Live (원격 실행 검증)"
read -p "선택 (1/2, 기본: $DEFAULT_MODE_NUM): " MODE_CHOICE
MODE_CHOICE=${MODE_CHOICE:-$DEFAULT_MODE_NUM}

if [ "$MODE_CHOICE" = "2" ]; then
    MODE="live"
else
    MODE="managed"
fi

# 2. Suite selection
echo ""
echo "[2] 테스트 스위트를 선택하십시오:"
echo "    1) quick (기본 기능 54개 빠른 검증) [기본값]"
echo "    2) full (기능/성능 84개 정밀 검증)"
echo "    3) all (감사/빌드 포함 88개 전체 검증)"
echo "    4) soak (12시간 전체 관제점 장기 연속 운용 신뢰성)"
echo "    5) case 지정 (예: TC-BAC-004, TC-BTL-*, TC-AVL-004)"
read -p "선택 (1/2/3/4/5, 기본: 1): " SUITE_CHOICE
SUITE_CHOICE=${SUITE_CHOICE:-1}

CASE_ARG=""
SOAK_ARG=""
SOAK_HOURS=""
SOAK_INTERVAL=""
if [ "$SUITE_CHOICE" = "2" ]; then
    SUITE="full"
elif [ "$SUITE_CHOICE" = "3" ]; then
    SUITE="all"
elif [ "$SUITE_CHOICE" = "4" ]; then
    SUITE="soak"
    CASE_ARG="--case TC-AVL-004"
    echo ""
    read -p "    신뢰성 시험 시간 (시간 단위) [기본값: 12.0]: " INPUT_SOAK_HOURS
    SOAK_HOURS=${INPUT_SOAK_HOURS:-12.0}
    read -p "    전체 관제점 순환 질의 주기 (초 단위) [기본값: 15.0]: " INPUT_SOAK_INTERVAL
    SOAK_INTERVAL=${INPUT_SOAK_INTERVAL:-15.0}
    SOAK_ARG="--soak --soak-hours $SOAK_HOURS --soak-interval $SOAK_INTERVAL"
elif [ "$SUITE_CHOICE" = "5" ]; then
    SUITE="all"
    read -p "실행할 TC ID 또는 패턴을 입력하십시오 (예: TC-BAC-004, TC-AVL-004): " CASE_NAME
    if [ -n "$CASE_NAME" ]; then
        CASE_ARG=""
        for tc_item in ${CASE_NAME//,/ }; do
            CASE_ARG="$CASE_ARG --case $tc_item"
        done
        if [[ "$CASE_NAME" =~ "TC-AVL-004" ]] || [[ "$CASE_NAME" =~ "soak" ]]; then
            echo ""
            read -p "    신뢰성 시험 시간 (시간 단위) [기본값: 12.0]: " INPUT_SOAK_HOURS
            SOAK_HOURS=${INPUT_SOAK_HOURS:-12.0}
            read -p "    전체 관제점 순환 질의 주기 (초 단위) [기본값: 15.0]: " INPUT_SOAK_INTERVAL
            SOAK_INTERVAL=${INPUT_SOAK_INTERVAL:-15.0}
            SOAK_ARG="--soak --soak-hours $SOAK_HOURS --soak-interval $SOAK_INTERVAL"
        fi
    fi
else
    SUITE="quick"
fi

# 3. Common Local PC Configuration & Specification file paths
echo ""
echo "[3] 설정 및 명세서 경로 확인 (로컬 PC):"
echo "    (검증 대상에 적용된 설정 파일 및 BTL/TPI 명세서 경로를 확인합니다.)"
read -p "    Gateway 설정 디렉터리        [기본값: $GATEWAY_CONFIG_DIR]: " INPUT_GW_CONF
GATEWAY_CONFIG_DIR="$(normalize_path "${INPUT_GW_CONF:-$GATEWAY_CONFIG_DIR}")"

read -p "    vport 설정 경로 (파일/폴더)  [기본값: $VPORT_CONFIG_PATH]: " INPUT_VP_CONF
VPORT_CONFIG_PATH="$(normalize_path "${INPUT_VP_CONF:-$VPORT_CONFIG_PATH}")"

DEFAULT_TPI="${TPI_PATH:-./model/cdu/[BTL]cdu.tpi}"
read -p "    TPI (EPICS) 파일 경로        [기본값: $DEFAULT_TPI]: " INPUT_TPI
TPI_PATH="$(normalize_path "${INPUT_TPI:-$DEFAULT_TPI}")"

PICS_ARG=""
if [ "$SUITE" = "all" ] || [ "$SUITE" = "full" ] || [[ "$CASE_NAME" =~ "BTL" ]]; then
    DEFAULT_PICS="${PICS_DOCX_PATH:-./model/cdu/[BTL]FUNC_CHECKLIST_CDU.docx}"
    read -p "    BTL 기능 체크리스트(PICS) 경로 [기본값: $DEFAULT_PICS]: " INPUT_PICS
    ACTUAL_PICS="$(normalize_path "${INPUT_PICS:-$DEFAULT_PICS}")"
    if [ -n "$ACTUAL_PICS" ]; then
        PICS_ARG="--pics-doc $ACTUAL_PICS"
        PICS_DOCX_PATH="$ACTUAL_PICS"
    fi
fi

if [ ! -d "$GATEWAY_ROOT/$GATEWAY_CONFIG_DIR" ] && [ ! -d "$GATEWAY_CONFIG_DIR" ]; then
    echo "    [주의] Gateway 설정 디렉터리를 찾을 수 없습니다: $GATEWAY_CONFIG_DIR"
fi
if [ ! -f "$GATEWAY_ROOT/$VPORT_CONFIG_PATH" ] && [ ! -f "$VPORT_CONFIG_PATH" ] && \
   [ ! -f "$GATEWAY_ROOT/$VPORT_CONFIG_PATH/vport.cfg" ] && [ ! -f "$VPORT_CONFIG_PATH/vport.cfg" ]; then
    echo "    [주의] vport 설정 경로(파일 또는 디렉터리)를 찾을 수 없습니다: $VPORT_CONFIG_PATH"
fi
if [ ! -f "$GATEWAY_ROOT/$TPI_PATH" ] && [ ! -f "$TPI_PATH" ]; then
    echo "    [주의] TPI 전자 명세서 파일을 찾을 수 없습니다: $TPI_PATH"
fi
if [ -n "$PICS_ARG" ] && [ ! -f "$GATEWAY_ROOT/$PICS_DOCX_PATH" ] && [ ! -f "$PICS_DOCX_PATH" ]; then
    echo "    [주의] BTL 기능 체크리스트(PICS) 문서를 찾을 수 없습니다: $PICS_DOCX_PATH"
fi

# 4. Mode-specific configuration
if [ "$MODE" = "managed" ]; then
    TARGET_IP="127.0.0.1"
    TARGET_PORT=47808

    BBMD_ARG="--bbmd-auto"
    LONG_ARG=""
    LONG_STATUS="제외 (일반 빠른 실행)"

    if [ "$SUITE" != "soak" ]; then
        # BBMD option for Managed mode
        echo ""
        echo "[4] BBMD (Foreign Device) 옵션:"
        read -p "    외부 BBMD 주소 (host:port, Enter 입력 시 내장 BBMD 자동 구동): " INPUT_BBMD
        if [ -n "$INPUT_BBMD" ]; then
            BBMD_ARG="--bbmd-endpoint $INPUT_BBMD"
        else
            BBMD_ARG="--bbmd-auto"
        fi
    fi

    # Preflight check and automatic build for binaries
    ensure_binaries

    MISSING_BINS=""
    [ ! -f "$GATEWAY_ROOT/$GATEWAY_BIN" ] && MISSING_BINS="$MISSING_BINS bacnet_ip_gateway"
    [ ! -f "$GATEWAY_ROOT/$VPORT_BIN" ] && MISSING_BINS="$MISSING_BINS vport"
    if [ -n "$MISSING_BINS" ]; then
        echo ""
        echo "[경고] 다음 바이너리를 찾을 수 없거나 빌드에 실패했습니다:$MISSING_BINS"
        echo "       해당 소스 디렉터리의 Makefile 및 의존성을 확인하십시오."
        echo ""
    fi

    ALLOW_BUILD_ARG=""
    ALLOW_BUILD_STATUS="제외"
    if [ "$SUITE" != "soak" ]; then
        # Step 5: Long tests option
        has_long_tc=0
        if [ "$SUITE_CHOICE" = "2" ] || [ "$SUITE_CHOICE" = "3" ]; then
            has_long_tc=1
        elif [ "$SUITE_CHOICE" = "5" ] && [ -n "$CASE_NAME" ]; then
            for tc_item in ${CASE_NAME//,/ }; do
                case "$tc_item" in
                    TC-CFG-006|TC-CFG-008|TC-BAC-012|TC-BAC-015|TC-OBS-003|\
                    TC-CFG*|TC-BAC*|TC-OBS*|TC-*|*)
                        has_long_tc=1
                        break
                        ;;
                esac
            done
        fi

        if [ "$has_long_tc" = "1" ]; then
            echo ""
            echo "[5] 장시간(60초/10분) 소요 스트레스 테스트 포함 여부:"
            echo "    (DCC 만료 60초, 설정 재검증 60초, 10분 포트 무응답 시험 등 총 5개 TC, 약 15분 소요)"
            read -p "    포함하시겠습니까? (y/N, 기본: N): " INPUT_LONG
            INPUT_LONG=${INPUT_LONG:-N}

            if [ "$INPUT_LONG" = "y" ] || [ "$INPUT_LONG" = "Y" ] || [ "$INPUT_LONG" = "yes" ]; then
                LONG_ARG="--long"
                LONG_STATUS="포함 (60초/10분 장기 시험 실행)"
            fi
        fi

        # Step 6: Host build option (TC-ENV-001)
        has_env_001=0
        if [ "$SUITE_CHOICE" = "3" ]; then
            has_env_001=1
        elif [ "$SUITE_CHOICE" = "5" ] && [ -n "$CASE_NAME" ]; then
            for tc_item in ${CASE_NAME//,/ }; do
                if [ "$tc_item" = "TC-ENV-001" ] || [ "$tc_item" = "TC-ENV*" ] || [ "$tc_item" = "TC-*" ] || [ "$tc_item" = "*" ]; then
                    has_env_001=1
                    break
                fi
            done
        fi

        if [ "$has_env_001" = "1" ]; then
            echo ""
            echo "[6] 호스트 빌드/컴파일러 경고 검증(TC-ENV-001) 허용 여부:"
            echo "    (컨테이너 내 host 'make test' 실행 및 컴파일러 warning 유무 검사, makeIMX는 실행하지 않음)"
            read -p "    --allow-build 를 활성화하시겠습니까? (y/N, 기본: N): " INPUT_BUILD
            INPUT_BUILD=${INPUT_BUILD:-N}
            if [ "$INPUT_BUILD" = "y" ] || [ "$INPUT_BUILD" = "Y" ] || [ "$INPUT_BUILD" = "yes" ]; then
                ALLOW_BUILD_ARG="--allow-build"
                ALLOW_BUILD_STATUS="포함 (host make test 검증 허용)"
            fi
        fi
    else
        LONG_STATUS="해당 없음 (Soak 전용)"
    fi

    RUN_ARGS="--suite $SUITE --mode managed --gateway-config-dir $GATEWAY_CONFIG_DIR --vport-config-path $VPORT_CONFIG_PATH --tpi $TPI_PATH $PICS_ARG --ip $TARGET_IP --port $TARGET_PORT --device-id $DEVICE_ID --response-time-ms $EXPECTED_RESPONSE_TIME_MS --gateway-bin $GATEWAY_BIN --vport-bin $VPORT_BIN --vport-test-bin $VPORT_TEST_BIN --unit-test-bin $UNIT_TEST_BIN $BBMD_ARG $LONG_ARG $ALLOW_BUILD_ARG $SOAK_ARG $CASE_ARG"

else
    # Live mode configuration
    echo ""
    read -p "[4] 원격 타깃 IP 주소 [기본값: $LIVE_TARGET_IP]: " INPUT_IP
    TARGET_IP=${INPUT_IP:-$LIVE_TARGET_IP}

    read -p "    타깃 BACnet UDP 포트 [기본값: $LIVE_TARGET_PORT]: " INPUT_PORT
    TARGET_PORT=${INPUT_PORT:-$LIVE_TARGET_PORT}

    BBMD_ARG="--bbmd-auto"
    EXTRA_LIVE_ARGS=""
    LONG_ARG=""
    LIVE_MUTATION_STATUS="해당 없음 (조회 전용)"
    LONG_STATUS="해당 없음 (Soak 전용)"

    if [ "$SUITE" != "soak" ]; then
        # 5. BBMD option for Live mode
        echo ""
        echo "[5] BBMD 옵션:"
        if [ -n "$LIVE_BBMD_ENDPOINT" ]; then
            read -p "    외부 BBMD 주소 (host:port, Enter 입력 시 기본값 사용) [기본값: $LIVE_BBMD_ENDPOINT]: " INPUT_BBMD
            LIVE_BBMD_VAL=${INPUT_BBMD:-$LIVE_BBMD_ENDPOINT}
            BBMD_ARG="--bbmd-endpoint $LIVE_BBMD_VAL"
        else
            read -p "    외부 BBMD 주소 (host:port, Enter 입력 시 내장 BBMD 자동 구동): " INPUT_BBMD
            if [ -n "$INPUT_BBMD" ]; then
                LIVE_BBMD_VAL="$INPUT_BBMD"
                BBMD_ARG="--bbmd-endpoint $LIVE_BBMD_VAL"
            else
                LIVE_BBMD_VAL="내장 BBMD 자동 구동"
                BBMD_ARG="--bbmd-auto"
            fi
        fi

        # 6. Live mutation option (WriteProperty / DCC / SubscribeCOV)
        echo ""
        echo "[6] Live 장비 상태 변경(Mutation) 테스트 허용 여부:"
        echo "    (WriteProperty 쓰기 제어, DCC 통신 차단, COV 구독 등 실제 장비의 값을 변경하는 TC)"
        DEFAULT_MUTATION_STR="N"
        if [ "$LIVE_ALLOW_MUTATION" = "True" ] || [ "$LIVE_ALLOW_MUTATION" = "true" ]; then
            DEFAULT_MUTATION_STR="Y"
        fi
        read -p "    상태 변경 TC를 허용하시겠습니까? (y/N, 기본: $DEFAULT_MUTATION_STR): " INPUT_MUTATION
        INPUT_MUTATION=${INPUT_MUTATION:-$DEFAULT_MUTATION_STR}

        if [ "$INPUT_MUTATION" = "y" ] || [ "$INPUT_MUTATION" = "Y" ] || [ "$INPUT_MUTATION" = "yes" ]; then
            EXTRA_LIVE_ARGS="--allow-live-mutation"
            LIVE_MUTATION_STATUS="허용 (제어/쓰기 TC 포함)"
        else
            LIVE_MUTATION_STATUS="차단 (안전 모드 - 조회 전용)"
        fi

        # 7. Long tests option for Live mode
        echo ""
        echo "[7] 장시간(60초/10분) 소요 스트레스 테스트 포함 여부:"
        echo "    (DCC 만료 60초 대기 등)"
        read -p "    포함하시겠습니까? (y/N, 기본: N): " INPUT_LONG
        INPUT_LONG=${INPUT_LONG:-N}

        if [ "$INPUT_LONG" = "y" ] || [ "$INPUT_LONG" = "Y" ] || [ "$INPUT_LONG" = "yes" ]; then
            LONG_ARG="--long"
            LONG_STATUS="포함"
        else
            LONG_STATUS="제외"
        fi
    fi

    # 8. SSH remote connection and resource monitoring option for Live mode
    echo ""
    echo "[8] 원격 SSH 접속 및 OS 자원(메모리/FD/vport) 모니터링 옵션:"
    echo "    (Live 검증 시 프로세스 생존 및 메모리/FD 누수 여부를 실시간 모니터링하기 위해 SSH 접속을 사용합니다.)"
    DEFAULT_SSH_STR="N"
    if [ "$LIVE_SSH_ENABLED" = "True" ] || [ "$LIVE_SSH_ENABLED" = "true" ]; then
        DEFAULT_SSH_STR="Y"
    fi
    read -p "    원격 SSH 접속을 활성화하시겠습니까? (y/N, 기본: $DEFAULT_SSH_STR): " INPUT_SSH
    INPUT_SSH=${INPUT_SSH:-$DEFAULT_SSH_STR}

    SSH_ARG=""
    SAVED_SSH_ENABLED="False"
    CURRENT_SSH_PASS=""

    test_ssh_connection() {
        local u="$1"
        local ip="$2"
        local port="$3"
        local pass="$4"
        local ssh_out=""
        local ret=0

        set +e
        if [ -n "$pass" ]; then
            if command -v sshpass >/dev/null 2>&1; then
                ssh_out=$(sshpass -p "$pass" ssh -p "$port" -o BatchMode=no -o ConnectTimeout=4 -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR -o HostKeyAlgorithms=+ssh-rsa -o PubkeyAcceptedKeyTypes=+ssh-rsa -o KexAlgorithms=+diffie-hellman-group14-sha1,diffie-hellman-group-exchange-sha1 "$u@$ip" "echo SSH_TEST_OK" 2>&1)
                ret=$?
            else
                ssh_out=$(python3 -c "
import os, sys, tempfile, subprocess

u, ip, port, password = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4]

def try_askpass():
    askpass_file = None
    try:
        with tempfile.NamedTemporaryFile('w', delete=False) as f:
            f.write('#!/bin/sh\necho \"' + password.replace('\\\\', '\\\\\\\\').replace('\"', '\\\\\"').replace('$', '\\\\$') + '\"\n')
            askpass_file = f.name
        os.chmod(askpass_file, 0o755)
        env = os.environ.copy()
        env['SSH_ASKPASS'] = askpass_file
        env['SSH_ASKPASS_REQUIRE'] = 'force'
        env['DISPLAY'] = ':0'
        cmd = ['ssh', '-p', str(port), '-o', 'StrictHostKeyChecking=no', '-o', 'UserKnownHostsFile=/dev/null', '-o', 'ConnectTimeout=4', '-o', 'LogLevel=ERROR', '-o', 'HostKeyAlgorithms=+ssh-rsa', '-o', 'PubkeyAcceptedKeyTypes=+ssh-rsa', '-o', 'KexAlgorithms=+diffie-hellman-group14-sha1,diffie-hellman-group-exchange-sha1', '-o', 'BatchMode=no', f'{u}@{ip}', 'echo SSH_TEST_OK']
        res = subprocess.run(cmd, env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=6)
        if 'SSH_TEST_OK' in res.stdout:
            print('SSH_TEST_OK')
            return 0
        err = (res.stderr or res.stdout).strip()
        lines = [l for l in err.splitlines() if 'password:' not in l.lower()]
        print('\n'.join(lines) if lines else 'Permission denied, please try again.')
        return 1
    except Exception:
        return None
    finally:
        if askpass_file and os.path.exists(askpass_file):
            try: os.remove(askpass_file)
            except OSError: pass

code = try_askpass()
if code is not None:
    sys.exit(code)

try:
    import pty, select, time
    cmd = ['ssh', '-p', str(port), '-o', 'StrictHostKeyChecking=no', '-o', 'UserKnownHostsFile=/dev/null', '-o', 'ConnectTimeout=4', '-o', 'LogLevel=ERROR', '-o', 'HostKeyAlgorithms=+ssh-rsa', '-o', 'PubkeyAcceptedKeyTypes=+ssh-rsa', '-o', 'KexAlgorithms=+diffie-hellman-group14-sha1,diffie-hellman-group-exchange-sha1', f'{u}@{ip}', 'echo SSH_TEST_OK']
    pid, fd = pty.fork()
    if pid == 0:
        os.execvp(cmd[0], cmd)
    else:
        buf = b''
        pass_sent = False
        start = time.time()
        while time.time() - start < 5.0:
            r, _, _ = select.select([fd], [], [], 0.2)
            if fd in r:
                try:
                    data = os.read(fd, 1024)
                    if not data: break
                    buf += data
                    if b'password:' in buf.lower() and not pass_sent:
                        time.sleep(0.1)
                        os.write(fd, password.encode() + b'\n')
                        pass_sent = True
                except OSError:
                    break
            if b'SSH_TEST_OK' in buf: break
        os.close(fd)
        _, status = os.waitpid(pid, 0)
        out = buf.decode(errors='replace').strip()
        if b'SSH_TEST_OK' in buf:
            print('SSH_TEST_OK')
            sys.exit(0)
        else:
            lines = [l for l in out.splitlines() if 'password:' not in l.lower()]
            msg = ' '.join(lines).strip()
            print(msg if msg else 'Permission denied, please try again.')
            sys.exit(1)
except Exception as e:
    print(str(e))
    sys.exit(1)
" "$u" "$ip" "$port" "$pass" 2>&1)
                ret=$?
            fi
        else
            if command -v ssh >/dev/null 2>&1; then
                ssh_out=$(ssh -p "$port" -o BatchMode=yes -o ConnectTimeout=4 -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR -o HostKeyAlgorithms=+ssh-rsa -o PubkeyAcceptedKeyTypes=+ssh-rsa -o KexAlgorithms=+diffie-hellman-group14-sha1,diffie-hellman-group-exchange-sha1 "$u@$ip" "echo SSH_TEST_OK" 2>&1)
                ret=$?
            else
                ret=1
                ssh_out="ssh command not found on host"
            fi
        fi
        set -e

        if [ $ret -eq 0 ] && [[ "$ssh_out" == *"SSH_TEST_OK"* ]]; then
            return 0
        else
            echo "$ssh_out"
            return 1
        fi
    }

    if [ "$INPUT_SSH" = "y" ] || [ "$INPUT_SSH" = "Y" ] || [ "$INPUT_SSH" = "yes" ]; then
        SAVED_SSH_ENABLED="True"
        while true; do
            read -p "    SSH 접속 계정 [기본값: $LIVE_SSH_USER]: " INPUT_SSH_USER
            CURRENT_SSH_USER=${INPUT_SSH_USER:-$LIVE_SSH_USER}
            read -p "    SSH 접속 포트 [기본값: $LIVE_SSH_PORT]: " INPUT_SSH_PORT
            CURRENT_SSH_PORT=${INPUT_SSH_PORT:-$LIVE_SSH_PORT}

            read -s -p "    SSH 접속 암호 (Enter: 키 기반 무암호 인증, 암호 사용 시 직접 입력): " INPUT_SSH_PASS
            echo ""
            CURRENT_SSH_PASS="$INPUT_SSH_PASS"

            echo -n "    [SSH] 원격 보드($CURRENT_SSH_USER@$TARGET_IP:$CURRENT_SSH_PORT) 접속 테스트 중... "
            set +e
            SSH_TEST_ERR=$(test_ssh_connection "$CURRENT_SSH_USER" "$TARGET_IP" "$CURRENT_SSH_PORT" "$CURRENT_SSH_PASS")
            SSH_TEST_CODE=$?
            set -e

            if [ $SSH_TEST_CODE -eq 0 ]; then
                echo "성공!"
                echo "    [SSH 접속 확인] $CURRENT_SSH_USER@$TARGET_IP:$CURRENT_SSH_PORT 인증 및 연결 확인 완료"
                break
            else
                echo "실패!"
                echo "    [오류] 원격 SSH 접속 실패: 암호 불일치, 키 인증 실패 또는 호스트 접근 불가"
                [ -n "$SSH_TEST_ERR" ] && echo "    (상세: $(echo "$SSH_TEST_ERR" | tr '\n' ' ' | head -c 120))"
                read -p "    SSH 접속 정보를 다시 입력하시겠습니까? (Y/n, 기본: Y): " RETRY_SSH
                RETRY_SSH=${RETRY_SSH:-Y}
                if [ "$RETRY_SSH" != "y" ] && [ "$RETRY_SSH" != "Y" ]; then
                    echo "    [주의] SSH 접속 실패 상태로 진행합니다. (관련 모니터링 TC는 BLOCKED 처리됩니다.)"
                    break
                fi
                echo ""
            fi
        done

        SSH_ARG="--ssh-enable --ssh-user $CURRENT_SSH_USER --ssh-port $CURRENT_SSH_PORT"
        if [ -n "$CURRENT_SSH_PASS" ]; then
            AUTH_DESC="암호 인증"
            SSH_ARG="$SSH_ARG --ssh-pass $CURRENT_SSH_PASS"
            export SSH_PASS="$CURRENT_SSH_PASS"
        else
            AUTH_DESC="키/무암호 인증"
        fi
        SSH_STATUS="활성화 ($CURRENT_SSH_USER@$TARGET_IP:$CURRENT_SSH_PORT, $AUTH_DESC - OS 자원 실시간 모니터링)"
    else
        CURRENT_SSH_USER="$LIVE_SSH_USER"
        CURRENT_SSH_PORT="$LIVE_SSH_PORT"
        CURRENT_SSH_PASS=""
        SSH_STATUS="비활성화 (BACnet 네트워크 응답 지표로 대체 감시)"
    fi

    # In Live mode, binaries are not executed on host, so binary flags & preflight are omitted
    RUN_ARGS="--suite $SUITE --mode live --gateway-config-dir $GATEWAY_CONFIG_DIR --vport-config-path $VPORT_CONFIG_PATH --tpi $TPI_PATH $PICS_ARG --ip $TARGET_IP --port $TARGET_PORT --device-id $DEVICE_ID --response-time-ms $EXPECTED_RESPONSE_TIME_MS $EXTRA_LIVE_ARGS $BBMD_ARG $LONG_ARG $ALLOW_BUILD_ARG $SSH_ARG $SOAK_ARG $CASE_ARG"
fi

# Summary
echo ""
echo "======================================================="
echo " [실행 구성 요약]"
echo " - Mode          : $MODE"
echo " - Suite         : $SUITE $CASE_ARG"
echo " - Target IP     : $TARGET_IP:$TARGET_PORT"
echo " - Response Time : ${EXPECTED_RESPONSE_TIME_MS} ms (Main 응답 지연 기준)"
if [ "$SUITE" = "soak" ]; then
    echo " - Soak Test     : 활성화 ($SOAK_HOURS 시간, $SOAK_INTERVAL 초 주기)"
else
    echo " - Long Tests    : $LONG_STATUS"
    if [ -n "$ALLOW_BUILD_ARG" ]; then
        echo " - Host Build    : $ALLOW_BUILD_STATUS"
    fi
    if [ "$MODE" = "managed" ]; then
        echo " - Gateway Bin   : $GATEWAY_BIN"
        echo " - vport Bin     : $VPORT_BIN"
        echo " - Gateway Config: $GATEWAY_CONFIG_DIR (로컬 PC)"
        echo " - vport Config  : $VPORT_CONFIG_PATH (로컬 PC)"
        echo " - BBMD          : $([ "$BBMD_ARG" = "--bbmd-auto" ] && echo '내장 BBMD 자동 구동 (UDP 47809)' || echo "$BBMD_ARG")"
    else
        echo " - Mutation      : $LIVE_MUTATION_STATUS"
        echo " - Gateway Config: $GATEWAY_CONFIG_DIR (로컬 PC)"
        echo " - vport Config  : $VPORT_CONFIG_PATH (로컬 PC)"
        echo " - BBMD          : $([ "$BBMD_ARG" = "--bbmd-auto" ] && echo '내장 BBMD 자동 구동 (UDP 47809)' || echo "$BBMD_ARG")"
    fi
fi
if [ "$MODE" = "live" ]; then
    echo " - Remote SSH    : $SSH_STATUS"
fi
echo " - TPI Path      : $TPI_PATH"
if [ -n "$PICS_ARG" ]; then
    echo " - PICS Doc      : $PICS_DOCX_PATH"
fi
echo "======================================================="
echo ""

# Save user selections for next run
save_config

ensure_docker_image

echo "[Docker] 컨테이너를 구동하여 테스트를 시작합니다..."
echo ""

SSH_VOL_OPT=""
if [ -d "$HOME/.ssh" ]; then
    SSH_VOL_OPT="-v $HOME/.ssh:/root/.ssh:ro -v $HOME/.ssh:$HOME/.ssh:ro"
fi
USER_MAP_OPT=""
if [ -f "/etc/passwd" ]; then
    USER_MAP_OPT="-v /etc/passwd:/etc/passwd:ro -v /etc/group:/etc/group:ro"
fi

# Bind-mount ES_ROOT at its real host path so absolute host paths remain valid inside container
EXTRA_VOL_OPTS="-v $ES_ROOT:$ES_ROOT"
add_extra_volume() {
    local p="$1"
    [ -z "$p" ] && return
    if [[ "$p" = /* ]]; then
        local target_dir="$p"
        [ -f "$target_dir" ] && target_dir="$(dirname "$target_dir")"
        if [ -d "$target_dir" ]; then
            if [[ "$target_dir" != "$ES_ROOT"* ]]; then
                EXTRA_VOL_OPTS="$EXTRA_VOL_OPTS -v $target_dir:$target_dir"
            fi
        fi
    fi
}
add_extra_volume "$GATEWAY_CONFIG_DIR"
add_extra_volume "$VPORT_CONFIG_PATH"
add_extra_volume "$TPI_PATH"
if [ -n "$PICS_ARG" ]; then
    add_extra_volume "$PICS_DOCX_PATH"
fi

docker run --rm -it --net=host --privileged \
    --user "$(id -u):$(id -g)" \
    -e PYTHONDONTWRITEBYTECODE=1 \
    -e SSH_PASS="$CURRENT_SSH_PASS" \
    -v "$ES_ROOT":/workspace \
    $EXTRA_VOL_OPTS \
    -w "/workspace/$PROJECT_DIR_NAME" \
    $SSH_VOL_OPT \
    $USER_MAP_OPT \
    "$DOCKER_IMAGE" \
    $RUN_ARGS
