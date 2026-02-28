// engine/reimpl/rawmaterialcomponent.cpp
// Reimplementation of RawMaterialComponent lifecycle methods from Application-x64.exe.
//
// RawMaterialComponent is a direct Component subclass (0x40 bytes).
// Four non-trivial vtable overrides:
//
// Methods reimplemented:
//   - Shutdown (0x140129330) — versioned serialization of field28, field38, field30
//   - Serialize (0x1401292c0) — init field28 from building data, register with world
//   - Draw (0x1401291d0) — complex: creates render object and registers draw item
//   - Update (0x140129130) — accumulate time, check threshold, mark for collection
//
// Draw is complex (56 instructions, allocs, vtable init). Using passthrough.

#include "RawMaterialComponent.h"
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
static HookInfo g_hookDraw;
static HookInfo g_hookUpdate;

// ============================================================
// Original function typedefs for passthrough
// ============================================================
typedef void (*fn_RawMaterial_Draw_t)(void* self);

// ============================================================
// Local helper function pointers
// ============================================================

// Component_Serialize at 0x1401189b0
typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t fn_Component_Serialize = nullptr;

// IO::Serializer IAT pointers
typedef void (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t fn_Serializer_Ctor = nullptr;
typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t fn_Serializer_Dtor = nullptr;
typedef void (*fn_Serializer_SerializeInt_t)(void* self, int* value, const char* name);
static fn_Serializer_SerializeInt_t fn_Serializer_SerializeInt = nullptr;
typedef int (*fn_Serializer_GetVersion_t)(void* self);
static fn_Serializer_GetVersion_t fn_Serializer_GetVersion = nullptr;

// System::Time::Serialize(this, serializer)
typedef void (*fn_Time_Serialize_t)(void* timePtr, void* serializer);
static fn_Time_Serialize_t fn_Time_Serialize = nullptr;

// FUN_140018020 — serialize field30 (version >= 2)
typedef void (*fn_SerializeField30_t)(void* fieldPtr, void* serializer);
static fn_SerializeField30_t fn_SerializeField30 = nullptr;

// Object::Base::IsDerivedFrom (from engine_funcs)
// Already extern: fn_IsDerivedFrom

// DAT_1401c7cb8 — type ID for building/structure data
static uint32_t* g_pBuildingDataTypeId = nullptr;

// Object::InternalBasePtr::CheckPtr via IAT at [0x140152ab0]
typedef void (*fn_CheckPtr_t)(void* ptr);
static fn_CheckPtr_t fn_CheckPtr = nullptr;

// FUN_140091e60 — register raw material with world manager
typedef void (*fn_RegisterRawMaterial_t)(void* worldMgr, void* entityChain);
static fn_RegisterRawMaterial_t fn_RegisterRawMaterial = nullptr;

// DAT_1401c3dd8 — game state singleton
static void** g_pGameState = nullptr;

// FUN_1400c2ea0(entity) — mark entity for collection
typedef void (*fn_MarkForCollection_t)(void* entity);
static fn_MarkForCollection_t fn_MarkForCollection = nullptr;

// FUN_140003110 — add entity to world pending list
typedef void (*fn_AddToPending_t)(void* listPtr, int newCount, void* entityPtr);
static fn_AddToPending_t fn_AddToPending = nullptr;

// System::Time::Time(out, float seconds) — construct Time from float
typedef void* (*fn_Time_FromFloat_t)(void* outTime, float seconds);
static fn_Time_FromFloat_t fn_Time_FromFloat = nullptr;

// DAT_14017225c — threshold time (float constant)
static float* g_pThresholdTime = nullptr;

// ============================================================
// Shutdown (0x140129330)
// Versioned serialization: field28 (int), field38 (Time), field30 (v2+)
// ============================================================
static void Reimpl_Shutdown(RawMaterialComponent* self, void* stream) {
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 3) {
        printf("[RawMaterialComponent::Shutdown] self=%p\n", (void*)self);
        callCount++;
    }
#endif

    fn_Component_Serialize(self, stream);

    uint8_t serializerBuf[24];
    fn_Serializer_Ctor(serializerBuf, stream, 2);

    // Serialize field28 (resource type ID)
    fn_Serializer_SerializeInt(serializerBuf, (int*)&self->field28, nullptr);

    // Serialize field38 (Time — accumulator)
    fn_Time_Serialize(&self->field38, serializerBuf);

    // Version >= 2: serialize field30
    int version = fn_Serializer_GetVersion(serializerBuf);
    if (version >= 2) {
        fn_SerializeField30(&self->field30, serializerBuf);
    }

    fn_Serializer_Dtor(serializerBuf);
}

// ============================================================
// Serialize (0x1401292c0) — vtable slot 4 (Serialize/OnStartup)
// Initializes field28 from building data type at +0x40, then
// registers the entity with the world raw material manager.
// ============================================================
static void Reimpl_Serialize(RawMaterialComponent* self) {
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 3) {
        printf("[RawMaterialComponent::Serialize] self=%p\n", (void*)self);
        callCount++;
    }
#endif

    // Get building data via IsDerivedFrom check on _unk18
    void* baseObj = self->_unk18;
    bool isDerived = fn_IsDerivedFrom(baseObj, *g_pBuildingDataTypeId);
    void* buildingData = nullptr;
    if (isDerived) {
        buildingData = baseObj;
    }

    // Copy resource type from building data +0x40 to field28
    *(uint32_t*)&self->field28 = *(uint32_t*)((uintptr_t)buildingData + 0x40);

    // CheckPtr on owner at +0x10
    fn_CheckPtr((void*)((uintptr_t)self + 0x10));

    // Get entity chain from owner+0x28, register with world manager
    uintptr_t gameState = (uintptr_t)*g_pGameState;
    uintptr_t worldMgr = *(uintptr_t*)(gameState + 0xf0);
    void* entityChain = *(void**)(*(uintptr_t*)((uintptr_t)self + 0x10) + 0x28);
    fn_RegisterRawMaterial((void*)worldMgr, entityChain);
}

// ============================================================
// Draw (0x1401291d0) — PASSTHROUGH
// Complex rendering: creates a visual object, sets up transform,
// allocates 0x60-byte render item with vtable, registers with
// the draw system. Too many internal dependencies to reimpl safely.
// ============================================================
static void Reimpl_Draw(RawMaterialComponent* self) {
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 3) {
        printf("[RawMaterialComponent::Draw] self=%p (passthrough)\n", (void*)self);
        callCount++;
    }
#endif

    ((fn_RawMaterial_Draw_t)g_hookDraw.trampoline)(self);
}

// ============================================================
// Update (0x140129130)
// Accumulates delta time into field38. When threshold exceeded,
// checks if entity is marked for removal (bit 10 of flags at
// entity+0x40). If not, marks for collection and adds to world
// pending list.
// ============================================================
static void Reimpl_Update(RawMaterialComponent* self, int64_t* pDeltaTime) {
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 3) {
        printf("[RawMaterialComponent::Update] self=%p dt=%lld\n",
               (void*)self, (long long)*pDeltaTime);
        callCount++;
    }
#endif

    // Accumulate delta time into field38 (Time)
    int64_t* pAccumulator = (int64_t*)&self->field38;
    *pAccumulator += *pDeltaTime;

    // Construct threshold time from float constant
    int64_t thresholdTime[3]; // Time object on stack
    fn_Time_FromFloat(thresholdTime, *g_pThresholdTime);

    // Check if accumulated time exceeds threshold
    if (thresholdTime[0] < *pAccumulator) {
        void* entity = *(void**)((uintptr_t)self + 0x10);

        uintptr_t gameState = (uintptr_t)*g_pGameState;
        uintptr_t worldMgr = *(uintptr_t*)(gameState + 0xe8);

        if (entity != nullptr) {
            fn_CheckPtr((void*)((uintptr_t)self + 0x10));

            // Check bit 10 of entity flags at +0x40
            uint32_t entityFlags = *(uint32_t*)((uintptr_t)entity + 0x40);
            if (((entityFlags >> 10) & 1) == 0) {
                fn_CheckPtr((void*)((uintptr_t)self + 0x10));
                fn_MarkForCollection(entity);

                // Add to world pending list: list at worldMgr+0xd0, count at worldMgr+0xd8
                int currentCount = *(int*)(worldMgr + 0xd8);
                fn_AddToPending((void*)(worldMgr + 0xd0), currentCount + 1,
                                (void*)((uintptr_t)self + 0x10));
            }
        }
    }
}

// ============================================================
// Install / Remove
// ============================================================
void InstallRawMaterialComponentShutdown() {
    fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));

    uintptr_t iatCtor = ResolveAddr(Module::Exe, 0x140152ad8);
    uintptr_t iatDtor = ResolveAddr(Module::Exe, 0x140152ad0);
    if (iatCtor) fn_Serializer_Ctor = *reinterpret_cast<fn_Serializer_Ctor_t*>(iatCtor);
    if (iatDtor) fn_Serializer_Dtor = *reinterpret_cast<fn_Serializer_Dtor_t*>(iatDtor);

    uintptr_t iatSerialize = ResolveAddr(Module::Exe, 0x140152ac8);
    if (iatSerialize) fn_Serializer_SerializeInt = *reinterpret_cast<fn_Serializer_SerializeInt_t*>(iatSerialize);

    uintptr_t iatGetVer = ResolveAddr(Module::Exe, 0x140152a48);
    if (iatGetVer) fn_Serializer_GetVersion = *reinterpret_cast<fn_Serializer_GetVersion_t*>(iatGetVer);

    // System::Time::Serialize — via IAT at [0x140152b08] (or resolve by export)
    fn_Time_Serialize = reinterpret_cast<fn_Time_Serialize_t>(
        ResolveExport("?Serialize@Time@System@@QEAAXAEAVSerializer@IO@@@Z"));
    if (!fn_Time_Serialize) {
        // Fallback: look up by IAT
        uintptr_t iatTimeSer = ResolveAddr(Module::Exe, 0x140152b08);
        if (iatTimeSer) fn_Time_Serialize = *reinterpret_cast<fn_Time_Serialize_t*>(iatTimeSer);
    }

    // FUN_140018020 — serialize field30 helper
    fn_SerializeField30 = reinterpret_cast<fn_SerializeField30_t>(
        ResolveAddr(Module::Exe, 0x140018020));

    if (!fn_Component_Serialize || !fn_Serializer_Ctor || !fn_Serializer_Dtor ||
        !fn_Serializer_SerializeInt || !fn_Serializer_GetVersion || !fn_Time_Serialize) {
#ifdef _DEBUG
        printf("[RawMaterialComponent] ERROR: failed to resolve Shutdown helpers\n");
        printf("[RawMaterialComponent]   Component_Serialize = %p\n", (void*)fn_Component_Serialize);
        printf("[RawMaterialComponent]   Time_Serialize = %p\n", (void*)fn_Time_Serialize);
#endif
        return;
    }

    uintptr_t target = ResolveAddr(Module::Exe, 0x140129330);
    // Prologue: PUSH RDI(2) + SUB RSP,0x40(4) + MOV qword,-0x2(9) = 15
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[RawMaterialComponent] Shutdown hooked at %p (prologue=15)\n", (void*)target);
#endif
}

void RemoveRawMaterialComponentShutdown() { RemoveHook(&g_hookShutdown); }

void InstallRawMaterialComponentSerialize() {
    g_pBuildingDataTypeId = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7cb8));

    uintptr_t iatCheckPtr = ResolveAddr(Module::Exe, 0x140152ab0);
    if (iatCheckPtr) fn_CheckPtr = *reinterpret_cast<fn_CheckPtr_t*>(iatCheckPtr);

    fn_RegisterRawMaterial = reinterpret_cast<fn_RegisterRawMaterial_t>(
        ResolveAddr(Module::Exe, 0x140091e60));

    g_pGameState = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x1401c3dd8));

    if (!fn_IsDerivedFrom || !g_pBuildingDataTypeId || !fn_CheckPtr ||
        !fn_RegisterRawMaterial || !g_pGameState) {
#ifdef _DEBUG
        printf("[RawMaterialComponent] ERROR: failed to resolve Serialize helpers\n");
#endif
        return;
    }

    uintptr_t target = ResolveAddr(Module::Exe, 0x1401292c0);
    // Prologue: MOV [RSP+0x8],RBX(5) + PUSH RDI(1) + SUB RSP,0x20(4) + MOV RBX,[RCX+0x18](4) = 14
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 14);
#ifdef _DEBUG
    printf("[RawMaterialComponent] Serialize hooked at %p (prologue=14)\n", (void*)target);
#endif
}

void RemoveRawMaterialComponentSerialize() { RemoveHook(&g_hookSerialize); }

void InstallRawMaterialComponentDraw() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1401291d0);
    // Prologue: PUSH RBX(1)+PUSH RSI(1)+PUSH RDI(1)+SUB RSP,0x30(4)+MOV qword,-0x2(9) = 16
    InstallHook(target, (void*)Reimpl_Draw, &g_hookDraw, 16);
#ifdef _DEBUG
    printf("[RawMaterialComponent] Draw hooked at %p (prologue=16, passthrough)\n", (void*)target);
#endif
}

void RemoveRawMaterialComponentDraw() { RemoveHook(&g_hookDraw); }

void InstallRawMaterialComponentUpdate() {
    g_pThresholdTime = reinterpret_cast<float*>(
        ResolveAddr(Module::Exe, 0x14017225c));

    // System::Time::Time(out, float) — via IAT at [0x140152b00]
    uintptr_t iatTimeFromFloat = ResolveAddr(Module::Exe, 0x140152b00);
    if (iatTimeFromFloat) fn_Time_FromFloat = *reinterpret_cast<fn_Time_FromFloat_t*>(iatTimeFromFloat);

    if (!fn_CheckPtr) {
        uintptr_t iatCheckPtr = ResolveAddr(Module::Exe, 0x140152ab0);
        if (iatCheckPtr) fn_CheckPtr = *reinterpret_cast<fn_CheckPtr_t*>(iatCheckPtr);
    }

    if (!g_pGameState) {
        g_pGameState = reinterpret_cast<void**>(
            ResolveAddr(Module::Exe, 0x1401c3dd8));
    }

    fn_MarkForCollection = reinterpret_cast<fn_MarkForCollection_t>(
        ResolveAddr(Module::Exe, 0x1400c2ea0));

    fn_AddToPending = reinterpret_cast<fn_AddToPending_t>(
        ResolveAddr(Module::Exe, 0x140003110));

    if (!g_pThresholdTime || !fn_Time_FromFloat || !fn_CheckPtr ||
        !g_pGameState || !fn_MarkForCollection || !fn_AddToPending) {
#ifdef _DEBUG
        printf("[RawMaterialComponent] ERROR: failed to resolve Update helpers\n");
#endif
        return;
    }

    uintptr_t target = ResolveAddr(Module::Exe, 0x140129130);
    // Prologue: PUSH RBX(2)+SUB RSP,0x20(4)+MOV RAX,[RDX](3)+MOVSS(8) = 17
    // NOTE: MOVSS at offset 9 is RIP-relative, but we don't use trampoline for Update
    InstallHook(target, (void*)Reimpl_Update, &g_hookUpdate, 17);
#ifdef _DEBUG
    printf("[RawMaterialComponent] Update hooked at %p (prologue=17)\n", (void*)target);
#endif
}

void RemoveRawMaterialComponentUpdate() { RemoveHook(&g_hookUpdate); }
