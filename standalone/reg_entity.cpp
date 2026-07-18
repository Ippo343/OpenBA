// standalone/reg_entity.cpp
// Entity-module registration wave — closes the Entity code-gap (6 addrs).
//
// All six EntityList methods are deeply reimplemented in
// code/Game/Entity/entitylist.cpp and ALREADY run live in standalone via
// EntityList_Update (which calls the reimpl bodies directly by name — the
// world ticks through them every frame). The ResolveAddr(Module::Exe, ...)
// references in entitylist.cpp (used to populate the s_fn_* fallback pointers,
// which are debug-only) resolved to 0 in standalone, leaving these addresses
// in the gap. Registering each entry → its reimpl body is purely additive:
// the bodies' closure is already satisfied (they run today), so this maps the
// entry address to the already-running implementation.
//
// SAFETY: each address verified as a true function ENTRY (Ghidra) and matches
// the reimpl body's signature. No mid-function / passthrough / typo issues.
#include "resolve.h"
#include "Entity/entitylist_funcs.h"   // exact signatures for the reimpl bodies
#include <cstdint>

void RegisterEntityBatch() {
    RegisterExeAddr(0x1400c2c70, reinterpret_cast<uintptr_t>(&EntityList_PostUpdateNodeHandler));
    RegisterExeAddr(0x1400c38e0, reinterpret_cast<uintptr_t>(&EntityList_RemoveEntity));
    RegisterExeAddr(0x1400c3ab0, reinterpret_cast<uintptr_t>(&EntityList_PostUpdateStaging));
    RegisterExeAddr(0x1400c3bd0, reinterpret_cast<uintptr_t>(&EntityList_PreUpdate));
    RegisterExeAddr(0x1400c43d0, reinterpret_cast<uintptr_t>(&EntityList_PostUpdate));
    RegisterExeAddr(0x1400c44a0, reinterpret_cast<uintptr_t>(&EntityList_Serialize));
}

