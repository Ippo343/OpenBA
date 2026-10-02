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
    const Object::External* resolved;   // +0x10 the cached External (null when not found)

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

// ============================================================
// The CitizenBehavior entity-bucket trees (+0x100 keyed by Profession,
// +0x160 keyed by ComponentDescription): 0x38-byte nodes whose element is an
// Array<InternalPtr<ComponentEntity>> (0x1400d81b0, 0x1400d7580).
// ============================================================
template <class Key>
struct EntityBucketNode {
    Key*              key;        // +0x00 compared by address
    ComponentEntity** entities;   // +0x08 Array<InternalPtr<ComponentEntity>> data
    int32_t           count;      // +0x10
    int32_t           capacity;   // +0x14
    EntityBucketNode* parent;     // +0x18
    EntityBucketNode* left;       // +0x20
    EntityBucketNode* right;      // +0x28
    int32_t           balance;    // +0x30
    uint8_t           _pad34[4];  // +0x34
};
static_assert(sizeof(EntityBucketNode<Profession>) == 0x38, "the CitizenBehavior tree node is 0x38");
static_assert(offsetof(EntityBucketNode<Profession>, entities) == 0x08, "EntityBucketNode +0x08");
static_assert(offsetof(EntityBucketNode<Profession>, count) == 0x10, "EntityBucketNode +0x10");
static_assert(offsetof(EntityBucketNode<Profession>, left) == 0x20, "EntityBucketNode +0x20");
static_assert(offsetof(EntityBucketNode<Profession>, right) == 0x28, "EntityBucketNode +0x28");

using ProfessionBucketNode = EntityBucketNode<Profession>;
using TemplateBucketNode   = EntityBucketNode<ComponentDescription>;

// The roots (goal_exe_helpers.cpp; LEFT: CitizenBehavior::field_0x100 /
// field_0x160 are `void*` in WORK-a's header, no node type declared there).
ProfessionBucketNode* ProfessionTreeRoot(const CitizenBehavior* cb);
TemplateBucketNode*   TemplateTreeRoot(const CitizenBehavior* cb);

// FUN_14001b340 - NamedExternalRef array resize (stride 0x18).
void Goal_RefArray_Resize(NamedExternalRefArray* arr, uint32_t newCount);

// ENDGAME B.3.8 block GOAL: `Goal_FillInheritedVtableSlots` (the by-name fill
// of slots 4 / 7 / 9 into the hand-built Goal tables) is gone with those
// tables — the Goal leaves are real classes, and Compile / GetInstanceType are
// inherited from Achievement::Goal, Destruct is each leaf's own member.
