// ============================================================================
// runtime/src/application/rt_application_port.cpp — RtApplicationPortInit()
//                                                    Wave 8 lane E
// ============================================================================
// B3R (the registry teardown, W1 RT-Core): every row the Application namespace
// registered either had no reader by name or by address, or lost its last
// reader when that reader was converted to a direct call (??1Application@0@
// and 0x18000a4f0: engine_lifecycle.cpp and engine/closure_batch2b.cpp call
// RtApp_Dtor / RtApp_SetDestroyTree by name). The message-callback and
// PlatformStartup register functions went with their rows. This entry point
// registers nothing; it stays defined only because
// standalone/resolve_standalone.cpp's RuntimePortInit() still calls it (the
// coordinator removes that call, then this file).
// ============================================================================

#include "../../funcs/rt_port.h"

void RtApplicationPortInit() {
}
