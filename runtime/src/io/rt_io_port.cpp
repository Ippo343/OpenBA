// ============================================================================
// runtime/src/io/rt_io_port.cpp — RtIoPortInit()  (Wave 4)
// ============================================================================
// B3R W2-RT (registry teardown): every IO row had no reader by name or by
// address, so the sixteen IO register functions went with their rows. This
// registers nothing and stays defined only because
// standalone/resolve_standalone.cpp's RuntimePortInit() still calls it (the
// coordinator removes that call, then this file).
// ============================================================================
#include "../../funcs/rt_port.h"

void RtIoPortInit() {
}
