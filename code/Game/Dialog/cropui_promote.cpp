// code/Game/Dialog/cropui_promote.cpp
//
// CropUI: the three pure virtuals of the SelectResourceUI base, CropUI's own idx4/idx7,
// and the standalone vtable promotion.
//
// ⚠ The Slot4/Slot7/Slot8 bodies in cropui.cpp are the ABSTRACT BASE's
// (0x140078be0 / 0x140078cb0 / 0x140078d30), not CropUI's — the file's comments
// attribute them to CropUI, which is wrong. Read byte-wise from retail's vtable at
// 0x14015f270 (14 slots), CropUI's own table is:
//   idx4  (+0x20) 0x1400b2b50  CropUI_Startup — base idx4, then bind "yield"
//   idx7  (+0x38) 0x1400b2a50  CropUI_Update  — maturity into the bar, then base idx7
//   idx8  (+0x40) 0x140078d30  SelectResourceUI::Slot8 (combo refresh)
//   idx9  (+0x48) 0x140078d80  SelectResourceUI::Slot9 (combo selection -> idx11)
//   idx11 (+0x58) 0x1400b2920  set crop type on CropFieldComponent+0x50
//   idx12 (+0x60) 0x1400b29d0  get current crop type
//   idx13 (+0x68) 0x1400b29c0  category filter — bytes are `33 C0 C3`
//                              (XOR EAX,EAX; RET) = 0. Orchard is 1, pasture 2.
//
// WHY THIS FILE EXISTS: CropUI was the last member of the SelectResourceUI family still
// on UI_DIALOG_STUB (OrchardUI and PastureUI were promoted earlier), so every body in
// cropui.cpp was dead code — the combo was never bound, never given element id 500, and
// never populated, and idx9 was a no-op so a pick could not reach idx11. That is why the
// crop field's "?" button did nothing: the dropdown opened with zero rows and had no
// handler behind it. Structure follows orchardui.cpp exactly.

#include "Dialog/ui_panels.h"
#include "engine_funcs.h"
#include "Object/factory_registration.h"
#include "resolve.h"
#include <cstdint>
#include <cstdlib>
#include <cstdio>

namespace {

typedef void* (*CropFindComp_t)(int64_t entity, void* outIBP);
typedef float (*CropMaturity_t)(void* comp);

CropFindComp_t s_findCropComp = nullptr;   // FUN_140005380
CropMaturity_t s_cropMaturity = nullptr;   // FUN_14011b410

void EnsureFuncs() {
    if (!s_findCropComp)
        s_findCropComp = reinterpret_cast<CropFindComp_t>(ResolveAddr(Module::Exe, 0x140005380));
    if (!s_cropMaturity)
        s_cropMaturity = reinterpret_cast<CropMaturity_t>(ResolveAddr(Module::Exe, 0x14011b410));
}

// Resolve the CropFieldComponent behind this panel's entity. Mirrors the retail
// prologue shared by idx7/idx11/idx12: entity(+0x28) -> IsDerivedFrom(ComponentEntity)
// -> InternalBasePtr -> FUN_140005380.
void* FindComp(void* self) {
    EnsureFuncs();
    if (!s_findCropComp || !self) return nullptr;
    uintptr_t s = reinterpret_cast<uintptr_t>(self);
    void* entity = *reinterpret_cast<void**>(s + 0x28);
    uint32_t entTid = g_TypeId_ComponentEntity ? *g_TypeId_ComponentEntity : 0;
    void* checked = fn_IsDerivedFrom(entity, entTid) ? entity : nullptr;
    uint64_t entIbp = 0;
    fn_InternalBasePtr_Ctor(&entIbp, checked);
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&entIbp);
    uint64_t comp = 0;
    s_findCropComp(static_cast<int64_t>(entIbp), &comp);
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&comp);
    return reinterpret_cast<void*>(comp);
}

// Retail filters the "yield" FindElement result through *g_TypeId_ProgressElement, an
// .rdata slot that is null unless BANISHED_LIVE_ELEMENT_TIDS is on. Same private
// by-name resolve orchardui.cpp uses, for the same reason: IsDerivedFrom(elem, 0) is
// always false, which would leave +0x38 null and turn the +0x124 write into a
// null-deref. See reference_gated_element_typeid_disables_controller.
uint32_t ProgressElementTypeId() {
    if (g_TypeId_ProgressElement && *g_TypeId_ProgressElement)
        return *g_TypeId_ProgressElement;
    static uint32_t s_local = 0;
    if (!s_local) s_local = ::Object::ResolveTypeIdByNameLazy("ProgressElement");
    return s_local;
}

}  // namespace

// ============================================================
// idx11 (+0x58) — retail 0x1400b2920. Set the crop type.
// ============================================================
// If the field currently has none and a real type is picked, retail dispatches the
// component's own vtable[14](comp, 2) BEFORE storing. Order matters: that handler reads
// +0x50 as "still unset" to decide this is a fresh planting.
void CropUI_Slot11(CropUI* self, int64_t newType) {
    void* comp = FindComp(self);

    if (std::getenv("BANISHED_DIALOG_TRACE")) {
        fprintf(stderr, "[CropUI] Slot11 self=%p comp=%p newType=%llx cur(+0x50)=%llx\n",
                reinterpret_cast<void*>(self), comp, (unsigned long long)newType,
                comp ? (unsigned long long)*reinterpret_cast<int64_t*>(
                           reinterpret_cast<uintptr_t>(comp) + 0x50) : 0ull);
        fflush(stderr);
    }

    // SA guard (retail unguarded): in retail a combo event can only be routed to an
    // entity that has a CropFieldComponent.
    if (!comp) return;

    int64_t cur = *reinterpret_cast<int64_t*>(reinterpret_cast<uintptr_t>(comp) + 0x50);
    if (cur == 0 && newType != 0) {
        void** vt = *reinterpret_cast<void***>(comp);
        typedef void (*StartCycle_t)(void*, int);
        reinterpret_cast<StartCycle_t>(vt[14])(comp, 2);
    }
    *reinterpret_cast<int64_t*>(reinterpret_cast<uintptr_t>(comp) + 0x50) = newType;
}

// idx12 (+0x60) — retail 0x1400b29d0. Current crop type via out-param.
void* CropUI_Slot12(CropUI* self, void* out) {
    void* comp = FindComp(self);
    *reinterpret_cast<uint64_t*>(out) =
        comp ? *reinterpret_cast<uint64_t*>(reinterpret_cast<uintptr_t>(comp) + 0x50) : 0;
    return out;
}

// idx13 (+0x68) — retail 0x1400b29c0 is literally `XOR EAX,EAX; RET`.
int CropUI_Slot13(CropUI* self) {
    (void)self;
    return 0;
}

// ============================================================
// STANDALONE promotion — real CropUI controller vtable
// ============================================================
namespace cropui_sa {

constexpr int kSlots = 48;
static void* g_vt[kSlots];

void StubNop(void*, ...) {}

uint32_t GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("CropUI");
    return c;
}

void* Dtor(void* self, uint32_t flags) {
    if (!self) return self;
    *reinterpret_cast<void**>(self) = g_vt;
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

// idx4 (+0x20) — retail 0x1400b2b50: base idx4 (combo bind + element id 500 +
// populate), then find "yield" and seat it at +0x38 with progress 1.0f.
void W_Slot4(void* self) {
    if (!self) return;
    uintptr_t s = reinterpret_cast<uintptr_t>(self);
    // Needs the DialogInstance (+0x18) the framework seats before dispatch.
    if (!*reinterpret_cast<void**>(s + 0x18)) return;

    CropUI_Slot4(reinterpret_cast<CropUI*>(self));   // = 0x140078be0, the base's idx4

    uint8_t nameBuf[8];
    fn_String_Ctor_CStr(nameBuf, "yield");
    void* parentElem = *reinterpret_cast<void**>(s + 0x20);
    uint64_t found = 0;
    fn_FindElement(parentElem, &found, nameBuf);
    fn_String_Dtor(nameBuf);

    void* raw = reinterpret_cast<void*>(found);
    void* checked = fn_IsDerivedFrom(raw, ProgressElementTypeId()) ? raw : nullptr;
    uint64_t ibp = 0;
    fn_InternalBasePtr_Ctor(&ibp, checked);
    *reinterpret_cast<uint64_t*>(s + 0x38) = ibp;

    // Retail writes this unguarded (Crop.rsc always carries "yield"); guarded here for
    // the same reason orchardui.cpp is — the shared StandardDialog mount could reach a
    // layout without it, which retail cannot.
    if (*reinterpret_cast<uint64_t*>(s + 0x38))
        *reinterpret_cast<float*>(*reinterpret_cast<uint64_t*>(s + 0x38) + 0x124) = 1.0f;

    if (std::getenv("BANISHED_DIALOG_TRACE")) {
        void* combo = reinterpret_cast<void*>(*reinterpret_cast<uint64_t*>(s + 0x30));
        fprintf(stderr, "[CropUI] Slot4 self=%p combo(+0x30)=%p yield(+0x38)=%p rows=%d\n",
                self, combo, reinterpret_cast<void*>(*reinterpret_cast<uint64_t*>(s + 0x38)),
                combo ? fn_ComboElement_GetItemCount(combo) : -1);
        fflush(stderr);
    }
}

// idx7 (+0x38) — retail 0x1400b2a50: mean maturity into the yield bar + percent text,
// then the base's idx7 (enable the combo iff it has items).
void W_Slot7(void* self) {
    if (!self) return;
    if (!SA_DialogEntityAlive(self)) return;
    uintptr_t s = reinterpret_cast<uintptr_t>(self);

    // Self-heal, same as orchardui.cpp: Slot4 may not have run for this instance yet.
    if (!*reinterpret_cast<uint64_t*>(s + 0x38)) {
        W_Slot4(self);
        if (!*reinterpret_cast<uint64_t*>(s + 0x38)) return;
    }

    void* comp = FindComp(self);
    if (!comp) return;

    EnsureFuncs();
    float maturity = s_cropMaturity ? s_cropMaturity(comp) : 0.0f;

    void* yieldElem = reinterpret_cast<void*>(*reinterpret_cast<uint64_t*>(s + 0x38));
    *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(yieldElem) + 0x128) = maturity;

    int percent = static_cast<int>(maturity * 100.0f + 0.5f);
    uint8_t textBuf[16];
    fn_String_Format(reinterpret_cast<char*>(textBuf), "%i",
                     static_cast<uint64_t>(static_cast<uint32_t>(percent)));
    fn_ProgressElement_SetProgressTextParameter(yieldElem, 0, textBuf);
    fn_String_Dtor(textBuf);

    if (*reinterpret_cast<uint64_t*>(s + 0x30))
        CropUI_Slot7(reinterpret_cast<CropUI*>(self));   // = 0x140078cb0, the base's idx7
}

// idx8 (+0x40) — base combo refresh, with the same null-combo self-heal.
void W_Slot8(void* self) {
    if (!self) return;
    if (!SA_DialogEntityAlive(self)) return;
    uintptr_t s = reinterpret_cast<uintptr_t>(self);
    if (!*reinterpret_cast<uint64_t*>(s + 0x30)) {
        W_Slot4(self);
        if (!*reinterpret_cast<uint64_t*>(s + 0x30)) return;
    }
    SelectResourceUI_Slot8(self);
}

// idx9 (+0x48) — combo selection -> idx11. This is the slot that was StubNop, i.e. the
// one that swallowed every pick on the "?" combo.
void W_Slot9(void* self, void* p2, int p3, int p4) {
    if (!self) return;
    SelectResourceUI_Slot9(self, p2, p3, p4);
}

struct Init {
    Init() {
        for (int i = 0; i < kSlots; ++i) g_vt[i] = reinterpret_cast<void*>(&StubNop);
        g_vt[0]  = reinterpret_cast<void*>(&Dtor);
        g_vt[2]  = reinterpret_cast<void*>(&GetTypeId);
        g_vt[4]  = reinterpret_cast<void*>(&W_Slot4);
        g_vt[7]  = reinterpret_cast<void*>(&W_Slot7);
        g_vt[8]  = reinterpret_cast<void*>(&W_Slot8);
        g_vt[9]  = reinterpret_cast<void*>(&W_Slot9);
        g_vt[11] = reinterpret_cast<void*>(&CropUI_Slot11);
        g_vt[12] = reinterpret_cast<void*>(&CropUI_Slot12);
        g_vt[13] = reinterpret_cast<void*>(&CropUI_Slot13);
    }
} g_init;

// CropUI object size 0x60 (engine/types/ui/Panels.h).
void Zero(void* p) {
    for (size_t o = 0; o < static_cast<size_t>(0x60); o += 8)
        *reinterpret_cast<uint64_t*>(static_cast<char*>(p) + o) = 0;
}
void* Factory() {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, static_cast<uint64_t>(0x60), 8);
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

}  // namespace cropui_sa

REGISTER_TYPE(CropUI, "SelectResourceUI", &cropui_sa::Factory, &cropui_sa::InPlace);
