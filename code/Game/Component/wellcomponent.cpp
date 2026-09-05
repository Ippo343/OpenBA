// code/Game/Component/wellcomponent.cpp
// Clean reimplementation of WellComponent lifecycle methods from Application-x64.exe.
//
// WellComponent (0x48 bytes) extends ControllerComponent. No additional fields.
// Vtable at 0x14015ae90, read from the BYTES:
//   [ 3] 0x14005cfa0  Shutdown/Serialize — two empty serializer blocks
//   [ 7] 0x14005cf60  OnRemove           — UNregister the water source
//   [16] 0x1400026c0  GetDefaultState    — return 2
//   [17] 0x140004080  CreateState        — self + 0x28
// Slots 16/17 are the same two functions ResidenceComponent uses, i.e. this is a
// state-machine controller whose default state is 2.
//
// ⚠ SLOT 7 IS OnRemove, NOT OnCreate. It calls 0x140091ba0, which SCANS FOR AND
// CLEARS the entity from the water-source array — a removal, not a registration.
// This file used to call it OnCreate (the standing Ghidra OnCreate/OnRemove
// off-by-one, see reference_ghidra_oncreate_onremove_mislabel), which is part of why
// the missing REGISTER half went unnoticed for so long: the file looked complete.
//

#include "Component/wellcomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Resolved helpers
// ============================================================

typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t s_fn_Component_Serialize = nullptr;

typedef void* (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t s_fn_Serializer_Ctor = nullptr;

typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t s_fn_Serializer_Dtor = nullptr;

// FUN_140091ba0 — on entity (un)register, scan the manager's entity array for the
// matching IBP and, if found, clear its natural-resource grid mark + null the slot.
// (Despite the legacy "ArrayPushEntity" name, it is a remove-by-IBP, not a push.)
typedef void (*fn_ArrayPushEntity_t)(void* manager, void* internalBasePtr);
static fn_ArrayPushEntity_t s_fn_ArrayPushEntity = nullptr;

// Game world/population manager singleton
static void** s_pWorldManager = nullptr;

static bool s_initialized = false;

// FUN_140003110 — dynamic 8-byte-element array set-count + insert (ported in
// action_base_hunt.cpp, registered). Used here for the IBP-array append.
typedef void (*fn_DynArraySetCount_t)(void* arr, int newCount, void* valuePtr);
static fn_DynArraySetCount_t s_fn_DynArraySetCount = nullptr;

// System::MemoryAllocator::MemAlloc/MemFree, the STATIC (size, align) pair behind
// dispatch slots 0x140152b60 / 0x140152b68.
typedef void* (*fn_MemAllocStatic_t)(size_t size, int align);
typedef void  (*fn_MemFreeStatic_t)(void* p);
static fn_MemAllocStatic_t s_fn_MemAllocStatic = nullptr;
static fn_MemFreeStatic_t  s_fn_MemFreeStatic  = nullptr;

typedef bool     (*fn_WellIsDerived_t)(const void* obj, uint32_t tid);
typedef uint32_t (*fn_WellGetTypeId_t)(void* self);
static fn_WellIsDerived_t s_fn_WellIsDerived = nullptr;

// The tile-position component the registrar looks for. 0x1401c79e0 is labelled three
// different ways across this repo (MapComponent twice, FootprintComponent once,
// "NaturalResource entity-data" once), so resolve it BY NAME and let the run settle it:
// the field this code reads (int16 x/y at +0x28/+0x2a) is the same pair the already-
// working unregister path reaches through FUN_140004680, which agrees on MapComponent.
static uint32_t WellMapComponentTypeId() {
    static uint32_t id = 0;
    if (!id || id == 0xFFFFFFFFu) id = ::Object::GetTypeIdByName("MapComponent");
    return id;
}

// ============================================================
// WellComponent::Shutdown (vtable slot [3], 0x14005cfa0)
// ============================================================
// Component_Serialize + two empty serializer blocks (controller compat).

void WellComponent_Shutdown(WellComponent* self, void* stream) {
    if (s_fn_Component_Serialize)
        s_fn_Component_Serialize(self, stream);

    // First empty serializer block (component section)
    uint8_t serializer1[24];
    if (s_fn_Serializer_Ctor)
        s_fn_Serializer_Ctor(serializer1, stream, 0);
    if (s_fn_Serializer_Dtor)
        s_fn_Serializer_Dtor(serializer1);

    // Second empty serializer block (controller section)
    uint8_t serializer2[24];
    if (s_fn_Serializer_Ctor)
        s_fn_Serializer_Ctor(serializer2, stream, 0);
    if (s_fn_Serializer_Dtor)
        s_fn_Serializer_Dtor(serializer2);
}

// ============================================================
// WellComponent::OnRemove (vtable slot [7], 0x14005cf60)
// ============================================================
// UNregisters the owning entity from the TownManager water-source list. The
// matching REGISTER is the state-2 enter at the bottom of this file.

void WellComponent_OnRemove(WellComponent* self) {
    // ⚠ FIXED 2026-08-22 (fire lane): retail 0x14005cf60 is
    //     InternalBasePtr::InternalBasePtr(&local, param_1)   // param_1 == THIS
    //     FUN_140091ba0(*(gw + 0xf0), &local)
    // — it wraps the COMPONENT, not the owner entity. This passed `self->owner`,
    // so the IBP handed to 0x140091ba0 identified the wrong object and its
    // scan-for-matching-IBP could never hit. Latent while WellComponent was inert
    // (slot 7 was never installed); it matters now that the FireWork spiral search
    // depends on wells being findable in the gw+0xf0 entity index.
    uint8_t basePtr[24];
    if (fn_InternalBasePtr_Ctor)
        fn_InternalBasePtr_Ctor(basePtr, self);

    // *(GameWorld + 0xF0) is the TownManager itself -- not a "water source array".
    // 0x140091ba0 takes the manager and indexes its own +0xe0/+0xe8 pair.
    if (s_pWorldManager && *s_pWorldManager && s_fn_ArrayPushEntity) {
        void* gameWorld = *s_pWorldManager;
        void* townMgr = *reinterpret_cast<void**>(
            reinterpret_cast<uintptr_t>(gameWorld) + 0xF0);
        s_fn_ArrayPushEntity(townMgr, basePtr);
    }
}

// FUN_140004680 — MapComponent/resData finder used by the remove scan (registered).
typedef void* (*fn_FindMapResData_t)(int64_t resData, void* outIBP);
static fn_FindMapResData_t s_fn_FindMapResData = nullptr;

// ============================================================
// Reimpl of FUN_1400889e0 (Component reg wave B — WELL cluster).
// Pure leaf (no exe callees). Compacts the 4-byte-stride int array of one terrain
// grid cell, deleting every entry equal to `value`. The cell is located from the
// world map width (*(int*)(*(int**)(world+0xf8)) >> 3 = cells-per-row) and the
// 8-px-quantised (x,y): cellIndex = (width>>3 ... already shifted)*(y>>3)+(x>>3);
// cell record stride 0x10 lives at manager+0x140; record = {int* data@+0, int count@+8}.
// Faithful 1:1 port of the Ghidra decompile (same quirky shift-and-overwrite compaction).
// ============================================================
void WellComp_RemoveCellEntry_889e0(int64_t manager, int x, int y, int value) {
    void* world = (g_pGameWorldSingleton && *g_pGameWorldSingleton) ? *g_pGameWorldSingleton : nullptr;
    if (!world) return;
    int width = *reinterpret_cast<int*>(*reinterpret_cast<int**>(
                    reinterpret_cast<uint8_t*>(world) + 0xf8)) >> 3;
    int64_t* cell = reinterpret_cast<int64_t*>(
        (int64_t)((width * (y >> 3)) + (x >> 3)) * 0x10 +
        *reinterpret_cast<int64_t*>(manager + 0x140));
    int count = (int)cell[1];
    if (count <= 0) return;

    int i = 0;
    int64_t byteOff = 0;
    do {
        if (*reinterpret_cast<int*>(byteOff + cell[0]) == value) {
            int64_t shiftOff = byteOff;
            int j = i;
            if (i < count) {
                do {
                    j++;
                    if (j < (int)cell[1]) {
                        int* p = reinterpret_cast<int*>(cell[0] + shiftOff);
                        if (p) *p = p[1];
                    }
                    shiftOff += 4;
                } while (j < (int)cell[1]);
            }
            *reinterpret_cast<int*>(&cell[1]) = (int)cell[1] - 1;
            i--;
            byteOff -= 4;
        }
        count = (int)cell[1];
        i++;
        byteOff += 4;
    } while (i < count);
}

// ============================================================
// Reimpl of FUN_140091ba0 (Component reg wave B — WELL cluster).
// On (un)register, scan the manager's IBP array {ptr@+0xe0, count@+0xe8} for the entry
// whose resolved value equals *param2 (the owner IBP). When found: resolve the entry's
// MapComponent/resData (FUN_140004680, registered) for its tile (short x@+0x28, y@+0x2a),
// clear the natural-resource grid mark via WellComp_RemoveCellEntry_889e0(manager, x, y,
// -1-index), then null the slot with a fresh InternalBasePtr. Closure-safe: FUN_140004680
// (reg) + FUN_1400889e0 (reg below) + CheckPtr/IBP-ctor (engine). 1:1 port.
// ============================================================
void WellComp_UnregisterEntity_91ba0(int64_t manager, void* param2) {
    if (*reinterpret_cast<int*>(manager + 0xe8) <= 0) return;
    int64_t off = 0;
    int idx = 0;
    do {
        int64_t arr = *reinterpret_cast<int64_t*>(manager + 0xe0);
        fn_InternalBasePtr_CheckPtr(param2);
        if (*reinterpret_cast<int64_t*>(arr + off) == *reinterpret_cast<int64_t*>(param2)) {
            if (idx < 0) return;
            int64_t* slot = reinterpret_cast<int64_t*>(
                *reinterpret_cast<int64_t*>(manager + 0xe0) + (int64_t)idx * 8);
            fn_InternalBasePtr_CheckPtr(slot);
            int64_t entry = *reinterpret_cast<int64_t*>(*slot + 0x10);
            fn_InternalBasePtr_CheckPtr(&entry);
            int64_t resData = 0;
            if (s_fn_FindMapResData) s_fn_FindMapResData(entry, &resData);
            fn_InternalBasePtr_CheckPtr(&resData);
            int rx = (int)*reinterpret_cast<int16_t*>(resData + 0x28);
            int ry = (int)*reinterpret_cast<int16_t*>(resData + 0x2a);
            WellComp_RemoveCellEntry_889e0(manager, rx, ry, -1 - idx);
            int64_t cleared = 0;
            fn_InternalBasePtr_Ctor(&cleared, nullptr);
            *reinterpret_cast<int64_t*>((int64_t)idx * 8 + *reinterpret_cast<int64_t*>(manager + 0xe0)) = cleared;
            return;
        }
        idx++;
        off += 8;
    } while (idx < *reinterpret_cast<int*>(manager + 0xe8));
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void WellComponent_InitFuncs() {
    if (s_initialized) return;

    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));

    uintptr_t slot;
    slot = ResolveAddr(Module::Exe, 0x140152ad8);
    if (slot) s_fn_Serializer_Ctor = *reinterpret_cast<fn_Serializer_Ctor_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152ad0);
    if (slot) s_fn_Serializer_Dtor = *reinterpret_cast<fn_Serializer_Dtor_t*>(slot);

    s_fn_ArrayPushEntity = reinterpret_cast<fn_ArrayPushEntity_t>(
        ResolveAddr(Module::Exe, 0x140091ba0));
    s_fn_FindMapResData = reinterpret_cast<fn_FindMapResData_t>(
        ResolveAddr(Module::Exe, 0x140004680));
    s_pWorldManager = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x1401c3dd8));

    s_fn_DynArraySetCount = reinterpret_cast<fn_DynArraySetCount_t>(
        ResolveAddr(Module::Exe, 0x140003110));
    s_fn_MemAllocStatic = reinterpret_cast<fn_MemAllocStatic_t>(
        ResolveExport("?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z"));
    s_fn_MemFreeStatic = reinterpret_cast<fn_MemFreeStatic_t>(
        ResolveExport("?MemFree@MemoryAllocator@System@@SAXPEAX@Z"));
    slot = ResolveAddr(Module::Exe, 0x140152a98);
    if (slot) s_fn_WellIsDerived = *reinterpret_cast<fn_WellIsDerived_t*>(slot);

    s_initialized = true;
}

// ============================================================
// THE WATER-SOURCE REGISTRATION HALF (added 2026-08-22, fire lane)
// ============================================================
// Until now only the REMOVE half of this pair existed, which is why the FireWork
// spiral search returned found=0 on every run even with a well standing in the town:
// townMgr+0xE0 (the water-source IBP array) and townMgr+0x140 (the coarse per-cell
// index the search actually walks) were never written by anyone.
//
// HOW RETAIL GETS HERE. WellComponent extends ControllerComponent and owns a private
// state-type table, built at startup by 0x14014bc20 (a byte-identical copy sits
// unreferenced at 0x14005d110):
//     0x14000fed0(2, 0x14005cf20)   state 2 "built",    enter = REGISTER
//     0x14000ffe0(3)                  transitions -> {3}
//     0x14000fed0(3, 0x14005cee0)   state 3 "demolish", enter = UNREGISTER
//     0x14000ffe0(6) (4) (5)          transitions -> {6,4,5}
// 0x14000fed0 appends a 0xf8-byte state entry {id@+0x00, enterHandler@+0x08} to the
// table at 0x1401d1d70; 0x14000ffe0 appends a {targetId,null} pair to that entry's
// transition list at +0x28 (count +0xa8). That is EXACTLY the layout
// controller_state_table.h already models, and exactly the graph ResidenceComponent
// has -- state 2 registers with the TownManager, state 3 unregisters, same transition
// targets. WellComponent's vtable slots 16/17 are the same two functions as
// ResidenceComponent's (0x1400026c0 "return 2" and 0x140004080 "self+0x28"), so the
// bring-up below is the residence pattern with the two handlers swapped in.

// ============================================================
// Reimpl of FUN_140088a80 — TownManager coarse-cell index push_back.
// The exact mirror of WellComp_RemoveCellEntry_889e0 above: same addressing
//     cellIdx = (mapWidth >> 3) * (y >> 3) + (x >> 3)
//     record  = *(townMgr + 0x140) + cellIdx * 0x10 = { int* data; int count; int cap }
// but it APPENDS `value` instead of compacting it out. This is the array the FireWork
// spiral search reads, so without this call the buckets stay empty however many wells
// the town has.
// ============================================================
void WellComp_AddCellEntry_88a80(int64_t townMgr, int x, int y, int value) {
    // This helper is shared: CitizenList_PostLoad_8f580 (0x14008f580) calls it for
    // every residence cell on a LOAD, long before any well reaches its state-2
    // enter — the only other place that used to run WellComponent_InitFuncs. With
    // the allocator pointer still null the growth branch bailed after having
    // already written the new capacity, leaving a record with cap>0 and a NULL
    // data pointer; the next insert into that same cell then saw cap >= newCount,
    // skipped the alloc entirely and wrote through the null. Resolve our own deps.
    WellComponent_InitFuncs();
    void* world = (g_pGameWorldSingleton && *g_pGameWorldSingleton) ? *g_pGameWorldSingleton : nullptr;
    if (!world) return;
    const int mapW = *reinterpret_cast<int*>(*reinterpret_cast<int**>(
                         reinterpret_cast<uint8_t*>(world) + 0xf8));
    const int64_t cellIdx = static_cast<int64_t>((mapW >> 3) * (y >> 3) + (x >> 3));
    uint8_t* rec = reinterpret_cast<uint8_t*>(
        *reinterpret_cast<int64_t*>(townMgr + 0x140) + cellIdx * 0x10);

    int* oldData = *reinterpret_cast<int**>(rec);
    const int count = *reinterpret_cast<int*>(rec + 0x08);
    const int newCount = count + 1;
    *reinterpret_cast<int*>(rec + 0x08) = newCount;

    int* data = oldData;
    if (newCount > *reinterpret_cast<int*>(rec + 0x0c)) {
        // Retail's growth rule: first growth of an EMPTY array takes exactly one slot;
        // any later growth-by-one triples. (`count == 0` is the only way the second
        // test fails, since newCount is always count+1 here.)
        int newCap = newCount;
        if (count != 0) newCap = newCount * 3;
        *reinterpret_cast<int*>(rec + 0x0c) = newCap;
        if (!s_fn_MemAllocStatic) { *reinterpret_cast<int*>(rec + 0x08) = count; return; }
        data = reinterpret_cast<int*>(s_fn_MemAllocStatic(static_cast<size_t>(newCap) * 4, 4));
        *reinterpret_cast<int**>(rec) = data;
    }
    // Rows below the old count come from the OLD buffer; the new row takes `value`.
    // When the buffer did not move, retail starts at oldCount and writes only that one.
    for (int i = (oldData == data) ? count : 0; i < newCount; ++i)
        data[i] = (i >= count) ? value : oldData[i];

    if (oldData != data && s_fn_MemFreeStatic) s_fn_MemFreeStatic(oldData);
}

// ============================================================
// Reimpl of FUN_140091cb0 — TownManager::RegisterWaterSource.
// 1. Park the water source's IBP in the array {ptr@+0xe0, count@+0xe8}, reusing the
//    first NULLED slot (the unregister leaves holes) and appending only if none.
// 2. Find the owner entity's MapComponent, and stamp its tile in the coarse cell index
//    with `-1 - slotIndex`. The negative encoding is what WellComp_RemoveCellEntry_889e0
//    searches for on removal, and what the FireWork search decodes back to a slot.
// ============================================================
void WellComp_RegisterEntity_91cb0(int64_t townMgr, void* ibp) {
    int index = -1;
    const int count = *reinterpret_cast<int*>(townMgr + 0xe8);
    if (count > 0) {
        int64_t* arr = *reinterpret_cast<int64_t**>(townMgr + 0xe0);
        for (int i = 0; i < count; ++i) {
            if (arr[i] != 0) continue;
            arr[i] = *reinterpret_cast<int64_t*>(ibp);
            index = i;
            break;
        }
    }
    if (index < 0) {
        index = *reinterpret_cast<int*>(townMgr + 0xe8);      // append at the current end
        if (!s_fn_DynArraySetCount) {
            fprintf(stderr, "[Well] register ABORTED: FUN_140003110 unresolved, the "
                            "water-source array cannot grow\n");
            fflush(stderr);
            return;
        }
        s_fn_DynArraySetCount(reinterpret_cast<void*>(townMgr + 0xe0), index + 1, ibp);
    }

    int64_t held = *reinterpret_cast<int64_t*>(ibp);
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&held);
    int64_t entity = *reinterpret_cast<int64_t*>(held + 0x10);
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&entity);

    const uint32_t tidMap = WellMapComponentTypeId();
    void* mapComp = nullptr;
    const int compCount = *reinterpret_cast<int*>(entity + 0x38);
    const int64_t comps = *reinterpret_cast<int64_t*>(entity + 0x30);
    for (int i = 0; i < compCount; ++i) {
        void* slot = reinterpret_cast<void*>(comps + static_cast<int64_t>(i) * 8);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(slot);
        void* comp = *reinterpret_cast<void**>(slot);
        if (!comp) continue;
        void** vt = *reinterpret_cast<void***>(comp);
        if (!vt || !vt[2]) continue;
        if (reinterpret_cast<fn_WellGetTypeId_t>(vt[2])(comp) != tidMap) continue;
        // Retail re-reads the slot and confirms with IsDerivedFrom before accepting.
        if (s_fn_WellIsDerived && s_fn_WellIsDerived(comp, tidMap)) mapComp = comp;
        break;
    }

    int64_t pos = 0;
    if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&pos, mapComp);
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&pos);
    if (pos == 0) {
        // Retail derefs this unguarded -- it relies on every registered water source
        // owning a MapComponent. Ours can legitimately reach here while the component
        // set is still being brought up, and a fault would take the whole boot with it,
        // so say which half failed instead of guessing.
        fprintf(stderr, "[Well] register INCOMPLETE: slot %d holds the IBP but the owner "
                        "entity has no MapComponent (tid=%u, %d components scanned) -- no "
                        "cell stamp, so the FireWork search still cannot find this well\n",
                index, tidMap, compCount);
        fflush(stderr);
        return;
    }
    const int tx = *reinterpret_cast<int16_t*>(pos + 0x28);
    const int ty = *reinterpret_cast<int16_t*>(pos + 0x2a);
    WellComp_AddCellEntry_88a80(townMgr, tx, ty, -1 - index);

    {
        static int n = 0;
        if (n < 8) { ++n;
            fprintf(stderr, "[Well] water source REGISTERED: slot=%d tile=(%d,%d) "
                            "cellValue=%d townMgr=%p arrayCount=%d\n",
                    index, tx, ty, -1 - index, reinterpret_cast<void*>(townMgr),
                    *reinterpret_cast<int*>(townMgr + 0xe8));
            fflush(stderr);
        }
    }
}

// ============================================================
// WellComponent state-2 "built" enter — 0x14005cf20.
// Wraps the COMPONENT (not the owner entity) in an InternalBasePtr and hands it to the
// registrar with TownManager = *(GameWorld+0xf0). Retail returns 0 in AL so the SM's
// Enter walk continues past this node.
// ============================================================
extern "C" char WellComponent_EnterBuilt(void* comp, void* /*sm*/) {
    WellComponent_InitFuncs();
    if (!fn_InternalBasePtr_Ctor || !s_pWorldManager || !*s_pWorldManager) return 0;
    uint8_t ibp[24] = {};
    fn_InternalBasePtr_Ctor(ibp, comp);
    void* gw = *s_pWorldManager;
    int64_t townMgr = *reinterpret_cast<int64_t*>(reinterpret_cast<uint8_t*>(gw) + 0xf0);
    if (!townMgr) return 0;
    WellComp_RegisterEntity_91cb0(townMgr, ibp);
    return 0;
}

// WellComponent state-3 "demolish" enter — 0x14005cee0. Byte-identical to the slot-7
// removal hook (0x14005cf60): wrap the component, call the unregister.
extern "C" char WellComponent_EnterDemolish(void* comp, void* /*sm*/) {
    WellComponent_OnRemove(reinterpret_cast<WellComponent*>(comp));
    return 0;
}

// ===== Standalone factory =====
// Bring-up is the ResidenceComponent pattern, one for one: WellComponent has the
// same slot 16 (GetDefaultState -> 2) and slot 17 (CreateState -> self+0x28), and
// its retail state table has the same two-state shape. Without the live path the
// component is inert, nothing ever enters state 2, and the town has wells that no
// bucket can be filled from.
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"
#include "Component/controller_state_table.h"

namespace { struct WellCtrlTag {}; }
using WellCtrlDispatch = ::ControllerLive::Dispatcher<WellCtrlTag>;

// The retail table, transcribed from 0x14014bc20 call for call.
static void WellComponent_PopulateStateTable() {
    using namespace ::ControllerLive;
    StateTable& t = WellCtrlDispatch::table;
    if (t.count != 0) return;                      // idempotent
    TableRegisterState(t, 2, reinterpret_cast<void*>(&WellComponent_EnterBuilt),
                       nullptr, nullptr, nullptr);
    TableAddTransition(t, 3, nullptr);
    TableRegisterState(t, 3, reinterpret_cast<void*>(&WellComponent_EnterDemolish),
                       nullptr, nullptr, nullptr);
    TableAddTransition(t, 6, nullptr);
    TableAddTransition(t, 4, nullptr);
    TableAddTransition(t, 5, nullptr);
}

static uint32_t WellComponent_LiveGetTypeId(void*) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("WellComponent");
    return cached;
}
static void* WellComponent_LiveVtable[::ComponentFactory::kInertVtableSlots];

static void WellComponent_ShutdownVSlot(void* s, void* stream) {   // vtable index 3
    // Slot 3 is the FIRST call into this class on a load-from-save:
    // PersistObject creates the object and dispatches vt[0x18] before Startup
    // or any tick. Until the resolver runs every s_fn_* is null and the fully
    // null-guarded body consumes ZERO bytes, so the record is short by however
    // much this component would have written, and the stream desyncs several
    // records LATER with no crash at the defect itself.
    // Same defect as ParticleComponent (2026-09-04) and StateMachineComponent
    // (2026-08-28). Rule, from uicomponent_serialize.cpp: resolve
    // unconditionally at the top of every call.
    WellComponent_InitFuncs();
    WellComponent_Shutdown(reinterpret_cast<WellComponent*>(s), stream);
}
static void WellComponent_OnRemoveVSlot(void* s) {                 // vtable index 7
    WellComponent_OnRemove(reinterpret_cast<WellComponent*>(s));
}
static int   WellComponent_GetDefaultState(void*) { return 2; }    // 0x1400026c0
static void* WellComponent_CreateState(void* self) {               // 0x140004080
    return reinterpret_cast<uint8_t*>(self) + 0x28;
}

static void WellComponent_FinishLiveVtable(void* mem) {
    // Slot 3 is the STREAM SERIALIZER, not a gameplay slot: the save encoding
    // does not care whether this component's live feature is enabled. Wired
    // UNCONDITIONALLY, ahead of the gate -- behind it, a disabled flag left the
    // class falling through to base-only Component_Serialize, which skips this
    // body's bytes and desyncs the stream for EVERY object after it.
    // See reference_vtable_slot3_serialize_never_installed.
    WellComponent_LiveVtable[3]  = reinterpret_cast<void*>(&WellComponent_ShutdownVSlot);
    if (!::ComponentFactory::LiveEnabled("BANISHED_LIVE_WELL")) return;
    WellComponent_LiveVtable[7]  = reinterpret_cast<void*>(&WellComponent_OnRemoveVSlot);
    WellComponent_LiveVtable[16] = reinterpret_cast<void*>(&WellComponent_GetDefaultState);
    WellComponent_LiveVtable[17] = reinterpret_cast<void*>(&WellComponent_CreateState);
    WellComponent_PopulateStateTable();
    if (mem) {
        // Embedded state node at +0x28: dispatcher vtable + the node+0x18
        // back-pointer the tail-called handlers read as their `this`.
        *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(mem) + 0x28) =
            reinterpret_cast<void*>(WellCtrlDispatch::Vtable());
        *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(mem) + 0x28 + 0x18) = mem;
    }
}

extern "C" void* WellComponent_Factory(void) {
    void* p = ::ComponentFactory::MakeInertTyped(0x48, 0x82,
        WellComponent_LiveVtable,
        reinterpret_cast<void*>(&WellComponent_LiveGetTypeId));
    WellComponent_FinishLiveVtable(p);
    return p;
}
static void* WellComponent_CreateInPlace(void* mem) {
    if (mem) ::ComponentFactory::InitInertTyped(mem, 0x48, 0x82,
        WellComponent_LiveVtable,
        reinterpret_cast<void*>(&WellComponent_LiveGetTypeId));
    WellComponent_FinishLiveVtable(mem);
    return mem;
}

REGISTER_TYPE(WellComponent, "ControllerComponent",
              &WellComponent_Factory, &WellComponent_CreateInPlace);
