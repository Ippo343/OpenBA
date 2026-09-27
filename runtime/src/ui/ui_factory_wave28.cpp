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
#include "../../types/ui/SpriteCapture.h"   // ENDGAME B.3.8 RB: the shared ctor/dtor body

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
// address: that is what lets a port of the
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
extern "C" void* RtUi_LayoutElement_Ctor(void* self);   // runtime/src/ui/layoutelement.cpp
// The inlined stores that used to live here were a second copy of
// ??0LayoutElement@UI@@QEAA@XZ; the class file owns the body since vtable
// batch 4 (2026-09-12) and this factory pair forwards to it.
void* Ctor_LayoutElement(void* p) { return RtUi_LayoutElement_Ctor(p); }

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
// ---- VTABLE BATCH 2 (2026-09-12): the eleven constructors below are now
// ---- ONE-LINE FORWARDERS. Each class file under runtime/src/ui/ owns its
// ---- own ??0 body (the class-file precedent batch 1 and batch 4 set), and
// ---- the RegisterRuntimeExport/Addr pair that used to be in this file for
// ---- each of them was deleted at the merge: one registration per address.
// ---- The two this file still constructs itself, Layout (0x1800d3a90) and
// ---- Number (0x180118b60), are NOT in that list -- their class files have no
// ---- out-of-line ctor to register, because retail inlines theirs into
// ---- Construct and Reconstruct.
extern "C" void* RtUi_ListDescription_Ctor(void* self);
void* Ctor_ListDescription(void* p) { return RtUi_ListDescription_Ctor(p); }

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
extern "C" void* RtUi_ListElement_Ctor(void* self);   // runtime/src/ui/listelement.cpp
// The inlined stores that used to live here were a second copy of
// ??0ListElement@UI@@QEAA@XZ; the class file owns the body since vtable
// batch 4 (2026-09-12) and this factory pair forwards to it.
void* Ctor_ListElement(void* p) { return RtUi_ListElement_Ctor(p); }

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
// UI::ProgressDescription   allocation 0xb0, align 8
//   ?CreateNewObject@ProgressDescription@UI@@SAPEAVBase@Object@@XZ 0x180077860 ord 2097
//   ?CreateInPlaceObject@ProgressDescription@UI@@SAPEAVBase@Object@@PEAX@Z 0x1800778e0 ord 1960
//   ??0ProgressDescription@UI@@QEAA@XZ                             0x18011a210 ord 320
// SHAPE C: the factory pair is a bare `alloc / null test / call the
// exported ctor`. The whole body lives in ??0ProgressDescription@UI@@QEAA@XZ.
// That ctor IS ported below, so this row does not merely move the wall.
extern "C" void* RtUi_ProgressDescription_Ctor(void* self);
void* Ctor_ProgressDescription(void* p) { return RtUi_ProgressDescription_Ctor(p); }

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
extern "C" void* RtUi_ProgressElement_Ctor(void* self);   // runtime/src/ui/progresselement.cpp
// The inlined stores that used to live here were a second copy of
// ??0ProgressElement@UI@@QEAA@XZ; the class file owns the body since vtable
// batch 4 (2026-09-12) and this factory pair forwards to it.
void* Ctor_ProgressElement(void* p) { return RtUi_ProgressElement_Ctor(p); }

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
extern "C" void* RtUi_RibbonElement_Ctor(void* self);   // runtime/src/ui/ribbonelement.cpp
// The inlined stores that used to live here were a second copy of
// ??0RibbonElement@UI@@QEAA@XZ; the class file owns the body since vtable
// batch 4 (2026-09-12) and this factory pair forwards to it.
void* Ctor_RibbonElement(void* p) { return RtUi_RibbonElement_Ctor(p); }

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
extern "C" void* RtUi_ScrollDescription_Ctor(void* self);
void* Ctor_ScrollDescription(void* p) { return RtUi_ScrollDescription_Ctor(p); }

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
extern "C" void* RtUi_ScrollElement_Ctor(void* self);   // runtime/src/ui/scrollelement.cpp
// The inlined stores that used to live here were a second copy of
// ??0ScrollElement@UI@@QEAA@XZ; the class file owns the body since vtable
// batch 4 (2026-09-12) and this factory pair forwards to it.
void* Ctor_ScrollElement(void* p) { return RtUi_ScrollElement_Ctor(p); }

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
extern "C" void* RtUi_SliderDescription_Ctor(void* self);
void* Ctor_SliderDescription(void* p) { return RtUi_SliderDescription_Ctor(p); }

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
extern "C" void* RtUi_SliderElement_Ctor(void* self);   // runtime/src/ui/sliderelement.cpp
// The inlined stores that used to live here were a second copy of
// ??0SliderElement@UI@@QEAA@XZ; the class file owns the body since vtable
// batch 4 (2026-09-12) and this factory pair forwards to it.
void* Ctor_SliderElement(void* p) { return RtUi_SliderElement_Ctor(p); }

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
extern "C" void* RtUi_SpinnerDescription_Ctor(void* self);
void* Ctor_SpinnerDescription(void* p) { return RtUi_SpinnerDescription_Ctor(p); }

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
extern "C" void* RtUi_SpinnerElement_Ctor(void* self);   // runtime/src/ui/spinnerelement.cpp
// The inlined stores that used to live here were a second copy of
// ??0SpinnerElement@UI@@QEAA@XZ; the class file owns the body since vtable
// batch 4 (2026-09-12) and this factory pair forwards to it.
void* Ctor_SpinnerElement(void* p) { return RtUi_SpinnerElement_Ctor(p); }

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
    // ENDGAME B.3.8 block RB (RULE B8-1): +0x20..+0x57 are ONE body with the
    // real constructor `UI::SpriteCapture()` (SpriteCapture.h, InitFields).
    UI::SpriteCapture::DoorBody::Init(reinterpret_cast<UI::SpriteCapture*>(p));   // B.3.8 bridge
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
extern "C" void* RtUi_TabDescription_Ctor(void* self);
void* Ctor_TabDescription(void* p) { return RtUi_TabDescription_Ctor(p); }

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
extern "C" void* RtUi_TabElement_Ctor(void* self);   // runtime/src/ui/tabelement.cpp
// The inlined stores that used to live here were a second copy of
// ??0TabElement@UI@@QEAA@XZ; the class file owns the body since vtable
// batch 4 (2026-09-12) and this factory pair forwards to it.
void* Ctor_TabElement(void* p) { return RtUi_TabElement_Ctor(p); }

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
extern "C" void* RtUi_TableDescription_Ctor(void* self);
void* Ctor_TableDescription(void* p) { return RtUi_TableDescription_Ctor(p); }

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
extern "C" void* RtUi_TableElement_Ctor(void* self);   // runtime/src/ui/tableelement.cpp
// The inlined stores that used to live here were a second copy of
// ??0TableElement@UI@@QEAA@XZ; the class file owns the body since vtable
// batch 4 (2026-09-12) and this factory pair forwards to it.
void* Ctor_TableElement(void* p) { return RtUi_TableElement_Ctor(p); }

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
extern "C" void* RtUi_TextDescription_Ctor(void* self);
void* Ctor_TextDescription(void* p) { return RtUi_TextDescription_Ctor(p); }

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
// *** THIS "NOT PORTED" NOTE WAS WRONG WHEN WRITTEN AND IS CORRECTED
// *** 2026-09-12. ***
// It used to read: "The ctor is NOT ported: it calls
// ?CacheText@TextElement@UI@@AEAAXXZ, a private member this lane does not own
// and did not read. The factory pair below therefore MOVES the cutover wall
// from the factory to the ctor rather than clearing it."
// Both halves fail. `?CacheText@TextElement@UI@@AEAAXXZ` (0x18011ec10) has
// been ported and registered since Wave 10, in
// runtime/src/ui/textelement_cachetext.cpp, alongside
// `?SetParameterCount@TextElement@UI@@AEAAXXZ` (0x18011e8e0) — so the named
// blocker was already gone. And `AEAA` means PRIVATE, not unexported; this DLL
// exports its private members (reference_aeaa_is_private_not_unexported.md),
// so "does not own" was never a reason either.
// `??0TextElement@UI@@QEAA@XZ` IS NOW PORTED, in
// runtime/src/ui/element_bodies_b5.cpp, registered under this exact name and
// at 0x18011eb80, so the `BASE(...)` below reaches OUR body. This row clears
// the wall; it does not move it.
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
    int n = 0;

    RegisterRuntimeExport("?CreateNewObject@SpriteSheet@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&RtUiFact_SpriteSheet_CreateNewObject);
    RegisterRuntimeAddr(0x180002f40ull, (void*)&RtUiFact_SpriteSheet_CreateNewObject); ++n;
    RegisterRuntimeExport("?CreateInPlaceObject@SpriteSheet@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&RtUiFact_SpriteSheet_CreateInPlaceObject);
    RegisterRuntimeAddr(0x180003000ull, (void*)&RtUiFact_SpriteSheet_CreateInPlaceObject); ++n;

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

    RegisterRuntimeExport("?CreateNewObject@ListDescription@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&RtUiFact_ListDescription_CreateNewObject);
    RegisterRuntimeAddr(0x180044c90ull, (void*)&RtUiFact_ListDescription_CreateNewObject); ++n;
    RegisterRuntimeExport("?CreateInPlaceObject@ListDescription@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&RtUiFact_ListDescription_CreateInPlaceObject);
    RegisterRuntimeAddr(0x180044d10ull, (void*)&RtUiFact_ListDescription_CreateInPlaceObject); ++n;

    RegisterRuntimeExport("?CreateNewObject@ListElement@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&RtUiFact_ListElement_CreateNewObject);
    RegisterRuntimeAddr(0x180045080ull, (void*)&RtUiFact_ListElement_CreateNewObject); ++n;
    RegisterRuntimeExport("?CreateInPlaceObject@ListElement@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&RtUiFact_ListElement_CreateInPlaceObject);
    RegisterRuntimeAddr(0x180045100ull, (void*)&RtUiFact_ListElement_CreateInPlaceObject); ++n;

    RegisterRuntimeExport("?CreateNewObject@ProgressDescription@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&RtUiFact_ProgressDescription_CreateNewObject);
    RegisterRuntimeAddr(0x180077860ull, (void*)&RtUiFact_ProgressDescription_CreateNewObject); ++n;
    RegisterRuntimeExport("?CreateInPlaceObject@ProgressDescription@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&RtUiFact_ProgressDescription_CreateInPlaceObject);
    RegisterRuntimeAddr(0x1800778e0ull, (void*)&RtUiFact_ProgressDescription_CreateInPlaceObject); ++n;

    RegisterRuntimeExport("?CreateNewObject@ProgressElement@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&RtUiFact_ProgressElement_CreateNewObject);
    RegisterRuntimeAddr(0x180077e30ull, (void*)&RtUiFact_ProgressElement_CreateNewObject); ++n;
    RegisterRuntimeExport("?CreateInPlaceObject@ProgressElement@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&RtUiFact_ProgressElement_CreateInPlaceObject);
    RegisterRuntimeAddr(0x180077eb0ull, (void*)&RtUiFact_ProgressElement_CreateInPlaceObject); ++n;

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

    RegisterRuntimeExport("?CreateNewObject@ScrollDescription@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&RtUiFact_ScrollDescription_CreateNewObject);
    RegisterRuntimeAddr(0x18003a210ull, (void*)&RtUiFact_ScrollDescription_CreateNewObject); ++n;
    RegisterRuntimeExport("?CreateInPlaceObject@ScrollDescription@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&RtUiFact_ScrollDescription_CreateInPlaceObject);
    RegisterRuntimeAddr(0x18003a290ull, (void*)&RtUiFact_ScrollDescription_CreateInPlaceObject); ++n;

    RegisterRuntimeExport("?CreateNewObject@ScrollElement@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&RtUiFact_ScrollElement_CreateNewObject);
    RegisterRuntimeAddr(0x18003a940ull, (void*)&RtUiFact_ScrollElement_CreateNewObject); ++n;
    RegisterRuntimeExport("?CreateInPlaceObject@ScrollElement@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&RtUiFact_ScrollElement_CreateInPlaceObject);
    RegisterRuntimeAddr(0x18003a9c0ull, (void*)&RtUiFact_ScrollElement_CreateInPlaceObject); ++n;

    RegisterRuntimeExport("?CreateNewObject@SliderDescription@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&RtUiFact_SliderDescription_CreateNewObject);
    RegisterRuntimeAddr(0x180049840ull, (void*)&RtUiFact_SliderDescription_CreateNewObject); ++n;
    RegisterRuntimeExport("?CreateInPlaceObject@SliderDescription@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&RtUiFact_SliderDescription_CreateInPlaceObject);
    RegisterRuntimeAddr(0x1800498c0ull, (void*)&RtUiFact_SliderDescription_CreateInPlaceObject); ++n;

    RegisterRuntimeExport("?CreateNewObject@SliderElement@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&RtUiFact_SliderElement_CreateNewObject);
    RegisterRuntimeAddr(0x180049e10ull, (void*)&RtUiFact_SliderElement_CreateNewObject); ++n;
    RegisterRuntimeExport("?CreateInPlaceObject@SliderElement@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&RtUiFact_SliderElement_CreateInPlaceObject);
    RegisterRuntimeAddr(0x180049e90ull, (void*)&RtUiFact_SliderElement_CreateInPlaceObject); ++n;

    RegisterRuntimeExport("?CreateNewObject@SpinnerDescription@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&RtUiFact_SpinnerDescription_CreateNewObject);
    RegisterRuntimeAddr(0x18004ae50ull, (void*)&RtUiFact_SpinnerDescription_CreateNewObject); ++n;
    RegisterRuntimeExport("?CreateInPlaceObject@SpinnerDescription@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&RtUiFact_SpinnerDescription_CreateInPlaceObject);
    RegisterRuntimeAddr(0x18004aed0ull, (void*)&RtUiFact_SpinnerDescription_CreateInPlaceObject); ++n;

    RegisterRuntimeExport("?CreateNewObject@SpinnerElement@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&RtUiFact_SpinnerElement_CreateNewObject);
    RegisterRuntimeAddr(0x18004b200ull, (void*)&RtUiFact_SpinnerElement_CreateNewObject); ++n;
    RegisterRuntimeExport("?CreateInPlaceObject@SpinnerElement@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&RtUiFact_SpinnerElement_CreateInPlaceObject);
    RegisterRuntimeAddr(0x18004b290ull, (void*)&RtUiFact_SpinnerElement_CreateInPlaceObject); ++n;

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

    RegisterRuntimeExport("?CreateNewObject@TabElement@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&RtUiFact_TabElement_CreateNewObject);
    RegisterRuntimeAddr(0x18004c7f0ull, (void*)&RtUiFact_TabElement_CreateNewObject); ++n;
    RegisterRuntimeExport("?CreateInPlaceObject@TabElement@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&RtUiFact_TabElement_CreateInPlaceObject);
    RegisterRuntimeAddr(0x18004c870ull, (void*)&RtUiFact_TabElement_CreateInPlaceObject); ++n;

    RegisterRuntimeExport("?CreateNewObject@TableDescription@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&RtUiFact_TableDescription_CreateNewObject);
    RegisterRuntimeAddr(0x180078fc0ull, (void*)&RtUiFact_TableDescription_CreateNewObject); ++n;
    RegisterRuntimeExport("?CreateInPlaceObject@TableDescription@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&RtUiFact_TableDescription_CreateInPlaceObject);
    RegisterRuntimeAddr(0x180079040ull, (void*)&RtUiFact_TableDescription_CreateInPlaceObject); ++n;

    RegisterRuntimeExport("?CreateNewObject@TableElement@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&RtUiFact_TableElement_CreateNewObject);
    RegisterRuntimeAddr(0x1800793c0ull, (void*)&RtUiFact_TableElement_CreateNewObject); ++n;
    RegisterRuntimeExport("?CreateInPlaceObject@TableElement@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&RtUiFact_TableElement_CreateInPlaceObject);
    RegisterRuntimeAddr(0x180079440ull, (void*)&RtUiFact_TableElement_CreateInPlaceObject); ++n;

    RegisterRuntimeExport("?CreateNewObject@TextDescription@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&RtUiFact_TextDescription_CreateNewObject);
    RegisterRuntimeAddr(0x180041bc0ull, (void*)&RtUiFact_TextDescription_CreateNewObject); ++n;
    RegisterRuntimeExport("?CreateInPlaceObject@TextDescription@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&RtUiFact_TextDescription_CreateInPlaceObject);
    RegisterRuntimeAddr(0x180041c40ull, (void*)&RtUiFact_TextDescription_CreateInPlaceObject); ++n;

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
