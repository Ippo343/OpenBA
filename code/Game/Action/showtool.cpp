// code/Game/Action/showtool.cpp
// Clean C++ reimplementation of ShowTool action methods.
// Source: Game/Action/ShowTool.cpp (from profile build debug strings)
//
// ShowTool -- 0x30 bytes. Shows a tool UI for a citizen.
// Inherits: Object::Internal -> Action (0x20) -> ShowTool
// Vtable at 0x1401589b0 (10 slots).
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h -- it is portable for standalone exe.

#include "ShowTool.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Local function pointers resolved at init time
// (NOT in engine_funcs.h -- ShowTool-specific helpers)
// ============================================================

// Object::InternalBasePtr::CheckPtr -- via IAT at 0x140152ab0
static void (*s_fn_CheckPtr)(void* ibp) = nullptr;

// Object::InternalBasePtr::Serialize -- via export
static void (*s_fn_IBP_Serialize)(void* ibp, void* serializer) = nullptr;

// FUN_1401176e0 -- hide tool on entity
static void (*s_fn_HideTool)(void* entity) = nullptr;

// FUN_1401173b0 -- show tool with specific ID on entity
static void (*s_fn_ShowToolOnEntity)(void* entity, int toolId) = nullptr;

// DAT_1401c7158 -- TypeInfo for entity type check
static uint32_t* s_TypeInfo_ShowToolEntity = nullptr;

static bool s_showtool_funcs_resolved = false;

// ============================================================
// Resolution -- called once from Install
// ============================================================
bool ShowTool_ResolveFuncs() {
    if (s_showtool_funcs_resolved) return true;

    // CheckPtr via IAT at [0x140152ab0]
    uintptr_t iatEntry = ResolveAddr(Module::Exe, 0x140152ab0);
    if (iatEntry) s_fn_CheckPtr = *reinterpret_cast<void(**)(void*)>(iatEntry);

    s_TypeInfo_ShowToolEntity = (uint32_t*)ResolveAddr(Module::Exe, 0x1401c7158);
    s_fn_HideTool = reinterpret_cast<void(*)(void*)>(
        ResolveAddr(Module::Exe, 0x1401176e0));
    s_fn_ShowToolOnEntity = reinterpret_cast<void(*)(void*, int)>(
        ResolveAddr(Module::Exe, 0x1401173b0));
    s_fn_IBP_Serialize = reinterpret_cast<void(*)(void*, void*)>(
        ResolveExport("?Serialize@InternalBasePtr@Object@@QEAAXAEAVSerializer@IO@@@Z"));

    bool ok = s_fn_CheckPtr && s_TypeInfo_ShowToolEntity &&
              s_fn_HideTool && s_fn_ShowToolOnEntity && s_fn_IBP_Serialize &&
              g_fn_Serializer_Ctor && g_fn_Serializer_Dtor &&
              g_fn_Serialize_Bool && g_fn_Serialize_Int &&
              fn_IsDerivedFrom;

#ifdef _DEBUG
    printf("[ShowTool] ResolveFuncs: CheckPtr=%p HideTool=%p ShowTool=%p TypeInfo=%p\n",
           (void*)s_fn_CheckPtr, (void*)s_fn_HideTool,
           (void*)s_fn_ShowToolOnEntity, (void*)s_TypeInfo_ShowToolEntity);
    printf("[ShowTool]   IBPSer=%p\n", (void*)s_fn_IBP_Serialize);
    if (!ok) printf("[ShowTool] ERROR: some function pointers not resolved!\n");
#endif

    s_showtool_funcs_resolved = ok;
    return ok;
}

// ============================================================
// ShowTool::Serialize (0x140042780)
// Serializes: status(+0x18), entity ptr(+0x20) with type check, int(+0x28)
// ============================================================
void ShowTool_Serialize_Impl(void* self, void* stream) {
    uint8_t* s = (uint8_t*)self;
    uint8_t ser1[24];
    uint8_t ser2[24];

    g_fn_Serializer_Ctor(ser1, stream, 0);
    g_fn_Serialize_Bool(ser1, s + 0x18, nullptr);
    g_fn_Serializer_Dtor(ser1);

    g_fn_Serializer_Ctor(ser2, stream, 0);
    s_fn_IBP_Serialize(s + 0x20, ser2);
    if (*(void**)(s + 0x20) != nullptr) {
        if (!fn_IsDerivedFrom(*(void**)(s + 0x20), *s_TypeInfo_ShowToolEntity)) {
            *(void**)(s + 0x20) = nullptr;
        }
    }
    g_fn_Serialize_Int(ser2, s + 0x28, nullptr);
    g_fn_Serializer_Dtor(ser2);
}

// ============================================================
// ShowTool::AbstractImpl / _purecall (0x140042710)
// If toolId > 10 and != 0xFFFFFFFE, hide tool. Otherwise show tool.
// Always returns 1.
// ============================================================
int ShowTool_AbstractImpl_Impl(void* self, void* actionStack) {
    (void)actionStack;
    uint8_t* s = (uint8_t*)self;
    uint32_t toolId = *(uint32_t*)(s + 0x28);

    if (toolId > 10 && toolId != 0xFFFFFFFE) {
        s_fn_CheckPtr(s + 0x20);
        s_fn_HideTool(*(void**)(s + 0x20));
        return 1;
    }

    s_fn_CheckPtr(s + 0x20);
    s_fn_ShowToolOnEntity(*(void**)(s + 0x20), (int)toolId);
    return 1;
}

// ============================================================
// ShowTool::Exit (0x140042700)
// 3 bytes -- trivial return 0. Too small to hook.
// Implemented for standalone exe completeness.
// ============================================================
int ShowTool_Exit_Impl(void* /*self*/) {
    return 0;
}

// ============================================================
// ShowTool::CheckEntity (0x1400426d0)
// Hides tool on the entity at +0x20.
// ============================================================
void ShowTool_CheckEntity_Impl(void* self) {
    uint8_t* s = (uint8_t*)self;
    s_fn_CheckPtr(s + 0x20);
    s_fn_HideTool(*(void**)(s + 0x20));
}

// ============================================================
// ShowTool::GetTarget (0x140042680)
// Returns true if entity at +0x20's parent (+0x10) matches the param.
// ============================================================
bool ShowTool_GetTarget_Impl(void* self, void* param) {
    uint8_t* s = (uint8_t*)self;
    s_fn_CheckPtr(s + 0x20);
    void* entityParent = *(void**)(*(uint8_t**)(s + 0x20) + 0x10);
    s_fn_CheckPtr(param);
    return entityParent == *(void**)param;
}
