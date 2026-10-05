#pragma once
// Declarations for Goal-module exe-helper reimplementations.
// See code/Game/Goal/goal_exe_helpers.cpp.

#include <cstddef>
#include <cstdint>
#include "Entity/CitizenBehavior.h"   // the entity-bucket trees' owner
#include "system/String.h"
#include "collection/Collection.h"

namespace Object { class External; }
class ComponentEntity;
class ComponentDescription;
class Profession;

// The by-name External reference retail's goals and tutorial steps hold
// (0x18 bytes): the resource's name, the ExternalFactory generation it was
// last resolved at, and the cached result.
// B4 Y GAMEA (H5): `name` is a real System::String. Retail's default ctor is
// inlined in every owner (StatGoal 0x1400daad2, TutorialGoalCount 0x1400dfaa3,
// TutorialSetupRestrictPlayer 0x1400e326f: String() qword 0, dword -1, qword 0,
// +0x0C unwritten, each under the owner's EH subobject marker); its copy ctor
// is 0x14001af90 (String copy, then +0x08 dword, +0x10 qword); its dtor is
// ~String alone, which every owner's dtor runs after its body in reverse
// declaration order (StatGoal 0x140026540, TutorialGoalCount 0x140027820,
// TutorialGoalDialog 0x140027590, TutorialImageHotKey 0x140027d70,
// TutorialImageToolbarIcon 0x140028090, TutorialSetupDialog 0x1400e2bf0,
// TutorialSetupRestrictPlayer 0x140028bb0).
struct NamedExternalRef {
    System::String name;            // +0x00 the resource's name
    int32_t  generation;            // +0x08 ExternalFactory generation (-1: never resolved)
    uint8_t  _pad0C[4];             // +0x0C written by neither the ctor nor the resolver
    Object::External* resolved;     // +0x10 the cached External (null when not found)

    NamedExternalRef() : generation(-1), resolved(nullptr) {}
    // 0x14001af90: +0x0C is not copied.
    NamedExternalRef(const NamedExternalRef& src)
        : name(src.name), generation(src.generation), resolved(src.resolved) {}
    NamedExternalRef& operator=(const NamedExternalRef&) = delete;

    // 0x1400265b0: ExternalPtr<External>'s lazy resolve by name.
    void Resolve();
    // 0x140027de0: ExternalPtr<Toolbar>'s (Tutorial/tutorial_base.cpp).
    void ResolveToolbar();
};
static_assert(sizeof(NamedExternalRef) == 0x18, "the by-name reference is 0x18");
static_assert(offsetof(NamedExternalRef, generation) == 0x08, "NamedExternalRef +0x08 generation");
static_assert(offsetof(NamedExternalRef, name) == 0x00, "NamedExternalRef +0x00 name");
static_assert(offsetof(NamedExternalRef, resolved) == 0x10, "NamedExternalRef +0x10 resolved");

// The element row of Array<NamedExternalRef> (goal_exe_helpers.cpp): retail's
// resize 0x14001b340 and destroy 0x14001b030 (the same two bodies
// MapGeneratorRows::kSpec_ExternalRef stands for).
namespace GoalRows {
extern const rtcoll::ArraySpec kSpec_GoalRefEntry;   // exe 0x14001b340 / 0x14001b030
}  // namespace GoalRows

// Collection::Array<NamedExternalRef> as the goals hold it. B4 Y GAMEA (H5): its
// ctor is the three zero stores (BuildingGoal's create 0x140025dc4 / 0x140025dc8),
// its dtor is ClearCallback (each element's ~String, MemFree, zero stores): the
// 0x14001b030 the owner's dtor calls (BuildingGoal 0x140025eab).
using NamedExternalRefArray = Collection::Array<NamedExternalRef, Collection::DefaultAllocator,
                                                Collection::ExplicitSpec<&GoalRows::kSpec_GoalRefEntry>>;
static_assert(sizeof(NamedExternalRefArray) == 0x10, "Array header is 0x10");

// The CitizenBehavior entity-bucket trees (+0x100 keyed by Profession, +0x160 keyed by
// ComponentDescription) are CitizenBehavior.h's ProfessionBucket / TemplateBucket
// (CitizenBucketNode<Key>, 0x38 bytes); their roots are field_0x100 / field_0x160.

// ENDGAME B.3.8 block GOAL: `Goal_FillInheritedVtableSlots` (the by-name fill
// of slots 4 / 7 / 9 into the hand-built Goal tables) is gone with those
// tables — the Goal leaves are real classes, and Compile / GetInstanceType are
// inherited from Achievement::Goal, Destruct is each leaf's own member.
