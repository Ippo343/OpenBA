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
#include "../../types/object/External.h"   // Object::ExternalFactory::_singleton
#include "../../types/object/Internal.h"   // Object::InternalFactory::_singleton

extern int RtObjectTypeSystemRegister();          // type_system.cpp
extern int RtObjectInternalRegister();            // internal.cpp
extern int RtObjectExternalRegister();            // external.cpp

// B3R RT-Core: every row the other thirteen register functions carried had no
// reader by name or by address, so those functions went with their rows
// (class_operator_delete.cpp and internalfactory_persist.cpp with them). The
// three below still carry rows other blocks read.
void RtObjectPortInit() {
    // ENDGAME B.3.9 OBJ (B39_RULES D7): the minted storage rows ARE the class
    // statics from here on (name door, address door, manifest publish). Every
    // *PortInit runs before the mint; a call after it aborts.
    int n = 0;
    n += RtObjectTypeSystemRegister();
    n += RtObjectInternalRegister();
    n += RtObjectExternalRegister();

    printf("[RtPort] Object: %d exports registered\n", n);
}

// ============================================================================
