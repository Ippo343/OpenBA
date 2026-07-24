// See dx11_tripwire.h for what this measures and what the measurement does
// and does not cover.

#include "dx11_tripwire.h"

#include "dx11_seam.h"
#include "resolve.h"

#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace VideoDX11 {
namespace {

// From `objdump -h VideoDX11-x64.dll`:
//   .text  size 00011964  VMA 0000000180001000
// Used only as a cross-check against the section table read out of the loaded
// image, in the same spirit as the seam's 84-slot verification: if the shipped
// DLL is not the build we mapped, say so instead of protecting a wrong range.
constexpr uintptr_t kExpectedTextRva  = 0x1000;
constexpr size_t    kExpectedTextSize = 0x11964;

uint8_t* g_textBase = nullptr;
size_t   g_textSize = 0;

bool g_enabled  = false;
bool g_resolved = false;
bool g_armed    = false;
bool g_broken   = false;   // a VirtualProtect failed; stop trying

PVOID g_veh = nullptr;

// Distinct faulting addresses. Small and fixed: if this ever overflows, the
// "no shipped code runs" claim has already failed comprehensively.
constexpr int kMaxHits = 32;
struct Hit {
    uintptr_t rip;         // faulting instruction (runtime address)
    uintptr_t ghidra;      // ... translated back to the Ghidra image
    uintptr_t caller;      // [RSP] at fault: the return address IF the fault was
                           // on a function's first instruction. Heuristic.
    unsigned  count;
    const char* reason;    // non-null => known loader/CRT plumbing, not a finding
};
Hit g_hits[kMaxHits];
int g_hitCount = 0;

// Detour trampolines are our bytes at a DLL address. Counted, but separately —
// they are expected, and reporting them as findings would bury a real one.
unsigned g_stubHits = 0;

// The DLL is not all renderer. decomp/videodx11.md partitions its 576 functions
// into the 85 that implement the plugin, at 0x180001000-0x180005b7f, and the
// 491 that are the MSVC CRT, the C++ exception machinery and locale tables, at
// 0x180005c24-0x180012fff.
//
// Only the first range is in scope. The CRT half runs whether we like it or not
// — the loader calls _DllMainCRTStartup on every thread attach and _freefls on
// every thread exit — and it cannot be removed while the DLL is loaded at all,
// which it must be: Runtime is what LoadLibrary's it, and the seam reads its
// twelve vtables out of its .rdata.
//
// Hits above the boundary are reported as "expected", below it as findings.
// Note the direction of the risk: a renderer function that Ghidra placed in the
// CRT range would be under-reported, but the two ranges are contiguous and
// exhaustive over .text, so that would require the attribution in
// decomp/videodx11.md to be wrong, not merely incomplete.
constexpr uintptr_t kRendererEnd = 0x180005b7f;   // inclusive
constexpr uintptr_t kCrtBegin    = 0x180005c24;

const char* ExpectedReason(uintptr_t ghidra) {
    if (ghidra >= kCrtBegin)
        return "MSVC CRT / EH / locale (decomp/videodx11.md: 0x180005c24+), "
               "not renderer code";
    if (ghidra > kRendererEnd)
        return "in the gap between the renderer and CRT ranges — unattributed";
    return nullptr;
}

bool Protect(uint8_t* at, size_t len, DWORD prot) {
    DWORD old = 0;
    if (VirtualProtect(at, len, prot, &old)) return true;
    fprintf(stderr, "[Dx11Tripwire] VirtualProtect(%p, 0x%llx, 0x%lx) failed: %lu\n",
            at, static_cast<unsigned long long>(len), prot, GetLastError());
    g_broken = true;
    return false;
}

// Disarming is PER PAGE, not per section, and that distinction is the whole
// difference between a useful measurement and a useless one.
//
// A hit has to be let through — blocking it turns the diagnostic into a crash.
// The first version restored execute on the entire .text to do that, which
// meant the first CRT-plumbing hit (_DllMainCRTStartup, _freefls: loader
// callbacks, not renderer code) blinded the tripwire for the rest of startup
// — precisely the window in which CreateDeviceData runs. Reopening only the
// 4 KiB page that faulted keeps the other seventeen armed, so a CRT page going
// quiet costs nothing elsewhere.
//
// Page granularity is VirtualProtect's, not a choice. It does mean the
// ~15 real functions sharing a page with a CRT callback lose coverage until the
// next frame boundary re-arms everything — a known, bounded blind spot rather
// than a silent one.
constexpr size_t kPage = 0x1000;
size_t   g_pageCount = 0;
size_t   g_openPages = 0;     // pages a hit has reopened, awaiting the next re-arm
bool     g_pageArmed[64];     // 0x11964 bytes of .text is 18 pages

uint8_t* PageAt(size_t i) { return g_textBase + i * kPage; }
size_t   PageLen(size_t i) {
    size_t off = i * kPage;
    return (g_textSize - off < kPage) ? g_textSize - off : kPage;
}

// PAGE_READWRITE, not PAGE_NOACCESS: the DLL's .text must stay readable, both
// because the seam re-reads original slot targets out of it and because
// stack-walking a crash dump reads code bytes. Only execute is withdrawn, which
// is exactly the event we want to detect.
void Arm() {
    if (g_broken) return;
    for (size_t i = 0; i < g_pageCount; ++i) {
        if (g_pageArmed[i]) continue;
        if (!Protect(PageAt(i), PageLen(i), PAGE_READWRITE)) return;
        g_pageArmed[i] = true;
    }
    g_openPages = 0;
    g_armed     = true;
}

void Disarm() {
    if (g_broken) return;
    for (size_t i = 0; i < g_pageCount; ++i) {
        if (!g_pageArmed[i]) continue;
        if (!Protect(PageAt(i), PageLen(i), PAGE_EXECUTE_READ)) return;
        g_pageArmed[i] = false;
    }
    g_armed = false;
}

// Reopen just the page that faulted, leaving the rest armed.
void DisarmPage(uintptr_t rip) {
    size_t i = (rip - reinterpret_cast<uintptr_t>(g_textBase)) / kPage;
    if (i >= g_pageCount || !g_pageArmed[i]) return;
    if (!Protect(PageAt(i), PageLen(i), PAGE_EXECUTE_READ)) return;
    g_pageArmed[i] = false;
    ++g_openPages;
}

void Record(uintptr_t rip, uintptr_t caller) {
    for (int i = 0; i < g_hitCount; ++i) {
        if (g_hits[i].rip == rip) { ++g_hits[i].count; return; }
    }
    if (g_hitCount >= kMaxHits) return;

    const ModuleInfo* info = GetModuleInfo(Module::VideoDX11);
    Hit& h = g_hits[g_hitCount++];
    h.rip    = rip;
    h.ghidra = info ? rip - static_cast<uintptr_t>(info->delta) : 0;
    h.caller = caller;
    h.count  = 1;
    h.reason = ExpectedReason(h.ghidra);

    if (h.reason) {
        printf("[Dx11Tripwire] expected entry at ghidra 0x%llx: %s\n",
               static_cast<unsigned long long>(h.ghidra), h.reason);
        fflush(stdout);
        return;
    }
    fprintf(stderr,
            "[Dx11Tripwire] *** SHIPPED VideoDX11 CODE EXECUTED *** at %p "
            "(ghidra 0x%llx), apparent caller %p\n",
            reinterpret_cast<void*>(rip),
            static_cast<unsigned long long>(h.ghidra),
            reinterpret_cast<void*>(caller));
    fflush(stderr);
}

LONG CALLBACK TripwireVeh(EXCEPTION_POINTERS* ep) {
    if (!g_armed) return EXCEPTION_CONTINUE_SEARCH;

    const EXCEPTION_RECORD* er = ep->ExceptionRecord;
    if (er->ExceptionCode != EXCEPTION_ACCESS_VIOLATION ||
        er->NumberParameters < 2)
        return EXCEPTION_CONTINUE_SEARCH;

    // ExceptionInformation[0] == 8 is "DEP violation" (attempted execute).
    // Anything else here is a genuine bad read/write and belongs to the crash
    // handler, not to us.
    if (er->ExceptionInformation[0] != 8) return EXCEPTION_CONTINUE_SEARCH;

    uintptr_t rip = static_cast<uintptr_t>(er->ExceptionInformation[1]);
    uintptr_t lo  = reinterpret_cast<uintptr_t>(g_textBase);
    if (rip < lo || rip >= lo + g_textSize) return EXCEPTION_CONTINUE_SEARCH;

    // Let it run. Whatever it is, blocking it would turn a diagnostic into a
    // crash, and we would learn less than one line of log tells us.
    DisarmPage(rip);

    if (Dx11IsDetourStub(rip)) {
        ++g_stubHits;
    } else {
        // At a called function's first instruction RSP points at the return
        // address, so [RSP] names the caller. That is only true for a fault on
        // an entry point; for a fault on an internal jump target the value is
        // whatever the frame holds, hence "apparent caller" in the report.
        // Read without a guard: this handler runs inside an exception already,
        // and MinGW has no __try, so a nested fault here would be worse than
        // the missing datum. RSP on a DEP fault is by construction the valid
        // stack of a live thread.
        uintptr_t rsp    = static_cast<uintptr_t>(ep->ContextRecord->Rsp);
        uintptr_t caller = (rsp && (rsp & 7) == 0)
                               ? *reinterpret_cast<uintptr_t*>(rsp)
                               : 0;
        Record(rip, caller);
    }
    return EXCEPTION_CONTINUE_EXECUTION;
}

// Find .text in the loaded image and check it against the objdump values.
bool LocateText() {
    const ModuleInfo* info = GetModuleInfo(Module::VideoDX11);
    if (!info || !info->handle) {
        fprintf(stderr, "[Dx11Tripwire] VideoDX11-x64.dll not loaded\n");
        return false;
    }
    uint8_t* base = reinterpret_cast<uint8_t*>(info->handle);
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return false;

    IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nt);
    for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++sec) {
        if (std::memcmp(sec->Name, ".text", 6) != 0) continue;

        if (sec->VirtualAddress != kExpectedTextRva ||
            sec->Misc.VirtualSize != kExpectedTextSize) {
            fprintf(stderr,
                    "[Dx11Tripwire] .text is RVA 0x%lx size 0x%lx, expected "
                    "0x%llx / 0x%llx — this is not the build dx11/README.md "
                    "maps. Not arming.\n",
                    sec->VirtualAddress, sec->Misc.VirtualSize,
                    static_cast<unsigned long long>(kExpectedTextRva),
                    static_cast<unsigned long long>(kExpectedTextSize));
            return false;
        }
        g_textBase  = base + sec->VirtualAddress;
        g_textSize  = sec->Misc.VirtualSize;
        g_pageCount = (g_textSize + kPage - 1) / kPage;
        if (g_pageCount > sizeof(g_pageArmed) / sizeof(g_pageArmed[0])) {
            fprintf(stderr, "[Dx11Tripwire] .text is %llu pages, table holds %llu\n",
                    static_cast<unsigned long long>(g_pageCount),
                    static_cast<unsigned long long>(sizeof(g_pageArmed) /
                                                    sizeof(g_pageArmed[0])));
            return false;
        }
        return true;
    }
    fprintf(stderr, "[Dx11Tripwire] no .text section found\n");
    return false;
}

void AtExitReport() {
    Disarm();
    Dx11TripwireReport();
}

}  // namespace

bool Dx11TripwireEnabled() {
    if (!g_resolved) {
        g_resolved = true;
        const char* v = std::getenv("BANISHED_DX11_TRIPWIRE");
        g_enabled = v && v[0] && std::strcmp(v, "0") != 0 &&
                    std::strcmp(v, "false") != 0;
    }
    return g_enabled;
}

bool Dx11TripwireInstall() {
    if (!Dx11TripwireEnabled()) return false;
    if (g_veh) return true;
    if (!LocateText()) return false;

    // First = 1 so we run ahead of standalone/main.cpp's FirstChanceFailFast,
    // which would otherwise fail-fast on our deliberate access violations.
    g_veh = AddVectoredExceptionHandler(1, TripwireVeh);
    if (!g_veh) {
        fprintf(stderr, "[Dx11Tripwire] AddVectoredExceptionHandler failed: %lu\n",
                GetLastError());
        return false;
    }

    Arm();
    if (!g_armed) return false;

    std::atexit(AtExitReport);
    printf("[Dx11Tripwire] ARMED over VideoDX11 .text %p..%p (0x%llx bytes). "
           "Any execution of shipped renderer code will be reported.\n",
           g_textBase, g_textBase + g_textSize,
           static_cast<unsigned long long>(g_textSize));
    fflush(stdout);
    return true;
}

void Dx11TripwireRearm() {
    if (!g_enabled || g_broken || !g_veh) return;
    if (!g_armed || g_openPages == 0) return;   // fully disarmed, or nothing to close
    Arm();
}

void Dx11TripwireDisarm() {
    if (g_armed) Disarm();
}

int Dx11TripwireHitCount() {
    int findings = 0;
    for (int i = 0; i < g_hitCount; ++i)
        if (!g_hits[i].reason) ++findings;
    return findings;
}

void Dx11TripwireReport() {
    if (!g_enabled) return;
    if (!g_veh) {
        printf("[Dx11Tripwire] never armed — nothing was measured\n");
        fflush(stdout);
        return;
    }
    int findings = 0;
    for (int i = 0; i < g_hitCount; ++i)
        if (!g_hits[i].reason) ++findings;

    if (findings == 0) {
        printf("[Dx11Tripwire] CLEAN: no shipped VideoDX11 RENDERER code "
               "executed while armed\n");
    } else {
        printf("[Dx11Tripwire] NOT CLEAN: %d distinct entry point(s) into "
               "shipped renderer code:\n", findings);
    }
    for (int i = 0; i < g_hitCount; ++i) {
        const Hit& h = g_hits[i];
        printf("[Dx11Tripwire]   %-8s ghidra 0x%llx  x%u  (caller %p)%s%s\n",
               h.reason ? "expected" : "FINDING",
               static_cast<unsigned long long>(h.ghidra), h.count,
               reinterpret_cast<void*>(h.caller),
               h.reason ? "  " : "", h.reason ? h.reason : "");
    }
    printf("[Dx11Tripwire]   %u detour-trampoline entries (our bytes at a DLL "
           "address)\n", g_stubHits);
    fflush(stdout);
}

}  // namespace VideoDX11
