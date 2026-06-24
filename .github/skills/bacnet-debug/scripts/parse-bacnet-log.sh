#!/bin/bash
# BACnet 서비스 로그에서 오브젝트 생성 실패, Luna bus 에러 등을 추출하는 스크립트
# 사용법: ./parse-bacnet-log.sh < bacnet.log
#         또는 journalctl -u com.acp.bacnet | ./parse-bacnet-log.sh

set -euo pipefail

echo "=== BACnet 로그 파서 ==="
echo ""

# stdin으로 로그를 받음
INPUT=$(cat)

# 1. BACstac 오브젝트 생성 실패
echo "--- BACstac 오브젝트 생성 실패 ---"
echo "$INPUT" | grep -iE "create.*fail|CreateDBObject.*fail|object.*error" || echo "(없음)"
echo ""

# 2. Luna bus 에러
echo "--- Luna Bus 에러 ---"
echo "$INPUT" | grep -iE "LSCall.*fail|luna.*error|service.*not.*available" || echo "(없음)"
echo ""

# 3. cniUpdated 관련
echo "--- cniUpdated 이벤트 ---"
echo "$INPUT" | grep -iE "cni.*update|cni.*signal|subscribe.*cni" || echo "(없음)"
echo ""

# 4. 메모리/초기화 에러
echo "--- 메모리/초기화 에러 ---"
echo "$INPUT" | grep -iE "malloc.*fail|alloc.*fail|init.*fail|NULL" || echo "(없음)"
echo ""

# 5. 프로퍼티 관련
echo "--- 프로퍼티 경고 ---"
echo "$INPUT" | grep -iE "property.*miss|prop.*not found|PropList|InitandAlloc" || echo "(없음)"
echo ""

echo "=== 파싱 완료 ==="
