// code/Game/Goal/goal_exe_helpers.cpp
// Clean reimplementation of the StatGoal lazy ExternalPtr resolver
// (FUN_1400265b0), ported to close the standalone Goal code-gap.
//
// Depends only on engine_funcs.h + resolve.h.
// Decompiled from Application-x64.exe via Ghidra.

#include "goal_exe_helpers.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "system/String.h"
#include "system/Filename.h"
#include "object/External.h"   // ENDGAME B.3.3 step 6A
#include "collection/Collection.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
#include "Goal/exe_bodies.h"

// ============================================================
// StatGoal ExternalPtr resolver (FUN_1400265b0)
// ============================================================
// Identical in shape to ResolveExternalByName_Impl (engine_funcs.h): a lazy
// ExternalPtr<T>::Resolve. The input points at an ExternalPtr struct:
//   +0x00: String (8 bytes) — resource name
//   +0x08: int32  — cached generation counter (vs ExternalFactory+0x08)
//   +0x10: void*  — cached External* result
// If the cached generation matches the factory's, the cached pointer is valid.
// Otherwise it converts the name String to a Filename, calls
// ExternalFactory::Find, validates the result against the ExternalPtr's
// expected type (TypeInfo at *0x140152870), caches it (or null), and updates
// the generation.
//
// In standalone the type-id storage slot (0x140152870) is unregistered → null,
// so the IsDerivedFrom check is skipped and the Find result is trusted — same
// strategy as ResolveExternalByName_Impl's TypeId_ComponentDescription_FC guard.
void StatGoal_ResolveExternalPtr(void* externalRef) {
    uintptr_t s = reinterpret_cast<uintptr_t>(externalRef);
    if (!::Object::ExternalFactory::_singleton) return;
    void* factory = ::Object::ExternalFactory::_singleton;
    ::Object::ExternalFactory* efactory =
        reinterpret_cast<::Object::ExternalFactory*>(factory);  // B.3.3 bridge

    int32_t refGen     = *reinterpret_cast<int32_t*>(s + 0x08);
    int32_t factoryGen = *reinterpret_cast<int32_t*>(reinterpret_cast<uintptr_t>(factory) + 0x08);
    if (refGen == factoryGen) return;

    // +0x00 of the ExternalPtr is the name String (B.3.1 bridge: the containing
    // ExternalPtr struct is still untyped bytes).
    System::Filename filename(reinterpret_cast<const System::String&>(*reinterpret_cast<char*>(s)));
    void* found = const_cast<void*>(static_cast<const void*>(efactory->Find(filename)));

    // STANDALONE DEVIATION, deliberate — identical to the one documented at
    // length in TutRestrict_ResolveRefOrLoad (tutorialsetuprestrictplayer.cpp)
    // and TutBase_ResolveToolbarRef (tutorial_base.cpp).
    //
    // Retail's Find() alone is enough because UpdateLoop::LoadResources has
    // already preloaded the Game/GameResources.rsc dependency tree INTO the
    // ExternalFactory; ours leaves those entries in the loop's m_resourceList,
    // so Find() misses for anything nothing else happened to load.
    //
    // This resolver feeds TutorialGoalCount::Check, whose "Zone" mode counts
    // entities in the EntityList's per-description BST keyed by the resolved
    // description POINTER. A miss writes null, the BST is then searched for key
    // 0, the count comes back 0, and the goal can never be met — the tutorial
    // stalls forever with no error anywhere. Measured on stepFirstClear2, whose
    // goal counts "Template\Clear.rsc": the WoodHouse goal one step earlier only
    // worked because the step's own restriction had FindOrLoad'ed that exact
    // name into the factory first. Nothing ever loads Clear.rsc by name.
    //
    // Retail would fault on neither leg (it never has the null), so this is not
    // a guard over a faithful crash — it is the standalone resource-environment
    // gap. The real fix remains making the preload populate the factory.
    bool loaded = false;
    if (!found) {
        const char* name = *reinterpret_cast<const char* const*>(s);
        if (name && name[0]) {
            // Only the LOADER rejects backslashes; the factory's key comparison
            // normalizes separators (measured — see tutorial_base.cpp).
            char normalized[260];
            size_t k = 0;
            for (; name[k] && k < sizeof(normalized) - 1; ++k)
                normalized[k] = (name[k] == '\\') ? '/' : name[k];
            normalized[k] = 0;

            System::String   normStr(normalized);
            System::Filename normFile(normStr);
            found = const_cast<void*>(static_cast<const void*>(efactory->FindOrLoad(normFile)));
            loaded = true;
        }
    }

    void* result = nullptr;
    if (found) {
        // Retail reads the type id through the import slot 0x140152870
        // (?_typeInfo@External@Object@@). ENDGAME B.3.9 TR: the class accessor; the
        // slot row (reg_unreg_lane1.cpp) is gone with its last reader.
        if (::Object::IsDerivedFromNullSafe(found, ::Object::External::TypeId())) {
            result = found;
        }
    }

    *reinterpret_cast<void**>(s + 0x10) = result;
    // Re-read the generation: a FindOrLoad that actually loaded bumps it, and
    // stamping the pre-load value would make the next call redo the whole
    // lookup for nothing.
    *reinterpret_cast<int32_t*>(s + 0x08) =
        *reinterpret_cast<int32_t*>(reinterpret_cast<uintptr_t>(factory) + 0x08);

}

// ============================================================
// Reference-triple array resize (FUN_14001b340)
// ============================================================
// Callers: BuildingGoal::Serialize (0x1400d76a0) and MapGenerator::Serialize
// (0x1400a2980), both on the LOAD leg only (serializer m_encoding == 0).
// `arr` is the familiar {data* @+0x00, count @+0x08, capacity @+0x0c} header.
//
// Same primitive as EventArray_Grow in code/Game/Game/eventlog.cpp — including
// the identical triple-capacity heuristic on a single-element grow — but with a
// 0x18 stride, over the ExternalPtr triple that StatGoal_ResolveExternalPtr and
// ResolveExternalByName_Impl operate on:
//   +0x00 System::String (resource name)   +0x08 int32 cached generation
//   +0x10 void* cached External*
//
// ENDGAME B.3.2 step 2: the body below WAS a hand-written copy of
// `rtcoll::SetSizeCore`. It is now one `Collection::Array<>::SetSize` driven by
// this instantiation's element ops, spelled as an `ArraySpec` row exactly like
// the 27 rows runtime/src/collection/array.cpp already carries. Compared arm for
// arm against SetSizeCore before the swap: exact-equal early return, `_size`
// stored before the capacity test, treble-on-single-append-from-non-empty,
// copy-construct the preserved prefix only when the block moved, init the new
// tail, destroy the truncated tail in place / the whole old buffer on a realloc,
// then free. Two differences, both deliberate:
//   * the allocation now goes through `?MemAlloc@MemoryAllocator@System@@`
//     (0x1800c7a50, the LOCKING static) instead of the unlocked instance
//     `Allocate` (0x1800c6e60) this body reached through `fn_MemAlloc`. Same
//     allocator and same heap (runtime/src/system/allocator.cpp:676), plus the
//     critical section retail's own Collection code takes.
//   * the old body returned early when the allocation came back null;
//     SetSizeCore instead skips a null element address per iteration. Both are
//     out-of-memory-only arms and neither is reachable while MemAlloc works.
namespace {

// The element: {System::String name @+0x00, int32 generation @+0x08,
//               External* cached @+0x10}, stride 0x18, align 8.
// +0x0c..+0x0f is written by NEITHER arm, which — together with the -1
// generation seed — is why this row cannot be derived from a C++ type's traits.
void AssignGoalRefEntry(uint8_t* d, const uint8_t* s) {
    // B.3.2 bridge: the entry is still untyped bytes in the BuildingGoal struct.
    new (d) System::String(*reinterpret_cast<const System::String*>(s));     // +0x00
    *reinterpret_cast<uint32_t*>(d + 0x08) = *reinterpret_cast<const uint32_t*>(s + 0x08);
    *reinterpret_cast<uint64_t*>(d + 0x10) = *reinterpret_cast<const uint64_t*>(s + 0x10);
}

// ⚠ The tail SEED VALUES are load-bearing: a newly added entry gets generation
// **-1**, not 0. Zero is a legitimate ExternalFactory generation, so a 0 seed
// would make the lazy resolver take its "cache still valid" early-out while the
// pointer is still null — the entry would silently never resolve. Faithful to
// retail, which stores 0xffffffff explicitly.
void InitGoalRefEntry(uint8_t* d) {
    *reinterpret_cast<uint64_t*>(d + 0x00) = 0;
    *reinterpret_cast<uint32_t*>(d + 0x08) = 0xFFFFFFFFu;   // generation -1
    *reinterpret_cast<uint64_t*>(d + 0x10) = 0;
}

void DestroyGoalRefEntry(void* /*ignoredThis*/, uint8_t* base, int count) {
    for (int i = 0; i < count; ++i)
        reinterpret_cast<System::String*>(base + static_cast<int64_t>(i) * 0x18)->~String();
}

// stays explicit: a String copy-ctor `assign` (byteCopy=false) with a real
// `destroy`, the load-bearing -1 generation seed in `init`, and an unwritten
// extent at [0x0c,0x10).
const rtcoll::ArraySpec kSpec_GoalRefEntry = {          // exe 0x14001b340
    "Array<{String,int32,External*}>", 0x18, 8,
    &AssignGoalRefEntry, &InitGoalRefEntry, &DestroyGoalRefEntry, false,
    /*assign unwritten*/ 0x0c, 0x10,
    /*init   unwritten*/ 0x0c, 0x10,
    /*seed zero       */ 0,    0 };

using GoalRefArray = Collection::Array<uint8_t, Collection::DefaultAllocator,
                                       Collection::ExplicitSpec<&kSpec_GoalRefEntry>>;

}  // namespace

void Goal_RefArray_Resize(void* arr, uint32_t newCount) {
    if (!arr) return;
    // B.3.2 bridge: BuildingGoal+0x38 is still a raw 0x10-byte array header in
    // an unconverted exe struct; the cast goes when BuildingGoal becomes a class.
    reinterpret_cast<GoalRefArray*>(arr)->SetSize(static_cast<int>(newCount));
}
