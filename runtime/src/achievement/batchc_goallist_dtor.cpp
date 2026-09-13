// ============================================================================
// runtime/src/achievement/batchc_goallist_dtor.cpp — BATCH C
//   ??1GoalList@Achievement@@QEAA@XZ             retail 0x18009f420, ordinal 602
// ============================================================================
//
// The last unported member of Achievement::GoalList. Its behaviour was already
// fully written down — runtime/types/achievement/Goal.h's GoalList banner names
// every step of it as one of the four functions that PIN the layout — and this
// file is that description turned into the body, checked line by line against
// the disassembly at 0x18009f420..0x18009f4cc.
//
//   for (i = 0; i < this->_goals._size; ++i) {     // bound RE-READ each pass
//       slot = &this->_goals._data[i];             // base RE-READ each pass
//       if (*slot) { (*slot)->vtable[0](*slot, 1); *slot = nullptr; }
//   }
//   if (this->_goals._data) MemFree(this->_goals._data);
//   this->_goals._data = nullptr;
//   *(uint64*)&this->_goals._size = 0;             // _size AND _capacity
//
// `vtable[0]` WITH FLAG 1 IS THE SCALAR DELETING DESTRUCTOR: the list OWNS its
// GoalInstances and frees each one. Goal.h's GoalInstanceVtbl::
// kScalarDeletingDtor records the same slot and the same flag, sourced from
// this very call.
//
// THE FREE IS INLINED IN RETAIL as lock / Free / unlock around the allocator's
// +0x640 critical section. goal_list.cpp:194 already records that this triple
// IS `?MemFree@MemoryAllocator@System@@SAXPEAX@Z` (0x1800c7ad0) instruction for
// instruction, and reuses `rtcoll::MemFree` for it; so does this file, for the
// same reason and through the same declaration.
//
// ORACLE: NONE, and it is stated rather than faked. Running the shipped body
// afterwards would call slot 0 with flag 1 on already-freed GoalInstances — a
// double free, not a comparison. the diff oracle records how many calls actually
// had goals to destroy; a GoalList destroyed empty is the common case and
// `calls=` alone would hide that.
// ============================================================================

#include <cstddef>
#include <cstdint>

#include "../../funcs/rt_port.h"
#include "../../types/achievement/Goal.h"
#include "../../types/object/Internal.h"

// runtime/src/collection/array.cpp, Wave 9 lane Q. DECLARED rather than
// #included — the same one-function link-time reuse goal_list.cpp documents.
namespace rtcoll { void MemFree(void* p); }

using RtAchievement::GoalList;
namespace GoalInstanceVtbl = RtAchievement::GoalInstanceVtbl;

extern "C" void RtAch_GoalList_Dtor(void* selfv) {
    GoalList* const self = static_cast<GoalList*>(selfv);

    const bool didWork = (self->_goals._size > 0);

    using DeletingDtorFn = void* (*)(void*, int);

    for (int32_t i = 0; i < self->_goals._size; ++i) {
        // `MOV R14,[RDI]` sits INSIDE the loop at 0x18009f450 — the base is
        // re-read every pass, so a destructor that reallocates the list is
        // seen. Not hoisted.
        void** const slot = reinterpret_cast<void**>(self->_goals._data) + i;
        void* const inst = *slot;
        if (inst == nullptr) continue;
        void** const vt = *reinterpret_cast<void***>(inst);
        reinterpret_cast<DeletingDtorFn>(vt[GoalInstanceVtbl::kScalarDeletingDtor / 8])(inst, 1);
        *slot = nullptr;
    }

    if (self->_goals._data != nullptr)
        rtcoll::MemFree(self->_goals._data);

    self->_goals._data = nullptr;
    // ONE qword store over _size and _capacity (0x18009f4b6), the
    // Collection::Array teardown shape Wave 2 pinned.
    *reinterpret_cast<uint64_t*>(&self->_goals._size) = 0;

}

// ============================================================================
// Registration — BOTH DOORS, unconditionally.
//
// 0x18009f420 was grepped across code/, engine/, runtime/, standalone/ and
// dx11/ first: it appears only in Goal.h's two comments, never as an identity
// token, so the address door is safe.
// ============================================================================
extern "C" int RtAchBatchCGoalListDtorRegister() {
    RegisterRuntimeExport("??1GoalList@Achievement@@QEAA@XZ", (void*)&RtAch_GoalList_Dtor);
    RegisterRuntimeAddr(0x18009f420ull, (void*)&RtAch_GoalList_Dtor);
    return 1;
}
