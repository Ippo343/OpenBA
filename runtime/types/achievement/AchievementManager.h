#pragma once
// ============================================================================
// runtime/types/achievement/AchievementManager.h — Wave 5, lane B
// Achievement::Achievement / AchievementInstance / AchievementManager layouts
// ============================================================================
//
// Every offset below is pinned by a named instruction in RETAIL
// Runtime-x64.dll (image base 0x180000000), read in Ghidra on 2026-09-06.
// Runtime-x64-profile.dll (the ToolKit build) is used here for NAMES ONLY, per
// PLAN.md §6 step 1 and reference_encoding_vtable_offsets_differ_between_builds.md.
//
// The two builds AGREE on this class's layout, and that was checked rather
// than assumed: the profile ctor (0x180028b40) zeroes exactly the same nine
// slots +0x00..+0x40 as retail's (0x18009eb00), and the profile's
// GetAchievementInstance (0x1800293a0) bounds-checks against +0x20 and indexes
// through +0x18 exactly as retail's (0x18009e8f0) does. Where a name below
// came out of the profile build it says so; where it is OUR name it says that
// too.
//
// PORTABILITY: no <windows.h>, per PLAN.md §4.1.
// ============================================================================

#include <cstdint>
#include <cstddef>

#include "../collection/Collection.h"
#include "../object/External.h"
#include "../object/Internal.h"
#include "../system/String.h"
#include "../system/Value.h"

namespace RtAchievement {

// ============================================================
// Achievement::GoalList — the goal-instance array a GoalList IS.
//
// LANE OWNERSHIP NOTE: Achievement::Goal / GoalInstance / GoalList are Wave 5
// LANE A's types, and lane A owns `runtime/src/achievement/goal.cpp` and any
// header it writes for them. This struct is deliberately named `GoalListView`
// and holds ONLY what lane B's three functions read, so the two lanes cannot
// collide on a type name. When lane A lands a real `GoalList`, this view
// should be deleted and its users pointed at that.
//
// GoalList is exactly a Collection::Array<InternalPtr<GoalInstance>>, 16 bytes:
//   ?GetGoalCount@GoalList@Achievement@@QEBAHXZ (0x180001700) is `MOV EAX,[RCX+8]`
//   ?GetGoal@GoalList@Achievement@@QEBA... (0x180001710) indexes `*(this+0x00)`
//   AchievementInstance::IsMet (0x18009e2f0) walks `*(this+0x10)` bounded by
//     `*(int*)(this+0x18)` — i.e. the GoalList embedded at instance+0x10.
// ============================================================
struct GoalInstanceView;

struct GoalListView {
    Collection::Array<GoalInstanceView*> goals;   // +0x00 {_data, _size, _capacity}
};
static_assert(sizeof(GoalListView) == 0x10, "Achievement::GoalList is 16 bytes");

// ============================================================
// Achievement::GoalInstance — only its flag word is in lane B's scope.
//
// AchievementInstance::IsMet (0x18009e2f0) and ::HasFailed (0x18009e2b0) read
// ONE dword, at +0x10, and test two bits of it:
//     bit 0  (0x1)  met
//     bit 1  (0x2)  failed
// IsMet requires bit 0 set AND bit 1 clear on every goal; HasFailed is true if
// bit 1 is set on any goal. Nothing else about GoalInstance is read here — its
// real layout is lane A's.
// ============================================================
struct GoalInstanceView {
    void*    vptr;    // +0x00
    uint64_t _pad08;  // +0x08
    uint32_t flags;   // +0x10 — bit 0 met, bit 1 failed
};
static_assert(offsetof(GoalInstanceView, flags) == 0x10, "GoalInstance flags @ +0x10");

namespace goalflags {
    constexpr uint32_t kMet    = 0x1;
    constexpr uint32_t kFailed = 0x2;
}

// ============================================================
// Achievement::Achievement — an Object::External subclass, 0x58 bytes.
//
// SIZE is pinned by the default ctor ??0Achievement@0@QEAA@XZ (0x18009e240),
// whose last store is the qword at +0x50; the array at +0x48 therefore ends at
// +0x58. FIELDS, each by the one-instruction accessor that reads it:
//   +0x20 ?GetStringTable@  (0x180001380)  `*retval = *(this+0x20)`
//   +0x28 ?GetTitle@        (0x1800013e0)  `*(this+0x28)`
//   +0x30 ?GetDescription@  (0x180001420)  `*(this+0x30)`
//   +0x38 ?GetSpriteSheet@  (0x180001390)  `*retval = *(this+0x38)`
//   +0x40 ?GetSpriteName@   (0x1800013a0)  `*(this+0x40)`
//   +0x48 ?GetGoalList@     (0x180001460)  `return this+0x48`
// and cross-checked field-for-field against ?Serialize@Achievement@1@
// (0x18009e070), which streams them in exactly that order:
//   ExternalPtr, String, String, ExternalPtr, String, then an int count at
//   +0x50 followed by the array body at +0x48.
// ============================================================
struct Achievement {
    RtObject::External base;                      // +0x00 .. +0x1f
    void*              stringTable;               // +0x20 ExternalPtr<UI::StringTable>
    RtSystem::String   title;                     // +0x28
    RtSystem::String   description;               // +0x30
    void*              spriteSheet;               // +0x38 ExternalPtr<UI::SpriteSheet>
    RtSystem::String   spriteName;                // +0x40
    Collection::Array<void*> goals;               // +0x48 Array<ExternalPtr<Goal>>
};
static_assert(offsetof(Achievement, stringTable) == 0x20, "Achievement::_stringTable @ +0x20");
static_assert(offsetof(Achievement, title)       == 0x28, "Achievement::_title @ +0x28");
static_assert(offsetof(Achievement, description) == 0x30, "Achievement::_description @ +0x30");
static_assert(offsetof(Achievement, spriteSheet) == 0x38, "Achievement::_spriteSheet @ +0x38");
static_assert(offsetof(Achievement, spriteName)  == 0x40, "Achievement::_spriteName @ +0x40");
static_assert(offsetof(Achievement, goals)       == 0x48, "Achievement::_goals @ +0x48");
static_assert(sizeof(Achievement) == 0x58, "Achievement is 0x58 bytes (ctor's last store is +0x50)");

// ============================================================
// Achievement::AchievementInstance — an Object::Internal subclass, 0x28 bytes.
//
// SIZE IS MEASURED, not inferred: ?CreateInstance@Achievement@1@ (0x18009df80)
// asks the allocator for exactly `Allocate(_singleton, 0x28, 8)` and then
// constructs an AchievementInstance in it.
//
// The ctor ??0AchievementInstance@Achievement@@QEAA@XZ (0x18009e3b0), read as
// bytes:
//     CALL 0x1800bfd10                 ; Object::Internal::Internal(this)
//     LEA  RAX,[0x180154700]           ; ??_7AchievementInstance@Achievement@@6B@
//     MOV  [RBX],RAX                   ; ... stamped over Internal's vtable
//     LEA  RAX,[RBX+0x10]; MOV [RAX],0; MOV [RAX+8],0   ; the GoalList
//     MOV  [RBX+0x20],0                ; the ExternalPtr<Achievement>
// and +0x20 is confirmed independently by SetAchievement (0x18009e440), whose
// first store is `MOV [RCX+0x20],RDX`, and by GetGoalList (0x180001960),
// `LEA RAX,[RCX+0x10]`.
// ============================================================
struct AchievementInstance {
    RtObject::Internal base;          // +0x00 .. +0x0f (vptr, factoryIndex, generation)
    GoalListView       goalList;      // +0x10 .. +0x1f
    Achievement*       achievement;   // +0x20 ExternalPtr<Achievement>
};
static_assert(offsetof(AchievementInstance, goalList)    == 0x10, "AchievementInstance::_goalList @ +0x10");
static_assert(offsetof(AchievementInstance, achievement) == 0x20, "AchievementInstance::_achievement @ +0x20");
static_assert(sizeof(AchievementInstance) == 0x28,
              "AchievementInstance is 0x28 bytes (CreateInstance allocates exactly 0x28)");

// ??_7AchievementInstance@Achievement@@6B@ — an EXPORTED data symbol
// (ordinal 1383) whose address is the very value the ctor stamps. Recorded
// here so the ctor port never has to clone slots out of a live object; see
// PLAN.md §2.2 D.
constexpr uintptr_t kAchievementInstanceVtableGhidra = 0x180154700;

// ============================================================
// Achievement::AchievementManager — 0x48 bytes, a process singleton.
//
// SIZE from the ctor ??0AchievementManager@Achievement@@QEAA@XZ (0x18009eb00),
// which zeroes eight qwords +0x00..+0x38 and one dword at +0x40.
//
// THREE OF THE SIX FIELD NAMES ARE THE ENGINE'S OWN, recovered from
// Runtime-x64-profile.dll's assert strings (`Achievement\AchievementManager.cpp`):
//     "_achievements"                        -> +0x00
//     "!_completed[_lastAchievementUpdate]"  -> +0x08 and +0x40
// `_instances`, `_eventHandler` and `_updateTimes` are OUR names; the profile
// build's asserts for those three only quote Array.h's generic
// "index >= 0 && index < _size", which names no member.
//
// FIELD PINS, one decompiled body each:
//   +0x00 GetAchievementCount (0x1800079b0) = `*(int*)(*(void**)this + 0x28)`
//         — i.e. this->_achievements->_count. THIS IS THE FIELD BEHIND THE
//         RECORDED "GetAchievementCount faults on a READ of 0x28" NOTE: the
//         `this` is fine, `_achievements` is the null one.
//   +0x08 IsAchievementAwarded (0x1800079c0) = `*(byte*)(index + *(this+8))`
//   +0x10 the same array's _size, bounded in ReadAchievements (0x18009eb90)
//   +0x18 GetAchievementInstance (0x18009e8f0) = `*(void**)(*(this+0x18) + i*8)`
//   +0x20 its _size, the loop bound in Reset (0x18009e5c0) and Update
//   +0x28 SetEventHandler (0x18009e6b0) = `*(void**)(this+0x28) = handler`
//   +0x30 Update (0x18009e6c0) `*(qword*)(*(this+0x30) + i*8) += time.ticks`
//   +0x38 its _size (profile Update asserts against +0x38 for that same index)
//   +0x40 Update's rotating cursor, `_lastAchievementUpdate`
//
// WHAT +0x30 ACTUALLY IS, because it is not obvious from retail alone: an
// Array<System::Time> of ACCUMULATED per-achievement time. Every tick, every
// not-yet-completed achievement's slot gets the frame delta added; when an
// achievement's turn comes round the cursor, its whole accumulated total is
// handed to AchievementInstance::Update and the slot is zeroed. So each
// instance is updated once every _size ticks with _size ticks' worth of time.
// (Retail inlines all of this; the profile build spells the call out as
// `AchievementInstance::Update(inst, _updateTimes[_lastAchievementUpdate])`.)
//
// INDEPENDENT WHOLE-STRUCT CONFIRMATION: the profile build's COPY CONSTRUCTOR
// ??0AchievementManager@Achievement@@QEAA@AEBV01@@Z (0x180029a40) walks the
// entire object once, and its shape is exactly the six fields below —
//     +0x00 copied as a raw qword                 (a non-owning pointer)
//     +0x08 zeroed, then a 1-byte-element Array copy helper
//     +0x18 zeroed, then an 8-byte-element Array copy helper
//     +0x28 copied as a raw qword                 (a function pointer)
//     +0x30 zeroed, then an 8-byte-element Array copy helper
//     +0x40 copied as a raw dword
// — three Arrays needing deep copies, two raw pointers and one int, in this
// order. No field is missed and none is left over.
// ============================================================
struct AchievementManager {
    RtObject::ExternalList*                    achievements;           // +0x00 `_achievements`
    Collection::Array<uint8_t>                 completed;              // +0x08 `_completed`
    Collection::Array<AchievementInstance*>    instances;              // +0x18 (our name)
    void                                    (*eventHandler)(int);      // +0x28 (our name)
    Collection::Array< ::Time>                 updateTimes;            // +0x30 (our name)
    int32_t                                    lastAchievementUpdate;  // +0x40 `_lastAchievementUpdate`
    int32_t                                    _pad44;                 // +0x44 — never read or written
};
static_assert(offsetof(AchievementManager, achievements)          == 0x00, "_achievements @ +0x00");
static_assert(offsetof(AchievementManager, completed)             == 0x08, "_completed @ +0x08");
static_assert(offsetof(AchievementManager, instances)             == 0x18, "instances @ +0x18");
static_assert(offsetof(AchievementManager, eventHandler)          == 0x28, "eventHandler @ +0x28");
static_assert(offsetof(AchievementManager, updateTimes)           == 0x30, "updateTimes @ +0x30");
static_assert(offsetof(AchievementManager, lastAchievementUpdate) == 0x40, "_lastAchievementUpdate @ +0x40");
static_assert(sizeof(AchievementManager) == 0x48,
              "AchievementManager is 0x48 bytes (ctor zeroes +0x00..+0x40)");

// IO::FileSystem+0x38 — the count behind
// ?AreUserPackFilesEnabled@FileSystem@IO@@QEBA_NXZ (retail 0x1800acfc0), which
// AchievementManager::Update inlines as `CMP dword [RAX+0x38],0 / JG skip`:
// if any user pack file (a mod) is mounted, achievements do not tick at all.
//
// runtime/types/io/FileSystem.h (Wave 4 lane D) currently buries this dword in
// `_unknown2C` with the comment "+0x30/+0x38 a package-filename array+count
// (NOT read by anything in this file)". True of that file, but the field IS
// read — here — and it now has a name. See WAVE5_LANE_B.md's corrections list.
constexpr size_t kFileSystemUserPackFileCountOffset = 0x38;

}  // namespace RtAchievement
