// code/Game/Goal/buildinggoal.cpp
// Clean reimplementation of BuildingGoal vtable methods.
//
// BuildingGoal (0x48 bytes) — achievement goal requiring building count.
// Vtable: 0x140160300
//
// Fields (from Ghidra decompilation):
//   +0x30: bool   requireAll (_field30)  — true = all buildings required, false = any
//   +0x38: void*  buildingTypeArray      — array of 0x18-byte entries
//   +0x40: int    buildingTypeCount      — number of entries
//
// Each entry in the array is 0x18 bytes:
//   +0x00: System::String (8 bytes) — building type name (an ExternalPtr triple)
//   +0x08: uint32 — cached ExternalFactory generation (-1 = never resolved)
//   +0x10: External* — RESOLVED ComponentDescription pointer, and the key the
//                    world-building BST is searched on. Filled lazily by
//                    ResolveExternalByName_Impl (0x14001b1d0); it is NOT a hash.
//
// The world building BST is at game+0xF0+0x160.
// BST nodes: +0x00=key (uint64), +0x10=count (int), +0x20=left, +0x28=right
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Goal/BuildingGoal.h"
#include "Goal/goal_exe_helpers.h"
#include "object/Internal.h"      // Object::detail::InternalMemAlloc / InternalMemFree
#include "Object/factory_registration.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "system/String.h"
#include "system/Filename.h"
#include <new>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch D
#include "achievement/Goal.h"   // ENDGAME B.3.7: QUALIFIED base calls

// ============================================================
// Construction / destruction (ENDGAME B.3.8 block GOAL)
// ============================================================
// ctor 0x1400d77a0: Goal() (the base's real ctor), the vptr (the compiler's),
// then requireAll = 1 and the entry array header zeroed.
BuildingGoal::BuildingGoal() : _field30(1), _pad31{}, _field38(nullptr), _field40(nullptr) {}

// The dtor inside [0] 0x140025e80: destroy the entry array at +0x38
// (0x14001b030); the compiler then runs ~Goal (retail's CALL).
BuildingGoal::~BuildingGoal() {
    if (fn_BuildingTypeArray_Dtor) {
        fn_BuildingTypeArray_Dtor(static_cast<void*>(&_field38));
    }
}

// [0] 0x140025e80: ~BuildingGoal -> ~Goal -> ~External, MemFree on &1.
Object::Base* BuildingGoal::DeletingDtor(unsigned flags) {
    this->~BuildingGoal();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// ============================================================
// BuildingGoal::Serialize (vtable +0x28, 0x1400d76a0)
// ============================================================
// Calls Goal::Serialize, then serializes:
//   1. requireAll (bool at +0x30)
//   2. the entry count (int, a local copy)
//   3. on load (encoding mode 0): resize the array (0x14001b340)
//   4. each entry: Filename temporary, String::Serialize, resolve if loading
bool BuildingGoal::Serialize(void* stream) {
    uintptr_t s = reinterpret_cast<uintptr_t>(this);

    // Achievement::Goal::Serialize (slot 5, 0x18009ee20) — QUALIFIED: an
    // unqualified call would dispatch back into this slot.
    Achievement::Goal::Serialize(stream);

    // Create Serializer on stack (24 bytes)
    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // Serialize requireAll (bool at +0x30)
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<bool*>(s + 0x30), nullptr);   // B.3.4 bridge

    // Serialize entry count
    int entryCount = *(int*)(s + 0x40);
    uint32_t countLocal = (uint32_t)entryCount;
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(&countLocal), nullptr);   // B.3.4 bridge

    // Check if loading (encoding mode == 0 means loading)
    int encodingMode = static_cast<int>(reinterpret_cast<IO::Serializer*>(serializer)->GetDirection());   // B.3.4 bridge  (IO::Serializer::GetDirection, retail 0x180006210)
    if (encodingMode == 0) {
        // Loading: resize the entry array (0x14001b340, ported as Goal_RefArray_Resize).
        // Was a guarded raw resolve of an unregistered address, i.e. a silent no-op:
        // the array stayed empty and every entry below wrote through a null base.
        Goal_RefArray_Resize((void*)(s + 0x38), countLocal);
    }

    // Serialize each entry
    void* arrayBase = *(void**)(s + 0x38);
    if ((int)countLocal > 0) {
        uint8_t* entry = (uint8_t*)arrayBase;
        for (uint32_t i = 0; i < countLocal; i++) {
            // Filename temporary retail's compiler emitted for this loop body.
            System::Filename filenameBuf;
            (void)filenameBuf;

            // String::Serialize — serialize the string at entry+0x00
            // B.3.1 bridge: the ExternalPtr triple is still untyped bytes.
            reinterpret_cast<System::String*>(entry)->Serialize(serializer);

            // On load: resolve the entry's description reference (0x14001b1d0).
            // NOT a string hash despite the old `fn_StringHash` name — it is the
            // lazy ExternalPtr resolve already ported as ResolveExternalByName_Impl
            // (engine_funcs.h). Writes entry+0x10 (resolved Description*) and
            // entry+0x08 (factory generation).
            if (encodingMode == 0) {
                ResolveExternalByName_Impl(entry);
            }

            entry += 0x18;
        }
    }

    // Destroy Serializer
    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge

    return true;
}

// ============================================================
// BuildingGoal::Reconstruct [6] 0x140025e30 / Destruct [7] 0x140001640
// ============================================================
// PORT GAP (retail 0x140025e30) — B38 D3.2: retail first runs DeletingDtor(0)
// (`CALL [RAX]` with EDX=0), then the ctor on `this`; the tree's slot 6 built
// over the live object without destroying it. The tree's body is kept here.
Object::External* BuildingGoal::Reconstruct() {
    return ::new (static_cast<void*>(this)) BuildingGoal();
}
void BuildingGoal::Destruct() { DeletingDtor(0); }

// ============================================================
// BuildingGoal::CheckGoalState (vtable +0x40, 0x1400d7580)
// ============================================================
// Iterates the entries, resolves each name, looks the resolved description up
// in the world's building BST (game+0xF0+0x160), counts matches.
//
// Returns:
//   requireAll mode (+0x30 != 0): 1 if ALL types matched, 0 otherwise
//   requireAny mode (+0x30 == 0): 1 if NO types matched, 2 if SOME matched
Achievement::Goal::GoalState BuildingGoal::CheckGoalState() const {
    uintptr_t s = reinterpret_cast<uintptr_t>(this);

    // Get world data
    uintptr_t gameWorld = (uintptr_t)*g_pGameWorldSingleton;
    uintptr_t worldData = *(uintptr_t*)(gameWorld + 0xF0);

    int matchCount = 0;
    int entryCount = *(const int*)(s + 0x40);
    int entryIdx = 0;

    if (entryCount > 0) {
        uintptr_t offset = 0;
        do {
            // Get entry from array
            uintptr_t entry = *(const uintptr_t*)(s + 0x38) + offset;

            // Retail copies the WHOLE 0x18 ExternalPtr triple to the stack, resolves
            // into the COPY, and reads the pointer back out of it — the array entry
            // is deliberately left untouched, so each evaluation re-resolves.
            //
            // The BST key is not a hash: it is the resolved ComponentDescription
            // pointer.
            uint8_t refCopy[0x18];
            // B.3.1 bridge: the ExternalPtr triple is still untyped bytes.
            new (refCopy) System::String(*reinterpret_cast<const System::String*>(entry));  // +0x00 name
            *(uint32_t*)(refCopy + 0x08) = *(uint32_t*)(entry + 0x08);     // +0x08 generation
            *(uint64_t*)(refCopy + 0x10) = *(uint64_t*)(entry + 0x10);     // +0x10 cached ptr
            ResolveExternalByName_Impl(refCopy);
            uint64_t lookupKey = *(uint64_t*)(refCopy + 0x10);
            reinterpret_cast<System::String*>(refCopy)->~String();

            // BST lookup in world building tree at worldData+0x160
            uint64_t* node = *(uint64_t**)(worldData + 0x160);
            while (node != nullptr) {
                uint64_t nodeKey = node[0];
                if (lookupKey < nodeKey) {
                    node = (uint64_t*)node[4];  // left child at +0x20
                } else if (lookupKey > nodeKey) {
                    node = (uint64_t*)node[5];  // right child at +0x28
                } else {
                    // Found matching node — check count at +0x10
                    if (node != nullptr && (int)node[2] > 0) {
                        matchCount++;
                    }
                    break;
                }
            }

            entryIdx++;
            offset += 0x18;
        } while (entryIdx < entryCount);
    }

    // Evaluate result based on requireAll flag
    char requireAll = *(const char*)(s + 0x30);

    if (requireAll != 0) {
        // requireAll mode
        if (matchCount == entryCount) {
            return static_cast<GoalState>(1);  // all matched — goal complete
        }
        return static_cast<GoalState>(0);
    }

    // requireAny mode
    if (matchCount == 0) {
        return static_cast<GoalState>(1);      // none matched
    }
    if (matchCount > 0) {
        return static_cast<GoalState>(2);      // some matched
    }
    return static_cast<GoalState>(0);
}

// ============================================================
// BuildingGoal::Construct (vtable +0x50, 0x140025de0)
// ============================================================
// Placement-new on `this` (retail folded it with createInPlace).
Object::External* BuildingGoal::Construct() {
    return ::new (static_cast<void*>(this)) BuildingGoal();
}

// create 0x140025d80: MemAlloc(0x48, 8), then the (inlined) ctor.
void* BuildingGoal::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(BuildingGoal), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(::new (p) BuildingGoal());
}
// createInPlace 0x140025de0.
void* BuildingGoal::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(::new (storage) BuildingGoal());
}

DEFINE_EXE_VTABLE(BuildingGoal, "_ZTV12BuildingGoal")
DEFINE_EXE_TYPE(BuildingGoal, "Achievement::Goal")

// ============================================================
// BuildingGoal_Probe — diagnostic, env-gated (BANISHED_GOAL_PROBE)
// ============================================================
// The base game ships NO scenarios: `Dialog\Goal.rsc` is the only goal resource
// in data0/data1.pkg, and BuildingGoal only ever gets built by the tutorial or by
// a mod. So nothing in a sandbox autostart run evaluates a BuildingGoal, and any
// claim that this path "works" from a normal run would be a null test
// (reference_null_test_measures_a_world_without_the_feature).
//
// This probe builds a real one-entry BuildingGoal on the stack against a live
// world, runs the actual Slot8 evaluator, and reports every stage: the world
// building BST contents, the resolved description pointer, and the verdict. Set
// BANISHED_GOAL_PROBE to a template name (default "Template\StorageBarn.rsc" —
// the starting town builds one).
extern "C" void BuildingGoal_Probe(const char* templateName) {
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;
    uintptr_t gameWorld = reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton);
    uintptr_t worldData = *reinterpret_cast<uintptr_t*>(gameWorld + 0xF0);
    if (!worldData) return;

    // --- Dump the world building BST (keyed on resolved description pointer) ---
    // Iterative in-order walk with an explicit stack; the tree is small (one node
    // per distinct built building type).
    uint64_t* stack[64];
    int sp = 0, dumped = 0;
    uint64_t* node = *reinterpret_cast<uint64_t**>(worldData + 0x160);
    fprintf(stderr, "[GoalProbe] world building BST @ %p:\n",
            reinterpret_cast<void*>(worldData + 0x160));
    while ((node || sp) && dumped < 64) {
        while (node && sp < 64) { stack[sp++] = node; node = reinterpret_cast<uint64_t*>(node[4]); }
        if (!sp) break;
        node = stack[--sp];
        fprintf(stderr, "[GoalProbe]   key=%016llx count=%d\n",
                static_cast<unsigned long long>(node[0]), static_cast<int>(node[2]));
        dumped++;
        node = reinterpret_cast<uint64_t*>(node[5]);
    }
    if (dumped == 0) fprintf(stderr, "[GoalProbe]   (empty)\n");

    // --- Build a one-entry requireAll BuildingGoal and evaluate it ---
    uint8_t goal[0x48];
    memset(goal, 0, sizeof(goal));
    goal[0x30] = 1;                                    // requireAll
    Goal_RefArray_Resize(goal + 0x38, 1);              // seeds generation -1
    uint8_t* entry = *reinterpret_cast<uint8_t**>(goal + 0x38);
    if (!entry) { fprintf(stderr, "[GoalProbe] resize FAILED (null array)\n"); fflush(stderr); return; }

    fprintf(stderr, "[GoalProbe] seeded entry gen=%d ptr=%p (gen must be -1, else the "
                    "resolver would treat it as cached)\n",
            *reinterpret_cast<int32_t*>(entry + 0x08),
            *reinterpret_cast<void**>(entry + 0x10));

    new (entry) System::String(templateName);          // +0x00 name  (B.3.1 bridge)

    // Resolve a copy for reporting — Slot8 deliberately resolves into a throwaway
    // triple and leaves the array entry alone, so read the pointer from our own copy.
    uint8_t refCopy[0x18];
    memset(refCopy, 0, sizeof(refCopy));
    new (refCopy) System::String(reinterpret_cast<const System::String&>(*entry));
    *reinterpret_cast<uint32_t*>(refCopy + 0x08) = *reinterpret_cast<uint32_t*>(entry + 0x08);
    *reinterpret_cast<uint64_t*>(refCopy + 0x10) = *reinterpret_cast<uint64_t*>(entry + 0x10);
    ResolveExternalByName_Impl(refCopy);
    void* resolved = *reinterpret_cast<void**>(refCopy + 0x10);
    reinterpret_cast<System::String*>(refCopy)->~String();

    *reinterpret_cast<uint64_t*>(goal + 0x40) = 1;     // count = 1
    // B.3.8 bridge: a raw stack image of the class (no vptr), evaluated through
    // a QUALIFIED (non-dispatching) member call.
    int verdict = static_cast<int>(
        reinterpret_cast<const BuildingGoal*>(goal)->BuildingGoal::CheckGoalState());   // B.3.8 bridge

    fprintf(stderr, "[GoalProbe] name=\"%s\" resolved=%p verdict=%d "
                    "(requireAll: 1=met, 0=not met)\n",
            templateName, resolved, verdict);
    if (!resolved) {
        fprintf(stderr, "[GoalProbe]   resolved=NULL — ResolveExternalByName uses find-ONLY "
                        "Find, so the template must already be in the ExternalFactory "
                        "(see StartTown_ResolveTemplate's FindOrLoad fallback).\n");
    }
    fflush(stderr);

    reinterpret_cast<System::String*>(entry)->~String();
    if (fn_MemFreeStatic) fn_MemFreeStatic(entry);
}
