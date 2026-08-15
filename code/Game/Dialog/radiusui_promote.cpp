// code/Game/Dialog/radiusui_promote.cpp
//
// RadiusUI: standalone vtable promotion (out of UI_DIALOG_STUB).
//
// RETAIL VTABLE — read byte-wise, not guessed. `vtable_RadiusUI` @ 0x14015d160,
// 11 slots. The 0x20 bytes BEFORE it (0x14015d140) are the RTTI type descriptor
// (shared pointer 0x14003a978 + the ASCII class name "RadiusUI"), exactly the trap
// reference_rtti_descriptor_mistaken_for_vtable warns about.
//
//   idx0  (+0x00) 0x140013640  ScalarDtor
//   idx1  (+0x08) 0x140013580
//   idx2  (+0x10) 0x140013570  GetTypeId
//   idx3  (+0x18) 0x14003a95a  \
//   idx4  (+0x20) 0x14003a984   |  6-byte `FF 25` jmp thunks into the exe's Runtime
//   idx7  (+0x38) 0x14003a97e   |  dispatch slots (0x1401505e0/e8/f0/f8/0x140150600/
//   idx8  (+0x40) 0x14003a96c   |  608/678/0x140151770) — i.e. INHERITED
//   idx9  (+0x48) 0x14003a972   |  UI::DialogController base methods, no exe override.
//   idx10 (+0x50) 0x14003a978  /
//   idx5  (+0x28) 0x140077650  RadiusUI::Slot5 — build the coverage overlay
//   idx6  (+0x30) 0x140077600  RadiusUI::Slot6 — tear the overlay down
//
// So RadiusUI overrides exactly two slots, and radiusui.cpp's header comments
// (idx5 = +0x28 = 0x140077650, idx6 = +0x30 = 0x140077600) are CORRECT — unlike
// the CropUI ones. Base slots stay StubNop, matching orchardui.cpp / eventlogui.cpp:
// they are Runtime DialogController methods with no exe body to port.
//
// DialogController slot semantics (see the verified map at the top of
// eventlogui.cpp): idx5 = setup on show, idx6 = teardown, idx7 = per-frame sync.
//
// WHY THIS FILE EXISTS: `UI_DIALOG_STUB(RadiusUI, "UI::DialogController", 0x38)`
// gave RadiusUI a 48-slot table where only idx0/idx2 were real, so both ported
// bodies were dead code and no info panel ever showed its coverage radius.
// RadiusUI is the FIRST controller in the UIDescription list of FishermansDock.rsc /
// HunterLodge.rsc / GathererShelter.rsc.
//
// The overlay submit itself (fn_RadiusCalc = 0x140055430) only became safe today:
// Player_HighlightAdd_55430 used to pass `gameWorld + 0x110` instead of
// `*(int64_t*)(gameWorld + 0x110)` and faulted instantly, which is almost certainly
// why RadiusUI was left stubbed.

#include "Dialog/ui_panels.h"
#include "engine_funcs.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>

namespace radiusui_sa {

constexpr int kSlots = 48;
static void* g_vt[kSlots];

void StubNop(void*, ...) {}

uint32_t GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("RadiusUI");
    return c;
}

void* Dtor(void* self, uint32_t flags) {
    if (!self) return self;
    // Retail's dtor path runs the base DialogController dtor, which for a still-open
    // panel has already had idx6 dispatched. Drop the overlay here too so a destroyed
    // controller cannot leak a highlight slot.
    RadiusUI_Slot6(reinterpret_cast<RadiusUI*>(self));
    *reinterpret_cast<void**>(self) = g_vt;
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

// idx5 (+0x28) — retail 0x140077650. Needs the DialogInstance (+0x18) the framework
// seats before dispatch, same precondition orchardui.cpp's idx4 wrapper checks.
void W_Slot5(void* self) {
    if (!self) return;
    if (!SA_DialogEntityAlive(self)) {
        if (std::getenv("BANISHED_DIALOG_TRACE")) {
            fprintf(stderr, "[RadiusUI] Slot5 SKIP (no live data source) self=%p\n", self);
            fflush(stderr);
        }
        return;
    }
    RadiusUI_Slot5(reinterpret_cast<RadiusUI*>(self));
}

// idx6 (+0x30) — retail 0x140077600. Unconditional: it resets +0x30 to -1 in both
// arms, and the manager pointer read is guarded by data30 >= 0 inside the body.
void W_Slot6(void* self) {
    if (!self) return;
    RadiusUI_Slot6(reinterpret_cast<RadiusUI*>(self));
}

struct Init {
    Init() {
        for (int i = 0; i < kSlots; ++i) g_vt[i] = reinterpret_cast<void*>(&StubNop);
        g_vt[0] = reinterpret_cast<void*>(&Dtor);
        g_vt[2] = reinterpret_cast<void*>(&GetTypeId);
        g_vt[5] = reinterpret_cast<void*>(&W_Slot5);
        g_vt[6] = reinterpret_cast<void*>(&W_Slot6);
    }
} g_init;

// RadiusUI object size 0x38. Factory_RadiusUI (0x140013590) alloc's 0x38/8, runs the
// DialogController base ctor, installs the vtable and — critically — seeds
// +0x30 = 0xFFFFFFFF. The generic UI_DIALOG_STUB Zero() left it 0, which would have
// made the very first Slot6 call RadiusDisplay_Remove(mgr, 0) on an overlay slot this
// controller never owned. The base ctor is skipped here (as in orchardui.cpp /
// cropui_promote.cpp): every base field is zero and the framework seats +0x18/+0x20/
// +0x28 before dispatch.
void Zero(void* p) {
    for (size_t o = 0; o < static_cast<size_t>(0x38); o += 8)
        *reinterpret_cast<uint64_t*>(static_cast<char*>(p) + o) = 0;
    *reinterpret_cast<int32_t*>(static_cast<char*>(p) + 0x30) = -1;
}
void* Factory() {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, static_cast<uint64_t>(0x38), 8);
    if (!p) return nullptr;
    Zero(p);
    *reinterpret_cast<void**>(p) = g_vt;
    return p;
}
void* InPlace(void* m) {
    if (!m) return nullptr;
    Zero(m);
    *reinterpret_cast<void**>(m) = g_vt;
    return m;
}

}  // namespace radiusui_sa

REGISTER_TYPE(RadiusUI, "UI::DialogController",
              &radiusui_sa::Factory, &radiusui_sa::InPlace);
