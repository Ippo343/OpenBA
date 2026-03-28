#pragma once
// CitizenBehavior — population manager / citizen list coordinator.
// Application-x64.exe, constructor at TradeComponent_impl_0a30 (found via Load 0x140096B00).
// Struct size: 0x1C0 bytes. No vtable (plain data struct).
//
// Lives at GameUpdateLoop+0xF0 (m_citizenList). Manages all citizen entities,
// profession assignment, housing search, resource shortage notifications,
// and per-frame stat collection for the population overview UI.
//
// Profile source path: Entity\CitizenBehavior.cpp
//
// Key methods (all in exe, "Entity_impl_" or "CitizenComponent_impl_" prefixes in Ghidra):
//   Update          0x14008c2b0 — main per-frame tick
//   CollectStats    0x14008a730 — collect population statistics and push to history
//   CountResources  0x140089ed0 — count resources across all buildings for each profession
//   CheckThresholds 0x14008c100 — check resource shortage thresholds, fire notifications
//   FindIdle        0x14008cbf0 — find idle citizen near a building (for worker assignment)
//   FindWorking     0x14008ca20 — find working citizen nearest to building
//   FindStudent     0x14008c5f0 — find student citizen (school with most students)
//   FindAny         0x14008c4b0 — find any available citizen
//   ShuffleEntities 0x140089470 — shuffle entity list for a profession BST node
//   ShuffleSchools  0x1400896a0 — shuffle the schools array
//   RequestWorker   0x14008d100 — add worker request to profession
//   AssignWorker    0x14008c3d0 — bind worker slot to profession
//   RemoveWorker    0x140091180 — remove worker from profession
//   BuildPathGraph  0x1400d70c0 — build/rebuild the path distance graph
//   ProcessPaths    0x1400d6f00 — process pending path calculations
//
// The 0x28-byte sub-object at +0x1B8 (PathGraph):
//   +0x00: void*   m_pairs       — array of {int,int} pairs (from/to grid IDs)
//   +0x08: int64   m_pairCount   — number of pairs
//   +0x10: int32   m_pendingIdx  — current processing index (-1 when done)
//   +0x18: void*   m_bstRoot     — BST of computed distances (key={high,low}, value=dist)
//   +0x20: int32   m_bstCount    — number of BST entries

#include <cstdint>
#include <cstddef>

// ============================================================
// ProfessionResourceEntry — per-profession resource tracking
// ============================================================
// Array of 0x20-byte entries, 0x15 (21) professions.
// Total array size: 0x15 * 0x20 = 0x2A0 bytes.
// Layout per entry:
//   +0x00: int32 currentCount   — current resource count
//   +0x04: int32 previousCount  — previous tick's count (for delta)
//   +0x08: int32 maxCount       — max/target count (set in ctor)
//   +0x0C: int32 _pad0C
//   +0x10: int32 _field10       — per-tick delta accumulator
//   +0x14: int32 _field14       — cleared each frame
//   +0x18: int64 timer          — countdown timer for threshold checks
struct ProfessionResourceEntry {
    int32_t  currentCount;    // +0x00
    int32_t  previousCount;   // +0x04
    int32_t  maxCount;        // +0x08: set in ctor (200, 50, 5000, etc.)
    int32_t  _pad0C;          // +0x0C
    int32_t  _field10;        // +0x10
    int32_t  _field14;        // +0x14
    int64_t  timer;           // +0x18
};
static_assert(sizeof(ProfessionResourceEntry) == 0x20, "ProfessionResourceEntry size");

// ============================================================
// ResourceNotifyEntry — resource shortage notification state
// ============================================================
// Array of 2 entries at CitizenBehavior+0x180, each 0x10 bytes.
struct ResourceNotifyEntry {
    int32_t  current;         // +0x00: current count
    int32_t  maxCapacity;     // +0x04: max capacity (5% threshold = max/20)
    int64_t  timer;           // +0x08: countdown to next notification
};
static_assert(sizeof(ResourceNotifyEntry) == 0x10, "ResourceNotifyEntry size");

// ============================================================
// PopulationSummary — summary stats at CitizenBehavior+0x180 area
// ============================================================
// The 0x18-byte struct at the pointer stored at +0x180.
// Fields: totalPop, alivePop, dead (adults+children), educated count, etc.
struct PopulationSummary {
    int32_t  totalWorkers;     // +0x00: total workers (alive + not dead)
    int32_t  totalWorkersIncl; // +0x04: total including dead
    int32_t  _field08;         // +0x08
    int32_t  _field0C;         // +0x0C
    int32_t  deadCount;        // +0x10: dead citizens this frame
    int32_t  educatedCount;    // +0x14: educated workers count
};
static_assert(sizeof(PopulationSummary) == 0x18, "PopulationSummary size");

// ============================================================
// PathGraph — path distance computation sub-object (0x28 bytes)
// ============================================================
// Allocated separately (MemAlloc 0x28) and stored at CitizenBehavior+0x1B8.
struct PathGraph {
    void*    m_pairs;         // +0x00: array of {int32, int32} grid coordinate pairs
    int64_t  m_pairCount;     // +0x08: number of pairs in the array
    int32_t  m_pendingIdx;    // +0x10: current processing index (-1 = done, >=0 = in progress)
    int32_t  _pad14;          // +0x14
    void*    m_bstRoot;       // +0x18: BST root for distance cache {(high,low)->distance}
    int32_t  m_bstCount;      // +0x20: entries in BST
    int32_t  _pad24;          // +0x24
};
static_assert(sizeof(PathGraph) == 0x28, "PathGraph size");

// ============================================================
// CitizenBehavior — 0x1C0 bytes, the population manager
// ============================================================
struct CitizenBehavior {
    // --- Entity arrays (citizens tracked by the system) ---
    void*    m_citizenArray;        // +0x00: InternalBasePtr[] — all citizen entities
    int32_t  m_citizenCount;        // +0x08: number of citizens
    int32_t  m_citizenCapacity;     // +0x0C: allocated capacity

    void*    m_buildingArray;       // +0x10: InternalBasePtr[] — tracked buildings
    int32_t  m_buildingCount;       // +0x18: number of buildings
    int32_t  m_buildingCapacity;    // +0x1C: allocated capacity

    // --- Schools/education tracking ---
    void*    m_schoolArray;         // +0x20: InternalBasePtr[] — school entities
    int32_t  m_schoolCount;         // +0x28: number of schools
    int32_t  m_schoolCapacity;      // +0x2C: allocated capacity

    uint8_t  _pad30[0x70];          // +0x30: additional entity/data arrays (profession slots,
                                    //        worker request queues, etc.)

    // --- Citizen entity list (param_1+0xa0/+0xa8 in decompiled code) ---
    void*    m_allCitizensPtr;      // +0xA0: InternalBasePtr[] — full citizen IBP array
    int32_t  m_allCitizensCount;    // +0xA8: count of citizens in full array
    int32_t  _padAC;               // +0xAC

    uint8_t  _padB0[0x50];         // +0xB0: more internal data

    // --- Profession BST ---
    void*    m_professionBST;       // +0x100: BST root — profession ID -> entity list node
                                    //   Node layout: {uint64 key, void* entityArray, int count, ...}

    uint8_t  _pad108[0x48];        // +0x108

    // --- Statistics/history ---
    void*    m_statsHistory;        // +0x150: pointer to stat tracking object (multi-array)

    uint8_t  _pad158[0x18];        // +0x158

    // --- Profession resource entries ---
    void*    m_professionResources; // +0x170: ProfessionResourceEntry[21] array (0x2A0 bytes)
                                    //   Indexed by profession type. Tracks current/prev/max counts.

    uint8_t  _pad178[0x08];        // +0x178

    // --- Resource notification thresholds ---
    ResourceNotifyEntry m_notifyEntries[2]; // +0x180: food/firewood shortage tracking (2 * 0x10)

    // --- Timers ---
    int64_t  m_accumTimer;          // +0x1A0: accumulated time for path graph updates
                                    //   (referred to as param_1[0x32] with longlong* indexing,
                                    //    but 0x32*8 = 0x190 — Ghidra uses longlong* param_1)
    // NOTE: Ghidra decompilation uses longlong* indexing, so:
    //   param_1[0x32] = offset 0x32*8 = 0x190
    //   param_1[0x33] = offset 0x33*8 = 0x198
    //   param_1[0x34] = offset 0x34*8 = 0x1A0
    //   param_1[0x35] = offset 0x35*8 = 0x1A8
    //   param_1[0x37] = offset 0x37*8 = 0x1B8

    // Corrected layout based on longlong* indexing:
    // +0x190: m_pathAccumTimer (param_1[0x32])
    // +0x198: m_statsTimer     (param_1[0x33])
    // +0x1A0: m_notifyTimer    (param_1[0x34])
    // +0x1A8: m_notifyFlags    (param_1[0x35], lower 32 bits)
    // +0x1AC: padding
    // +0x1B0: unused
    // +0x1B8: m_pathGraph      (param_1[0x37])

    uint8_t  _pad1A0_raw[0x20];     // +0x1A0: see comment above for detailed field layout

    // Using accessors to match Ghidra's longlong* indexing
    // The struct is accessed as longlong* in the decompiled code

    // Let's define the end fields precisely:
};

// The full struct as accessed by the game code via longlong* indexing.
// Since fields are accessed as param_1[0x32] etc. (longlong* offsets),
// we define using a raw byte array with accessor methods.
struct CitizenBehaviorRaw {
    uint8_t data[0x1C0];

    // Accessors matching Ghidra's longlong* param_1 indexing
    int64_t& qword(int idx)       { return *reinterpret_cast<int64_t*>(data + idx * 8); }
    int32_t& dword_at(int offset) { return *reinterpret_cast<int32_t*>(data + offset); }
    void*&   ptr_at(int offset)   { return *reinterpret_cast<void**>(data + offset); }

    // Named accessors for key fields (from constructor and Update analysis)
    void*&   citizenArray()       { return ptr_at(0x00); }
    int32_t& citizenCount()       { return dword_at(0x08); }

    void*&   buildingArray()      { return ptr_at(0x10); }
    int32_t& buildingCount()      { return dword_at(0x18); }

    void*&   schoolArray()        { return ptr_at(0x20); }
    int32_t& schoolCount()        { return dword_at(0x28); }

    void*&   allCitizensPtr()     { return ptr_at(0xA0); }
    int32_t& allCitizensCount()   { return dword_at(0xA8); }

    void*&   professionBST()      { return ptr_at(0x100); }

    void*&   statsHistory()       { return ptr_at(0x150); }

    void*&   profResources()      { return ptr_at(0x170); } // ProfessionResourceEntry[21]

    // Resource notification entries at +0x180
    ResourceNotifyEntry* notifyEntries() {
        return reinterpret_cast<ResourceNotifyEntry*>(data + 0x180);
    }

    // Timers (from Update: param_1[0x32..0x35])
    int64_t& pathAccumTimer()     { return qword(0x32); } // +0x190
    int64_t& statsTimer()         { return qword(0x33); } // +0x198
    int64_t& notifyTimer()        { return qword(0x34); } // +0x1A0
    int32_t& notifyFlags()        { return dword_at(0x1A8); } // param_1[0x35] low dword

    // Path graph sub-object
    PathGraph*& pathGraph()       { return *reinterpret_cast<PathGraph**>(data + 0x1B8); }
                                   // param_1[0x37]
};
static_assert(sizeof(CitizenBehaviorRaw) == 0x1C0, "CitizenBehaviorRaw must be 0x1C0 bytes");
