// Phase 0 — the VideoDX11 replacement seam. See dx11_seam.h for the design and
// for why this patches vtable slots rather than wrapping DeviceData.

#include "dx11_seam.h"

#include "resolve.h"

#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace VideoDX11 {

// ============================================================
// The twelve vtables, and every slot target as walked from .rdata
// 0x180018580-0x180018c60 on 2026-07-24 (see ../README.md).
//
// This table is the runtime counterpart of the static_asserts in
// dx11/types/VideoDX11Data.h: it is checked against the live DLL at install
// time, so a wrong base, a wrong ASLR delta, or a different build of
// VideoDX11-x64.dll is caught before a single byte is written.
// ============================================================
struct VtDesc {
    const char* name;
    uintptr_t   ghidra;      // vtable address in the Ghidra image
    int         slots;
    uintptr_t   target[kMaxSlots];
};

static const VtDesc kVtables[static_cast<int>(Vt::kVtCount)] = {
    { "DeviceData", 0x180018bb0, 20, {
        0x180004270, 0x180004690, 0x180004e10, 0x1800051c0,
        0x180001190, 0x180005430, 0x180005440, 0x180001060,
        0x180005450, 0x180005650, 0x1800056c0, 0x180005730,
        0x1800057a0, 0x180005810, 0x180005880, 0x1800058f0,
        0x180005960, 0x1800059d0, 0x180005a40, 0x180005aa0 } },

    { "ContextData", 0x1800186f0, 20, {
        0x180001600, 0x180001650, 0x180001720, 0x1800017c0,
        0x180001920, 0x180001ad0, 0x180001b60, 0x180001be0,
        0x180001c40, 0x180001cb0, 0x180001d10, 0x180001d30,
        0x180001d80, 0x180001ef0, 0x180001e60, 0x180001f00,
        0x180002000, 0x180002020, 0x180002030, 0x180002150 } },

    { "ImageBufferData", 0x180018790, 7, {
        0x180002400, 0x180002580, 0x180002650, 0x180002870,
        0x180002da0, 0x180002e10, 0x180002ef0 } },

    { "ConstantBufferData", 0x1800185d0, 4, {
        0x180001240, 0x180001300, 0x180001460, 0x180001450 } },

    // Slots 3 and 4 are byte-identical to VertexBufferData's slots 3 and 4 —
    // the same two addresses appear in both vtables.
    { "IndexBufferData", 0x1800187c8, 7, {
        0x180003080, 0x1800030f0, 0x1800031c0, 0x180003290,
        0x180003340, 0x180003380, 0x1800033b0 } },

    { "VertexBufferData", 0x180018830, 7, {
        0x180003b20, 0x180003b90, 0x180003c60, 0x180003290,
        0x180003340, 0x180003d30, 0x180003d90 } },

    { "VertexLayoutData", 0x180018878, 3, {
        0x180003f60, 0x180003fc0, 0x180003ff0 } },

    { "VertexProgramData", 0x180018890, 3, {
        0x180004110, 0x1800041a0, 0x180004230 } },

    { "PixelProgramData", 0x180018800, 3, {
        0x180003580, 0x180003610, 0x1800036a0 } },

    { "RenderStateData", 0x180018818, 3, {
        0x1800036e0, 0x180003760, 0x180003a90 } },

    { "VideoProgramData", 0x1800185a8, 3, {
        0x180001140, 0x180001060, 0x180001190 } },

    { "RenderBufferData", 0x180018588, 4, {
        0x180001010, 0x180001060, 0x180001070, 0x1800010e0 } },
};

const char* VtName(Vt v)       { return kVtables[static_cast<int>(v)].name; }
int         VtSlotCount(Vt v)  { return kVtables[static_cast<int>(v)].slots; }
uintptr_t   VtGhidraAddr(Vt v) { return kVtables[static_cast<int>(v)].ghidra; }

// ============================================================
// Registration table
// ============================================================
struct SlotEntry {
    void*       impl      = nullptr;   // our reimplementation
    void*       original  = nullptr;   // captured before any patch
    const char* envFlag   = nullptr;
    const char* label     = nullptr;
    bool        defaultOn = false;
    bool        live      = false;     // is our version currently installed?
};

static SlotEntry s_slots[static_cast<int>(Vt::kVtCount)][kMaxSlots];
static void**    s_vtable[static_cast<int>(Vt::kVtCount)] = { nullptr };
static bool      s_installed = false;

bool Dx11SeamReady() { return s_installed; }

static bool ValidSlot(Vt v, int slot) {
    int vi = static_cast<int>(v);
    return vi >= 0 && vi < static_cast<int>(Vt::kVtCount) &&
           slot >= 0 && slot < kVtables[vi].slots;
}

void* Dx11OriginalSlot(Vt v, int slot) {
    if (!ValidSlot(v, slot)) return nullptr;
    return s_slots[static_cast<int>(v)][slot].original;
}

// Same semantics as ComponentFactory::LiveEnabled: an explicit env setting wins
// in both directions; otherwise the registration's defaultOn decides. Kept local
// so dx11/ has no dependency on code/Game/.
static bool EnvOn(const char* v) {
    return v && v[0] && std::strcmp(v, "0") != 0 && std::strcmp(v, "false") != 0;
}
static bool SlotEnabled(const SlotEntry& e) {
    if (e.envFlag) {
        const char* v = std::getenv(e.envFlag);
        if (v && v[0]) return EnvOn(v);
    }
    return e.defaultOn;
}

bool Dx11RegisterSlot(Vt v, int slot, void* impl,
                      const char* envFlag, bool defaultOn, const char* label) {
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
    e.impl      = impl;
    e.envFlag   = envFlag;
    e.label     = label;
    e.defaultOn = defaultOn;
    return true;
}

// Write one pointer into the DLL's read-only .rdata.
static bool WriteSlot(void** vtable, int slot, void* value) {
    void** target = vtable + slot;
    DWORD  oldProtect = 0;
    if (!VirtualProtect(target, sizeof(void*), PAGE_READWRITE, &oldProtect))
        return false;
    *target = value;
    DWORD ignored = 0;
    VirtualProtect(target, sizeof(void*), oldProtect, &ignored);
    // .rdata is data, not code, so no FlushInstructionCache is needed. The
    // indirect call reads it fresh on every dispatch.
    return true;
}

bool Dx11SetSlotEnabled(Vt v, int slot, bool useOurs) {
    if (!s_installed || !ValidSlot(v, slot)) return false;
    SlotEntry& e = s_slots[static_cast<int>(v)][slot];
    if (!e.impl) return false;
    void* want = useOurs ? e.impl : e.original;
    if (!WriteSlot(s_vtable[static_cast<int>(v)], slot, want)) {
        fprintf(stderr, "[Dx11Seam] VirtualProtect failed for %s slot %d\n",
                VtName(v), slot);
        return false;
    }
    e.live = useOurs;
    printf("[Dx11Seam] %s slot %2d -> %s (%s)\n", VtName(v), slot,
           useOurs ? "OURS" : "original", e.label ? e.label : "?");
    return true;
}

bool Dx11SeamInstall() {
    if (s_installed) return true;

    const ModuleInfo* info = GetModuleInfo(Module::VideoDX11);
    if (!info || !info->handle) {
        printf("[Dx11Seam] VideoDX11-x64.dll not loaded — seam not installed\n");
        return false;
    }

    // --- Pass 1: resolve the vtables and verify every slot ---------------
    // Nothing is written until all 84 slots match. A mismatch means our map of
    // the DLL is wrong, and patching on a wrong map corrupts a vtable.
    int mismatches = 0;
    int checked    = 0;
    for (int vi = 0; vi < static_cast<int>(Vt::kVtCount); ++vi) {
        const VtDesc& d = kVtables[vi];
        void** vt = reinterpret_cast<void**>(
            ResolveAddr(Module::VideoDX11, d.ghidra));
        if (!vt) {
            fprintf(stderr, "[Dx11Seam] could not resolve %s vtable @ 0x%llx\n",
                    d.name, static_cast<unsigned long long>(d.ghidra));
            ++mismatches;
            continue;
        }
        s_vtable[vi] = vt;

        for (int slot = 0; slot < d.slots; ++slot) {
            uintptr_t expected = ResolveAddr(Module::VideoDX11, d.target[slot]);
            uintptr_t actual   = reinterpret_cast<uintptr_t>(vt[slot]);
            ++checked;
            if (expected != actual) {
                fprintf(stderr,
                        "[Dx11Seam] MISMATCH %s slot %d: expected %p (ghidra 0x%llx), "
                        "found %p\n",
                        d.name, slot, reinterpret_cast<void*>(expected),
                        static_cast<unsigned long long>(d.target[slot]),
                        reinterpret_cast<void*>(actual));
                ++mismatches;
            }
            s_slots[vi][slot].original = vt[slot];
        }
    }

    if (mismatches) {
        fprintf(stderr,
                "[Dx11Seam] ABORT: %d/%d slots did not match the map in "
                "dx11/README.md. Nothing patched; running on the original "
                "plugin.\n", mismatches, checked);
        return false;
    }
    printf("[Dx11Seam] Verified %d/%d vtable slots against dx11/README.md "
           "(base %p, delta %+lld)\n",
           checked, checked, reinterpret_cast<void*>(info->runtimeBase),
           static_cast<long long>(info->delta));

    s_installed = true;

    // --- Pass 2: apply the enabled overrides -----------------------------
    int applied = 0, dormant = 0, failed = 0;
    for (int vi = 0; vi < static_cast<int>(Vt::kVtCount); ++vi) {
        for (int slot = 0; slot < kVtables[vi].slots; ++slot) {
            SlotEntry& e = s_slots[vi][slot];
            if (!e.impl) continue;
            if (!SlotEnabled(e)) { ++dormant; continue; }
            if (WriteSlot(s_vtable[vi], slot, e.impl)) {
                e.live = true;
                ++applied;
            } else {
                fprintf(stderr, "[Dx11Seam] VirtualProtect failed for %s slot %d (%s)\n",
                        kVtables[vi].name, slot, e.label ? e.label : "?");
                ++failed;
            }
        }
    }

    printf("[Dx11Seam] Installed: %d slot(s) ours, %d dormant, %d failed, "
           "%d still original\n",
           applied, dormant, failed, checked - applied);
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
        printf("[Dx11Seam] %-19s @ %p (%d slots)\n",
               d.name, (void*)s_vtable[vi], d.slots);
        for (int slot = 0; slot < d.slots; ++slot) {
            const SlotEntry& e = s_slots[vi][slot];
            if (!e.impl) continue;   // un-ported slots are the silent majority
            printf("[Dx11Seam]     slot %2d  %-7s  %s\n", slot,
                   e.live ? "OURS" : "orig", e.label ? e.label : "?");
        }
    }
    fflush(stdout);
}

}  // namespace VideoDX11
