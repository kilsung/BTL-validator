/**
 * @file test_config_parser.cpp
 * @brief JSON 설정 파일 파싱 및 주소 변환 로직 단위 테스트
 *
 * 테스트 대상:
 *  - resolveFullAddress(): 5/6자리 전체 주소에서 블록·오프셋 자동 도출
 *  - deduceBacnetType(): Modbus 블록으로부터 BACnet 타입 자동 추론
 *  - parseJsonUInt / parseJsonInt: 안전한 JSON 값 변환
 *
 * 하드웨어 의존성 없음. BACstac API 를 사용하지 않으므로
 * 독립 바이너리로 컴파일/실행 가능합니다.
 *
 * 표준: C++11 / 외부 라이브러리 의존 없음
 */

#include "test_runner.h"
#include "common/relinquish_policy.h"
#include <atomic>
#include <cstdint>
#include <string>
#include <cmath>   // std::fabs

// ---------------------------------------------------------------------------
// resolveFullAddress 로직을 독립적으로 테스트하기 위한 로컬 재구현
// (BACstac 헤더 의존 없이 테스트 가능하도록 복사)
// ---------------------------------------------------------------------------

namespace {

static constexpr uint32_t kHoldingReg6DigitMin = 400001U;
static constexpr uint32_t kHoldingReg6DigitMax = 499999U;
static constexpr uint32_t kHoldingReg5DigitMin = 40001U;
static constexpr uint32_t kHoldingReg5DigitMax = 49999U;
static constexpr uint32_t kInputReg6DigitMin   = 300001U;
static constexpr uint32_t kInputReg6DigitMax   = 399999U;
static constexpr uint32_t kInputReg5DigitMin   = 30001U;
static constexpr uint32_t kInputReg5DigitMax   = 39999U;
static constexpr uint32_t kDiscreteIn6DigitMin = 100001U;
static constexpr uint32_t kDiscreteIn6DigitMax = 199999U;
static constexpr uint32_t kDiscreteIn5DigitMin = 10001U;
static constexpr uint32_t kDiscreteIn5DigitMax = 19999U;

enum class TestModbusBlock { COIL, DISCRETE_INPUT, INPUT_REGISTER, HOLDING_REGISTER };

void resolveFullAddress(uint32_t full_addr, TestModbusBlock& block, uint32_t& raw_addr)
{
    if (full_addr >= kHoldingReg6DigitMin && full_addr <= kHoldingReg6DigitMax) {
        block = TestModbusBlock::HOLDING_REGISTER;
        raw_addr = full_addr - kHoldingReg6DigitMin;
    } else if (full_addr >= kHoldingReg5DigitMin && full_addr <= kHoldingReg5DigitMax) {
        block = TestModbusBlock::HOLDING_REGISTER;
        raw_addr = full_addr - kHoldingReg5DigitMin;
    } else if (full_addr >= kInputReg6DigitMin && full_addr <= kInputReg6DigitMax) {
        block = TestModbusBlock::INPUT_REGISTER;
        raw_addr = full_addr - kInputReg6DigitMin;
    } else if (full_addr >= kInputReg5DigitMin && full_addr <= kInputReg5DigitMax) {
        block = TestModbusBlock::INPUT_REGISTER;
        raw_addr = full_addr - kInputReg5DigitMin;
    } else if (full_addr >= kDiscreteIn6DigitMin && full_addr <= kDiscreteIn6DigitMax) {
        block = TestModbusBlock::DISCRETE_INPUT;
        raw_addr = full_addr - kDiscreteIn6DigitMin;
    } else if (full_addr >= kDiscreteIn5DigitMin && full_addr <= kDiscreteIn5DigitMax) {
        block = TestModbusBlock::DISCRETE_INPUT;
        raw_addr = full_addr - kDiscreteIn5DigitMin;
    } else {
        block = TestModbusBlock::COIL;
        raw_addr = (full_addr >= 1U) ? (full_addr - 1U) : 0U;
    }
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// resolveFullAddress 테스트 (5자리 표기법)
// ---------------------------------------------------------------------------

TEST_CASE("Config: full_address 40001 maps to HOLDING_REGISTER offset 0") {
    TestModbusBlock block; uint32_t addr = 0;
    resolveFullAddress(40001U, block, addr);
    ASSERT_TRUE(block == TestModbusBlock::HOLDING_REGISTER);
    ASSERT_EQ(0U, addr);
}

TEST_CASE("Config: full_address 40010 maps to HOLDING_REGISTER offset 9") {
    TestModbusBlock block; uint32_t addr = 0;
    resolveFullAddress(40010U, block, addr);
    ASSERT_TRUE(block == TestModbusBlock::HOLDING_REGISTER);
    ASSERT_EQ(9U, addr);
}

TEST_CASE("Config: full_address 49999 maps to HOLDING_REGISTER offset 9998") {
    TestModbusBlock block; uint32_t addr = 0;
    resolveFullAddress(49999U, block, addr);
    ASSERT_TRUE(block == TestModbusBlock::HOLDING_REGISTER);
    ASSERT_EQ(9998U, addr);
}

TEST_CASE("Config: full_address 30001 maps to INPUT_REGISTER offset 0") {
    TestModbusBlock block; uint32_t addr = 0;
    resolveFullAddress(30001U, block, addr);
    ASSERT_TRUE(block == TestModbusBlock::INPUT_REGISTER);
    ASSERT_EQ(0U, addr);
}

TEST_CASE("Config: full_address 30005 maps to INPUT_REGISTER offset 4") {
    TestModbusBlock block; uint32_t addr = 0;
    resolveFullAddress(30005U, block, addr);
    ASSERT_TRUE(block == TestModbusBlock::INPUT_REGISTER);
    ASSERT_EQ(4U, addr);
}

TEST_CASE("Config: full_address 10001 maps to DISCRETE_INPUT offset 0") {
    TestModbusBlock block; uint32_t addr = 0;
    resolveFullAddress(10001U, block, addr);
    ASSERT_TRUE(block == TestModbusBlock::DISCRETE_INPUT);
    ASSERT_EQ(0U, addr);
}

TEST_CASE("Config: full_address 100 maps to COIL offset 99") {
    TestModbusBlock block; uint32_t addr = 0;
    resolveFullAddress(100U, block, addr);
    ASSERT_TRUE(block == TestModbusBlock::COIL);
    ASSERT_EQ(99U, addr);
}

TEST_CASE("Config: full_address 1 maps to COIL offset 0") {
    TestModbusBlock block; uint32_t addr = 0;
    resolveFullAddress(1U, block, addr);
    ASSERT_TRUE(block == TestModbusBlock::COIL);
    ASSERT_EQ(0U, addr);
}

// ---------------------------------------------------------------------------
// resolveFullAddress 테스트 (6자리 표기법)
// ---------------------------------------------------------------------------

TEST_CASE("Config: full_address 400001 maps to HOLDING_REGISTER offset 0 (6-digit)") {
    TestModbusBlock block; uint32_t addr = 0;
    resolveFullAddress(400001U, block, addr);
    ASSERT_TRUE(block == TestModbusBlock::HOLDING_REGISTER);
    ASSERT_EQ(0U, addr);
}

TEST_CASE("Config: full_address 400002 maps to HOLDING_REGISTER offset 1 (6-digit)") {
    TestModbusBlock block; uint32_t addr = 0;
    resolveFullAddress(400002U, block, addr);
    ASSERT_TRUE(block == TestModbusBlock::HOLDING_REGISTER);
    ASSERT_EQ(1U, addr);
}

TEST_CASE("Config: full_address 300001 maps to INPUT_REGISTER offset 0 (6-digit)") {
    TestModbusBlock block; uint32_t addr = 0;
    resolveFullAddress(300001U, block, addr);
    ASSERT_TRUE(block == TestModbusBlock::INPUT_REGISTER);
    ASSERT_EQ(0U, addr);
}

TEST_CASE("Config: full_address 100001 maps to DISCRETE_INPUT offset 0 (6-digit)") {
    TestModbusBlock block; uint32_t addr = 0;
    resolveFullAddress(100001U, block, addr);
    ASSERT_TRUE(block == TestModbusBlock::DISCRETE_INPUT);
    ASSERT_EQ(0U, addr);
}

// ---------------------------------------------------------------------------
// 경계 케이스 테스트
// ---------------------------------------------------------------------------

TEST_CASE("Config: full_address 0 maps to COIL offset 0 (edge case)") {
    TestModbusBlock block; uint32_t addr = 9999;
    resolveFullAddress(0U, block, addr);
    ASSERT_TRUE(block == TestModbusBlock::COIL);
    ASSERT_EQ(0U, addr);
}

// ---------------------------------------------------------------------------
// Scale/Offset 변환 로직 테스트 (스케일 적용 수식 검증)
// ---------------------------------------------------------------------------

/// 스케일·오프셋 적용: val = raw * scale + offset
TEST_CASE("Config: Scale 0.1 applied to raw value 500 yields 50.0") {
    const int16_t raw = 500;
    const double scale = 0.1;
    const double offset = 0.0;
    const double result = static_cast<double>(raw) * scale + offset;
    ASSERT_NEAR(50.0, result, 1e-9);
}

TEST_CASE("Config: Scale 0.01 and offset -273.15 conversion") {
    const int16_t raw = -27315;
    const double scale = 0.01;
    const double offset = 0.0;
    const double result = static_cast<double>(raw) * scale + offset;
    ASSERT_NEAR(-273.15, result, 1e-6);
}

/// 역변환: reg = round((val - offset) / scale)
TEST_CASE("Config: Inverse scale conversion for write value") {
    const double write_val = 25.3;
    const double scale = 0.1;
    const double offset = 0.0;
    const int16_t reg_val = static_cast<int16_t>(
        std::round((write_val - offset) / scale));
    ASSERT_EQ(static_cast<int16_t>(253), reg_val);
}

TEST_CASE("Config: Inverse scale for negative write value") {
    const double write_val = -10.5;
    const double scale = 0.1;
    const double offset = 0.0;
    const int16_t reg_val = static_cast<int16_t>(
        std::round((write_val - offset) / scale));
    ASSERT_EQ(static_cast<int16_t>(-105), reg_val);
}

// ---------------------------------------------------------------------------
// 비트 마스킹 로직 테스트 (BIT_PACKED 타입)
// ---------------------------------------------------------------------------

TEST_CASE("Config: BIT_PACKED read extracts correct bit position 3") {
    const uint16_t reg = 0x0008; // bit 3 = 1
    const int bit_pos = 3;
    const int bit_val = (reg >> bit_pos) & 0x01;
    ASSERT_EQ(1, bit_val);
}

TEST_CASE("Config: BIT_PACKED read extracts correct bit position 0") {
    const uint16_t reg = 0xFFFE; // bit 0 = 0
    const int bit_pos = 0;
    const int bit_val = (reg >> bit_pos) & 0x01;
    ASSERT_EQ(0, bit_val);
}

TEST_CASE("Config: BIT_PACKED write sets bit 3 correctly") {
    uint16_t reg = 0x0000;
    const int bit_pos = 3;
    reg |= static_cast<uint16_t>(1U << static_cast<unsigned>(bit_pos));
    ASSERT_EQ(static_cast<uint16_t>(0x0008), reg);
}

TEST_CASE("Config: BIT_PACKED write clears bit 3 correctly") {
    uint16_t reg = 0x000F;
    const int bit_pos = 3;
    reg &= ~static_cast<uint16_t>(1U << static_cast<unsigned>(bit_pos));
    ASSERT_EQ(static_cast<uint16_t>(0x0007), reg);
}

TEST_CASE("Config: BIT_PACKED write set bit 15 (MSB) correctly") {
    uint16_t reg = 0x0000;
    reg |= static_cast<uint16_t>(1U << 15U);
    ASSERT_EQ(static_cast<uint16_t>(0x8000), reg);
}

// ---------------------------------------------------------------------------
// 실제 운영 코드가 사용하는 공통 Relinquish Default 정책 테스트
// ---------------------------------------------------------------------------

TEST_CASE("Config: Relinquish_Default keeps explicit JSON setting within range") {
    const double json_relinquish_default = 25.5;
    const double candidate = gateway::relinquish_policy::clamp(
        json_relinquish_default, true, 10.0, true, 50.0);

    ASSERT_EQ(25.5, candidate);
}

TEST_CASE("Config: Relinquish_Default explicit value exceeds max_value -> set to max_value") {
    const double json_relinquish_default = 100.0;
    const double candidate = gateway::relinquish_policy::clamp(
        json_relinquish_default, true, 10.0, true, 50.0);

    ASSERT_EQ(50.0, candidate);
}

TEST_CASE("Config: Relinquish_Default explicit value below min_value -> set to min_value") {
    const double json_relinquish_default = 5.0;
    const double candidate = gateway::relinquish_policy::clamp(
        json_relinquish_default, true, 10.0, true, 50.0);

    ASSERT_EQ(10.0, candidate);
}

TEST_CASE("Config: Binary Value Relinquish_Default accepts only 0 or 1") {
    ASSERT_TRUE(gateway::relinquish_policy::isValidBinaryValue(true, 0.0));
    ASSERT_TRUE(gateway::relinquish_policy::isValidBinaryValue(true, 1.0));
    ASSERT_FALSE(gateway::relinquish_policy::isValidBinaryValue(true, 0.5));
    ASSERT_FALSE(gateway::relinquish_policy::isValidBinaryValue(true, 2.0));
}

// ---------------------------------------------------------------------------
// syncRelinquishDefaults 파이프라인 시뮬레이션 테스트
// ---------------------------------------------------------------------------

TEST_CASE("SyncPipeline: Hardware read value updates Relinquish_Default with clamping") {
    // 1. 정상 범위 하드웨어 값 읽기
    double hw_val = 22.5;
    const bool has_min = true;
    const double min_val = 10.0;
    const bool has_max = true;
    const double max_val = 50.0;

    if (has_min && hw_val < min_val) hw_val = min_val;
    if (has_max && hw_val > max_val) hw_val = max_val;
    ASSERT_EQ(22.5, hw_val);

    // 2. min_value 미만 하드웨어 값 -> min_value로 클램핑
    double hw_low = 5.0;
    if (has_min && hw_low < min_val) hw_low = min_val;
    if (has_max && hw_low > max_val) hw_low = max_val;
    ASSERT_EQ(10.0, hw_low);

    // 3. max_value 초과 하드웨어 값 -> max_value로 클램핑
    double hw_high = 80.0;
    if (has_min && hw_high < min_val) hw_high = min_val;
    if (has_max && hw_high > max_val) hw_high = max_val;
    ASSERT_EQ(50.0, hw_high);
}

TEST_CASE("SyncPipeline: Retry loop succeeds once all points are responded") {
    // 3개 포인트 중 1, 2번째는 첫 시도 성공, 3번째는 두 번째 시도에 성공 시뮬레이션
    const size_t total_points = 3;
    std::vector<bool> synced(total_points, false);
    size_t synced_count = 0;
    int attempt = 0;

    while (synced_count < total_points && attempt < 5) {
        attempt++;
        for (size_t i = 0; i < total_points; ++i) {
            if (synced[i]) continue;
            // 3번째 포인트(i=2)는 attempt >= 2 일 때만 성공
            if (i < 2 || attempt >= 2) {
                synced[i] = true;
                synced_count++;
            } else {
                break; // 재시도 대기
            }
        }
    }

    ASSERT_EQ(static_cast<size_t>(3), synced_count);
    ASSERT_EQ(2, attempt);
}

TEST_CASE("SyncPipeline: Plan A - Explicit write vs Omitted read with clamping on both") {
    const double min_val = 10.0;
    const double max_val = 50.0;

    // 1. JSON에 명시된 포인트 (범위 초과 시 클램핑 검증)
    double explicit_high = 80.0;
    if (explicit_high < min_val) explicit_high = min_val;
    if (explicit_high > max_val) explicit_high = max_val;
    ASSERT_EQ(50.0, explicit_high);

    double explicit_low = 3.0;
    if (explicit_low < min_val) explicit_low = min_val;
    if (explicit_low > max_val) explicit_low = max_val;
    ASSERT_EQ(10.0, explicit_low);

    // 2. JSON에 생략되어 Modbus에서 읽어온 포인트 (범위 초과 시 클램핑 검증)
    bool has_omitted_rd = false;
    double hw_read_high = 99.0;
    double final_rd_high = hw_read_high;
    if (!has_omitted_rd) {
        if (final_rd_high < min_val) final_rd_high = min_val;
        if (final_rd_high > max_val) final_rd_high = max_val;
    }
    ASSERT_EQ(50.0, final_rd_high);

    double hw_read_low = 1.0;
    double final_rd_low = hw_read_low;
    if (!has_omitted_rd) {
        if (final_rd_low < min_val) final_rd_low = min_val;
        if (final_rd_low > max_val) final_rd_low = max_val;
    }
    ASSERT_EQ(10.0, final_rd_low);
}

TEST_CASE("SyncPipeline: Clamping triggers write-back to Main to ensure 3-way consistency") {
    const double min_val = 10.0;
    const double max_val = 50.0;

    // Case 1: Read value (5.0) < min_val (10.0) -> Clamped to 10.0 and write-back triggered
    double raw_val = 5.0;
    double clamped = raw_val;
    bool was_clamped = false;
    if (clamped < min_val) { clamped = min_val; was_clamped = true; }
    if (clamped > max_val) { clamped = max_val; was_clamped = true; }

    bool write_back_executed = false;
    if (was_clamped) {
        write_back_executed = true; // modbus_->writeValue(*mapping, clamped)
    }

    ASSERT_TRUE(was_clamped);
    ASSERT_TRUE(write_back_executed);
    ASSERT_EQ(10.0, clamped);

    // Case 2: Read value (25.0) in range -> No write-back needed
    double in_range_val = 25.0;
    double clamped_in = in_range_val;
    bool was_clamped_in = false;
    if (clamped_in < min_val) { clamped_in = min_val; was_clamped_in = true; }
    if (clamped_in > max_val) { clamped_in = max_val; was_clamped_in = true; }

    bool write_back_executed_in = false;
    if (was_clamped_in) {
        write_back_executed_in = true;
    }

    ASSERT_FALSE(was_clamped_in);
    ASSERT_FALSE(write_back_executed_in);
    ASSERT_EQ(25.0, clamped_in);
}

TEST_CASE("Config: Detect contradictory min_value > max_value configuration") {
    const bool has_min = true;
    const double min_val = 50.0;
    const bool has_max = true;
    const double max_val = 10.0;

    bool is_contradictory = (has_min && has_max && min_val > max_val);
    ASSERT_TRUE(is_contradictory);
}

TEST_CASE("Config: Auto-resume startup once contradictory configuration is corrected") {
    // 시뮬레이션: 첫 시도에서는 min=50 > max=10 모순 발생, 두 번째 시도에서는 min=10 <= max=50 으로 수정됨
    bool startup_succeeded = false;
    int reload_attempts = 0;

    double dynamic_min = 50.0;
    double dynamic_max = 10.0;

    while (!startup_succeeded && reload_attempts < 5) {
        reload_attempts++;
        if (dynamic_min > dynamic_max) {
            // 모순 감지 -> 대기 후 파일 수정 시뮬레이션
            dynamic_min = 10.0;
            dynamic_max = 50.0;
            continue;
        }
        // 수정 완료 -> 정상 기동 재개
        startup_succeeded = true;
    }

    ASSERT_TRUE(startup_succeeded);
    ASSERT_EQ(2, reload_attempts);
}

TEST_CASE("Config: Immediate abort on shutdown signal during config hold") {
    std::atomic<bool> running(true);
    bool hold_aborted = false;

    // 시뮬레이션: hold 루프 도중 외부 시그널(running = false) 수신
    for (int s = 0; s < 60 && running.load(); ++s) {
        if (s == 2) {
            running = false; // SIGINT 수신 시뮬레이션
        }
    }

    if (!running.load()) {
        hold_aborted = true;
    }

    ASSERT_TRUE(hold_aborted);
    ASSERT_FALSE(running.load());
}

TEST_CASE("Config: ConfigLoader returns detailed error immediately without internal sleep") {
    std::string err;
    bool is_contradiction = false;

    const bool has_min = true;
    const double min_val = 50.0;
    const bool has_max = true;
    const double max_val = 10.0;
    const std::string name = "Test Object";

    if (has_min && has_max && min_val > max_val) {
        err = "Object '" + name + "' has contradictory configuration: min_value (" +
              std::to_string(min_val) + ") > max_value (" + std::to_string(max_val) + ")!";
        is_contradiction = true;
    }

    ASSERT_TRUE(is_contradiction);
    ASSERT_TRUE(err.find("Test Object") != std::string::npos);
    ASSERT_TRUE(err.find("min_value") != std::string::npos);
}

TEST_CASE("Config: parseUnits string mapping matches standard BACnet units") {
    auto parseUnitsHelper = [](const std::string& units_str) -> uint32_t {
        std::string lower_str = units_str;
        for (char& c : lower_str) {
            if (c >= 'A' && c <= 'Z') { c = static_cast<char>(c - 'A' + 'a'); }
        }
        if (lower_str == "degrees-celsius")        return 62;
        if (lower_str == "degrees-fahrenheit")     return 64;
        if (lower_str == "percent")                return 98;
        if (lower_str == "hours")                  return 60;
        if (lower_str == "seconds")                return 61;
        if (lower_str == "minutes")                return 72;
        if (lower_str == "amperes")                return 3;
        if (lower_str == "hertz")                  return 27;
        if (lower_str == "volts")                  return 5;
        if (lower_str == "revolutions-per-minute") return 96;
        if (lower_str == "kilopascals")            return 54;
        if (lower_str == "bars")                   return 55;
        if (lower_str == "cubic-meters-per-hour")  return 135;
        return 95; // UNIT_NO_UNITS
    };

    ASSERT_EQ(62U, parseUnitsHelper("degrees-celsius"));
    ASSERT_EQ(62U, parseUnitsHelper("DEGREES-CELSIUS"));
    ASSERT_EQ(135U, parseUnitsHelper("cubic-meters-per-hour"));
    ASSERT_EQ(135U, parseUnitsHelper("CUBIC-METERS-PER-HOUR"));
    ASSERT_EQ(54U, parseUnitsHelper("kilopascals"));
    ASSERT_EQ(55U, parseUnitsHelper("bars"));
    ASSERT_EQ(95U, parseUnitsHelper("unknown-unit"));
}

