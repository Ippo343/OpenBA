#include "unit.h"
UNIT_TEST(harness_smoke) { CHECK_EQ(1 + 1, 2); CHECK_STREQ("a", "a"); }
