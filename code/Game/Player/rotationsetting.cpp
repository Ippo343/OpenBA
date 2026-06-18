// code/Game/Player/rotationsetting.cpp
// Standalone factory + vtable for RotationSetting (factory cascade).
//
// RotationSetting (size 0x18) extends ToolSetting. The exe factory
// (RotationSetting_Factory 0x140026d20): MemAlloc(0x18,8) ->
// Object::Internal::Internal -> install vtable @ 0x140160710 -> +0x10 = 0.
// 4-slot vtable (Dtor, slot1, GetTypeId, Serialize/Persist) — no per-type
// methods are reimplemented; this is a dormant landmine-removal factory.

#ifdef STANDALONE_EXE
#include "tool_base_standalone.h"
#include "Object/factory_registration.h"
#include "address_registry.h"
#include "resolve.h"
#include <cstdint>

static uint32_t RotationSetting_GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("RotationSetting");
    return c;
}
static void* g_RotationSetting_Vtable[4] = {
    (void*)&Tool_ScalarDtor,         // +0x00 Dtor (generic Internal teardown)
    (void*)&Tool_Slot1,              // +0x08 slot1 (null)
    (void*)&RotationSetting_GetTypeId, // +0x10 GetTypeId
    (void*)&Tool_Persist,            // +0x18 Serialize (base)
};
extern "C" void RegisterRotationSettingVtable() {
    RegisterExeAddr(0x140160710, (uintptr_t)g_RotationSetting_Vtable);
}
extern "C" void* RotationSetting_CreateInPlace_SA(void* mem) {
    return Tool_InitInPlace(mem, 0x18, g_RotationSetting_Vtable, 0);
}
extern "C" void* RotationSetting_Factory_SA(void) {
    return Tool_Alloc(0x18, g_RotationSetting_Vtable, 0);
}
REGISTER_TYPE(RotationSetting, "ToolSetting", &RotationSetting_Factory_SA, &RotationSetting_CreateInPlace_SA);
#endif // STANDALONE_EXE
