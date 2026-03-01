// engine/reimpl/showtool.cpp
// Reimplementation of ShowTool action methods from Application-x64.exe.
//
// ShowTool — 0x30 bytes. Shows a tool UI for a citizen.
// Inherits: Object::Internal -> Action (0x20) -> ShowTool
// Vtable at 0x1401589b0 (10 slots).
//
// Overrides vs Action base:
//   Serialize   (0x140042780) — serializes status, entity ptr(+0x20) with DAT_1401c7158 type check, int(+0x28)
//   _purecall   (0x140042710) — shows/hides tool based on tool ID at +0x28
//   Exit        (0x140042700) — 3 bytes (ret 0), too small to hook
//   CheckEntity (0x1400426d0) — hides tool on entity at +0x20
//   GetTarget   (0x140042680) — compares entity's parent with param
//   Execute uses base (0x1400c4a90). Enter uses base (0x1400c4b90).

#include "ShowTool.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Helper function pointers
// ============================================================

typedef void (*fn_CheckPtr_t)(void* ibp);
static fn_CheckPtr_t fn_CheckPtr = nullptr;

typedef void (*fn_IBP_Serialize_t)(void* ibp, void* serializer);
static fn_IBP_Serialize_t fn_IBP_Serialize = nullptr;

typedef void (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t fn_Serializer_Ctor = nullptr;
typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t fn_Serializer_Dtor = nullptr;
typedef void (*fn_Serialize_Bool_t)(void* self, void* field, const char* name);
static fn_Serialize_Bool_t fn_Serialize_Bool = nullptr;
typedef void (*fn_Serialize_Int_t)(void* self, void* field, const char* name);
static fn_Serialize_Int_t fn_Serialize_Int = nullptr;

// FUN_1401176e0 — hide tool on entity (used by CheckEntity and _purecall when toolId > 10)
typedef void (*fn_HideTool_t)(void* entity);
static fn_HideTool_t fn_HideTool = nullptr;

// FUN_1401173b0 — show tool with specific ID on entity
typedef void (*fn_ShowToolOnEntity_t)(void* entity, int toolId);
static fn_ShowToolOnEntity_t fn_ShowToolOnEntity = nullptr;

// Type check for ShowTool entity (DAT_1401c7158)
static uint32_t* g_TypeInfo_ShowToolEntity = nullptr;

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookSerialize;
static HookInfo g_hookPurecall;
static HookInfo g_hookCheckEntity;
static HookInfo g_hookGetTarget;

// ============================================================
// ShowTool::Serialize (0x140042780)
// Serializes: status(+0x18), entity ptr(+0x20) with type check, int(+0x28)
// ============================================================
static void Reimpl_Serialize(void* self, void* stream) {
    uint8_t* s = (uint8_t*)self;
    uint8_t ser1[16];
    uint8_t ser2[24];

    fn_Serializer_Ctor(ser1, stream, 0);
    fn_Serialize_Bool(ser1, s + 0x18, nullptr);
    fn_Serializer_Dtor(ser1);

    fn_Serializer_Ctor(ser2, stream, 0);
    fn_IBP_Serialize(s + 0x20, ser2);
    if (*(void**)(s + 0x20) != nullptr) {
        if (!fn_IsDerivedFrom(*(void**)(s + 0x20), *g_TypeInfo_ShowToolEntity)) {
            *(void**)(s + 0x20) = nullptr;
        }
    }
    fn_Serialize_Int(ser2, s + 0x28, nullptr);
    fn_Serializer_Dtor(ser2);
}

// ============================================================
// ShowTool::_purecall (0x140042710)
// If toolId > 10 and != 0xFFFFFFFE, hide tool. Otherwise show tool with specific ID.
// Always returns 1 (action remains active / done in one tick).
// ============================================================
static uint64_t Reimpl_Purecall(void* self) {
    uint8_t* s = (uint8_t*)self;
    uint32_t toolId = *(uint32_t*)(s + 0x28);

    if (toolId > 10 && toolId != 0xFFFFFFFE) {
        fn_CheckPtr(s + 0x20);
        fn_HideTool(*(void**)(s + 0x20));
        return 1;
    }

    fn_CheckPtr(s + 0x20);
    fn_ShowToolOnEntity(*(void**)(s + 0x20), (int)toolId);
    return 1;
}

// ============================================================
// ShowTool::CheckEntity (0x1400426d0)
// Hides tool on the entity at +0x20.
// ============================================================
static void Reimpl_CheckEntity(void* self) {
    uint8_t* s = (uint8_t*)self;
    fn_CheckPtr(s + 0x20);
    fn_HideTool(*(void**)(s + 0x20));
}

// ============================================================
// ShowTool::GetTarget (0x140042680)
// Returns true if entity at +0x20's parent (+0x10) matches the param.
// ============================================================
static bool Reimpl_GetTarget(void* self, void* param) {
    uint8_t* s = (uint8_t*)self;
    fn_CheckPtr(s + 0x20);
    void* entityParent = *(void**)(*(uint8_t**)(s + 0x20) + 0x10);
    fn_CheckPtr(param);
    return entityParent == *(void**)param;
}

// ShowTool::Exit at 0x140042700 is 3 bytes (ret 0), too small to hook.

// ============================================================
// Install / Remove
// ============================================================

static void ResolveHelpers() {
    // CheckPtr is called indirectly via IAT at [0x140152ab0]
    uintptr_t iatEntry = ResolveAddr(Module::Exe, 0x140152ab0);
    if (iatEntry) fn_CheckPtr = *reinterpret_cast<fn_CheckPtr_t*>(iatEntry);
    g_TypeInfo_ShowToolEntity = (uint32_t*)ResolveAddr(Module::Exe, 0x1401c7158);
    fn_HideTool = (fn_HideTool_t)ResolveAddr(Module::Exe, 0x1401176e0);
    fn_ShowToolOnEntity = (fn_ShowToolOnEntity_t)ResolveAddr(Module::Exe, 0x1401173b0);
    fn_Serializer_Ctor = (fn_Serializer_Ctor_t)ResolveExport("?Serializer@IO@@QEAA@PEAVStream@1@H@Z");
    fn_Serializer_Dtor = (fn_Serializer_Dtor_t)ResolveExport("??1Serializer@IO@@QEAA@XZ");
    fn_Serialize_Bool = (fn_Serialize_Bool_t)ResolveExport("?Serialize@Serializer@IO@@QEAAXAEA_NPEBD@Z");
    fn_Serialize_Int = (fn_Serialize_Int_t)ResolveExport("?Serialize@Serializer@IO@@QEAAXAEAint@PEBD@Z");
    fn_IBP_Serialize = (fn_IBP_Serialize_t)ResolveExport("?Serialize@InternalBasePtr@Object@@QEAAXAEAVSerializer@IO@@@Z");
}

void InstallShowToolSerialize() {
    ResolveHelpers();
    uintptr_t target = ResolveAddr(Module::Exe, 0x140042780);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[ShowTool] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveShowToolSerialize() { RemoveHook(&g_hookSerialize); }

void InstallShowToolPurecall() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140042710);
    InstallHook(target, (void*)Reimpl_Purecall, &g_hookPurecall, 13);
#ifdef _DEBUG
    printf("[ShowTool] Purecall hooked at %p\n", (void*)target);
#endif
}
void RemoveShowToolPurecall() { RemoveHook(&g_hookPurecall); }

void InstallShowToolCheckEntity() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400426d0);
    InstallHook(target, (void*)Reimpl_CheckEntity, &g_hookCheckEntity, 12);
#ifdef _DEBUG
    printf("[ShowTool] CheckEntity hooked at %p\n", (void*)target);
#endif
}
void RemoveShowToolCheckEntity() { RemoveHook(&g_hookCheckEntity); }

void InstallShowToolGetTarget() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140042680);
    InstallHook(target, (void*)Reimpl_GetTarget, &g_hookGetTarget, 13);
#ifdef _DEBUG
    printf("[ShowTool] GetTarget hooked at %p\n", (void*)target);
#endif
}
void RemoveShowToolGetTarget() { RemoveHook(&g_hookGetTarget); }
