// code/Game/Component/storagecomponent.cpp
// Clean reimplementation of StorageComponent lifecycle methods from Application-x64.exe.
//
// StorageComponent (0x40 bytes) manages storage slots for buildings.
// PERF HOTSPOT: QueryCount is O(n*m) per citizen per tick.
//
// Vtable at 0x140164720. Non-trivial overrides:
//   Shutdown  (0x14012c730) — versioned serialization of storage slot data
//   Draw      (0x14012c640) — rebuild storage slots from building template
//   QueryCount (0x14012cf10) — count items across all storage slots (44 xrefs)
//   Helper_c560 (0x14012c560) — check material flag intersection
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Component/storagecomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

// ============================================================
// Resolved helpers
// ============================================================

// Component_Serialize (base class) at 0x1401189b0
typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t s_fn_Component_Serialize = nullptr;

// IO::Serializer ctor/dtor — dispatch slots
typedef void (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t s_fn_Serializer_Ctor = nullptr;
typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t s_fn_Serializer_Dtor = nullptr;

// IO::Serializer::Serialize(int*) — dispatch slot 0x140152ac8
typedef void (*fn_Serialize_Int_t)(void* serializer, int* field, const char* name);
static fn_Serialize_Int_t s_fn_Serialize_Int = nullptr;

// IO::Serializer::GetVersion — dispatch slot 0x140152a48
typedef int (*fn_Serializer_GetVersion_t)(void* self);
static fn_Serializer_GetVersion_t s_fn_Serializer_GetVersion = nullptr;

// FUN_14000f090 — resize dynamic array (0x10-byte entries)
typedef void (*fn_DynArrayResize_t)(void* arrayBase, int newCount);
static fn_DynArrayResize_t s_fn_DynArrayResize = nullptr;

// FUN_140002340 — serialize External reference (item type ptr)
typedef void (*fn_SerializeExternalRef_t)(void* refPtr, void* serializer);
static fn_SerializeExternalRef_t s_fn_SerializeExternalRef = nullptr;

// IO::Serializer vtable slot +0x98 — serialize via vtable call (for field3c)
// This is a virtual call: (*(code**)(serializer_vtable + 0x98))(serializer, field, name)
// We call it indirectly through the serializer's vtable.

// fn_IsDerivedFrom — already in engine_funcs.h

// StorageDescription type ID at 0x1401c7eb0
static uint32_t* s_TypeId_StorageDescription = nullptr;

// FUN_14012d000 — add storage slot entry
typedef void (*fn_StorageAddSlot_t)(void* self, uintptr_t materialType, int count);
static fn_StorageAddSlot_t s_fn_StorageAddSlot = nullptr;

// FUN_140004680 — FindComponent (MapComponent type)
typedef void* (*fn_FindComponent_Map_t)(uintptr_t entity, void* retval);
static fn_FindComponent_Map_t s_fn_FindComponent_Map = nullptr;

static bool s_initialized = false;

static void EnsureHelpers() {
    if (s_initialized) return;

    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));

    uintptr_t slot;

    slot = ResolveAddr(Module::Exe, 0x140152ad8);
    if (slot) s_fn_Serializer_Ctor = *reinterpret_cast<fn_Serializer_Ctor_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152ad0);
    if (slot) s_fn_Serializer_Dtor = *reinterpret_cast<fn_Serializer_Dtor_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152ac8);
    if (slot) s_fn_Serialize_Int = *reinterpret_cast<fn_Serialize_Int_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152a48);
    if (slot) s_fn_Serializer_GetVersion = *reinterpret_cast<fn_Serializer_GetVersion_t*>(slot);

    s_fn_DynArrayResize = reinterpret_cast<fn_DynArrayResize_t>(
        ResolveAddr(Module::Exe, 0x14000f090));

    s_fn_SerializeExternalRef = reinterpret_cast<fn_SerializeExternalRef_t>(
        ResolveAddr(Module::Exe, 0x140002340));

    s_TypeId_StorageDescription = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7eb0));

    s_fn_StorageAddSlot = reinterpret_cast<fn_StorageAddSlot_t>(
        ResolveAddr(Module::Exe, 0x14012d000));

    s_fn_FindComponent_Map = reinterpret_cast<fn_FindComponent_Map_t>(
        ResolveAddr(Module::Exe, 0x140004680));

    s_initialized = true;
}

// ============================================================
// Effective type-ids — the raw 0x1401c7xxx .rdata addresses resolve to NULL in the
// standalone exe, so use ::Object::GetTypeIdByName (the shared
// globals are never written).
// ============================================================
static uint32_t EffTid_StorageDescription() {
    if (s_TypeId_StorageDescription && *s_TypeId_StorageDescription)
        return *s_TypeId_StorageDescription;
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("StorageDescription");
    return cached;
}
static uint32_t EffTid_RawMaterialDescription() {
    if (g_TypeId_RawMaterialDescription && *g_TypeId_RawMaterialDescription)
        return *g_TypeId_RawMaterialDescription;
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("RawMaterialDescription");
    return cached;
}

// ============================================================
// Helper: Get StorageDescription sibling component
// ============================================================
static void* GetStorageDescription(StorageComponent* self) {
    EnsureHelpers();
    uint32_t tid = EffTid_StorageDescription();
    if (!fn_IsDerivedFrom || tid == 0) return nullptr;

    void* obj = self->_unk18;
    if (!obj) return nullptr;

    bool derived = fn_IsDerivedFrom(obj, tid);
    return derived ? obj : nullptr;
}

// ============================================================
// StorageComponent::Shutdown (vtable slot [3], 0x14012c730)
// ============================================================
// Versioned serialization (version 1):
//   - Serialize slot count
//   - If loading (version==0 path), resize slot array
//   - For each slot: serialize two ints (type fields) + External ref
//   - Serialize field38 (int) and field3c (via vtable call)
//   - Version 0 migration: copy StorageDescription+0x28 into field3c
//
// Original disassembly:
//   Component_Serialize(self, stream)
//   Serializer_Ctor(local, stream, 1)
//   Serialize_Int(&slotCount)
//   if loading: DynArrayResize(self+0x28, slotCount)
//   loop: Serialize_Int(slot+0x08), Serialize_Int(slot+0x0C), SerializeExternalRef(slot+0x00)
//   Serialize_Int(self+0x38)
//   vtable[+0x98](serializer, self+0x3C, nullptr)  -- serialize field 0x3C
//   if version==0 && loading: field3c = StorageDescription+0x28
//   Serializer_Dtor(local)

void StorageComponent_Shutdown(StorageComponent* self, void* stream) {
    EnsureHelpers();

    // Call base Component_Serialize
    if (s_fn_Component_Serialize) {
        s_fn_Component_Serialize(self, stream);
    }

    // Create local IO::Serializer (version 1)
    uint8_t serializer[24];
    if (s_fn_Serializer_Ctor) {
        s_fn_Serializer_Ctor(serializer, stream, 1);
    }

    // Serialize slot count
    uintptr_t selfAddr = (uintptr_t)self;
    int slotCount = *(int*)(selfAddr + 0x30);
    int serSlotCount = slotCount;
    if (s_fn_Serialize_Int) {
        s_fn_Serialize_Int(serializer, &serSlotCount, nullptr);
    }

    // If loading (encoding direction == 0), resize the slot array
    int encodingDir = *(int*)(*(uintptr_t*)serializer + 8);
    if (encodingDir == 0) {
        if (s_fn_DynArrayResize) {
            s_fn_DynArrayResize((void*)(selfAddr + 0x28), serSlotCount);
        }
    }

    // Iterate storage slots (each 0x10 bytes: [ptr(8), int(4), int(4)])
    if (serSlotCount > 0) {
        uintptr_t slotBase = *(uintptr_t*)(selfAddr + 0x28);
        for (int i = 0; i < serSlotCount; i++) {
            uintptr_t slotEntry = slotBase + (uintptr_t)i * 0x10;
            // Serialize two int fields at slot+0x08 and slot+0x0C
            if (s_fn_Serialize_Int) {
                s_fn_Serialize_Int(serializer, (int*)(slotEntry + 0x08), nullptr);
                s_fn_Serialize_Int(serializer, (int*)(slotEntry + 0x0C), nullptr);
            }
            // Serialize External reference at slot+0x00
            if (s_fn_SerializeExternalRef) {
                s_fn_SerializeExternalRef((void*)slotEntry, serializer);
            }
        }
    }

    // Serialize field38 (int at self+0x38)
    if (s_fn_Serialize_Int) {
        s_fn_Serialize_Int(serializer, (int*)(selfAddr + 0x38), nullptr);
    }

    // Serialize field3c through the serializer's ENCODING object at Serializer+0x08 —
    // NOT the Stream at Serializer+0x00. IO::Serializer::Serializer (Runtime 0x1800b9960)
    // lays it out as { Stream* @+0x00, Encoding* @+0x08 }, and retail dispatches off the
    // ENCODING (0x14012c7fc: MOV RCX,[RSP+0x30] = serializer+8; CALL [RAX+0x98]).
    //
    // ⚠ This read +0x00, so it called the STREAM's vtable slot 0x98 with this=Stream —
    // a different class, same slot index. Measured: the two slots resolve to different
    // functions, and the stream one writes EIGHT bytes at self+0x3c, running off the end
    // of a 0x40-byte StorageComponent and zeroing the boundary tag of the very next heap
    // block — which is this component's own slot array. That poisoned the allocator free
    // list on EVERY StorageComponent at load, and the game died much later inside
    // Allocate/UnlinkFreeTag (hence LOAD DONE, a healthy-looking run, then a crash with
    // no obvious connection). Probe evidence: tag@+0x40 0x0e -> 0, and
    // 0x00002000076e40be -> 0x0000200000000000 (low dword zeroed = an 8-byte write).
    //
    // Third arg mirrors retail's XOR R8D,R8D.
    uintptr_t encoding = *(uintptr_t*)((uintptr_t)serializer + 8);
    if (encoding) {
        uintptr_t encVtable = *(uintptr_t*)encoding;
        typedef void (*fn_EncSer98_t)(void* enc, void* field, const char* name);
        fn_EncSer98_t encSer98 = *(fn_EncSer98_t*)(encVtable + 0x98);
        if (encSer98) encSer98((void*)encoding, (void*)(selfAddr + 0x3c), nullptr);
    }


    // Version 0 migration: if loading and version==0, copy from StorageDescription
    if (s_fn_Serializer_GetVersion) {
        int version = s_fn_Serializer_GetVersion(serializer);
        if (version == 0 && encodingDir == 0) {
            void* storageDesc = GetStorageDescription(self);
            if (storageDesc) {
                *(uint32_t*)(selfAddr + 0x3c) = *(uint32_t*)((uintptr_t)storageDesc + 0x28);
            }
        }
    }

    // Destroy serializer
    if (s_fn_Serializer_Dtor) {
        s_fn_Serializer_Dtor(serializer);
    }
}

// ============================================================
// StorageComponent::Draw (vtable slot [6], 0x14012c640)
// ============================================================
// Rebuilds storage slot descriptors from building template data.
//
// Original logic:
//   1. Get StorageDescription via IsDerivedFrom(self->_unk18, typeId)
//   2. If storageDesc[0x21] == 0, return (no storage defined)
//   3. If storageDesc[0x20] != 0, find MapComponent on entity,
//      compute tile area = map.width * map.height
//   4. Read maxPerSlot from storageDesc+0x24, defaultType from storageDesc+0x28
//   5. Resize slot array to 0
//   6. Set field38 = maxPerSlot * tileArea, field3c = defaultType
//   7. Iterate storageDesc template entries (at +0x30 ptr, +0x38 count),
//      each 0x10 bytes: [materialType(8), count(4), pad(4)]
//      Call StorageAddSlot(self, materialType, count) for each

void StorageComponent_Draw(StorageComponent* self) {
    EnsureHelpers();

    // Get StorageDescription
    void* storageDesc = GetStorageDescription(self);
    if (!storageDesc) return;

    uintptr_t descAddr = (uintptr_t)storageDesc;
    uintptr_t selfAddr = (uintptr_t)self;

    // Check if storage is defined (byte at +0x21). NOTE: this early return leaves +0x38
    // (max volume capacity) at its ctor ZERO, and a zero capacity makes CanAdd refuse
    // everything and makes PickupStoredRawMaterial compute a requested count of 0 — which
    // deposits a {resType, 0} stack that hangs the spill loop. Report the exit, don't
    // assume it: an empty log line here would otherwise read as "capacity was set fine".
    if (*(uint8_t*)(descAddr + 0x21) == 0) {
        if (std::getenv("BANISHED_STORAGE_TRACE")) {
            std::fprintf(stderr, "[StorageCap] SKIPPED (desc+0x21==0) self=%p desc=%p — "
                         "capacity(+0x38) stays 0\n", self, storageDesc);
            std::fflush(stderr);
        }
        return;
    }

    // Compute tile multiplier
    int tileArea = 1;
    if (*(uint8_t*)(descAddr + 0x20) != 0) {
        // Find MapComponent on entity, get tile dimensions
        if (fn_InternalBasePtr_CheckPtr && s_fn_FindComponent_Map) {
            fn_InternalBasePtr_CheckPtr((void*)(selfAddr + 0x10));
            uintptr_t entity = *(uintptr_t*)(selfAddr + 0x10);
            uintptr_t mapCompBuf[2];
            s_fn_FindComponent_Map(entity, (void*)mapCompBuf);
            fn_InternalBasePtr_CheckPtr((void*)mapCompBuf);
            uintptr_t mapComp = mapCompBuf[0];
            if (mapComp) {
                int16_t w = *(int16_t*)(mapComp + 0x2c);
                int16_t h = *(int16_t*)(mapComp + 0x2e);
                tileArea = (int)w * (int)h;
            }
        }
    }

    // Read template data
    int maxPerSlot = *(int*)(descAddr + 0x24);
    uint32_t defaultType = *(uint32_t*)(descAddr + 0x28);

    // Resize slot array to 0 (clear existing)
    if (s_fn_DynArrayResize) {
        s_fn_DynArrayResize((void*)(selfAddr + 0x28), 0);
    }

    // Set computed values
    *(int*)(selfAddr + 0x38) = maxPerSlot * tileArea;
    *(uint32_t*)(selfAddr + 0x3c) = defaultType;
    if (std::getenv("BANISHED_STORAGE_TRACE")) {
        std::fprintf(stderr, "[StorageCap] self=%p capacity(+0x38)=%d (maxPerSlot=%d x tileArea=%d) "
                     "defaultType=0x%x%s\n", self, maxPerSlot * tileArea, maxPerSlot, tileArea,
                     defaultType, (maxPerSlot * tileArea == 0) ? "  <-- ZERO CAPACITY" : "");
        std::fflush(stderr);
    }

    // Iterate template storage entries
    uintptr_t templateArray = *(uintptr_t*)(descAddr + 0x30);
    int templateCount = *(int*)(descAddr + 0x38);

    if (templateCount > 0 && s_fn_StorageAddSlot) {
        for (int i = 0; i < templateCount; i++) {
            uintptr_t entry = templateArray + (uintptr_t)i * 0x10;
            uintptr_t materialType = *(uintptr_t*)entry;
            int count = *(int*)(entry + 8);
            s_fn_StorageAddSlot((void*)self, materialType, count);
        }
    }
}

// ============================================================
// Helper: find the RawMaterialDescription on a material TEMPLATE entity
// (descriptions at +0x28 raw-pointer array / +0x30 count) — the inline
// scan shared by QueryCount / Helper_c560 in the original.
// ============================================================
static void* FindRawMatDescription(uintptr_t materialEntity) {
    uint32_t tid = EffTid_RawMaterialDescription();
    if (!materialEntity || !fn_IsDerivedFrom || tid == 0) return nullptr;
    int compCount = *(int*)(materialEntity + 0x30);
    uintptr_t compArray = *(uintptr_t*)(materialEntity + 0x28);
    for (int i = 0; i < compCount; i++) {
        void* comp = *(void**)(compArray + (uintptr_t)i * 8);
        typedef uint32_t (*GetTypeId_t)(void*);
        uint32_t typeId = ((GetTypeId_t)(*(uintptr_t*)(*(uintptr_t*)comp + 0x10)))(comp);
        if (typeId == tid) {
            // Original: IsDerivedFrom-narrow, then break either way.
            return fn_IsDerivedFrom(comp, tid) ? comp : nullptr;
        }
    }
    return nullptr;
}

// ============================================================
// StorageComponent::QueryCount (0x14012cf10) — faithful reimpl
// ============================================================
// VOLUME-weighted total (verified vs Ghidra): for each storage slot
// (comp+0x28, stride 0x10: {materialEntity(8), count(4), reserved(4)}), find
// the material's RawMaterialDescription and accumulate
//   desc(+0x48 unit volume) * slot count(+0x08).
// The previous port summed raw counts (UNFAITHFUL — capacity +0x38 is in
// volume units, _volumeLimit * area). Standalone-inert guard: a slot whose
// material has no RawMaterialDescription contributes 0 (retail reads
// *(NULL+0x48)).

int StorageComponent_QueryCount(void* storageComp) {
    if (!storageComp) return 0;

    uintptr_t sc = (uintptr_t)storageComp;
    int slotCount = *(int*)(sc + 0x30);
    uintptr_t slotArray = *(uintptr_t*)(sc + 0x28);
    int total = 0;

    for (int i = 0; i < slotCount; i++) {
        uintptr_t slotEntry = slotArray + (uintptr_t)i * 0x10;
        void* matDesc = FindRawMatDescription(*(uintptr_t*)slotEntry);
        int unitVol = matDesc ? *(int*)((uintptr_t)matDesc + 0x48) : 0;
        total += unitVol * *(int*)(slotEntry + 0x08);
    }

    return total;
}

// ============================================================
// StorageComponent::QueryAvailable (0x14012ce70, "BuildRoad_helper_ce70")
// ============================================================
// Per-MATERIAL available count: scan the slot rows (comp+0x28, stride 0x10:
// {materialEntity(8), count(4), reserved(4)}) for the row whose material
// entity matches, return count - reserved; no row -> 0. Named BuildRoad_*
// in Ghidra because the road resource branch is its highest-profile caller
// (BuildRoad_Execute checks the citizen's HOME storage for the road
// material), but it is a plain StorageComponent accessor.
// Was never registered: the first stone road ever placed (the only work
// item with a non-null material at +0x50) called a null fn ptr and died
// at BuildRoad_Execute+0x7a7 (2026-07-26).

int StorageComponent_QueryAvailable(void* storageComp, int64_t materialEntity) {
    if (!storageComp) return 0;

    uintptr_t sc = (uintptr_t)storageComp;
    int slotCount = *(int*)(sc + 0x30);
    uintptr_t slotArray = *(uintptr_t*)(sc + 0x28);

    for (int i = 0; i < slotCount; i++) {
        uintptr_t row = slotArray + (uintptr_t)i * 0x10;
        if (*(int64_t*)row == materialEntity)
            return *(int*)(row + 0x08) - *(int*)(row + 0x0c);
    }
    return 0;
}

// ============================================================
// StorageComponent::Helper_c560 (0x14012c560) — null-safe reimpl
// ============================================================
// Checks if (param_1[0x3C] & RawMaterialDescription[0x5C]) != 0.
// Searches material type entity for RawMaterialDescription component,
// guards against NULL.

uint32_t StorageComponent_Helper_c560(void* param_1, void* param_2) {
    // Type-id via EffTid (GetTypeIdByName in the standalone exe).
    void* rawMatDesc = FindRawMatDescription((uintptr_t)param_2);

    if (!rawMatDesc) return 0;

    uint32_t flags1 = *(uint32_t*)((uintptr_t)param_1 + 0x3C);
    uint32_t flags2 = *(uint32_t*)((uintptr_t)rawMatDesc + 0x5C);
    return (flags1 & flags2) != 0 ? 1 : 0;
}

// ============================================================
// Reimpl of FUN_14000f090 (StorageComponent dynamic-array resize:
//   grow/shrink an array of 0x10-byte slot entries, plain memcpy of survivors)
// ============================================================
void StorageComponent_DynArrayResize(void* arrayBase, int newCount) {
    int64_t* field = (int64_t*)arrayBase;     // [0]=ptr, [1] low=count, high=capacity
    int oldCount = (int)field[1];
    if (newCount == oldCount) return;

    int64_t* oldBuf = (int64_t*)field[0];
    *(int*)((uintptr_t)field + 8) = newCount;
    if (*(int*)((uintptr_t)field + 0xc) < newCount) {
        *(int*)((uintptr_t)field + 0xc) = newCount;
        if (newCount == oldCount + 1 && oldCount != 0) {
            *(int*)((uintptr_t)field + 0xc) = newCount * 3;
        }
        void* nb = fn_MemAlloc(g_pMemAllocSingleton,
                               (uint64_t)((int64_t)*(int*)((uintptr_t)field + 0xc) << 4), 8);
        field[0] = (int64_t)nb;
    }

    int start = (oldBuf == (int64_t*)field[0]) ? oldCount : 0;
    if (start < newCount) {
        int64_t off = (int64_t)start * 0x10;
        for (int i = start; i < newCount; i++) {
            int64_t* dst = (int64_t*)(field[0] + off);
            if (dst) {
                if (i < oldCount) {
                    dst[0] = *(int64_t*)((uintptr_t)oldBuf + off);
                    dst[1] = *(int64_t*)((uintptr_t)oldBuf + off + 8);
                } else {
                    dst[0] = 0;
                    dst[1] = 0;
                }
            }
            off += 0x10;
        }
    }

    if (oldBuf != (int64_t*)field[0]) {
        if (oldBuf) fn_MemFreeStatic(oldBuf);
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void StorageComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[StorageComponent] Funcs resolved: Component_Serialize=%p Serializer_Ctor=%p "
           "DynArrayResize=%p StorageAddSlot=%p TypeId_StorageDesc=%p\n",
           (void*)s_fn_Component_Serialize, (void*)s_fn_Serializer_Ctor,
           (void*)s_fn_DynArrayResize, (void*)s_fn_StorageAddSlot,
           (void*)s_TypeId_StorageDescription);
#endif
}

// ===== Standalone factory (Foundation D — component instantiation) =====
// In standalone the original exe is not mapped, so Object::Factory::CreateObject
// must resolve to a C++ factory here. Produces an inert-but-valid StorageComponent
// (size 0x40, +0x20 capability flags 0x81, Object::Internal base + shared stub
// vtable, zeroed body). Size from StorageComponent_Factory MemAlloc; flags from the
// Component_Ctor (+0x20=0x80) chain OR'd with the leaf ctor bits.
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"

// Slot 6 (+0x30) = OnAdded, retail 0x14012c640 = StorageComponent::Draw — verified by
// reading the real vtable at 0x140164720: [3]=0x14012c730 (Shutdown), [6]=0x14012c640.
// ComponentEntity_OnAddedPass (the tail of EntityList::RegisterEntity) dispatches slot 6
// on every component whose capability flags (+0x20) have bit 0 — StorageComponent's 0x81
// qualifies — so in retail EVERY entity carrying a StorageComponent gets its capacity
// (+0x38 = _volumeLimit x area) and its template slots at register time.
//
// Standalone left the slot a stub and compensated with four hand-calls in starttown.cpp
// (CitizenCarryStorageInit), all on the FOUNDING colonists. No birth path calls it, so
// every citizen born after world start kept capacity 0 forever. Capacity 0 makes
// PickupStoredRawMaterial request (0 - 0)/unitVolume = 0 items, the only rejection is
// `available < requested` which 0 passes, and AddItem (0x14012d000) then appends a
// {resType, 0} stack. Pf_SpillAllStorage_7570's exit test is the stack COUNT at +0x30
// while its work is gated on the entry's own count, so a zero-count stack spins it
// forever — an in-world FREEZE, first reached by the pasture cull (2026-07-31). That
// spill loop is byte-faithful to retail 0x1400c7570; retail relies on a zero-count stack
// never existing, which is exactly what this slot guarantees.
//
// Buildings are unaffected: Draw's own `desc+0x21 (_available) == 0` gate short-circuits
// for a barn, whose capacity instead comes from the build-complete init
// (TownStorage_BuildCompleteInit_7ff0), which deliberately does NOT test that byte.
// COMPONENT_LIVE_FACTORY_ONADDED(StorageComponent, 0x40, 0x81,
//                                "BANISHED_LIVE_STORAGE_ONADDED", StorageComponent_Draw)
// hand-expanded (save/load wave 2026-08-27) to ALSO wire slot 3 (Shutdown/Serialize,
// 0x14012c730). The body has been ported in this file since the original storage wave
// but nothing ever wrote it into the live vtable, so a loaded save's StorageComponent
// kept the FillTypedVtable base-Component serialize (owner IBP + capability flags
// only) and never restored the +0x28 slot array / +0x38 capacity / +0x3c default type
// — the row array (+0x50 per readers, i.e. slot-array header) stayed null, and the
// first post-load QueryCount/PickupStoredRawMaterial call on that workplace crashed.
// Wired UNCONDITIONALLY (not behind the ONADDED env var): serialize round-trip is not
// an opt-in feature the way the Draw rebuild is.
static uint32_t StorageComponent_LiveGetTypeId(void*) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("StorageComponent");
    return cached;
}
static void* StorageComponent_LiveVtable[::ComponentFactory::kInertVtableSlots];
static void StorageComponent_OnAddedVSlot(void* s) {
    StorageComponent_Draw(reinterpret_cast<StorageComponent*>(s));
}
static void StorageComponent_ShutdownVSlot(void* self, void* stream) {
    StorageComponent_Shutdown(reinterpret_cast<StorageComponent*>(self), stream);
}
static void StorageComponent_FinishLiveVtable() {
    if (::ComponentFactory::LiveEnabled("BANISHED_LIVE_STORAGE_ONADDED"))
        StorageComponent_LiveVtable[6] = reinterpret_cast<void*>(&StorageComponent_OnAddedVSlot);
    StorageComponent_LiveVtable[3] = reinterpret_cast<void*>(&StorageComponent_ShutdownVSlot);
}
extern "C" void* StorageComponent_Factory(void) {
    void* p = ::ComponentFactory::MakeInertTyped(0x40, 0x81,
        StorageComponent_LiveVtable,
        reinterpret_cast<void*>(&StorageComponent_LiveGetTypeId));
    StorageComponent_FinishLiveVtable();
    return p;
}
static void* StorageComponent_CreateInPlace(void* mem) {
    if (mem) ::ComponentFactory::InitInertTyped(mem, 0x40, 0x81,
        StorageComponent_LiveVtable,
        reinterpret_cast<void*>(&StorageComponent_LiveGetTypeId));
    StorageComponent_FinishLiveVtable();
    return mem;
}
REGISTER_TYPE(StorageComponent, "Component",
              &StorageComponent_Factory, &StorageComponent_CreateInPlace);
