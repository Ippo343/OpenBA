#pragma once
// Minimal test harness for tests/unit (ENDGAME B). No dependencies.
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace unit {
struct Test { const char* name; void (*fn)(); Test* next; };
inline Test*& Head() { static Test* head = nullptr; return head; }
inline int& Failures() { static int n = 0; return n; }
struct Register { Register(Test* t) { t->next = Head(); Head() = t; } };
}  // namespace unit

#define UNIT_TEST(Name) \
    static void Name(); \
    static unit::Test Name##_test{#Name, &Name, nullptr}; \
    static unit::Register Name##_reg(&Name##_test); \
    static void Name()

#define CHECK(cond) do { if (!(cond)) { \
    std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++unit::Failures(); } } while (0)
#define CHECK_EQ(a, b) do { auto _a = (a); auto _b = (b); if (!(_a == _b)) { \
    std::printf("  FAIL %s:%d: %s == %s\n", __FILE__, __LINE__, #a, #b); ++unit::Failures(); } } while (0)
#define CHECK_STREQ(a, b) do { const char* _a = (a); const char* _b = (b); \
    if (!((_a == nullptr && _b == nullptr) || (_a && _b && std::strcmp(_a, _b) == 0))) { \
    std::printf("  FAIL %s:%d: %s == %s (\"%s\" vs \"%s\")\n", __FILE__, __LINE__, #a, #b, \
        _a ? _a : "(null)", _b ? _b : "(null)"); ++unit::Failures(); } } while (0)
