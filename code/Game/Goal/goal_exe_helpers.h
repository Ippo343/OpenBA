#pragma once
// Declarations for Goal-module exe-helper reimplementations.
// See code/Game/Goal/goal_exe_helpers.cpp.

#include <cstddef>
#include <cstdint>
#include "Entity/CitizenBehavior.h"   // the entity-bucket trees' owner

namespace Object { class External; }
namespace System { class String; }
class ComponentEntity;
class ComponentDescription;
class Profession;

// The by-name External reference retail's goals and tutorial steps hold
// (0x18 bytes): the resource's name, the ExternalFactory generation it was
// last resolved at, and the cached result. Retail's ctor is inlined
// (StatGoal 0x1400daad2: name = 0, generation = -1, cached = 0).
struct NamedExternalRef {
    // +0x00 System::String. H5: raw storage — a String member would run the
    // out-of-line System::String() where retail stores 0 (0x1400daad2) and a
    // destructor where the owners destroy it by hand. LEFT: Name() is the one
    // reinterpretation.
    uint8_t  name[8];
    int32_t  generation;            // +0x08 ExternalFactory generation (-1: never resolved)
    uint8_t  _pad0C[4];             // +0x0C written by neither the ctor nor the resolver
    Object::External* resolved;     // +0x10 the cached External (null when not found)

    System::String&       Name();          // goal_exe_helpers.cpp
    const System::String& Name() const;

    // 0x1400265b0: ExternalPtr<External>'s lazy resolve by name.
    void Resolve();
    // 0x140027de0: ExternalPtr<Toolbar>'s (Tutorial/tutorial_base.cpp).
    void ResolveToolbar();
};
static_assert(sizeof(NamedExternalRef) == 0x18, "the by-name reference is 0x18");
static_assert(offsetof(NamedExternalRef, generation) == 0x08, "NamedExternalRef +0x08 generation");
static_assert(offsetof(NamedExternalRef, resolved) == 0x10, "NamedExternalRef +0x10 resolved");

// A Collection::Array<NamedExternalRef> header as the goals hold it (H5: raw
// triple; the owners free it by hand through 0x14001b030).
struct NamedExternalRefArray {
    NamedExternalRef* data;         // +0x00
    int32_t  count;                 // +0x08
    int32_t  capacity;              // +0x0C
};
static_assert(sizeof(NamedExternalRefArray) == 0x10, "Array header is 0x10");

// The CitizenBehavior entity-bucket trees (+0x100 keyed by Profession, +0x160 keyed by
// ComponentDescription) are CitizenBehavior.h's ProfessionBucket / TemplateBucket
// (CitizenBucketNode<Key>, 0x38 bytes); their roots are field_0x100 / field_0x160.

// FUN_14001b340 - NamedExternalRef array resize (stride 0x18).
void Goal_RefArray_Resize(NamedExternalRefArray* arr, uint32_t newCount);

// ENDGAME B.3.8 block GOAL: `Goal_FillInheritedVtableSlots` (the by-name fill
// of slots 4 / 7 / 9 into the hand-built Goal tables) is gone with those
// tables — the Goal leaves are real classes, and Compile / GetInstanceType are
// inherited from Achievement::Goal, Destruct is each leaf's own member.
