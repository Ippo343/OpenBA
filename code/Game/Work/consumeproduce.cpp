// code/Game/Work/consumeproduce.cpp
// Clean C++ reimplementation of ConsumeProduce Work class methods.
// Source: Game/Work/ConsumeProduce.cpp (from profile build debug strings)
//
// ConsumeProduce -- consume/produce work cycle. Size = 0x78.
// Inherits: Object::Internal -> Work (0x48) -> ConsumeProduce
// Ctor: 0x1400e9340, Vtable: 0x140161220
//
// Execute (slot 4) is NOT reimplemented here -- it is ~500 lines of complex
// game logic involving pathfinding, storage queries, and citizen AI state
// transitions. It remains as a trampoline in the hook layer until all
// dependent helper functions are resolved.
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h -- it is portable for standalone exe.

#include "Work/consumeproduce_funcs.h"
#include "BuildActions.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Local function pointers resolved at init time
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

    // FindPositionComponent (FUN_140002e10) -- find WorkComponent on entity
    s_fn_FindPositionComponent = reinterpret_cast<void*(*)(void*, void*)>(
        ResolveAddr(Module::Exe, 0x140002e10));

    // TypeId for IsDerivedFrom check
    s_typeId_1401c7158 = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7158));

    // Vtables
    s_vtable_Work = (void*)ResolveAddr(Module::Exe, 0x140161a80);
    s_vtable_ConsumeProduce = (void*)ResolveAddr(Module::Exe, ConsumeProduceActionAddr::Vtable);

    bool ok = s_fn_ObjectInternal_Ctor && s_fn_ObjectInternal_Dtor &&
              s_fn_Work_Serialize && s_fn_Serializer_Ctor && s_fn_Serializer_Dtor &&
              s_fn_IBP_Serialize && s_fn_Type_Serialize &&
              s_fn_Serializer_SerializeIntBulk &&
              s_fn_ResizeIBPArray && s_fn_ResizeIntArray &&
              s_fn_Work_ValidateTask && s_fn_TaskListCleanup &&
              s_fn_FindPositionComponent &&
              s_typeId_1401c7158 &&
              fn_InternalBasePtr_Ctor && fn_InternalBasePtr_CheckPtr &&
              fn_IsDerivedFrom && fn_MemFreeStatic &&
              g_fn_Serialize_Int && g_fn_Serialize_Bool &&
              s_vtable_Work && s_vtable_ConsumeProduce;

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
    self->field_50 = 0;    // Type
    self->field_58 = nullptr;  // IBP array
    self->field_60 = 0;       // IBP count
    self->field_68 = nullptr;  // int array
    self->field_70 = 0;       // int count

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

    // Free the int array at +0x68
    fn_MemFreeStatic(self->field_68);
    self->field_68 = nullptr;
    self->field_70 = 0;

    // Free the IBP array at +0x58
    fn_MemFreeStatic(self->field_58);
    self->field_58 = nullptr;
    self->field_60 = 0;

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
    g_fn_Serialize_Int(serializerBuf, &self->field_60, nullptr);

    // If loading, resize the IBP array to match deserialized count
    if (isLoading) {
        s_fn_ResizeIBPArray((void*)&self->field_58, self->field_60);
    }

    // Serialize each InternalBasePtr in the array
    uintptr_t* ibpArray = (uintptr_t*)self->field_58;
    int count = self->field_60;
    for (int i = 0; i < count; i++) {
        s_fn_IBP_Serialize(&ibpArray[i], serializerBuf);

        // After deserialize, validate the entity type
        void* ptr = (void*)ibpArray[i];
        if (ptr != nullptr) {
            bool valid = fn_IsDerivedFrom(ptr, *s_typeId_1401c7158);
            if (!valid) {
                ibpArray[i] = 0;  // null out invalid refs
            }
        }
    }

    // --- Serialize int array count (+0x70) ---
    g_fn_Serialize_Int(serializerBuf, &self->field_70, nullptr);

    // If loading, resize the int array
    if (isLoading) {
        s_fn_ResizeIntArray((void*)&self->field_68, self->field_70);
    }

    // Bulk-serialize the int array
    // Original: IO::Serializer::Serialize(serializer, count, intArrayPtr, name)
    s_fn_Serializer_SerializeIntBulk(serializerBuf, self->field_70, self->field_68, nullptr);

    // --- Serialize ConsumeProduce fields ---
    g_fn_Serialize_Bool(serializerBuf, &self->field_48, nullptr);   // bool at +0x48
    g_fn_Serialize_Int(serializerBuf, &self->field_4c, nullptr);    // int at +0x4C
    s_fn_Type_Serialize(&self->field_50, serializerBuf);            // Type at +0x50
    g_fn_Serialize_Int(serializerBuf, &self->field_54, nullptr);    // int at +0x54

    s_fn_Serializer_Dtor(serializerBuf);
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
void ConsumeProduce_OnCancel_Impl(ConsumeProduceAction* self, void* param2, void* param3, uint32_t param4) {
    // Copy params, clear bit 16, call OnComplete (slot 6)
    // In original: reads vtable slot +0x30 (OnComplete) and calls it
    uint32_t modifiedParam4 = param4 & 0xFFFEFFFF;  // clear bit 16

    typedef int (*OnComplete_t)(void* self, void* p2, void* p3, uint32_t p4);
    void* vtable = self->vtable;
    OnComplete_t onComplete = *(OnComplete_t*)((uintptr_t)vtable + 0x30);
    onComplete(self, param2, param3, modifiedParam4);
}

// ============================================================
// Cleanup (0x1400e8f30) -- vtable slot 8
// Iterates assigned citizens in the IBP array, clears their task state.
// ============================================================
void ConsumeProduce_Cleanup_Impl(ConsumeProduceAction* self) {
    int count = self->field_60;
    if (count <= 0) return;

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
}

// ============================================================
// Validate (0x1400e7530) -- vtable slot 9
// Checks if all assigned citizen tasks are still valid.
// Returns: 1 = all valid, 0 = some invalid (with cleanup).
// ============================================================
int ConsumeProduce_Validate_Impl(ConsumeProduceAction* self, void* param2) {
    int count = self->field_60;
    if (count <= 0) {
        // No assigned citizens -- free temp array and return valid
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
