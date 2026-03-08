// engine/reimpl/toolbardescription.cpp
// Reimplementation of ToolbarDescription vtable methods from Application-x64.exe.
//
// ToolbarDescription (0x68 bytes), vtable at 0x140164980.
// Extends Object::External (0x20 bytes base).
//
// Vtable overrides (exe-range functions):
//   +0x00: 0x140039500 — Slot0 Dtor(self, flags)
//   +0x28: 0x14012da30 — Slot5 Serialize(self, stream)
//   +0x30: 0x1400394e0 — Slot6 Cleanup(self)
//
// Unhookable (too small or whole-function = 12 bytes):
//   +0x08: 0x140039480 — 8 bytes, Slot1 GetTypeInfo
//   +0x10: 0x140039470 — 7 bytes, Slot2 GetTypeId
//   +0x18: 0x14012db60 — 6 bytes, Slot3 GetSerializeVersion (returns 4)
//   +0x20: 0x14003a912 — 6 bytes, Slot4 JMP thunk
//   +0x38: 0x140001640 — 7 bytes, Slot7 thunk
//   +0x40: 0x140004950 — tiny, Slot8
//   +0x48: 0x1400394d0 — 12 bytes (whole function), Slot9 CreateInPlace

#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Vtable addresses (exe .rdata, resolved once at runtime)
// ============================================================
static const uintptr_t kVtable_ToolbarDescription = 0x140164980;

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookDtor;
static HookInfo g_hookSerialize;
static HookInfo g_hookCleanup;

// ============================================================
// Reimplemented functions
// ============================================================

// Dtor: calls FUN_14012db70 (complex inline cleanup helper), conditionally frees
// FUN_14012db70: sets vtable, destroys String array at +0x50 (count at +0x58),
// frees array, destroys Strings at +0x48/+0x40/+0x38/+0x28, sets base vtable, ~External
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "ToolbarDescription::Dtor";
    fn_ToolbarDescription_cleanup(self);
    if (flags & 1) fn_MemFreeStatic(self);
    return self;
}

// Serialize: ExternalPtr at +0x20, String at +0x28, ExternalPtr2 at +0x30,
// String at +0x38, String at +0x40, String at +0x48,
// count at +0x58, resize String array at +0x50 if loading, serialize each string,
// Serialize int at +0x64 (= decimal 100), int at +0x60
static uint64_t Reimpl_Serialize(void* self, void* stream) {
    g_lastHook = "ToolbarDescription::Serialize";
    uintptr_t s = (uintptr_t)self;
    uint8_t serializer[24];
    g_fn_Serializer_Ctor(serializer, stream, 0);

    fn_ExternalPtr_Serialize((void*)(s + 0x20), serializer);
    g_fn_String_Serialize((void*)(s + 0x28), serializer);
    fn_ExternalPtr_Serialize2((void*)(s + 0x30), serializer);
    g_fn_String_Serialize((void*)(s + 0x38), serializer);
    g_fn_String_Serialize((void*)(s + 0x40), serializer);
    g_fn_String_Serialize((void*)(s + 0x48), serializer);

    // Serialize count into a LOCAL copy (original does NOT serialize directly into +0x58)
    uint32_t count = *(uint32_t*)(s + 0x58);
    g_fn_Serialize_Int(serializer, (void*)&count, nullptr);

    // Check if loading (serializer internal mode == 0) — resize array with deserialized count
    uintptr_t ser_obj = *(uintptr_t*)((uintptr_t)serializer);
    if (*(int*)(ser_obj + 8) == 0) {
        fn_StringArray_Resize((void*)(s + 0x50), count);
    }

    // Re-read array pointer AFTER potential resize, iterate with deserialized count
    void* arr_ptr = *(void**)(s + 0x50);
    if ((int)count > 0) {
        for (uint32_t i = 0; i < count; ++i) {
            g_fn_String_Serialize((uint8_t*)arr_ptr + i * 8, serializer);
        }
    }

    g_fn_Serialize_Int(serializer, (void*)(s + 100), nullptr); // +0x64
    g_fn_Serialize_Int(serializer, (void*)(s + 0x60), nullptr);

    g_fn_Serializer_Dtor(serializer);
    return 1;
}

// Cleanup: virtual dtor(no-free) + ToolbarDescription_Ctor (0x14012dc30)
// From disassembly at 0x1400394e0: calls vtable[0](self,0) then JMP 0x14012dc30
// ToolbarDescription_Ctor: External::External + set vtable + zero all fields + init defaults
static void Reimpl_Cleanup(void* self) {
    g_lastHook = "ToolbarDescription::Cleanup";
    typedef void* (*fn_VDtor_t)(void*, uint32_t);
    ((fn_VDtor_t)(*(void***)self)[0])(self, 0);
    fn_External_External(self);
    uintptr_t s = (uintptr_t)self;
    *(void**)self = (void*)ResolveAddr(Module::Exe, kVtable_ToolbarDescription);
    *(uint64_t*)(s + 0x20) = 0;
    *(uint64_t*)(s + 0x28) = 0;
    *(uint64_t*)(s + 0x30) = 0;
    *(uint64_t*)(s + 0x38) = 0;
    *(uint64_t*)(s + 0x40) = 0;
    *(uint64_t*)(s + 0x48) = 0;
    *(uint64_t*)(s + 0x50) = 0;
    *(uint64_t*)(s + 0x58) = 0;
    *(uint32_t*)(s + 0x60) = 0xffffffffu; // default -1
    *(uint32_t*)(s + 100)  = 0;           // +0x64 = 0
}

// ============================================================
// Install / Remove
// ============================================================
void InstallToolbarDescriptionDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140039500);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 12);
#ifdef _DEBUG
    printf("[ToolbarDescription] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveToolbarDescriptionDtor() { RemoveHook(&g_hookDtor); }

void InstallToolbarDescriptionSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14012da30);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[ToolbarDescription] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveToolbarDescriptionSerialize() { RemoveHook(&g_hookSerialize); }

void InstallToolbarDescriptionCleanup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400394e0);
    InstallHook(target, (void*)Reimpl_Cleanup, &g_hookCleanup, 14);
#ifdef _DEBUG
    printf("[ToolbarDescription] Cleanup hooked at %p\n", (void*)target);
#endif
}
void RemoveToolbarDescriptionCleanup() { RemoveHook(&g_hookCleanup); }
