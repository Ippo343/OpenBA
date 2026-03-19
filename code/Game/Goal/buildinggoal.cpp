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
// This file must NOT depend on hooks.h — portable for standalone exe.

#include "Goal/buildinggoal_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

// FUN_14001b1d0 — String hash computation (updates string's hash field)
typedef void (*fn_StringHash_t)(void* str);
static fn_StringHash_t s_fn_StringHash = nullptr;

static bool s_helpersResolved = false;

static void EnsureHelpers() {
    if (s_helpersResolved) return;
    s_fn_StringHash = (fn_StringHash_t)ResolveAddr(Module::Exe, 0x14001b1d0);
    s_helpersResolved = true;
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
    EnsureHelpers();
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
            // entry+0x08: uint32 field (copied to local_38)
            // entry+0x10: uint64 hash key
            uint64_t hashKey = *(uint64_t*)(entry + 0x10);

            // Compute string hash (FUN_14001b1d0)
            if (s_fn_StringHash) {
                s_fn_StringHash(strBuf);
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
