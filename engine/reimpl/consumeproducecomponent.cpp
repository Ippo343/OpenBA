// engine/reimpl/consumeproducecomponent.cpp
// Reimplementation of ConsumeProduceComponent lifecycle methods from Application-x64.exe.
//
// ConsumeProduceComponent extends ResourceProduceComponent (0x70 bytes).
// Two non-trivial vtable overrides:
//
// Methods reimplemented:
//   - Shutdown (0x140118e00) — parent Shutdown + versioned serialization of consume fields
//   - Serialize (0x140118ef0) — init consume data from building template
//
// Serialize is complex (62 instructions, component lookups, type checks).
// Using passthrough for Serialize.

#include "ConsumeProduceComponent.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookShutdown;
static HookInfo g_hookSerialize;

// ============================================================
// Original function typedefs for passthrough
// ============================================================
typedef void (*fn_ConsumeProduceSerialize_t)(void* self);

// ============================================================
// Local helper function pointers
// ============================================================

// ResourceProduceComponent_Shutdown (parent) at 0x14012b4a0
typedef void (*fn_ParentShutdown_t)(void* self, void* stream);
static fn_ParentShutdown_t fn_ParentShutdown = nullptr;

// IO::Serializer IAT pointers
typedef void (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t fn_Serializer_Ctor = nullptr;
typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t fn_Serializer_Dtor = nullptr;
typedef void (*fn_Serializer_SerializeInt_t)(void* self, int* value, const char* name);
static fn_Serializer_SerializeInt_t fn_Serializer_SerializeInt = nullptr;
typedef int (*fn_Serializer_GetVersion_t)(void* self);
static fn_Serializer_GetVersion_t fn_Serializer_GetVersion = nullptr;

// FUN_140023a00 — resize float array
typedef void (*fn_ResizeFloatArray_t)(void* arrayPtr, int newSize);
static fn_ResizeFloatArray_t fn_ResizeFloatArray = nullptr;

// IO::Serializer::Serialize(this, count, floatArray, name) — serialize float array
typedef void (*fn_Serializer_SerializeFloatArray_t)(void* self, int count, float* array, const char* name);
static fn_Serializer_SerializeFloatArray_t fn_Serializer_SerializeFloatArray = nullptr;

// ============================================================
// Shutdown (0x140118e00)
// Calls parent ResourceProduceComponent_Shutdown, then serializes:
//   - consumeResourceId (+0x50)
//   - consumeData1 count (+0x60) with array resize
//   - float array at consumeData0 (+0x58)
//   - consumeState (+0x68)
//   - Version-0 migration: if consumeResourceId < 0 on load, reset to 0
// ============================================================
static void Reimpl_Shutdown(ConsumeProduceComponent* self, void* stream) {
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 3) {
        printf("[ConsumeProduceComponent::Shutdown] self=%p consumeResId=%d\n",
               (void*)self, self->consumeResourceId);
        callCount++;
    }
#endif

    // Call parent ResourceProduceComponent::Shutdown
    fn_ParentShutdown(self, stream);

    // Create versioned serializer (version 1)
    uint8_t serBuf[24]; // 3 qwords for Serializer
    int64_t* pSerData = (int64_t*)serBuf;
    fn_Serializer_Ctor(serBuf, stream, 1);

    // Serialize consumeResourceId at +0x50
    fn_Serializer_SerializeInt(serBuf, &self->consumeResourceId, nullptr);

    // Serialize consume array count (+0x60 as int)
    fn_Serializer_SerializeInt(serBuf, (int*)&self->consumeData1, nullptr);

    // On load (direction == 0): resize array if needed
    if (*(int*)(pSerData[0] + 8) == 0) {
        fn_ResizeFloatArray(&self->consumeData0, *(int*)&self->consumeData1);
    }

    // Serialize float array: consumeData0 ptr, consumeData1 count
    fn_Serializer_SerializeFloatArray(serBuf, *(int*)&self->consumeData1,
                                       *(float**)&self->consumeData0, nullptr);

    // Serialize consumeState at +0x68
    fn_Serializer_SerializeInt(serBuf, (int*)&self->consumeState, nullptr);

    // Version-0 migration: on load, if consumeResourceId < 0, reset to 0
    int version = fn_Serializer_GetVersion(serBuf);
    if (version == 0 && self->consumeResourceId < 0) {
        // Check if we're loading (stream direction == 0)
        if (*(int*)((uintptr_t)stream + 8) == 0) {
            self->consumeResourceId = 0;
        }
    }

    fn_Serializer_Dtor(serBuf);
}

// ============================================================
// Serialize (0x140118ef0) — PASSTHROUGH
// Complex initialization: looks up component types on the entity,
// finds building template data, copies consume slot info, initializes
// float array to 1.0f per slot. Too many type lookups to reimpl safely.
// ============================================================
static void Reimpl_Serialize(ConsumeProduceComponent* self) {
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 3) {
        printf("[ConsumeProduceComponent::Serialize] self=%p consumeResId=%d (passthrough)\n",
               (void*)self, self->consumeResourceId);
        callCount++;
    }
#endif

    ((fn_ConsumeProduceSerialize_t)g_hookSerialize.trampoline)(self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallConsumeProduceComponentShutdown() {
    fn_ParentShutdown = reinterpret_cast<fn_ParentShutdown_t>(
        ResolveAddr(Module::Exe, 0x14012b4a0));

    uintptr_t iatCtor = ResolveAddr(Module::Exe, 0x140152ad8);
    uintptr_t iatDtor = ResolveAddr(Module::Exe, 0x140152ad0);
    if (iatCtor) fn_Serializer_Ctor = *reinterpret_cast<fn_Serializer_Ctor_t*>(iatCtor);
    if (iatDtor) fn_Serializer_Dtor = *reinterpret_cast<fn_Serializer_Dtor_t*>(iatDtor);

    uintptr_t iatSerialize = ResolveAddr(Module::Exe, 0x140152ac8);
    if (iatSerialize) fn_Serializer_SerializeInt = *reinterpret_cast<fn_Serializer_SerializeInt_t*>(iatSerialize);

    uintptr_t iatGetVer = ResolveAddr(Module::Exe, 0x140152a48);
    if (iatGetVer) fn_Serializer_GetVersion = *reinterpret_cast<fn_Serializer_GetVersion_t*>(iatGetVer);

    fn_ResizeFloatArray = reinterpret_cast<fn_ResizeFloatArray_t>(
        ResolveAddr(Module::Exe, 0x140023a00));

    // Serializer::Serialize(float array) — different IAT entry at [0x140151ea8]
    uintptr_t iatFloatArray = ResolveAddr(Module::Exe, 0x140151ea8);
    if (iatFloatArray) fn_Serializer_SerializeFloatArray = *reinterpret_cast<fn_Serializer_SerializeFloatArray_t*>(iatFloatArray);

#ifdef _DEBUG
    printf("[ConsumeProduceComponent] ParentShutdown = %p\n", (void*)fn_ParentShutdown);
    printf("[ConsumeProduceComponent] Serializer_Ctor = %p\n", (void*)fn_Serializer_Ctor);
    printf("[ConsumeProduceComponent] ResizeFloatArray = %p\n", (void*)fn_ResizeFloatArray);
#endif

    if (!fn_ParentShutdown || !fn_Serializer_Ctor || !fn_Serializer_Dtor ||
        !fn_Serializer_SerializeInt || !fn_Serializer_GetVersion || !fn_ResizeFloatArray) {
#ifdef _DEBUG
        printf("[ConsumeProduceComponent] ERROR: failed to resolve Shutdown helpers\n");
#endif
        return;
    }

    uintptr_t target = ResolveAddr(Module::Exe, 0x140118e00);
    // Prologue: PUSH RDI(2) + SUB RSP,0x40(4) + MOV qword,-0x2(9) = 15
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[ConsumeProduceComponent] Shutdown hooked at %p (prologue=15)\n", (void*)target);
#endif
}

void RemoveConsumeProduceComponentShutdown() { RemoveHook(&g_hookShutdown); }

void InstallConsumeProduceComponentSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140118ef0);
    // Prologue: PUSH RSI(1)+PUSH RDI(1)+PUSH R15(2)+SUB RSP,0x20(4)+MOV R15,RCX(3)+MOV(5) = boundary at 12
    // At offset 4 (after PUSH R15): SUB RSP starts. 4+4=8. +MOV R15,RCX=11. +MOV [RSP+0x40],RBX starts at 11.
    // 1+1+2+4+3=11, next is MOV [RSP+0x40],RBX (5 bytes) = 16. But 12 falls within MOV R15,RCX?
    // Let me recount: PUSH RSI=1@0, PUSH RDI=1@1, PUSH R15=2@2, SUB RSP,0x20=4@4, MOV R15,RCX=3@8.
    // Wait: 0x140118ef0=PUSH RSI(1), ef1=PUSH RDI(1), ef2=PUSH R15(2), ef4=SUB RSP,0x20(4), ef8=nope.
    // ef0+1=ef1, +1=ef2, +2=ef4, +4=ef8 (not ef9). ef8=MOV R15,RCX(3)=efb. efb+1=efc=MOV [RSP+0x40],RBX(5)=f01.
    // Boundaries: 0,1,2,4,8,11,16. 12 falls in MOV [RSP+0x40],RBX (11-16). Need 16.
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 16);
#ifdef _DEBUG
    printf("[ConsumeProduceComponent] Serialize hooked at %p (prologue=16, passthrough)\n", (void*)target);
#endif
}

void RemoveConsumeProduceComponentSerialize() { RemoveHook(&g_hookSerialize); }
