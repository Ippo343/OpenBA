// ============================================================================
// runtime/src/ui/description_dtors.cpp — 26 UI ElementDescription/Element
// destructors (`??1<Class>@UI@@UEAA@XZ`), transcribed individually from the
// retail Runtime-x64.dll disassembly (never Ghidra's decompiled C — see
// reference_ghidra_c_output_vs_disassembly.md).
// ============================================================================
//
// STYLE AND SHARED IDIOM. This file follows runtime/src/ui/groupdescription.cpp
// (`RtUi_GroupDescription_Dtor`) and runtime/src/ui/animatedimagedescription.cpp
// end to end: `RtAddr<Fn>(va)` for an address-only callee, `RT_DATA_SLOT` for a
// vtable resolved by its mangled symbol, `RT_EXPORT_ONCE` for an exported
// callee resolved by name. The one addition here is `ReleaseStringField`,
// factored out because the SAME eight-instruction sequence — decrement a
// refcounted System::String's counter at `data-4`; if it reaches zero, walk
// the FIELD back 8 bytes (to the allocation header) and MemFree THAT, leaving
// the field holding a freed pointer (retail's asymmetry, kept exactly as
// groupdescription.cpp documents it); otherwise null the field — appears
// 60+ times below, byte-identical every time down to which register or
// immediate supplies the null. Factoring it is not "one body with a constant
// swapped": each of the 26 functions below is still transcribed on its own
// from its own disassembly for vtable stamps, member counts, sub-object
// destructor calls and its base-chain target, which is where these classes
// actually differ.
//
// TWO BASE CHAINS, converging on the SAME string-field idiom either way:
//   * Every `*Description` class chains down through `??_7ElementDescription@
//     UI@@6B@` and tail-jumps (a real `JMP`, transcribed as a call then
//     return) to `??1External@Object@@UEAA@XZ` (0x1800bc950, EXPORTED —
//     resolved through RT_EXPORT_ONCE).
//   * Every `*Element` class tail-jumps instead to `~Element@UI@@`
//     (0x1801161a0). Ghidra's own analysis names it but the mangled form is
//     not confirmed exported, so it is reached by address through
//     `RtAddr<ElemDtorFn>(0x1801161a0ull)`, exactly as
//     animatedimagedescription.cpp reaches `~ImageDescription` by address.
//
// TWO CLASSES ARE NOT PEERS OF THEIR SIBLINGS — the disassembly proves it,
// not the name:
//   * `CheckDescription` and `ComboDescription` each restamp
//     `??_7ButtonDescription@UI@@6B@` (0x180157370) BETWEEN their own vtable
//     and `ElementDescription`'s, i.e. they derive from ButtonDescription, not
//     directly from ElementDescription — and ButtonDescription itself adds no
//     member of its own (its dtor releases only ElementDescription's shared
//     +0x60 field), so there is nothing to destroy at that intermediate level,
//     only the restamp.
//   * `ComboElement` restamps `??_7ButtonElement@UI@@6B@` (0x180157310)
//     immediately before its tail jump to `~Element`, i.e. it derives from
//     ButtonElement, not directly from Element.
//   * `LabelDescription` does not restamp ElementDescription or release +0x60
//     itself at all: after releasing its own +0xb0 field it TAIL-JUMPS to
//     `??1TextDescription@UI@@UEAA@XZ` (0x18011eae0, this same file, called
//     directly rather than re-resolved) — i.e. LabelDescription derives from
//     TextDescription.
//
// HELPER DESTRUCTORS THIS FILE CALLS BUT DOES NOT PORT. All are reached
// through `RtAddr` (registry-first, so a future port of any of them is picked
// up automatically without touching this file):
//   ALREADY PORTED (grepped in runtime/src before writing this file):
//     0x180024aa0  Array<void*>::~Array           runtime/src/collection/array.cpp
//     0x180037310  Array<T>::~Array (11-byte diff) runtime/src/collection/array.cpp
//     0x180010ab0  ListCleanup (elem-owning ~Array) runtime/src/collection/array_destroy.cpp
//     0x180120f20  UserInterface::CloseDialog      runtime/src/ui/userinterface_close.cpp
//   NOT PORTED (no hit anywhere under runtime/ or standalone/; each is an
//   undocumented `FUN_` in Ghidra taking one pointer argument, the same
//   one-member-Array-destructor shape the ported ones above have — reached
//   here by raw address, exactly as groupdescription.cpp reaches the
//   Array<ExternalPtr<ElementDescription>> destructor at 0x180039440):
//     0x18003db40, 0x18003da00, 0x18003d8c0, 0x180043a00, 0x180079870,
//     0x18004cc90, 0x18003e310, 0x18003e0c0
// ============================================================================
#include <cstddef>
#include <cstdint>

#include "../../funcs/rt_port.h"

// ResolveAddr, forward-declared rather than pulled in from standalone/resolve.h
// — see the identical note in groupdescription.cpp. Must stay byte-identical
// to resolve.h's declaration; the linker checks the two.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

// The other destructor in this file that a tail-jump reaches directly.
extern "C" void RtUi_TextDescription_Dtor(void* selfv);
// Ten sibling ??1 bodies (Border, Button, Check, ClipRegion, Combo, Container,
// DocumentText, Edit, Graph, Label) live in their own class files under
// runtime/src/ui/ (vtable batch 1); they were duplicated here and dropped at
// the merge (2026-09-12), one registration per address.

namespace {

template <typename Fn>
Fn RtAddr(uintptr_t ghidra) { return reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, ghidra)); }

using MemFreeFn  = void (*)(void*);
using ExtDtorFn  = void (*)(void*);   // ??1External@Object@@UEAA@XZ, exported
using ElemDtorFn = void (*)(void*);   // ~Element@UI@@, address-only
using SubDtorFn  = void (*)(void*);   // every unported/array helper below: (this) -> void

// ---- the shared refcounted System::String release, see the banner ---------
void ReleaseStringField(void* fieldAddr) {
    char** field = reinterpret_cast<char**>(fieldAddr);
    char* s = *field;
    if (s != nullptr) {
        int32_t* refcount = reinterpret_cast<int32_t*>(s - 4);
        if (--(*refcount) == 0) {
            *field = s - 8;
            RT_EXPORT_ONCE(MemFreeFn, "?MemFree@MemoryAllocator@System@@SAXPEAX@Z")(*field);
        } else {
            *field = nullptr;
        }
    }
}
inline void ReleaseStringField(uint8_t* p, size_t off) { ReleaseStringField(p + off); }

void TailExternalDtor(uint8_t* p) {
    RT_EXPORT_ONCE(ExtDtorFn, "??1External@Object@@UEAA@XZ")(p);
}
void TailElementDtor(uint8_t* p) {
    RtAddr<ElemDtorFn>(0x1801161a0ull)(p);
}

}  // namespace










// ============================================================================
// THE THIRTEEN UI::*Description ??1 BODIES THAT WERE HERE ARE GONE.
// ============================================================================
// Layout, List, Number, NumberEdit, Progress, Scroll, Slider, Spinner, Tab,
// Table and Text each own their destructor in their own class file under
// runtime/src/ui/ (vtable batch 2), the same precedent batch 1's ten set.
// They were duplicated here and dropped at the merge (2026-09-12): one
// registration per address, and the class file is the one place the rest of
// that class's evidence already lives.
//
// `RtUi_TextDescription_Dtor` is still DECLARED above because
// labeldescription.cpp tail-jumps straight into it; it is DEFINED in
// runtime/src/ui/textdescription.cpp.
// ============================================================================

// ============================================================================
// ??1ComboElement@UI@@UEAA@XZ   retail 0x180112490
// ============================================================================
// Derives from ButtonElement (final restamp before the tail jump, see the
// banner). +0x120 holds a DialogInstance-shaped pointer (the open dropdown, if
// any): `+0x10` off THAT is the owning UserInterface*. When set, retail closes
// it through `UserInterface::CloseDialog` — the exact `void* tmp = child; ...
// (ui, &tmp)` calling shape runtime/src/ui/userinterface_close.cpp already
// documents for CloseDialog's own recursive calls (its lines 383-388).
extern "C" void RtUi_ComboElement_Dtor(void* selfv) {
    uint8_t* p = reinterpret_cast<uint8_t*>(selfv);
    *reinterpret_cast<void**>(p) = RT_DATA_SLOT(void*, "??_7ComboElement@UI@@6B@");        // 1801124a6

    uint8_t* dlgInst = *reinterpret_cast<uint8_t**>(p + 0x120);    // 1801124b0
    if (dlgInst != nullptr) {                                      // 1801124b7
        void* tmp = dlgInst;                                        // 1801124bc  the by-value InternalPtr
        void* ui  = *reinterpret_cast<void**>(dlgInst + 0x10);      // 1801124c6
        using CloseDialogFn = void (*)(void* ui, void** dlgPtr);
        RtAddr<CloseDialogFn>(0x180120f20ull)(ui, reinterpret_cast<void**>(&tmp)); // 1801124ca, ALREADY PORTED
    }

    RtAddr<SubDtorFn>(0x180024aa0ull)(p + 0x108);  // 1801124d7, ALREADY PORTED (array.cpp)
    RtAddr<SubDtorFn>(0x180037310ull)(p + 0xf8);   // 1801124e4, ALREADY PORTED (array.cpp)
    RtAddr<SubDtorFn>(0x180010ab0ull)(p + 0xe8);   // 1801124f1, ALREADY PORTED (array_destroy.cpp, ListCleanup)

    *reinterpret_cast<void**>(p) = RT_DATA_SLOT(void*, "??_7ButtonElement@UI@@6B@");       // 1801124f7
    TailElementDtor(p);            // 180112509  JMP 0x1801161a0
}

// ============================================================================
// ??1DocumentTextElement@UI@@UEAA@XZ   retail 0x1800cf320
// ============================================================================
// Derives directly from Element (single own-vtable stamp, tail straight to
// ~Element). The first field release addresses +0x198 through a LOCAL that
// caches the field's address (retail's `LEA RAX,[RCX+0x198]` kept on the
// stack) rather than re-deriving `RCX+0x198` at the point of use like every
// other site in this file — same ReleaseStringField operation, just handed
// the address instead of `(p, off)`.
extern "C" void RtUi_DocumentTextElement_Dtor(void* selfv) {
    uint8_t* p = reinterpret_cast<uint8_t*>(selfv);
    *reinterpret_cast<void**>(p) = RT_DATA_SLOT(void*, "??_7DocumentTextElement@UI@@6B@"); // 1800cf336

    ReleaseStringField(p + 0x198);  // 1800cf340..1800cf372, addressed via a cached local
    ReleaseStringField(p, 0x168);   // 1800cf372

    RtAddr<SubDtorFn>(0x18003e310ull)(p + 0x158);  // 1800cf3af, unported
    RtAddr<SubDtorFn>(0x18003e0c0ull)(p + 0x148);  // 1800cf3bc, unported
    RtAddr<SubDtorFn>(0x18003db40ull)(p + 0x130);  // 1800cf3c9, unported
    RtAddr<SubDtorFn>(0x18003da00ull)(p + 0x120);  // 1800cf3d6, unported
    RtAddr<SubDtorFn>(0x18003d8c0ull)(p + 0x110);  // 1800cf3e3, unported
    RtAddr<SubDtorFn>(0x180037310ull)(p + 0xe8);   // 1800cf3f0, ALREADY PORTED (array.cpp)

    TailElementDtor(p);            // 1800cf3fe  JMP 0x1801161a0
}

// ============================================================================
// ??1EditElement@UI@@UEAA@XZ   retail 0x1800d0e50
// ============================================================================
// Derives directly from Element. Before releasing its two own String fields,
// if byte +0xe0 is set, retail decrements a DWORD counter one hop off the
// `Input::Configuration` singleton (`?_singleton@Configuration@Input@@
// 0PEAV12@EA`, 0x1801fe768 — the same singleton runtime/src/ui/
// userinterface_draw.cpp's file-local InputConfigSingleton() reads; that
// accessor is private to that file, so it is re-resolved here by the same
// mangled name rather than shared).
extern "C" void RtUi_EditElement_Dtor(void* selfv) {
    uint8_t* p = reinterpret_cast<uint8_t*>(selfv);
    *reinterpret_cast<void**>(p) = RT_DATA_SLOT(void*, "??_7EditElement@UI@@6B@");         // 1800d0e66

    if (p[0xe0] != 0) {                                                                     // 1800d0e70
        void* cfg = *RT_DATA_SLOT(void**, "?_singleton@Configuration@Input@@0PEAV12@EA");   // 1800d0e79
        void* counterObj = *reinterpret_cast<void**>(cfg);                                  // 1800d0e80
        --*reinterpret_cast<int32_t*>(counterObj);                                          // 1800d0e83
    }

    ReleaseStringField(p, 0xd8);   // 1800d0e85
    ReleaseStringField(p, 0xc8);   // 1800d0ebb

    TailElementDtor(p);            // 1800d0ef9  JMP 0x1801161a0
}



// ============================================================================
// Registration. Both doors for every row, exactly like groupdescription.cpp:
// the mangled export name (what the minted vtable and BaseTypeInfo ask for)
// and the retail ghidra address (what a ported caller's ResolveAddr literal
// asks for).
// ============================================================================
int RtUiDescriptionDtorsRegister() {
    int n = 0;











    RegisterRuntimeExport("??1ComboElement@UI@@UEAA@XZ", (void*)&RtUi_ComboElement_Dtor); ++n;
    RegisterRuntimeAddr(0x180112490ull, (void*)&RtUi_ComboElement_Dtor);

    RegisterRuntimeExport("??1DocumentTextElement@UI@@UEAA@XZ", (void*)&RtUi_DocumentTextElement_Dtor); ++n;
    RegisterRuntimeAddr(0x1800cf320ull, (void*)&RtUi_DocumentTextElement_Dtor);

    RegisterRuntimeExport("??1EditElement@UI@@UEAA@XZ", (void*)&RtUi_EditElement_Dtor); ++n;
    RegisterRuntimeAddr(0x1800d0e50ull, (void*)&RtUi_EditElement_Dtor);



    return n;
}
