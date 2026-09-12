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
// KILL SWITCH — DEFAULT ON, DELIBERATELY
// ---------------------------------------------------------------------------
// `BANISHED_RT_UIFACT2=0` disables. Default ON follows
// `stringtable_factory.cpp`, NOT `dialog_factory.cpp`: the latter's opt-in
// `BANISHED_RT_DLGFACTORY` is why `Dialog@UI` has been ported since Wave 11 and
// has contributed nothing to the live factory count for seventeen waves. A
// registration that does not happen cannot clear a trap.
//
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

// An out-of-line base-class ctor call, exactly as retail makes it.
//
// Resolved through ResolveExport (the PORT-REGISTRY-AWARE resolver), NOT by
// address and NOT through ResolveExportRaw: that is what lets a port of the
// base ctor be reached instead of the DLL's copy
// (reference_resolveaddr_consults_port_registry_first.md,
//  reference_bypassed_port_needs_a_source_checker.md). Cached, because
// ResolveExport is a string search under the loader lock and calling it per
// construction is the mistake reference_rtoriginal_is_getprocaddress.md
// measured at 5.6 points of sim time.
//
// The null branch is NOT a guard over a faithful deref: at cutover, with the
// DLL absent, an unported base ctor resolves to nothing, and the honest
// behaviour is to SAY SO ONCE rather than jump to address 0 and leave an
// EXEC-of-0 whose crash log names the resolver instead of the missing symbol
// (reference_diagnostic_field_cannot_express_the_answer.md).
void RtFactBaseCall(const char* mangled, void* self, void** cache);
#define BASE(MANGLED, SELF) \
    do { static void* rt_base_cache__ = nullptr; \
         RtFactBaseCall((MANGLED), (SELF), &rt_base_cache__); } while (0)


void RtFactBaseCall(const char* mangled, void* self, void** cache) {
    using Fn = void* (*)(void*);
    Fn fn = reinterpret_cast<Fn>(RtResolveCached(mangled, cache));
    if (fn) { fn(self); return; }
    static int complained = 0;
    if (complained < 16) {
        ++complained;
        printf("[RtPort] FACTUI2: base ctor %s is UNRESOLVED -- the object is "
               "left UNCONSTRUCTED. Port it, or this factory is a wall that "
               "moved rather than one that cleared.\n", mangled);
        fflush(stdout);
    }
}

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


// --------------------------------------------------------------------------
// UI::ImageDescription   allocation 0x80, align 8
//   ?CreateNewObject@ImageDescription@UI@@SAPEAVBase@Object@@XZ    0x180035dc0 ord 2074
//   ?CreateInPlaceObject@ImageDescription@UI@@SAPEAVBase@Object@@PEAX@Z 0x180035eb0 ord 1937
//   ??0ImageDescription@UI@@QEAA@XZ                                0x1801182d0 ord 231
void* Ctor_ImageDescription(void* p) {
    uint8_t* self = static_cast<uint8_t*>(p);
    VPTR(self, "??_7Base@Object@@6B@");
    VPTR(self, "??_7External@Object@@6B@");
    U64(self, 0x08, 0);
    U64(self, 0x10, 0);
    U64(self, 0x18, 0);
    VPTR(self, "??_7ElementDescription@UI@@6B@");
    U64(self, 0x20, 0);
    U64(self, 0x28, 0);
    U64(self, 0x30, 0);
    U32(self, 0x38, 0);
    U32(self, 0x3c, 0xffffffffu);
    U64(self, 0x40, 0x3ull);
    U64(self, 0x48, 0);
    U64(self, 0x50, 0);
    U64(self, 0x58, 0);
    U64(self, 0x60, 0);
    VPTR(self, "??_7ImageDescription@UI@@6B@");
    U64(self, 0x68, 0);
    U64(self, 0x70, 0);
    U64(self, 0x78, 0xffffffffffffffffull);
    return p;
}

extern "C" void* RtUiFact_ImageDescription_CreateNewObject() {
    ++g_new;
    void* q = FactoryNew(0x80, &Ctor_ImageDescription);
    if (!q) ++g_newNull;
    return q;
}
extern "C" void* RtUiFact_ImageDescription_CreateInPlaceObject(void* p) {
    ++g_inplace;
    if (!p) { ++g_inplaceNull; return nullptr; }
    return FactoryInPlace(p, &Ctor_ImageDescription);
}

// --------------------------------------------------------------------------
// UI::ImageElement   allocation 0xe0, align 8
//   ?CreateNewObject@ImageElement@UI@@SAPEAVBase@Object@@XZ        0x1800363a0 ord 2075
//   ?CreateInPlaceObject@ImageElement@UI@@SAPEAVBase@Object@@PEAX@Z 0x180036450 ord 1938
//   ??0ImageElement@UI@@QEAA@XZ                                    0x180118410 ord 233
void* Ctor_ImageElement(void* p) {
    uint8_t* self = static_cast<uint8_t*>(p);
    BASE("??0Element@UI@@QEAA@XZ", self);
    VPTR(self, "??_7ImageElement@UI@@6B@");
    U64(self, 0xc0, 0);
    U32(self, 0xc8, 0xffffffffu);
    U32(self, 0xd0, 0);
    U64(self, 0xd4, 0xffffffffffffffffull);
    return p;
}

extern "C" void* RtUiFact_ImageElement_CreateNewObject() {
    ++g_new;
    void* q = FactoryNew(0xe0, &Ctor_ImageElement);
    if (!q) ++g_newNull;
    return q;
}
extern "C" void* RtUiFact_ImageElement_CreateInPlaceObject(void* p) {
    ++g_inplace;
    if (!p) { ++g_inplaceNull; return nullptr; }
    return FactoryInPlace(p, &Ctor_ImageElement);
}

// --------------------------------------------------------------------------
// UI::LabelDescription   allocation 0xb8, align 8
//   ?CreateNewObject@LabelDescription@UI@@SAPEAVBase@Object@@XZ    0x1800424a0 ord 2077
//   ?CreateInPlaceObject@LabelDescription@UI@@SAPEAVBase@Object@@PEAX@Z 0x180042520 ord 1940
//   ??0LabelDescription@UI@@QEAA@XZ                                0x1800d29d0 ord 246
// SHAPE C: the factory pair is a bare `alloc / null test / call the
// exported ctor`. The whole body lives in ??0LabelDescription@UI@@QEAA@XZ.
// That ctor IS ported below, so this row does not merely move the wall.
void* Ctor_LabelDescription(void* p) {
    uint8_t* self = static_cast<uint8_t*>(p);
    BASE("??0TextDescription@UI@@QEAA@XZ", self);
    VPTR(self, "??_7LabelDescription@UI@@6B@");
    U32(self, 0x90, 0x3u);
    U64(self, 0x94, 0x1ull);
    U32(self, 0x9c, 0x2u);
    U64(self, 0xa0, 0xffffffffffffffffull);
    U64(self, 0xa8, 0);
    U64(self, 0xb0, 0);
    return p;
}

extern "C" void* RtUiFact_LabelDescription_CreateNewObject() {
    ++g_new;
    void* q = FactoryNew(0xb8, &Ctor_LabelDescription);
    if (!q) ++g_newNull;
    return q;
}
extern "C" void* RtUiFact_LabelDescription_CreateInPlaceObject(void* p) {
    ++g_inplace;
    if (!p) { ++g_inplaceNull; return nullptr; }
    return FactoryInPlace(p, &Ctor_LabelDescription);
}

// --------------------------------------------------------------------------
// UI::LabelElement   allocation 0x130, align 8
//   ?CreateNewObject@LabelElement@UI@@SAPEAVBase@Object@@XZ        0x1800428e0 ord 2078
//   ?CreateInPlaceObject@LabelElement@UI@@SAPEAVBase@Object@@PEAX@Z 0x180042960 ord 1941
//   ??0LabelElement@UI@@QEAA@XZ                                    0x1800d2620 ord 248
// SHAPE C: the factory pair is a bare `alloc / null test / call the
// exported ctor`. The whole body lives in ??0LabelElement@UI@@QEAA@XZ.
// That ctor IS ported below, so this row does not merely move the wall.
void* Ctor_LabelElement(void* p) {
    uint8_t* self = static_cast<uint8_t*>(p);
    BASE("??0TextElement@UI@@QEAA@XZ", self);
    VPTR(self, "??_7LabelElement@UI@@6B@");
    U64(self, 0x100, 0);
    U32(self, 0x108, 0xffffffffu);
    U32(self, 0x110, 0x3u);
    U64(self, 0x114, 0x1ull);
    U32(self, 0x11c, 0x2u);
    U64(self, 0x120, 0xffffffffffffffffull);
    U32(self, 0x128, 0xffffffffu);
    U32(self, 0x12c, 0);
    return p;
}

extern "C" void* RtUiFact_LabelElement_CreateNewObject() {
    ++g_new;
    void* q = FactoryNew(0x130, &Ctor_LabelElement);
    if (!q) ++g_newNull;
    return q;
}
extern "C" void* RtUiFact_LabelElement_CreateInPlaceObject(void* p) {
    ++g_inplace;
    if (!p) { ++g_inplaceNull; return nullptr; }
    return FactoryInPlace(p, &Ctor_LabelElement);
}

// --------------------------------------------------------------------------
// UI::LayoutDescription   allocation 0x78, align 8
//   ?CreateNewObject@LayoutDescription@UI@@SAPEAVBase@Object@@XZ   0x180042e30 ord 2079
//   ?CreateInPlaceObject@LayoutDescription@UI@@SAPEAVBase@Object@@PEAX@Z 0x180042f20 ord 1942
//   ??0LayoutDescription@UI@@QEAA@XZ                               0x1800d3a90 ord 250
void* Ctor_LayoutDescription(void* p) {
    uint8_t* self = static_cast<uint8_t*>(p);
    VPTR(self, "??_7Base@Object@@6B@");
    VPTR(self, "??_7External@Object@@6B@");
    U64(self, 0x08, 0);
    U64(self, 0x10, 0);
    U64(self, 0x18, 0);
    VPTR(self, "??_7ElementDescription@UI@@6B@");
    U64(self, 0x20, 0);
    U64(self, 0x28, 0);
    U64(self, 0x30, 0);
    U32(self, 0x38, 0);
    U32(self, 0x3c, 0xffffffffu);
    U64(self, 0x40, 0x3ull);
    U64(self, 0x48, 0);
    U64(self, 0x50, 0);
    U64(self, 0x58, 0);
    U64(self, 0x60, 0);
    VPTR(self, "??_7LayoutDescription@UI@@6B@");
    U64(self, 0x68, 0);
    U64(self, 0x70, 0);
    return p;
}

extern "C" void* RtUiFact_LayoutDescription_CreateNewObject() {
    ++g_new;
    void* q = FactoryNew(0x78, &Ctor_LayoutDescription);
    if (!q) ++g_newNull;
    return q;
}
extern "C" void* RtUiFact_LayoutDescription_CreateInPlaceObject(void* p) {
    ++g_inplace;
    if (!p) { ++g_inplaceNull; return nullptr; }
    return FactoryInPlace(p, &Ctor_LayoutDescription);
}

// --------------------------------------------------------------------------
// UI::LayoutElement   allocation 0xd0, align 8
//   ?CreateNewObject@LayoutElement@UI@@SAPEAVBase@Object@@XZ       0x180043380 ord 2080
//   ?CreateInPlaceObject@LayoutElement@UI@@SAPEAVBase@Object@@PEAX@Z 0x180043420 ord 1943
//   ??0LayoutElement@UI@@QEAA@XZ                                   0x1800d32e0 ord 252
void* Ctor_LayoutElement(void* p) {
    uint8_t* self = static_cast<uint8_t*>(p);
    BASE("??0Element@UI@@QEAA@XZ", self);
    VPTR(self, "??_7LayoutElement@UI@@6B@");
    U64(self, 0xc0, 0);
    U64(self, 0xc8, 0);
    return p;
}

extern "C" void* RtUiFact_LayoutElement_CreateNewObject() {
    ++g_new;
    void* q = FactoryNew(0xd0, &Ctor_LayoutElement);
    if (!q) ++g_newNull;
    return q;
}
extern "C" void* RtUiFact_LayoutElement_CreateInPlaceObject(void* p) {
    ++g_inplace;
    if (!p) { ++g_inplaceNull; return nullptr; }
    return FactoryInPlace(p, &Ctor_LayoutElement);
}

// --------------------------------------------------------------------------
// UI::ListDescription   allocation 0x90, align 8
//   ?CreateNewObject@ListDescription@UI@@SAPEAVBase@Object@@XZ     0x180044c90 ord 2081
//   ?CreateInPlaceObject@ListDescription@UI@@SAPEAVBase@Object@@PEAX@Z 0x180044d10 ord 1944
//   ??0ListDescription@UI@@QEAA@XZ                                 0x1800d49b0 ord 256
// SHAPE C: the factory pair is a bare `alloc / null test / call the
// exported ctor`. The whole body lives in ??0ListDescription@UI@@QEAA@XZ.
// That ctor IS ported below, so this row does not merely move the wall.
void* Ctor_ListDescription(void* p) {
    uint8_t* self = static_cast<uint8_t*>(p);
    VPTR(self, "??_7Base@Object@@6B@");
    VPTR(self, "??_7External@Object@@6B@");
    U64(self, 0x08, 0);
    U64(self, 0x10, 0);
    U64(self, 0x18, 0);
    VPTR(self, "??_7ElementDescription@UI@@6B@");
    U64(self, 0x20, 0);
    U64(self, 0x28, 0);
    U64(self, 0x30, 0);
    U32(self, 0x38, 0);
    U32(self, 0x3c, 0xffffffffu);
    U64(self, 0x40, 0x3ull);
    U64(self, 0x48, 0);
    U64(self, 0x50, 0);
    U64(self, 0x58, 0);
    U64(self, 0x60, 0);
    VPTR(self, "??_7ListDescription@UI@@6B@");
    U64(self, 0x68, 0);
    U64(self, 0x70, 0);
    U64(self, 0x78, 0);
    U64(self, 0x80, 0);
    U64(self, 0x88, 0xffffffffff808080ull);
    return p;
}

extern "C" void* RtUiFact_ListDescription_CreateNewObject() {
    ++g_new;
    void* q = FactoryNew(0x90, &Ctor_ListDescription);
    if (!q) ++g_newNull;
    return q;
}
extern "C" void* RtUiFact_ListDescription_CreateInPlaceObject(void* p) {
    ++g_inplace;
    if (!p) { ++g_inplaceNull; return nullptr; }
    return FactoryInPlace(p, &Ctor_ListDescription);
}

// --------------------------------------------------------------------------
// UI::ListElement   allocation 0x100, align 8
//   ?CreateNewObject@ListElement@UI@@SAPEAVBase@Object@@XZ         0x180045080 ord 2082
//   ?CreateInPlaceObject@ListElement@UI@@SAPEAVBase@Object@@PEAX@Z 0x180045100 ord 1945
//   ??0ListElement@UI@@QEAA@XZ                                     0x1800d4570 ord 258
// SHAPE C: the factory pair is a bare `alloc / null test / call the
// exported ctor`. The whole body lives in ??0ListElement@UI@@QEAA@XZ.
// That ctor IS ported below, so this row does not merely move the wall.
void* Ctor_ListElement(void* p) {
    uint8_t* self = static_cast<uint8_t*>(p);
    BASE("??0Element@UI@@QEAA@XZ", self);
    VPTR(self, "??_7ListElement@UI@@6B@");
    U64(self, 0xc0, 0);
    U32(self, 0xc8, 0xffffffffu);
    U64(self, 0xd0, 0xffffffffff808080ull);
    U64(self, 0xd8, 0xffffffffffffffffull);
    U64(self, 0xe0, 0);
    U64(self, 0xe8, 0);
    U64(self, 0xf0, 0);
    U64(self, 0xf8, 0);
    return p;
}

extern "C" void* RtUiFact_ListElement_CreateNewObject() {
    ++g_new;
    void* q = FactoryNew(0x100, &Ctor_ListElement);
    if (!q) ++g_newNull;
    return q;
}
extern "C" void* RtUiFact_ListElement_CreateInPlaceObject(void* p) {
    ++g_inplace;
    if (!p) { ++g_inplaceNull; return nullptr; }
    return FactoryInPlace(p, &Ctor_ListElement);
}

// --------------------------------------------------------------------------
// UI::NumberDescription   allocation 0x88, align 8
//   ?CreateNewObject@NumberDescription@UI@@SAPEAVBase@Object@@XZ   0x18004a330 ord 2085
//   ?CreateInPlaceObject@NumberDescription@UI@@SAPEAVBase@Object@@PEAX@Z 0x18004a420 ord 1948
//   ??0NumberDescription@UI@@QEAA@XZ                               0x180118b60 ord 281
void* Ctor_NumberDescription(void* p) {
    uint8_t* self = static_cast<uint8_t*>(p);
    VPTR(self, "??_7Base@Object@@6B@");
    VPTR(self, "??_7External@Object@@6B@");
    U64(self, 0x08, 0);
    U64(self, 0x10, 0);
    U64(self, 0x18, 0);
    VPTR(self, "??_7ElementDescription@UI@@6B@");
    U64(self, 0x20, 0);
    U64(self, 0x28, 0);
    U64(self, 0x30, 0);
    U32(self, 0x38, 0);
    U32(self, 0x3c, 0xffffffffu);
    U64(self, 0x40, 0x3ull);
    U64(self, 0x48, 0);
    U64(self, 0x50, 0);
    U64(self, 0x58, 0);
    U64(self, 0x60, 0);
    VPTR(self, "??_7NumberDescription@UI@@6B@");
    U64(self, 0x68, 0);
    U64(self, 0x70, 0);
    U32(self, 0x78, 0);
    U8(self, 0x7c, 0);
    return p;
}

extern "C" void* RtUiFact_NumberDescription_CreateNewObject() {
    ++g_new;
    void* q = FactoryNew(0x88, &Ctor_NumberDescription);
    if (!q) ++g_newNull;
    return q;
}
extern "C" void* RtUiFact_NumberDescription_CreateInPlaceObject(void* p) {
    ++g_inplace;
    if (!p) { ++g_inplaceNull; return nullptr; }
    return FactoryInPlace(p, &Ctor_NumberDescription);
}

// --------------------------------------------------------------------------
// UI::NumberEditDescription   allocation 0x88, align 8
//   ?CreateNewObject@NumberEditDescription@UI@@SAPEAVBase@Object@@XZ 0x180045f80 ord 2086
//   ?CreateInPlaceObject@NumberEditDescription@UI@@SAPEAVBase@Object@@PEAX@Z 0x180046000 ord 1949
//   ??0NumberEditDescription@UI@@QEAA@XZ                           0x1800d5050 ord 283
// SHAPE C: the factory pair is a bare `alloc / null test / call the
// exported ctor`. The whole body lives in ??0NumberEditDescription@UI@@QEAA@XZ.
// That ctor IS ported below, so this row does not merely move the wall.
void* Ctor_NumberEditDescription(void* p) {
    uint8_t* self = static_cast<uint8_t*>(p);
    VPTR(self, "??_7Base@Object@@6B@");
    VPTR(self, "??_7External@Object@@6B@");
    U64(self, 0x08, 0);
    U64(self, 0x10, 0);
    U64(self, 0x18, 0);
    VPTR(self, "??_7ElementDescription@UI@@6B@");
    U64(self, 0x20, 0);
    U64(self, 0x28, 0);
    U64(self, 0x30, 0);
    U32(self, 0x38, 0);
    U32(self, 0x3c, 0xffffffffu);
    U64(self, 0x40, 0x3ull);
    U64(self, 0x48, 0);
    U64(self, 0x50, 0);
    U64(self, 0x58, 0);
    U64(self, 0x60, 0);
    VPTR(self, "??_7NumberEditDescription@UI@@6B@");
    U64(self, 0x68, 0);
    U32(self, 0x74, 0x6u);
    U32(self, 0x78, 0xffff8001u);
    U32(self, 0x7c, 0x7fffu);
    U8(self, 0x80, 0x1u);
    return p;
}

extern "C" void* RtUiFact_NumberEditDescription_CreateNewObject() {
    ++g_new;
    void* q = FactoryNew(0x88, &Ctor_NumberEditDescription);
    if (!q) ++g_newNull;
    return q;
}
extern "C" void* RtUiFact_NumberEditDescription_CreateInPlaceObject(void* p) {
    ++g_inplace;
    if (!p) { ++g_inplaceNull; return nullptr; }
    return FactoryInPlace(p, &Ctor_NumberEditDescription);
}

// --------------------------------------------------------------------------
// UI::NumberEditElement   allocation 0x100, align 8
//   ?CreateNewObject@NumberEditElement@UI@@SAPEAVBase@Object@@XZ   0x180046340 ord 2087
//   ?CreateInPlaceObject@NumberEditElement@UI@@SAPEAVBase@Object@@PEAX@Z 0x1800463e0 ord 1950
//   ??0NumberEditElement@UI@@QEAA@XZ                               0x1800d4c70 ord 285
void* Ctor_NumberEditElement(void* p) {
    uint8_t* self = static_cast<uint8_t*>(p);
    BASE("??0EditElement@UI@@QEAA@XZ", self);
    VPTR(self, "??_7NumberEditElement@UI@@6B@");
    U32(self, 0xf8, 0xffff8001u);
    U32(self, 0xfc, 0x8000u);
    return p;
}

extern "C" void* RtUiFact_NumberEditElement_CreateNewObject() {
    ++g_new;
    void* q = FactoryNew(0x100, &Ctor_NumberEditElement);
    if (!q) ++g_newNull;
    return q;
}
extern "C" void* RtUiFact_NumberEditElement_CreateInPlaceObject(void* p) {
    ++g_inplace;
    if (!p) { ++g_inplaceNull; return nullptr; }
    return FactoryInPlace(p, &Ctor_NumberEditElement);
}

// --------------------------------------------------------------------------
// UI::NumberElement   allocation 0xe8, align 8
//   ?CreateNewObject@NumberElement@UI@@SAPEAVBase@Object@@XZ       0x18004a910 ord 2088
//   ?CreateInPlaceObject@NumberElement@UI@@SAPEAVBase@Object@@PEAX@Z 0x18004a990 ord 1951
//   ??0NumberElement@UI@@QEAA@XZ                                   0x180118c70 ord 287
// SHAPE C: the factory pair is a bare `alloc / null test / call the
// exported ctor`. The whole body lives in ??0NumberElement@UI@@QEAA@XZ.
// The ctor is NOT ported: it calls ?CacheText@NumberElement@UI@@AEAAXXZ,
// a private member this lane does not own and did not read. The factory
// pair below therefore MOVES the cutover wall from the factory to the
// ctor rather than clearing it. Recorded as a residual.
void* Ctor_NumberElement(void* p) { BASE("??0NumberElement@UI@@QEAA@XZ", p); return p; }

extern "C" void* RtUiFact_NumberElement_CreateNewObject() {
    ++g_new;
    void* q = FactoryNew(0xe8, &Ctor_NumberElement);
    if (!q) ++g_newNull;
    return q;
}
extern "C" void* RtUiFact_NumberElement_CreateInPlaceObject(void* p) {
    ++g_inplace;
    if (!p) { ++g_inplaceNull; return nullptr; }
    return FactoryInPlace(p, &Ctor_NumberElement);
}

// --------------------------------------------------------------------------
// UI::PageDescription   allocation 0x80, align 8
//   ?CreateNewObject@PageDescription@UI@@SAPEAVBase@Object@@XZ     0x180046fd0 ord 2090
//   ?CreateInPlaceObject@PageDescription@UI@@SAPEAVBase@Object@@PEAX@Z 0x180047050 ord 1953
//   ??0PageDescription@UI@@QEAA@XZ                                 0x1800d54e0 ord 291
// SHAPE C: the factory pair is a bare `alloc / null test / call the
// exported ctor`. The whole body lives in ??0PageDescription@UI@@QEAA@XZ.
// That ctor IS ported below, so this row does not merely move the wall.
void* Ctor_PageDescription(void* p) {
    uint8_t* self = static_cast<uint8_t*>(p);
    VPTR(self, "??_7Base@Object@@6B@");
    VPTR(self, "??_7External@Object@@6B@");
    U64(self, 0x08, 0);
    U64(self, 0x10, 0);
    U64(self, 0x18, 0);
    VPTR(self, "??_7ElementDescription@UI@@6B@");
    U64(self, 0x20, 0);
    U64(self, 0x28, 0);
    U64(self, 0x30, 0);
    U32(self, 0x38, 0);
    U32(self, 0x3c, 0xffffffffu);
    U64(self, 0x40, 0x3ull);
    U64(self, 0x48, 0);
    U64(self, 0x50, 0);
    U64(self, 0x58, 0);
    U64(self, 0x60, 0);
    VPTR(self, "??_7GroupDescription@UI@@6B@");
    U64(self, 0x68, 0);
    U64(self, 0x70, 0);
    VPTR(self, "??_7PageDescription@UI@@6B@");
    U32(self, 0x78, 0);
    return p;
}

extern "C" void* RtUiFact_PageDescription_CreateNewObject() {
    ++g_new;
    void* q = FactoryNew(0x80, &Ctor_PageDescription);
    if (!q) ++g_newNull;
    return q;
}
extern "C" void* RtUiFact_PageDescription_CreateInPlaceObject(void* p) {
    ++g_inplace;
    if (!p) { ++g_inplaceNull; return nullptr; }
    return FactoryInPlace(p, &Ctor_PageDescription);
}

// --------------------------------------------------------------------------
// UI::PageElement   allocation 0xc8, align 8
//   ?CreateNewObject@PageElement@UI@@SAPEAVBase@Object@@XZ         0x180047360 ord 2091
//   ?CreateInPlaceObject@PageElement@UI@@SAPEAVBase@Object@@PEAX@Z 0x180047400 ord 1954
//   ??0PageElement@UI@@QEAA@XZ                                     0x1800d5260 ord 293
void* Ctor_PageElement(void* p) {
    uint8_t* self = static_cast<uint8_t*>(p);
    BASE("??0Element@UI@@QEAA@XZ", self);
    VPTR(self, "??_7GroupElement@UI@@6B@");
    VPTR(self, "??_7PageElement@UI@@6B@");
    U32(self, 0xc0, 0);
    return p;
}

extern "C" void* RtUiFact_PageElement_CreateNewObject() {
    ++g_new;
    void* q = FactoryNew(0xc8, &Ctor_PageElement);
    if (!q) ++g_newNull;
    return q;
}
extern "C" void* RtUiFact_PageElement_CreateInPlaceObject(void* p) {
    ++g_inplace;
    if (!p) { ++g_inplaceNull; return nullptr; }
    return FactoryInPlace(p, &Ctor_PageElement);
}

// --------------------------------------------------------------------------
// UI::ProgressDescription   allocation 0xb0, align 8
//   ?CreateNewObject@ProgressDescription@UI@@SAPEAVBase@Object@@XZ 0x180077860 ord 2097
//   ?CreateInPlaceObject@ProgressDescription@UI@@SAPEAVBase@Object@@PEAX@Z 0x1800778e0 ord 1960
//   ??0ProgressDescription@UI@@QEAA@XZ                             0x18011a210 ord 320
// SHAPE C: the factory pair is a bare `alloc / null test / call the
// exported ctor`. The whole body lives in ??0ProgressDescription@UI@@QEAA@XZ.
// That ctor IS ported below, so this row does not merely move the wall.
void* Ctor_ProgressDescription(void* p) {
    uint8_t* self = static_cast<uint8_t*>(p);
    VPTR(self, "??_7Base@Object@@6B@");
    VPTR(self, "??_7External@Object@@6B@");
    U64(self, 0x08, 0);
    U64(self, 0x10, 0);
    U64(self, 0x18, 0);
    VPTR(self, "??_7ElementDescription@UI@@6B@");
    U64(self, 0x20, 0);
    U64(self, 0x28, 0);
    U64(self, 0x30, 0);
    U32(self, 0x38, 0);
    U32(self, 0x3c, 0xffffffffu);
    U64(self, 0x40, 0x3ull);
    U64(self, 0x48, 0);
    U64(self, 0x50, 0);
    U64(self, 0x58, 0);
    U64(self, 0x60, 0);
    VPTR(self, "??_7ProgressDescription@UI@@6B@");
    U32(self, 0x68, 0);
    U64(self, 0x70, 0);
    U64(self, 0x78, 0);
    U64(self, 0x80, 0);
    U64(self, 0x88, 0);
    U64(self, 0x90, 0);
    U64(self, 0x98, 0);
    U64(self, 0xa0, 0);
    U64(self, 0xa8, 0);
    return p;
}

extern "C" void* RtUiFact_ProgressDescription_CreateNewObject() {
    ++g_new;
    void* q = FactoryNew(0xb0, &Ctor_ProgressDescription);
    if (!q) ++g_newNull;
    return q;
}
extern "C" void* RtUiFact_ProgressDescription_CreateInPlaceObject(void* p) {
    ++g_inplace;
    if (!p) { ++g_inplaceNull; return nullptr; }
    return FactoryInPlace(p, &Ctor_ProgressDescription);
}

// --------------------------------------------------------------------------
// UI::ProgressElement   allocation 0x130, align 8
//   ?CreateNewObject@ProgressElement@UI@@SAPEAVBase@Object@@XZ     0x180077e30 ord 2098
//   ?CreateInPlaceObject@ProgressElement@UI@@SAPEAVBase@Object@@PEAX@Z 0x180077eb0 ord 1961
//   ??0ProgressElement@UI@@QEAA@XZ                                 0x180119c40 ord 322
// SHAPE C: the factory pair is a bare `alloc / null test / call the
// exported ctor`. The whole body lives in ??0ProgressElement@UI@@QEAA@XZ.
// That ctor IS ported below, so this row does not merely move the wall.
void* Ctor_ProgressElement(void* p) {
    uint8_t* self = static_cast<uint8_t*>(p);
    BASE("??0Element@UI@@QEAA@XZ", self);
    VPTR(self, "??_7ProgressElement@UI@@6B@");
    U64(self, 0xc0, 0);
    U32(self, 0xc8, 0xffffffffu);
    U64(self, 0xd0, 0);
    U32(self, 0xd8, 0xffffffffu);
    U64(self, 0xe0, 0);
    U32(self, 0xe8, 0xffffffffu);
    U64(self, 0xf0, 0);
    U32(self, 0xf8, 0xffffffffu);
    U64(self, 0x100, 0);
    U32(self, 0x108, 0xffffffffu);
    U64(self, 0x110, 0);
    U32(self, 0x118, 0xffffffffu);
    U32(self, 0x120, 0xffffffffu);
    U32(self, 0x124, 0x3f800000u);
    U32(self, 0x128, 0x3e800000u);
    return p;
}

extern "C" void* RtUiFact_ProgressElement_CreateNewObject() {
    ++g_new;
    void* q = FactoryNew(0x130, &Ctor_ProgressElement);
    if (!q) ++g_newNull;
    return q;
}
extern "C" void* RtUiFact_ProgressElement_CreateInPlaceObject(void* p) {
    ++g_inplace;
    if (!p) { ++g_inplaceNull; return nullptr; }
    return FactoryInPlace(p, &Ctor_ProgressElement);
}

// --------------------------------------------------------------------------
// UI::RadioDescription   allocation 0x80, align 8
//   ?CreateNewObject@RadioDescription@UI@@SAPEAVBase@Object@@XZ    0x180047760 ord 2099
//   ?CreateInPlaceObject@RadioDescription@UI@@SAPEAVBase@Object@@PEAX@Z 0x1800477e0 ord 1962
//   ??0RadioDescription@UI@@QEAA@XZ                                0x1800d5b50 ord 331
// SHAPE C: the factory pair is a bare `alloc / null test / call the
// exported ctor`. The whole body lives in ??0RadioDescription@UI@@QEAA@XZ.
// That ctor IS ported below, so this row does not merely move the wall.
void* Ctor_RadioDescription(void* p) {
    uint8_t* self = static_cast<uint8_t*>(p);
    VPTR(self, "??_7Base@Object@@6B@");
    VPTR(self, "??_7External@Object@@6B@");
    U64(self, 0x08, 0);
    U64(self, 0x10, 0);
    U64(self, 0x18, 0);
    VPTR(self, "??_7ElementDescription@UI@@6B@");
    U64(self, 0x20, 0);
    U64(self, 0x28, 0);
    U64(self, 0x30, 0);
    U32(self, 0x38, 0);
    U32(self, 0x3c, 0xffffffffu);
    U64(self, 0x40, 0x3ull);
    U64(self, 0x48, 0);
    U64(self, 0x50, 0);
    U64(self, 0x58, 0);
    U64(self, 0x60, 0);
    VPTR(self, "??_7GroupDescription@UI@@6B@");
    U64(self, 0x68, 0);
    U64(self, 0x70, 0);
    VPTR(self, "??_7RadioDescription@UI@@6B@");
    U32(self, 0x78, 0xffffffffu);
    return p;
}

extern "C" void* RtUiFact_RadioDescription_CreateNewObject() {
    ++g_new;
    void* q = FactoryNew(0x80, &Ctor_RadioDescription);
    if (!q) ++g_newNull;
    return q;
}
extern "C" void* RtUiFact_RadioDescription_CreateInPlaceObject(void* p) {
    ++g_inplace;
    if (!p) { ++g_inplaceNull; return nullptr; }
    return FactoryInPlace(p, &Ctor_RadioDescription);
}

// --------------------------------------------------------------------------
// UI::RadioElement   allocation 0xd8, align 8
//   ?CreateNewObject@RadioElement@UI@@SAPEAVBase@Object@@XZ        0x180047af0 ord 2100
//   ?CreateInPlaceObject@RadioElement@UI@@SAPEAVBase@Object@@PEAX@Z 0x180047ba0 ord 1963
//   ??0RadioElement@UI@@QEAA@XZ                                    0x1800d5870 ord 333
void* Ctor_RadioElement(void* p) {
    uint8_t* self = static_cast<uint8_t*>(p);
    BASE("??0Element@UI@@QEAA@XZ", self);
    VPTR(self, "??_7GroupElement@UI@@6B@");
    VPTR(self, "??_7RadioElement@UI@@6B@");
    U64(self, 0xc0, 0);
    U64(self, 0xc8, 0);
    U32(self, 0xd0, 0xffffffffu);
    return p;
}

extern "C" void* RtUiFact_RadioElement_CreateNewObject() {
    ++g_new;
    void* q = FactoryNew(0xd8, &Ctor_RadioElement);
    if (!q) ++g_newNull;
    return q;
}
extern "C" void* RtUiFact_RadioElement_CreateInPlaceObject(void* p) {
    ++g_inplace;
    if (!p) { ++g_inplaceNull; return nullptr; }
    return FactoryInPlace(p, &Ctor_RadioElement);
}

// --------------------------------------------------------------------------
// UI::RibbonDescription   allocation 0x80, align 8
//   ?CreateNewObject@RibbonDescription@UI@@SAPEAVBase@Object@@XZ   0x180044220 ord 2103
//   ?CreateInPlaceObject@RibbonDescription@UI@@SAPEAVBase@Object@@PEAX@Z 0x180044310 ord 1966
//   ??0RibbonDescription@UI@@QEAA@XZ                               0x1800d5bf0 ord 356
void* Ctor_RibbonDescription(void* p) {
    uint8_t* self = static_cast<uint8_t*>(p);
    VPTR(self, "??_7Base@Object@@6B@");
    VPTR(self, "??_7External@Object@@6B@");
    U64(self, 0x08, 0);
    U64(self, 0x10, 0);
    U64(self, 0x18, 0);
    VPTR(self, "??_7ElementDescription@UI@@6B@");
    U64(self, 0x20, 0);
    U64(self, 0x28, 0);
    U64(self, 0x30, 0);
    U32(self, 0x38, 0);
    U32(self, 0x3c, 0xffffffffu);
    U64(self, 0x40, 0x3ull);
    U64(self, 0x48, 0);
    U64(self, 0x50, 0);
    U64(self, 0x58, 0);
    U64(self, 0x60, 0);
    VPTR(self, "??_7RibbonDescription@UI@@6B@");
    U16(self, 0x68, 0);
    U32(self, 0x6c, 0);
    U64(self, 0x70, 0);
    U64(self, 0x78, 0);
    return p;
}

extern "C" void* RtUiFact_RibbonDescription_CreateNewObject() {
    ++g_new;
    void* q = FactoryNew(0x80, &Ctor_RibbonDescription);
    if (!q) ++g_newNull;
    return q;
}
extern "C" void* RtUiFact_RibbonDescription_CreateInPlaceObject(void* p) {
    ++g_inplace;
    if (!p) { ++g_inplaceNull; return nullptr; }
    return FactoryInPlace(p, &Ctor_RibbonDescription);
}

// --------------------------------------------------------------------------
// UI::RibbonElement   allocation 0xd8, align 8
//   ?CreateNewObject@RibbonElement@UI@@SAPEAVBase@Object@@XZ       0x1800447d0 ord 2104
//   ?CreateInPlaceObject@RibbonElement@UI@@SAPEAVBase@Object@@PEAX@Z 0x180044880 ord 1967
//   ??0RibbonElement@UI@@QEAA@XZ                                   0x1800d5d10 ord 358
void* Ctor_RibbonElement(void* p) {
    uint8_t* self = static_cast<uint8_t*>(p);
    BASE("??0Element@UI@@QEAA@XZ", self);
    VPTR(self, "??_7RibbonElement@UI@@6B@");
    U16(self, 0xc0, 0);
    U32(self, 0xc4, 0);
    U64(self, 0xc8, 0);
    U64(self, 0xd0, 0);
    return p;
}

extern "C" void* RtUiFact_RibbonElement_CreateNewObject() {
    ++g_new;
    void* q = FactoryNew(0xd8, &Ctor_RibbonElement);
    if (!q) ++g_newNull;
    return q;
}
extern "C" void* RtUiFact_RibbonElement_CreateInPlaceObject(void* p) {
    ++g_inplace;
    if (!p) { ++g_inplaceNull; return nullptr; }
    return FactoryInPlace(p, &Ctor_RibbonElement);
}

// --------------------------------------------------------------------------
// UI::ScrollDescription   allocation 0xc8, align 8
//   ?CreateNewObject@ScrollDescription@UI@@SAPEAVBase@Object@@XZ   0x18003a210 ord 2105
//   ?CreateInPlaceObject@ScrollDescription@UI@@SAPEAVBase@Object@@PEAX@Z 0x18003a290 ord 1968
//   ??0ScrollDescription@UI@@QEAA@XZ                               0x18011a4e0 ord 378
// SHAPE C: the factory pair is a bare `alloc / null test / call the
// exported ctor`. The whole body lives in ??0ScrollDescription@UI@@QEAA@XZ.
// That ctor IS ported below, so this row does not merely move the wall.
void* Ctor_ScrollDescription(void* p) {
    uint8_t* self = static_cast<uint8_t*>(p);
    VPTR(self, "??_7Base@Object@@6B@");
    VPTR(self, "??_7External@Object@@6B@");
    U64(self, 0x08, 0);
    U64(self, 0x10, 0);
    U64(self, 0x18, 0);
    VPTR(self, "??_7ElementDescription@UI@@6B@");
    U64(self, 0x20, 0);
    U64(self, 0x28, 0);
    U64(self, 0x30, 0);
    U32(self, 0x38, 0);
    U32(self, 0x3c, 0xffffffffu);
    U64(self, 0x40, 0x3ull);
    U64(self, 0x48, 0);
    U64(self, 0x50, 0);
    U64(self, 0x58, 0);
    U64(self, 0x60, 0);
    VPTR(self, "??_7ScrollDescription@UI@@6B@");
    U64(self, 0x68, 0);
    U64(self, 0x70, 0);
    U64(self, 0x78, 0);
    U64(self, 0x80, 0);
    U64(self, 0x88, 0);
    U64(self, 0x90, 0);
    U64(self, 0x98, 0);
    U64(self, 0xa0, 0);
    U64(self, 0xa8, 0);
    U64(self, 0xb0, 0);
    U8(self, 0xb8, 0x1u);
    U32(self, 0xbc, 0x1u);
    U64(self, 0xc0, 0xffffffffffffffffull);
    return p;
}

extern "C" void* RtUiFact_ScrollDescription_CreateNewObject() {
    ++g_new;
    void* q = FactoryNew(0xc8, &Ctor_ScrollDescription);
    if (!q) ++g_newNull;
    return q;
}
extern "C" void* RtUiFact_ScrollDescription_CreateInPlaceObject(void* p) {
    ++g_inplace;
    if (!p) { ++g_inplaceNull; return nullptr; }
    return FactoryInPlace(p, &Ctor_ScrollDescription);
}

// --------------------------------------------------------------------------
// UI::ScrollElement   allocation 0x178, align 8
//   ?CreateNewObject@ScrollElement@UI@@SAPEAVBase@Object@@XZ       0x18003a940 ord 2106
//   ?CreateInPlaceObject@ScrollElement@UI@@SAPEAVBase@Object@@PEAX@Z 0x18003a9c0 ord 1969
//   ??0ScrollElement@UI@@QEAA@XZ                                   0x18011a7f0 ord 380
// SHAPE C: the factory pair is a bare `alloc / null test / call the
// exported ctor`. The whole body lives in ??0ScrollElement@UI@@QEAA@XZ.
// That ctor IS ported below, so this row does not merely move the wall.
void* Ctor_ScrollElement(void* p) {
    uint8_t* self = static_cast<uint8_t*>(p);
    BASE("??0Element@UI@@QEAA@XZ", self);
    VPTR(self, "??_7ScrollElement@UI@@6B@");
    U64(self, 0xc0, 0);
    U32(self, 0xc8, 0xffffffffu);
    U64(self, 0xd0, 0);
    U32(self, 0xd8, 0xffffffffu);
    U64(self, 0xe0, 0);
    U32(self, 0xe8, 0xffffffffu);
    U64(self, 0xf0, 0);
    U32(self, 0xf8, 0xffffffffu);
    U64(self, 0x100, 0);
    U32(self, 0x108, 0xffffffffu);
    U64(self, 0x110, 0);
    U32(self, 0x118, 0xffffffffu);
    U64(self, 0x120, 0);
    U32(self, 0x128, 0xffffffffu);
    U64(self, 0x130, 0);
    U32(self, 0x138, 0xffffffffu);
    U64(self, 0x140, 0);
    U32(self, 0x148, 0xffffffffu);
    U8(self, 0x150, 0x1u);
    U64(self, 0x154, 0xffffffffffffffffull);
    U32(self, 0x160, 0);
    U32(self, 0x164, 0x3eaaaaabu);
    U64(self, 0x168, 0x40800000ull);
    U32(self, 0x170, 0);
    return p;
}

extern "C" void* RtUiFact_ScrollElement_CreateNewObject() {
    ++g_new;
    void* q = FactoryNew(0x178, &Ctor_ScrollElement);
    if (!q) ++g_newNull;
    return q;
}
extern "C" void* RtUiFact_ScrollElement_CreateInPlaceObject(void* p) {
    ++g_inplace;
    if (!p) { ++g_inplaceNull; return nullptr; }
    return FactoryInPlace(p, &Ctor_ScrollElement);
}

// --------------------------------------------------------------------------
// UI::SliderDescription   allocation 0xb8, align 8
//   ?CreateNewObject@SliderDescription@UI@@SAPEAVBase@Object@@XZ   0x180049840 ord 2108
//   ?CreateInPlaceObject@SliderDescription@UI@@SAPEAVBase@Object@@PEAX@Z 0x1800498c0 ord 1971
//   ??0SliderDescription@UI@@QEAA@XZ                               0x1800d7b60 ord 385
// SHAPE C: the factory pair is a bare `alloc / null test / call the
// exported ctor`. The whole body lives in ??0SliderDescription@UI@@QEAA@XZ.
// That ctor IS ported below, so this row does not merely move the wall.
void* Ctor_SliderDescription(void* p) {
    uint8_t* self = static_cast<uint8_t*>(p);
    VPTR(self, "??_7Base@Object@@6B@");
    VPTR(self, "??_7External@Object@@6B@");
    U64(self, 0x08, 0);
    U64(self, 0x10, 0);
    U64(self, 0x18, 0);
    VPTR(self, "??_7ElementDescription@UI@@6B@");
    U64(self, 0x20, 0);
    U64(self, 0x28, 0);
    U64(self, 0x30, 0);
    U32(self, 0x38, 0);
    U32(self, 0x3c, 0xffffffffu);
    U64(self, 0x40, 0x3ull);
    U64(self, 0x48, 0);
    U64(self, 0x50, 0);
    U64(self, 0x58, 0);
    U64(self, 0x60, 0);
    VPTR(self, "??_7SliderDescription@UI@@6B@");
    U64(self, 0x68, 0);
    U64(self, 0x70, 0);
    U64(self, 0x78, 0);
    U64(self, 0x80, 0);
    U64(self, 0x88, 0);
    U64(self, 0x90, 0);
    U64(self, 0x98, 0);
    U64(self, 0xa0, 0);
    U32(self, 0xa8, 0);
    U32(self, 0xac, 0x3f800000u);
    U32(self, 0xb0, 0xau);
    return p;
}

extern "C" void* RtUiFact_SliderDescription_CreateNewObject() {
    ++g_new;
    void* q = FactoryNew(0xb8, &Ctor_SliderDescription);
    if (!q) ++g_newNull;
    return q;
}
extern "C" void* RtUiFact_SliderDescription_CreateInPlaceObject(void* p) {
    ++g_inplace;
    if (!p) { ++g_inplaceNull; return nullptr; }
    return FactoryInPlace(p, &Ctor_SliderDescription);
}

// --------------------------------------------------------------------------
// UI::SliderElement   allocation 0x130, align 8
//   ?CreateNewObject@SliderElement@UI@@SAPEAVBase@Object@@XZ       0x180049e10 ord 2109
//   ?CreateInPlaceObject@SliderElement@UI@@SAPEAVBase@Object@@PEAX@Z 0x180049e90 ord 1972
//   ??0SliderElement@UI@@QEAA@XZ                                   0x1800d7570 ord 387
// SHAPE C: the factory pair is a bare `alloc / null test / call the
// exported ctor`. The whole body lives in ??0SliderElement@UI@@QEAA@XZ.
// That ctor IS ported below, so this row does not merely move the wall.
void* Ctor_SliderElement(void* p) {
    uint8_t* self = static_cast<uint8_t*>(p);
    BASE("??0Element@UI@@QEAA@XZ", self);
    VPTR(self, "??_7SliderElement@UI@@6B@");
    U64(self, 0xc0, 0);
    U32(self, 0xc8, 0xffffffffu);
    U64(self, 0xd0, 0);
    U32(self, 0xd8, 0xffffffffu);
    U64(self, 0xe0, 0);
    U32(self, 0xe8, 0xffffffffu);
    U64(self, 0xf0, 0);
    U32(self, 0xf8, 0xffffffffu);
    U64(self, 0x100, 0);
    U32(self, 0x108, 0xffffffffu);
    U64(self, 0x110, 0);
    U32(self, 0x118, 0);
    U32(self, 0x11c, 0x3f800000u);
    U32(self, 0x120, 0xau);
    U8(self, 0x124, 0);
    U32(self, 0x128, 0x3e800000u);
    return p;
}

extern "C" void* RtUiFact_SliderElement_CreateNewObject() {
    ++g_new;
    void* q = FactoryNew(0x130, &Ctor_SliderElement);
    if (!q) ++g_newNull;
    return q;
}
extern "C" void* RtUiFact_SliderElement_CreateInPlaceObject(void* p) {
    ++g_inplace;
    if (!p) { ++g_inplaceNull; return nullptr; }
    return FactoryInPlace(p, &Ctor_SliderElement);
}

// --------------------------------------------------------------------------
// UI::SpinnerDescription   allocation 0x88, align 8
//   ?CreateNewObject@SpinnerDescription@UI@@SAPEAVBase@Object@@XZ  0x18004ae50 ord 2112
//   ?CreateInPlaceObject@SpinnerDescription@UI@@SAPEAVBase@Object@@PEAX@Z 0x18004aed0 ord 1975
//   ??0SpinnerDescription@UI@@QEAA@XZ                              0x1800d89c0 ord 393
// SHAPE C: the factory pair is a bare `alloc / null test / call the
// exported ctor`. The whole body lives in ??0SpinnerDescription@UI@@QEAA@XZ.
// That ctor IS ported below, so this row does not merely move the wall.
void* Ctor_SpinnerDescription(void* p) {
    uint8_t* self = static_cast<uint8_t*>(p);
    VPTR(self, "??_7Base@Object@@6B@");
    VPTR(self, "??_7External@Object@@6B@");
    U64(self, 0x08, 0);
    U64(self, 0x10, 0);
    U64(self, 0x18, 0);
    VPTR(self, "??_7ElementDescription@UI@@6B@");
    U64(self, 0x20, 0);
    U64(self, 0x28, 0);
    U64(self, 0x30, 0);
    U32(self, 0x38, 0);
    U32(self, 0x3c, 0xffffffffu);
    U64(self, 0x40, 0x3ull);
    U64(self, 0x48, 0);
    U64(self, 0x50, 0);
    U64(self, 0x58, 0);
    U64(self, 0x60, 0);
    VPTR(self, "??_7SpinnerDescription@UI@@6B@");
    U64(self, 0x68, 0);
    U64(self, 0x70, 0);
    U64(self, 0x78, 0);
    U32(self, 0x80, 0x1u);
    return p;
}

extern "C" void* RtUiFact_SpinnerDescription_CreateNewObject() {
    ++g_new;
    void* q = FactoryNew(0x88, &Ctor_SpinnerDescription);
    if (!q) ++g_newNull;
    return q;
}
extern "C" void* RtUiFact_SpinnerDescription_CreateInPlaceObject(void* p) {
    ++g_inplace;
    if (!p) { ++g_inplaceNull; return nullptr; }
    return FactoryInPlace(p, &Ctor_SpinnerDescription);
}

// --------------------------------------------------------------------------
// UI::SpinnerElement   allocation 0xc8, align 8
//   ?CreateNewObject@SpinnerElement@UI@@SAPEAVBase@Object@@XZ      0x18004b200 ord 2113
//   ?CreateInPlaceObject@SpinnerElement@UI@@SAPEAVBase@Object@@PEAX@Z 0x18004b290 ord 1976
//   ??0SpinnerElement@UI@@QEAA@XZ                                  0x1800d8670 ord 395
void* Ctor_SpinnerElement(void* p) {
    uint8_t* self = static_cast<uint8_t*>(p);
    BASE("??0Element@UI@@QEAA@XZ", self);
    VPTR(self, "??_7SpinnerElement@UI@@6B@");
    U32(self, 0xc0, 0x1u);
    return p;
}

extern "C" void* RtUiFact_SpinnerElement_CreateNewObject() {
    ++g_new;
    void* q = FactoryNew(0xc8, &Ctor_SpinnerElement);
    if (!q) ++g_newNull;
    return q;
}
extern "C" void* RtUiFact_SpinnerElement_CreateInPlaceObject(void* p) {
    ++g_inplace;
    if (!p) { ++g_inplaceNull; return nullptr; }
    return FactoryInPlace(p, &Ctor_SpinnerElement);
}

// --------------------------------------------------------------------------
// UI::SpriteCapture   allocation 0x58, align 8
//   ?CreateNewObject@SpriteCapture@UI@@SAPEAVBase@Object@@XZ       0x18003c900 ord 2114
//   ?CreateInPlaceObject@SpriteCapture@UI@@SAPEAVBase@Object@@PEAX@Z 0x18003c9c0 ord 1977
//   ??0SpriteCapture@UI@@QEAA@XZ                                   0x1800d9140 ord 403
void* Ctor_SpriteCapture(void* p) {
    uint8_t* self = static_cast<uint8_t*>(p);
    VPTR(self, "??_7Base@Object@@6B@");
    VPTR(self, "??_7External@Object@@6B@");
    U64(self, 0x08, 0);
    U64(self, 0x10, 0);
    U64(self, 0x18, 0);
    VPTR(self, "??_7SpriteCapture@UI@@6B@");
    U64(self, 0x20, 0);
    U64(self, 0x28, 0);
    U64(self, 0x30, 0);
    U64(self, 0x38, 0);
    U64(self, 0x40, 0);
    U64(self, 0x48, 0);
    U64(self, 0x50, 0);
    return p;
}

extern "C" void* RtUiFact_SpriteCapture_CreateNewObject() {
    ++g_new;
    void* q = FactoryNew(0x58, &Ctor_SpriteCapture);
    if (!q) ++g_newNull;
    return q;
}
extern "C" void* RtUiFact_SpriteCapture_CreateInPlaceObject(void* p) {
    ++g_inplace;
    if (!p) { ++g_inplaceNull; return nullptr; }
    return FactoryInPlace(p, &Ctor_SpriteCapture);
}

// --------------------------------------------------------------------------
// UI::TabDescription   allocation 0xb8, align 8
//   ?CreateNewObject@TabDescription@UI@@SAPEAVBase@Object@@XZ      0x18004c2e0 ord 2118
//   ?CreateInPlaceObject@TabDescription@UI@@SAPEAVBase@Object@@PEAX@Z 0x18004c360 ord 1981
//   ??0TabDescription@UI@@QEAA@XZ                                  0x1800daa00 ord 427
// SHAPE C: the factory pair is a bare `alloc / null test / call the
// exported ctor`. The whole body lives in ??0TabDescription@UI@@QEAA@XZ.
// That ctor IS ported below, so this row does not merely move the wall.
void* Ctor_TabDescription(void* p) {
    uint8_t* self = static_cast<uint8_t*>(p);
    VPTR(self, "??_7Base@Object@@6B@");
    VPTR(self, "??_7External@Object@@6B@");
    U64(self, 0x08, 0);
    U64(self, 0x10, 0);
    U64(self, 0x18, 0);
    VPTR(self, "??_7ElementDescription@UI@@6B@");
    U64(self, 0x20, 0);
    U64(self, 0x28, 0);
    U64(self, 0x30, 0);
    U32(self, 0x38, 0);
    U32(self, 0x3c, 0xffffffffu);
    U64(self, 0x40, 0x3ull);
    U64(self, 0x48, 0);
    U64(self, 0x50, 0);
    U64(self, 0x58, 0);
    U64(self, 0x60, 0);
    VPTR(self, "??_7TabDescription@UI@@6B@");
    U64(self, 0x68, 0);
    U64(self, 0x70, 0);
    U64(self, 0x78, 0);
    U64(self, 0x80, 0);
    U64(self, 0x88, 0);
    U64(self, 0x90, 0);
    U64(self, 0x98, 0);
    U64(self, 0xa8, 0);
    U64(self, 0xb0, 0);
    return p;
}

extern "C" void* RtUiFact_TabDescription_CreateNewObject() {
    ++g_new;
    void* q = FactoryNew(0xb8, &Ctor_TabDescription);
    if (!q) ++g_newNull;
    return q;
}
extern "C" void* RtUiFact_TabDescription_CreateInPlaceObject(void* p) {
    ++g_inplace;
    if (!p) { ++g_inplaceNull; return nullptr; }
    return FactoryInPlace(p, &Ctor_TabDescription);
}

// --------------------------------------------------------------------------
// UI::TabElement   allocation 0x108, align 8
//   ?CreateNewObject@TabElement@UI@@SAPEAVBase@Object@@XZ          0x18004c7f0 ord 2119
//   ?CreateInPlaceObject@TabElement@UI@@SAPEAVBase@Object@@PEAX@Z  0x18004c870 ord 1982
//   ??0TabElement@UI@@QEAA@XZ                                      0x1800da390 ord 429
// SHAPE C: the factory pair is a bare `alloc / null test / call the
// exported ctor`. The whole body lives in ??0TabElement@UI@@QEAA@XZ.
// That ctor IS ported below, so this row does not merely move the wall.
void* Ctor_TabElement(void* p) {
    uint8_t* self = static_cast<uint8_t*>(p);
    BASE("??0Element@UI@@QEAA@XZ", self);
    VPTR(self, "??_7TabElement@UI@@6B@");
    U64(self, 0xc0, 0);
    U64(self, 0xc8, 0);
    U32(self, 0xd0, 0xffffffffu);
    U64(self, 0xd8, 0);
    U32(self, 0xe0, 0xffffffffu);
    U64(self, 0xe8, 0);
    U32(self, 0xf0, 0xffffffffu);
    U64(self, 0xf8, 0);
    U32(self, 0x100, 0);
    return p;
}

extern "C" void* RtUiFact_TabElement_CreateNewObject() {
    ++g_new;
    void* q = FactoryNew(0x108, &Ctor_TabElement);
    if (!q) ++g_newNull;
    return q;
}
extern "C" void* RtUiFact_TabElement_CreateInPlaceObject(void* p) {
    ++g_inplace;
    if (!p) { ++g_inplaceNull; return nullptr; }
    return FactoryInPlace(p, &Ctor_TabElement);
}

// --------------------------------------------------------------------------
// UI::TableDescription   allocation 0x98, align 8
//   ?CreateNewObject@TableDescription@UI@@SAPEAVBase@Object@@XZ    0x180078fc0 ord 2120
//   ?CreateInPlaceObject@TableDescription@UI@@SAPEAVBase@Object@@PEAX@Z 0x180079040 ord 1983
//   ??0TableDescription@UI@@QEAA@XZ                                0x18011e7a0 ord 431
// SHAPE C: the factory pair is a bare `alloc / null test / call the
// exported ctor`. The whole body lives in ??0TableDescription@UI@@QEAA@XZ.
// That ctor IS ported below, so this row does not merely move the wall.
void* Ctor_TableDescription(void* p) {
    uint8_t* self = static_cast<uint8_t*>(p);
    VPTR(self, "??_7Base@Object@@6B@");
    VPTR(self, "??_7External@Object@@6B@");
    U64(self, 0x08, 0);
    U64(self, 0x10, 0);
    U64(self, 0x18, 0);
    VPTR(self, "??_7ElementDescription@UI@@6B@");
    U64(self, 0x20, 0);
    U64(self, 0x28, 0);
    U64(self, 0x30, 0);
    U32(self, 0x38, 0);
    U32(self, 0x3c, 0xffffffffu);
    U64(self, 0x40, 0x3ull);
    U64(self, 0x48, 0);
    U64(self, 0x50, 0);
    U64(self, 0x58, 0);
    U64(self, 0x60, 0);
    VPTR(self, "??_7TableDescription@UI@@6B@");
    U32(self, 0x68, 0x1u);
    U64(self, 0x6c, 0x1ull);
    U64(self, 0x78, 0);
    U64(self, 0x80, 0);
    U64(self, 0x88, 0);
    U64(self, 0x90, 0);
    return p;
}

extern "C" void* RtUiFact_TableDescription_CreateNewObject() {
    ++g_new;
    void* q = FactoryNew(0x98, &Ctor_TableDescription);
    if (!q) ++g_newNull;
    return q;
}
extern "C" void* RtUiFact_TableDescription_CreateInPlaceObject(void* p) {
    ++g_inplace;
    if (!p) { ++g_inplaceNull; return nullptr; }
    return FactoryInPlace(p, &Ctor_TableDescription);
}

// --------------------------------------------------------------------------
// UI::TableElement   allocation 0xf0, align 8
//   ?CreateNewObject@TableElement@UI@@SAPEAVBase@Object@@XZ        0x1800793c0 ord 2121
//   ?CreateInPlaceObject@TableElement@UI@@SAPEAVBase@Object@@PEAX@Z 0x180079440 ord 1984
//   ??0TableElement@UI@@QEAA@XZ                                    0x18011e2f0 ord 433
// SHAPE C: the factory pair is a bare `alloc / null test / call the
// exported ctor`. The whole body lives in ??0TableElement@UI@@QEAA@XZ.
// That ctor IS ported below, so this row does not merely move the wall.
void* Ctor_TableElement(void* p) {
    uint8_t* self = static_cast<uint8_t*>(p);
    BASE("??0Element@UI@@QEAA@XZ", self);
    VPTR(self, "??_7TableElement@UI@@6B@");
    U64(self, 0xc0, 0);
    U32(self, 0xc8, 0);
    U64(self, 0xd0, 0);
    U64(self, 0xd8, 0);
    U64(self, 0xe0, 0);
    U64(self, 0xe8, 0);
    return p;
}

extern "C" void* RtUiFact_TableElement_CreateNewObject() {
    ++g_new;
    void* q = FactoryNew(0xf0, &Ctor_TableElement);
    if (!q) ++g_newNull;
    return q;
}
extern "C" void* RtUiFact_TableElement_CreateInPlaceObject(void* p) {
    ++g_inplace;
    if (!p) { ++g_inplaceNull; return nullptr; }
    return FactoryInPlace(p, &Ctor_TableElement);
}

// --------------------------------------------------------------------------
// UI::TextDescription   allocation 0x90, align 8
//   ?CreateNewObject@TextDescription@UI@@SAPEAVBase@Object@@XZ     0x180041bc0 ord 2122
//   ?CreateInPlaceObject@TextDescription@UI@@SAPEAVBase@Object@@PEAX@Z 0x180041c40 ord 1985
//   ??0TextDescription@UI@@QEAA@XZ                                 0x18011ea20 ord 435
// SHAPE C: the factory pair is a bare `alloc / null test / call the
// exported ctor`. The whole body lives in ??0TextDescription@UI@@QEAA@XZ.
// That ctor IS ported below, so this row does not merely move the wall.
void* Ctor_TextDescription(void* p) {
    uint8_t* self = static_cast<uint8_t*>(p);
    VPTR(self, "??_7Base@Object@@6B@");
    VPTR(self, "??_7External@Object@@6B@");
    U64(self, 0x08, 0);
    U64(self, 0x10, 0);
    U64(self, 0x18, 0);
    VPTR(self, "??_7ElementDescription@UI@@6B@");
    U64(self, 0x20, 0);
    U64(self, 0x28, 0);
    U64(self, 0x30, 0);
    U32(self, 0x38, 0);
    U32(self, 0x3c, 0xffffffffu);
    U64(self, 0x40, 0x3ull);
    U64(self, 0x48, 0);
    U64(self, 0x50, 0);
    U64(self, 0x58, 0);
    U64(self, 0x60, 0);
    VPTR(self, "??_7TextDescription@UI@@6B@");
    U64(self, 0x68, 0);
    U64(self, 0x70, 0);
    U64(self, 0x78, 0);
    U32(self, 0x80, 0);
    U16(self, 0x84, 0);
    U32(self, 0x88, 0xc8u);
    U32(self, 0x8c, 0xffffffffu);
    return p;
}

extern "C" void* RtUiFact_TextDescription_CreateNewObject() {
    ++g_new;
    void* q = FactoryNew(0x90, &Ctor_TextDescription);
    if (!q) ++g_newNull;
    return q;
}
extern "C" void* RtUiFact_TextDescription_CreateInPlaceObject(void* p) {
    ++g_inplace;
    if (!p) { ++g_inplaceNull; return nullptr; }
    return FactoryInPlace(p, &Ctor_TextDescription);
}

// --------------------------------------------------------------------------
// UI::TextElement   allocation 0x100, align 8
//   ?CreateNewObject@TextElement@UI@@SAPEAVBase@Object@@XZ         0x180041fe0 ord 2123
//   ?CreateInPlaceObject@TextElement@UI@@SAPEAVBase@Object@@PEAX@Z 0x180042060 ord 1986
//   ??0TextElement@UI@@QEAA@XZ                                     0x18011eb80 ord 437
// SHAPE C: the factory pair is a bare `alloc / null test / call the
// exported ctor`. The whole body lives in ??0TextElement@UI@@QEAA@XZ.
// The ctor is NOT ported: it calls ?CacheText@TextElement@UI@@AEAAXXZ,
// a private member this lane does not own and did not read. The factory
// pair below therefore MOVES the cutover wall from the factory to the
// ctor rather than clearing it. Recorded as a residual.
void* Ctor_TextElement(void* p) { BASE("??0TextElement@UI@@QEAA@XZ", p); return p; }

extern "C" void* RtUiFact_TextElement_CreateNewObject() {
    ++g_new;
    void* q = FactoryNew(0x100, &Ctor_TextElement);
    if (!q) ++g_newNull;
    return q;
}
extern "C" void* RtUiFact_TextElement_CreateInPlaceObject(void* p) {
    ++g_inplace;
    if (!p) { ++g_inplaceNull; return nullptr; }
    return FactoryInPlace(p, &Ctor_TextElement);
}

// --------------------------------------------------------------------------
// UI::ToolTipDialog   allocation 0xc0, align 8
//   ?CreateNewObject@ToolTipDialog@UI@@SAPEAVBase@Object@@XZ       0x18004d0b0 ord 2124
//   ?CreateInPlaceObject@ToolTipDialog@UI@@SAPEAVBase@Object@@PEAX@Z 0x18004d140 ord 1987
//   ??0ToolTipDialog@UI@@QEAA@XZ                                   0x1800dad00 ord 449
void* Ctor_ToolTipDialog(void* p) {
    uint8_t* self = static_cast<uint8_t*>(p);
    BASE("??0DialogInstance@UI@@QEAA@XZ", self);
    VPTR(self, "??_7ToolTipDialog@UI@@6B@");
    return p;
}

extern "C" void* RtUiFact_ToolTipDialog_CreateNewObject() {
    ++g_new;
    void* q = FactoryNew(0xc0, &Ctor_ToolTipDialog);
    if (!q) ++g_newNull;
    return q;
}
extern "C" void* RtUiFact_ToolTipDialog_CreateInPlaceObject(void* p) {
    ++g_inplace;
    if (!p) { ++g_inplaceNull; return nullptr; }
    return FactoryInPlace(p, &Ctor_ToolTipDialog);
}

int RtUiFactory2Register() {
    if (!RtWaveEnabled("BANISHED_RT_UIFACT2")) return 0;
    int n = 0;

    RegisterRuntimeExport("?CreateNewObject@SpriteSheet@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&RtUiFact_SpriteSheet_CreateNewObject);
    RegisterRuntimeAddr(0x180002f40ull, (void*)&RtUiFact_SpriteSheet_CreateNewObject); ++n;
    RegisterRuntimeExport("?CreateInPlaceObject@SpriteSheet@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&RtUiFact_SpriteSheet_CreateInPlaceObject);
    RegisterRuntimeAddr(0x180003000ull, (void*)&RtUiFact_SpriteSheet_CreateInPlaceObject); ++n;

    RegisterRuntimeExport("?CreateNewObject@ImageDescription@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&RtUiFact_ImageDescription_CreateNewObject);
    RegisterRuntimeAddr(0x180035dc0ull, (void*)&RtUiFact_ImageDescription_CreateNewObject); ++n;
    RegisterRuntimeExport("?CreateInPlaceObject@ImageDescription@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&RtUiFact_ImageDescription_CreateInPlaceObject);
    RegisterRuntimeAddr(0x180035eb0ull, (void*)&RtUiFact_ImageDescription_CreateInPlaceObject); ++n;
    RegisterRuntimeExport("??0ImageDescription@UI@@QEAA@XZ", (void*)&Ctor_ImageDescription);
    RegisterRuntimeAddr(0x1801182d0ull, (void*)&Ctor_ImageDescription); ++n;

    RegisterRuntimeExport("?CreateNewObject@ImageElement@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&RtUiFact_ImageElement_CreateNewObject);
    RegisterRuntimeAddr(0x1800363a0ull, (void*)&RtUiFact_ImageElement_CreateNewObject); ++n;
    RegisterRuntimeExport("?CreateInPlaceObject@ImageElement@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&RtUiFact_ImageElement_CreateInPlaceObject);
    RegisterRuntimeAddr(0x180036450ull, (void*)&RtUiFact_ImageElement_CreateInPlaceObject); ++n;
    RegisterRuntimeExport("??0ImageElement@UI@@QEAA@XZ", (void*)&Ctor_ImageElement);
    RegisterRuntimeAddr(0x180118410ull, (void*)&Ctor_ImageElement); ++n;

    RegisterRuntimeExport("?CreateNewObject@LabelDescription@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&RtUiFact_LabelDescription_CreateNewObject);
    RegisterRuntimeAddr(0x1800424a0ull, (void*)&RtUiFact_LabelDescription_CreateNewObject); ++n;
    RegisterRuntimeExport("?CreateInPlaceObject@LabelDescription@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&RtUiFact_LabelDescription_CreateInPlaceObject);
    RegisterRuntimeAddr(0x180042520ull, (void*)&RtUiFact_LabelDescription_CreateInPlaceObject); ++n;
    RegisterRuntimeExport("??0LabelDescription@UI@@QEAA@XZ", (void*)&Ctor_LabelDescription);
    RegisterRuntimeAddr(0x1800d29d0ull, (void*)&Ctor_LabelDescription); ++n;

    RegisterRuntimeExport("?CreateNewObject@LabelElement@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&RtUiFact_LabelElement_CreateNewObject);
    RegisterRuntimeAddr(0x1800428e0ull, (void*)&RtUiFact_LabelElement_CreateNewObject); ++n;
    RegisterRuntimeExport("?CreateInPlaceObject@LabelElement@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&RtUiFact_LabelElement_CreateInPlaceObject);
    RegisterRuntimeAddr(0x180042960ull, (void*)&RtUiFact_LabelElement_CreateInPlaceObject); ++n;
    RegisterRuntimeExport("??0LabelElement@UI@@QEAA@XZ", (void*)&Ctor_LabelElement);
    RegisterRuntimeAddr(0x1800d2620ull, (void*)&Ctor_LabelElement); ++n;

    RegisterRuntimeExport("?CreateNewObject@LayoutDescription@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&RtUiFact_LayoutDescription_CreateNewObject);
    RegisterRuntimeAddr(0x180042e30ull, (void*)&RtUiFact_LayoutDescription_CreateNewObject); ++n;
    RegisterRuntimeExport("?CreateInPlaceObject@LayoutDescription@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&RtUiFact_LayoutDescription_CreateInPlaceObject);
    RegisterRuntimeAddr(0x180042f20ull, (void*)&RtUiFact_LayoutDescription_CreateInPlaceObject); ++n;
    RegisterRuntimeExport("??0LayoutDescription@UI@@QEAA@XZ", (void*)&Ctor_LayoutDescription);
    RegisterRuntimeAddr(0x1800d3a90ull, (void*)&Ctor_LayoutDescription); ++n;

    RegisterRuntimeExport("?CreateNewObject@LayoutElement@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&RtUiFact_LayoutElement_CreateNewObject);
    RegisterRuntimeAddr(0x180043380ull, (void*)&RtUiFact_LayoutElement_CreateNewObject); ++n;
    RegisterRuntimeExport("?CreateInPlaceObject@LayoutElement@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&RtUiFact_LayoutElement_CreateInPlaceObject);
    RegisterRuntimeAddr(0x180043420ull, (void*)&RtUiFact_LayoutElement_CreateInPlaceObject); ++n;
    RegisterRuntimeExport("??0LayoutElement@UI@@QEAA@XZ", (void*)&Ctor_LayoutElement);
    RegisterRuntimeAddr(0x1800d32e0ull, (void*)&Ctor_LayoutElement); ++n;

    RegisterRuntimeExport("?CreateNewObject@ListDescription@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&RtUiFact_ListDescription_CreateNewObject);
    RegisterRuntimeAddr(0x180044c90ull, (void*)&RtUiFact_ListDescription_CreateNewObject); ++n;
    RegisterRuntimeExport("?CreateInPlaceObject@ListDescription@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&RtUiFact_ListDescription_CreateInPlaceObject);
    RegisterRuntimeAddr(0x180044d10ull, (void*)&RtUiFact_ListDescription_CreateInPlaceObject); ++n;
    RegisterRuntimeExport("??0ListDescription@UI@@QEAA@XZ", (void*)&Ctor_ListDescription);
    RegisterRuntimeAddr(0x1800d49b0ull, (void*)&Ctor_ListDescription); ++n;

    RegisterRuntimeExport("?CreateNewObject@ListElement@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&RtUiFact_ListElement_CreateNewObject);
    RegisterRuntimeAddr(0x180045080ull, (void*)&RtUiFact_ListElement_CreateNewObject); ++n;
    RegisterRuntimeExport("?CreateInPlaceObject@ListElement@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&RtUiFact_ListElement_CreateInPlaceObject);
    RegisterRuntimeAddr(0x180045100ull, (void*)&RtUiFact_ListElement_CreateInPlaceObject); ++n;
    RegisterRuntimeExport("??0ListElement@UI@@QEAA@XZ", (void*)&Ctor_ListElement);
    RegisterRuntimeAddr(0x1800d4570ull, (void*)&Ctor_ListElement); ++n;

    RegisterRuntimeExport("?CreateNewObject@NumberDescription@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&RtUiFact_NumberDescription_CreateNewObject);
    RegisterRuntimeAddr(0x18004a330ull, (void*)&RtUiFact_NumberDescription_CreateNewObject); ++n;
    RegisterRuntimeExport("?CreateInPlaceObject@NumberDescription@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&RtUiFact_NumberDescription_CreateInPlaceObject);
    RegisterRuntimeAddr(0x18004a420ull, (void*)&RtUiFact_NumberDescription_CreateInPlaceObject); ++n;
    RegisterRuntimeExport("??0NumberDescription@UI@@QEAA@XZ", (void*)&Ctor_NumberDescription);
    RegisterRuntimeAddr(0x180118b60ull, (void*)&Ctor_NumberDescription); ++n;

    RegisterRuntimeExport("?CreateNewObject@NumberEditDescription@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&RtUiFact_NumberEditDescription_CreateNewObject);
    RegisterRuntimeAddr(0x180045f80ull, (void*)&RtUiFact_NumberEditDescription_CreateNewObject); ++n;
    RegisterRuntimeExport("?CreateInPlaceObject@NumberEditDescription@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&RtUiFact_NumberEditDescription_CreateInPlaceObject);
    RegisterRuntimeAddr(0x180046000ull, (void*)&RtUiFact_NumberEditDescription_CreateInPlaceObject); ++n;
    RegisterRuntimeExport("??0NumberEditDescription@UI@@QEAA@XZ", (void*)&Ctor_NumberEditDescription);
    RegisterRuntimeAddr(0x1800d5050ull, (void*)&Ctor_NumberEditDescription); ++n;

    RegisterRuntimeExport("?CreateNewObject@NumberEditElement@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&RtUiFact_NumberEditElement_CreateNewObject);
    RegisterRuntimeAddr(0x180046340ull, (void*)&RtUiFact_NumberEditElement_CreateNewObject); ++n;
    RegisterRuntimeExport("?CreateInPlaceObject@NumberEditElement@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&RtUiFact_NumberEditElement_CreateInPlaceObject);
    RegisterRuntimeAddr(0x1800463e0ull, (void*)&RtUiFact_NumberEditElement_CreateInPlaceObject); ++n;
    RegisterRuntimeExport("??0NumberEditElement@UI@@QEAA@XZ", (void*)&Ctor_NumberEditElement);
    RegisterRuntimeAddr(0x1800d4c70ull, (void*)&Ctor_NumberEditElement); ++n;

    RegisterRuntimeExport("?CreateNewObject@NumberElement@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&RtUiFact_NumberElement_CreateNewObject);
    RegisterRuntimeAddr(0x18004a910ull, (void*)&RtUiFact_NumberElement_CreateNewObject); ++n;
    RegisterRuntimeExport("?CreateInPlaceObject@NumberElement@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&RtUiFact_NumberElement_CreateInPlaceObject);
    RegisterRuntimeAddr(0x18004a990ull, (void*)&RtUiFact_NumberElement_CreateInPlaceObject); ++n;

    RegisterRuntimeExport("?CreateNewObject@PageDescription@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&RtUiFact_PageDescription_CreateNewObject);
    RegisterRuntimeAddr(0x180046fd0ull, (void*)&RtUiFact_PageDescription_CreateNewObject); ++n;
    RegisterRuntimeExport("?CreateInPlaceObject@PageDescription@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&RtUiFact_PageDescription_CreateInPlaceObject);
    RegisterRuntimeAddr(0x180047050ull, (void*)&RtUiFact_PageDescription_CreateInPlaceObject); ++n;
    RegisterRuntimeExport("??0PageDescription@UI@@QEAA@XZ", (void*)&Ctor_PageDescription);
    RegisterRuntimeAddr(0x1800d54e0ull, (void*)&Ctor_PageDescription); ++n;

    RegisterRuntimeExport("?CreateNewObject@PageElement@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&RtUiFact_PageElement_CreateNewObject);
    RegisterRuntimeAddr(0x180047360ull, (void*)&RtUiFact_PageElement_CreateNewObject); ++n;
    RegisterRuntimeExport("?CreateInPlaceObject@PageElement@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&RtUiFact_PageElement_CreateInPlaceObject);
    RegisterRuntimeAddr(0x180047400ull, (void*)&RtUiFact_PageElement_CreateInPlaceObject); ++n;
    RegisterRuntimeExport("??0PageElement@UI@@QEAA@XZ", (void*)&Ctor_PageElement);
    RegisterRuntimeAddr(0x1800d5260ull, (void*)&Ctor_PageElement); ++n;

    RegisterRuntimeExport("?CreateNewObject@ProgressDescription@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&RtUiFact_ProgressDescription_CreateNewObject);
    RegisterRuntimeAddr(0x180077860ull, (void*)&RtUiFact_ProgressDescription_CreateNewObject); ++n;
    RegisterRuntimeExport("?CreateInPlaceObject@ProgressDescription@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&RtUiFact_ProgressDescription_CreateInPlaceObject);
    RegisterRuntimeAddr(0x1800778e0ull, (void*)&RtUiFact_ProgressDescription_CreateInPlaceObject); ++n;
    RegisterRuntimeExport("??0ProgressDescription@UI@@QEAA@XZ", (void*)&Ctor_ProgressDescription);
    RegisterRuntimeAddr(0x18011a210ull, (void*)&Ctor_ProgressDescription); ++n;

    RegisterRuntimeExport("?CreateNewObject@ProgressElement@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&RtUiFact_ProgressElement_CreateNewObject);
    RegisterRuntimeAddr(0x180077e30ull, (void*)&RtUiFact_ProgressElement_CreateNewObject); ++n;
    RegisterRuntimeExport("?CreateInPlaceObject@ProgressElement@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&RtUiFact_ProgressElement_CreateInPlaceObject);
    RegisterRuntimeAddr(0x180077eb0ull, (void*)&RtUiFact_ProgressElement_CreateInPlaceObject); ++n;
    RegisterRuntimeExport("??0ProgressElement@UI@@QEAA@XZ", (void*)&Ctor_ProgressElement);
    RegisterRuntimeAddr(0x180119c40ull, (void*)&Ctor_ProgressElement); ++n;

    RegisterRuntimeExport("?CreateNewObject@RadioDescription@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&RtUiFact_RadioDescription_CreateNewObject);
    RegisterRuntimeAddr(0x180047760ull, (void*)&RtUiFact_RadioDescription_CreateNewObject); ++n;
    RegisterRuntimeExport("?CreateInPlaceObject@RadioDescription@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&RtUiFact_RadioDescription_CreateInPlaceObject);
    RegisterRuntimeAddr(0x1800477e0ull, (void*)&RtUiFact_RadioDescription_CreateInPlaceObject); ++n;
    RegisterRuntimeExport("??0RadioDescription@UI@@QEAA@XZ", (void*)&Ctor_RadioDescription);
    RegisterRuntimeAddr(0x1800d5b50ull, (void*)&Ctor_RadioDescription); ++n;

    RegisterRuntimeExport("?CreateNewObject@RadioElement@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&RtUiFact_RadioElement_CreateNewObject);
    RegisterRuntimeAddr(0x180047af0ull, (void*)&RtUiFact_RadioElement_CreateNewObject); ++n;
    RegisterRuntimeExport("?CreateInPlaceObject@RadioElement@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&RtUiFact_RadioElement_CreateInPlaceObject);
    RegisterRuntimeAddr(0x180047ba0ull, (void*)&RtUiFact_RadioElement_CreateInPlaceObject); ++n;
    RegisterRuntimeExport("??0RadioElement@UI@@QEAA@XZ", (void*)&Ctor_RadioElement);
    RegisterRuntimeAddr(0x1800d5870ull, (void*)&Ctor_RadioElement); ++n;

    RegisterRuntimeExport("?CreateNewObject@RibbonDescription@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&RtUiFact_RibbonDescription_CreateNewObject);
    RegisterRuntimeAddr(0x180044220ull, (void*)&RtUiFact_RibbonDescription_CreateNewObject); ++n;
    RegisterRuntimeExport("?CreateInPlaceObject@RibbonDescription@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&RtUiFact_RibbonDescription_CreateInPlaceObject);
    RegisterRuntimeAddr(0x180044310ull, (void*)&RtUiFact_RibbonDescription_CreateInPlaceObject); ++n;
    RegisterRuntimeExport("??0RibbonDescription@UI@@QEAA@XZ", (void*)&Ctor_RibbonDescription);
    RegisterRuntimeAddr(0x1800d5bf0ull, (void*)&Ctor_RibbonDescription); ++n;

    RegisterRuntimeExport("?CreateNewObject@RibbonElement@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&RtUiFact_RibbonElement_CreateNewObject);
    RegisterRuntimeAddr(0x1800447d0ull, (void*)&RtUiFact_RibbonElement_CreateNewObject); ++n;
    RegisterRuntimeExport("?CreateInPlaceObject@RibbonElement@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&RtUiFact_RibbonElement_CreateInPlaceObject);
    RegisterRuntimeAddr(0x180044880ull, (void*)&RtUiFact_RibbonElement_CreateInPlaceObject); ++n;
    RegisterRuntimeExport("??0RibbonElement@UI@@QEAA@XZ", (void*)&Ctor_RibbonElement);
    RegisterRuntimeAddr(0x1800d5d10ull, (void*)&Ctor_RibbonElement); ++n;

    RegisterRuntimeExport("?CreateNewObject@ScrollDescription@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&RtUiFact_ScrollDescription_CreateNewObject);
    RegisterRuntimeAddr(0x18003a210ull, (void*)&RtUiFact_ScrollDescription_CreateNewObject); ++n;
    RegisterRuntimeExport("?CreateInPlaceObject@ScrollDescription@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&RtUiFact_ScrollDescription_CreateInPlaceObject);
    RegisterRuntimeAddr(0x18003a290ull, (void*)&RtUiFact_ScrollDescription_CreateInPlaceObject); ++n;
    RegisterRuntimeExport("??0ScrollDescription@UI@@QEAA@XZ", (void*)&Ctor_ScrollDescription);
    RegisterRuntimeAddr(0x18011a4e0ull, (void*)&Ctor_ScrollDescription); ++n;

    RegisterRuntimeExport("?CreateNewObject@ScrollElement@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&RtUiFact_ScrollElement_CreateNewObject);
    RegisterRuntimeAddr(0x18003a940ull, (void*)&RtUiFact_ScrollElement_CreateNewObject); ++n;
    RegisterRuntimeExport("?CreateInPlaceObject@ScrollElement@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&RtUiFact_ScrollElement_CreateInPlaceObject);
    RegisterRuntimeAddr(0x18003a9c0ull, (void*)&RtUiFact_ScrollElement_CreateInPlaceObject); ++n;
    RegisterRuntimeExport("??0ScrollElement@UI@@QEAA@XZ", (void*)&Ctor_ScrollElement);
    RegisterRuntimeAddr(0x18011a7f0ull, (void*)&Ctor_ScrollElement); ++n;

    RegisterRuntimeExport("?CreateNewObject@SliderDescription@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&RtUiFact_SliderDescription_CreateNewObject);
    RegisterRuntimeAddr(0x180049840ull, (void*)&RtUiFact_SliderDescription_CreateNewObject); ++n;
    RegisterRuntimeExport("?CreateInPlaceObject@SliderDescription@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&RtUiFact_SliderDescription_CreateInPlaceObject);
    RegisterRuntimeAddr(0x1800498c0ull, (void*)&RtUiFact_SliderDescription_CreateInPlaceObject); ++n;
    RegisterRuntimeExport("??0SliderDescription@UI@@QEAA@XZ", (void*)&Ctor_SliderDescription);
    RegisterRuntimeAddr(0x1800d7b60ull, (void*)&Ctor_SliderDescription); ++n;

    RegisterRuntimeExport("?CreateNewObject@SliderElement@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&RtUiFact_SliderElement_CreateNewObject);
    RegisterRuntimeAddr(0x180049e10ull, (void*)&RtUiFact_SliderElement_CreateNewObject); ++n;
    RegisterRuntimeExport("?CreateInPlaceObject@SliderElement@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&RtUiFact_SliderElement_CreateInPlaceObject);
    RegisterRuntimeAddr(0x180049e90ull, (void*)&RtUiFact_SliderElement_CreateInPlaceObject); ++n;
    RegisterRuntimeExport("??0SliderElement@UI@@QEAA@XZ", (void*)&Ctor_SliderElement);
    RegisterRuntimeAddr(0x1800d7570ull, (void*)&Ctor_SliderElement); ++n;

    RegisterRuntimeExport("?CreateNewObject@SpinnerDescription@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&RtUiFact_SpinnerDescription_CreateNewObject);
    RegisterRuntimeAddr(0x18004ae50ull, (void*)&RtUiFact_SpinnerDescription_CreateNewObject); ++n;
    RegisterRuntimeExport("?CreateInPlaceObject@SpinnerDescription@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&RtUiFact_SpinnerDescription_CreateInPlaceObject);
    RegisterRuntimeAddr(0x18004aed0ull, (void*)&RtUiFact_SpinnerDescription_CreateInPlaceObject); ++n;
    RegisterRuntimeExport("??0SpinnerDescription@UI@@QEAA@XZ", (void*)&Ctor_SpinnerDescription);
    RegisterRuntimeAddr(0x1800d89c0ull, (void*)&Ctor_SpinnerDescription); ++n;

    RegisterRuntimeExport("?CreateNewObject@SpinnerElement@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&RtUiFact_SpinnerElement_CreateNewObject);
    RegisterRuntimeAddr(0x18004b200ull, (void*)&RtUiFact_SpinnerElement_CreateNewObject); ++n;
    RegisterRuntimeExport("?CreateInPlaceObject@SpinnerElement@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&RtUiFact_SpinnerElement_CreateInPlaceObject);
    RegisterRuntimeAddr(0x18004b290ull, (void*)&RtUiFact_SpinnerElement_CreateInPlaceObject); ++n;
    RegisterRuntimeExport("??0SpinnerElement@UI@@QEAA@XZ", (void*)&Ctor_SpinnerElement);
    RegisterRuntimeAddr(0x1800d8670ull, (void*)&Ctor_SpinnerElement); ++n;

    RegisterRuntimeExport("?CreateNewObject@SpriteCapture@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&RtUiFact_SpriteCapture_CreateNewObject);
    RegisterRuntimeAddr(0x18003c900ull, (void*)&RtUiFact_SpriteCapture_CreateNewObject); ++n;
    RegisterRuntimeExport("?CreateInPlaceObject@SpriteCapture@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&RtUiFact_SpriteCapture_CreateInPlaceObject);
    RegisterRuntimeAddr(0x18003c9c0ull, (void*)&RtUiFact_SpriteCapture_CreateInPlaceObject); ++n;
    RegisterRuntimeExport("??0SpriteCapture@UI@@QEAA@XZ", (void*)&Ctor_SpriteCapture);
    RegisterRuntimeAddr(0x1800d9140ull, (void*)&Ctor_SpriteCapture); ++n;

    RegisterRuntimeExport("?CreateNewObject@TabDescription@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&RtUiFact_TabDescription_CreateNewObject);
    RegisterRuntimeAddr(0x18004c2e0ull, (void*)&RtUiFact_TabDescription_CreateNewObject); ++n;
    RegisterRuntimeExport("?CreateInPlaceObject@TabDescription@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&RtUiFact_TabDescription_CreateInPlaceObject);
    RegisterRuntimeAddr(0x18004c360ull, (void*)&RtUiFact_TabDescription_CreateInPlaceObject); ++n;
    RegisterRuntimeExport("??0TabDescription@UI@@QEAA@XZ", (void*)&Ctor_TabDescription);
    RegisterRuntimeAddr(0x1800daa00ull, (void*)&Ctor_TabDescription); ++n;

    RegisterRuntimeExport("?CreateNewObject@TabElement@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&RtUiFact_TabElement_CreateNewObject);
    RegisterRuntimeAddr(0x18004c7f0ull, (void*)&RtUiFact_TabElement_CreateNewObject); ++n;
    RegisterRuntimeExport("?CreateInPlaceObject@TabElement@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&RtUiFact_TabElement_CreateInPlaceObject);
    RegisterRuntimeAddr(0x18004c870ull, (void*)&RtUiFact_TabElement_CreateInPlaceObject); ++n;
    RegisterRuntimeExport("??0TabElement@UI@@QEAA@XZ", (void*)&Ctor_TabElement);
    RegisterRuntimeAddr(0x1800da390ull, (void*)&Ctor_TabElement); ++n;

    RegisterRuntimeExport("?CreateNewObject@TableDescription@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&RtUiFact_TableDescription_CreateNewObject);
    RegisterRuntimeAddr(0x180078fc0ull, (void*)&RtUiFact_TableDescription_CreateNewObject); ++n;
    RegisterRuntimeExport("?CreateInPlaceObject@TableDescription@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&RtUiFact_TableDescription_CreateInPlaceObject);
    RegisterRuntimeAddr(0x180079040ull, (void*)&RtUiFact_TableDescription_CreateInPlaceObject); ++n;
    RegisterRuntimeExport("??0TableDescription@UI@@QEAA@XZ", (void*)&Ctor_TableDescription);
    RegisterRuntimeAddr(0x18011e7a0ull, (void*)&Ctor_TableDescription); ++n;

    RegisterRuntimeExport("?CreateNewObject@TableElement@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&RtUiFact_TableElement_CreateNewObject);
    RegisterRuntimeAddr(0x1800793c0ull, (void*)&RtUiFact_TableElement_CreateNewObject); ++n;
    RegisterRuntimeExport("?CreateInPlaceObject@TableElement@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&RtUiFact_TableElement_CreateInPlaceObject);
    RegisterRuntimeAddr(0x180079440ull, (void*)&RtUiFact_TableElement_CreateInPlaceObject); ++n;
    RegisterRuntimeExport("??0TableElement@UI@@QEAA@XZ", (void*)&Ctor_TableElement);
    RegisterRuntimeAddr(0x18011e2f0ull, (void*)&Ctor_TableElement); ++n;

    RegisterRuntimeExport("?CreateNewObject@TextDescription@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&RtUiFact_TextDescription_CreateNewObject);
    RegisterRuntimeAddr(0x180041bc0ull, (void*)&RtUiFact_TextDescription_CreateNewObject); ++n;
    RegisterRuntimeExport("?CreateInPlaceObject@TextDescription@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&RtUiFact_TextDescription_CreateInPlaceObject);
    RegisterRuntimeAddr(0x180041c40ull, (void*)&RtUiFact_TextDescription_CreateInPlaceObject); ++n;
    RegisterRuntimeExport("??0TextDescription@UI@@QEAA@XZ", (void*)&Ctor_TextDescription);
    RegisterRuntimeAddr(0x18011ea20ull, (void*)&Ctor_TextDescription); ++n;

    RegisterRuntimeExport("?CreateNewObject@TextElement@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&RtUiFact_TextElement_CreateNewObject);
    RegisterRuntimeAddr(0x180041fe0ull, (void*)&RtUiFact_TextElement_CreateNewObject); ++n;
    RegisterRuntimeExport("?CreateInPlaceObject@TextElement@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&RtUiFact_TextElement_CreateInPlaceObject);
    RegisterRuntimeAddr(0x180042060ull, (void*)&RtUiFact_TextElement_CreateInPlaceObject); ++n;

    RegisterRuntimeExport("?CreateNewObject@ToolTipDialog@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&RtUiFact_ToolTipDialog_CreateNewObject);
    RegisterRuntimeAddr(0x18004d0b0ull, (void*)&RtUiFact_ToolTipDialog_CreateNewObject); ++n;
    RegisterRuntimeExport("?CreateInPlaceObject@ToolTipDialog@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&RtUiFact_ToolTipDialog_CreateInPlaceObject);
    RegisterRuntimeAddr(0x18004d140ull, (void*)&RtUiFact_ToolTipDialog_CreateInPlaceObject); ++n;
    RegisterRuntimeExport("??0ToolTipDialog@UI@@QEAA@XZ", (void*)&Ctor_ToolTipDialog);
    RegisterRuntimeAddr(0x1800dad00ull, (void*)&Ctor_ToolTipDialog); ++n;

    return n;
}
