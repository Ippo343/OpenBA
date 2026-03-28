// code/Game/Entity/citizenbehavior.cpp
// Clean reimplementation of CitizenBehavior (population manager).
//
// CitizenBehavior (0x1C0 bytes) — manages all citizens, worker assignment,
// profession statistics, resource shortage notifications, and path distance graphs.
// Lives at GameUpdateLoop+0xF0 (m_citizenList).
//
// Profile source path: Entity\CitizenBehavior.cpp
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h — portable for standalone exe.

#include "Entity/citizenbehavior_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstring>

// ============================================================
// Internal helper addresses (resolved lazily)
// ============================================================

// System::Time::Time constructor — creates Time from float seconds
typedef void* (*fn_Time_Ctor_t)(void* out, float seconds);
static fn_Time_Ctor_t s_fn_Time_Ctor = nullptr;

// System::Random::Rand(Random*, uint max) -> uint [0, max)
typedef uint32_t (*fn_Random_Rand_t)(void* random, uint32_t max);
static fn_Random_Rand_t s_fn_Random_Rand = nullptr;

static bool s_resolved = false;

static void EnsureResolved() {
    if (s_resolved) return;

    // System::Time::Time(Time*, float) — Runtime export
    s_fn_Time_Ctor = reinterpret_cast<fn_Time_Ctor_t>(
        ResolveExport("?Time@Time@System@@QEAA@M@Z"));
    if (!s_fn_Time_Ctor) {
        // Fallback: resolve from ordinal or known address
        s_fn_Time_Ctor = reinterpret_cast<fn_Time_Ctor_t>(
            ResolveOrdinal(0x031e));
    }

    // System::Random::Rand — Runtime export
    s_fn_Random_Rand = reinterpret_cast<fn_Random_Rand_t>(
        ResolveExport("?Rand@Random@System@@QEAAI@Z"));

    s_resolved = true;
}

// ============================================================
// CitizenBehavior::Ctor (found via GameUpdateLoop::Load)
// ============================================================
// Zeroes all 0x1C0 bytes, initializes profession resource array (21 entries
// with default max counts), stats sub-arrays, and allocates the PathGraph.
//
// Original: TradeComponent_impl_0a30
CitizenBehaviorRaw* CitizenBehavior_Ctor(CitizenBehaviorRaw* self) {
    EnsureResolved();

    // Zero all fields
    memset(self->data, 0, 0x1C0);

    // The decompiled code zeroes param_1[0]..param_1[0x2d] (qwords),
    // then specifically zeros *(uint32_t*)(param_1 + 0x21) and
    // *(uint32_t*)(param_1 + 0x2d), plus param_1[0x2e]..param_1[0x34]
    // and *(uint64_t*)((char*)param_1 + 0x1ac).
    // memset(0, 0x1C0) covers all of this.

    // Initialize profession resource array at +0x170 (ptr to 0x2A0 byte array).
    // fn_ProfResArray_Init allocates 21 entries (0x20 bytes each)
    // and sets default maxCount values.
    int64_t* profResPtr = reinterpret_cast<int64_t*>(self->data + 0x170);
    if (fn_ProfResArray_Init) {
        fn_ProfResArray_Init(profResPtr, 0x15); // 21 professions
    }

    // Set default max counts for each profession
    // These are game balance constants from the constructor:
    void* profArray = self->profResources();
    if (profArray) {
        uint8_t* arr = reinterpret_cast<uint8_t*>(profArray);
        // Profession indices and their default maxCount values:
        //  0: 200   (Laborer)
        //  1: 200   (Builder)
        //  2: 200   (Farmer)
        //  3: 200   (Fisherman)
        //  4:  50   (Hunter)
        //  5: 5000  (Gatherer)
        //  6: 100   (Forester)
        //  7:  50   (Woodcutter)
        //  8: 500   (Blacksmith)
        //  9: 200   (Stonecutter)
        // 10: 200   (Tailor)
        // 11: 1000  (Doctor/Herbalist)
        // 12: 1000  (Teacher)
        // 13: 1000  (Cleric)
        // 14: 1000  (Innkeeper/Brewer)
        // 15: 1000  (Vendor)
        // 16: 1000  (Miner)
        // 17: 1000  (Pastor)
        // 18: 1000  (Trader)
        // 19: 1000  (Herdsman)
        // 20: 1000  (Farmer2/Crop)
        static const int32_t defaultMax[21] = {
            200, 200, 200, 200, 50, 5000, 100, 50, 500,
            200, 200, 1000, 1000, 1000, 1000, 1000, 1000,
            1000, 1000, 1000, 1000
        };
        for (int i = 0; i < 21; i++) {
            *reinterpret_cast<int32_t*>(arr + i * 0x20 + 0x08) = defaultMax[i];
        }
    }

    // Initialize stats tracking arrays
    // FUN_14000f090(param_1 + 0x30, 2) — at offset 0x180 (0x30*8)
    // Actually: param_1 + 0x30 in qword indexing = byte offset 0x180
    // Wait — the decompiled code uses `undefined8 *param_1`, not `longlong *param_1`
    // for the constructor. Let me recheck:
    // param_1 + 0x30 = &param_1[0x30] = base + 0x30*8 = base + 0x180
    // param_1 + 0x2e = &param_1[0x2e] = base + 0x2e*8 = base + 0x170 (profResources)
    // param_1 + 0x2a = &param_1[0x2a] = base + 0x2a*8 = base + 0x150 (statsHistory)

    // FUN_14000f090(base + 0x180, 2) — init notification/summary at +0x180
    if (fn_SummaryArray_Init) {
        fn_SummaryArray_Init(self->data + 0x180, 2);
    }

    // TradeComponent_impl_f760_sub_7640(base + 0x150, 0x47) — init stats tracking
    if (fn_StatsArray_Init) {
        fn_StatsArray_Init(self->data + 0x150, 0x47);
    }

    // *(uint32_t*)(param_1 + 0x35) = 0 — clear notify flags at +0x1A8
    self->notifyFlags() = 0;

    // Allocate PathGraph sub-object (0x28 bytes)
    PathGraph* pg = nullptr;
    if (fn_MemAlloc && g_pMemAllocSingleton) {
        pg = reinterpret_cast<PathGraph*>(
            fn_MemAlloc(g_pMemAllocSingleton, 0x28, 4));
    }
    if (pg) {
        pg->m_pairs = nullptr;
        pg->m_pairCount = 0;
        pg->m_pendingIdx = -1;
        pg->_pad14 = 0;
        pg->m_bstRoot = nullptr;
        pg->m_bstCount = 0;
        pg->_pad24 = 0;
    }
    self->pathGraph() = pg;

    return self;
}

// ============================================================
// CitizenBehavior::Update (0x14008c2b0)
// ============================================================
// Main per-frame tick. Called from GameUpdateLoop::Update.
//
// Logic:
//   1. Advance timers: pathAccumTimer += delta, statsTimer -= delta, notifyTimer -= delta
//   2. Call CountResources (tallies resources per profession)
//   3. Call CheckThresholds (fires shortage notifications)
//   4. If pathAccumTimer >= pathGraphInterval:
//      a. If ProcessPaths returns false: call BuildPathGraph
//      b. Else if pending index < 0 and timer > interval: clamp timer
//   5. If statsTimer <= 0: call CollectStats, reset timer
//   6. If notifyTimer < 0: clear notifyFlags, reset timer to 0
//
// Original: Entity_impl_c2b0
void CitizenBehavior_Update(CitizenBehaviorRaw* self, int64_t* deltaTime) {
    EnsureResolved();

    int64_t* q = reinterpret_cast<int64_t*>(self->data);

    // Advance timers (Ghidra longlong* indexing)
    q[0x32] += *deltaTime;       // pathAccumTimer
    q[0x33] -= *deltaTime;       // statsTimer
    q[0x34] -= *deltaTime;       // notifyTimer

    // CountResources — Entity_helper_9ed0(self)
    if (fn_CitizenBehavior_CountResources) {
        fn_CitizenBehavior_CountResources(q);
    }

    // CheckThresholds — Entity_helper_c100(self, deltaTime)
    if (fn_CitizenBehavior_CheckThresholds) {
        fn_CitizenBehavior_CheckThresholds(self, deltaTime);
    }

    // Path graph processing
    // DAT_140172234 is the path graph interval (float seconds)
    // System::Time::Time constructs a Time value from float
    int64_t pathInterval = 0;
    if (s_fn_Time_Ctor) {
        float* intervalFloat = reinterpret_cast<float*>(
            ResolveAddr(Module::Exe, 0x140172234));
        if (intervalFloat) {
            s_fn_Time_Ctor(&pathInterval, *intervalFloat);
        }
    }

    if (pathInterval <= q[0x32]) {
        // Try to process pending paths
        bool pathsDone = false;
        PathGraph* pg = self->pathGraph();
        if (fn_CitizenBehavior_ProcessPaths && pg) {
            pathsDone = (bool)(uint8_t)fn_CitizenBehavior_ProcessPaths(pg);
        }

        if (!pathsDone) {
            // Build new path graph
            if (fn_CitizenBehavior_BuildPathGraph && pg) {
                fn_CitizenBehavior_BuildPathGraph(reinterpret_cast<int64_t*>(pg));
            }
        } else if (pg && pg->m_pendingIdx < 0 && pathInterval < q[0x32]) {
            // Clamp accumulated time: subtract intervals until within one interval
            int64_t acc = q[0x32];
            do {
                acc -= pathInterval;
            } while (pathInterval < acc);
            q[0x32] = acc;
        }
    }

    // Stats collection (periodic)
    if (q[0x33] < 1) {
        if (fn_CitizenBehavior_CollectStats) {
            fn_CitizenBehavior_CollectStats(self);
        }
        // Reset timer from DAT_1401721fc (float seconds)
        if (s_fn_Time_Ctor) {
            float* statsInterval = reinterpret_cast<float*>(
                ResolveAddr(Module::Exe, 0x1401721fc));
            if (statsInterval) {
                int64_t newTime = 0;
                s_fn_Time_Ctor(&newTime, *statsInterval);
                q[0x33] = newTime;
            }
        }
    }

    // Notification timer reset
    if (q[0x34] < 0) {
        *reinterpret_cast<uint32_t*>(&q[0x35]) = 0; // clear notify flags
        q[0x34] = 0;
    }
}

// ============================================================
// CitizenBehavior::ShuffleSchools (0x1400896a0)
// ============================================================
// Fisher-Yates shuffle on the schools array at +0x20/+0x28.
// Uses the global System::Random at GameUpdateLoop+0xD8.
//
// Original: FUN_1400896a0
void CitizenBehavior_ShuffleSchools(CitizenBehaviorRaw* self) {
    EnsureResolved();

    uint32_t count = *reinterpret_cast<uint32_t*>(self->data + 0x28);
    if (count < 2) return;

    // Get global Random from GameUpdateLoop
    void* random = *reinterpret_cast<void**>(
        reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton) + 0xD8);
    if (!random || !s_fn_Random_Rand) return;

    uint64_t* schoolArray = *reinterpret_cast<uint64_t**>(self->data + 0x20);
    if (!schoolArray) return;

    for (uint32_t i = 0; i < count; i++) {
        uint32_t j = s_fn_Random_Rand(random, count);
        if (i != j) {
            uint64_t tmp = schoolArray[i];
            schoolArray[i] = schoolArray[j];
            schoolArray[j] = tmp;
        }
    }
}

// ============================================================
// CitizenBehavior::ShuffleEntities (0x140089470)
// ============================================================
// Looks up a profession in the BST at +0x100 by profId key,
// then Fisher-Yates shuffles the entity array in that BST node.
//
// BST node layout:
//   +0x00: uint64 key (profession ID)
//   +0x08: void*  entityArray (InternalBasePtr[])
//   +0x10: int32  entityCount
//   +0x20: void*  leftChild
//   +0x28: void*  rightChild
//
// Original: FUN_140089470
void CitizenBehavior_ShuffleEntities(CitizenBehaviorRaw* self, uint64_t profId) {
    EnsureResolved();

    // Walk BST at +0x100
    uint64_t* node = *reinterpret_cast<uint64_t**>(self->data + 0x100);
    if (!node) return;

    while (node) {
        uint64_t key = node[0];
        if (profId < key) {
            node = reinterpret_cast<uint64_t*>(node[4]); // left child at +0x20
        } else if (profId > key) {
            node = reinterpret_cast<uint64_t*>(node[5]); // right child at +0x28
        } else {
            // Found the node
            uint32_t count = static_cast<uint32_t>(node[2]); // entity count at +0x10
            if (count < 2) return;

            void* random = *reinterpret_cast<void**>(
                reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton) + 0xD8);
            if (!random || !s_fn_Random_Rand) return;

            uint64_t* entities = reinterpret_cast<uint64_t*>(node[1]); // array at +0x08
            for (uint32_t i = 0; i < count; i++) {
                uint32_t j = s_fn_Random_Rand(random, count);
                if (i != j) {
                    uint64_t tmp = entities[i];
                    entities[i] = entities[j];
                    entities[j] = tmp;
                }
            }
            return;
        }
    }
}

// ============================================================
// CitizenBehavior::FindAny (0x14008c4b0)
// ============================================================
// Iterates allCitizensPtr (at +0xA0/+0xA8), finds the first citizen
// that is alive (age >= adult threshold), not dead (deathTimestamp == 0),
// and matches the expected profession. Assigns housing via
// CitizenComponent_AssignHousing if the citizen's current workplace/home
// doesn't match. Returns InternalBasePtr to the found citizen.
//
// Citizen checks (from decompiled code):
//   - *(float*)(citizen+0x288->+0x44) <= *(float*)(citizen+0x3c)  [age >= adult]
//   - *(int64*)(citizen+0x208) == 0                                [not dead]
//   - *(int64*)(citizen+0x1f0) == profListEntry+0x30               [profession matches]
//
// Original: CitizenComponent_impl_c4b0
void* CitizenBehavior_FindAny(CitizenBehaviorRaw* self, void* retval,
                               void* profSlot, uint64_t profId) {
    // This function is complex with many CheckPtr calls and component lookups.
    // For now, delegate to the original via the already-resolved function pointer.
    if (fn_CitizenList_FindAny) {
        fn_CitizenList_FindAny(self, retval, profSlot, profId);
        return retval;
    }

    // Fallback: return null InternalBasePtr
    if (fn_InternalBasePtr_Ctor) {
        fn_InternalBasePtr_Ctor(retval, nullptr);
    }
    return retval;
}
