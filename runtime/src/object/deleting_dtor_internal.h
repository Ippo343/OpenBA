#pragma once
// ============================================================================
// runtime/src/object/deleting_dtor_internal.h — Wave 9 lane K
// ============================================================================
//
// The three things deleting_dtor.cpp and deleting_dtor_probe.cpp must agree on:
// the row layout, the body's signature, and which rows the probe may safely
// use as subjects. Only those two files include this.
//
// The full commentary — what the family is, why it is the VECTOR deleting
// destructor and not the scalar one the brief assumed, and what the two halves
// of the oracle each cover — is the banner of deleting_dtor.cpp.
// ============================================================================

#include <cstdint>


namespace rtcoll {
// Lane Q's by-address call to ?MemFree@MemoryAllocator@System@@SAXPEAX@Z
// (0x1800c7ad0), declared rather than re-derived so lane E has ONE place to
// repoint when the allocator moves. Defined in
// runtime/src/collection/array.cpp; declared identically in
// runtime/src/collection/array_internal.h, and the linker checks the two agree.
void* MemAlloc(uint64_t bytes, int align);
void  MemFree(void* p);
}  // namespace rtcoll

namespace rtdtor {

// `~Class`. Every one of the 37 destructors this family calls takes `this` and
// nothing else, verified per address in the disassembly — not assumed from the
// mangling, because lane Q's SetSize correction was precisely a family whose
// arity varied behind one typedef.
using DtorFn = void (*)(void* self);

// The registered entry point's shape: `??_E<Class>@@UEAAPEAXI@Z`.
// Bit 0 of `flags` = free the block, bit 1 = this was a `delete[]`.
using DeletingDtorFn = void* (*)(void* self, unsigned flags);

// One row per registered address. `dtorVa` and `elemSize` are the ONLY two
// values that vary across the 37; both are re-derived from the shipped DLL by
// tools/rt_deleting_dtor_verify.py.
struct DelDtorSpec {
    uintptr_t   addr;       // the deleting destructor's own Ghidra VA
    uintptr_t   dtorVa;     // ~Class, reached through ResolveAddr (port-aware)
    uint32_t    elemSize;   // sizeof(Class), the array branch's stride
    const char* name;
};

// THE TABLE IS NO LONGER POSITIONAL (ENDGAME B.3 step 9, B39_RULES.md D4).
// Until step 9 every wrapper indexed kSpecs[N] by a literal N, and three
// waves paid for it at merge time (a keep-both merge gave two lanes the first
// lane's rows and dispatched THE WRONG DESTRUCTOR with no diagnostic). Step 9
// DELETES rows from many blocks at once, so each wrapper now names its row by
// the deleting destructor's own address -- `Row<0xADDR>()` in deleting_dtor.cpp,
// a compile-time lookup that fails the build when the row is missing -- and
// kSpecCount is sizeof(kSpecs) there. Deleting a row is: the row, its wrapper,
// its RegisterRuntimeAddr line. Nothing is renumbered.

// The body and the table.
void* DeletingDtorCore(void* self, unsigned flags, int i);
void  EhVectorDtor(uint8_t* base, uint32_t elemSize, int count, DtorFn dtor);
const DelDtorSpec* Specs();

}  // namespace rtdtor

// Declared in runtime/funcs/rt_port.h too; this is the definition's home.
void RtObjectDeletingDtorPortInit();
