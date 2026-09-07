#pragma once
// ============================================================================
// runtime/types/achievement/Goal.h — Achievement::Goal / GoalInstance / GoalList
// ============================================================================
// Wave 5, lane A of the Runtime-x64.dll port (docs/runtime_port/PLAN.md §4.2,
// §6 step 3). Full write-up: docs/runtime_port/WAVE5_LANE_A.md.
//
// EVIDENCE RULE (PLAN §6): every offset below is read out of a decompiled or
// disassembled Runtime-x64.dll body, and the address of that body is named next
// to it. Nothing here is inferred from the profile build — the profile build's
// NAMES win, its OFFSETS must not (reference_encoding_vtable_offsets_differ_
// between_builds.md). Nothing here is inferred from an exe-side struct either.
//
// THE THREE CLASSES, AND HOW THEY RELATE
// --------------------------------------
//   Achievement::Goal          the resource: a rule loaded off disk. Derives
//                              Object::External, so it is refcounted-by-path
//                              and lives in the ExternalFactory.
//   Achievement::GoalInstance  the live tracker for one Goal in one game.
//                              Derives Object::Internal, so it is handle-table
//                              managed and serialized by {index, generation}.
//   Achievement::GoalList      a bare Collection::Array of GoalInstance
//                              handles. NOT an Object at all — no vtable.
//
// The exe subclasses all of Goal/GoalInstance (StatGoal, TimeGoal,
// PopulationGoal, BuildingGoal, FinishTutorialGoal, ...), so these three
// layouts are the PREFIX of a dozen exe types in code/Game/Goal/ and
// code/Game/Tutorial/. Widening any of them silently moves every exe field.
//
// PORTABILITY: no <windows.h>, per PLAN.md §4.1.
// ============================================================================

#include <cstdint>
#include <cstddef>

#include "../collection/Collection.h"
#include "../object/External.h"
#include "../object/Internal.h"
#include "../system/String.h"

namespace RtAchievement {

// ============================================================
// Achievement::Goal — 0x30 bytes = Object::External (0x20) + 0x10.
//
// PINS
//   ??0Goal@Achievement@@QEAA@XZ (0x18009ed50) writes, in this order:
//       [this]      = ??_7Base@Object@@6B@       (0x180154788)
//       [this]      = ??_7External@Object@@6B@   (0x1801547a0)
//       [this+0x08] = 0   [this+0x10] = 0   [this+0x18] = 0
//       [this]      = ??_7Goal@Achievement@@6B@  (0x1801546b0)
//       [this+0x20] = 0   [this+0x28] = 0
//     Six qword stores covering 0x00..0x2f exactly — which is what fixes
//     sizeof at 0x30, and what makes the two Goal-owned fields +0x20 and +0x28.
//     The three successive vtable stores are MSVC's base-ctor inlining: Base's
//     and External's are dead stores overwritten before anything reads them.
//   ??1Goal@Achievement@@UEAA@XZ (0x18009edb0) re-stamps ??_7Goal, runs the
//     String release idiom on [this+0x28], then tail-jumps
//     ??1External@Object@@UEAA@XZ (0x1800bc950) — so +0x28 is a System::String
//     and everything below +0x20 belongs to External.
//   ?GetText@Goal@Achievement@@QEBA?AVString@System@@XZ (0x180001ba0) copies
//     [this+0x28] into the return slot and bumps [data-4] — String again.
//   ?GetStringTable@Goal@Achievement@@QEBA?AV?$ExternalPtr@VStringTable@UI@@@Object@@XZ
//     (0x180001b90) is one instruction: `*retval = [this+0x20]`. Its mangled
//     name is what NAMES +0x20 as ExternalPtr<UI::StringTable>.
//   ?Serialize@Goal@Achievement@@UEAA_NAEAVStream@IO@@@Z (0x18009ee20)
//     serializes exactly those two fields, in that order: an ExternalPtr
//     serializer over `this+0x20`, then String::Serialize over `this+0x28`.
//     A load therefore consumes precisely these two and nothing else.
//
// NOT MODELLED: Goal has no fields of its own below +0x20. The `_base` member
// carries External's own (path @+0x08, the 0x10-stride owning array @+0x10);
// see runtime/types/object/External.h, which pinned those in Wave 3.
// ============================================================
struct Goal {
    RtObject::External          _base;         // +0x00 .. +0x1f (vtable, path, refs)
    RtObject::ExternalPtr<void> _stringTable;  // +0x20  ExternalPtr<UI::StringTable>
    RtSystem::String            _text;         // +0x28  the goal's display text
};
static_assert(sizeof(Goal) == 0x30, "Achievement::Goal is 0x30 bytes (ctor 0x18009ed50 writes 0x00..0x2f)");
static_assert(offsetof(Goal, _base)        == 0x00, "Goal derives Object::External at +0x00");
static_assert(offsetof(Goal, _stringTable) == 0x20, "Goal::_stringTable @ +0x20 (GetStringTable 0x180001b90)");
static_assert(offsetof(Goal, _text)        == 0x28, "Goal::_text @ +0x28 (GetText 0x180001ba0, ~Goal 0x18009edb0)");

// ============================================================
// Achievement::GoalInstance — 0x20 bytes = Object::Internal (0x10) + 0x10.
//
// PINS
//   ??0GoalInstance@Achievement@@QEAA@XZ (0x18009ef00):
//       CALL ??0Internal@Object@@QEAA@XZ (0x1800bfd10)
//       [this]      = ??_7GoalInstance@Achievement@@6B@ (0x180154678)
//       [this+0x10] = 0   (a DWORD store — `MOV dword ptr [RBX+0x10],EAX`)
//       [this+0x18] = 0   (a QWORD store)
//     The dword-vs-qword split is what says +0x10 is 32 bits wide and +0x14 is
//     never written by the constructor at all.
//   ?IsMet@GoalInstance@...     (0x18009eed0)  `return [this+0x10] & 1`
//   ?HasFailed@GoalInstance@... (0x18009eec0)  `return ([this+0x10] >> 1) & 1`
//   ?Update@GoalInstance@...    (0x18009f010)  ORs 1 or 2 into [this+0x10] and
//                                              ANDs ~1 out of it
//     — three independent readers of the same dword as a bit set.
//   ?GetGoal@GoalInstance@Achievement@@QEBA?AV?$ExternalPtr@VGoal@Achievement@@@Object@@XZ
//     (0x180001fe0) is one instruction: `*retval = [this+0x18]`. Its mangled
//     name NAMES +0x18 as ExternalPtr<Achievement::Goal>.
//   ?CheckGoalState@GoalInstance@... (0x18009f060) dereferences [this+0x18] and
//     calls THAT object's vtable +0x40 — i.e. +0x18 holds a live Goal*, not a
//     handle. Corroborated by ?SetGoal@GoalInstance@... (0x18009eee0).
//   ?Persist@GoalInstance@... (0x18009ef70) serializes [this+0x18] as an
//     ExternalPtr and [this+0x10] as a uint, in that order.
//
// +0x14 IS REAL PADDING, NOT AN UNKNOWN FIELD: the constructor zeroes +0x10 as
// a dword and +0x18 as a qword, so +0x14 is left holding whatever the
// allocation held. No decompiled body in this lane reads or writes it. It is
// declared explicitly so `sizeof` is right and so nobody "discovers" a field
// there later without repinning it.
// ============================================================
namespace GoalStateBits {
    constexpr uint32_t kMet    = 0x1;   // set when CheckGoalState returns 1
    constexpr uint32_t kFailed = 0x2;   // set when CheckGoalState returns 2; latches
}

// Achievement::Goal::GoalState, the enum CheckGoalState returns (vtable +0x30
// on GoalInstance). Named from the mangled return type
// `?AW4GoalState@Goal@2@`; the three values are the three EAX comparisons in
// Update (0x18009f010). There is no fourth case — any other value leaves the
// flags untouched, which is how an "unknown/pending" state is expressed.
enum GoalState : int32_t {
    kGoalStateNotMet = 0,
    kGoalStateMet    = 1,
    kGoalStateFailed = 2,
};

struct GoalInstance {
    RtObject::Internal          _base;    // +0x00 .. +0x0f (vtable, factoryIndex, generation)
    uint32_t                    _flags;   // +0x10  GoalStateBits
    uint32_t                    _pad14;   // +0x14  NEVER written by the ctor — real padding
    RtObject::ExternalPtr<void> _goal;    // +0x18  ExternalPtr<Achievement::Goal>
};
static_assert(sizeof(GoalInstance) == 0x20, "Achievement::GoalInstance is 0x20 bytes");
static_assert(offsetof(GoalInstance, _base)  == 0x00, "GoalInstance derives Object::Internal at +0x00");
static_assert(offsetof(GoalInstance, _flags) == 0x10, "GoalInstance::_flags @ +0x10 (IsMet 0x18009eed0)");
static_assert(offsetof(GoalInstance, _goal)  == 0x18, "GoalInstance::_goal @ +0x18 (GetGoal 0x180001fe0)");

// ============================================================
// Achievement::GoalList — 0x10 bytes, and it is EXACTLY one Collection::Array.
//
// PINS
//   ??0GoalList@Achievement@@QEAA@XZ (0x18009f0b0) zeroes +0x00 and +0x08 and
//     nothing else.
//   ?GetGoalCount@GoalList@... (0x180001700) `return *(int*)(this+8)`
//     — the Array's _size, at Array's own +0x08.
//   ?GetGoal@GoalList@... (0x180001710) `*retval = ((void**)*this)[index]`
//     — an 8-byte-stride index off the Array's _data at +0x00, and 8 bytes is
//     sizeof(InternalPtr<T>).
//   ??1GoalList@Achievement@@QEAA@XZ (0x18009f420) walks [0, count) calling
//     each element's vtable slot 0 with flag 1 (the scalar DELETING destructor,
//     so the list OWNS its instances), nulls each slot, frees the buffer under
//     the allocator's +0x640 critical section, then zeroes +0x00 and +0x08 as
//     two qwords — clearing _size and _capacity together, which is the
//     Collection::Array teardown shape Wave 2 pinned.
//   ?Initialize@GoalList@Achievement@@QEAAXV?$Array@V?$ExternalPtr@VGoal@
//     Achievement@@@Object@@@Collection@@@Z (0x18009f0e0) hands `this` straight
//     to the Array resize helper FUN_180009d70 with the source array's count.
//
// The ELEMENT TYPE is named by GetGoal's mangled return type,
// `InternalPtr<GoalInstance>`, which is Object::InternalBasePtr's 8 bytes
// (runtime/types/object/Internal.h). Note the asymmetry, which is real and
// load-bearing: GoalList holds INTERNAL pointers to instances, while each
// instance holds an EXTERNAL pointer to its Goal resource.
//
// GoalList is NOT an Object: no vtable, no type id, its destructor is
// non-virtual (`??1GoalList@Achievement@@QEAA@XZ`, note the Q not U). It is a
// plain member of AchievementInstance (?GetGoalList@AchievementInstance@... ,
// 0x180001960, returns `const GoalList&`).
// ============================================================
struct GoalList {
    Collection::Array<RtObject::InternalBasePtr> _goals;   // +0x00 data, +0x08 size, +0x0c capacity
};
static_assert(sizeof(GoalList) == 0x10, "Achievement::GoalList is one Collection::Array");
static_assert(offsetof(GoalList, _goals) == 0x00, "GoalList::_goals @ +0x00");

// ---- retail vtable slot offsets, read out of RETAIL .rdata ------------------
// ??_7GoalInstance@Achievement@@6B@ is 0x180154678 and ??_7Goal@Achievement@@6B@
// is 0x1801546b0, i.e. GoalInstance's table is exactly 7 slots (0x38 bytes)
// wide and Goal's begins where it ends. Both are EXPORTED data symbols, so a
// port stamps the DLL's own table by name and never owns one of its own
// (the Wave 3 finding: "147 ??_7 vtables ARE exported").
//
// Only the slots this lane actually dispatches through are listed. Every one is
// corroborated by a retail CALL instruction, named next to it. Do NOT take a
// slot number from the profile build.
namespace GoalInstanceVtbl {
    constexpr int kScalarDeletingDtor = 0x00;  // 0x1800020c0 — ~GoalList (0x18009f420) calls [0](p,1)
    constexpr int kPersist            = 0x18;  // 0x18009ef70
    constexpr int kStartup            = 0x20;  // 0x18009f000
    constexpr int kUpdate             = 0x28;  // 0x18009f010
    constexpr int kCheckGoalState     = 0x30;  // 0x18009f060 — Update does `CALL [RAX+0x30]`
}
namespace GoalVtbl {
    constexpr int kScalarDeletingDtor = 0x00;  // 0x180001d70
    constexpr int kSerialize          = 0x28;  // 0x18009ee20
    constexpr int kGetInstanceType    = 0x48;  // 0x18009eeb0 — GoalList::Initialize does `CALL [..+0x48]`
}

}  // namespace RtAchievement
