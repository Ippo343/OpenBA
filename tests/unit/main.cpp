// tests/unit/main.cpp -- runs every UNIT_TEST in link order. Exit code = failures.
#include "unit.h"

int main() {
    int tests = 0;
    for (unit::Test* t = unit::Head(); t; t = t->next) {
        int before = unit::Failures();
        t->fn();
        ++tests;
        std::printf("%s %s\n", unit::Failures() == before ? "ok  " : "FAIL", t->name);
    }
    std::printf("%d tests, %d failures\n", tests, unit::Failures());
    return unit::Failures() ? 1 : 0;
}
