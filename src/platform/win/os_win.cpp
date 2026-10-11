// ============================================================================
// src/platform/win/os_win.cpp -- Platform:: process-level services over Win32
// ============================================================================
// The Windows half of src/platform/common/platform_os.h. ComInit / ComUninit (THREADS
// E.7) were removed by Phase D.4 SDL-5 D2 with their callers.
// ============================================================================

#include "platform_os.h"

#include <windows.h>

namespace Platform {

bool IsReadablePage(const void* p) {
    MEMORY_BASIC_INFORMATION mbi;
    return VirtualQuery(p, &mbi, sizeof(mbi)) &&
           mbi.State == MEM_COMMIT &&
           !(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD));
}

}  // namespace Platform
