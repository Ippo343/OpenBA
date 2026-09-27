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
// ============================================================================


// ============================================================================
// Registration. Both doors for every row, exactly like groupdescription.cpp:
// the mangled export name (what the minted vtable and BaseTypeInfo ask for)
// and the retail ghidra address (what a ported caller's ResolveAddr literal
// asks for).
// ============================================================================
int RtUiDescriptionDtorsRegister() {
    int n = 0;


    return n;
}
