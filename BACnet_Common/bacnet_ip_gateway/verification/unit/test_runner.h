/**
 * @file test_runner.h
 * @brief 외부 테스트 프레임워크 없는 순수 C++11 단위 테스트 러너
 *
 * 라이선스 준수:
 *  - 외부 라이브러리(GTest 등) 의존 없음.
 *  - C++11 STL 만 사용합니다.
 *
 * 설계:
 *  - TEST_CASE 매크로는 함수명 충돌 방지를 위해 __COUNTER__ 를 사용합니다.
 *    (같은 번호의 __LINE__ 이 다른 파일에서 충돌하는 문제 해결)
 *  - 전역 레지스트리는 함수 내 static 변수로 초기화 순서 문제를 회피합니다.
 *
 * 표준: C++11
 */

#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <cmath>
#include <stdexcept>

// ---------------------------------------------------------------------------
// 테스트 레지스트리
// ---------------------------------------------------------------------------

namespace test_framework {

struct TestCase {
    std::string            name;
    std::function<void()>  fn;
};

/// 전역 테스트 케이스 목록 (함수 내 static 으로 초기화 순서 문제 회피)
inline std::vector<TestCase>& getRegistry()
{
    static std::vector<TestCase> registry;
    return registry;
}

struct Registrar {
    Registrar(const std::string& name, std::function<void()> fn)
    {
        getRegistry().push_back({name, std::move(fn)});
    }
};

} // namespace test_framework

// ---------------------------------------------------------------------------
// 매크로
// ---------------------------------------------------------------------------

// __COUNTER__ : 파일과 무관하게 전처리기 전역으로 단조 증가하는 카운터
// → 여러 .cpp 파일을 같은 링크 유닛에 포함해도 심볼 충돌 없음

#define DETAIL_TEST_FN(counter)   test_fn_detail_##counter
#define DETAIL_TEST_REG(counter)  test_reg_detail_##counter
#define DETAIL_EXPAND(counter, name)                                      \
    static void DETAIL_TEST_FN(counter)();                                \
    static ::test_framework::Registrar DETAIL_TEST_REG(counter)(          \
        name, DETAIL_TEST_FN(counter));                                   \
    static void DETAIL_TEST_FN(counter)()

/// 테스트 케이스를 등록하는 매크로
#define TEST_CASE(test_name) DETAIL_EXPAND(__COUNTER__, test_name)

/// 조건이 참인지 검사
#define ASSERT_TRUE(cond)                                                 \
    do {                                                                  \
        if (!(cond)) {                                                    \
            throw std::runtime_error(                                     \
                std::string(__FILE__) + ":" + std::to_string(__LINE__) + \
                ": ASSERT_TRUE failed: " #cond);                         \
        }                                                                 \
    } while (false)

/// 조건이 거짓인지 검사
#define ASSERT_FALSE(cond) ASSERT_TRUE(!(cond))

/// 두 값이 같은지 검사
#define ASSERT_EQ(expected, actual)                                       \
    do {                                                                  \
        if (!((expected) == (actual))) {                                  \
            throw std::runtime_error(                                     \
                std::string(__FILE__) + ":" + std::to_string(__LINE__) + \
                ": ASSERT_EQ failed: " #expected " != " #actual);        \
        }                                                                 \
    } while (false)

/// 두 값이 다른지 검사
#define ASSERT_NE(a, b)                                                   \
    do {                                                                  \
        if ((a) == (b)) {                                                 \
            throw std::runtime_error(                                     \
                std::string(__FILE__) + ":" + std::to_string(__LINE__) + \
                ": ASSERT_NE failed: " #a " == " #b);                    \
        }                                                                 \
    } while (false)

/// 부동소수점 비교 (절대 허용 오차 epsilon)
#define ASSERT_NEAR(expected, actual, epsilon)                            \
    do {                                                                  \
        if (std::fabs(static_cast<double>(expected) -                    \
                      static_cast<double>(actual)) > (epsilon))          \
        {                                                                 \
            throw std::runtime_error(                                     \
                std::string(__FILE__) + ":" + std::to_string(__LINE__) + \
                ": ASSERT_NEAR failed: |" #expected " - " #actual        \
                "| > " #epsilon);                                         \
        }                                                                 \
    } while (false)

/// 모든 등록된 테스트를 실행하고 요약을 출력합니다.
#define RUN_ALL_TESTS()                                                   \
    ([]() -> int {                                                        \
        auto& registry = ::test_framework::getRegistry();                 \
        int passed = 0, failed = 0;                                       \
        std::cout << "\n====== Unit Test Runner ======\n";                \
        for (auto& tc : registry) {                                       \
            std::cout << "  [ RUN ] " << tc.name << "\n         ";       \
            try {                                                          \
                tc.fn();                                                  \
                std::cout << "PASS\n";                                    \
                ++passed;                                                 \
            } catch (const std::exception& ex) {                         \
                std::cout << "FAIL\n";                                    \
                std::cerr << "    >> " << ex.what() << "\n";             \
                ++failed;                                                 \
            }                                                             \
        }                                                                 \
        std::cout << "=============================\n";                   \
        std::cout << "  Passed: " << passed                               \
                  << "  Failed: " << failed                               \
                  << "  Total: " << (passed + failed) << "\n";           \
        std::cout << "=============================\n\n";                 \
        return (failed == 0) ? 0 : 1;                                    \
    }())
