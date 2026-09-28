// ============================================================================
// runtime/src/achievement/rt_achievement_port.cpp — RtAchievementPortInit()
// ============================================================================
// B3R (the registry teardown, W1 RT-Core): every row the Achievement namespace
// registered had no reader by name or by address, so the six register
// functions went with their rows. This entry point registers nothing; it stays
// defined only because standalone/resolve_standalone.cpp's RuntimePortInit()
// still calls it (the coordinator removes that call, then this file).
// ============================================================================

#include "../../funcs/rt_port.h"

void RtAchievementPortInit() {
}
