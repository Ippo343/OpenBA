#pragma once
// ============================================================================
// src/platform/common/platform_os.h -- process-level OS services (Phase D.4,
// THREADS seam, E.7)
// ============================================================================
// D4_RULES C1: namespace `Platform`, free functions, NO OS header. Windows bodies:
// src/platform/win/os_win.cpp, one call each.
// ============================================================================

#include <cstdint>

namespace Platform {

// COM, for the Windows build's COM consumers (XAudio2, DirectInput 8, ShellExecuteW;
// plans/THREADS.md A.4). Engine::Startup 0x1800dbc12 calls CoInitializeEx(NULL, 0):
// 0 is COINIT_MULTITHREADED, not COINIT_APARTMENTTHREADED (2). The result is the
// HRESULT (0 S_OK, 1 S_FALSE, negative on failure); retail discards it. A POSIX
// impl has no COM: ComInit returns 0 and ComUninit does nothing. The SDL seam drops
// both calls once it has removed the three consumers.
int32_t ComInit();      // CoInitializeEx(NULL, COINIT_MULTITHREADED)
void    ComUninit();    // CoUninitialize: Engine::Shutdown's tail jump 0x1800db88b

// A diagnostic probe, not retail behaviour: true when the page holding `p` is
// committed and neither no-access nor a guard page (Windows: VirtualQuery succeeds,
// State == MEM_COMMIT, Protect has neither PAGE_NOACCESS nor PAGE_GUARD). Used by
// the serializer desync trace in game/Entity/componententity.cpp before it reads a
// dword through a candidate buffer pointer.
bool IsReadablePage(const void* p);

}  // namespace Platform
