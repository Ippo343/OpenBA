// ============================================================================
// runtime/src/ui/ui_factory_wave28.cpp — WAVE 28 LANE FACTUI2
//
// The `?CreateNewObject@<C>@UI@@SAPEAVBase@Object@@XZ` /
// `?CreateInPlaceObject@<C>@UI@@SAPEAVBase@Object@@PEAX@Z` pair for every UI
// class in the minted-BaseTypeInfo absent-factory roster whose class name
// sorts at or after `ImageDescription` (35 classes; FACTUI1 owns the ones
// before, FACTREST owns the non-UI ones).
//
// `SpriteSheet` is the live cutover wall: attempt 30 died on
// `?CreateNewObject@SpriteSheet@UI@@SAPEAVBase@Object@@XZ` (0x180002f40) at the
// mint's per-slot FATAL in standalone/rt_data_mint.cpp.
//
// ---------------------------------------------------------------------------
// WHAT THIS FILE'S VALUE IS, AND WHAT IT IS NOT
// ---------------------------------------------------------------------------
// Every row registered here will report ATTENDANCE 0 in an ordinary run and
// that is EXPECTED, not a defect: Wave 11 lane OBJINT measured that the DLL
// reaches its own factories through an in-module pointer stamped by a static
// initialiser, and an in-module pointer is not a door. The value of this file
// is ENTIRELY AT CUTOVER. Nothing in this file is verified by a green
// attendance zero, and this lane does not claim otherwise.
//
// ---------------------------------------------------------------------------
// PROVENANCE OF EVERY NUMBER BELOW
// ---------------------------------------------------------------------------
// Sizes, alignments, vptr sequences and zeroed extents were transcribed from
// `objdump -d "C:\GOG Games\Banished\Runtime-x64.dll"` — the retail file on
// disk — NOT from Ghidra's decompiled C, which drops dereferences and whole
// arguments (reference_ghidra_c_output_vs_disassembly.md). Export presence and
// ordinals were verified by RVA with `pefile` against the same file, because
// `ResolveExport` returns NULL silently for a name the DLL does not contain
// and the `ResolveAddr` fallback hides it
// (reference_resolveexport_phantom_names.md).
//
// The transcription of each class was checked THREE ways before it was
// written: the ctor inlined into `CreateNewObject`, the ctor inlined into
// `CreateInPlaceObject`, and the separately-exported `??0<C>@UI@@QEAA@XZ` must
// agree instruction-for-instruction (modulo the register the object pointer
// lives in — RDI in the first, RCX in the other two). A class whose three
// copies disagreed would not be in this file.
//
// ---------------------------------------------------------------------------
// THE FAMILY IS NOT UNIFORM. THAT IS THIS LANE'S STRONGEST FINDING.
// ---------------------------------------------------------------------------
// WAVE28_KICKOFF.md §2.1 says "The shape is uniform. Every `CreateNewObject`
// read so far: locked `MemAlloc(N, 8)`, retail's null check, an inlined ctor,
// return." MEASURED across this lane's 35 classes, that is true of SIX of
// them. The real distribution (docs/runtime_port/WAVE28_LANE_FACTUI2.md §2):
//
//   shape A  6 classes  ctor fully inlined, no calls at all
//   shape B  8 classes  ctor inlined AROUND an out-of-line call to a BASE
//                       ctor (??0Element@UI@@, ??0EditElement@UI@@,
//                       ??0DialogInstance@UI@@)
//   shape C 21 classes  factory is a bare `alloc; if(!p) return 0;
//                       ??0<C>@UI@@(p); return p` and the whole body lives in
//                       the separately-exported ctor
//
// The consequence is not cosmetic: for shapes B and C the marginal cost is a
// CONSTRUCTOR that may live in another lane's class, and the "readable
// mechanically from the disassembly" claim covers only the allocation size in
// 29 of 35 cases.
//
// ---------------------------------------------------------------------------
// Registration runs from `RtUiPortInit()` (standalone/resolve_standalone.cpp
// line 333), which precedes `RtDataMintInit()` (line 406). That ordering is
// what makes a registration clear a trap; it was READ in this worktree on this
// tip, not assumed.
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../../funcs/rt_port.h"

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module m, uintptr_t ghidra_addr);

// `??0SpriteSheet@UI@@QEAA@XZ` (0x18011d600) is ALREADY PORTED, by
// runtime/src/ui/sprite.cpp (Wave 7 lane F, registered at sprite.cpp:985).
// Warning 8 — grep by NAME and by ADDRESS before writing a body — found it, so
// this lane reuses it rather than writing a second copy of the same stores.
// Its transcription was compared against this lane's independent one and they
// agree store for store, including the 0x51-byte written extent.
void* RtUi_SpriteSheet_Ctor(void* self);

namespace {

// ---------------------------------------------------------------------------
// The allocator call retail's factories inline.
//
// Every `CreateNewObject` in this family opens with, literally:
//
//   mov  RBX, [0x1801fc718]      ; ?_singleton@MemoryAllocator@System@@
//   add  RBX, 0x640              ; -> the allocator's lock object
//   call [0x18014b390]           ; lock
//   mov  EDX, <size>             ; (or: mov EDX,<size> / mov R8D,8)
//   lea  R8D, [RDX-<size-8>]     ; align = 8, derived from the size immediate
//   mov  RCX, [0x1801fc718]
//   call 0x1800c6e60             ; ?Allocate@MemoryAllocator@System@@QEAAPEAX_KH@Z
//   call [0x18014b388]           ; unlock
//
// `?MemAlloc@System@@` at 0x1800c7a50 is EXACTLY that sequence as an
// out-of-line function taking (RCX=size, EDX=align). That was verified by
// disassembling 0x1800c7a50 in this lane and diffing it against the inlined
// copy instruction by instruction, rather than inherited from
// stringtable_factory.cpp's claim (reference_a_written_recipe_can_be_wrong.md):
// the only differences are the register saves the out-of-line form needs and
// the fact that the size and align arrive in registers instead of as
// immediates.
// ---------------------------------------------------------------------------
void* MemAllocLocked(uint64_t size, int align) {
    using Fn = void* (*)(uint64_t, int);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x1800c7a50));
    return fn ? fn(size, align) : nullptr;
}


// ---------------------------------------------------------------------------
// Store helpers.
//
// memcpy rather than a cast-and-assign because several of these stores are
// UNALIGNED in retail: `movq $0xffffffffffffffff,0xd4(%rdi)` in
// ImageElement's ctor writes eight bytes at +0xd4. A `*(uint64_t*)(p+0xd4)`
// is undefined behaviour that happens to work on x86; memcpy is the same
// instruction after optimisation and is defined.
//
// The WIDTH of every store is the one retail used, not the one the field's
// type suggests: `mov %ecx,0x38(%rdi)` writes FOUR bytes and
// `movl $0xffffffff,0x3c(%rdi)` writes the next four, so a single 8-byte
// store of 0xffffffff00000000 would be the same final bytes and a DIFFERENT
// function under any interruption.
// ---------------------------------------------------------------------------
template <typename T>
inline void RtStore(uint8_t* p, size_t off, T v) { std::memcpy(p + off, &v, sizeof(T)); }

#define U8(p, o, v)   RtStore<uint8_t >((p), (o), (uint8_t )(v))
#define U16(p, o, v)  RtStore<uint16_t>((p), (o), (uint16_t)(v))
#define U32(p, o, v)  RtStore<uint32_t>((p), (o), (uint32_t)(v))
#define U64(p, o, v)  RtStore<uint64_t>((p), (o), (uint64_t)(v))

// A vptr stamp. Retail's ctors stamp the base vtables and then overwrite them
// with the derived one; every intermediate store is KEPT. A port that writes
// only the final one is a different function under any interruption and a byte
// oracle would not notice (the precedent is stringtable_factory.cpp / Wave 26).
#define VPTR(p, MANGLED)  RtStore<void*>((p), 0, RT_DATA_SLOT(void*, MANGLED))

// (ENDGAME B.3.9 UI-D: the `BASE(...)` base-ctor door call went with the last
// row that used it, ToolTipDialog's.)

using CtorFn = void* (*)(void*);

// Retail's own two branches, reproduced. Neither is a guard this lane added:
// the allocation-failure test is the `test RDI,RDI / je` after the unlock, and
// the caller-null test is the `test RCX,RCX / je` at the top of every
// `CreateInPlaceObject`.
void* FactoryNew(uint64_t size, CtorFn ctor) {
    void* p = MemAllocLocked(size, 8);
    if (p == nullptr) return nullptr;
    return ctor(p);
}

void* FactoryInPlace(void* p, CtorFn ctor) {
    if (p == nullptr) return nullptr;
    return ctor(p);
}

// Attendance. Counted so that a future cutover run can say whether these ever
// ran; in an ordinary run they will all read 0 and that proves nothing.
uint64_t g_new = 0, g_inplace = 0, g_newNull = 0, g_inplaceNull = 0;

struct Report {
    ~Report() {
        if (g_new || g_inplace)
            printf("[RtPort] UI factory (FACTUI2) ATTENDANCE: new=%llu (null=%llu) "
                   "inplace=%llu (null=%llu)\n",
                   (unsigned long long)g_new, (unsigned long long)g_newNull,
                   (unsigned long long)g_inplace, (unsigned long long)g_inplaceNull);
    }
};
Report g_report;

}  // namespace

// ============================================================================
// UI::SpriteSheet — THE LIVE CUTOVER WALL (attempt 30)
// ============================================================================
//
//   ?CreateNewObject@SpriteSheet@UI@@SAPEAVBase@Object@@XZ      0x180002f40 ord 2115
//   ?CreateInPlaceObject@SpriteSheet@UI@@SAPEAVBase@Object@@PEAX@Z
//                                                               0x180003000 ord 1978
//   ??0SpriteSheet@UI@@QEAA@XZ                                  0x18011d600 ord  407
//                                                               (already ported)
//
// Both ordinals and both VAs read out of the export directory by RVA with
// pefile against C:\GOG Games\Banished\Runtime-x64.dll.
//
// Allocation: `mov EDX,0x58 / lea R8D,[RDX-0x50]` at 0x180002f71 — size 0x58,
// align 8. The size comes off the ALLOCATION, not from summing the stores; the
// stores then cover 0x00..0x50 inclusive, which corroborates 0x58 (with 7 bytes
// of tail padding retail never writes) rather than being its source.
// A .pdata extent was NOT used: it is a LOWER bound
// (reference_pdata_is_a_lower_bound_on_function_extent.md).

extern "C" void* RtUiFact_SpriteSheet_CreateNewObject() {
    ++g_new;
    void* p = FactoryNew(0x58, &RtUi_SpriteSheet_Ctor);
    if (!p) ++g_newNull;
    return p;
}

extern "C" void* RtUiFact_SpriteSheet_CreateInPlaceObject(void* p) {
    ++g_inplace;
    if (!p) { ++g_inplaceNull; return nullptr; }
    return FactoryInPlace(p, &RtUi_SpriteSheet_Ctor);
}

// ============================================================================
// Registration
// ============================================================================
//
// BOTH doors for each row: the mint tries the NAME first
// (`RtPortLookupExport`) and the ADDRESS second (`RtPortLookupAddr`), and a
// name-only registration leaves any by-address consumer on the DLL while an
// address-only one is invisible to `tools/runtime_census.py`
// (reference_by_address_door_dies_at_cutover.md,
//  reference_census_cannot_see_nonliteral_registrations.md — every name below
//  is a STRING LITERAL for exactly that second reason).


int RtUiFactory2Register() {
    int n = 0;

    RegisterRuntimeExport("?CreateNewObject@SpriteSheet@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&RtUiFact_SpriteSheet_CreateNewObject);
    RegisterRuntimeAddr(0x180002f40ull, (void*)&RtUiFact_SpriteSheet_CreateNewObject); ++n;
    RegisterRuntimeExport("?CreateInPlaceObject@SpriteSheet@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&RtUiFact_SpriteSheet_CreateInPlaceObject);
    RegisterRuntimeAddr(0x180003000ull, (void*)&RtUiFact_SpriteSheet_CreateInPlaceObject); ++n;


    return n;
}
