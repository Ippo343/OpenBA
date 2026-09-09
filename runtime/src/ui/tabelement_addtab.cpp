// ============================================================================
// runtime/src/ui/tabelement_addtab.cpp — WAVE 10, lane UIORACLE
//
//   ?AddTab@TabElement@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@
//    V?$ExternalPtr@VElementDescription@UI@@@4@0@Z     retail 0x1800DA130
//                                                      ordinal 1589
// ============================================================================
//
// THE RECORDED BLOCKER, RE-DERIVED FROM THE DISASSEMBLY RATHER THAN INHERITED
// ---------------------------------------------------------------------------
// Three documents hold this row shut and each names a different reason:
//
//   * WAVE7_LANE_E.md §2 — "the body constructs a temporary ElementDescription/
//     DialogInstance and is not a leaf accessor".
//   * WAVE8_5_HOLDBACK_RECHECK.md row 22 — "needs ElementDescription::
//     CreateInstance reversed/owned first".
//   * The UI census — "not replayable, needs the predictive oracle"; parked
//     behind the allocator NO-GO.
//
// Re-checked (reference_holdback_blockers_are_stale_check_them_first.md):
//
//  1. THE FIRST ONE IS DISSOLVED, AND IT WAS A MISREADING OF THE FRAME.
//     The stack temporary is not an ElementDescription and it is not a
//     DialogInstance. It is a plain `UI::Dialog`, DEFAULT-CONSTRUCTED, and the
//     construction is `??0Dialog@UI@@QEAA@XZ` INLINED — the two listings agree
//     store for store:
//
//        AddTab 0x1800DA163..0x1800DA1D2     ??0Dialog 0x180112DC1..0x180112E2A
//        vptr := 0x180154788, 0x1801547A0    identical
//        +0x08/+0x10/+0x18 := 0              identical
//        vptr := 0x1801562A0 (Dialog vftbl)  identical
//        +0x20..+0x28 := 0 (one MOVDQA)      two MOV, same 16 bytes
//        +0x30 := 0xFFFFFFFF                 identical
//        +0x34/+0x38/+0x40/+0x48..+0x80 := 0 identical
//
//     …so the object is 0x88 bytes and the exported constructor and destructor
//     ARE the inline sequences. This port calls them instead of open-coding a
//     memset it would have to keep in sync with a class it does not own. The
//     teardown matches the same way (0x1800DA295..0x1800DA329 against
//     ??1Dialog@UI@@UEAA@XZ 0x180112E40).
//
//  2. THE SECOND ONE IS TRUE AND IS NOT A BLOCKER FOR THE *BODY*.
//     `ElementDescription::CreateInstance` is genuinely unported. But AddTab
//     does not call it by name — it makes an INDIRECT call through the
//     ExternalPtr's own vtable slot +0x40 (0x1800DA1E5, 0x1800DA235), which is
//     a pointer both this port and the DLL load from the same object. Porting
//     the body needs the slot, not the callee. What the missing callee DOES
//     block is the ORACLE's coverage of the two objects it creates — stated
//     below, and declared with Exclude() so it is counted rather than implied.
//
//  3. THE ALLOCATOR NO-GO IS NOT LOAD-BEARING HERE EITHER. Every allocation in
//     this function happens inside a callee: the two CreateInstance calls, and
//     `Collection::Array<InternalPtr<Element>>::SetSize(n, fill)` (0x180076900),
//     which has been PORTED and registered since Wave 9 lane UIW
//     (array.cpp, kSpec_Fill8Z). **This lane's conclusion does not depend on
//     lane ALLOC's re-test of that gate.**
//
//  4. NOT REPLAYABLE — STILL TRUE, AND MORE SO THAN THE ROW SAYS. A replay
//     oracle would have to run retail's body a second time; retail's body
//     CREATES TWO ELEMENTS AND APPENDS THEM. Running it twice does not produce
//     a comparable copy, it produces a TabElement with four children and a tab
//     count of two. That is not a weak oracle, it is corruption.
//
// THE ORACLE, AND EXACTLY WHAT IT DOES AND DOES NOT COVER
// -------------------------------------------------------
// Predictive (runtime/funcs/rt_predict.h): ours predicts and writes nothing,
// the shipped DLL runs for real on pristine state, and we compare. Because ours
// writes nothing, the two CreateInstance calls do not happen twice — retail
// makes them, and THE CALLER STILL GETS RETAIL'S ANSWER IN ITS RETURN SLOT, so
// a predicting run is a working run.
//
//   COMPARED, and non-vacuously (`changed=` is >= 1 every round because the
//   tab count always moves):
//     * `this+0xC0`, the tab count, +1 (0x1800DA286).
//     * The DIRTY WALK: `p->_flags |= 0x8000000` for `this` and every ancestor
//       up the `_parent` (+0x98) chain — run TWICE, once after each append
//       (0x1800DA211 and 0x1800DA270), UNCONDITIONALLY. Note the contrast with
//       UpdateContainers, whose walk is guarded on "did bit 0 change"; a tidied
//       port that shared one helper between them would be wrong in one of the
//       two. The second walk's predictions land on addresses the first already
//       predicted and are reported in the `superseded=` column, which is what
//       that column is for: an intermediate value, never a post-state claim.
//
//   EXCLUDED, i.e. holes that PRINT rather than hide:
//     * The children Array header at `this+0xA0` (data pointer, size, capacity).
//       Retail appends two elements our suppressed body did not create, so the
//       size moves by two and the buffer may be reallocated. Not predictable
//       while CreateInstance is unported.
//     * The two created Elements themselves, including the `_parent` store at
//       `elem+0x98`. They do not exist in predict mode.
//
//   THE SURVIVING BLOCKER, NAMED: `?CreateInstance@ElementDescription@UI@@…`
//   (and the ExternalPtr vtable +0x40 thunk that reaches it) is unported, so
//   the append half of this function is verified only in the sense that its
//   inputs and its call targets are recorded (`RtSink::Call`). The day that
//   callee lands, the Exclude on `this+0xA0` can be replaced by a prediction
//   and this file's `excluded=` column should go to zero.
//
// ROUTING — CHECKED, NOT ASSUMED
// -------------------------------
// `get_xrefs_to 0x1800DA130` returns FOUR references and NOT ONE OF THEM IS A
// CALL: three data references (the export table and two RTTI/relocation slots)
// plus the external entry point. The DLL never calls its own AddTab. The only
// caller in the process is ours — `UIDesc_AddControllers_Impl`
// (code/Game/Dialog/toolui.cpp:133), which runs while a dialog whose
// UIDescription carries tab entries is being built. So, unlike
// `TabElement::UpdateContainers`, THIS PORT NEEDS NO DETOUR: there is no split
// brain to close, because there is no second caller.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <cstdio>

#include "../../funcs/rt_port.h"
#include "../../funcs/rt_predict.h"
#include "../../types/ui/Element.h"
#include "../../types/ui/TabElement.h"

// ----------------------------------------------------------------------------
// ResolveAddr, forward-declared rather than pulled in from standalone/resolve.h
// (same reasoning as runtime/src/ui/widgets_layout.cpp and rt_port.h itself:
// that header drags in <windows.h>, which collides with engine type/member
// names). This declaration must stay byte-identical to resolve.h's.
// ----------------------------------------------------------------------------
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace {

namespace TAE = UI::TabElementOffsets;

inline uint8_t* B(void* p, int off) { return reinterpret_cast<uint8_t*>(p) + off; }

constexpr int      kFlags     = offsetof(UI::Element, _flags);    // 0x34
constexpr int      kParent    = offsetof(UI::Element, _parent);   // 0x98
constexpr int      kTabCount  = 0xC0;                             // 0x1800DA286
constexpr uint32_t kDirtyBit  = 0x8000000u;                       // NeedsSizeCalc

// UI::Dialog, 0x88 bytes — see the store-for-store table in the banner.
constexpr size_t kDialogSize = 0x88;

// ??0Dialog@UI@@QEAA@XZ (0x180112DB0) and ??1Dialog@UI@@UEAA@XZ (0x180112E40).
// `UEAA` is the VIRTUAL spelling and it is the right one here: retail's inline
// teardown is the non-deleting destructor's body, and the QEAA spelling of a
// virtual destructor IS NOT IN THE EXPORT TABLE — it would resolve to null in
// silence (reference_phantom_dtor_mangling_qeaa_vs_ueaa.md, the same trap that
// cost Wave 6 a session). Both names were checked against the DLL's export
// directory before being written here, not inferred from the class shape
// (reference_resolveexport_phantom_names.md).
using DialogCtorFn = void* (*)(void*);
using DialogDtorFn = void  (*)(void*);
inline DialogCtorFn DialogCtor() {
    return RT_EXPORT_ONCE(DialogCtorFn, "??0Dialog@UI@@QEAA@XZ");
}
inline DialogDtorFn DialogDtor() {
    return RT_EXPORT_ONCE(DialogDtorFn, "??1Dialog@UI@@UEAA@XZ");
}

// Collection::Array<InternalPtr<Element>>::SetSize(n, const T& fill) — retail
// 0x180076900, PORTED and registered (array.cpp, kSpec_Fill8Z). Not exported (a
// header-only template instantiation), so it is reached by address through
// ResolveAddr, which consults the PORT REGISTRY FIRST
// (reference_resolveaddr_consults_port_registry_first.md).
inline void ArrayCellsSetSizeFill(void* arrayHdr, int32_t n, const void* fill) {
    using Fn = void (*)(void*, int32_t, const void*);
    static Fn fn = nullptr;                 // cache a hit, retry a miss
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x180076900));
    fn(arrayHdr, n, fill);
}

// ?UpdateContainers@TabElement@UI@@AEAAXXZ — PORTED in this same wave
// (runtime/src/ui/widgets_layout.cpp) and registered under name AND address.
void UpdateContainers(void* self) {
    using Fn = void (*)(void*);
    RT_EXPORT_ONCE(Fn, "?UpdateContainers@TabElement@UI@@AEAAXXZ")(self);
}

// The ExternalPtr<ElementDescription> virtual at slot +0x40 (index 8).
// (this, InternalPtr<Element>* ret, UI::Dialog* ctx) -> ret
using CreateInstanceFn = void** (*)(void*, void**, void*);
inline CreateInstanceFn CreateInstanceSlot(void* extPtr) {
    return reinterpret_cast<CreateInstanceFn*>(*reinterpret_cast<void**>(extPtr))[8];
}

// The UNCONDITIONAL dirty walk. Deliberately NOT shared with
// widgets_layout.cpp's `WalkDirty`, and not with UpdateContainers' guarded one:
// retail spells this walk out twice here with no guard at all
// (0x1800DA211, 0x1800DA270), and a helper shared between a guarded and an
// unguarded walk is one edit away from being wrong in whichever caller the
// editor was not looking at.
void DirtyWalk(void* self, RtSink& out) {
    for (uint8_t* p = reinterpret_cast<uint8_t*>(self); p != nullptr;
         p = *reinterpret_cast<uint8_t**>(p + kParent)) {
        out.U32(p + kFlags, out.ReadU32(p + kFlags) | kDirtyBit,
                "element._flags |= NeedsSizeCalc");
    }
}

// ---------------------------------------------------------------------------
// The body, written ONCE against an RtSink.
//
// `ret` is BOTH this function's hidden return slot AND the destination of the
// second CreateInstance (0x1800DA232 loads RSI, which is the incoming RDX).
// That aliasing is retail's, not a modelling shortcut, and it is called out
// because an oracle that snapshotted `self` and `ret` independently would be
// comparing one buffer against itself
// (reference_oracle_must_snapshot_aliased_inputs.md).
// ---------------------------------------------------------------------------
void AddTabBody(void* self, void** ret, void* descA, void* descB, RtSink& out) {
    alignas(16) uint8_t dlg[kDialogSize];
    DialogCtor()(dlg);                                        // 0x1800DA163..1D2

    void* cell = nullptr;   // retail's [RBP+0x67], the scratch InternalPtr

    // ---- tab BUTTON: created from descA into the scratch cell --------------
    CreateInstanceFn createA = CreateInstanceSlot(descA);
    if (out.Call("ExternalPtr<ElementDescription>::vtbl+0x40 (tab)",
                 reinterpret_cast<const void*>(createA),
                 reinterpret_cast<int64_t>(descA),
                 reinterpret_cast<int64_t>(&cell),
                 reinterpret_cast<int64_t>(dlg))) {
        createA(descA, &cell, dlg);                           // 0x1800DA1E5
        if (cell) *reinterpret_cast<void**>(B(cell, kParent)) = self;   // 0x1800DA1F0
        ArrayCellsSetSizeFill(B(self, TAE::kChildrenData),
                              *reinterpret_cast<int32_t*>(B(self, TAE::kChildrenCount)) + 1,
                              &cell);                          // 0x1800DA209
    }
    DirtyWalk(self, out);                                      // 0x1800DA211

    // ---- tab CONTENT: created from descB straight into the return slot -----
    CreateInstanceFn createB = CreateInstanceSlot(descB);
    if (out.Call("ExternalPtr<ElementDescription>::vtbl+0x40 (content)",
                 reinterpret_cast<const void*>(createB),
                 reinterpret_cast<int64_t>(descB),
                 reinterpret_cast<int64_t>(ret),
                 reinterpret_cast<int64_t>(dlg))) {
        createB(descB, ret, dlg);                              // 0x1800DA235
        cell = *ret;                                           // 0x1800DA238/23B
        if (cell) *reinterpret_cast<void**>(B(cell, kParent)) = self;   // 0x1800DA244
        ArrayCellsSetSizeFill(B(self, TAE::kChildrenData),
                              *reinterpret_cast<int32_t*>(B(self, TAE::kChildrenCount)) + 1,
                              &cell);                          // 0x1800DA25E
    }
    DirtyWalk(self, out);                                      // 0x1800DA270

    out.U32(B(self, kTabCount),
            out.ReadU32(B(self, kTabCount)) + 1, "TabElement._tabCount");  // 0x1800DA286

    // UpdateContainers is suppressed in predict mode for the reason its own
    // port documents: it recomputes CHILD visibility, and in predict mode the
    // children retail is about to append do not exist yet. Retail runs its own
    // a moment later, on the real children.
    if (out.Call("UI::TabElement::UpdateContainers",
                 reinterpret_cast<const void*>(&UpdateContainers),
                 reinterpret_cast<int64_t>(self))) {
        UpdateContainers(self);                                // 0x1800DA28F
    }

    DialogDtor()(dlg);                                         // 0x1800DA295..329
}

} // namespace

// ============================================================================
// The registered entry point.
// ============================================================================
extern "C" void** RtUi_TabElement_AddTab(void* self, void** ret,
                                         void* descA, void* descB) {
    RT_PREDICT_SITE("UI::TabElement::AddTab");

    if (!RT_PREDICT_ON) {
        RtSink live;
        AddTabBody(self, ret, descA, descB, live);
        return ret;                                            // 0x1800DA32D
    }

    {
        RtPredictRound round(&rt_predict_site__);

        // Coverage: every flag word the dirty walk can reach, plus the tab
        // count. Predictions alone are only a lower bound; this is what turns
        // the round into an oracle rather than a spot check.
        for (uint8_t* p = reinterpret_cast<uint8_t*>(self); p != nullptr;
             p = *reinterpret_cast<uint8_t**>(p + kParent))
            round.Watch(p + kFlags, sizeof(uint32_t), "element._flags");
        round.Watch(B(self, kTabCount), sizeof(uint32_t), "TabElement._tabCount");
        round.Watch(B(self, TAE::kChildrenData), 0x10, "Element::_children header");

        // The append half. Retail creates and appends two Elements our
        // suppressed body did not, so the array header moves for reasons no
        // prediction can model while ElementDescription::CreateInstance is
        // unported. Declared, so it prints in `excluded=` and "not reported"
        // can never quietly read as "verified".
        round.Exclude(B(self, TAE::kChildrenData), 0x10,
                      "two suppressed CreateInstance calls + two appends; "
                      "ElementDescription::CreateInstance is not ported");

        AddTabBody(self, ret, descA, descB, round.sink());      // predicts

        // THEIRS. RT_ORIGINAL_ONCE, not RT_EXPORT_ONCE: by the time this runs
        // our body is registered under this very name, and RT_EXPORT_ONCE would
        // resolve to ourselves and recurse. There is no detour on 0x1800DA130
        // (the DLL never calls its own AddTab — see the routing note in the
        // banner), so the raw export IS retail here.
        using Fn = void** (*)(void*, void**, void*, void*);
        RT_ORIGINAL_ONCE(Fn,
            "?AddTab@TabElement@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@"
            "V?$ExternalPtr@VElementDescription@UI@@@4@0@Z")(self, ret, descA, descB);

        round.Verify();
    }
    return ret;
}

// ============================================================================
// Registration — NAME AND ADDRESS, both.
// ============================================================================
int RtUiTabAddTabRegister() {
    int n = 0;
    RegisterRuntimeExport(
        "?AddTab@TabElement@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@"
        "V?$ExternalPtr@VElementDescription@UI@@@4@0@Z",
        (void*)&RtUi_TabElement_AddTab); ++n;
    RegisterRuntimeAddr(0x1800da130, (void*)&RtUi_TabElement_AddTab);
    return n;
}
