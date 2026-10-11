// ============================================================================
// src/platform/win/os_win.cpp -- Platform:: process-level services over Win32
// ============================================================================
// The Windows half of src/platform/common/platform_os.h. ComInit / ComUninit were
// RtEngineLifecyclePlatformCoInit / CoUninit in engine_lifecycle_win.cpp until
// Phase D.4 THREADS E.7; the calls and their arguments are unchanged.
// ============================================================================

#include "platform_os.h"

#include <windows.h>

namespace Platform {

int32_t ComInit()   { return CoInitializeEx(nullptr, COINIT_MULTITHREADED); }
void    ComUninit() { CoUninitialize(); }

bool IsReadablePage(const void* p) {
    MEMORY_BASIC_INFORMATION mbi;
    return VirtualQuery(p, &mbi, sizeof(mbi)) &&
           mbi.State == MEM_COMMIT &&
           !(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD));
}

}  // namespace Platform
