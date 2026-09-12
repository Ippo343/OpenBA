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
// ??1TextDescription@UI@@UEAA@XZ   retail 0x18011eae0
// ============================================================================
// Written BEFORE LabelDescription's below, which tail-jumps into this exact
// function rather than re-resolving it.
extern "C" void RtUi_TextDescription_Dtor(void* selfv) {
    uint8_t* p = reinterpret_cast<uint8_t*>(selfv);
    *reinterpret_cast<void**>(p) = RT_DATA_SLOT(void*, "??_7TextDescription@UI@@6B@");     // 18011eaf6
    ReleaseStringField(p, 0x78);   // 18011eb00

    *reinterpret_cast<void**>(p) = RT_DATA_SLOT(void*, "??_7ElementDescription@UI@@6B@");  // 18011eb2a
    ReleaseStringField(p, 0x60);   // 18011eb34
    TailExternalDtor(p);           // 18011eb66  JMP
}


// ============================================================================
// ??1LayoutDescription@UI@@UEAA@XZ   retail 0x1800d3a10
// ============================================================================
extern "C" void RtUi_LayoutDescription_Dtor(void* selfv) {
    uint8_t* p = reinterpret_cast<uint8_t*>(selfv);
    *reinterpret_cast<void**>(p) = RT_DATA_SLOT(void*, "??_7LayoutDescription@UI@@6B@");   // 1800d3a26
    RtAddr<SubDtorFn>(0x180043a00ull)(p + 0x68);   // 1800d3a34, unported

    *reinterpret_cast<void**>(p) = RT_DATA_SLOT(void*, "??_7ElementDescription@UI@@6B@");  // 1800d3a3a
    ReleaseStringField(p, 0x60);   // 1800d3a44
    TailExternalDtor(p);           // 1800d3a76  JMP
}

// ============================================================================
// ??1ListDescription@UI@@UEAA@XZ   retail 0x1800d4910
// ============================================================================
extern "C" void RtUi_ListDescription_Dtor(void* selfv) {
    uint8_t* p = reinterpret_cast<uint8_t*>(selfv);
    *reinterpret_cast<void**>(p) = RT_DATA_SLOT(void*, "??_7ListDescription@UI@@6B@");     // 1800d4926
    ReleaseStringField(p, 0x80);   // 1800d4930

    *reinterpret_cast<void**>(p) = RT_DATA_SLOT(void*, "??_7ElementDescription@UI@@6B@");  // 1800d4966
    ReleaseStringField(p, 0x60);   // 1800d4970
    TailExternalDtor(p);           // 1800d49a2  JMP
}

// ============================================================================
// ??1NumberDescription@UI@@UEAA@XZ   retail 0x180118c00
// ============================================================================
extern "C" void RtUi_NumberDescription_Dtor(void* selfv) {
    uint8_t* p = reinterpret_cast<uint8_t*>(selfv);
    *reinterpret_cast<void**>(p) = RT_DATA_SLOT(void*, "??_7NumberDescription@UI@@6B@");   // 180118c16
    *reinterpret_cast<void**>(p) = RT_DATA_SLOT(void*, "??_7ElementDescription@UI@@6B@");  // 180118c20
    ReleaseStringField(p, 0x60);   // 180118c2a
    TailExternalDtor(p);           // 180118c5c  JMP
}

// ============================================================================
// ??1NumberEditDescription@UI@@UEAA@XZ   retail 0x1800d4fe0
// ============================================================================
extern "C" void RtUi_NumberEditDescription_Dtor(void* selfv) {
    uint8_t* p = reinterpret_cast<uint8_t*>(selfv);
    *reinterpret_cast<void**>(p) = RT_DATA_SLOT(void*, "??_7NumberEditDescription@UI@@6B@"); // 1800d4ff6
    *reinterpret_cast<void**>(p) = RT_DATA_SLOT(void*, "??_7ElementDescription@UI@@6B@");    // 1800d5000
    ReleaseStringField(p, 0x60);   // 1800d500a
    TailExternalDtor(p);           // 1800d503c  JMP
}

// ============================================================================
// ??1ProgressDescription@UI@@UEAA@XZ   retail 0x18011a080
// ============================================================================
extern "C" void RtUi_ProgressDescription_Dtor(void* selfv) {
    uint8_t* p = reinterpret_cast<uint8_t*>(selfv);
    *reinterpret_cast<void**>(p) = RT_DATA_SLOT(void*, "??_7ProgressDescription@UI@@6B@"); // 18011a09b

    ReleaseStringField(p, 0xa0);   // 18011a0a5
    ReleaseStringField(p, 0x98);   // 18011a0d8
    ReleaseStringField(p, 0x90);   // 18011a10a
    ReleaseStringField(p, 0x88);   // 18011a13c
    ReleaseStringField(p, 0x80);   // 18011a16e
    ReleaseStringField(p, 0x78);   // 18011a1a0

    *reinterpret_cast<void**>(p) = RT_DATA_SLOT(void*, "??_7ElementDescription@UI@@6B@");  // 18011a1c6
    ReleaseStringField(p, 0x60);   // 18011a1d0
    TailExternalDtor(p);           // 18011a203  JMP
}

// ============================================================================
// ??1ScrollDescription@UI@@UEAA@XZ   retail 0x18011a5d0
// ============================================================================
// Same nine-field own-member set as BorderDescription (0xb0..0x70), different
// vtable — the two classes happen to add the same shape of members.
extern "C" void RtUi_ScrollDescription_Dtor(void* selfv) {
    uint8_t* p = reinterpret_cast<uint8_t*>(selfv);
    *reinterpret_cast<void**>(p) = RT_DATA_SLOT(void*, "??_7ScrollDescription@UI@@6B@");   // 18011a5eb

    ReleaseStringField(p, 0xb0);   // 18011a5f5
    ReleaseStringField(p, 0xa8);   // 18011a628
    ReleaseStringField(p, 0xa0);   // 18011a65a
    ReleaseStringField(p, 0x98);   // 18011a68c
    ReleaseStringField(p, 0x90);   // 18011a6be
    ReleaseStringField(p, 0x88);   // 18011a6f0
    ReleaseStringField(p, 0x80);   // 18011a722
    ReleaseStringField(p, 0x78);   // 18011a754
    ReleaseStringField(p, 0x70);   // 18011a77a

    *reinterpret_cast<void**>(p) = RT_DATA_SLOT(void*, "??_7ElementDescription@UI@@6B@");  // 18011a7a0
    ReleaseStringField(p, 0x60);   // 18011a7aa
    TailExternalDtor(p);           // 18011a7dd  JMP
}

// ============================================================================
// ??1SliderDescription@UI@@UEAA@XZ   retail 0x1800d7a00
// ============================================================================
extern "C" void RtUi_SliderDescription_Dtor(void* selfv) {
    uint8_t* p = reinterpret_cast<uint8_t*>(selfv);
    *reinterpret_cast<void**>(p) = RT_DATA_SLOT(void*, "??_7SliderDescription@UI@@6B@");   // 1800d7a1b

    ReleaseStringField(p, 0x98);   // 1800d7a25
    ReleaseStringField(p, 0x90);   // 1800d7a58
    ReleaseStringField(p, 0x88);   // 1800d7a8a
    ReleaseStringField(p, 0x80);   // 1800d7abc
    ReleaseStringField(p, 0x78);   // 1800d7aee

    *reinterpret_cast<void**>(p) = RT_DATA_SLOT(void*, "??_7ElementDescription@UI@@6B@");  // 1800d7b14
    ReleaseStringField(p, 0x60);   // 1800d7b1e
    TailExternalDtor(p);           // 1800d7b51  JMP
}

// ============================================================================
// ??1SpinnerDescription@UI@@UEAA@XZ   retail 0x1800d8950
// ============================================================================
extern "C" void RtUi_SpinnerDescription_Dtor(void* selfv) {
    uint8_t* p = reinterpret_cast<uint8_t*>(selfv);
    *reinterpret_cast<void**>(p) = RT_DATA_SLOT(void*, "??_7SpinnerDescription@UI@@6B@");  // 1800d8966
    *reinterpret_cast<void**>(p) = RT_DATA_SLOT(void*, "??_7ElementDescription@UI@@6B@");  // 1800d8970
    ReleaseStringField(p, 0x60);   // 1800d897a
    TailExternalDtor(p);           // 1800d89ac  JMP
}

// ============================================================================
// ??1TabDescription@UI@@UEAA@XZ   retail 0x1800da8e0
// ============================================================================
extern "C" void RtUi_TabDescription_Dtor(void* selfv) {
    uint8_t* p = reinterpret_cast<uint8_t*>(selfv);
    *reinterpret_cast<void**>(p) = RT_DATA_SLOT(void*, "??_7TabDescription@UI@@6B@");      // 1800da8fb
    RtAddr<SubDtorFn>(0x18004cc90ull)(p + 0xa8);   // 1800da90c, unported

    ReleaseStringField(p, 0x98);   // 1800da912
    ReleaseStringField(p, 0x90);   // 1800da945
    ReleaseStringField(p, 0x88);   // 1800da977

    *reinterpret_cast<void**>(p) = RT_DATA_SLOT(void*, "??_7ElementDescription@UI@@6B@");  // 1800da9a9
    ReleaseStringField(p, 0x60);   // 1800da9b3
    TailExternalDtor(p);           // 1800da9e6  JMP
}

// ============================================================================
// ??1TableDescription@UI@@UEAA@XZ   retail 0x18011e6f0
// ============================================================================
extern "C" void RtUi_TableDescription_Dtor(void* selfv) {
    uint8_t* p = reinterpret_cast<uint8_t*>(selfv);
    *reinterpret_cast<void**>(p) = RT_DATA_SLOT(void*, "??_7TableDescription@UI@@6B@");    // 18011e706
    ReleaseStringField(p, 0x90);   // 18011e710
    RtAddr<SubDtorFn>(0x180079870ull)(p + 0x78);   // 18011e74a, unported

    *reinterpret_cast<void**>(p) = RT_DATA_SLOT(void*, "??_7ElementDescription@UI@@6B@");  // 18011e750
    ReleaseStringField(p, 0x60);   // 18011e75a
    TailExternalDtor(p);           // 18011e78c  JMP
}

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











    RegisterRuntimeExport("??1LayoutDescription@UI@@UEAA@XZ", (void*)&RtUi_LayoutDescription_Dtor); ++n;
    RegisterRuntimeAddr(0x1800d3a10ull, (void*)&RtUi_LayoutDescription_Dtor);

    RegisterRuntimeExport("??1ListDescription@UI@@UEAA@XZ", (void*)&RtUi_ListDescription_Dtor); ++n;
    RegisterRuntimeAddr(0x1800d4910ull, (void*)&RtUi_ListDescription_Dtor);

    RegisterRuntimeExport("??1NumberDescription@UI@@UEAA@XZ", (void*)&RtUi_NumberDescription_Dtor); ++n;
    RegisterRuntimeAddr(0x180118c00ull, (void*)&RtUi_NumberDescription_Dtor);

    RegisterRuntimeExport("??1NumberEditDescription@UI@@UEAA@XZ", (void*)&RtUi_NumberEditDescription_Dtor); ++n;
    RegisterRuntimeAddr(0x1800d4fe0ull, (void*)&RtUi_NumberEditDescription_Dtor);

    RegisterRuntimeExport("??1ProgressDescription@UI@@UEAA@XZ", (void*)&RtUi_ProgressDescription_Dtor); ++n;
    RegisterRuntimeAddr(0x18011a080ull, (void*)&RtUi_ProgressDescription_Dtor);

    RegisterRuntimeExport("??1ScrollDescription@UI@@UEAA@XZ", (void*)&RtUi_ScrollDescription_Dtor); ++n;
    RegisterRuntimeAddr(0x18011a5d0ull, (void*)&RtUi_ScrollDescription_Dtor);

    RegisterRuntimeExport("??1SliderDescription@UI@@UEAA@XZ", (void*)&RtUi_SliderDescription_Dtor); ++n;
    RegisterRuntimeAddr(0x1800d7a00ull, (void*)&RtUi_SliderDescription_Dtor);

    RegisterRuntimeExport("??1SpinnerDescription@UI@@UEAA@XZ", (void*)&RtUi_SpinnerDescription_Dtor); ++n;
    RegisterRuntimeAddr(0x1800d8950ull, (void*)&RtUi_SpinnerDescription_Dtor);

    RegisterRuntimeExport("??1TabDescription@UI@@UEAA@XZ", (void*)&RtUi_TabDescription_Dtor); ++n;
    RegisterRuntimeAddr(0x1800da8e0ull, (void*)&RtUi_TabDescription_Dtor);

    RegisterRuntimeExport("??1TableDescription@UI@@UEAA@XZ", (void*)&RtUi_TableDescription_Dtor); ++n;
    RegisterRuntimeAddr(0x18011e6f0ull, (void*)&RtUi_TableDescription_Dtor);

    RegisterRuntimeExport("??1TextDescription@UI@@UEAA@XZ", (void*)&RtUi_TextDescription_Dtor); ++n;
    RegisterRuntimeAddr(0x18011eae0ull, (void*)&RtUi_TextDescription_Dtor);

    RegisterRuntimeExport("??1ComboElement@UI@@UEAA@XZ", (void*)&RtUi_ComboElement_Dtor); ++n;
    RegisterRuntimeAddr(0x180112490ull, (void*)&RtUi_ComboElement_Dtor);

    RegisterRuntimeExport("??1DocumentTextElement@UI@@UEAA@XZ", (void*)&RtUi_DocumentTextElement_Dtor); ++n;
    RegisterRuntimeAddr(0x1800cf320ull, (void*)&RtUi_DocumentTextElement_Dtor);

    RegisterRuntimeExport("??1EditElement@UI@@UEAA@XZ", (void*)&RtUi_EditElement_Dtor); ++n;
    RegisterRuntimeAddr(0x1800d0e50ull, (void*)&RtUi_EditElement_Dtor);



    return n;
}
