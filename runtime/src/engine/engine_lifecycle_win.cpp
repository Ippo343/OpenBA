// ============================================================================
// runtime/src/engine/engine_lifecycle_win.cpp — Wave 9 lane ENG, platform half
// ============================================================================
// PLAN.md §4.1 decision 1 keeps every non-`_win` file under runtime/src/ free
// of <windows.h>. `Engine::Engine::Startup` and `::Shutdown` make exactly three
// OS calls between them that are not already behind somebody else's seam:
//
//   Startup  0x1800dbc12  XOR EDX,EDX ; XOR ECX,ECX
//            0x1800dbc16  CALL qword ptr [0x18014b618]   ; ole32!CoInitializeEx
//   Shutdown 0x1800db88b  JMP  qword ptr [0x18014b610]   ; ole32!CoUninitialize
//   Shutdown 0x1800db5e8  CALL qword ptr [0x18014b118]   ; kernel32!FreeLibrary
//            0x1800db639  (and again, on the renderer plugin at 0x1801fc6d0)
//
// The three IAT slots were read out of Runtime-x64.dll's import directory with
// pefile, not guessed from a Ghidra label. `CoInitializeEx(nullptr, 0)` — both
// argument registers are zeroed, and 0 is COINIT_MULTITHREADED, NOT
// COINIT_APARTMENTTHREADED (which is 2); that distinction is load-bearing for
// DirectInput and for the shell dialogs Persistence opens.
//
// GetSystemInfo is NOT here: `Engine::Startup` reaches it only through the
// copy of `MemoryAllocator::Startup` that MSVC inlined into it, and
// runtime/src/system/allocator_win.cpp already owns that call.
// ============================================================================

#include <windows.h>

// Declared in engine_lifecycle.cpp. Not a general seam — this lane is the only
// caller and each call is one retail instruction's worth of behaviour.
long RtEngineLifecyclePlatformCoInit() {
    return static_cast<long>(CoInitializeEx(nullptr, 0));
}

void RtEngineLifecyclePlatformCoUninit() {
    CoUninitialize();
}

void RtEngineLifecyclePlatformFreeLibrary(void* module) {
    FreeLibrary(static_cast<HMODULE>(module));
}
