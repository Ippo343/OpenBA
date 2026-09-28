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
//
// ENDGAME B.3 step 6, FRAGMENT PASS F1 (2026-09-25): every body below now
// lives in its class's MEMBER — `Element::RemoveChildren` and the eight leaf
// classes' non-virtual members declared in runtime/types/ui/{Element,
// TableElement,ImageElement,LabelElement,TextElement,DocumentTextElement,
// TabElement,PageElement,GraphElement}.h — and reaches the fields by name
// instead of by raw offset. The registered doors keep their names and are
// one-line QUALIFIED delegations at the bottom of the file. The retslot doors
// (`GetSprite` ×2, `GetLink`) keep retail's `(this, ret, …)` shape and
// placement-new the member's prvalue; the by-value 16-byte `UI::Sprite` /
// `UI::String` and the `InternalPtr<Element>` arguments arrive by HIDDEN
// POINTER (RULE B6-11), so those members take the pointer.
// ============================================================================

#include <atomic>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <new>

#include "../../funcs/rt_port.h"
#include "../../types/system/String.h"
#include "../../funcs/rt_sink.h"
#include "../../types/ui/Element.h"
#include "../../types/object/VSlot.h"   // B3X W0: Object::SlotAddressOf for the recorder
#include "../../types/ui/TableElement.h"
#include "../../types/ui/ImageElement.h"
#include "../../types/ui/LabelElement.h"
#include "../../types/ui/TextElement.h"
#include "../../types/ui/DocumentTextElement.h"
#include "../../types/ui/TabElement.h"
#include "../../types/ui/PageElement.h"
#include "../../types/ui/GraphElement.h"
#include "../collection/array_internal.h" // ENDGAME B.3.2 step 2 — kSpec_I32 etc.
// B6-5 door caches (AtomicAddr / AtomicExport) and the ResolveAddr
// declaration this file used to spell for itself.
#include "element_common.h"

namespace {

inline uint8_t* B(void* p, int off) { return reinterpret_cast<uint8_t*>(p) + off; }

// Every mutator in this lane, having changed its own state, walks `this` then
// its parent chain OR-ing this bit into the dirty-flags word at +0x34. This
// lane found the field independently (eight bodies: TableElement::SetSize,
// ImageElement::SetSprite/SetWidthHeight, TextElement::SetParameter/SetText,
// DocumentTextElement::SetText x2, PageElement::SetActivePage) before lane A's
// Element.h landed; it is UI::Element::_flags / kElementFlagNeedsSizeCalc and
// UI::Element::_parent.
inline void WalkDirty(UI::Element* self) {
    for (UI::Element* node = self; node; node = node->_parent)
        node->_flags |= UI::kElementFlagNeedsSizeCalc;
}

// ---- Helpers this lane calls through rather than reimplements --------------
//
// ?RemoveChildren@Element@UI@@QEAAXXZ (0x180115570) is `UI::Element::
// RemoveChildren`, defined near the end of this file; TableElement::SetSize
// calls it as a member.
//
// ?CacheText@TextElement@UI@@AEAAXXZ (0x18011ec10) is `UI::TextElement::
// CacheText` (textelement_cachetext.cpp); called as a member.
//
// ?UpdateContainers@TabElement@UI@@AEAAXXZ (0x1800d9710) is `UI::TabElement::
// UpdateContainers` (tabelement.cpp); called as a member.

// ??4String@System@@QEAAAEAV01@AEBV01@@Z -- System::String::operator=.
//
// B.3.1 bridge: the destination is a slot in the untyped array storage
// TextElement keeps its parameters in, so the eight bytes are cast here at the
// one use site; the cast dies when Collection::Array<T> is a template.
void RtStringAssign(void* self, const void* other) {
    *reinterpret_cast<System::String*>(self) =
        *reinterpret_cast<const System::String*>(other);
}

// RULE B6-5: the namespace-scope door caches the member bodies read through.
std::atomic<void*> g_memFree{nullptr};          // ?MemFree@MemoryAllocator@System@@SAXPEAX@Z
std::atomic<void*> g_graphArrayClear{nullptr};  // 0x1800416d0

// ?MemFree@MemoryAllocator@System@@SAXPEAX@Z
void RtMemFree(void* p) {
    using Fn = void (*)(void*);
    reinterpret_cast<Fn>(rtuielem::AtomicExport(
        g_memFree, "?MemFree@MemoryAllocator@System@@SAXPEAX@Z"))(p);
}

// ENDGAME B.3.2 step 2 — the `RtArrayInt32SetSize`/`RtArrayCellsSetSize`
// forwarders that used to sit here are GONE. `TableElement`'s row/col-size
// arrays and `Element::_children` bridge through their row with
// `rtcoll::AsArray`:
//
//   0x18002fea0  kSpec_I32      TRAIT-EQUIVALENT for int32_t (tail left
//                UNINITIALISED, matching `SpecFor<int32_t>()`'s empty `init`)
//   0x1800764c0  kSpec_PtrZero  NOT trait-equivalent: its `InitZero8` ZEROES
//                each new tail slot, which a trait-derived `Array<Element*>`
//                (a pointer is trivially default-constructible, so the trait
//                leaves the tail UNINITIALISED) would not do.
inline void RtArrayInt32SetSize(void* arrayHeader, int32_t n) {
    rtcoll::AsArray<&rtcoll::kSpec_I32>(arrayHeader)->SetSize(n);
}
inline void RtArrayCellsSetSize(void* arrayHeader, int32_t n) {
    rtcoll::AsArray<&rtcoll::kSpec_PtrZero>(arrayHeader)->SetSize(n);
}

// ENDGAME B.3.2 step 2 — left as a free-function call, deliberately.
// 0x1800416d0 is array_destroy.cpp's J2_ENTRY driving `FreeInnerArraysCounted`
// off a `DestroySpec` (`kD_InnerArr_18`), a different type/dispatch from the
// `rtcoll::ArraySpec` a `Collection::Array<T>::~Array()` would call, and no
// `kSpec_*` row exists for it — per the batch rule, a "DtorElem" helper may
// not be convertible at all. Retry a miss rather than caching it.
void RtGraphArrayClear(void* arrayHeader) {
    using Fn = void (*)(void*);
    reinterpret_cast<Fn>(rtuielem::AtomicAddr(g_graphArrayClear, 0x1800416d0))(arrayHeader);
}

namespace TAE = UI::TabElementOffsets;

// ---- ?UpdateContainers@TabElement@UI@@AEAAXXZ  0x1800d9710 -----------------
//
// WAVE 10. Lane E declined this in Wave 7 ("visibility recomputation, not
// reversed by this lane") and lane V2 confirmed it a REAL DEPENDENCY. It is
// ported here, written against the live sink (runtime/funcs/rt_sink.h).
//
// WHY THE REPLAY ORACLE CANNOT DO IT -- two independent reasons:
//
//  1. The mutated state is a GRAPH. The dirty bit propagates from each child
//     up its _parent (+0x98) chain, through objects that are not inside any
//     copyable extent of `this`. There is no scratch to run retail against --
//     the same rock SetUpperLeft was held back on (WAVE7_LANE_B.md sec2b).
//
//  2. The propagation is GUARDED BY "did bit 0 actually change" (0x1800d9760
//     and 0x1800d97cc). Under replay our body has already flipped it, so
//     retail's guard is false and it never walks the chain -- the compare goes
//     green having tested nothing. Lane C named this exact shape on
//     EnableCursor: "the object compare would pass while the only thing that
//     can go wrong went untested."
//
// ---- Layout facts, all from the disassembly at 0x1800d9710 ----------------
//
//   this+0xc0   int32   tab count            (loop bound, 0x1800d971f/0x1800d97ed)
//   this+0xf8   int32   active tab index     (0x1800d9750)
//   this+0xa0   ptr     Element::_children._data
//   this+0xc4   int32   inset used by the resize call (0x1800d9786)
//   this+0xfc   int32   a second inset, doubled (0x1800d979b, LEA +RDX*2)
//   this+0x68/0x6c/0x70/0x74  int32  the element's own x/y/w/h
//   child+0x34  uint32  Element::_flags   (bit 0 = hidden; 0x8000000 = dirty)
//   child+0x98  ptr     Element::_parent
//
// THE ADDRESSING IS FAITHFUL, AND ITS MEANING IS NOT ESTABLISHED HERE.
// Retail indexes the children array at BYTE offset 0x20 + i*0x10 (RSI starts
// at 0x20 at 0x1800d9730 and adds 0x10 at 0x1800d97e9), i.e. _data[4 + 2*i]
// for an Array<Element*>. Why the containers start at index 4 and sit two
// apart is AddTab's business (0x1800da130, still held back) and is NOT
// claimed here. The port reproduces the addressing, not an interpretation of
// it -- and if the addressing is wrong, the oracle's `unpredicted=` column is
// what says so, because retail's real writes would then land where we
// predicted nothing.
//
// One ordering detail a "tidied" port would drop: in the propagate branch
// retail RE-READS child+0x34 after storing the new flag word, so the child
// ends at (nf | 0x8000000), not (old | 0x8000000). The predictor gets this
// right only because it reads back through the sink (RtSink::ReadU32).

static void TabElement_UpdateContainers_Body(void* self, RtSink& out) {
    const int32_t tabCount = *reinterpret_cast<int32_t*>(B(self, 0xc0));
    if (tabCount <= 0) return;

    const int32_t active = *reinterpret_cast<int32_t*>(B(self, TAE::kActiveTab));
    uint8_t* const data  = *reinterpret_cast<uint8_t**>(B(self, TAE::kChildrenData));

    for (int32_t i = 0; i < tabCount; i++) {
        uint8_t* const child =
            *reinterpret_cast<uint8_t**>(data + 0x20 + static_cast<size_t>(i) * 0x10);

        const uint32_t old = out.ReadU32(child + 0x34);
        const uint32_t nf  = (i == active) ? (old & 0xfffffffeu)   // show
                                           : (old | 1u);           // hide
        out.U32(child + 0x34, nf, "child._flags");

        // Guarded dirty propagation, starting AT THE CHILD (0x1800d9770 ORs
        // [RCX+0x34] before RCX walks to +0x98).
        if (((nf ^ old) & 1u) != 0) {
            for (uint8_t* p = child; p != nullptr;
                 p = *reinterpret_cast<uint8_t**>(p + 0x98)) {
                out.U32(p + 0x34, out.ReadU32(p + 0x34) | 0x8000000u,
                        "ancestor._flags |= dirty");
            }
        }

        // The active tab's container is resized to fill the tab body. This is
        // an indirect call through the child's own vtable slot +0x28; in
        // predict mode RtSink::Call suppresses it and the caller declares the
        // footprint it would have written with Exclude().
        if (i == active) {
            const int32_t pad = *reinterpret_cast<int32_t*>(B(self, 0xc4));
            const int32_t a1  = pad + *reinterpret_cast<int32_t*>(B(self, 0x68));
            const int32_t a2  = *reinterpret_cast<int32_t*>(B(self, 0xfc))
                              + pad * 2
                              + *reinterpret_cast<int32_t*>(B(self, 0x6c));
            const int32_t a3  = *reinterpret_cast<int32_t*>(B(self, 0x70)) - pad;
            const int32_t a4  = *reinterpret_cast<int32_t*>(B(self, 0x74)) - pad;

            // The recorder is handed the slot's address, taken from the member
            // (never a literal index); the call itself is by name.
            const void* const slot =
                Object::SlotAddressOf(child, &UI::Element::ComputeLayout);
            if (out.Call("Element::ComputeLayout (resize container)",
                         slot, a1, a2, a3, a4)) {
                reinterpret_cast<UI::Element*>(child)->ComputeLayout(a1, a2, a3, a4);
            }
        }
    }
}

// The body that lived here was a second copy of ?UpdateContainers@TabElement@UI@@
// (0x1800d9710); runtime/src/ui/tabelement.cpp owns it since vtable batch 4
// (2026-09-12) as `UI::TabElement::UpdateContainers`, registered with both
// doors there.

}  // namespace

namespace UI {

// ============================================================================
// UI::TableElement
// ============================================================================
// GetRowCount / GetColumnCount are one instruction each and live inline in
// TableElement.h; their doors are below.

// ?SetChild@TableElement@UI@@QEAAXV?$InternalPtr@VElement@UI@@@Object@@H@Z
// 0x18011daa0 — the handle arrives as the ADDRESS of the caller's copy.
void TableElement::SetChild(Object::InternalPtr<Element>* child, int32_t index) {
    Element* elem = child->Get();
    if (elem) elem->_parent = this;

    Element** cellSlot = _children._data + index;
    Element* old = *cellSlot;
    if (old != elem) {
        if (old) {
            // Slot 0 with flag 1 — the scalar deleting destructor, the
            // InternalPtr<T> "release and possibly delete" idiom
            // (runtime/types/object/Internal.h's kInternalVtblScalarDtor).
            old->DeletingDtor(1);
            *cellSlot = nullptr;
        }
        WalkDirty(this);
    }
    *cellSlot = elem;

    // NO ORACLE for the branch that matters: the release path destroys the
    // previous occupant, and there is no safe way to re-exercise it against a
    // second implementation. See WAVE7_LANE_E.md sec4a.
}

// ?SetChild@TableElement@UI@@QEAAXV?$InternalPtr@VElement@UI@@@Object@@HH@Z
// 0x18011db50 — pure delegate to the overload above, `_colCount*col + row`.
void TableElement::SetChild(Object::InternalPtr<Element>* child, int32_t column,
                            int32_t row) {
    SetChild(child, _columnCount * column + row);
}

// ?SetSize@TableElement@UI@@QEAAXHH@Z  0x18011db90
void TableElement::SetSize(int32_t rows, int32_t columns) {
    _rowCount = rows;
    _columnCount = columns;
    RtArrayInt32SetSize(&_rowSizes, rows);
    RtArrayInt32SetSize(&_columnSizes, columns);
    RemoveChildren();
    RtArrayCellsSetSize(&_children, rows * columns);
    WalkDirty(this);
}

// ============================================================================
// UI::ImageElement
// ============================================================================

// 16, not UI::Sprite.h's kSpriteWrittenBytes (0x0C): that constant bounds what
// a Sprite CONSTRUCTOR writes, and this function is not one -- it is a COPY,
// proven at the instruction level (0x1801182a0: two independent 8-byte
// `MOV RAX,[..]`/`MOV [..],RAX` pairs, not a 12-byte-then-stop sequence). The
// implicit copy of `UI::Sprite` moves all four fields, pad0C included.
Sprite ImageElement::GetSprite() const {
    return _sprite;
}

// ?SetRotation@ImageElement@UI@@QEAAXM@Z  0x1801182c0
void ImageElement::SetRotation(float degrees) {
    _rotation = degrees;
    // No dirty-flag walk in retail -- confirmed from the disassembly, not
    // assumed by analogy with the other setters in this file.
}

// ?SetSprite@ImageElement@UI@@QEAAXVSprite@2@@Z  0x180118600. `VSprite@2@`
// BY VALUE is a 16-byte class, which retail passes by HIDDEN POINTER (RULE
// B6-11). 16 bytes throughout: this function COPIES an existing Sprite value
// rather than constructing one, and retail's own copy is two unconditional
// 8-byte `MOV RAX,[..]`/`MOV [..],RAX` pairs -- the full 16, not a 12-then-stop.
void ImageElement::SetSprite(const Sprite* sprite) {
    const bool differs = (_sprite.spriteSheet != sprite->spriteSheet) ||
                         (_sprite.spriteIndex != sprite->spriteIndex);
    if (differs) WalkDirty(this);
    _sprite = *sprite;
}

// ?SetWidthHeight@ImageElement@UI@@QEAAXHH@Z  0x180118260
void ImageElement::SetWidthHeight(int32_t width, int32_t height) {
    const bool differs = (_width != width) || (_height != height);
    if (differs) {
        _width = width;
        _height = height;
        WalkDirty(this);
    }
}

// ============================================================================
// UI::LabelElement
// ============================================================================

// 16, not kSpriteWrittenBytes -- same "copy, not construction" reasoning as
// ImageElement::GetSprite above; retail's copy (0x1800429d0) is likewise two
// unconditional 8-byte MOV pairs.
Sprite LabelElement::GetSprite() const {
    return _sprite;
}

// 16, not kSpriteWrittenBytes -- same "copy, not construction" reasoning as
// ImageElement::SetSprite above; retail's copy (0x180042980) is likewise two
// unconditional 8-byte MOV pairs. The Sprite arrives by hidden pointer.
void LabelElement::SetSprite(const Sprite* sprite) {
    const bool differs = (_sprite.spriteSheet != sprite->spriteSheet) ||
                         (_sprite.spriteIndex != sprite->spriteIndex);
    if (differs) WalkDirty(this);
    _sprite = *sprite;
}

// ============================================================================
// UI::TextElement
// ============================================================================

// ?SetParameter@TextElement@UI@@QEAAXHAEBVString@System@@@Z  0x18011e9b0
//
// NO ORACLE. `str`'s assignment into the slot is a REAL refcount
// acquire/release on a shared System::String, so restoring the slot and
// re-running retail could be a use-after-free rather than a diagnostic, and
// running retail after ours without restoring is vacuous. See
// WAVE7_LANE_E.md sec4a.
void TextElement::SetParameter(int32_t index, const System::String& value) {
    if (index >= 0 && index < _params._size) {
        WalkDirty(this);
        RtStringAssign(&_params._data[index], &value);
        CacheText();
    }
}

// ?SetText@TextElement@UI@@QEAAXVString@2@@Z  0x180042070. The 0x10-byte
// UI::String BY VALUE arrives by HIDDEN POINTER (RULE B6-11). NO ORACLE, same
// reasoning as SetParameter above (CacheText's refcount management on the
// cached-text pointer at +0xF8).
void TextElement::SetText(const String* text) {
    _text = *text;
    WalkDirty(this);
    CacheText();
}

// ============================================================================
// UI::DocumentTextElement
// ============================================================================

// ?GetLink@DocumentTextElement@UI@@QEBA?AVString@System@@H@Z  0x18003c0c0 —
// RETSLOT. The link array at +0x158 has 0x18-byte entries, the target String
// at +0x00; the result is that String with its reference bumped, or an empty
// String if the slot is null — exactly System::String's copy constructor
// (Adopt: null stays null, non-null gets `++refcount`). No bounds check on
// `index` (matches SpriteSheet::GetSpriteNameFromIndex's pattern).
System::String DocumentTextElement::GetLink(int32_t index) const {
    const Link* links = static_cast<const Link*>(_linksData);
    return *reinterpret_cast<const System::String*>(&links[index].text);
}

// ?SetText@DocumentTextElement@UI@@QEAAXVString@2@@Z  0x18003bfd0. UI::String
// is a POD, non-refcounted 16-byte value, passed by HIDDEN POINTER (RULE
// B6-11). Retail's own copy is two unconditional 8-byte MOV pairs
// (`MOV RAX,[RDX]`/`[RCX+0xc0],RAX` then `+0x8`/`+0xc8`) -- the full 16.
void DocumentTextElement::SetText(const UI::String* text) {
    _text = *text;
    WalkDirty(this);
}

// ?SetText@DocumentTextElement@UI@@QEAAXAEBVString@System@@@Z  0x18003c010
void DocumentTextElement::SetText(const System::String& text) {
    // Reset the by-value UI::String slot: table=0, index=-1. Retail's own
    // codegen (`CONCAT44(uStack_c, 0xffffffff)`) writes the qword's high
    // dword from an UNINITIALIZED stack local -- i.e. retail's own value at
    // +0xCC is non-deterministic. Reproducing a specific number there would
    // be inventing a fact retail does not have; only the low dword (the
    // actual -1 index reset) is written here. See WAVE7_LANE_E.md sec6.
    _text.table = nullptr;
    _text.index = -1;

    char* oldStr = _sourceText;
    char* newStr = text._data;
    if (oldStr != newStr) {
        if (oldStr) {
            int32_t* rc = reinterpret_cast<int32_t*>(oldStr - 4);
            *rc -= 1;
            if (*rc == 0) {
                RtMemFree(oldStr - 8);
            }
            _sourceText = nullptr;
        }
        _sourceText = newStr;
        if (newStr) *reinterpret_cast<int32_t*>(newStr - 4) += 1;
    }
    WalkDirty(this);

    // NO ORACLE for the refcount swap (see TextElement::SetParameter above).
}

// ============================================================================
// UI::TabElement / UI::PageElement
// ============================================================================

// ?SetActiveTab@TabElement@UI@@QEAAXH@Z  0x18004c890 — guard, store, then the
// private (exported) UpdateContainers.
void TabElement::SetActiveTab(int32_t tab) {
    if (tab != _activeTab) {
        _activeTab = tab;
        UpdateContainers();
    }
}

// ?SetActivePage@PageElement@UI@@QEAAXH@Z  0x1800d5110
void PageElement::SetActivePage(int32_t page) {
    const int32_t count = _children._size;
    const int32_t current = _activePage;
    if (page > -2 && page < count && page != current) {
        _activePage = page;
        WalkDirty(this);
        Element** data = _children._data;
        for (int32_t i = 0; i < count; ++i) {
            Element* child = data[i];
            if (!child) continue;
            const uint32_t flags = child->_flags;
            const uint32_t updated = (i == page) ? (flags & ~kElementFlagHidden)
                                                 : (flags | kElementFlagHidden);
            if (updated != flags) {
                child->_flags = updated;
                WalkDirty(child);
            }
        }
    }
}

// ============================================================================
// UI::GraphElement
// ============================================================================

// ?RemoveGraphs@GraphElement@UI@@QEAAXXZ  0x180040610 — tail-calls the
// destroy-and-free helper on the series array.
void GraphElement::RemoveGraphs() {
    RtGraphArrayClear(&_seriesData);
}

// ============================================================================
// ?AddGraphPoint@GraphElement@UI@@QEAAXHH@Z  retail 0x180040690  ord 1561
//                                                          (WAVE 9 LANE UIF)
// ============================================================================
//
// THE BODY, 12 instructions at 0x180040690:
//
//     AddGraphPoint(this, int graph, int value):
//         [home slot] = value                 ; spilled so R8 can point AT it
//         RCX = this->[0xf0]                  ; the graph-series array's data
//         RAX = (int64)graph                  ; MOVSXD — SIGNED widening
//         RDX = RAX * 3
//         RCX = RCX + 8 + RDX*8               ; = data + graph*0x18 + 8
//         EDX = RCX->[8] + 1                  ; the point array's size, plus one
//         R8  = &value
//         JMP/CALL 0x180023830                ; Array<uint32>::SetSize(n, &fill)
//
// So a series record is 0x18 bytes (RDX = RAX*3 then scaled by 8) and the
// per-series point Array lives at record+8. `SetSize(size+1, &value)` is this
// engine's APPEND idiom: grow by one and fill the new tail from `*fill`.
//
// THE MOVSXD IS REPRODUCED AS A SIGNED WIDENING, deliberately (Ghidra's C
// prints it as a zero extension — reference_ghidra_c_output_vs_disassembly.md).
// A negative `graph` indexes backwards exactly as retail does. NOT guarded.
//
// ORACLE: none — a replay would append the point a SECOND time.
void GraphElement::AddGraphPoint(int32_t graph, int32_t value) {
    uint8_t* seriesData = static_cast<uint8_t*>(_seriesData);
    // record = data + graph*0x18; the point Array header sits at record + 8.
    uint8_t* pointArray = seriesData + static_cast<int64_t>(graph) * 0x18 + 8;

    // ENDGAME B.3.2 step 2 — `kSpec_Fill4` is trait-equivalent for `int32_t`,
    // so the bridge is a plain `Array<int32_t>` view of the nested header.
    auto* points = reinterpret_cast<Collection::Array<int32_t>*>(pointArray);
    points->SetSize(points->GetSize() + 1, value);
}

// ============================================================================
// ?AddGraph@GraphElement@UI@@QEAAXI@Z  retail 0x180040620  (WAVE 9 LANE UIW)
// ============================================================================
//
// THE BODY, 0x180040620..0x18004068b:
//
//     AddGraph(this, uint32 colour):
//         old = (int64)this->_graphs._size;      ; MOVSXD from [RCX+0xf8]
//         uint8_t tmp[0x18];                     ; at [RSP+0x28]
//         *(uint64*)(tmp+0x08) = 0;              ; the nested Array header,
//         *(uint64*)(tmp+0x10) = 0;              ;   zeroed; tmp+0..7 is NOT
//         0x1800417d0(&this->_graphs, _graphs._size + 1, tmp);
//         0x18001c750(tmp + 8);                  ; ~Array on the temp
//         this->_graphs._data[old].colour = colour;
//
// THREE THINGS WORTH THE INK:
//
//  1. **`tmp + 0x00 .. 0x07` IS DELIBERATELY UNINITIALISED.** Retail's fill
//     lives at `[RSP+0x28]` and only `[RSP+0x30]` and `[RSP+0x38]` are zeroed.
//     The LAST line of the function overwrites the copied colour with the real
//     `colour`, so nothing observable depends on those bytes.
//  2. **The temp's nested Array IS destroyed afterwards, and it is empty.** It
//     is reproduced because it is a real call retail makes.
//  3. **`old` IS TAKEN BEFORE THE APPEND AND SIGN-EXTENDED.** The final store
//     indexes `_data + old*0x18` with no bounds check, exactly as retail does.
//
// NO ORACLE, for the same reason as AddGraphPoint above.
void GraphElement::AddGraph(uint32_t colour) {
    const int64_t old = _seriesCount;                      // MOVSXD

    uint8_t tmp[0x18];
    // tmp[0x00..0x07] intentionally left uninitialised — see (1) above.
    *reinterpret_cast<uint64_t*>(tmp + 0x08) = 0;
    *reinterpret_cast<uint64_t*>(tmp + 0x10) = 0;

    // ENDGAME B.3.2 step 2 — `_seriesData` is GraphElement.h's raw
    // `Array<GraphSeries>` header (NON-TRIVIAL `kSpec_GraphSeries`), so it
    // bridges through the hand-derived row; the local `tmp+8` nested
    // `Array<int32_t>` is a real stack object, so its teardown is a plain
    // `.~Array()` (0x18001c750 is a J1, trait-equivalent for `int32_t`).
    rtcoll::AsArray<&rtcoll::kSpec_GraphSeries>(&_seriesData)
        ->SetSizeFill(_seriesCount + 1, tmp);

    reinterpret_cast<Collection::Array<int32_t>*>(tmp + 8)->~Array();

    uint8_t* data = static_cast<uint8_t*>(_seriesData);
    *reinterpret_cast<uint32_t*>(data + old * 0x18) = colour;
}

// ============================================================
// UI::Element::RemoveChildren  retail 0x180115570  (Wave 9 lane B3)
// ============================================================
//
//   for (i = 0; i < this->_children.size; ++i) {
//       Element** data = this->_children.data;      // RE-READ every iteration
//       if (data[i]) { data[i]->vtbl[0](data[i], 1); data[i] = nullptr; }
//   }
//   Collection::Array<Element*>::SetSize(&this->_children, 0);   // TAIL JMP
//
// Slot 0 called with a flag of 1 is the scalar deleting destructor -- destroy
// AND free -- the idiom runtime/types/object/Internal.h already names as
// kInternalVtblScalarDtor. It is NOT `??1Element`.
//
// THE RE-READ OF `_children.data` INSIDE THE LOOP IS LOAD-BEARING, and is why
// this is an index loop over a refetched base rather than the obvious cached
// pointer: retail reloads `[RBP + 0xa0]` at 0x1801155a0 on every iteration,
// after the previous child's destructor has run.
//
// The loop bound, by contrast, IS hoisted in retail -- `_children.size` is read
// into EBX's comparand from `[RBP+0xa8]` each iteration (0x1801155ca) but the
// SetSize that changes it runs only after the loop. Reading it once here is the
// same sequence of accesses for any object whose destructors do not resize the
// parent, and for one that does, retail's per-iteration reload would be the
// faithful spelling -- noted rather than silently chosen, because it is the one
// place this port is not instruction-for-instruction.
//
// NO ORACLE, stated rather than glossed: the function DESTROYS the children.
void Element::RemoveChildren() {
    const int32_t count = _children._size;

    for (int32_t i = 0; i < count; ++i) {
        Element** data = _children._data;              // re-read, see above
        Element* child = data[i];
        if (child == nullptr) continue;
        child->DeletingDtor(1);                        // slot 0, flag 1
        data[i] = nullptr;
    }
    RtArrayCellsSetSize(&_children, 0);
}

}  // namespace UI

// ============================================================================
// THE DOORS — each keeps its registered name (and, where it has one, its
// address) and delegates with a QUALIFIED non-virtual call. None of these
// retail bodies tests `this` for null, so no door does.
// ============================================================================
namespace {

int32_t TableElement_GetColumnCount(const void* self) {
    return reinterpret_cast<const UI::TableElement*>(self)->UI::TableElement::GetColumnCount();   // B.3.6 bridge
}
int32_t TableElement_GetRowCount(const void* self) {
    return reinterpret_cast<const UI::TableElement*>(self)->UI::TableElement::GetRowCount();   // B.3.6 bridge
}
// The by-value InternalPtr<Element> arrives as the ADDRESS of the caller's copy.
void TableElement_SetChild2(void* self, void** elemSlot, int32_t col) {
    reinterpret_cast<UI::TableElement*>(self)->UI::TableElement::SetChild(   // B.3.6 bridge
        reinterpret_cast<Object::InternalPtr<UI::Element>*>(elemSlot), col);   // B.3.6 bridge
}
void TableElement_SetChild3(void* self, void** elemSlot, int32_t col, int32_t row) {
    reinterpret_cast<UI::TableElement*>(self)->UI::TableElement::SetChild(   // B.3.6 bridge
        reinterpret_cast<Object::InternalPtr<UI::Element>*>(elemSlot), col, row);   // B.3.6 bridge
}
void TableElement_SetSize(void* self, int32_t rows, int32_t cols) {
    reinterpret_cast<UI::TableElement*>(self)->UI::TableElement::SetSize(rows, cols);   // B.3.6 bridge
}

// RETSLOT: (this, ret) — placement-new the member's prvalue into the slot.
void* ImageElement_GetSprite(const void* self, void* retval) {
    new (retval) UI::Sprite(
        reinterpret_cast<const UI::ImageElement*>(self)->UI::ImageElement::GetSprite());   // B.3.6 bridge
    return retval;
}
void ImageElement_SetRotation(void* self, float angle) {
    reinterpret_cast<UI::ImageElement*>(self)->UI::ImageElement::SetRotation(angle);   // B.3.6 bridge
}
// `VSprite@2@` by value: RDX holds the ADDRESS of the caller's copy.
void ImageElement_SetSprite(void* self, const void* spriteVal) {
    reinterpret_cast<UI::ImageElement*>(self)->UI::ImageElement::SetSprite(   // B.3.6 bridge
        static_cast<const UI::Sprite*>(spriteVal));
}
void ImageElement_SetWidthHeight(void* self, int32_t w, int32_t h) {
    reinterpret_cast<UI::ImageElement*>(self)->UI::ImageElement::SetWidthHeight(w, h);   // B.3.6 bridge
}

void* LabelElement_GetSprite(const void* self, void* retval) {
    new (retval) UI::Sprite(
        reinterpret_cast<const UI::LabelElement*>(self)->UI::LabelElement::GetSprite());   // B.3.6 bridge
    return retval;
}
void LabelElement_SetSprite(void* self, const void* spriteVal) {
    reinterpret_cast<UI::LabelElement*>(self)->UI::LabelElement::SetSprite(   // B.3.6 bridge
        static_cast<const UI::Sprite*>(spriteVal));
}

// `AEBVString@System@@` — a const reference, a bare pointer in R8.
void TextElement_SetParameter(void* self, int32_t index, const void* strRef) {
    reinterpret_cast<UI::TextElement*>(self)->UI::TextElement::SetParameter(   // B.3.6 bridge
        index, *static_cast<const System::String*>(strRef));
}
// `VString@2@` by value: RDX holds the ADDRESS of the caller's UI::String.
void TextElement_SetText(void* self, const void* uiStringVal) {
    reinterpret_cast<UI::TextElement*>(self)->UI::TextElement::SetText(   // B.3.6 bridge
        static_cast<const UI::String*>(uiStringVal));
}

// RETSLOT: (this, ret, index).
void* DocumentTextElement_GetLink(const void* self, void* retval, int32_t index) {
    new (retval) System::String(
        reinterpret_cast<const UI::DocumentTextElement*>(self)->UI::DocumentTextElement::GetLink(index));   // B.3.6 bridge
    return retval;
}
// THE DOOR DECIDES THE OVERLOAD: `QEAAXVString@2@@Z` (UI::String by hidden
// pointer) vs `QEAAXAEBVString@System@@@Z` (a System::String const&).
void DocumentTextElement_SetText_UiString(void* self, const void* uiStringVal) {
    reinterpret_cast<UI::DocumentTextElement*>(self)->UI::DocumentTextElement::SetText(   // B.3.6 bridge
        static_cast<const UI::String*>(uiStringVal));
}
void DocumentTextElement_SetText_StringRef(void* self, const void* strRef) {
    reinterpret_cast<UI::DocumentTextElement*>(self)->UI::DocumentTextElement::SetText(   // B.3.6 bridge
        *static_cast<const System::String*>(strRef));
}

void TabElement_SetActiveTab(void* self, int32_t tab) {
    reinterpret_cast<UI::TabElement*>(self)->UI::TabElement::SetActiveTab(tab);   // B.3.6 bridge
}

int32_t PageElement_GetActivePage(const void* self) {
    return reinterpret_cast<const UI::PageElement*>(self)->UI::PageElement::GetActivePage();   // B.3.6 bridge
}
void PageElement_SetActivePage(void* self, int32_t page) {
    reinterpret_cast<UI::PageElement*>(self)->UI::PageElement::SetActivePage(page);   // B.3.6 bridge
}

void GraphElement_RemoveGraphs(void* self) {
    reinterpret_cast<UI::GraphElement*>(self)->UI::GraphElement::RemoveGraphs();   // B.3.6 bridge
}
void GraphElement_AddGraphPoint(void* self, int32_t graph, int32_t value) {
    reinterpret_cast<UI::GraphElement*>(self)->UI::GraphElement::AddGraphPoint(graph, value);   // B.3.6 bridge
}
void GraphElement_AddGraph(void* self, uint32_t colour) {
    reinterpret_cast<UI::GraphElement*>(self)->UI::GraphElement::AddGraph(colour);   // B.3.6 bridge
}

void Element_RemoveChildren(void* self) {
    reinterpret_cast<UI::Element*>(self)->UI::Element::RemoveChildren();   // B.3.6 bridge
}

}  // namespace

// ============================================================================
// Registration
// ============================================================================
// WAVE 10 LANE UIORACLE — bodies live in their own translation units so this
// file (lane E's, and busy) is not a merge point for them. Registration is
// still done from RtUiWidgetsLayRegister below, because rt_ui_port.cpp says in
// its own banner: "Do not edit this file to add a lane."
int RtUiTabAddTabRegister();
int RtUiTextCacheRegister();

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
    // WAVE 10: UpdateContainers is no longer a forwarder. Registered under BOTH
    // its name and its address -- the convention that exists because lane B3
    // registered Element::RemoveChildren by name only and left the tree's one
    // address-door bypass (see the note further down this function).
    // WAVE 10 LANE UIORACLE: the AddTab hold-back is spent. Its stated blocker
    // ("constructs a temporary ElementDescription/DialogInstance") was a
    // misreading of the frame -- the temporary is a default-constructed
    // UI::Dialog and retail's inline construction IS ??0Dialog@UI@@QEAA@XZ,
    // store for store. Body, oracle and the full re-derivation are in
    // runtime/src/ui/tabelement_addtab.cpp.
    n += RtUiTabAddTabRegister();

    // WAVE 10 LANE UIORACLE: TextElement::CacheText + SetParameterCount, with
    // the predictive oracle the census row asked for. See
    // runtime/src/ui/textelement_cachetext.cpp. NOTE: RtTextElementCacheText
    // above needs no edit -- it resolves through ResolveExport, which consults
    // the port registry first, so it reaches the port from this point on.
    n += RtUiTextCacheRegister();

    RegisterRuntimeExport("?GetActivePage@PageElement@UI@@QEBAHXZ", (void*)&PageElement_GetActivePage); ++n;
    RegisterRuntimeExport("?SetActivePage@PageElement@UI@@QEAAXH@Z", (void*)&PageElement_SetActivePage); ++n;

    RegisterRuntimeExport("?RemoveGraphs@GraphElement@UI@@QEAAXXZ", (void*)&GraphElement_RemoveGraphs); ++n;
    // Wave 9 lane B3 -- a REAL DEPENDENCY row (WAVE9_LANE_V.md §1b).
    RegisterRuntimeExport("?RemoveChildren@Element@UI@@QEAAXXZ",
                          (void*)&Element_RemoveChildren); ++n;
    // WAVE 9 LANE UIF — A REAL HOLE, FOUND BY tools/port_bypass_audit.py, not
    // by reading. Lane B3 registered this port under its NAME only, so
    // `ResolveAddr(Module::Runtime, 0x180115570)` missed the address registry
    // and fell through to the DLL — and after cutover, to NULL. Nothing
    // exercised it until `??1Element` (runtime/src/ui/element.cpp) became the
    // first by-address caller, at which point the audit flagged it as the
    // tree's only address-door BYPASS. This is exactly the Wave-5-lane-D
    // failure mode the "register TWICE, name AND address" convention exists to
    // close (see the Registration note in element.cpp and dialog.cpp).
    RegisterRuntimeAddr(0x180115570, (void*)&Element_RemoveChildren);
    // WAVE 9 LANE UIF: AddGraphPoint's hold-back is spent — its one callee
    // (0x180023830) became ours in lane Q, so the row closes.
    RegisterRuntimeExport("?AddGraphPoint@GraphElement@UI@@QEAAXHH@Z",
                          (void*)&GraphElement_AddGraphPoint); ++n;
    RegisterRuntimeAddr(0x180040690, (void*)&GraphElement_AddGraphPoint);
    // WAVE 9 LANE UIW: the hold-back above is spent. Lane UIF declined this row
    // because 0x1800417d0 was an unported unexported internal and porting
    // AddGraph would have moved the cutover row one frame deeper. That frame is
    // now ours -- 0x1800417d0 (kSpec_GraphSeries), 0x18001cdf0 and 0x1800419b0
    // are all registered in runtime/src/collection/array.cpp -- so the row
    // closes. See docs/runtime_port/WAVE9_LANE_UIW.md.
    RegisterRuntimeExport("?AddGraph@GraphElement@UI@@QEAAXI@Z",
                          (void*)&GraphElement_AddGraph); ++n;
    RegisterRuntimeAddr(0x180040620, (void*)&GraphElement_AddGraph);

    printf("[RtPort] UI widgets(layout): %d exports registered\n", n);
    return n;
}
