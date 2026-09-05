// standalone/reg_w2_screenshot.cpp
//
// Wave-2 lane 4: registers GameUpdateLoop::TakeScreenshot (exe 0x140095da0).
//
// This is the address behind the live quicksave/quickload EXEC-of-0 documented
// in docs/waves/code_fns_wave.md. `code/Game/Game/gameupdateloop.cpp:527`
// builds `fn_QuickSave` from `ResolveAddr(Module::Exe, 0x140095da0)` and calls
// it from Draw at :2061 (m_quickSavePressed) and :2136 (m_quickLoadPressed).
// Until now that resolve returned 0.
//
// The port is code/Game/Game/w2_takescreenshot.cpp; its ABI is void(void*),
// identical to retail's `void f(GameUpdateLoop*)` and to fn_QuickSave_t.
//
// Integrator: call RegisterW2Screenshot() from RegisterCodeFunctions() in
// standalone/address_registry.cpp, and delete the now-dead else-branches of the
// two `if (fn_QuickSave)` guards in gameupdateloop.cpp (they are self-retiring:
// with this registered, fn_QuickSave is non-null and the fallback never runs).

#include "resolve.h"
#include <cstdint>

extern "C" void GameUpdateLoop_TakeScreenshot(void* self);   // 0x140095da0

extern "C" void RegisterW2Screenshot() {
    RegisterExeAddr(0x140095da0, reinterpret_cast<uintptr_t>(&GameUpdateLoop_TakeScreenshot));
}
