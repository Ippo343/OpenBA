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
//   +0x00: System::String (8 bytes) — building type name
//   +0x08: uint32 — unknown field
//   +0x10: uint64 — building type hash/key for BST lookup
//
// The world building BST is at game+0xF0+0x160.
// BST nodes: +0x00=key (uint64), +0x10=count (int), +0x20=left, +0x28=right
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Goal/buildinggoal_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

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
        // Loading: resize array
        if (fn_BuildingTypeArray_Resize) {
            fn_BuildingTypeArray_Resize((void*)(s + 0x38), countLocal);
        }
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

            // On load: compute hash
            if (encodingMode == 0) {
                if (fn_StringHash) {
                    fn_StringHash(entry);
                }
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

            // Copy string, compute hash, extract key
            uint8_t strBuf[8];
            fn_String_Ctor_Copy(strBuf, (void*)entry);
            // entry+0x10: uint64 hash key
            uint64_t hashKey = *(uint64_t*)(entry + 0x10);

            // Compute string hash (FUN_14001b1d0)
            if (fn_StringHash) {
                fn_StringHash(strBuf);
            }

            // Use the hash key from the entry
            uint64_t lookupKey = hashKey;
            fn_String_Dtor(strBuf);

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
