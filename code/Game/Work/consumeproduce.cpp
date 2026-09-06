// code/Game/Work/consumeproduce.cpp
// Clean C++ reimplementation of ConsumeProduce Work class.
// Source: Game/Work/ConsumeProduce.cpp (from profile build debug strings)
//
// ConsumeProduce -- consume/produce work cycle. Size = 0x78.
// Inherits: Object::Internal -> Work (0x48) -> ConsumeProduce
// Ctor: 0x1400e9340, Vtable: 0x140161220
//
// Methods implemented:
//   Ctor (0x1400e9340), ScalarDtor (0x14002a190), Serialize (0x1400e9020)
//   Execute (vtable slot 4, 0x1400e7840)
//   OnComplete (vtable slot 6, 0x1400e77c0)
//   OnCancel (vtable slot 7, 0x1400e7780)
//   Cleanup (vtable slot 8, 0x1400e8f30)
//   Validate (vtable slot 9, 0x1400e7530)
//   GetTarget (vtable slot 10, 0x1400e8fb0)
//

#include "Work/consumeproduce_funcs.h"
#include "BuildActions.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdio>
#include <cstdint>
#include <cstring>

// ============================================================
// Local function pointers resolved at init time
// (only those NOT already declared in engine_funcs.h)
// ============================================================

// Object::Internal::Internal(this) -- via IAT at 0x140152768
static void* (*s_fn_ObjectInternal_Ctor)(void* self) = nullptr;

// Object::Internal::~Internal(this) -- via IAT at 0x140152770
static void (*s_fn_ObjectInternal_Dtor)(void* self) = nullptr;

// Work::Serialize(this, stream) -- base class serialize at 0x1400f7400
static void (*s_fn_Work_Serialize)(void* self, void* stream) = nullptr;

// IO::Serializer ctor/dtor -- via IAT
static void (*s_fn_Serializer_Ctor)(void* self, void* stream, int version) = nullptr;
static void (*s_fn_Serializer_Dtor)(void* self) = nullptr;

// Object::InternalBasePtr::Serialize(this, serializer) -- via IAT at 0x140152aa0
static void (*s_fn_IBP_Serialize)(void* basePtr, void* serializer) = nullptr;

// Object::Type::Serialize(type*, serializer) -- via IAT at 0x1401516b0
static void (*s_fn_Type_Serialize)(void* type, void* serializer) = nullptr;

// IO::Serializer::Serialize(serializer, count, intBuffer, name) -- bulk int serialize
// via IAT at 0x140151278
static void (*s_fn_Serializer_SerializeIntBulk)(void* ser, int count, void* buf, const char* name) = nullptr;

// FUN_1400032e0 -- resize IBP array (8-byte elements)
static void (*s_fn_ResizeIBPArray)(void* arrayField, int newCount) = nullptr;

// FUN_1400021d0 -- resize int array (4-byte elements)
static void (*s_fn_ResizeIntArray)(void* arrayField, int newCount) = nullptr;

// FUN_1400f7100 -- Work::ValidateTask helper
static uint8_t (*s_fn_Work_ValidateTask)(void* self, void* param2, void* ibp, void** outTask) = nullptr;

// FUN_1400c5400 -- task list cleanup (removes pending work entries)
static void (*s_fn_TaskListCleanup)(void* taskList) = nullptr;

// FUN_140002e10 -- FindComponent (type DWORD_1401c80e0) on entity, returns IBP
// Signature: (entity, outIBP*) -> outIBP* (member function ABI: RCX=entity, RDX=outIBP)
static void* (*s_fn_FindPositionComponent)(void* entity, void* outIBP) = nullptr;

// TypeId at DAT_1401c7158 -- used for IsDerivedFrom check in Serialize
static uint32_t* s_typeId_1401c7158 = nullptr;

// Cached vtable addresses
static void* s_vtable_Work = nullptr;
static void* s_vtable_ConsumeProduce = nullptr;

// FUN_14012c3f0 -- ModelComponent::SetActiveState(component, stateIndex, enabled)
// Toggles visibility state bits on a model/mesh component.
typedef void (*s_fn_ModelComp_SetActiveState_t)(void* component, uint8_t stateIndex, char enabled);
static s_fn_ModelComp_SetActiveState_t s_fn_ModelComp_SetActiveState = nullptr;

// FUN_1401193a0 -- ConsumeProduceAction_helper_93a0(stateMachineComp)
// Checks if production count is below threshold. Returns bool-as-uint64.
typedef uint64_t (*s_fn_CheckProduceThreshold_t)(uintptr_t smComponent);
static s_fn_CheckProduceThreshold_t s_fn_CheckProduceThreshold = nullptr;

// FUN_14012b310 -- ConsumeProduceAction_helper_b310(stateMachineComp)
// Resets production state, sets field +0x48 = 1, transitions state machine.
typedef void (*s_fn_ResetProductionState_t)(uintptr_t smComponent);
static s_fn_ResetProductionState_t s_fn_ResetProductionState = nullptr;

// FUN_14009c480 -- Map_RecalcZones(mapGrid, vec3, param3, param4)
typedef void (*s_fn_Map_RecalcZones_t)(int* mapGrid, float* vec3, uint64_t param3, uint32_t param4);
static s_fn_Map_RecalcZones_t s_fn_Map_RecalcZones = nullptr;

// FUN_1400bef70 -- SetupConsumeProduceWorkChain (13 params)
// Creates the action chain for resource hauling + production work.
typedef void (*s_fn_SetupWorkChain_t)(
    void* citizenIBP, int priority, void* entityIBP, float* destPos,
    float heading, uintptr_t componentData, uint64_t productType,
    uint32_t flags, int* actionRef, uint32_t field54, int slotIndex,
    float produceTime, uint32_t packedParams);
static s_fn_SetupWorkChain_t s_fn_SetupWorkChain = nullptr;

static bool s_resolved = false;

// ============================================================
// Resolution -- called once
// ============================================================
bool ConsumeProduce_ResolveFuncs() {
    if (s_resolved) return true;

    // Object::Internal ctor/dtor via IAT
    uintptr_t iatCtor = ResolveAddr(Module::Exe, 0x140152768);
    uintptr_t iatDtor = ResolveAddr(Module::Exe, 0x140152770);
    if (iatCtor) s_fn_ObjectInternal_Ctor = *reinterpret_cast<void*(**)(void*)>(iatCtor);
    if (iatDtor) s_fn_ObjectInternal_Dtor = *reinterpret_cast<void(**)(void*)>(iatDtor);

    // Work::Serialize
    s_fn_Work_Serialize = reinterpret_cast<void(*)(void*, void*)>(
        ResolveAddr(Module::Exe, 0x1400f7400));

    // IO::Serializer ctor/dtor via IAT
    uintptr_t iatSerCtor = ResolveAddr(Module::Exe, 0x140152ad8);
    uintptr_t iatSerDtor = ResolveAddr(Module::Exe, 0x140152ad0);
    if (iatSerCtor) s_fn_Serializer_Ctor = *reinterpret_cast<void(**)(void*, void*, int)>(iatSerCtor);
    if (iatSerDtor) s_fn_Serializer_Dtor = *reinterpret_cast<void(**)(void*)>(iatSerDtor);

    // InternalBasePtr::Serialize via IAT at 0x140152aa0
    uintptr_t iatIBPSer = ResolveAddr(Module::Exe, 0x140152aa0);
    if (iatIBPSer) s_fn_IBP_Serialize = *reinterpret_cast<void(**)(void*, void*)>(iatIBPSer);

    // Object::Type::Serialize -- via IAT at 0x1401516b0
    uintptr_t iatTypeSer = ResolveAddr(Module::Exe, 0x1401516b0);
    if (iatTypeSer) s_fn_Type_Serialize = *reinterpret_cast<void(**)(void*, void*)>(iatTypeSer);

    // IO::Serializer::Serialize bulk ints -- via IAT at 0x140151278
    uintptr_t iatBulkSer = ResolveAddr(Module::Exe, 0x140151278);
    if (iatBulkSer) s_fn_Serializer_SerializeIntBulk = *reinterpret_cast<void(**)(void*, int, void*, const char*)>(iatBulkSer);

    // Array resize helpers
    s_fn_ResizeIBPArray = reinterpret_cast<void(*)(void*, int)>(
        ResolveAddr(Module::Exe, 0x1400032e0));
    s_fn_ResizeIntArray = reinterpret_cast<void(*)(void*, int)>(
        ResolveAddr(Module::Exe, 0x1400021d0));

    // Work::ValidateTask (FUN_1400f7100)
    s_fn_Work_ValidateTask = reinterpret_cast<uint8_t(*)(void*, void*, void*, void**)>(
        ResolveAddr(Module::Exe, 0x1400f7100));

    // Task list cleanup (FUN_1400c5400)
    s_fn_TaskListCleanup = reinterpret_cast<void(*)(void*)>(
        ResolveAddr(Module::Exe, 0x1400c5400));

    // FindPositionComponent (FUN_140002e10)
    s_fn_FindPositionComponent = reinterpret_cast<void*(*)(void*, void*)>(
        ResolveAddr(Module::Exe, 0x140002e10));

    // TypeId for IsDerivedFrom check in Serialize
    s_typeId_1401c7158 = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7158));

    // Vtables
    s_vtable_Work = (void*)ResolveAddr(Module::Exe, 0x140161a80);
    s_vtable_ConsumeProduce = (void*)ResolveAddr(Module::Exe, ConsumeProduceActionAddr::Vtable);

    // Execute-specific helpers
    s_fn_ModelComp_SetActiveState = reinterpret_cast<s_fn_ModelComp_SetActiveState_t>(
        ResolveAddr(Module::Exe, 0x14012c3f0));

    s_fn_CheckProduceThreshold = reinterpret_cast<s_fn_CheckProduceThreshold_t>(
        ResolveAddr(Module::Exe, 0x1401193a0));

    s_fn_ResetProductionState = reinterpret_cast<s_fn_ResetProductionState_t>(
        ResolveAddr(Module::Exe, 0x14012b310));

    s_fn_Map_RecalcZones = reinterpret_cast<s_fn_Map_RecalcZones_t>(
        ResolveAddr(Module::Exe, 0x14009c480));

    s_fn_SetupWorkChain = reinterpret_cast<s_fn_SetupWorkChain_t>(
        ResolveAddr(Module::Exe, 0x1400bef70));

    bool ok = s_fn_ObjectInternal_Ctor && s_fn_ObjectInternal_Dtor &&
              s_fn_Work_Serialize && s_fn_Serializer_Ctor && s_fn_Serializer_Dtor &&
              s_fn_IBP_Serialize && s_fn_Type_Serialize &&
              s_fn_Serializer_SerializeIntBulk &&
              s_fn_ResizeIBPArray && s_fn_ResizeIntArray &&
              s_fn_Work_ValidateTask && s_fn_TaskListCleanup &&
              s_fn_FindPositionComponent &&
              s_typeId_1401c7158 &&
              s_vtable_Work && s_vtable_ConsumeProduce &&
              s_fn_ModelComp_SetActiveState && s_fn_CheckProduceThreshold &&
              s_fn_ResetProductionState && s_fn_Map_RecalcZones &&
              s_fn_SetupWorkChain &&
              fn_InternalBasePtr_Ctor && fn_InternalBasePtr_CheckPtr &&
              fn_IsDerivedFrom && fn_MemFreeStatic &&
              g_fn_Serialize_Int && g_fn_Serialize_Bool &&
              fn_GetActiveProductIndex &&
              fn_FindStorageForResource && fn_Map_CheckPassable &&
              fn_Storage_FindSlot && fn_Random_Rand &&
              fn_Entity_GetPosition && fn_Entity_FindComponent_Position &&
              fn_CitizenAI_LongTermPlan && fn_TryAssignWorkplace &&
              fn_Pathfinding_FindPath &&
              g_pGameWorldSingleton &&
              g_TypeId_ConsumeProduceComponent &&
              g_TypeId_StatusIconComponent &&
              g_TypeId_ConsumeProduceComponent_FC &&
              g_TypeId_StorageComponent_FC &&
              g_TypeId_RawMaterialDescription &&
              g_TypeId_WorkComponent;

#ifdef _DEBUG
    printf("[ConsumeProduce] ResolveFuncs: ObjCtor=%p ObjDtor=%p WorkSer=%p\n",
           (void*)s_fn_ObjectInternal_Ctor, (void*)s_fn_ObjectInternal_Dtor,
           (void*)s_fn_Work_Serialize);
    printf("[ConsumeProduce]   SerCtor=%p SerDtor=%p IBPSer=%p TypeSer=%p BulkSer=%p\n",
           (void*)s_fn_Serializer_Ctor, (void*)s_fn_Serializer_Dtor,
           (void*)s_fn_IBP_Serialize, (void*)s_fn_Type_Serialize,
           (void*)s_fn_Serializer_SerializeIntBulk);
    printf("[ConsumeProduce]   ResizeIBP=%p ResizeInt=%p ValidateTask=%p TaskClean=%p\n",
           (void*)s_fn_ResizeIBPArray, (void*)s_fn_ResizeIntArray,
           (void*)s_fn_Work_ValidateTask, (void*)s_fn_TaskListCleanup);
    printf("[ConsumeProduce]   FindPos=%p typeId=%p vtWork=%p vtCP=%p\n",
           (void*)s_fn_FindPositionComponent, (void*)s_typeId_1401c7158,
           s_vtable_Work, s_vtable_ConsumeProduce);
    printf("[ConsumeProduce]   SetActiveState=%p CheckThresh=%p ResetProd=%p\n",
           (void*)s_fn_ModelComp_SetActiveState, (void*)s_fn_CheckProduceThreshold,
           (void*)s_fn_ResetProductionState);
    printf("[ConsumeProduce]   RecalcZones=%p SetupWorkChain=%p\n",
           (void*)s_fn_Map_RecalcZones, (void*)s_fn_SetupWorkChain);
    if (!ok) printf("[ConsumeProduce] ERROR: some function pointers not resolved!\n");
#endif

    s_resolved = ok;
    return ok;
}

// ============================================================
// Constructor (0x1400e9340)
// Object::Internal::Internal() -> Work fields -> ConsumeProduce vtable + fields
// ============================================================
void* ConsumeProduce_Ctor_Impl(ConsumeProduceAction* self) {
    // Initialize Internal base
    s_fn_ObjectInternal_Ctor(self);

    // Set Work vtable (intermediate step, matches original)
    self->vtable = s_vtable_Work;

    // Initialize Work base fields
    self->field_10 = nullptr;
    self->field_18 = nullptr;
    self->field_20 = -1;
    self->field_24 = 0;
    self->field_28 = -1;
    self->field_2c = 0;
    self->field_30 = nullptr;
    self->field_38 = nullptr;
    self->field_40 = 0;

    // Set ConsumeProduce vtable
    self->vtable = s_vtable_ConsumeProduce;

    // ConsumeProduce-specific fields
    // Note: field_48 (bool) and field_4c/field_54 (ints) are NOT initialized by ctor
    self->field_50 = 0;        // Type
    self->field_58 = nullptr;  // IBP DynArray data ptr
    self->field_60 = 0;        // IBP DynArray count
    self->field_64 = 0;        // IBP DynArray capacity
    self->field_68 = nullptr;  // int DynArray data ptr
    self->field_70 = 0;        // int DynArray count
    self->field_74 = 0;        // int DynArray capacity

    return self;
}

// ============================================================
// ScalarDtor (0x14002a190)
// Calls BaseDtor (0x1400e9190) which frees arrays and resets vtable chain,
// then optionally frees memory.
// ============================================================
void* ConsumeProduce_ScalarDtor_Impl(ConsumeProduceAction* self, uint32_t flags) {
    // BaseDtor body (0x1400e9190):
    // Set ConsumeProduce vtable (for correct virtual dispatch during destruction)
    self->vtable = s_vtable_ConsumeProduce;

    // Free the int DynArray at +0x68 (original uses 8-byte writes to zero count+capacity)
    fn_MemFreeStatic(self->field_68);
    self->field_68 = nullptr;
    self->field_70 = 0;
    self->field_74 = 0;

    // Free the IBP DynArray at +0x58 (original uses 8-byte writes to zero count+capacity)
    fn_MemFreeStatic(self->field_58);
    self->field_58 = nullptr;
    self->field_60 = 0;
    self->field_64 = 0;

    // Set Work vtable (base class destructor phase)
    self->vtable = s_vtable_Work;

    // Call Object::Internal::~Internal (tail-called in original via jmp)
    s_fn_ObjectInternal_Dtor(self);

    // Free memory if requested (from ScalarDtor wrapper)
    if (flags & 1) {
        fn_MemFreeStatic(self);
    }

    return self;
}

// ============================================================
// Serialize (0x1400e9020)
// Work::Serialize + Serializer(version=0) + IBP array + int array + fields
// ============================================================
void ConsumeProduce_Serialize_Impl(ConsumeProduceAction* self, void* stream) {
    // Call base Work::Serialize
    s_fn_Work_Serialize(self, stream);

    // Create IO::Serializer with version 0
    uint8_t serializerBuf[24];
    s_fn_Serializer_Ctor(serializerBuf, stream, 0);

    // Get the serializer's internal stream ptr (at offset +0x00)
    // Check if loading (stream+8 == 0 means loading)
    uintptr_t serStreamPtr = *(uintptr_t*)serializerBuf;
    bool isLoading = (*(int*)(serStreamPtr + 8) == 0);

    // --- Serialize IBP array count (+0x60) ---
    // Use a local copy like the original does (original serializes into a stack local,
    // not directly into self->field_60). This avoids issues if ResizeIBPArray
    // modifies field_60 as a side-effect.
    int ibpCount = self->field_60;
    g_fn_Serialize_Int(serializerBuf, &ibpCount, nullptr);

    // If loading, resize the IBP array to match deserialized count
    if (isLoading) {
        s_fn_ResizeIBPArray((void*)&self->field_58, ibpCount);
    }

    // Serialize each InternalBasePtr in the array
    uintptr_t* ibpArray = (uintptr_t*)self->field_58;
#ifdef _DEBUG
    if (ibpCount > 0 && ibpArray == nullptr) {
        printf("[ConsumeProduce::Serialize] BUG: ibpCount=%d but ibpArray=NULL! self=%p isLoading=%d field_58=%p field_60=%d\n",
               ibpCount, (void*)self, (int)isLoading, (void*)self->field_58, self->field_60);
    }
#endif
    for (int i = 0; i < ibpCount; i++) {
        s_fn_IBP_Serialize(&ibpArray[i], serializerBuf);

        // After deserialize, validate the entity type.
        //
        // GUARDED because standalone GENUINELY DIFFERS from retail here, not to paper
        // over a faithful crash: in retail DAT_1401c7158 is always a live CitizenComponent
        // type id, but in our build that .rdata slot is registered only when
        // BANISHED_LIVE_CITIZENAI_CITID is set. That flag GRADUATED 2026-08-26 and is now
        // default-on (kill-switch =0), so the common case is a live id — but the guard
        // stays, because the kill-switch still produces a NULL slot and this is
        // Serialize-side, which autostart never reaches, so a regression here would only
        // ever surface as a save/load crash. See standalone/reg_citizenai_typeids.cpp.
        //
        // When the id is unavailable we KEEP the ref rather than clearing it. Retail's
        // intent is "drop refs that are not citizen entities"; with no id we cannot tell,
        // and these refs are citizens in the overwhelming majority, so clearing them would
        // destroy live state — strictly worse than skipping a check we cannot perform.
        // Same both-pointer-and-value shape as completetask.cpp's reader.
        void* ptr = (void*)ibpArray[i];
        if (ptr != nullptr && s_typeId_1401c7158 && *s_typeId_1401c7158) {
            bool valid = fn_IsDerivedFrom(ptr, *s_typeId_1401c7158);
            if (!valid) {
                ibpArray[i] = 0;  // null out invalid refs
            }
        }
    }

    // --- Serialize int array count (+0x70) ---
    int intCount = self->field_70;
    g_fn_Serialize_Int(serializerBuf, &intCount, nullptr);

    // If loading, resize the int array
    if (isLoading) {
        s_fn_ResizeIntArray((void*)&self->field_68, intCount);
    }

    // Bulk-serialize the int array
    // Original: IO::Serializer::Serialize(serializer, count, intArrayPtr, name)
    s_fn_Serializer_SerializeIntBulk(serializerBuf, intCount, self->field_68, nullptr);

    // --- Serialize ConsumeProduce fields ---
    g_fn_Serialize_Bool(serializerBuf, &self->field_48, nullptr);   // bool at +0x48
    g_fn_Serialize_Int(serializerBuf, &self->field_4c, nullptr);    // int at +0x4C
    s_fn_Type_Serialize(&self->field_50, serializerBuf);            // Type at +0x50
    g_fn_Serialize_Int(serializerBuf, &self->field_54, nullptr);    // int at +0x54

    s_fn_Serializer_Dtor(serializerBuf);
}

// ============================================================
// Helpers for Execute
// ============================================================

// Find a component by type ID on an entity (iterates component list at +0x30/+0x38).
// Returns the component pointer, or nullptr if not found.
// Find a DESCRIPTION by type id in an object's child list at +0x28 (count +0x30).
//
// This is NOT the same walk as FindComponentOnEntity above, and the difference is
// load-bearing: a ComponentEntity keeps its COMPONENT array at +0x30 with the count at
// +0x38, while a description keeps its CHILD list at +0x28 with the count at +0x30. A
// StorageComponent slot record holds a DESCRIPTION in its first qword, so the material
// lookup has to use this form. Retail inlines exactly this loop at 0x1400e7d0f
// (`*(int*)(uVar11 + 0x30)` / `*(longlong*)(uVar11 + 0x28)`), and HappinessComponent
// ::IsActive walks the same slot the same way.
//
// Reusing the component-list finder here is what made Execute fault on its very first
// live pass once slot-18 CreateWork started minting work: reading the count from
// matEntity+0x38 and the array from +0x30 landed inside the slot's (capacity, stored)
// int pair, so the "component pointer" came back as 0x0000000500000005 -- two adjacent
// int32 5s -- and the vtable read off it was the access violation.
static void* FindDescriptionOnObject(uintptr_t obj, uint32_t typeId) {
    int count = *(int*)(obj + 0x30);
    uintptr_t arr = *(uintptr_t*)(obj + 0x28);
    for (int i = 0; i < count; i++) {
        void* c = *(void**)(arr + (uintptr_t)i * 8);
        uint32_t t = (*(uint32_t(**)(void*))(*(uintptr_t*)c + 0x10))(c);
        if (t == typeId) {
            return fn_IsDerivedFrom(c, typeId) ? c : nullptr;
        }
    }
    return nullptr;
}

static void* FindComponentOnEntity(uintptr_t entity, uint32_t typeId) {
    int count = *(int*)(entity + 0x38);
    uintptr_t compArray = *(uintptr_t*)(entity + 0x30);
    for (int i = 0; i < count; i++) {
        uintptr_t* compPtr = (uintptr_t*)(compArray + (uintptr_t)i * 8);
        fn_InternalBasePtr_CheckPtr(compPtr);
        uintptr_t comp = *compPtr;
        // Call vtable slot +0x10 to get component type ID
        uint32_t compType = (*(uint32_t(**)(void*))(*(uintptr_t*)comp + 0x10))((void*)comp);
        if (compType == typeId) {
            fn_InternalBasePtr_CheckPtr(compPtr);
            void* found = (void*)*compPtr;
            if (fn_IsDerivedFrom(found, typeId)) {
                return found;
            }
            break;
        }
    }
    return nullptr;
}

// Finalize action: call vtable +0x50, set citizen state flags, store citizen ref.
static int FinalizeAction(ConsumeProduceAction* self, void* entityIBP, void* actionIBP, int priority, int slotIndex) {
    fn_InternalBasePtr_CheckPtr(entityIBP);
    uintptr_t citizenEntity = *(uintptr_t*)entityIBP;

    // Call vtable slot +0x50 (Shared method)
    uintptr_t vtable = *(uintptr_t*)self;
    typedef uint64_t* (*VtSlot50_t)(ConsumeProduceAction*, void*);
    VtSlot50_t vtShared = *(VtSlot50_t*)(vtable + 0x50);
    uint64_t* result = vtShared(self, actionIBP);

    uint64_t actionPtr = *result;
    *(uint32_t*)(citizenEntity + 0x54) |= 4;
    *(int32_t*)(citizenEntity + 0x30) = priority;
    *(int32_t*)(citizenEntity + 0xb0) = 0;

    if (actionPtr == *(uint64_t*)(citizenEntity + 0x1f0) && priority >= 0) {
        *(uint32_t*)(citizenEntity + 0x88) |= 0x20000;
    }

    // Store citizen entity ref at self->field_58[slotIndex]
    uintptr_t slotArray = (uintptr_t)self->field_58;
    *(uintptr_t*)(slotArray + (int64_t)slotIndex * 8) = *(uintptr_t*)entityIBP;
    return 2;
}

// ============================================================
// Execute (0x1400e7840) -- vtable slot 4
//
// Main production work logic. Returns 1 (retry) or 2 (action pushed).
// ============================================================
int ConsumeProduce_Execute_Impl(ConsumeProduceAction* self, void* entityIBP) {
    uintptr_t gameWorld = (uintptr_t)*g_pGameWorldSingleton;
    int* mapGrid = *(int**)(gameWorld + 0xf8);
    void** worldEntityList = *(void***)(gameWorld + 0xf0);

    void* entityFieldPtr = (void*)((uintptr_t)self + 0x10);  // self->field_10 as IBP

    // --- Find PositionComponent on work entity ---
    uintptr_t posIBP = 0;
    fn_InternalBasePtr_CheckPtr(entityFieldPtr);
    fn_Entity_FindComponent_Position(*(uintptr_t*)entityFieldPtr, &posIBP);

    // --- Find ConsumeProduceComponent (type 0x1401c7350) on work entity ---
    uintptr_t smCompIBP = 0;
    fn_InternalBasePtr_CheckPtr(entityFieldPtr);
    fn_FindComponent_ConsumeProduce((void*)*(uintptr_t*)entityFieldPtr, &smCompIBP);

    // --- Get ConsumeProduceDescription from ConsumeProduceComponent's entity +0x18 ---
    fn_InternalBasePtr_CheckPtr(&smCompIBP);
    void* descBase = *(void**)(smCompIBP + 0x18);
    bool isDerived = fn_IsDerivedFrom(descBase, *g_TypeId_ConsumeProduceComponent);
    void* cpDesc = isDerived ? descBase : nullptr;  // ConsumeProduceDescription*

    // --- Find Unknown7ee8 component (for storage slot lookup) ---
    uintptr_t unkCompIBP = 0;
    fn_InternalBasePtr_CheckPtr(entityFieldPtr);
    fn_FindComponent_Unknown7ee8((void*)*(uintptr_t*)entityFieldPtr, &unkCompIBP);

    // --- Check resource depletion (bVar6) ---
    // Get active product index from ConsumeProduce component
    bool resourcesDepleted = false;
    fn_InternalBasePtr_CheckPtr(&smCompIBP);
    int activeProductIdx = fn_GetActiveProductIndex((void*)smCompIBP);
    fn_InternalBasePtr_CheckPtr(&smCompIBP);

    uintptr_t cpDescAddr = (uintptr_t)cpDesc;

    if (*(int*)(smCompIBP + 0x50) < 0) {
        // Multi-product mode: check ALL products for depletion
        int productCount = *(int*)(cpDescAddr + 0x30);
        if (productCount > 0) {
            int* productPtr = (int*)(*(uintptr_t*)(cpDescAddr + 0x28) + 0x18);
            for (int i = 0; i < productCount; i++) {
                int materialType = *productPtr;
                productPtr += 10;  // stride = 0x28 bytes = 10 ints
                // Check world material supply vs demand
                uintptr_t* matArray = (uintptr_t*)worldEntityList[0x2e];
                int supply = (int)matArray[(int64_t)materialType * 4];
                int demand = (int)matArray[(int64_t)materialType * 4 + 1];
                if (demand <= supply) {
                    resourcesDepleted = true;
                }
            }
        }
    } else {
        // Single product mode: check specific active product
        uintptr_t productData = *(uintptr_t*)(cpDescAddr + 0x28);
        int materialType = *(int*)(productData + 0x18 + (int64_t)activeProductIdx * 0x28);
        uintptr_t* matArray = (uintptr_t*)worldEntityList[0x2e];
        int supply = (int)matArray[materialType * 4];
        int demand = (int)matArray[materialType * 4 + 1];
        resourcesDepleted = (demand <= supply);
    }

    // --- Find HappinessComponent (type 0x1401c7e08) on work entity ---
    fn_InternalBasePtr_CheckPtr(entityFieldPtr);
    uintptr_t entity = *(uintptr_t*)entityFieldPtr;
    void* meshComp = FindComponentOnEntity(entity, *g_TypeId_StatusIconComponent);

    uintptr_t meshCompIBP = 0;
    fn_InternalBasePtr_Ctor(&meshCompIBP, meshComp);

    // Set active state = 2, enabled = resourcesDepleted
    fn_InternalBasePtr_CheckPtr(&meshCompIBP);
    s_fn_ModelComp_SetActiveState((void*)meshCompIBP, 2, resourcesDepleted ? '\x01' : '\0');

    // --- Find ConsumeProduceComponent (type 0x1401c7350) for produce check ---
    fn_InternalBasePtr_CheckPtr((void*)((uintptr_t)self + 0x10));
    entity = *(uintptr_t*)((uintptr_t)self + 0x10);
    void* smComp2 = FindComponentOnEntity(entity, *g_TypeId_ConsumeProduceComponent_FC);

    uintptr_t smComp2IBP = 0;
    fn_InternalBasePtr_Ctor(&smComp2IBP, smComp2);

    // Check produce threshold
    fn_InternalBasePtr_CheckPtr(&smComp2IBP);
    uint64_t canProduce = s_fn_CheckProduceThreshold(smComp2IBP);
    // [CPWork] the gate that decides whether the tavern is allowed to make more at
    // all (retail 0x1401193a0 -- a stock-vs-limit test against the "Alcohol Limit 500"
    // the panel shows). Printed on CHANGE only; a permanent 0 here means the work runs
    // forever and produces nothing, which reads exactly like a broken produce path.
    {
        static int n = 0; static int last = -1;
        const int cp = (int)(char)canProduce;
        if (n < 8 && cp != last) { ++n; last = cp;
            fprintf(stderr, "[CPWork] canProduce(threshold)=%d depleted=%d\n",
                    cp, resourcesDepleted ? 1 : 0);
            fflush(stderr);
        }
    }

    // ============================================================
    // BRANCH: Not depleted
    // ============================================================
    if (!resourcesDepleted) {
        if ((char)canProduce == '\0') {
            goto handle_depleted_or_idle;
        }

        // --- Pick random empty output slot ---
        void* randomPtr = *(void**)(gameWorld + 0xd8);
        uint32_t startSlot = fn_Random_Rand(randomPtr, (uint32_t)self->field_60);
        int totalSlots = self->field_60;
        uint32_t slot = startSlot;

        if (totalSlots < 1) {
            return 1;
        }

        // Find an empty slot (field_58[slot] == 0)
        int attempts = 0;
        while (*(uintptr_t*)((uintptr_t)self->field_58 + (uintptr_t)slot * 8) != 0) {
            slot++;
            if ((int)slot >= totalSlots) {
                slot = 0;
            }
            attempts++;
            if (attempts >= totalSlots) {
                return 1;  // All slots occupied
            }
        }

        if (slot == 0xffffffff) {
            return 1;
        }

        // --- Find StorageComponent on work entity ---
        fn_InternalBasePtr_CheckPtr((void*)((uintptr_t)self + 0x10));
        entity = *(uintptr_t*)((uintptr_t)self + 0x10);
        void* storageComp = FindComponentOnEntity(entity, *g_TypeId_StorageComponent_FC);

        uintptr_t storageCompIBP = 0;
        fn_InternalBasePtr_Ctor(&storageCompIBP, storageComp);

        // --- Setup loop vars for product iteration ---
        int64_t productStride = (int64_t)activeProductIdx * 5;
        uintptr_t productData = *(uintptr_t*)(cpDescAddr + 0x28);

        // Get citizen position data for cell index calculation
        fn_InternalBasePtr_CheckPtr(entityIBP);
        uintptr_t citizenEntity = *(uintptr_t*)entityIBP;
        fn_InternalBasePtr_CheckPtr((void*)(citizenEntity + 0x278));
        uintptr_t citizenSM = *(uintptr_t*)(citizenEntity + 0x278);
        fn_InternalBasePtr_CheckPtr((void*)(citizenSM + 0x68));
        uintptr_t citizenPos = *(uintptr_t*)(citizenSM + 0x68);

        (void)*(float*)(citizenPos + 0x160);  // posHeight -- read but not used in reimpl
        int mapSize = *mapGrid;
        int halfMap = mapSize >> 1;
        int cellX = (int)(*(float*)(citizenPos + 0x158) + (float)halfMap);
        int cellY = (int)(*(float*)(citizenPos + 0x15c) + (float)halfMap);
        if (cellX < 0) cellX = 0;
        if (cellX >= mapSize) cellX = mapSize - 1;
        if (cellY < 0) cellY = 0;
        if (cellY >= mapSize) cellY = mapSize - 1;
        int citizenCell = cellY * mapSize + cellX;

        // --- Iterate input materials for active product ---
        // [CPWork] Execute had NEVER run before slot-18 CreateWork was wired, so its
        // first live pass faulted here: FindComponentOnEntity was handed matEntity
        // 0x0000000500000005 -- two adjacent int32 5s, i.e. a (capacity,stored) pair read
        // where a good POINTER belongs. Print the storage the loop is actually walking so
        // it can be compared against the one HappinessComponent::IsActive reports for the
        // same building; if they differ, the StorageComponent finder is the fault, not the
        // slot stride. Budgeted, outcome-only.
        {
            static int n = 0;
            if (n < 6) { ++n;
                const uintptr_t sd = storageCompIBP ? *(uintptr_t*)(storageCompIBP + 0x28) : 0;
                fprintf(stderr, "[CPWork] Execute storageComp=%p ibp=%p data=%p count=%d "
                        "slot0=%p tid_storage=%u activeProduct=%d cpSel(+0x50)=%d\n",
                        storageComp, (void*)storageCompIBP, (void*)sd,
                        storageCompIBP ? *(int*)(storageCompIBP + 0x30) : -1,
                        sd ? *(void**)sd : nullptr,
                        g_TypeId_StorageComponent_FC ? *g_TypeId_StorageComponent_FC : 0u,
                        activeProductIdx, *(int*)(smCompIBP + 0x50));
                fflush(stderr);
            }
        }
        int inputCount = *(int*)(storageCompIBP + 0x30);
        if (inputCount > 0) {
            uintptr_t inputOffset = 0;
            for (int inputIdx = 0; inputIdx < inputCount; inputIdx++) {
                // Get material entity from storage component
                fn_InternalBasePtr_CheckPtr(&storageCompIBP);
                uintptr_t storageData = *(uintptr_t*)(storageCompIBP + 0x28);
                uintptr_t matEntity = *(uintptr_t*)(inputOffset + storageData);

                // Check if this material entity matches the required material for active product
                bool matMatches = (*(uintptr_t*)(productData + 0x10 + productStride * 8) == matEntity);
                if (!matMatches) {
                    int64_t matCheckCount = (int64_t)*(int*)(productData + 8 + productStride * 8);
                    if (matCheckCount > 0) {
                        uintptr_t* matCheckPtr = *(uintptr_t**)(productData + productStride * 8);
                        for (int64_t j = 0; j < matCheckCount; j++) {
                            if (matCheckPtr[j * 2] == matEntity) {
                                matMatches = true;
                                break;
                            }
                        }
                    }
                }

                if (matMatches) {
                    inputOffset += 0x10;
                    continue;
                }

                // Material doesn't match -- need to find and haul it
                // Get available amount from storage
                void* rawMatComp = FindDescriptionOnObject(matEntity, *g_TypeId_RawMaterialDescription);

                // Check remaining amount in storage: capacity - currently stored
                int available = *(int*)(inputOffset + 8 + storageData) - *(int*)(inputOffset + 0xc + storageData);

                // Find storage on citizen's home entity
                fn_InternalBasePtr_CheckPtr(entityIBP);
                citizenEntity = *(uintptr_t*)entityIBP;
                fn_InternalBasePtr_CheckPtr((void*)(citizenEntity + 0x10));
                uintptr_t homeEntity = *(uintptr_t*)(citizenEntity + 0x10);

                // Find StorageComponent on home entity
                void* homeStorage = FindComponentOnEntity(homeEntity, *g_TypeId_StorageComponent_FC);
                uintptr_t homeStorageIBP = 0;
                fn_InternalBasePtr_Ctor(&homeStorageIBP, homeStorage);

                // Check if home storage has capacity for the material
                fn_InternalBasePtr_CheckPtr(&homeStorageIBP);
                int homeCapacity = *(int*)(homeStorageIBP + 0x38);
                fn_InternalBasePtr_CheckPtr(&homeStorageIBP);
                int homeStored = fn_StorageComponent_QueryCount((void*)homeStorageIBP);

                if (homeCapacity - homeStored < available * *(int*)((uintptr_t)rawMatComp + 0x48)) {
                    // Not enough capacity -- check citizen's own storage
                    fn_InternalBasePtr_CheckPtr(entityIBP);
                    citizenEntity = *(uintptr_t*)entityIBP;
                    fn_InternalBasePtr_CheckPtr((void*)(citizenEntity + 0x10));
                    homeEntity = *(uintptr_t*)(citizenEntity + 0x10);

                    void* citizenStorage = FindComponentOnEntity(homeEntity, *g_TypeId_StorageComponent_FC);
                    uintptr_t citizenStorageIBP = 0;
                    fn_InternalBasePtr_Ctor(&citizenStorageIBP, citizenStorage);

                    fn_InternalBasePtr_CheckPtr(&citizenStorageIBP);
                    int cap2 = *(int*)(citizenStorageIBP + 0x38);
                    fn_InternalBasePtr_CheckPtr(&citizenStorageIBP);
                    int stored2 = fn_StorageComponent_QueryCount((void*)citizenStorageIBP);
                    available = (cap2 - stored2) / *(int*)((uintptr_t)rawMatComp + 0x48);
                }

                if (available == 0) {
                    // No resources available -- replan citizen
                    fn_InternalBasePtr_CheckPtr(entityIBP);
                    fn_CitizenAI_LongTermPlan(*(void**)entityIBP);
                    return 2;
                }

                // --- Search for storage with this resource ---
                float searchPos[3] = { 0.0f, 0.0f, 0.0f };

                // Get entity ref for pathfinding
                uintptr_t entityRef = *(uintptr_t*)entityFieldPtr;

                uintptr_t foundEntity = 0;
                fn_FindStorageForResource(
                    worldEntityList, &foundEntity,
                    *(uint32_t*)((uintptr_t)self + 0x28), matEntity,
                    available, '\x01', searchPos, (void*)((uintptr_t)self + 0x10));

                if (foundEntity != 0) {
                    // Check passability
                    uint64_t passable = fn_Map_CheckPassable(mapGrid, 0xe1f, (uint32_t)citizenCell,
                                                             *(uint32_t*)((uintptr_t)self + 0x28));
                    if ((char)passable == '\0') {
                        return 1;
                    }

                    // Get terrain position for storage
                    float terrainPos[4] = {};
                    if (unkCompIBP == 0) {
                        if (posIBP != 0) {
                            fn_InternalBasePtr_CheckPtr(&posIBP);
                            fn_Entity_GetPosition((void*)posIBP, (void*)&foundEntity, 0, 0, nullptr);
                            terrainPos[0] = *(float*)&foundEntity;
                            terrainPos[1] = ((float*)&foundEntity)[1];
                            terrainPos[2] = ((float*)&foundEntity)[2];
                        }
                    } else {
                        // Use storage slot lookup
                        int slotMapSize = *mapGrid;
                        int slotCellX = (int)(searchPos[0] + (float)(slotMapSize >> 1));
                        int slotCellY = (int)(searchPos[1] + (float)(slotMapSize >> 1));
                        if (slotCellX < 0) slotCellX = 0;
                        if (slotCellX >= slotMapSize) slotCellX = slotMapSize - 1;
                        if (slotCellY < 0) slotCellY = 0;
                        if (slotCellY >= slotMapSize) slotCellY = slotMapSize - 1;

                        fn_InternalBasePtr_CheckPtr(&unkCompIBP);
                        int slotResult = fn_Storage_FindSlot(
                            (void*)unkCompIBP, slotMapSize * slotCellY + slotCellX,
                            (float*)&terrainPos, *(uint32_t*)((uintptr_t)self + 0x28));
                        if (slotResult >= 0) {
                            // Set up pathfinding and create work action
                            uintptr_t destEntity = foundEntity;
                            uintptr_t destEntityRef = *(uintptr_t*)entityIBP;

                            fn_Pathfinding_FindPath(
                                &worldEntityList, &destEntityRef,
                                *(int*)((uintptr_t)self + 0x20), &destEntity,
                                searchPos, &entityRef, (float*)&terrainPos,
                                (int64_t)matEntity, available, slot, 0, '\x01', 0, 0);

                            int priority = *(int*)((uintptr_t)self + 0x20);
                            fn_InternalBasePtr_CheckPtr(entityIBP);
                            return FinalizeAction(self, entityIBP, &destEntityRef, priority, slot);
                        }
                    }
                }

                inputOffset += 0x10;
            }
        }

        // --- Second pass: check each input material for nearby storage ---
        bool allInputsAvailable = true;
        int inputMatCount = *(int*)(productData + 8 + productStride * 8);
        for (int matIdx = 0; matIdx < inputMatCount; matIdx++) {
            uintptr_t matEntity2 = *(uintptr_t*)(*(uintptr_t*)(productData + productStride * 8) + (uintptr_t)matIdx * 0x10);

            // Check if this material is already in our storage
            int existingAmount = 0;
            fn_InternalBasePtr_CheckPtr(&storageCompIBP);
            int storageSlotCount = *(int*)(storageCompIBP + 0x30);
            if (storageSlotCount > 0) {
                uintptr_t* storageSlots = *(uintptr_t**)(storageCompIBP + 0x28);
                for (int si = 0; si < storageSlotCount; si++) {
                    if (storageSlots[si * 2] == matEntity2) {
                        if (si >= 0) {
                            existingAmount = (int)storageSlots[si * 2 + 1] - *(int*)((uintptr_t)storageSlots + si * 0x10 + 0xc);
                        }
                        break;
                    }
                }
            }

            int required = *(int*)(*(uintptr_t*)(productData + productStride * 8) + 8 + (uintptr_t)matIdx * 0x10);
            if (existingAmount < required) {
                allInputsAvailable = false;

                // Search for this material in nearby entities
                float searchPos2[3] = { 0.0f, 0.0f, 0.0f };

                // Get building position for search
                fn_InternalBasePtr_CheckPtr(entityIBP);
                citizenEntity = *(uintptr_t*)entityIBP;
                fn_InternalBasePtr_CheckPtr((void*)(citizenEntity + 0x278));
                citizenSM = *(uintptr_t*)(citizenEntity + 0x278);
                fn_InternalBasePtr_CheckPtr((void*)(citizenSM + 0x68));
                uintptr_t bldgPos = *(uintptr_t*)(citizenSM + 0x68);

                mapSize = *mapGrid;
                halfMap = mapSize >> 1;
                int bx = (int)(*(float*)(bldgPos + 0x158) + (float)halfMap);
                int by = (int)(*(float*)(bldgPos + 0x15c) + (float)halfMap);
                if (bx < 0) bx = 0;
                if (bx >= mapSize) bx = mapSize - 1;
                if (by < 0) by = 0;
                if (by >= mapSize) by = mapSize - 1;

                uintptr_t foundStorage = 0;
                fn_FindStorageForResource(
                    worldEntityList, (void*)&foundStorage,
                    mapSize * by + bx, matEntity2, 1, '\0', searchPos2,
                    (void*)((uintptr_t)self + 0x10));

                if (foundStorage != 0) {
                    // Found storage -- check if it has the material available
                    fn_InternalBasePtr_CheckPtr(&foundStorage);
                    uintptr_t fStorage = foundStorage;
                    void* fStorageComp = FindComponentOnEntity(fStorage, *g_TypeId_StorageComponent_FC);
                    uintptr_t fStorageIBP = 0;
                    fn_InternalBasePtr_Ctor(&fStorageIBP, fStorageComp);

                    // Check if storage has the material
                    fn_InternalBasePtr_CheckPtr(&fStorageIBP);
                    int fSlotCount = *(int*)(fStorageIBP + 0x30);
                    if (fSlotCount > 0) {
                        uintptr_t* fSlots = *(uintptr_t**)(fStorageIBP + 0x28);
                        for (int fi = 0; fi < fSlotCount; fi++) {
                            if (fSlots[fi * 2] == matEntity2) {
                                if (fi >= 0 && (int)fSlots[fi * 2 + 1] != *(int*)((uintptr_t)fSlots + fi * 0x10 + 0xc)) {
                                    // Material available -- find storage on citizen's home entity
                                    fn_InternalBasePtr_CheckPtr(entityIBP);
                                    citizenEntity = *(uintptr_t*)entityIBP;
                                    fn_InternalBasePtr_CheckPtr((void*)(citizenEntity + 0x10));
                                    uintptr_t homeEnt2 = *(uintptr_t*)(citizenEntity + 0x10);
                                    void* homeStorage2 = FindComponentOnEntity(homeEnt2, *g_TypeId_StorageComponent_FC);
                                    uintptr_t homeStorage2IBP = 0;
                                    fn_InternalBasePtr_Ctor(&homeStorage2IBP, homeStorage2);

                                    fn_InternalBasePtr_CheckPtr(&homeStorage2IBP);
                                    int hs2Count = fn_StorageComponent_QueryCount((void*)homeStorage2IBP);
                                    if (hs2Count < 1) {
                                        // Storage empty -- set up path to fetch material
                                        uintptr_t fetchDest = foundStorage;
                                        fn_InternalBasePtr_CheckPtr(&posIBP);
                                        float fetchPos[4] = {};
                                        fn_Entity_GetPosition((void*)posIBP, fetchPos, 0, slot, nullptr);

                                        uintptr_t pathSrc = *(uintptr_t*)entityFieldPtr;

                                        // arg2 is the CITIZEN entity IBP, not the world entity
                                        // list. FindPath immediately does *(*arg2 + 0x278) to
                                        // reach the CitizenAIComponent and clean its task list;
                                        // handing it (GameWorld+0xf0) walked a non-canonical
                                        // "list" pointer and faulted in TaskListCleanup.
                                        // Retail (0x1400e87b9): [RBP] = *R12 (= *entityIBP) and
                                        // LEA RDX,[RBP] — the same &(*entityIBP) the sibling
                                        // call at 0x1400e83f4 passes.
                                        uintptr_t pathCitizen = *(uintptr_t*)entityIBP;

                                        fn_Pathfinding_FindPath(
                                            &foundStorage, &pathCitizen,
                                            *(int*)((uintptr_t)self + 0x20), &pathSrc,
                                            (float*)&fetchPos, &fetchDest, searchPos2,
                                            (int64_t)matEntity2, -1, slot, 1, '\0', 0, 0);

                                        int priority2 = *(int*)((uintptr_t)self + 0x20);
                                        fn_InternalBasePtr_CheckPtr(entityIBP);
                                        return FinalizeAction(self, entityIBP, &fetchDest, priority2, slot);
                                    }
                                }
                                break;
                            }
                        }
                    }
                }
            }
        }

        // --- Third pass: check assigned positions for validity ---
        // Retail 0x1400e7840 bounds BOTH position loops below by *(int*)(self+0x70) — the
        // status INT ARRAY's own count (param_1[0xe]) — not by the citizen-slot count at
        // +0x60 (param_1[0xc]). The port used field_60: a workplace with more worker slots
        // than produce positions (e.g. 20 vs 18) then read and WROTE statusArray[18..19],
        // which is the next heap block's boundary tag (+1 then +2 in its high dword, the
        // "free-list link with wrong high bits" teardown crash in the 800x10 stress run,
        // caught 2026-09-04 by a hardware watchpoint: Execute wrote 1, OnComplete wrote 2).
        int totalPositions = (int)self->field_70;  // number of output positions (= int array count)
        uint32_t firstFreePos = 0xffffffff;
        int firstFreeIndex = -1;
        uint32_t freeCount = 0;

        for (int posIdx = 0; posIdx < totalPositions; posIdx++) {
            fn_InternalBasePtr_CheckPtr(&posIBP);
            float posVec[3];
            fn_Entity_GetPosition((void*)posIBP, posVec, 2, posIdx, nullptr);

            mapSize = *mapGrid;
            halfMap = mapSize >> 1;
            int px = (int)(posVec[0] + (float)halfMap);
            int py = (int)(posVec[1] + (float)halfMap);
            if (px < 0) px = 0;
            if (px >= mapSize) px = mapSize - 1;
            if (py < 0) py = 0;
            if (py >= mapSize) py = mapSize - 1;
            int posCell = py * mapSize + px;

            // Check if position has status 2 but no structure
            int* statusArray = (int*)self->field_68;
            uintptr_t structBase = *(uintptr_t*)((uintptr_t)mapGrid + 0x80);
            if (statusArray[posIdx] == 2 &&
                *(uintptr_t*)(structBase + 8 + (int64_t)posCell * 0x30) == 0) {
                statusArray[posIdx] = 0;
            }

            if (statusArray[posIdx] == 0) {
                freeCount++;
                if ((int)firstFreePos < 0) {
                    firstFreePos = (uint32_t)posIdx;
                    firstFreeIndex = posCell;
                }
            }
        }

        // If all positions are free, mark as not producing
        if (freeCount == (uint32_t)totalPositions) {
            *(uint8_t*)((uintptr_t)self + 0x48) = 0;  // field_48 = false
        }

        // If no free position found, mark as producing
        if ((int)firstFreePos < 0) {
            *(uint8_t*)((uintptr_t)self + 0x48) = 1;  // field_48 = true
        } else if (*(char*)((uintptr_t)self + 0x48) == '\0' && allInputsAvailable) {
            goto setup_produce_action;
        }

        // --- Check each position for workplace assignment ---
        for (int posIdx2 = 0; posIdx2 < totalPositions; posIdx2++) {
            fn_InternalBasePtr_CheckPtr(&posIBP);
            float checkPos[3];
            fn_Entity_GetPosition((void*)posIBP, checkPos, 2, posIdx2, nullptr);

            mapSize = *mapGrid;
            halfMap = mapSize >> 1;
            int cx = (int)(checkPos[0] + (float)halfMap);
            int cy = (int)(checkPos[1] + (float)halfMap);
            if (cx < 0) cx = 0;
            if (cx >= mapSize) cx = mapSize - 1;
            if (cy < 0) cy = 0;
            if (cy >= mapSize) cy = mapSize - 1;
            int checkCell = cy * mapSize + cx;

            uintptr_t structBase2 = *(uintptr_t*)((uintptr_t)mapGrid + 0x80);
            uintptr_t structEntity = *(uintptr_t*)(structBase2 + 8 + (int64_t)checkCell * 0x30);

            if (structEntity != 0) {
                fn_InternalBasePtr_CheckPtr((void*)((uintptr_t)self + 0x10));
                if (structEntity != *(uintptr_t*)((uintptr_t)self + 0x10)) {
                    // Found a different entity at this position -- check for StorageLocation
                    fn_InternalBasePtr_CheckPtr(&structEntity);
                    void* locComp = FindComponentOnEntity(structEntity, *g_TypeId_WorkComponent);
                    uintptr_t locCompIBP = 0;
                    fn_InternalBasePtr_Ctor(&locCompIBP, locComp);

                    fn_InternalBasePtr_CheckPtr(&locCompIBP);
                    uintptr_t citizenRef3 = *(uintptr_t*)entityIBP;
                    uintptr_t assignMgr = *(uintptr_t*)(gameWorld + 0x118);
                    uint64_t assigned = fn_TryAssignWorkplace(
                        assignMgr, &citizenRef3, *(int*)(locCompIBP + 0x28), '\x01');
                    if ((char)assigned != '\0') {
                        return 2;
                    }
                }
            }
        }

        if ((int)firstFreePos < 0) {
            return 1;
        }

    setup_produce_action:
        // --- Set up produce action ---
        // Find HappinessComponent again and toggle state 3
        fn_InternalBasePtr_CheckPtr(entityFieldPtr);
        entity = *(uintptr_t*)entityFieldPtr;
        void* meshComp2 = FindComponentOnEntity(entity, *g_TypeId_StatusIconComponent);
        uintptr_t meshComp2IBP = 0;
        fn_InternalBasePtr_Ctor(&meshComp2IBP, meshComp2);
        fn_InternalBasePtr_CheckPtr(&meshComp2IBP);
        s_fn_ModelComp_SetActiveState((void*)meshComp2IBP, 3, !allInputsAvailable ? '\x01' : '\0');

        if (allInputsAvailable) {
            // Get position for the output slot
            fn_InternalBasePtr_CheckPtr(&posIBP);
            float producePos[3];
            float produceHeading[2] = {};
            fn_Entity_GetPosition((void*)posIBP, producePos, 0, slot, produceHeading);

            mapSize = *mapGrid;
            halfMap = mapSize >> 1;
            int prodCellX = (int)(producePos[1] + (float)halfMap);
            int prodCellY = (int)(producePos[0] + (float)halfMap);
            if (prodCellY < 0) prodCellY = 0;
            if (prodCellY >= mapSize) prodCellY = mapSize - 1;
            if (prodCellX < 0) prodCellX = 0;
            if (prodCellX >= mapSize) prodCellX = mapSize - 1;

            // Recalculate map zones
            s_fn_Map_RecalcZones(mapGrid, producePos, 0, slot);

            // Get cell structs for passability check
            uintptr_t structBase3 = *(uintptr_t*)((uintptr_t)mapGrid + 0x80);
            uintptr_t srcCellStruct = structBase3 + (int64_t)citizenCell * 0x30;
            uintptr_t dstCellStruct = structBase3 + (int64_t)(prodCellX * mapSize + prodCellY) * 0x30;

            uint32_t srcMask = 1 << (*(uint8_t*)(dstCellStruct + 0x29) & 0x1f);
            uint32_t dstMask = 1 << (*(uint8_t*)(srcCellStruct + 0x29) & 0x1f);
            uint32_t combined = srcMask | dstMask;
            uint32_t passFlags = combined | 0xe1f;

            if ((passFlags & dstMask) != 0 && (passFlags & srcMask) != 0 &&
                ((combined & 0x1e0) != 0 ||
                 (*(int16_t*)(srcCellStruct + 0x26) != 0 &&
                  *(int16_t*)(dstCellStruct + 0x26) != 0 &&
                  *(int16_t*)(srcCellStruct + 0x26) == *(int16_t*)(dstCellStruct + 0x26)))) {

                // Check if enough consume cycles have passed
                char canFinish = '\0';
                if (*(int*)(cpDescAddr + 0x40) <= self->field_4c) {
                    canFinish = '\x01';
                    self->field_4c -= *(int*)(cpDescAddr + 0x40);

                    // Increment produce counter on ConsumeProduceComponent
                    fn_InternalBasePtr_CheckPtr((void*)((uintptr_t)self + 0x10));
                    entity = *(uintptr_t*)((uintptr_t)self + 0x10);
                    void* smComp3 = FindComponentOnEntity(entity, *g_TypeId_ConsumeProduceComponent_FC);
                    uintptr_t smComp3IBP = 0;
                    fn_InternalBasePtr_Ctor(&smComp3IBP, smComp3);
                    fn_InternalBasePtr_CheckPtr(&smComp3IBP);
                    *(int*)(smComp3IBP + 0x68) += 1;
                }

                // [CPWork] THE PRODUCE GATE. This is the moment a batch of ale exists;
                // everything before it is the citizen walking and hauling. Report the
                // OUTCOME and both sides of the comparison, because "no ale" has three
                // shapes that look identical from outside: the gate never reached, the
                // gate reached but the accumulated cycle count never catches the
                // description threshold, and the gate passing with nothing downstream.
                {
                    static int n = 0;
                    static int lastAcc = -1;
                    if (n < 12 && (canFinish || self->field_4c != lastAcc)) {
                        ++n; lastAcc = self->field_4c;
                        fprintf(stderr, "[CPWork] produceGate acc(+0x4c)=%d need(desc+0x40)=%d -> %s\n",
                                self->field_4c, *(int*)(cpDescAddr + 0x40),
                                canFinish ? "PRODUCED" : "not yet");
                        fflush(stderr);
                    }
                }
                // Create work action chain
                uintptr_t citizenRef4 = *(uintptr_t*)entityIBP;
                uintptr_t workEntityRef = *(uintptr_t*)((uintptr_t)self + 0x10);
                int actionPriority = (int)*(int64_t*)((uintptr_t)self + 0x50);

                s_fn_SetupWorkChain(
                    &citizenRef4,
                    *(int*)((uintptr_t)self + 0x20),
                    &workEntityRef,
                    producePos,
                    produceHeading[0],
                    *(uintptr_t*)(*(uintptr_t*)((uintptr_t)self + 0x10) + 0x28),
                    *(uint64_t*)(productData + 0x10 + productStride * 8),
                    (uint32_t)canFinish,
                    (int*)&actionPriority,
                    *(uint32_t*)((uintptr_t)self + 0x54),
                    firstFreeIndex,
                    *(float*)(cpDescAddr + 0x44),
                    (canFinish ? 0x8000 : 0) | ((firstFreePos | 0x100) << 8) | slot);

                // Update status array for the position
                if (canFinish != '\0') {
                    ((int*)self->field_68)[(int)firstFreePos] = 1;
                }

                int priority3 = *(int*)((uintptr_t)self + 0x20);
                fn_InternalBasePtr_CheckPtr(entityIBP);
                return FinalizeAction(self, entityIBP, &actionPriority, priority3, slot);
            }
        }
        return 1;
    }

    // ============================================================
    // BRANCH: Depleted resources or threshold not met
    // ============================================================
    if ((char)canProduce != '\0') {
    handle_depleted_or_idle:
        // Nothing to do here -- fall through
        (void)0;
    }

    // ResetProductionState IS 0x14012b310, and b310 is not a passive "reset": it drives
    // the ResourceProduce state machine 13 -> 14 (PAUSED). State 14's enter calls
    // SetAccepting(wpc, false), which raises the "work is disabled at this location"
    // sign AND evicts the entire worker roster — permanently, because state 14 has no
    // per-frame handler and the only 14 -> 13 requester is the panel's resume event.
    //
    // Retail GUARDS this call; we did not. At 0x1400e8eca:
    //     TEST AL,AL
    //     JNZ  0x1400e8ee1        ; skip straight to the SetActiveState(3,0) tail
    //     CALL 0x14012b310        ; only reached when AL == 0
    // AL is helper_93a0 (0x1401193a0) — our `canProduce`. So the (depleted &&
    // canProduce) arm must SKIP b310 and only set model state 3, keeping its workers.
    //
    // Symptom this caused: a woodcutter works normally until the depletion test flips
    // (~500-1000 frames), is still under its produce limit (canProduce=1), and where
    // retail would idle the model and keep the roster, we paused the building and
    // threw the worker back to laborer. The `[CPWork] canProduce=1 depleted=1` line
    // logged immediately before State14Enter is exactly this arm.
    //
    // The !depleted && !canProduce path (the goto above) still reaches b310, since
    // canProduce == 0 there — which is retail's intent: nothing to make, so stand down.
    if ((char)canProduce == '\0') {
        fn_InternalBasePtr_CheckPtr(&smCompIBP);
        s_fn_ResetProductionState(smCompIBP);
    }

    // Set active state = 3, disabled
    fn_InternalBasePtr_CheckPtr(&meshCompIBP);
    s_fn_ModelComp_SetActiveState((void*)meshCompIBP, 3, '\0');

    return 1;
}

// ============================================================
// OnComplete (0x1400e77c0) -- vtable slot 6
// Stores null IBP into the assigned citizen slot, updates state tracking.
// param4 encodes: bits [6:0] = slot index, bit 15 = update int array,
//   bits [14:8] = int array index, bit 16 = increment field_4c
// ============================================================
int ConsumeProduce_OnComplete_Impl(ConsumeProduceAction* self, void* /*param2*/, void* /*param3*/, uint32_t param4) {
    // Clear the citizen assignment: store null IBP into slot
    uintptr_t nullIBP = 0;
    fn_InternalBasePtr_Ctor(&nullIBP, nullptr);

    uint32_t slotIdx = param4 & 0x7F;
    uintptr_t* ibpArray = (uintptr_t*)self->field_58;
    ibpArray[slotIdx] = nullIBP;

    // If bit 15 set, mark the int array slot as state 2 (completed)
    if ((param4 >> 15) & 1) {
        int* intArray = (int*)self->field_68;
        uint32_t intIdx = (param4 >> 8) & 0x7F;
        intArray[intIdx] = 2;
    }

    // If bit 16 set, increment the produce counter
    if (param4 & 0x10000) {
        self->field_4c += 1;
    }

    return 0;
}

// ============================================================
// OnCancel (0x1400e7780) -- vtable slot 7
// Delegates to OnComplete with bit 16 cleared (no produce increment).
// ============================================================
int ConsumeProduce_OnCancel_Impl(ConsumeProduceAction* self, void* param2, void* param3, uint32_t param4) {
    // Copy params, clear bit 16, call OnComplete (slot 6)
    // In original: reads vtable slot +0x30 (OnComplete) and calls it
    uint32_t modifiedParam4 = param4 & 0xFFFEFFFF;  // clear bit 16

    typedef int (*OnComplete_t)(void* self, void* p2, void* p3, uint32_t p4);
    void* vtable = self->vtable;
    OnComplete_t onComplete = *(OnComplete_t*)((uintptr_t)vtable + 0x30);
    onComplete(self, param2, param3, modifiedParam4);
    return 0;
}

// ============================================================
// Cleanup (0x1400e8f30) -- vtable slot 8
// Iterates assigned citizens in the IBP array, clears their task state.
// ============================================================
int ConsumeProduce_Cleanup_Impl(ConsumeProduceAction* self) {
    int count = self->field_60;
    if (count <= 0) return 0;

    uintptr_t* ibpArray = (uintptr_t*)self->field_58;

    for (int i = 0; i < count; i++) {
        uintptr_t entityIBP = ibpArray[i];
        if (entityIBP == 0) continue;

        // CheckPtr on the entity IBP
        fn_InternalBasePtr_CheckPtr(&ibpArray[i]);
        uintptr_t entity = ibpArray[i];

        // Get entity's StateMachineComponent at +0x278
        fn_InternalBasePtr_CheckPtr((void*)(entity + 0x278));
        uintptr_t smComp = *(uintptr_t*)(entity + 0x278);

        // Call task list cleanup on the SM component's task list (+0x48)
        s_fn_TaskListCleanup((void*)(smComp + 0x48));
    }
    return 0;
}

// ============================================================
// Validate (0x1400e7530) -- vtable slot 9
// Checks if all assigned citizen tasks are still valid.
// Returns: 1 = all valid, 0 = some invalid (with cleanup).
// ============================================================
int ConsumeProduce_Validate_Impl(ConsumeProduceAction* self, void* param2) {
    int count = self->field_60;
    if (count <= 0) {
        // No assigned citizens -- return valid
        return 1;
    }

    uintptr_t* ibpArray = (uintptr_t*)self->field_58;
    bool allValid = true;
    int validCount = 0;
    int* tempTypes = nullptr;  // dynamic array of type IDs per valid citizen
    int tempCapacity = 0;

    for (int i = 0; i < count; i++) {
        uintptr_t entityIBP = ibpArray[i];
        if (entityIBP == 0) continue;

        // Validate task for this citizen
        uintptr_t taskResult = 0;
        uintptr_t ibpBuf = entityIBP;
        uint8_t valid = s_fn_Work_ValidateTask(self, param2, &ibpBuf, (void**)&taskResult);

        if (valid == 0) {
            // Task invalid -- clear citizen from slot
            uintptr_t nullIBP = 0;
            fn_InternalBasePtr_Ctor(&nullIBP, nullptr);
            ibpArray[i] = nullIBP;
            allValid = false;
        } else {
            // Task valid -- track the type for cross-referencing
            fn_InternalBasePtr_CheckPtr(&taskResult);
            int typeTag = *(int*)(taskResult + 0x34);

            int newCount = validCount + 1;
            if (newCount != validCount) {
                int newCap = tempCapacity;
                if (tempCapacity < newCount) {
                    newCap = newCount;
                    if (validCount != 0) {
                        newCap = newCount * 3;
                    }
                    int* newArr = (int*)fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)newCap * 4, 4);
                    // Copy old data if buffer changed
                    int startIdx = (tempTypes == newArr) ? validCount : 0;
                    for (int j = startIdx; j < newCount; j++) {
                        if (j < validCount && tempTypes != newArr) {
                            newArr[j] = tempTypes[j];
                        } else if (j >= validCount) {
                            newArr[j] = (typeTag >> 4) & 0xF;
                        }
                    }
                    if (tempTypes != newArr) {
                        fn_MemFreeStatic(tempTypes);
                    }
                    tempTypes = newArr;
                    tempCapacity = newCap;
                } else {
                    // Buffer has capacity -- just set the new element
                    if (tempTypes) {
                        tempTypes[validCount] = (typeTag >> 4) & 0xF;
                    }
                }
                validCount = newCount;
            }
        }
    }

    // If some citizens were invalidated, update the int array state
    if (!allValid) {
        int* intArray = (int*)self->field_68;
        int intCount = self->field_70;
        for (int k = 0; k < intCount; k++) {
            // Check if this slot's type is still present in valid citizens
            bool found = false;
            for (int j = 0; j < validCount; j++) {
                if (tempTypes[j] == k) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                // No valid citizen handles this slot -- mark as needs-redo
                if (intArray[k] == 1) {
                    intArray[k] = 2;
                }
            }
        }
    }

    fn_MemFreeStatic(tempTypes);
    return allValid ? 1 : 0;
}

// ============================================================
// GetTarget (0x1400e8fb0) -- vtable slot 10
// Returns the target position from the entity's position component.
// Hidden return ptr ABI: RCX=self, RDX=retbuf
// ============================================================
void* ConsumeProduce_GetTarget_Impl(ConsumeProduceAction* self, void* retbuf) {
    // CheckPtr on the entity IBP at +0x10
    fn_InternalBasePtr_CheckPtr(&self->field_10);

    // Find the position/work component (type 0x1401c80e0) on the entity
    uintptr_t componentIBP = 0;
    s_fn_FindPositionComponent(self->field_10, &componentIBP);

    // CheckPtr on the found component
    fn_InternalBasePtr_CheckPtr(&componentIBP);

    // Read target position at component+0x48 and store in retbuf
    uintptr_t comp = componentIBP;
    *(uint64_t*)retbuf = *(uint64_t*)(comp + 0x48);

    return retbuf;
}

// ============================================================
// Standalone factory + vtable — Work family (factory cascade).
// Vtable @ 0x140161220 (13 slots), size 0x78, parent "Work".
// Shared slot helpers: work_base_standalone.{h,cpp}.
//
// FLAG: slot11 GetPriority (+0x58) is NOT implemented in this file (no
// ConsumeProduce_GetPriority_Impl exists). Left as a clearly-marked nullptr.
// ============================================================
#include "work_base_standalone.h"
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t ConsumeProduce_GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("ConsumeProduce");
    return c;
}

// Slot11 (+0x58) — retail 0x14002a0f0 is `return 0 < *(int*)(self + 0x60)` (the shared
// body ClearStorage also uses: tree count at +0x60, not the +0x50 the generic
// Work_Slot11_HasActiveTasks reads). Was pinned to 0 by Work_SlotRet0 (2026-09-05).
static int ConsumeProduce_Slot11_HasActiveTasks(void* self) {
    return *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(self) + 0x60) > 0 ? 1 : 0;
}

static void* g_ConsumeProduce_Vtable[13] = {
    (void*)&ConsumeProduce_ScalarDtor_Impl,  // +0x00 ScalarDtor
    (void*)&Work_Slot1,                      // +0x08 slot1 (null)
    (void*)&ConsumeProduce_GetTypeId,        // +0x10 GetTypeId
    (void*)&ConsumeProduce_Serialize_Impl,   // +0x18 Serialize
    (void*)&ConsumeProduce_Execute_Impl,     // +0x20 Execute
    (void*)&Work_Slot5,                      // +0x28 slot5 (returns 0)
    (void*)&ConsumeProduce_OnComplete_Impl,  // +0x30 OnComplete
    (void*)&ConsumeProduce_OnCancel_Impl,    // +0x38 OnCancel
    (void*)&ConsumeProduce_Cleanup_Impl,     // +0x40 Cleanup
    (void*)&ConsumeProduce_Validate_Impl,    // +0x48 Validate
    (void*)&ConsumeProduce_GetTarget_Impl,   // +0x50 GetTarget
    (void*)&ConsumeProduce_Slot11_HasActiveTasks, // +0x58 HasActiveTasks (0x14002a0f0: +0x60 > 0)
    (void*)&Work_Slot12,                     // +0x60 slot12 (void nop)
};

extern "C" void RegisterConsumeProduceVtable() {
    RegisterExeAddr(0x140161220, (uintptr_t)g_ConsumeProduce_Vtable);
}
extern "C" void* ConsumeProduce_CreateInPlace(void* mem) {
    ConsumeProduce_ResolveFuncs();
    return ConsumeProduce_Ctor_Impl((ConsumeProduceAction*)mem);
}
extern "C" void* ConsumeProduce_Factory(void) {
    ConsumeProduce_ResolveFuncs();
    void* p = Work_Alloc(0x78);
    return p ? ConsumeProduce_Ctor_Impl((ConsumeProduceAction*)p) : nullptr;
}
REGISTER_TYPE(ConsumeProduce, "Work", &ConsumeProduce_Factory, &ConsumeProduce_CreateInPlace);
