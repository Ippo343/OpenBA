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

#include "Goal/buildinggoal_funcs.h"
#include "Goal/goal_exe_helpers.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstring>

// Vtable pointer for BuildingGoal (resolved at runtime)
static void* s_vtable_BuildingGoal = nullptr;
static bool s_resolved = false;

static void EnsureResolved() {
    if (s_resolved) return;
    s_vtable_BuildingGoal = (void*)ResolveAddr(Module::Exe, 0x140160300);
    s_resolved = true;
}

// ============================================================
// BuildingGoal::Dtor (vtable +0x00, 0x140025e80)
// ============================================================
// Scalar destructor: sets vtable, destroys building type array at +0x38,
// calls Goal::~Goal, optionally frees memory.
//
// Decompiled:
//   *(vtable*)this = &vtable_BuildingGoal;
//   FUN_14001b030(&this->+0x38);  // destroy building type array
//   Goal::~Goal(this);
//   if (flags & 1) MemFree(this);
void* BuildingGoal_Dtor(BuildingGoal* self, uint32_t flags) {
    EnsureResolved();
    uintptr_t s = (uintptr_t)self;

    // Set vtable
    *(void**)self = s_vtable_BuildingGoal;

    // Destroy building type array at +0x38
    if (fn_BuildingTypeArray_Dtor) {
        fn_BuildingTypeArray_Dtor((void*)(s + 0x38));
    }

    // Call Goal base destructor
    if (fn_Goal_BaseDtor) {
        fn_Goal_BaseDtor(self);
    }

    // If bit 0 set, free memory
    if ((flags & 1) != 0) {
        if (fn_MemFreeStatic) {
            fn_MemFreeStatic(self);
        }
    }

    return self;
}

// ============================================================
// BuildingGoal::Slot5 (vtable +0x28, 0x1400d76a0)
// ============================================================
// Serialize/Load: calls Goal::Serialize base, then serializes:
//   1. requireAll (bool at +0x30) via Serialize_Bool
//   2. entry count (int, copy at local then written to +0x40)
//   3. On load (encoding mode 0): resize array via FUN_14001b340
//   4. For each entry: Filename ctor, String::Serialize, hash if loading, Filename dtor
//
// Decompiled from 0x1400d76a0 disassembly.
int BuildingGoal_Slot5(BuildingGoal* self, void* stream) {
    uintptr_t s = (uintptr_t)self;

    // Call Goal base serialize
    if (fn_Goal_BaseSerialize) {
        fn_Goal_BaseSerialize(self, stream);
    }

    // Create Serializer on stack (24 bytes)
    uint8_t serializer[24];
    if (g_fn_Serializer_Ctor) {
        g_fn_Serializer_Ctor(serializer, stream, 0);
    }

    // Serialize requireAll (bool at +0x30)
    if (g_fn_Serialize_Bool) {
        g_fn_Serialize_Bool(serializer, (void*)(s + 0x30), nullptr);
    }

    // Serialize entry count
    int entryCount = *(int*)(s + 0x40);
    uint32_t countLocal = (uint32_t)entryCount;
    if (g_fn_Serialize_Int) {
        g_fn_Serialize_Int(serializer, &countLocal, nullptr);
    }

    // Check if loading (encoding mode == 0 means loading)
    uintptr_t serPtr = *(uintptr_t*)serializer;
    int encodingMode = *(int*)(serPtr + 0x08);
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
            // Filename ctor on stack (16 bytes)
            uint8_t filenameBuf[16];
            if (fn_Filename_Ctor) {
                fn_Filename_Ctor(filenameBuf);
            }

            // String::Serialize — serialize the string at entry+0x00
            if (g_fn_String_Serialize) {
                g_fn_String_Serialize(entry, serializer);
            }

            // On load: resolve the entry's description reference (0x14001b1d0).
            // NOT a string hash despite the old `fn_StringHash` name — it is the
            // lazy ExternalPtr resolve already ported as ResolveExternalByName_Impl
            // (engine_funcs.h), which every other caller in the tree calls inline
            // rather than by address. Writes entry+0x10 (resolved Description*) and
            // entry+0x08 (factory generation).
            if (encodingMode == 0) {
                ResolveExternalByName_Impl(entry);
            }

            // Filename dtor
            if (fn_Filename_Dtor) {
                fn_Filename_Dtor(filenameBuf);
            }

            entry += 0x18;
        }
    }

    // Destroy Serializer
    if (g_fn_Serializer_Dtor) {
        g_fn_Serializer_Dtor(serializer);
    }

    return 1;
}

// ============================================================
// BuildingGoal::Slot6 (vtable +0x30, 0x140025e30)
// ============================================================
// Factory constructor: calls Goal::Goal() base ctor, sets vtable,
// initializes fields: +0x30=1 (requireAll), +0x38=0 (array), +0x40=0 (count).
// Returns null if input is null.
BuildingGoal* BuildingGoal_Slot6(BuildingGoal* self) {
    EnsureResolved();

    if (self == nullptr) {
        return nullptr;
    }

    // Call Goal base default constructor
    if (fn_Goal_BaseCtor) {
        fn_Goal_BaseCtor(self);
    }

    // Set vtable
    *(void**)self = s_vtable_BuildingGoal;

    // Initialize fields
    *(uint8_t*)((uintptr_t)self + 0x30) = 1;   // requireAll = true
    *(uint64_t*)((uintptr_t)self + 0x38) = 0;   // array ptr = null
    *(uint64_t*)((uintptr_t)self + 0x40) = 0;   // count = 0

    return self;
}

// ============================================================
// BuildingGoal::Slot8 (vtable +0x40, 0x1400d75d0)
// ============================================================
// Checks whether the goal's required buildings have been built.
// Iterates entries in the building type array, hashes each name,
// looks up in the world's building BST, counts matches.
//
// Returns:
//   requireAll mode (+0x30 != 0):
//     1 if ALL types matched, 0 otherwise
//   requireAny mode (+0x30 == 0):
//     1 if NO types matched (goal not started)
//     2 if SOME types matched (goal in progress)
//     0 otherwise

int BuildingGoal_Slot8(BuildingGoal* self) {
    uintptr_t s = (uintptr_t)self;

    // Get world data
    uintptr_t gameWorld = (uintptr_t)*g_pGameWorldSingleton;
    uintptr_t worldData = *(uintptr_t*)(gameWorld + 0xF0);

    int matchCount = 0;
    int entryCount = *(int*)(s + 0x40);
    int entryIdx = 0;

    if (entryCount > 0) {
        uintptr_t offset = 0;
        do {
            // Get entry from array
            uintptr_t entry = *(uintptr_t*)(s + 0x38) + offset;

            // Retail copies the WHOLE 0x18 ExternalPtr triple to the stack, resolves
            // into the COPY, and reads the pointer back out of it — the array entry
            // is deliberately left untouched, so each evaluation re-resolves.
            //
            // The previous port passed an 8-byte `strBuf` here, which was wrong twice
            // over: it read the "key" BEFORE resolving (so it always saw the stale
            // value), and the resolver writes at +0x08 and +0x10, i.e. 0x18 bytes into
            // an 8-byte stack buffer — a latent smash of whatever local followed it,
            // armed the moment the resolve stopped being a no-op.
            //
            // The BST key is not a hash: it is the resolved ComponentDescription
            // pointer. While the resolve never ran it stayed 0, so every goal looked
            // up key 0, matched nothing, and could never be met.
            uint8_t refCopy[0x18];
            fn_String_Ctor_Copy(refCopy, (void*)entry);                    // +0x00 name
            *(uint32_t*)(refCopy + 0x08) = *(uint32_t*)(entry + 0x08);     // +0x08 generation
            *(uint64_t*)(refCopy + 0x10) = *(uint64_t*)(entry + 0x10);     // +0x10 cached ptr
            ResolveExternalByName_Impl(refCopy);
            uint64_t lookupKey = *(uint64_t*)(refCopy + 0x10);
            fn_String_Dtor(refCopy);

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
    char requireAll = *(char*)(s + 0x30);

    if (requireAll != 0) {
        // requireAll mode
        if (matchCount == entryCount) {
            return 1;  // all matched — goal complete
        }
        // requireAll is still set, not all matched
        return 0;
    }

    // requireAny mode
    if (matchCount == 0) {
        return 1;  // none matched — goal not started
    }
    if (matchCount > 0) {
        return 2;  // some matched — goal in progress
    }
    return 0;
}

// ============================================================
// BuildingGoal::CreateInstance (vtable +0x50, 0x140025de0)
// ============================================================
// Same as Slot6 — constructs a new BuildingGoal in-place.
BuildingGoal* BuildingGoal_CreateInstance(BuildingGoal* self) {
    return BuildingGoal_Slot6(self);
}


#include "Object/factory_registration.h"
#include "address_registry.h"

// ============================================================
// BuildingGoal vtable (exe addr 0x140160300, 11 slots)
// ============================================================
static uint32_t BuildingGoal_GetTypeId_Std(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("BuildingGoal");
    return cached;
}

static int BuildingGoal_Slot3_Trampoline(BuildingGoal* self) { return BuildingGoal_Slot3(self); }

static void* g_BuildingGoal_Vtable[11] = {
    reinterpret_cast<void*>(&BuildingGoal_Dtor),                // +0x00 dtor
    nullptr,                                                    // +0x08 GetTypeInfo
    reinterpret_cast<void*>(&BuildingGoal_GetTypeId_Std),       // +0x10 GetTypeId
    reinterpret_cast<void*>(&BuildingGoal_Slot3_Trampoline),    // +0x18 Slot3 (returns 2)
    nullptr,                                                    // +0x20 Slot4 (Goal JMP thunk)
    reinterpret_cast<void*>(&BuildingGoal_Slot5),               // +0x28 Slot5 / Load
    reinterpret_cast<void*>(&BuildingGoal_Slot6),               // +0x30 Slot6 / Cleanup
    nullptr,                                                    // +0x38 Slot7 (base stub)
    reinterpret_cast<void*>(&BuildingGoal_Slot8),               // +0x40 Slot8 / Check
    nullptr,                                                    // +0x48 Slot9 (base stub)
    reinterpret_cast<void*>(&BuildingGoal_CreateInstance),      // +0x50 Slot10 / CreateInstance
};

extern "C" void RegisterBuildingGoalVtable() {
    RegisterExeAddr(0x140160300, reinterpret_cast<uintptr_t>(g_BuildingGoal_Vtable));
}

extern "C" void* BuildingGoal_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x48, 8);
    if (!p) return nullptr;
    return BuildingGoal_Slot6(reinterpret_cast<BuildingGoal*>(p));
}

extern "C" void* BuildingGoal_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    return BuildingGoal_Slot6(reinterpret_cast<BuildingGoal*>(memory));
}

REGISTER_TYPE(BuildingGoal, "Achievement::Goal",
              &BuildingGoal_Factory, &BuildingGoal_CreateInPlace);

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
    if (!fn_String_Ctor_CStr || !fn_String_Dtor) return;
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

    fn_String_Ctor_CStr(entry, templateName);          // +0x00 name

    // Resolve a copy for reporting — Slot8 deliberately resolves into a throwaway
    // triple and leaves the array entry alone, so read the pointer from our own copy.
    uint8_t refCopy[0x18];
    memset(refCopy, 0, sizeof(refCopy));
    fn_String_Ctor_Copy(refCopy, entry);
    *reinterpret_cast<uint32_t*>(refCopy + 0x08) = *reinterpret_cast<uint32_t*>(entry + 0x08);
    *reinterpret_cast<uint64_t*>(refCopy + 0x10) = *reinterpret_cast<uint64_t*>(entry + 0x10);
    ResolveExternalByName_Impl(refCopy);
    void* resolved = *reinterpret_cast<void**>(refCopy + 0x10);
    fn_String_Dtor(refCopy);

    *reinterpret_cast<uint64_t*>(goal + 0x40) = 1;     // count = 1
    int verdict = BuildingGoal_Slot8(reinterpret_cast<BuildingGoal*>(goal));

    fprintf(stderr, "[GoalProbe] name=\"%s\" resolved=%p verdict=%d "
                    "(requireAll: 1=met, 0=not met)\n",
            templateName, resolved, verdict);
    if (!resolved) {
        fprintf(stderr, "[GoalProbe]   resolved=NULL — ResolveExternalByName uses find-ONLY "
                        "Find, so the template must already be in the ExternalFactory "
                        "(see StartTown_ResolveTemplate's FindOrLoad fallback).\n");
    }
    fflush(stderr);

    fn_String_Dtor(entry);
    if (fn_MemFreeStatic) fn_MemFreeStatic(entry);
}
