// ============================================================================
// runtime/src/object/rt_object_port.cpp — RtObjectPortInit()  (Wave 3)
// ============================================================================
// One entry point for the Object namespace: the reflection system (Base /
// Type / BaseTypeInfo / Factory), the internal handle table (Internal /
// InternalBasePtr / InternalFactory) and the external resource table
// (External / ExternalFactory / ExternalList).
//
// Same shape as RtSystemPortInit(): each lane owns exactly ONE source file and
// exposes exactly ONE Register function, so no two lanes ever edit the same
// file. This file and runtime/funcs/rt_port.h are the only shared surface and
// they are already written.
//
// Switches, outermost first:
//   BANISHED_RT_OBJECT=0    leave the whole Object namespace on the DLL
//   BANISHED_RT_TYPESYS=0   ... just Base / Type / BaseTypeInfo / Factory
//   BANISHED_RT_INTERNAL=0  ... just Internal / InternalBasePtr / InternalFactory
//   BANISHED_RT_EXTERNAL=0  ... just External / ExternalFactory / ExternalList
//
// STATE WARNING (PLAN.md §4.1 decision 5, §4.3 risk 3). Unlike Waves 1 and 2,
// these classes own state and are constructed by the DLL. Registration
// redirects OUR call sites only; the DLL's own internal calls still land in
// the DLL, and objects the DLL built carry DLL vtables. Every port in this
// wave must therefore operate on the LIVE DLL singleton's own tables — the
// allocator pattern from Wave 2 — and must never keep a private replica that
// the DLL can desynchronise. A class ported half-way with two vtables in play
// is the swap-remove UAF family again
// (reference_internalfactory_swapremove_wild_write.md).
// ============================================================================

#include <cstdio>
#include "../../funcs/rt_port.h"

extern int RtObjectTypeSystemRegister();   // type_system.cpp   (lane A)
extern int RtObjectInternalRegister();     // internal.cpp      (lane B)
extern int RtObjectExternalRegister();     // external.cpp      (lane C)
extern int RtObjectExternalSetRegister();  // external_set.cpp  (Wave 9 lane RES)

void RtObjectPortInit() {
    if (!RtWaveEnabled("BANISHED_RT_OBJECT")) {
        printf("[RtPort] BANISHED_RT_OBJECT=0 -- Object namespace port disabled\n");
        return;
    }
    int n = 0;
    if (RtWaveEnabled("BANISHED_RT_TYPESYS"))  n += RtObjectTypeSystemRegister();
    if (RtWaveEnabled("BANISHED_RT_INTERNAL")) n += RtObjectInternalRegister();
    if (RtWaveEnabled("BANISHED_RT_EXTERNAL")) n += RtObjectExternalRegister();
    // Wave 9 lane RES: the resource tree's own Set bodies, plus Clean. Nested
    // under BANISHED_RT_EXTERNAL because it is the same class's state, with its
    // own switch so the twelve unexported Set doors can be A/B'd on their own.
    if (RtWaveEnabled("BANISHED_RT_EXTERNAL") && RtWaveEnabled("BANISHED_RT_RESSET"))
        n += RtObjectExternalSetRegister();
    printf("[RtPort] Object: %d exports registered\n", n);
}
