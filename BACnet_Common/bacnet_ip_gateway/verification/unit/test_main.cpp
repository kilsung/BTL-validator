/**
 * @file test_main.cpp
 * @brief 단위 테스트 진입점 (main 함수)
 *
 * 이 파일은 test_runner.h 의 RUN_ALL_TESTS() 매크로를 호출하여
 * 모든 TEST_CASE 등록 함수를 실행합니다.
 *
 * 빌드:
 *   g++ -std=c++11 -Wall -I. \
 *       test_main.cpp \
 *       test_modbus_crc.cpp \
 *       test_modbus_frame_builder.cpp \
 *       test_mock_serial_port.cpp \
 *       test_config_parser.cpp \
 *       ../src/common/modbus_crc.cpp \
 *       -o run_tests && ./run_tests
 *
 * 반환값:
 *   0: 모든 테스트 통과
 *   1: 하나 이상의 테스트 실패
 *
 * 라이선스 준수:
 *  - 외부 라이브러리 의존 없음. C++11 STL 만 사용.
 *
 * 표준: C++11
 */

#include "test_runner.h"

int main(int /*argc*/, char* /*argv*/[])
{
    return RUN_ALL_TESTS();
}
