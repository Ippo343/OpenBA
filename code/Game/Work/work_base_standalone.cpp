// code/Game/Work/work_base_standalone.cpp
// Shared standalone factory/vtable helpers for the Work family.
// See work_base_standalone.h and notes/FACTORY_CASCADE_145_BRIEF.md.

#include "work_base_standalone.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "Object/factory_registration.h"   // Object::GetTypeIdByName
#include "Object/exe_type.h"               // DEFINE_EXE_VTABLE
#include "Work/Work.h"                     // Work (ENDGAME B.3.8), WorkAddr::Vtable
#include "object/Internal.h"               // Object::detail::InternalMemFree
#include <cstdint>
#include <cstdio>

// Stamps the Work BASE vtable into `self`, the first thing retail's Work scalar dtor
// (0x14002ca60) does. ENDGAME B.3.8: the base table is the compiler's (Work::Vtable()),
// the table 0x140161a80 serves.
static void Work_StampBaseVtable(void* self) {
    if (self) *reinterpret_cast<const void**>(self) = Work::Vtable();
}

// slot1 (+0x08) — exe returns DAT_xxxx (per-type, statically null).
extern "C" void* Work_Slot1(void) { return nullptr; }

// slot5 (+0x28) — exe 0x1400f7540 is `xor eax,eax; ret` (returns 0).
extern "C" int Work_Slot5(void) { return 0; }

// slot12 (+0x60) — exe 0x140006b10 is a void nop.
extern "C" void Work_Slot12(void) {}

// Shared allocator. The per-type factory runs the type's own *_Ctor_Impl on this
// memory (the ctor calls Object::Internal::Internal, zeroes Work + type fields,
// and installs the vtable via ResolveAddr).
extern "C" void* Work_Alloc(int size) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    return fn_MemAlloc(g_pMemAllocSingleton, static_cast<uint64_t>(size), 8);
}

// Generic ctor for Work types lacking a dedicated *_Ctor_Impl. Mirrors the common
// Work-base init seen in BuildRoad_Ctor_Impl: Object::Internal::Internal, then the
// Work base fields (the two task-index sentinels at +0x20 / +0x28 default to -1),
// the rest zeroed, and the type vtable installed.
extern "C" void* Work_GenericCtor(void* mem, int size, void* vtable) {
    if (!mem) return nullptr;
    if (fn_ObjectInternal_Ctor) fn_ObjectInternal_Ctor(mem);
    uint8_t* s = reinterpret_cast<uint8_t*>(mem);
    for (int i = 0x10; i < size; ++i) s[i] = 0;
    // Work base defaults (from the exe Work ctors): +0x20 and +0x28 = -1.
    if (size > 0x24) *reinterpret_cast<int32_t*>(s + 0x20) = -1;
    if (size > 0x2c) *reinterpret_cast<int32_t*>(s + 0x28) = -1;
    *reinterpret_cast<void**>(mem) = vtable;
    return mem;
}

// Generic ScalarDtor for Work types lacking a dedicated *_ScalarDtor_Impl.
extern "C" void* Work_GenericScalarDtor(void* self, unsigned int flags) {
    if (!self) return self;
    // Retail 0x14002ca60, in full:
    //     *(void**)this = &vtable_Work; Object::Internal::~Internal(this);
    //     if (flags & 1) MemFree(this); return this;
    // The vtable re-stamp was missing here: every Work leaf that has no ported
    // *_ScalarDtor_Impl of its own reached ~Internal still carrying its LEAF vtable,
    // where retail has already walked the chain down to Work's. The ported leaf dtors
    // (ClearStorage/MarketWork/Pickup/...) all do the stamp, so this shim was the odd
    // one out. Restored below; it is the same end state either way, and it is what
    // makes a virtual dispatched from inside ~Internal land on the base.
    Work_StampBaseVtable(self);
    if (fn_ObjectInternal_Dtor) fn_ObjectInternal_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

// No-op Serialize stand-in (see header note).
extern "C" void Work_SerializeNop(void* /*self*/, void* /*stream*/) {}

// Fallback slot returning 0 (false / lowest priority).
extern "C" int Work_SlotRet0(void) { return 0; }

// Shared slot11 — exe 0x140029860. Bytes: 33 C0 39 41 50 0F 9F C0 C3, i.e.
//   EAX = 0 ; CMP dword[RCX+0x50], EAX ; SETG AL ; RET   ->  return (self->+0x50 > 0).
// See the header for which five types share it and which four only look like they do.
extern "C" int Work_Slot11_HasActiveTasks(void* self) {
    if (!self) return 0;
    return *reinterpret_cast<int32_t*>(reinterpret_cast<uintptr_t>(self) + 0x50) > 0 ? 1 : 0;
}

// Stand-in for retail's _purecall slots, kept for the leaves' hand-built tables
// until each leaf converts. The two BASE tables are the compiler's now; their pure
// slots are the toolchain's __cxa_pure_virtual (retail's _purecall aborts too).
extern "C" int Work_PureCall(void) {
    static bool reported = false;
    if (!reported) {
        reported = true;
        std::fprintf(stderr, "[WorkBase] _purecall slot dispatched on a Work leaf table "
                             "— this should be unreachable.\n");
        std::fflush(stderr);
    }
    return 0;
}

// ============================================================================
// Work — ENDGAME B.3.8 block WRK (Work.h). Retail table 0x140161a80.
// ============================================================================

// The default base init every retail leaf ctor inlines after `??0Internal`
// (e.g. BuildRoad_Ctor 0x1400e4ad0). The 0x140161a80 stamp is the compiler's.
Work::Work()
    : Object::Internal(),
      field_10(nullptr), field_18(nullptr),
      field_20(-1), field_24(0), field_28(-1), field_2c(0),
      field_30(nullptr), field_38(nullptr), field_40(0) {}

// 0x1400f72e0 (Work_Ctor25): `??0Internal`, the compiler's stamp, then the one
// body it shares with its by-address door (Work::InitFromEntity,
// work_base_reimpl.cpp).
Work::Work(void* pEntityIBP) : Object::Internal() {
    InitFromEntity(pEntityIBP);
}

// [0] 0x14002ca60: stamp 0x140161a80, `~Internal`, `if (flags & 1) MemFree`.
Object::Base* Work::DeletingDtor(unsigned flags) {
    this->~Work();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// [1]/[2] 0x14002ca50 / 0x14002ca40 over Work's BaseTypeInfo. The abstract
// registration stays in factory_registrations_bulk.cpp (B38 D4), so the base
// reads its id by name; only a half-built or half-destroyed object dispatches
// here. The old table answered null / a lazily cached id.
const char* Work::TypeName() const { return "Work"; }
uint32_t    Work::Type() const { return ::Object::GetTypeIdByName("Work"); }

// [5] 0x1400f7540 `XOR AL,AL; RET` (shared by 22 tables).
int  Work::Slot5(void*, void*) { return 0; }
// [12] 0x140006b10 `RET 0` (shared by 25 tables).
void Work::Slot12() {}

DEFINE_EXE_VTABLE(Work, "_ZTV4Work")

// B38 D5: the exe address serves the COMPILER's table while by-address readers
// remain — the unconverted leaves' dtors re-stamp it on the way down the chain
// (retail's order), Work_Ctor25_Impl stamps it, and
// code/Game/Component/clearareacomponent_states.cpp stamps it while building a
// ClearArea by hand.
extern "C" void RegisterWorkBaseVtable() {
    RegisterExeAddr(WorkAddr::Vtable, reinterpret_cast<uintptr_t>(Work::Vtable()));   // 0x140161a80
}

