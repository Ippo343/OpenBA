// ============================================================================
// runtime/src/ui/textelement_cachetext.cpp — WAVE 10, lane UIORACLE
//
//   ?SetParameterCount@TextElement@UI@@AEAAXXZ   retail 0x18011e8e0  (ord 3938)
//   ?CacheText@TextElement@UI@@AEAAXXZ           retail 0x18011ec10  (ord 1680)
//
// Both are `AEAA` — PRIVATE, and this DLL exports its private members, so
// "private" was never the reason they were not ported
// (reference_aeaa_is_private_not_unexported.md).
// ============================================================================
//
// WHY THIS FILE EXISTS AND WHAT THE RECORDED BLOCKER TURNED OUT TO BE
// -------------------------------------------------------------------
// The UI census carries `?CacheText@TextElement@UI@@AEAAXXZ` as "not replayable,
// needs the predictive oracle", parked behind the allocator NO-GO. Both halves
// of that were re-checked from scratch rather than inherited
// (reference_holdback_blockers_are_stale_check_them_first.md):
//
//  * NOT REPLAYABLE — STILL TRUE, and it is the SAME defect Wave 9 lane M
//    found on `NumberElement::SetValue`. This body swaps a refcounted
//    System::String at `this+0xF8`: ours releases the old buffer, and a replay
//    that then hands retail a shallow copy hands it a pointer ours has already
//    freed. WAVE9.md finding (1). A prefix replay here is not a weak oracle,
//    it is a use-after-free.
//
//  * The body is written against the live sink (runtime/funcs/rt_sink.h).
//
//  * THE ALLOCATOR "NO-GO" IS NOT LOad-BEARING FOR THIS ROW. This body does not
//    contain an allocator: every allocation and every free it performs is a
//    CALL into a function that is already ported and registered —
//    `String::FormatParamList` (Wave 9 lane B3), `MemoryAllocator::MemFree`
//    (Wave 2), `UI::String::operator System::String` (Wave 7 lane F) and, as of
//    this commit, `Collection::Array<System::String>::SetSize` (0x180029a80).
//    Nothing here decides where memory comes from. **This lane's conclusion
//    therefore does NOT depend on lane ALLOC's re-test of that gate.**
//
//  * ONE DEPENDENCY WAS REAL AND IS CLOSED HERE. `CacheText`'s first act is a
//    call to `SetParameterCount` (0x18011e8e0), whose own callee 0x180029a80
//    was an unported, unexported `Array<System::String>::SetSize`. Wave 9's
//    array_destroy.cpp predicted this exact arrival — "It will get a SECOND
//    caller the day 0x180029a80 … lands" — so 0x180029a80 is ported in
//    runtime/src/collection/array.cpp as `kSpec_StringRefZ` in this commit and
//    is reached through the registry, not by a private second body.
//
// LAYOUT (all offsets read from the two disassemblies, not from decompiled C)
// --------------------------------------------------------------------------
//   this+0xC8  Collection::Array<System::String>._data   (the parameters)
//   this+0xD0  ...                       ._size          <- the branch selector
//   this+0xD4  ...                       ._capacity
//   this+0xD8  UI::String (16 bytes) — the raw text reference
//   this+0xF8  char* — the OWNED, refcounted cached display text
//
// THE TWO BRANCHES, AND WHY THEY NEED DIFFERENT ORACLES
// -----------------------------------------------------
// `_size == 0`  -> the cached text becomes the System::String that
//   `UI::String::operator System::String` returns. That operator is
//   `StringTable::GetText(id)` (0x180002550) under an eight-instruction wrapper:
//   it hands back a refcounted COPY OF THE TABLE'S OWN BUFFER, so ours and
//   retail produce the IDENTICAL POINTER. This branch is exactly comparable.
//
// `_size != 0`  -> `String::FormatParamList` builds a FRESH allocation. Ours and
//   retail allocate different blocks, so the pointer at +0xF8 is NOT comparable
//   and predicting it would fail 100% of calls — the shape
//   reference_oracle_must_not_compare_unwritten_bytes.md warns about, one step
//   over. On this branch the pointer prediction is declared with `Exclude()`,
//   which makes the round record it in the `unverifiable=` column instead of
//   comparing it. The hole is counted, never silent.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/system/String.h"
#include "../../funcs/rt_sink.h"
#include "../../types/ui/TextElement.h"

// ----------------------------------------------------------------------------
// ResolveAddr, forward-declared rather than pulled in from standalone/resolve.h
// (same reasoning as runtime/src/ui/widgets_layout.cpp and rt_port.h itself:
// that header drags in <windows.h>, which collides with engine type/member
// names). This declaration must stay byte-identical to resolve.h's.
// ----------------------------------------------------------------------------
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace {

namespace TE = UI::TextElementOffsets;

inline uint8_t* B(void* p, int off) { return reinterpret_cast<uint8_t*>(p) + off; }

// System::String is ONE POINTER (runtime/types/system/String.h). The block in
// front of it is { int32 length; int32 refcount; } and the object's value is
// block+8.
// ENDGAME B.3.1 step 2: `Str` IS `System::String`. The local POD is gone; the
// name is kept as an alias so the offsets/comments below still read the way the
// disassembly does.
using Str = System::String;

inline int32_t& RefCount(void* data) {
    return *reinterpret_cast<int32_t*>(static_cast<char*>(data) - 4);
}
inline int32_t Length(const void* data) {
    return data ? *reinterpret_cast<const int32_t*>(static_cast<const char*>(data) - 8) : 0;
}

// ---- The callees. Every one goes through the port registry ----------------
//
// RT_EXPORT_ONCE: these are REAL DEPENDENCIES (the body calls them to do work),
// so they go through the port registry
// (reference_bypassed_port_needs_a_source_checker.md).

// ??BString@UI@@QEBA?AV0System@@XZ (0x180035020) — PORTED, runtime/src/ui/
// sprite.cpp. Hidden return slot: RCX=this, RDX=&ret (0x180035020's own
// `MOV qword ptr [RSP+0x10],RDX` / `MOV RAX,RBX`), so the typedef carries it
// (reference_hidden_retslot_wild_write.md).
// UI::String::operator System::String is another batch's class, so this stays
// a name lookup; only its RESULT is a real System::String now.
using UiStringToStringFn = Str* (*)(const void* self, Str* ret);
inline UiStringToStringFn UiStringToString() {
    return RT_EXPORT_ONCE(UiStringToStringFn, "??BString@UI@@QEBA?AV0System@@XZ");
}

// ?FormatParamList@String@System@@SA?AV12@AEBV12@PEBV12@@Z (0x1800c8dc0) —
// PORTED, Wave 9 lane B3. STATIC with a hidden return slot, so the argument
// order is (ret, fmt, params) and NOT (fmt, params). Ghidra's decompiled C
// prints this call with TWO arguments and DROPS the third
// (`MOV R8,qword ptr [RDI+0xc8]` at 0x18011ecbd) — the exact
// reference_ghidra_c_output_vs_disassembly.md failure. The params pointer is
// the parameter Array's DATA pointer, not the Array header.
// ?MemFree@MemoryAllocator@System@@SAXPEAX@Z (0x1800c7ad0) — PORTED, Wave 2.
using MemFreeFn = void (*)(void*);
inline MemFreeFn MemFree() {
    return RT_EXPORT_ONCE(MemFreeFn, "?MemFree@MemoryAllocator@System@@SAXPEAX@Z");
}

// Collection::Array<System::String>::SetSize(int) — retail 0x180029a80, PORTED
// in this commit (runtime/src/collection/array.cpp, kSpec_StringRefZ). Not
// exported (a header-only template instantiation), so it is reached by address,
// the same idiom widgets_layout.cpp uses for 0x1800764c0 — and through
// ResolveAddr, which consults the PORT REGISTRY FIRST
// (reference_resolveaddr_consults_port_registry_first.md), so this reaches our
// body and not the DLL's.
inline void ArrayStringSetSize(void* arrayHdr, int32_t n) {
    using Fn = void (*)(void*, int32_t);
    static Fn fn = nullptr;                 // cache a hit, retry a miss
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x180029a80));
    fn(arrayHdr, n);
}

// ============================================================================
// ?SetParameterCount@TextElement@UI@@AEAAXXZ — retail 0x18011e8e0
// ============================================================================
//
// Resize the parameter Array to however many "@n" slots the current format
// string contains. Straight-line; the only subtlety is that retail takes a
// SECOND reference on the same buffer (RBX at 0x18011e91a-0x18011e922) before
// calling FormatParamCount and releases it afterwards, which is the compiler's
// materialisation of passing the String by value. It is reproduced because the
// refcount traffic is observable, not because the count depends on it.
//
// IT IS IDEMPOTENT, and the CacheText oracle below depends on that: a second
// call computes the same n and 0x180029a80's first instruction pair
// (`CMP EDI,ESI; JZ`) returns immediately.
void SetParameterCountBody(void* self) {
    Str tmp;
    UiStringToString()(B(self, TE::kTextString), &tmp);       // 0x18011e903

    char* held = nullptr;                                     // RBX
    if (tmp._data) { held = tmp._data; ++RefCount(tmp._data); }  // 0x18011e91a

    const int n = System::String::FormatParamCount(held);     // 0x18011e928
    ArrayStringSetSize(B(self, TE::kParamArrayData), n);      // 0x18011e936

    if (held) {                                               // 0x18011e93c
        if (--RefCount(held) == 0) MemFree()(held - 8);
    }
    // 0x18011e962 — `tmp`'s destructor, at the end of the body, is retail's
    // release of the same slot.
}

// ============================================================================
// The CacheText body, written ONCE against an RtSink.
// ============================================================================
//
// `out` carries the ONE store this function makes to the element: the new
// cached-text pointer at +0xF8. In live mode it is a store; in predict mode it
// is a recorded prediction and memory is untouched.
//
// WHAT PREDICT MODE DELIBERATELY DOES NOT DO, and why each is right:
//
//  * IT DOES NOT RELEASE THE OLD BUFFER. A release is a refcount decrement and
//    possibly a free — a real mutation, and the very one that makes the replay
//    oracle a use-after-free. The predictor instead PREDICTS the decremented
//    refcount, but only when the pre-count is > 1, i.e. only when retail will
//    not free the block; when the pre-count is 1 the location is about to stop
//    existing and is declared with Exclude() rather than compared.
//
//  * IT DOES CALL SetParameterCount FOR REAL. That is not an exception to
//    "predict mode writes nothing", it is a stated modelling choice with a
//    reason: the params array is FormatParamList's third argument, so a
//    predictor that skipped the resize would format from a stale (possibly
//    null) params pointer and predict a string retail never produces. Running
//    it is safe because it is idempotent — retail's own call, moments later,
//    finds the size already correct and returns at its first branch. The
//    array header is declared with Exclude() so the write is COUNTED, not
//    silently tolerated.
//
// `outNew` receives the String the body decided on, still holding its
// reference. The caller releases it AFTER Verify(), so the comparison never
// reads a buffer either side has dropped.
void CacheTextBody(void* self, RtSink& out) {
    SetParameterCountBody(self);                              // 0x18011ec29

    Str tmp;
    UiStringToString()(B(self, TE::kTextString), &tmp);       // 0x18011ec42 / ec9c

    Str result;                                               // [RBP+0x38]
    Str param0;                                               // [RBP+0x40]
    char* newData = nullptr;

    // The branch selector is the parameter Array's _size, which the call above
    // has just set. Retail reads it at 0x18011ec39 — BEFORE the operator call,
    // which is immaterial (the operator does not touch `self`) but is why the
    // decompiled C shows the CMP above both arms.
    if (*reinterpret_cast<int32_t*>(B(self, TE::kParamCount)) == 0) {
        newData = tmp._data;
    } else {
        if (tmp._data) { param0._data = tmp._data; ++RefCount(tmp._data); }  // 0x18011ecb3
        // The params pointer is the parameter Array's DATA pointer, not the
        // Array header — Ghidra's decompiled C drops this third argument
        // (reference_ghidra_c_output_vs_disassembly.md). B.3.1 bridge: the
        // array element storage is untyped until Collection::Array is a
        // template.
        result = System::String::FormatParamList(
            param0, *reinterpret_cast<Str**>(B(self, TE::kParamArrayData)));
        newData = result._data;                               // 0x18011ecd1: R14 = &result
    }

    char* cur = *reinterpret_cast<char**>(B(self, TE::kCachedText));
    if (cur != newData) {                                     // 0x18011ec55 / ecdb
        if (cur) {
            if (--RefCount(cur) == 0) {
                // Retail stores the decremented pointer back into the
                // MEMBER before freeing (`ADD qword ptr [RDI+0xf8],-0x8;
                // MOV RCX,[RDI+0xf8]`), so MemFree's argument comes out of
                // the element, not out of a register. Reproduced literally.
                *reinterpret_cast<char**>(B(self, TE::kCachedText)) = cur - 8;
                MemFree()(*reinterpret_cast<void**>(B(self, TE::kCachedText)));
            } else {
                *reinterpret_cast<char**>(B(self, TE::kCachedText)) = nullptr;
            }
        }

        out.Ptr(B(self, TE::kCachedText), newData, "TextElement::_cachedText");

        if (newData) ++RefCount(newData);
    }

    // Retail releases `result` and `param0` here (0x18011ed1d, 0x18011ed46) and
    // `tmp` last (0x18011ed66) — which is exactly reverse declaration order,
    // so the three destructors at the end of this scope are that sequence.
}

} // namespace

// ============================================================================
// The registered entry points.
// ============================================================================

extern "C" void RtUi_TextElement_SetParameterCount(void* self) {
    // NO ORACLE, and the reason is not laziness. Its whole effect is
    // 0x180029a80's, which HAS one — the Collection probe array.cpp registers
    // for it. A shadow predicate here would
    // re-derive FormatParamCount's answer in a second spelling and agree with
    // itself by construction, which is the vacuous shape
    // reference_probe_needs_an_effectiveness_control.md rules out. The CacheText
    // round below covers this body's ONE observable output (the parameter
    // array) transitively: it Excludes the header and then formats from it, so
    // a wrong count shows up as a wrong STRING.
    SetParameterCountBody(self);
}

extern "C" void RtUi_TextElement_CacheText(void* self) {
    RtSink live;
    CacheTextBody(self, live);
}

// ============================================================================
// Registration. NAME AND ADDRESS, both, for every body — the convention that
// exists because Wave 9 lane B3 registered Element::RemoveChildren by name only
// and left the tree's one address-door bypass.
// ============================================================================
int RtUiTextCacheRegister() {
    int n = 0;
    RegisterRuntimeExport("?SetParameterCount@TextElement@UI@@AEAAXXZ",
                          (void*)&RtUi_TextElement_SetParameterCount); ++n;
    RegisterRuntimeAddr(0x18011e8e0, (void*)&RtUi_TextElement_SetParameterCount);

    RegisterRuntimeExport("?CacheText@TextElement@UI@@AEAAXXZ",
                          (void*)&RtUi_TextElement_CacheText); ++n;
    RegisterRuntimeAddr(0x18011ec10, (void*)&RtUi_TextElement_CacheText);
    return n;
}
