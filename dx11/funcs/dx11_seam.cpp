// The VideoDX11 replacement seam. See dx11_seam.h for the design, and for why
// this owns the twelve vtables rather than wrapping DeviceData or patching the
// shipped DLL.

#include "dx11_seam.h"

#include <cstdio>
#include <cstdlib>   // std::atexit

namespace VideoDX11 {

// ============================================================
// The twelve vtables
// ============================================================
// `ghidra` is the shipped DLL's vtable address. Nothing resolves it any more —
// it is the key that decomp/videodx11.md and the status board in
// dx11/README.md are indexed by, and it is printed in the seam report so a log
// line can still be matched against the decompilation.
struct VtDesc {
    const char* name;
    uintptr_t   ghidra;
    int         slots;
};

static const VtDesc kVtables[static_cast<int>(Vt::kVtCount)] = {
    { "DeviceData",         0x180018bb0, 20 },
    { "ContextData",        0x1800186f0, 20 },
    { "ImageBufferData",    0x180018790,  7 },
    { "ConstantBufferData", 0x1800185d0,  4 },
    { "IndexBufferData",    0x1800187c8,  7 },
    { "VertexBufferData",   0x180018830,  7 },
    { "VertexLayoutData",   0x180018878,  3 },
    { "VertexProgramData",  0x180018890,  3 },
    { "PixelProgramData",   0x180018800,  3 },
    { "RenderStateData",    0x180018818,  3 },
    { "VideoProgramData",   0x1800185a8,  3 },
    { "RenderBufferData",   0x180018588,  4 },
};

const char* VtName(Vt v)       { return kVtables[static_cast<int>(v)].name; }
int         VtSlotCount(Vt v)  { return kVtables[static_cast<int>(v)].slots; }
uintptr_t   VtGhidraAddr(Vt v) { return kVtables[static_cast<int>(v)].ghidra; }

// ============================================================
// Registration table
// ============================================================
// Deliberately a POD with no default member initializers. Lanes register from
// static initializers in their own .cpp (see DX11_REGISTER_SLOT), and those run
// in unspecified order relative to each other. Keeping SlotEntry trivially
// constructible puts s_slots in .bss with guaranteed zero-initialization
// *before* any dynamic initializer runs, so registration order cannot matter.
struct SlotEntry {
    void*       impl;
    const char* label;
    bool        live;
};

static SlotEntry s_slots[static_cast<int>(Vt::kVtCount)][kMaxSlots];
static bool      s_installed = false;

// ============================================================
// Our twelve vtables
// ============================================================
//
// The leading `rtti` word is not padding. MSVC puts the RTTI Complete Object
// Locator at vtable[-1], and the shipped DLL's arrays have a zero there
// (checked at 0x180018ba8 for DeviceData and 0x1800186e8 for ContextData — the
// DLL carries no object locators). Reproducing the zero keeps our arrays
// shaped like the ones they replace, so anything that reads [-1] reads what it
// read before rather than the tail of the previous array.
struct OurVtable {
    void* rtti;
    void* slot[kMaxSlots];
};
static OurVtable s_ourVt[static_cast<int>(Vt::kVtCount)];

void** Dx11LiveVtable(Vt v) {
    if (!s_installed) return nullptr;
    int vi = static_cast<int>(v);
    if (vi < 0 || vi >= static_cast<int>(Vt::kVtCount)) return nullptr;
    return s_ourVt[vi].slot;
}

bool Dx11SeamReady() { return s_installed; }

static bool ValidSlot(Vt v, int slot) {
    int vi = static_cast<int>(v);
    return vi >= 0 && vi < static_cast<int>(Vt::kVtCount) &&
           slot >= 0 && slot < kVtables[vi].slots;
}

// Fallback accounting. Every Dx11OriginalSlot() call site is a "our port could
// not resolve a Runtime import, run the original" branch left over from when an
// original was mapped. Counting is unconditional (not env-gated) because the
// cost is one increment on a path that is meant to be dead.
static unsigned s_forward[static_cast<int>(Vt::kVtCount)][kMaxSlots];
static unsigned s_forwardTotal = 0;

unsigned Dx11ForwardCount() { return s_forwardTotal; }

void* Dx11OriginalSlot(Vt v, int slot) {
    if (!ValidSlot(v, slot)) return nullptr;
    const SlotEntry& e = s_slots[static_cast<int>(v)][slot];

    ++s_forwardTotal;
    if (s_forward[static_cast<int>(v)][slot]++ == 0) {
        fprintf(stderr,
                "[Dx11Seam] FORWARD TO ORIGINAL: %s slot %d (%s) — but the "
                "shipped DLL is not loaded, so there is no original. Returning "
                "null; the caller will fault. A Runtime import failed to "
                "resolve.\n",
                VtName(v), slot, e.label ? e.label : "?");
        fflush(stderr);
    }
    return nullptr;
}

bool Dx11RegisterSlot(Vt v, int slot, void* impl, const char* label) {
    if (s_installed) {
        fprintf(stderr, "[Dx11Seam] REJECTED %s slot %d (%s): registered after install\n",
                ValidSlot(v, slot) ? VtName(v) : "?", slot, label ? label : "");
        return false;
    }
    if (!ValidSlot(v, slot) || !impl) {
        fprintf(stderr, "[Dx11Seam] REJECTED slot registration (%s): bad vtable/slot/impl\n",
                label ? label : "?");
        return false;
    }
    SlotEntry& e = s_slots[static_cast<int>(v)][slot];
    if (e.impl) {
        fprintf(stderr, "[Dx11Seam] REJECTED %s slot %d (%s): already registered by %s\n",
                VtName(v), slot, label ? label : "?", e.label ? e.label : "?");
        return false;
    }
    e.impl  = impl;
    e.label = label;
    return true;
}

// ============================================================
// Install — build the twelve vtables from the registrations
// ============================================================
bool Dx11SeamInstall() {
    if (s_installed) return true;

    int total = 0, holes = 0;

    for (int vi = 0; vi < static_cast<int>(Vt::kVtCount); ++vi) {
        s_ourVt[vi].rtti = nullptr;
        for (int slot = 0; slot < kVtables[vi].slots; ++slot) {
            SlotEntry& e = s_slots[vi][slot];
            ++total;
            if (!e.impl) {
                fprintf(stderr,
                        "[Dx11Seam] HOLE: %s slot %d has no reimplementation, "
                        "and there is no shipped code to fall back to.\n",
                        kVtables[vi].name, slot);
                ++holes;
                continue;
            }
            s_ourVt[vi].slot[slot] = e.impl;
            e.live = true;
        }
    }

    if (holes) {
        fprintf(stderr,
                "[Dx11Seam] ABORT: %d/%d slots unimplemented. Not installing.\n",
                holes, total);
        return false;
    }

    s_installed = true;
    printf("[Dx11Seam] %d/%d slots ours, in %d exe-owned vtables. "
           "VideoDX11-x64.dll is not required.\n",
           total, total, static_cast<int>(Vt::kVtCount));
    for (int vi = 0; vi < static_cast<int>(Vt::kVtCount); ++vi) {
        printf("[Dx11Seam]   %-19s -> %p (%d slots)\n",
               kVtables[vi].name, static_cast<void*>(s_ourVt[vi].slot),
               kVtables[vi].slots);
    }

    // Print the slot table (and, crucially, the fallback count) at exit. The
    // count is only evidence if it is always reported: a check that has to be
    // remembered to be switched on proves nothing about the runs where nobody
    // remembered.
    std::atexit(Dx11SeamReport);
    fflush(stdout);
    return true;
}

void Dx11SeamReport() {
    if (!s_installed) {
        printf("[Dx11Seam] not installed\n");
        return;
    }
    printf("\n[Dx11Seam] === slot table ===\n");
    for (int vi = 0; vi < static_cast<int>(Vt::kVtCount); ++vi) {
        const VtDesc& d = kVtables[vi];
        printf("[Dx11Seam] %-19s @ %p (%d slots, was 0x%llx)\n",
               d.name, (void*)Dx11LiveVtable(static_cast<Vt>(vi)), d.slots,
               static_cast<unsigned long long>(d.ghidra));
        for (int slot = 0; slot < d.slots; ++slot) {
            const SlotEntry& e = s_slots[vi][slot];
            if (!e.impl) continue;
            printf("[Dx11Seam]     slot %2d  %-7s  %s%s\n", slot,
                   e.live ? "OURS" : "----", e.label ? e.label : "?",
                   s_forward[vi][slot] ? "   *** HIT THE DEAD FALLBACK ***" : "");
        }
    }
    printf("[Dx11Seam] fallbacks to shipped code: %u (expected 0)\n",
           s_forwardTotal);
    fflush(stdout);
}

}  // namespace VideoDX11
