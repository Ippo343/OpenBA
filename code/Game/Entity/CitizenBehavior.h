#pragma once
// ============================================================================
// code/Game/Entity/CitizenBehavior.h — ENDGAME B.4 W0-H1
//
// `CitizenBehavior` (0x1C0): the town's population manager — the lists of
// citizens and of the buildings they use, the per-profession resource counts,
// the stats history and the path-distance graph. No vtable, no base: a plain
// class. Lives at GameUpdateLoop+0xF0 (m_citizenList). Profile source path:
// Entity\CitizenBehavior.cpp.
//
// RETAIL (Application-x64.exe):
//   alloc  GameUpdateLoop::Load 0x140096b00: MemAlloc(0x1c0, 4) + ctor
//   ctor   0x140090a30: zeroes +0x00..+0x1B3 (the two trees +0x100 / +0x160 as
//          {qword root, dword count}); Array SetSize +0x170 to 21 (0x1400177c0)
//          with the per-entry defaults, +0x180 to 2 (0x14000f090), +0x150 to 71
//          (0x140017640); +0x1A8 = 0; +0x1B8 = MemAlloc(0x28, 4) PathGraph
//          {0, 0, -1, {0, 0}}
//   Serialize 0x14008f760 (version 6): each IBP array's count, then its
//          elements through InternalBasePtr::Serialize (0x140152aa0) + an
//          IsDerivedFrom against the cell named on the field; +0x110 / +0x120
//          through the ExternalPtr array body 0x1400049c0; +0x190 / +0x198 /
//          +0x1A0 Time (0x140152aa8); +0x1A8 float (0x140152a60); +0x1AC /
//          +0x1B0 int (0x140152ac8)
//   Update 0x14008c2b0: +0x190 += dt, +0x198 -= dt, +0x1A0 -= dt; +0x1B8 the
//          path graph; +0x198 < 1 -> 0x14008a730; +0x1A0 < 0 -> +0x1A8 = 0
//
// Every `Collection::Array<T>` is declared as its three trivial fields
// {T* data; int32 count; int32 capacity} (H5: the Array type has a destructor
// that no tree code runs on this object today). Element types are the
// serializer's validated cells; an InternalPtr<T> element is the bare `T*` it
// holds. System::Time fields are int64 ticks (see CitizenComponent.h).
//
// The engine/types/CitizenBehavior.h header forwards here and still carries the
// `CitizenBehaviorRaw` accessor mirror its bodies use (H10: it goes with its last
// user).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "collection/Collection.h"   // Collection::Array<int32_t> (the +0x140 buckets)

class ComponentEntity;
class CemeteryComponent;
class HappinessComponent;
class AttendanceComponent;
class HealerComponent;
class LivestockComponent;
class PastureComponent;
class CitizenComponent;
class HerdComponent;
class PopulationComponent;
class BuildComponent;
class WellComponent;
class DiseaseComponent;
class ComponentDescription;
class Map;
namespace Object { template <class T> class InternalPtr; }

// ============================================================
// ProfessionResourceEntry — one element of the +0x170 array (stride 0x20).
// ============================================================
// The ctor sets +0x08 of the 21 entries (200, 200, 200, 200, 50, 5000, 100, 50,
// 500, 200, 200, then 1000 x10). The serializer writes five ints (+0x00, +0x04,
// +0x08, +0x0C, +0x10) and the Time at +0x18; +0x14 is not serialized.
struct ProfessionResourceEntry {
    int32_t  currentCount;    // +0x00
    int32_t  previousCount;   // +0x04
    int32_t  maxCount;        // +0x08: set in ctor (200, 50, 5000, etc.)
    int32_t  field_0x0C;      // +0x0C: int, serialized
    int32_t  _field10;        // +0x10
    int32_t  _field14;        // +0x14: not serialized
    int64_t  timer;           // +0x18: System::Time
};
static_assert(sizeof(ProfessionResourceEntry) == 0x20, "ProfessionResourceEntry size");
static_assert(offsetof(ProfessionResourceEntry, currentCount)  == 0x00, "");
static_assert(offsetof(ProfessionResourceEntry, previousCount) == 0x04, "");
static_assert(offsetof(ProfessionResourceEntry, maxCount)      == 0x08, "");
static_assert(offsetof(ProfessionResourceEntry, field_0x0C)    == 0x0C, "");
static_assert(offsetof(ProfessionResourceEntry, _field10)      == 0x10, "");
static_assert(offsetof(ProfessionResourceEntry, _field14)      == 0x14, "");
static_assert(offsetof(ProfessionResourceEntry, timer)         == 0x18, "");

// ============================================================
// ResourceNotifyEntry — one element of the +0x180 array (stride 0x10, 2 of them).
// ============================================================
// Serialized as int, int, Time.
struct ResourceNotifyEntry {
    int32_t  current;         // +0x00: current count
    int32_t  maxCapacity;     // +0x04: max capacity (5% threshold = max/20)
    int64_t  timer;           // +0x08: System::Time, countdown to the next notification
};
static_assert(sizeof(ResourceNotifyEntry) == 0x10, "ResourceNotifyEntry size");
static_assert(offsetof(ResourceNotifyEntry, current)     == 0x00, "");
static_assert(offsetof(ResourceNotifyEntry, maxCapacity) == 0x04, "");
static_assert(offsetof(ResourceNotifyEntry, timer)       == 0x08, "");

// ============================================================
// PopulationSummary — a 0x18-byte stats record (tree-side; not a retail field
// of CitizenBehavior).
// ============================================================
struct PopulationSummary {
    int32_t  totalWorkers;     // +0x00: total workers (alive + not dead)
    int32_t  totalWorkersIncl; // +0x04: total including dead
    int32_t  _field08;         // +0x08
    int32_t  _field0C;         // +0x0C
    int32_t  deadCount;        // +0x10: dead citizens this frame
    int32_t  educatedCount;    // +0x14: educated workers count
};
static_assert(sizeof(PopulationSummary) == 0x18, "PopulationSummary size");
static_assert(offsetof(PopulationSummary, totalWorkers)     == 0x00, "");
static_assert(offsetof(PopulationSummary, totalWorkersIncl) == 0x04, "");
static_assert(offsetof(PopulationSummary, _field08)         == 0x08, "");
static_assert(offsetof(PopulationSummary, _field0C)         == 0x0C, "");
static_assert(offsetof(PopulationSummary, deadCount)        == 0x10, "");
static_assert(offsetof(PopulationSummary, educatedCount)    == 0x14, "");

// ============================================================
// PathGraph — the path-distance sub-object at +0x1B8 (MemAlloc(0x28, 4) in the ctor)
// ============================================================
// ctor 0x140090ce9: +0x00 = 0, +0x08 = 0 (qword: count and capacity), +0x10 = -1,
// +0x18 = 0 (qword), +0x20 = 0 (dword). Update 0x14008c2b0 reads +0x10 as int.
struct PathGraph {
    void*    m_pairs;         // +0x00: Array data: {int32, int32} grid coordinate pairs
    int32_t  m_pairCount;     // +0x08: Array count
    int32_t  m_pairCapacity;  // +0x0C: Array capacity
    int32_t  m_pendingIdx;    // +0x10: current processing index (-1 = done, >= 0 = in progress)
    int32_t  _pad14;          // +0x14
    void*    m_bstRoot;       // +0x18: tree root: the distance cache {(high,low) -> distance}
    int32_t  m_bstCount;      // +0x20: tree count
    int32_t  _pad24;          // +0x24

    // ---- members (B4 WORK-a; no retail symbol, the tree's names; bodies in
    // citizenbehavior.cpp) ----
    bool ProcessPaths();      // 0x1400d6f00 up to 7 pending pair costs per call
    void BuildPathGraph();    // 0x1400d70c0 rebuild the pair list when idle
};
static_assert(sizeof(PathGraph) == 0x28, "PathGraph size");
static_assert(offsetof(PathGraph, m_pairs)        == 0x00, "");
static_assert(offsetof(PathGraph, m_pairCount)    == 0x08, "");
static_assert(offsetof(PathGraph, m_pairCapacity) == 0x0C, "");
static_assert(offsetof(PathGraph, m_pendingIdx)   == 0x10, "");
static_assert(offsetof(PathGraph, _pad14)         == 0x14, "");
static_assert(offsetof(PathGraph, m_bstRoot)      == 0x18, "");
static_assert(offsetof(PathGraph, m_bstCount)     == 0x20, "");
static_assert(offsetof(PathGraph, _pad24)         == 0x24, "");

// ============================================================
// CitizenBehavior — 0x1C0 bytes
// ============================================================
class CitizenBehavior {
public:
    // +0x00..+0xFF: sixteen Collection::Array<InternalPtr<T>> (0x10 each), T = the
    // cell the serializer validates each element against.
    ComponentEntity** field_0x00;          // +0x00 (cell 0x1401c5d70 ComponentEntity)
    int32_t  field_0x08;                   // +0x08 count
    int32_t  field_0x0C;                   // +0x0C capacity
    ComponentEntity** field_0x10;          // +0x10 (cell 0x1401c5d70)
    int32_t  field_0x18;                   // +0x18 count
    int32_t  field_0x1C;                   // +0x1C capacity
    ComponentEntity** field_0x20;          // +0x20 (cell 0x1401c5d70)
    int32_t  field_0x28;                   // +0x28 count
    int32_t  field_0x2C;                   // +0x2C capacity
    ComponentEntity** field_0x30;          // +0x30 (cell 0x1401c5d70)
    int32_t  field_0x38;                   // +0x38 count
    int32_t  field_0x3C;                   // +0x3C capacity
    CemeteryComponent** cemeteries;        // +0x40 (cell 0x1401c4090)
    int32_t  cemeteryCount;                // +0x48
    int32_t  cemeteryCapacity;             // +0x4C
    HappinessComponent** happinessComponents; // +0x50 (cell 0x1401c42c0)
    int32_t  happinessComponentCount;      // +0x58
    int32_t  happinessComponentCapacity;   // +0x5C
    AttendanceComponent** attendanceComponents; // +0x60 (cell 0x1401c3f40)
    int32_t  attendanceComponentCount;     // +0x68
    int32_t  attendanceComponentCapacity;  // +0x6C
    HealerComponent** healers;             // +0x70 (cell 0x1401c4330)
    int32_t  healerCount;                  // +0x78
    int32_t  healerCapacity;               // +0x7C
    LivestockComponent** livestock;        // +0x80 (cell 0x1401c7970)
    int32_t  livestockCount;               // +0x88
    int32_t  livestockCapacity;            // +0x8C
    PastureComponent** pastures;           // +0x90 (cell 0x1401c7ba0)
    int32_t  pastureCount;                 // +0x98
    int32_t  pastureCapacity;              // +0x9C
    CitizenComponent** citizens;           // +0xA0 (cell 0x1401c7158)
    int32_t  citizenCount;                 // +0xA8
    int32_t  citizenCapacity;              // +0xAC
    HerdComponent** herds;                 // +0xB0 (cell 0x1401c7820)
    int32_t  herdCount;                    // +0xB8
    int32_t  herdCapacity;                 // +0xBC
    PopulationComponent** populations;     // +0xC0 (cell 0x1401c4790)
    int32_t  populationCount;              // +0xC8
    int32_t  populationCapacity;           // +0xCC
    BuildComponent** buildComponents;      // +0xD0 (cell 0x1401c7008)
    int32_t  buildComponentCount;          // +0xD8
    int32_t  buildComponentCapacity;       // +0xDC
    WellComponent** wells;                 // +0xE0 (cell 0x1401c4c28)
    int32_t  wellCount;                    // +0xE8
    int32_t  wellCapacity;                 // +0xEC
    DiseaseComponent** diseases;           // +0xF0 (cell 0x1401c4170; serialized at version > 0)
    int32_t  diseaseCount;                 // +0xF8
    int32_t  diseaseCapacity;              // +0xFC

    void*    field_0x100;                  // +0x100 tree root (0x38-byte nodes; the key is an
                                           //        ExternalPtr<Profession>, serialized by 0x140018020)
    int32_t  field_0x108;                  // +0x108 tree count (the ctor's dword store)
    uint8_t  _pad10C[4];                   // +0x10C
    ComponentDescription** field_0x110;    // +0x110 Array<ExternalPtr<ComponentDescription>> data
    int32_t  field_0x118;                  // +0x118 count
    int32_t  field_0x11C;                  // +0x11C capacity
    ComponentDescription** field_0x120;    // +0x120 Array<ExternalPtr<ComponentDescription>> data
    int32_t  field_0x128;                  // +0x128 count
    int32_t  field_0x12C;                  // +0x12C capacity
    int32_t* field_0x130;                  // +0x130 Array of {int32, int32} pairs (0x140017f60)
    int32_t  field_0x138;                  // +0x138 count
    int32_t  field_0x13C;                  // +0x13C capacity
    Collection::Array<int32_t>* field_0x140; // +0x140 Array of 0x10-byte elements, each an inner int Array
                                           //        (0x140017640; version > 4): the COARSE water
                                           //        index, (W>>3)^2 buckets of water ids (< 0: a
                                           //        well, -1-id into +0xE0; >= 0: a +0x130 pair)
    int32_t  field_0x148;                  // +0x148 count
    int32_t  field_0x14C;                  // +0x14C capacity
    void*    field_0x150;                  // +0x150 Array of 0x10-byte elements, each an inner int Array;
                                           //        the ctor sizes it to 71 (0x140017640)
    int32_t  field_0x158;                  // +0x158 count
    int32_t  field_0x15C;                  // +0x15C capacity
    void*    field_0x160;                  // +0x160 tree root (serialized by 0x140017cd0)
    int32_t  field_0x168;                  // +0x168 tree count (the ctor's dword store)
    uint8_t  _pad16C[4];                   // +0x16C
    ProfessionResourceEntry* professionResources; // +0x170 Array data; the ctor sizes it to 21
    int32_t  professionResourceCount;      // +0x178
    int32_t  professionResourceCapacity;   // +0x17C
    ResourceNotifyEntry* notifyEntries;    // +0x180 Array data; the ctor sizes it to 2
    int32_t  notifyEntryCount;             // +0x188
    int32_t  notifyEntryCapacity;          // +0x18C
    int64_t  field_0x190;                  // +0x190 System::Time: += dt; drives the path graph (Update)
    int64_t  field_0x198;                  // +0x198 System::Time: -= dt; < 1 -> 0x14008a730 (Update)
    int64_t  field_0x1A0;                  // +0x1A0 System::Time: -= dt; < 0 -> +0x1A8 = 0 (Update)
    float    field_0x1A8;                  // +0x1A8 float (serialized by the float overload; Update
                                           //        stores dword 0)
    int32_t  field_0x1AC;                  // +0x1AC int (version > 1)
    int32_t  field_0x1B0;                  // +0x1B0 int (version > 2)
    uint8_t  _pad1B4[4];                   // +0x1B4
    PathGraph* pathGraph;                  // +0x1B8 MemAlloc(0x28, 4) in the ctor

    // ---- members (B4 WORK-a; bodies in citizenbehavior.cpp) ----
    void Update(int64_t* deltaTime);       // 0x14008c2b0 (deltaTime: a System::Time*)
    void CountResources();                 // 0x140089ed0
    void CheckThresholds(int64_t* deltaTime); // 0x14008c100
    void CollectStats();                   // 0x14008a730
    // The water search FireWork's Execute (slot 4) runs from the citizen's cell:
    // an expanding ring over the +0x140 coarse index, closest reachable well or
    // water pair wins; writes two terrain positions, returns 1 when it did.
    int64_t FindWater(uint32_t centreCell, int extent, float* outA, float* outB);  // 0x1400881d0
    void ScanWaterCell(Map* map, int coarseCell, int ringX, int ringY, uint32_t srcCell,
                       int* bestCost, int* bestId, WellComponent** bestWell);    // 0x140088730

    // ---- the water-source register (B4 COMP-f; bodies in Component/wellcomponent.cpp) ----
    void RemoveCellEntry(int x, int y, int value);                    // 0x1400889e0
    void AddCellEntry(int x, int y, int value);                       // 0x140088a80
    void UnregisterWell(Object::InternalPtr<WellComponent>* well);    // 0x140091ba0
    void RegisterWell(Object::InternalPtr<WellComponent>* well);      // 0x140091cb0
};

static_assert(sizeof(CitizenBehavior) == 0x1C0, "CitizenBehavior is 0x1C0 (MemAlloc in 0x140096b00)");
static_assert(alignof(CitizenBehavior) == 8, "CitizenBehavior alignment");
static_assert(offsetof(CitizenBehavior, field_0x00) == 0x00, "");
static_assert(offsetof(CitizenBehavior, field_0x08) == 0x08, "");
static_assert(offsetof(CitizenBehavior, field_0x0C) == 0x0C, "");
static_assert(offsetof(CitizenBehavior, field_0x10) == 0x10, "");
static_assert(offsetof(CitizenBehavior, field_0x18) == 0x18, "");
static_assert(offsetof(CitizenBehavior, field_0x1C) == 0x1C, "");
static_assert(offsetof(CitizenBehavior, field_0x20) == 0x20, "");
static_assert(offsetof(CitizenBehavior, field_0x28) == 0x28, "");
static_assert(offsetof(CitizenBehavior, field_0x2C) == 0x2C, "");
static_assert(offsetof(CitizenBehavior, field_0x30) == 0x30, "");
static_assert(offsetof(CitizenBehavior, field_0x38) == 0x38, "");
static_assert(offsetof(CitizenBehavior, field_0x3C) == 0x3C, "");
static_assert(offsetof(CitizenBehavior, cemeteries) == 0x40, "");
static_assert(offsetof(CitizenBehavior, cemeteryCount) == 0x48, "");
static_assert(offsetof(CitizenBehavior, cemeteryCapacity) == 0x4C, "");
static_assert(offsetof(CitizenBehavior, happinessComponents) == 0x50, "");
static_assert(offsetof(CitizenBehavior, happinessComponentCount) == 0x58, "");
static_assert(offsetof(CitizenBehavior, happinessComponentCapacity) == 0x5C, "");
static_assert(offsetof(CitizenBehavior, attendanceComponents) == 0x60, "");
static_assert(offsetof(CitizenBehavior, attendanceComponentCount) == 0x68, "");
static_assert(offsetof(CitizenBehavior, attendanceComponentCapacity) == 0x6C, "");
static_assert(offsetof(CitizenBehavior, healers) == 0x70, "");
static_assert(offsetof(CitizenBehavior, healerCount) == 0x78, "");
static_assert(offsetof(CitizenBehavior, healerCapacity) == 0x7C, "");
static_assert(offsetof(CitizenBehavior, livestock) == 0x80, "");
static_assert(offsetof(CitizenBehavior, livestockCount) == 0x88, "");
static_assert(offsetof(CitizenBehavior, livestockCapacity) == 0x8C, "");
static_assert(offsetof(CitizenBehavior, pastures) == 0x90, "");
static_assert(offsetof(CitizenBehavior, pastureCount) == 0x98, "");
static_assert(offsetof(CitizenBehavior, pastureCapacity) == 0x9C, "");
static_assert(offsetof(CitizenBehavior, citizens) == 0xA0, "");
static_assert(offsetof(CitizenBehavior, citizenCount) == 0xA8, "");
static_assert(offsetof(CitizenBehavior, citizenCapacity) == 0xAC, "");
static_assert(offsetof(CitizenBehavior, herds) == 0xB0, "");
static_assert(offsetof(CitizenBehavior, herdCount) == 0xB8, "");
static_assert(offsetof(CitizenBehavior, herdCapacity) == 0xBC, "");
static_assert(offsetof(CitizenBehavior, populations) == 0xC0, "");
static_assert(offsetof(CitizenBehavior, populationCount) == 0xC8, "");
static_assert(offsetof(CitizenBehavior, populationCapacity) == 0xCC, "");
static_assert(offsetof(CitizenBehavior, buildComponents) == 0xD0, "");
static_assert(offsetof(CitizenBehavior, buildComponentCount) == 0xD8, "");
static_assert(offsetof(CitizenBehavior, buildComponentCapacity) == 0xDC, "");
static_assert(offsetof(CitizenBehavior, wells) == 0xE0, "");
static_assert(offsetof(CitizenBehavior, wellCount) == 0xE8, "");
static_assert(offsetof(CitizenBehavior, wellCapacity) == 0xEC, "");
static_assert(offsetof(CitizenBehavior, diseases) == 0xF0, "");
static_assert(offsetof(CitizenBehavior, diseaseCount) == 0xF8, "");
static_assert(offsetof(CitizenBehavior, diseaseCapacity) == 0xFC, "");
static_assert(offsetof(CitizenBehavior, field_0x100) == 0x100, "");
static_assert(offsetof(CitizenBehavior, field_0x108) == 0x108, "");
static_assert(offsetof(CitizenBehavior, _pad10C) == 0x10C, "");
static_assert(offsetof(CitizenBehavior, field_0x110) == 0x110, "");
static_assert(offsetof(CitizenBehavior, field_0x118) == 0x118, "");
static_assert(offsetof(CitizenBehavior, field_0x11C) == 0x11C, "");
static_assert(offsetof(CitizenBehavior, field_0x120) == 0x120, "");
static_assert(offsetof(CitizenBehavior, field_0x128) == 0x128, "");
static_assert(offsetof(CitizenBehavior, field_0x12C) == 0x12C, "");
static_assert(offsetof(CitizenBehavior, field_0x130) == 0x130, "");
static_assert(offsetof(CitizenBehavior, field_0x138) == 0x138, "");
static_assert(offsetof(CitizenBehavior, field_0x13C) == 0x13C, "");
static_assert(offsetof(CitizenBehavior, field_0x140) == 0x140, "");
static_assert(sizeof(Collection::Array<int32_t>) == 0x10, "+0x140 bucket stride 0x10 (0x140088730)");
static_assert(offsetof(CitizenBehavior, field_0x148) == 0x148, "");
static_assert(offsetof(CitizenBehavior, field_0x14C) == 0x14C, "");
static_assert(offsetof(CitizenBehavior, field_0x150) == 0x150, "");
static_assert(offsetof(CitizenBehavior, field_0x158) == 0x158, "");
static_assert(offsetof(CitizenBehavior, field_0x15C) == 0x15C, "");
static_assert(offsetof(CitizenBehavior, field_0x160) == 0x160, "");
static_assert(offsetof(CitizenBehavior, field_0x168) == 0x168, "");
static_assert(offsetof(CitizenBehavior, _pad16C) == 0x16C, "");
static_assert(offsetof(CitizenBehavior, professionResources) == 0x170, "");
static_assert(offsetof(CitizenBehavior, professionResourceCount) == 0x178, "");
static_assert(offsetof(CitizenBehavior, professionResourceCapacity) == 0x17C, "");
static_assert(offsetof(CitizenBehavior, notifyEntries) == 0x180, "");
static_assert(offsetof(CitizenBehavior, notifyEntryCount) == 0x188, "");
static_assert(offsetof(CitizenBehavior, notifyEntryCapacity) == 0x18C, "");
static_assert(offsetof(CitizenBehavior, field_0x190) == 0x190, "");
static_assert(offsetof(CitizenBehavior, field_0x198) == 0x198, "");
static_assert(offsetof(CitizenBehavior, field_0x1A0) == 0x1A0, "");
static_assert(offsetof(CitizenBehavior, field_0x1A8) == 0x1A8, "");
static_assert(offsetof(CitizenBehavior, field_0x1AC) == 0x1AC, "");
static_assert(offsetof(CitizenBehavior, field_0x1B0) == 0x1B0, "");
static_assert(offsetof(CitizenBehavior, _pad1B4) == 0x1B4, "");
static_assert(offsetof(CitizenBehavior, pathGraph) == 0x1B8, "");
