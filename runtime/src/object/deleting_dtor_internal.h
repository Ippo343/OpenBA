#pragma once
// ============================================================================
// runtime/src/object/deleting_dtor_internal.h — Wave 9 lane K
// ============================================================================
//
// ENDGAME B.3.9 OBJ: the RT_DELDTOR table this header described (the row
// layout, `DeletingDtorCore`, `Row<>`, the `rtdtor` namespace) is GONE with
// its last row -- every class declares its own `DeletingDtor` (B39_RULES D1.2,
// D4). What stays is the one declaration three other translation units include
// it for (class_operator_delete.cpp, ui_operator_delete.cpp,
// vtbatch7_animgroup_particlesystem.cpp); the same two functions are also
// declared in runtime/types/collection/Collection.h.
// ============================================================================

#include <cstdint>

namespace rtcoll {
// Lane Q's by-address call to ?MemFree@MemoryAllocator@System@@SAXPEAX@Z
// (0x1800c7ad0), declared rather than re-derived so lane E has ONE place to
// repoint when the allocator moves. Defined in
// runtime/src/collection/array.cpp; declared identically in
// runtime/types/collection/Collection.h, and the linker checks the two agree.
void* MemAlloc(uint64_t bytes, int align);
void  MemFree(void* p);
}  // namespace rtcoll
