// ============================================================================
// tools/endsprites_probe/seam_stub.cpp — WAVE 23 LANE TAIL
//
// The minimum seam that lets the REAL wave files (runtime/src/ui/spritecapture.cpp
// and runtime/src/ui/sprite.cpp) link into a standalone sweep exe.
//
// THE POINT OF THE STUBS IS THAT THEY ARE INERT. `RegisterRuntimeExport` and
// friends are no-ops here, so the port registry does not exist in this process
// and there is NO WAY for the sweep's "theirs" side to come back as our own
// body. The sweep resolves retail strictly through GetProcAddress on the
// shipped export table. That is what makes this an oracle rather than the
// self-check that left a Wave 5 probe dark for five waves
// (reference_registering_an_address_converts_a_selfcheck_into_an_oracle.md).
//
// `ResolveExportRaw` / `RtResolveCached` DO go to the real DLL, because the
// ported bodies legitimately call MemAlloc/MemFree through them on paths the
// sweep does not exercise. If one of those paths were ever reached with no
// allocator singleton the call would fault loudly rather than silently
// returning a wrong answer, which is the behaviour we want here.
// ============================================================================

#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {
HMODULE RtModule() {
    static HMODULE h = LoadLibraryA("C:/GOG Games/Banished/Runtime-x64.dll");
    return h;
}
}  // namespace

// NOTE: rt_port.h declares these with C++ LINKAGE, not extern "C". The first
// draft of this file wrapped them in extern "C" and every one came back as an
// undefined reference -- the mangling is part of the contract.

void RegisterRuntimeExport(const char*, void*) {}
void RegisterRuntimeAddr(uintptr_t, void*) {}
void RegisterRuntimeOrdinal(uint16_t, void*) {}

void* ResolveExportRaw(const char* name) {
    HMODULE h = RtModule();
    return h ? reinterpret_cast<void*>(GetProcAddress(h, name)) : nullptr;
}

// In this process the registry is empty, so the port-aware door and the raw
// door are the same thing by construction. Said here rather than left implicit.
void* ResolveExport(const char* name) { return ResolveExportRaw(name); }

void* RtResolveCached(const char* mangled, void** cache) {
    if (cache && *cache) return *cache;
    void* p = ResolveExportRaw(mangled);
    if (p && cache) *cache = p;
    return p;
}

uint64_t RtResolveMissCount() { return 0; }
void     RtResolveMissSummary() {}
int      RtResolveSlotAudit() { return 0; }
uint64_t RtResolvePreInitHitCount() { return 0; }
uint64_t RtResolvePreInitFrozenCount() { return 0; }
void     RtResolveMarkPortInitDone() {}
bool     RtResolvePortInitDone() { return true; }
void     RtResolveNoteEarly(const char*, uintptr_t, uint16_t, void*) {}
uint64_t RtResolveEarlyCount() { return 0; }
void     RtResolveEarlySummary() {}
bool     RuntimeExportIsRegistered(const char*) { return false; }

// The two probe/registration hooks the wave files reference but that this
// harness has no use for. Inert on purpose.
int  RtUiSpriteSheetVtableRegister() { return 0; }
void RtUiSpriteCaptureProbeArm() {}
void RtUiSpriteCaptureProbe() {}
void RtUiSpriteCaptureProbeReport() {}

// The address seam. `Module::Runtime` is the only module this harness knows
// about, and it maps a Ghidra address to the loaded image by delta.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };

uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr) {
    if (mod != Module::Runtime) return 0;
    HMODULE h = RtModule();
    if (!h) return 0;
    return reinterpret_cast<uintptr_t>(h) + (ghidra_addr - 0x180000000ull);
}

uintptr_t ResolveRuntimeRaw(uintptr_t ghidra_addr) {
    return ResolveAddr(Module::Runtime, ghidra_addr);
}

// sprite.cpp's aggregator pulls in the SpriteContext wave; not linked here.
int RtUiSpriteContextRegister() { return 0; }
