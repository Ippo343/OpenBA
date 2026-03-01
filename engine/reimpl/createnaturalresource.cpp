// engine/reimpl/createnaturalresource.cpp
// Reimplementation of CreateNaturalResource action methods from Application-x64.exe.
//
// CreateNaturalResource — 0x38 bytes. Creates a natural resource entity in the world.
// Inherits: Object::Internal -> Action (0x20) -> CreateNaturalResource
// Vtable at 0x14015fca0 (10 slots).
//
// Overrides vs Action base:
//   Serialize  (0x1400c6890) — serializes status, StringHash(+0x20), int(+0x30), versioned entity(+0x28)
//   _purecall  (0x1400c6650) — complex: allocates entity, sets up model/NR/growth components, places in world
//   GetTarget  (0x1400c6610) — returns true if param matches entity at +0x28
//   Execute uses base. Enter/Exit/CheckEntity use base.
//
// Field layout (0x38 bytes):
//   +0x20: void*    resourceType   (StringHash or type ref, serialized via FUN_140002340)
//   +0x28: void*    parentEntity   (entity ref, ComponentEntity type)
//   +0x30: int32_t  resourceSlot
//   +0x34: padding

#include "CreateNaturalResource.h"
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
typedef int (*fn_Serializer_GetVersion_t)(void* self);
static fn_Serializer_GetVersion_t fn_Serializer_GetVersion = nullptr;

// FUN_140002340 — serialize StringHash/type ref
typedef void (*fn_SerializeTypeRef_t)(void* field, void* serializer);
static fn_SerializeTypeRef_t fn_SerializeTypeRef = nullptr;

static uint32_t* g_TypeInfo_ComponentEntity = nullptr;

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookSerialize;
static HookInfo g_hookPurecall;
static HookInfo g_hookGetTarget;

// ============================================================
// CreateNaturalResource::Serialize (0x1400c6890)
// Serializes: status(+0x18), typeRef(+0x20), int(+0x30), versioned entity(+0x28)
// ============================================================
static void Reimpl_Serialize(void* self, void* stream) {
    g_lastHook = "CreateNaturalResource::Serialize";
    uint8_t* s = (uint8_t*)self;
    uint8_t ser1[16];
    uint8_t ser2[24];

    // Serialize status
    fn_Serializer_Ctor(ser1, stream, 0);
    fn_Serialize_Bool(ser1, s + 0x18, nullptr);
    fn_Serializer_Dtor(ser1);

    // Serialize resource data (versioned)
    fn_Serializer_Ctor(ser2, stream, 1);
    fn_SerializeTypeRef(s + 0x20, ser2);
    fn_Serialize_Int(ser2, s + 0x30, nullptr);
    int version = fn_Serializer_GetVersion(ser2);
    if (version > 0) {
        fn_IBP_Serialize(s + 0x28, ser2);
        if (*(void**)(s + 0x28) != nullptr) {
            if (!fn_IsDerivedFrom(*(void**)(s + 0x28), *g_TypeInfo_ComponentEntity)) {
                *(void**)(s + 0x28) = nullptr;
            }
        }
    }
    fn_Serializer_Dtor(ser2);
}

// ============================================================
// CreateNaturalResource::_purecall (0x1400c6650)
// Complex function (132 instructions) — creates entity, sets up components.
// Passthrough via trampoline.
// ============================================================
typedef uint64_t (*fn_CNRPurecall_t)(void* self);

static uint64_t Reimpl_Purecall(void* self) {
    g_lastHook = "CreateNaturalResource::Purecall";
    return ((fn_CNRPurecall_t)g_hookPurecall.trampoline)(self);
}

// ============================================================
// CreateNaturalResource::GetTarget (0x1400c6610)
// Returns true if param matches entity at +0x28.
// ============================================================
static bool Reimpl_GetTarget(void* self, void* param) {
    g_lastHook = "CreateNaturalResource::GetTarget";
    uint8_t* s = (uint8_t*)self;
    fn_CheckPtr(s + 0x28);
    return *(void**)param == *(void**)(s + 0x28);
}

// ============================================================
// Install / Remove
// ============================================================

static void ResolveHelpers() {
    // CheckPtr is called indirectly via IAT at [0x140152ab0]
    uintptr_t iatEntry = ResolveAddr(Module::Exe, 0x140152ab0);
    if (iatEntry) fn_CheckPtr = *reinterpret_cast<fn_CheckPtr_t*>(iatEntry);
    g_TypeInfo_ComponentEntity = (uint32_t*)ResolveAddr(Module::Exe, 0x1401c4218);
    fn_SerializeTypeRef = (fn_SerializeTypeRef_t)ResolveAddr(Module::Exe, 0x140002340);
    fn_Serializer_Ctor = (fn_Serializer_Ctor_t)ResolveExport("?Serializer@IO@@QEAA@PEAVStream@1@H@Z");
    fn_Serializer_Dtor = (fn_Serializer_Dtor_t)ResolveExport("??1Serializer@IO@@QEAA@XZ");
    fn_Serialize_Bool = (fn_Serialize_Bool_t)ResolveExport("?Serialize@Serializer@IO@@QEAAXAEA_NPEBD@Z");
    fn_Serialize_Int = (fn_Serialize_Int_t)ResolveExport("?Serialize@Serializer@IO@@QEAAXAEAint@PEBD@Z");
    fn_Serializer_GetVersion = (fn_Serializer_GetVersion_t)ResolveExport("?GetVersion@Serializer@IO@@QEAAHXZ");
    fn_IBP_Serialize = (fn_IBP_Serialize_t)ResolveExport("?Serialize@InternalBasePtr@Object@@QEAAXAEAVSerializer@IO@@@Z");
}

void InstallCreateNaturalResourceSerialize() {
    ResolveHelpers();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c6890);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[CreateNaturalResource] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveCreateNaturalResourceSerialize() { RemoveHook(&g_hookSerialize); }

void InstallCreateNaturalResourcePurecall() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c6650);
    InstallHook(target, (void*)Reimpl_Purecall, &g_hookPurecall, 20);
#ifdef _DEBUG
    printf("[CreateNaturalResource] Purecall hooked (passthrough) at %p\n", (void*)target);
#endif
}
void RemoveCreateNaturalResourcePurecall() { RemoveHook(&g_hookPurecall); }

void InstallCreateNaturalResourceGetTarget() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c6610);
    InstallHook(target, (void*)Reimpl_GetTarget, &g_hookGetTarget, 14);
#ifdef _DEBUG
    printf("[CreateNaturalResource] GetTarget hooked at %p\n", (void*)target);
#endif
}
void RemoveCreateNaturalResourceGetTarget() { RemoveHook(&g_hookGetTarget); }
