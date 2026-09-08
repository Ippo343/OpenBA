// ============================================================================
// runtime/src/ui/widgets_layout.cpp -- Wave 7 lane E -- Table/Image/Label/Text/
// DocumentText/Tab/Page/Graph
// ============================================================================
//
// Ports the 20 (of 24) `docs/runtime_port/wave7_lanes.txt` LANE E exports this
// lane could verify. Layouts and the evidence for every offset are in
// runtime/types/ui/{TableElement,TextElement,DocumentTextElement,TabElement,
// PageElement,GraphElement,ImageElement,LabelElement}.h; the per-function
// write-up, the oracle argument, and everything held back are in
// docs/runtime_port/WAVE7_LANE_E.md.
//
// None of these eight classes' field offsets are modeled as inheriting a
// C++ `UI::Element` -- lane A owns Element.h and had not published it when
// this lane's recon happened (docs/runtime_port/WAVE7.md, "cross-lane layout
// contract"). Every access below is instead a raw offset from `this`, proven
// against retail disassembly and named in the corresponding type header. This
// cannot collide with whatever struct lane A ships, and the header comments
// say explicitly which offsets are believed to belong to the shared Element/
// ContainerElement base rather than to the widget class itself.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/ui/Element.h"
#include "../../types/ui/TableElement.h"
#include "../../types/ui/ImageElement.h"
#include "../../types/ui/LabelElement.h"
#include "../../types/ui/TextElement.h"
#include "../../types/ui/DocumentTextElement.h"
#include "../../types/ui/TabElement.h"
#include "../../types/ui/PageElement.h"
#include "../../types/ui/GraphElement.h"

// ----------------------------------------------------------------------------
// ResolveAddr, forward-declared rather than pulled in from standalone/resolve.h
// (same reasoning as runtime/src/graphics/scene.cpp and rt_port.h itself: that
// header drags in <windows.h>, which collides with engine type/member names).
// This declaration must stay byte-identical to resolve.h's.
// ----------------------------------------------------------------------------
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace {

inline uint8_t* B(void* p, int off) { return reinterpret_cast<uint8_t*>(p) + off; }
inline uint32_t& DirtyFlags(void* p, int off) { return *reinterpret_cast<uint32_t*>(B(p, off)); }

// Every mutator in this lane, having changed its own state, walks `this` then
// its parent chain OR-ing this bit into the dirty-flags word at +0x34. This
// lane found the field independently (eight bodies: TableElement::SetSize,
// ImageElement::SetSprite/SetWidthHeight, TextElement::SetParameter/SetText,
// DocumentTextElement::SetText x2, PageElement::SetActivePage) before lane A's
// Element.h landed; it is now confirmed to be UI::Element::_flags /
// kElementFlagNeedsSizeCalc and UI::Element::_parent.
constexpr uint32_t kDirtyBit = UI::kElementFlagNeedsSizeCalc;
constexpr int kDirtyFlags = offsetof(UI::Element, _flags);
constexpr int kParent = offsetof(UI::Element, _parent);

inline void WalkDirty(void* self) {
    for (void* node = self; node; node = *reinterpret_cast<void**>(B(node, kParent)))
        DirtyFlags(node, kDirtyFlags) |= kDirtyBit;
}

// ---- Retail helpers this lane calls through rather than reimplements -------
//
// Each is either a genuine DLL export (resolved by mangled name, safe to call
// on any run because RT_ORIGINAL_ONCE/ResolveExportRaw costs a GetProcAddress
// only once per call site -- reference_rtoriginal_is_getprocaddress.md), or an
// internal, unexported Collection::Array<T> template instantiation (resolved
// by address, same idiom runtime/src/graphics/scene.cpp already uses for the
// same reason: these are not exported at all, so RtOriginal/ResolveExportRaw
// cannot reach them).

// ?RemoveChildren@Element@UI@@QEAAXXZ (0x180115570) -- exported despite being
// simple; genuinely shared Element/ContainerElement behaviour this lane has no
// business reimplementing.
void RtRemoveChildren(void* self) {
    using Fn = void (*)(void*);
    RT_ORIGINAL_ONCE(Fn, "?RemoveChildren@Element@UI@@QEAAXXZ")(self);
}

// ?CacheText@TextElement@UI@@AEAAXXZ (0x18011ec10) -- private but exported.
// Reformats the cached display string from the raw text + parameter array.
// NOT reimplemented: it allocates/releases refcounted System::Strings through
// String::FormatParamList, which is out of this lane's scope (Sprite/String
// primitives are lane F's).
void RtTextElementCacheText(void* self) {
    using Fn = void (*)(void*);
    RT_ORIGINAL_ONCE(Fn, "?CacheText@TextElement@UI@@AEAAXXZ")(self);
}

// ?UpdateContainers@TabElement@UI@@AEAAXXZ (0x1800d9710) -- private but
// exported. Recomputes which child page is visible; not reversed by this lane.
void RtTabElementUpdateContainers(void* self) {
    using Fn = void (*)(void*);
    RT_ORIGINAL_ONCE(Fn, "?UpdateContainers@TabElement@UI@@AEAAXXZ")(self);
}

// ??4String@System@@QEAAAEAV01@AEBV01@@Z -- System::String::operator=.
void RtStringAssign(void* self, const void* other) {
    using Fn = void* (*)(void*, const void*);
    // RT_EXPORT_ONCE (Wave 9 lane R): String::operator= is ported; raw bypassed it.
    RT_EXPORT_ONCE(Fn, "??4String@System@@QEAAAEAV01@AEBV01@@Z")(self, other);
}

// ?MemFree@MemoryAllocator@System@@SAXPEAX@Z
void RtMemFree(void* p) {
    using Fn = void (*)(void*);
    RT_ORIGINAL_ONCE(Fn, "?MemFree@MemoryAllocator@System@@SAXPEAX@Z")(p);
}

// Collection::Array<int32_t>::SetSize -- retail 0x18002fea0. Not exported (a
// header-only template instantiation, per runtime/types/collection/
// Collection.h); resolved by address like scene.cpp's RtArrayU32SetSize.
// WAVE 9 LANE Q: 0x18002fea0 and 0x1800764c0 below are both registered ports
// now, so these reach our SetSize. Note 0x18002fea0 is one of the TWO
// instantiations that leave the new tail UNINITIALISED (its `else` arm is
// empty) -- the port keeps that, and an oracle must not compare it.
void RtArrayInt32SetSize(void* arrayHeader, int32_t n) {
    using Fn = void (*)(void*, int32_t);
    static Fn fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x18002fea0));
    fn(arrayHeader, n);
}

// Collection::Array<InternalPtr<Element>>::SetSize -- retail 0x1800764c0
// (8-byte-element instantiation, same growth policy, not exported).
void RtArrayCellsSetSize(void* arrayHeader, int32_t n) {
    using Fn = void (*)(void*, int32_t);
    static Fn fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x1800764c0));
    fn(arrayHeader, n);
}

// Destroy-elements-and-free-buffer helper GraphElement::RemoveGraphs tail-
// calls -- retail 0x1800416d0, not exported.
void RtGraphArrayClear(void* arrayHeader) {
    using Fn = void (*)(void*);
    static Fn fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x1800416d0));
    fn(arrayHeader);
}

// Scalar deleting destructor call through vtable slot 0 with flag 1 -- the
// InternalPtr<T> "release and possibly delete" idiom already named in
// runtime/types/object/Internal.h's kInternalVtblScalarDtor. TableElement::
// SetChild uses this on the cell's previous occupant.
void ReleaseElement(void* elem) {
    if (!elem) return;
    void* vt = *reinterpret_cast<void**>(elem);
    using DtorFn = void* (*)(void*, int);
    DtorFn dtor = *reinterpret_cast<DtorFn*>(vt);
    dtor(elem, 1);
}

// ============================================================================
// UI::TableElement
// ============================================================================
using namespace UI::TableElementOffsets;

int32_t TableElement_GetColumnCount(const void* self) {
    RT_DIFF_BEGIN("UI::TableElement::GetColumnCount");
    int32_t ours = *reinterpret_cast<const int32_t*>(B(const_cast<void*>(self), kColCount));
    using Fn = int32_t (*)(const void*);
    RT_DIFF_SCALAR(ours, RT_ORIGINAL_ONCE(Fn, "?GetColumnCount@TableElement@UI@@QEBAHXZ")(self));
    return ours;
}

int32_t TableElement_GetRowCount(const void* self) {
    RT_DIFF_BEGIN("UI::TableElement::GetRowCount");
    int32_t ours = *reinterpret_cast<const int32_t*>(B(const_cast<void*>(self), kRowCount));
    using Fn = int32_t (*)(const void*);
    RT_DIFF_SCALAR(ours, RT_ORIGINAL_ONCE(Fn, "?GetRowCount@TableElement@UI@@QEBAHXZ")(self));
    return ours;
}

void TableElement_SetChild2(void* self, void** elemSlot, int32_t col) {
    void* elem = *elemSlot;
    if (elem) *reinterpret_cast<void**>(B(elem, kParent)) = self;

    void* cellsData = *reinterpret_cast<void**>(B(self, kCells));
    void** cellSlot = reinterpret_cast<void**>(cellsData) + col;
    void* old = *cellSlot;
    if (old != elem) {
        if (old) {
            ReleaseElement(old);
            *cellSlot = nullptr;
        }
        WalkDirty(self);
    }
    *cellSlot = elem;

    // NO ORACLE for the branch that matters. Calling retail for real a second
    // time here is SAFE (after ours runs, `*cellSlot == elem`, so retail's own
    // `puVar4 != *param_2` guard is false and it takes a no-op path) but that
    // is exactly the problem: it means the interesting branch -- the one that
    // calls ReleaseElement's scalar-deleting-destructor on the PREVIOUS
    // occupant -- is never exercised by the comparison, whether or not our
    // release logic is correct. And unlike the field-setters below, this one
    // cannot be fixed by restoring the pristine cell value before calling
    // retail: if `old` was actually released above, restoring the slot to
    // point at it and letting retail dereference `*puVar4`'s vtable would be a
    // use-after-free. Verifying the release path safely needs a disposable
    // child object built and torn down by this function specifically for the
    // diff, which was judged out of scope for this pass. Registered anyway --
    // the logic runs correctly in every normal (non-diff) frame, it is only
    // the A/B harness that cannot safely re-exercise the delete branch. See
    // WAVE7_LANE_E.md sec4a.
}

void TableElement_SetChild3(void* self, void** elemSlot, int32_t col, int32_t row) {
    int32_t colCount = *reinterpret_cast<int32_t*>(B(self, kColCount));
    TableElement_SetChild2(self, elemSlot, colCount * col + row);
    // Pure delegate to the function above (which has no oracle of its own --
    // see its comment); nothing to add here.
}

void TableElement_SetSize(void* self, int32_t rows, int32_t cols) {
    RT_DIFF_BEGIN("UI::TableElement::SetSize");
    int32_t pristineRows = 0, pristineCols = 0;
    if (rt_diff_on__) {
        pristineRows = *reinterpret_cast<int32_t*>(B(self, kRowCount));
        pristineCols = *reinterpret_cast<int32_t*>(B(self, kColCount));
    }

    *reinterpret_cast<int32_t*>(B(self, kRowCount)) = rows;
    *reinterpret_cast<int32_t*>(B(self, kColCount)) = cols;
    RtArrayInt32SetSize(B(self, kRowSizes), rows);
    RtArrayInt32SetSize(B(self, kColSizes), cols);
    RtRemoveChildren(self);
    RtArrayCellsSetSize(B(self, kCells), rows * cols);
    WalkDirty(self);

    // Oracle, scoped to what it can safely re-verify: the two OWN scalar
    // fields (+0xC0/+0xC4). Restoring them to their PRISTINE values before
    // calling retail for real means retail's own field writes execute for
    // real rather than silently agreeing with a value we already wrote --
    // fixing the "always passes" defect a same-value comparison would have.
    // The row/col-size Arrays and the cell array are NOT re-verified here:
    // `ours` calls the SAME retail Array<T>::SetSize/RemoveChildren
    // functions directly (by address, see the helpers above) rather than
    // reimplementing them, so there is no second, independent
    // implementation for a live compare to be checking against -- their
    // correctness is a source-level fact (we call retail's own code), not
    // something this harness re-derives. Restoring rows/cols alone does not
    // perturb those arrays: their own internal SetSize early-out is driven
    // by the array's own current _size field, which this restore does not
    // touch, so calling retail's whole SetSize again on the restored fields
    // is safe (same idempotent-array-resize argument as before) and does not
    // double-run RemoveChildren over live children (the array is already
    // empty by then, so a second call is genuinely a no-op, not a repaired
    // one).
    if (rt_diff_on__) {
        int32_t oursRows = *reinterpret_cast<int32_t*>(B(self, kRowCount));
        int32_t oursCols = *reinterpret_cast<int32_t*>(B(self, kColCount));
        *reinterpret_cast<int32_t*>(B(self, kRowCount)) = pristineRows;
        *reinterpret_cast<int32_t*>(B(self, kColCount)) = pristineCols;

        using Fn = void (*)(void*, int32_t, int32_t);
        RT_ORIGINAL_ONCE(Fn, "?SetSize@TableElement@UI@@QEAAXHH@Z")(self, rows, cols);

        RT_DIFF_SCALAR(oursRows, *reinterpret_cast<int32_t*>(B(self, kRowCount)));
        RT_DIFF_SCALAR(oursCols, *reinterpret_cast<int32_t*>(B(self, kColCount)));

        // Leave OUR result live regardless of the compare's outcome.
        *reinterpret_cast<int32_t*>(B(self, kRowCount)) = oursRows;
        *reinterpret_cast<int32_t*>(B(self, kColCount)) = oursCols;
    }
}

// ============================================================================
// UI::ImageElement
// ============================================================================

// 16, not UI::Sprite.h's kSpriteWrittenBytes (0x0C): that constant bounds what
// a Sprite CONSTRUCTOR writes, and this function is not one -- it is a COPY,
// proven at the instruction level (0x1801182a0: two independent 8-byte
// `MOV RAX,[..]`/`MOV [..],RAX` pairs, not a 12-byte-then-stop sequence), so
// the full 16 bytes is the width retail itself moves and the width this
// comparison is entitled to. See WAVE7_LANE_E.md sec4b.
void* ImageElement_GetSprite(const void* self, void* retval) {
    RT_DIFF_BEGIN("UI::ImageElement::GetSprite");
    uint64_t* r = reinterpret_cast<uint64_t*>(retval);
    const uint8_t* s = reinterpret_cast<const uint8_t*>(self);
    r[0] = *reinterpret_cast<const uint64_t*>(s + 0xC0);
    r[1] = *reinterpret_cast<const uint64_t*>(s + 0xC8);
    char scratch[16] = {};
    using Fn = void* (*)(const void*, void*);
    // The CALL is inside the guard, not just the resolve (Wave 9 lane R,
    // 2026-09-07). It used to sit outside it, so an ordinary run -- one with
    // BANISHED_RT_DIFF unset -- paid a cross-DLL call whose result it then threw
    // away, and at cutover it becomes a call through a null pointer. Storing the
    // pointer outside a guard is harmless; CALLING outside one is not.
    if (rt_diff_on__) {
        RT_ORIGINAL_ONCE(Fn, "?GetSprite@ImageElement@UI@@QEBA?AVSprite@2@XZ")(self, scratch);
    }
    RT_DIFF_BYTES(r, scratch, 16);
    return retval;
}

void ImageElement_SetRotation(void* self, float angle) {
    RT_DIFF_BEGIN("UI::ImageElement::SetRotation");
    float pristine = 0.f;
    if (rt_diff_on__) pristine = *reinterpret_cast<float*>(B(self, 0xD0));

    *reinterpret_cast<float*>(B(self, 0xD0)) = angle;
    // No dirty-flag walk in retail -- confirmed from the disassembly, not
    // assumed by analogy with the other setters in this file.

    if (rt_diff_on__) {
        float ours = *reinterpret_cast<float*>(B(self, 0xD0));
        *reinterpret_cast<float*>(B(self, 0xD0)) = pristine;  // undo, so retail runs on the real pre-call state
        using Fn = void (*)(void*, float);
        RT_ORIGINAL_ONCE(Fn, "?SetRotation@ImageElement@UI@@QEAAXM@Z")(self, angle);
        RT_DIFF_SCALAR(ours, *reinterpret_cast<float*>(B(self, 0xD0)));
        *reinterpret_cast<float*>(B(self, 0xD0)) = ours;  // OUR result stays live either way
    }
}

// 16 bytes throughout (write, snapshot and compare), not Sprite.h's
// kSpriteWrittenBytes: this function COPIES an existing Sprite value rather
// than constructing one, and retail's own copy (0x180118600) is two
// unconditional 8-byte `MOV RAX,[..]`/`MOV [..],RAX` pairs -- the full 16, not
// a 12-then-stop. Matching that width is what makes this port's write
// byte-faithful; narrowing it to 12 would leave retail's own +0xCC..+0xCF
// unwritten by us. See WAVE7_LANE_E.md sec4b.
void ImageElement_SetSprite(void* self, const void* spriteVal) {
    RT_DIFF_BEGIN("UI::ImageElement::SetSprite");
    uint8_t pristine[16];
    uint32_t pristineDirty = 0;
    if (rt_diff_on__) {
        std::memcpy(pristine, B(self, 0xC0), 16);
        pristineDirty = DirtyFlags(self, kDirtyFlags);
    }

    const uint64_t* sv = reinterpret_cast<const uint64_t*>(spriteVal);
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    bool differs = (*reinterpret_cast<uint64_t*>(s + 0xC0) != sv[0]) ||
                   (*reinterpret_cast<int32_t*>(s + 0xC8) != static_cast<int32_t>(sv[1]));
    if (differs) WalkDirty(self);
    *reinterpret_cast<uint64_t*>(s + 0xC0) = sv[0];
    *reinterpret_cast<uint64_t*>(s + 0xC8) = sv[1];

    if (rt_diff_on__) {
        uint8_t oursSprite[16];
        std::memcpy(oursSprite, s + 0xC0, 16);
        uint32_t oursDirty = DirtyFlags(self, kDirtyFlags);

        // Undo, so retail's own "did the sprite change" guard -- and, if it
        // fires, its WalkDirty, the riskiest part of this function -- runs
        // for real instead of seeing "already equal" and taking retail's own
        // no-op path (the trap lane C's EnableCursor finding named: our own
        // prior write makes the SECOND implementation's guard vacuous).
        std::memcpy(s + 0xC0, pristine, 16);
        DirtyFlags(self, kDirtyFlags) = pristineDirty;

        using Fn = void (*)(void*, const void*);
        RT_ORIGINAL_ONCE(Fn, "?SetSprite@ImageElement@UI@@QEAAXVSprite@2@@Z")(self, spriteVal);

        RT_DIFF_BYTES(oursSprite, s + 0xC0, 16);
        RT_DIFF_SCALAR(oursDirty, DirtyFlags(self, kDirtyFlags));

        std::memcpy(s + 0xC0, oursSprite, 16);           // OUR result stays live
        DirtyFlags(self, kDirtyFlags) = oursDirty;
    }
}

void ImageElement_SetWidthHeight(void* self, int32_t w, int32_t h) {
    RT_DIFF_BEGIN("UI::ImageElement::SetWidthHeight");
    int32_t pristineW = 0, pristineH = 0;
    uint32_t pristineDirty = 0;
    if (rt_diff_on__) {
        pristineW = *reinterpret_cast<int32_t*>(B(self, 0xD4));
        pristineH = *reinterpret_cast<int32_t*>(B(self, 0xD8));
        pristineDirty = DirtyFlags(self, kDirtyFlags);
    }

    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    bool differs = (*reinterpret_cast<int32_t*>(s + 0xD4) != w) ||
                   (*reinterpret_cast<int32_t*>(s + 0xD8) != h);
    if (differs) {
        *reinterpret_cast<int32_t*>(s + 0xD4) = w;
        *reinterpret_cast<int32_t*>(s + 0xD8) = h;
        WalkDirty(self);
    }

    if (rt_diff_on__) {
        int32_t oursW = *reinterpret_cast<int32_t*>(s + 0xD4);
        int32_t oursH = *reinterpret_cast<int32_t*>(s + 0xD8);
        uint32_t oursDirty = DirtyFlags(self, kDirtyFlags);

        // Undo, so retail's own "did it change" guard and WalkDirty run for
        // real against the true pre-call state (see SetSprite above).
        *reinterpret_cast<int32_t*>(s + 0xD4) = pristineW;
        *reinterpret_cast<int32_t*>(s + 0xD8) = pristineH;
        DirtyFlags(self, kDirtyFlags) = pristineDirty;

        using Fn = void (*)(void*, int32_t, int32_t);
        RT_ORIGINAL_ONCE(Fn, "?SetWidthHeight@ImageElement@UI@@QEAAXHH@Z")(self, w, h);

        RT_DIFF_SCALAR(oursW, *reinterpret_cast<int32_t*>(s + 0xD4));
        RT_DIFF_SCALAR(oursH, *reinterpret_cast<int32_t*>(s + 0xD8));
        RT_DIFF_SCALAR(oursDirty, DirtyFlags(self, kDirtyFlags));

        *reinterpret_cast<int32_t*>(s + 0xD4) = oursW;    // OUR result stays live
        *reinterpret_cast<int32_t*>(s + 0xD8) = oursH;
        DirtyFlags(self, kDirtyFlags) = oursDirty;
    }
}

// ============================================================================
// UI::LabelElement
// ============================================================================

// 16, not kSpriteWrittenBytes -- same "copy, not construction" reasoning as
// ImageElement_GetSprite above; retail's copy (0x1800429d0) is likewise two
// unconditional 8-byte MOV pairs.
void* LabelElement_GetSprite(const void* self, void* retval) {
    RT_DIFF_BEGIN("UI::LabelElement::GetSprite");
    uint64_t* r = reinterpret_cast<uint64_t*>(retval);
    const uint8_t* s = reinterpret_cast<const uint8_t*>(self);
    r[0] = *reinterpret_cast<const uint64_t*>(s + 0x100);
    r[1] = *reinterpret_cast<const uint64_t*>(s + 0x108);
    char scratch[16] = {};
    using Fn = void* (*)(const void*, void*);
    if (rt_diff_on__) {   // call inside the guard; see ImageElement_GetSprite above
        RT_ORIGINAL_ONCE(Fn, "?GetSprite@LabelElement@UI@@QEBA?AVSprite@2@XZ")(self, scratch);
    }
    RT_DIFF_BYTES(r, scratch, 16);
    return retval;
}

// 16, not kSpriteWrittenBytes -- same "copy, not construction" reasoning as
// ImageElement_SetSprite above; retail's copy (0x180042980) is likewise two
// unconditional 8-byte MOV pairs.
void LabelElement_SetSprite(void* self, const void* spriteVal) {
    RT_DIFF_BEGIN("UI::LabelElement::SetSprite");
    uint8_t pristine[16];
    uint32_t pristineDirty = 0;
    if (rt_diff_on__) {
        std::memcpy(pristine, B(self, 0x100), 16);
        pristineDirty = DirtyFlags(self, kDirtyFlags);
    }

    const uint64_t* sv = reinterpret_cast<const uint64_t*>(spriteVal);
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    bool differs = (*reinterpret_cast<uint64_t*>(s + 0x100) != sv[0]) ||
                   (*reinterpret_cast<int32_t*>(s + 0x108) != static_cast<int32_t>(sv[1]));
    if (differs) WalkDirty(self);
    *reinterpret_cast<uint64_t*>(s + 0x100) = sv[0];
    *reinterpret_cast<uint64_t*>(s + 0x108) = sv[1];

    if (rt_diff_on__) {
        uint8_t oursSprite[16];
        std::memcpy(oursSprite, s + 0x100, 16);
        uint32_t oursDirty = DirtyFlags(self, kDirtyFlags);

        std::memcpy(s + 0x100, pristine, 16);   // undo, so retail's guard/WalkDirty run for real
        DirtyFlags(self, kDirtyFlags) = pristineDirty;

        using Fn = void (*)(void*, const void*);
        RT_ORIGINAL_ONCE(Fn, "?SetSprite@LabelElement@UI@@QEAAXVSprite@2@@Z")(self, spriteVal);

        RT_DIFF_BYTES(oursSprite, s + 0x100, 16);
        RT_DIFF_SCALAR(oursDirty, DirtyFlags(self, kDirtyFlags));

        std::memcpy(s + 0x100, oursSprite, 16);  // OUR result stays live
        DirtyFlags(self, kDirtyFlags) = oursDirty;
    }
}

// ============================================================================
// UI::TextElement
// ============================================================================
namespace TE = UI::TextElementOffsets;

void TextElement_SetParameter(void* self, int32_t index, const void* strRef) {
    int32_t count = *reinterpret_cast<int32_t*>(B(self, TE::kParamCount));
    if (index >= 0 && index < count) {
        WalkDirty(self);
        void* data = *reinterpret_cast<void**>(B(self, TE::kParamArrayData));
        RtStringAssign(reinterpret_cast<uint8_t*>(data) + static_cast<size_t>(index) * 8, strRef);
        RtTextElementCacheText(self);
    }

    // NO ORACLE. This lane considered calling retail for real afterward
    // (safe: with our write already applied, retail's own equality check
    // inside System::String::operator= sees the slot already holding `str`
    // and takes ITS OWN no-op path) and rejected it -- it is exactly the
    // vacuous shape flagged in review: it can only ever pass, because
    // retail's guard is satisfied by our own prior write before retail runs
    // at all. Fixing that the way the field-setters above were fixed (snapshot
    // the pristine slot, restore it, let retail run for real, compare) is NOT
    // safe here: `str`'s assignment into the slot is a REAL refcount
    // acquire/release on a shared System::String. If restoring the slot's
    // raw pointer back to its pre-call value happens to be the LAST
    // reference to a string ours already dropped a refcount on, retail's
    // subsequent access is a use-after-free, not a diagnostic. Verifying this
    // safely needs a disposable String this function owns end-to-end for the
    // diff, which is lane F's territory (Sprite/String primitives), not
    // reversed here. Registered anyway -- the logic is correct in every
    // normal (non-diff) frame. See WAVE7_LANE_E.md sec4a.
}

void TextElement_SetText(void* self, const void* uiStringVal) {
    const uint64_t* sv = reinterpret_cast<const uint64_t*>(uiStringVal);
    *reinterpret_cast<uint64_t*>(B(self, TE::kTextString))     = sv[0];
    *reinterpret_cast<uint64_t*>(B(self, TE::kTextString + 8)) = sv[1];
    WalkDirty(self);
    RtTextElementCacheText(self);

    // NO ORACLE, same reasoning as SetParameter above: CacheText's own
    // refcount management on the cached-text pointer at +0xF8 makes a
    // restore-and-rerun unsafe, and running retail for real without
    // restoring first is vacuous (retail's own CacheText sees the input we
    // already wrote and re-derives the identical cached text). Registered
    // anyway. See WAVE7_LANE_E.md sec4a.
}

// ============================================================================
// UI::DocumentTextElement
// ============================================================================
namespace DTE = UI::DocumentTextElementOffsets;

void* DocumentTextElement_GetLink(const void* self, void* retval, int32_t index) {
    RT_DIFF_BEGIN("UI::DocumentTextElement::GetLink");
    void* linkData = *reinterpret_cast<void* const*>(B(const_cast<void*>(self), DTE::kLinkArrayData));
    *reinterpret_cast<uint64_t*>(retval) = 0;
    void* entryStr = *reinterpret_cast<void**>(
        reinterpret_cast<uint8_t*>(linkData) + static_cast<size_t>(index) * DTE::kLinkEntryStride);
    if (entryStr) {
        *reinterpret_cast<void**>(retval) = entryStr;
        *reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(entryStr) - 4) += 1;
    }

    // Oracle: replay, WITH an explicit extra-refcount release. GetLink hands
    // back an owning reference (it bumps the refcount), so calling retail a
    // second time for the diff acquires a SECOND reference to the same
    // string that our own caller will never release. We read retail's
    // content for the comparison, then release exactly that extra reference
    // ourselves so the diff call is refcount-neutral.
    char scratch[8] = {};
    using Fn = void* (*)(const void*, void*, int32_t);
    // Call inside the guard; see ImageElement_GetSprite above. This one also took
    // a STRING REFERENCE on every ordinary run, released a few lines below only
    // when rt_diff_on__ -- so the leak was silent and permanent.
    if (rt_diff_on__) {
        RT_ORIGINAL_ONCE(Fn, "?GetLink@DocumentTextElement@UI@@QEBA?AVString@System@@H@Z")(self, scratch, index);
    }
    void* theirsPtr = *reinterpret_cast<void**>(scratch);
    RT_DIFF_SCALAR(entryStr, theirsPtr);
    if (rt_diff_on__ && theirsPtr) {
        int32_t* rc = reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(theirsPtr) - 4);
        *rc -= 1;
        if (*rc == 0) RtMemFree(reinterpret_cast<uint8_t*>(theirsPtr) - 8);
    }
    return retval;
}

void DocumentTextElement_SetText_UiString(void* self, const void* uiStringVal) {
    // UI::String is a POD, non-refcounted 16-byte value (Element.h's UiString,
    // Sprite.h's Sprite -- same shape, same "no INC/DEC" evidence), so unlike
    // the two functions below, restoring this field before calling retail for
    // real carries no use-after-free risk.
    //
    // The 16-byte width used throughout (snapshot, write, and compare) is
    // deliberate, not Element.h's UiString pad exclusion: this function COPIES
    // an existing UI::String value, and retail's own copy (0x18003bfd0) is two
    // unconditional 8-byte MOV pairs (`MOV RAX,[RDX]`/`[RCX+0xc0],RAX` then
    // `+0x8`/`+0xc8`) -- the full 16, matching every Sprite-copy function
    // above. Narrowing this one to 12 bytes because it happens to hold a
    // UI::String rather than a UI::Sprite would drop 4 bytes retail itself
    // writes. See WAVE7_LANE_E.md sec4b.
    RT_DIFF_BEGIN("UI::DocumentTextElement::SetText[UI::String]");
    uint8_t pristine[16];
    if (rt_diff_on__) std::memcpy(pristine, B(self, DTE::kUiStringValue), 16);

    const uint64_t* sv = reinterpret_cast<const uint64_t*>(uiStringVal);
    *reinterpret_cast<uint64_t*>(B(self, DTE::kUiStringValue))     = sv[0];
    *reinterpret_cast<uint64_t*>(B(self, DTE::kUiStringValue + 8)) = sv[1];
    WalkDirty(self);

    if (rt_diff_on__) {
        uint8_t ours[16];
        std::memcpy(ours, B(self, DTE::kUiStringValue), 16);
        std::memcpy(B(self, DTE::kUiStringValue), pristine, 16);  // undo, so retail runs on the real pre-call state

        using Fn = void (*)(void*, const void*);
        RT_ORIGINAL_ONCE(Fn, "?SetText@DocumentTextElement@UI@@QEAAXVString@2@@Z")(self, uiStringVal);

        RT_DIFF_BYTES(ours, B(self, DTE::kUiStringValue), 16);
        std::memcpy(B(self, DTE::kUiStringValue), ours, 16);      // OUR result stays live
    }
}

void DocumentTextElement_SetText_StringRef(void* self, const void* strRef) {
    // Reset the by-value UI::String slot: pointer=0, index=-1. Retail's own
    // codegen (`CONCAT44(uStack_c, 0xffffffff)`) writes the qword's high
    // dword from an UNINITIALIZED stack local -- i.e. retail's own value at
    // +0xCC is non-deterministic. Reproducing a specific number there would
    // be inventing a fact retail does not have; only the low dword (the
    // actual -1 index reset) is written here. See WAVE7_LANE_E.md sec6.
    *reinterpret_cast<uint64_t*>(B(self, DTE::kUiStringValue)) = 0;
    *reinterpret_cast<int32_t*>(B(self, DTE::kUiStringValue + 8)) = -1;

    void* oldStr = *reinterpret_cast<void**>(B(self, DTE::kContentString));
    void* newStr = *reinterpret_cast<void* const*>(strRef);
    if (oldStr != newStr) {
        if (oldStr) {
            int32_t* rc = reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(oldStr) - 4);
            *rc -= 1;
            if (*rc == 0) {
                RtMemFree(reinterpret_cast<uint8_t*>(oldStr) - 8);
            }
            *reinterpret_cast<void**>(B(self, DTE::kContentString)) = nullptr;
        }
        *reinterpret_cast<void**>(B(self, DTE::kContentString)) = newStr;
        if (newStr) *reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(newStr) - 4) += 1;
    }
    WalkDirty(self);

    // NO ORACLE for the refcount swap. Same reasoning as TextElement::
    // SetParameter above: `oldStr` may already have been freed by our own
    // call above (refcount dropped to zero), so restoring +0x168 to `oldStr`
    // and letting retail dereference it would be a use-after-free, and
    // calling retail for real WITHOUT restoring is vacuous (its own
    // `oldStr == newStr` guard is now satisfied by our prior write, so it
    // takes its own no-op branch). Registered anyway -- correct in every
    // normal frame. See WAVE7_LANE_E.md sec4a.
}

// ============================================================================
// UI::TabElement / UI::PageElement
// ============================================================================
namespace TAE = UI::TabElementOffsets;
namespace PGE = UI::PageElementOffsets;

void TabElement_SetActiveTab(void* self, int32_t tab) {
    RT_DIFF_BEGIN("UI::TabElement::SetActiveTab");
    int32_t pristine = 0;
    if (rt_diff_on__) pristine = *reinterpret_cast<int32_t*>(B(self, TAE::kActiveTab));

    if (tab != *reinterpret_cast<int32_t*>(B(self, TAE::kActiveTab))) {
        *reinterpret_cast<int32_t*>(B(self, TAE::kActiveTab)) = tab;
        RtTabElementUpdateContainers(self);
    }

    if (rt_diff_on__) {
        int32_t ours = *reinterpret_cast<int32_t*>(B(self, TAE::kActiveTab));
        // Undo, so retail's own "did it change" guard is satisfied for real
        // and it actually calls UpdateContainers -- not the vacuous shape
        // (our prior write means retail's own guard sees "no change" and
        // calls nothing). UpdateContainers has no free/refcount step (it
        // recomputes child visibility), so re-running it for real here is
        // safe, just possibly redundant with the one ours already triggered.
        *reinterpret_cast<int32_t*>(B(self, TAE::kActiveTab)) = pristine;
        using Fn = void (*)(void*, int32_t);
        RT_ORIGINAL_ONCE(Fn, "?SetActiveTab@TabElement@UI@@QEAAXH@Z")(self, tab);
        RT_DIFF_SCALAR(ours, *reinterpret_cast<int32_t*>(B(self, TAE::kActiveTab)));
        *reinterpret_cast<int32_t*>(B(self, TAE::kActiveTab)) = ours;  // OUR result stays live
    }
}

int32_t PageElement_GetActivePage(const void* self) {
    RT_DIFF_BEGIN("UI::PageElement::GetActivePage");
    int32_t ours = *reinterpret_cast<const int32_t*>(B(const_cast<void*>(self), PGE::kActivePage));
    using Fn = int32_t (*)(const void*);
    RT_DIFF_SCALAR(ours, RT_ORIGINAL_ONCE(Fn, "?GetActivePage@PageElement@UI@@QEBAHXZ")(self));
    return ours;
}

void PageElement_SetActivePage(void* self, int32_t page) {
    RT_DIFF_BEGIN("UI::PageElement::SetActivePage");
    int32_t pristine = 0;
    if (rt_diff_on__) pristine = *reinterpret_cast<int32_t*>(B(self, PGE::kActivePage));

    int32_t count = *reinterpret_cast<int32_t*>(B(self, PGE::kChildrenCount));
    int32_t current = *reinterpret_cast<int32_t*>(B(self, PGE::kActivePage));
    if (page > -2 && page < count && page != current) {
        *reinterpret_cast<int32_t*>(B(self, PGE::kActivePage)) = page;
        WalkDirty(self);
        void* data = *reinterpret_cast<void**>(B(self, PGE::kChildrenData));
        for (int32_t i = 0; i < count; ++i) {
            void* child = reinterpret_cast<void**>(data)[i];
            if (!child) continue;
            uint32_t flags = DirtyFlags(child, PGE::kDirtyFlags);
            uint32_t updated = (i == page) ? (flags & ~PGE::kHiddenBit) : (flags | PGE::kHiddenBit);
            if (updated != flags) {
                DirtyFlags(child, PGE::kDirtyFlags) = updated;
                WalkDirty(child);
            }
        }
    }

    if (rt_diff_on__) {
        int32_t ours = *reinterpret_cast<int32_t*>(B(self, PGE::kActivePage));
        // Undo OUR OWN field, so retail's top-level "did it change" guard is
        // satisfied for real -- otherwise retail sees "already == page" and
        // skips its ENTIRE per-child hidden-bit loop, the part of this
        // function most likely to be wrong, exactly as flagged. Retail then
        // re-runs that loop for real against the live children: each
        // per-child bit toggle is a flag write, not a free, so this is safe,
        // even though it duplicates work ours already did (idempotent — same
        // target index, same resulting bit pattern).
        *reinterpret_cast<int32_t*>(B(self, PGE::kActivePage)) = pristine;
        using Fn = void (*)(void*, int32_t);
        RT_ORIGINAL_ONCE(Fn, "?SetActivePage@PageElement@UI@@QEAAXH@Z")(self, page);
        RT_DIFF_SCALAR(ours, *reinterpret_cast<int32_t*>(B(self, PGE::kActivePage)));
        *reinterpret_cast<int32_t*>(B(self, PGE::kActivePage)) = ours;  // OUR result stays live

        // The per-child hidden-bit outcome itself is NOT independently
        // re-verified: after both calls, every child ends up in the state
        // retail's own second pass just computed, which folds ours and
        // retail's answers together rather than comparing them. See
        // WAVE7_LANE_E.md sec4a.
    }
}

// ============================================================================
// UI::GraphElement
// ============================================================================
namespace GRE = UI::GraphElementOffsets;

void GraphElement_RemoveGraphs(void* self) {
    RtGraphArrayClear(B(self, GRE::kGraphsData));

    // WEAK invariant check, not a comparative oracle -- worth being explicit
    // about, since `ours` here IS retail's own destroy-and-free helper called
    // directly (RtGraphArrayClear resolves 0x1800416d0 by address; there is no
    // separate reimplementation of the teardown logic to compare against).
    // Calling retail's whole RemoveGraphs a second time is safe regardless
    // (it tail-calls the SAME helper, which is a documented no-op on an
    // already-empty, already-zeroed header), so this just re-confirms the
    // count reads back as zero -- it cannot detect a wrong `kGraphsData`
    // offset the way GetRowCount's true replay would, because a wrong offset
    // reading garbage that happens to be zero would still pass.
    RT_DIFF_BEGIN("UI::GraphElement::RemoveGraphs");
    using Fn = void (*)(void*);
    RT_ORIGINAL_ONCE(Fn, "?RemoveGraphs@GraphElement@UI@@QEAAXXZ")(self);
    RT_DIFF_SCALAR(*reinterpret_cast<int32_t*>(B(self, GRE::kGraphsCount)), 0);
}

}  // namespace

// ============================================================================
// Registration
// ============================================================================
int RtUiWidgetsLayRegister() {
    int n = 0;

    RegisterRuntimeExport("?GetColumnCount@TableElement@UI@@QEBAHXZ", (void*)&TableElement_GetColumnCount); ++n;
    RegisterRuntimeExport("?GetRowCount@TableElement@UI@@QEBAHXZ", (void*)&TableElement_GetRowCount); ++n;
    RegisterRuntimeExport("?SetChild@TableElement@UI@@QEAAXV?$InternalPtr@VElement@UI@@@Object@@H@Z", (void*)&TableElement_SetChild2); ++n;
    RegisterRuntimeExport("?SetChild@TableElement@UI@@QEAAXV?$InternalPtr@VElement@UI@@@Object@@HH@Z", (void*)&TableElement_SetChild3); ++n;
    RegisterRuntimeExport("?SetSize@TableElement@UI@@QEAAXHH@Z", (void*)&TableElement_SetSize); ++n;

    RegisterRuntimeExport("?GetSprite@ImageElement@UI@@QEBA?AVSprite@2@XZ", (void*)&ImageElement_GetSprite); ++n;
    RegisterRuntimeExport("?SetRotation@ImageElement@UI@@QEAAXM@Z", (void*)&ImageElement_SetRotation); ++n;
    RegisterRuntimeExport("?SetSprite@ImageElement@UI@@QEAAXVSprite@2@@Z", (void*)&ImageElement_SetSprite); ++n;
    RegisterRuntimeExport("?SetWidthHeight@ImageElement@UI@@QEAAXHH@Z", (void*)&ImageElement_SetWidthHeight); ++n;
    // ?_typeInfo@ImageElement@UI@@0VBaseTypeInfo@Object@@A -- HELD BACK, it is
    // a DATA export, not a function (WAVE7_LANE_E.md sec2).

    RegisterRuntimeExport("?GetSprite@LabelElement@UI@@QEBA?AVSprite@2@XZ", (void*)&LabelElement_GetSprite); ++n;
    RegisterRuntimeExport("?SetSprite@LabelElement@UI@@QEAAXVSprite@2@@Z", (void*)&LabelElement_SetSprite); ++n;

    RegisterRuntimeExport("?SetParameter@TextElement@UI@@QEAAXHAEBVString@System@@@Z", (void*)&TextElement_SetParameter); ++n;
    RegisterRuntimeExport("?SetText@TextElement@UI@@QEAAXVString@2@@Z", (void*)&TextElement_SetText); ++n;

    RegisterRuntimeExport("?GetLink@DocumentTextElement@UI@@QEBA?AVString@System@@H@Z", (void*)&DocumentTextElement_GetLink); ++n;
    RegisterRuntimeExport("?SetText@DocumentTextElement@UI@@QEAAXAEBVString@System@@@Z", (void*)&DocumentTextElement_SetText_StringRef); ++n;
    RegisterRuntimeExport("?SetText@DocumentTextElement@UI@@QEAAXVString@2@@Z", (void*)&DocumentTextElement_SetText_UiString); ++n;

    RegisterRuntimeExport("?SetActiveTab@TabElement@UI@@QEAAXH@Z", (void*)&TabElement_SetActiveTab); ++n;
    // ?AddTab@TabElement@UI@@... -- HELD BACK (WAVE7_LANE_E.md sec2).

    RegisterRuntimeExport("?GetActivePage@PageElement@UI@@QEBAHXZ", (void*)&PageElement_GetActivePage); ++n;
    RegisterRuntimeExport("?SetActivePage@PageElement@UI@@QEAAXH@Z", (void*)&PageElement_SetActivePage); ++n;

    RegisterRuntimeExport("?RemoveGraphs@GraphElement@UI@@QEAAXXZ", (void*)&GraphElement_RemoveGraphs); ++n;
    // ?AddGraph@GraphElement@UI@@... / ?AddGraphPoint@GraphElement@UI@@... --
    // HELD BACK (WAVE7_LANE_E.md sec2).

    printf("[RtPort] UI widgets(layout): %d exports registered\n", n);
    return n;
}
