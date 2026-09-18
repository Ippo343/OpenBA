// code/Game/Action/removeanimal.cpp
// Clean C++ reimplementation of RemoveAnimal action methods.
// Source: Game/Action/RemoveAnimal.cpp (from profile build debug strings)
//
// RemoveAnimal -- smallest action (0x28 bytes). Removes an animal entity from the world.
// Inherits: Object::Internal -> Action (0x20) -> RemoveAnimal
// Factory: 0x140022e70, Vtable: 0x14015ffe0
//
// Vtable (10 slots):
//   [0] +0x00 ScalarDtor    = 0x140022ee0
//   [1] +0x08 VectorDtor    = 0x140022e60 (returns type ptr, unhookable)
//   [2] +0x10 BaseDtor      = 0x140022e50 (returns TypeInfo, unhookable)
//   [3] +0x18 Serialize     = 0x1400ca870
//   [4] +0x20 AbstractImpl  = 0x1400ca7f0  (DISABLED: RIP-relative MOV)
//   [5] +0x28 Execute       = 0x1400c4a90  (base Action, returns 0)
//   [6] +0x30 Enter         = 0x1400c4b90  (base Action, void nop)
//   [7] +0x38 Exit          = 0x1400c4ba0  (base Action, void nop)
//   [8] +0x40 CheckEntity   = 0x1400ca760  (DISABLED: RIP-relative CALL)
//   [9] +0x48 GetTarget     = 0x1400ca720
//
// Decompiled from Application-x64.exe using Ghidra.

#include "RemoveAnimal.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "Component/coll_bridge.h"
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B

// ============================================================
// Local function pointers resolved at init time
// (NOT in engine_funcs.h -- RemoveAnimal-specific helpers)
// ============================================================

// Object::Internal::~Internal(this) -- via IAT at 0x140152770
static void (*s_fn_ObjectInternal_Dtor)(void* self) = nullptr;

// Object::InternalBasePtr::Serialize(this, serializer) -- via IAT at 0x140152aa0
static void (*s_fn_IBP_Serialize)(void* basePtr, void* serializer) = nullptr;

// FUN_1400c2ea0 -- marks entity for removal (sets bit 10 of flags at +0x40)
static void (*s_fn_MarkEntityRemoval)(void* entity) = nullptr;

// FUN_140003110 (array push) is the exe-side Collection::Array<Ptr8>::
// SetSize(n,fill) monomorphisation -- ENDGAME B.3.2 step 2 converted its one
// call site below to a GameColl::AsArray<GameColl::Ptr8> member call, so its
// resolved-pointer stand-in is gone.

// FUN_140005500 -- get model component from entity
// Params: entity, outIBP (InternalBasePtr*)
static void (*s_fn_GetModelComp)(void* entity, void* outIBP) = nullptr;

// FUN_1400c3470 -- notify entity changed (triggers visual update)
static void (*s_fn_NotifyEntityChanged)(void* entity) = nullptr;

// TypeInfo_ComponentEntity (DAT_1401c4218)
static uint32_t* s_typeId_ComponentEntity = nullptr;

// TypeInfo_RemoveAnimal (DAT_1401c6080)
static uint32_t* s_typeInfo_RemoveAnimal = nullptr;

// DAT_1401c6088 -- VectorDtor return value
static uint64_t* s_dat_VectorDtorReturn = nullptr;

// Vtable addresses for dtor chain
static uintptr_t s_vtable_RemoveAnimal = 0;
static uintptr_t s_vtable_Action = 0;

static bool s_funcs_resolved = false;

// ============================================================
// Resolution -- called once from Install
// ============================================================
bool RemoveAnimal_ResolveFuncs() {
    if (s_funcs_resolved) return true;

    // Object::Internal dtor via IAT
    uintptr_t iatDtor = ResolveAddr(Module::Exe, 0x140152770);
    if (iatDtor) s_fn_ObjectInternal_Dtor = *reinterpret_cast<void(**)(void*)>(iatDtor);

    // InternalBasePtr::Serialize via IAT at 0x140152aa0
    uintptr_t iatIBPSer = ResolveAddr(Module::Exe, 0x140152aa0);
    if (iatIBPSer) s_fn_IBP_Serialize = *reinterpret_cast<void(**)(void*, void*)>(iatIBPSer);

    // FUN_1400c2ea0 -- mark entity removal
    s_fn_MarkEntityRemoval = (void(*)(void*))ResolveAddr(Module::Exe, 0x1400c2ea0);

    // FUN_140005500 -- get model component
    s_fn_GetModelComp = (void(*)(void*, void*))ResolveAddr(Module::Exe, 0x140005500);

    // FUN_1400c3470 -- notify entity changed
    s_fn_NotifyEntityChanged = (void(*)(void*))ResolveAddr(Module::Exe, 0x1400c3470);

    // Type IDs
    s_typeId_ComponentEntity = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c4218));
    s_typeInfo_RemoveAnimal = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c6080));
    s_dat_VectorDtorReturn = reinterpret_cast<uint64_t*>(
        ResolveAddr(Module::Exe, 0x1401c6088));

    // Vtable addresses
    s_vtable_RemoveAnimal = ResolveAddr(Module::Exe, 0x14015ffe0);
    s_vtable_Action = ResolveAddr(Module::Exe, 0x14015faa0);

    bool ok = s_fn_ObjectInternal_Dtor && s_fn_IBP_Serialize &&
              s_fn_MarkEntityRemoval &&
              s_fn_GetModelComp && s_fn_NotifyEntityChanged &&
              s_typeId_ComponentEntity && s_typeInfo_RemoveAnimal &&
              s_dat_VectorDtorReturn &&
              s_vtable_RemoveAnimal && s_vtable_Action &&
              fn_InternalBasePtr_CheckPtr && fn_InternalBasePtr_DeletePtr &&
              fn_IsDerivedFrom && fn_MemFreeStatic &&
              g_fn_Serializer_Ctor && g_fn_Serializer_Dtor &&
              g_fn_Serialize_Bool && g_pGameWorldSingleton;

#ifdef _DEBUG
    printf("[RemoveAnimal] ResolveFuncs: ObjDtor=%p IBPSer=%p MarkRemove=%p\n",
           (void*)s_fn_ObjectInternal_Dtor, (void*)s_fn_IBP_Serialize,
           (void*)s_fn_MarkEntityRemoval);
    printf("[RemoveAnimal]   GetModel=%p Notify=%p typeIdCE=%p typeInfoRA=%p\n",
           (void*)s_fn_GetModelComp, (void*)s_fn_NotifyEntityChanged,
           (void*)s_typeId_ComponentEntity, (void*)s_typeInfo_RemoveAnimal);
    if (!ok) printf("[RemoveAnimal] ERROR: some function pointers not resolved!\n");
#endif

    s_funcs_resolved = ok;
    return ok;
}

// ============================================================
// Slot 0: ScalarDeletingDtor (0x140022ee0)
// Sets vtables to RemoveAnimal then Action, cleanup entity ref at +0x10,
// calls ~Internal, optionally frees memory.
// ============================================================
void* RemoveAnimal_ScalarDtor_Impl(RemoveAnimal* self, uint32_t flags) {
    uint8_t* s = (uint8_t*)self;

    // Set RemoveAnimal vtable (for correct virtual dispatch during destruction)
    *(uintptr_t*)s = s_vtable_RemoveAnimal;
    // Set Action vtable (base class destructor phase)
    *(uintptr_t*)s = s_vtable_Action;

    // If entity ref at +0x10 is non-null, delete it
    if (*(void**)(s + 0x10) != nullptr) {
        reinterpret_cast< ::Object::InternalBasePtr*>(s + 0x10)->DeletePtr();   // B.3.3 bridge
    }

    // Call Object::Internal::~Internal
    s_fn_ObjectInternal_Dtor(self);

    if (flags & 1) {
        fn_MemFreeStatic(self);
    }
    return self;
}

// ============================================================
// Slot 1: VectorDeletingDtor (0x140022e60) -- unhookable (7 bytes, RIP-relative)
// Returns DAT_1401c6088 (type factory pointer).
// ============================================================
uint64_t RemoveAnimal_VectorDtor_Impl() {
    return *s_dat_VectorDtorReturn;
}

// ============================================================
// Slot 2: BaseDtor (0x140022e50) -- unhookable (7 bytes, RIP-relative)
// Returns TypeInfo for RemoveAnimal (DAT_1401c6080).
// ============================================================
uint32_t RemoveAnimal_BaseDtor_Impl() {
    return *s_typeInfo_RemoveAnimal;
}

// ============================================================
// Slot 3: Serialize (0x1400ca870)
// Serializes status byte (+0x18) and entity ref at +0x20 with ComponentEntity
// type validation on deserialization.
// ============================================================
void RemoveAnimal_Serialize_Impl(RemoveAnimal* self, void* stream) {
    // Reached through the registered vtable, which the factory path never
    // has to touch — an exe-ctor-built action arrives here with the file
    // statics still null. Lazy-resolve like the other entry points do.
    RemoveAnimal_ResolveFuncs();
    uint8_t* s = (uint8_t*)self;
    uint8_t localSerializer1[16];
    uint8_t localSerializer2[24];

    // Serialize status byte (+0x18)
    g_fn_Serializer_Ctor(localSerializer1, stream, 0);
    g_fn_Serialize_Bool(localSerializer1, s + 0x18, nullptr);
    g_fn_Serializer_Dtor(localSerializer1);

    // Serialize entity ref at +0x20
    g_fn_Serializer_Ctor(localSerializer2, stream, 0);
    s_fn_IBP_Serialize(s + 0x20, localSerializer2);

    // Type-check: if the deserialized ptr is not a ComponentEntity, null it
    void* entityRef = *(void**)(s + 0x20);
    if (entityRef != nullptr) {
        if (!fn_IsDerivedFrom(entityRef, *s_typeId_ComponentEntity)) {
            *(void**)(s + 0x20) = nullptr;
        }
    }

    g_fn_Serializer_Dtor(localSerializer2);
}

// ============================================================
// Slot 4: AbstractImpl (0x1400ca7f0) -- DISABLED hook (RIP-relative MOV in prologue)
// Marks the animal entity at +0x20 for removal, queues to cleanup list.
// Returns 0 (action complete).
// ============================================================
int RemoveAnimal_AbstractImpl_Impl(RemoveAnimal* self, void* actionStack) {
    RemoveAnimal_ResolveFuncs();   // lazy-resolve (exe-ctor-built action)
    (void)actionStack;
    uint8_t* s = (uint8_t*)self;
    void* entityRef = *(void**)(s + 0x20);

    // ~310 kills produced ZERO component dumps below, and the dump sits inside the
    // `entityRef != nullptr` arm — so "RemoveAnimal never ran" and "RemoveAnimal ran with
    // no animal" look identical from there. Count at the TOP, before any branch, and
    // report both arms.
    {
        static long long s_calls = 0, s_null = 0;
        ++s_calls;
        if (!entityRef) ++s_null;
    }

    // Get cleanup manager from game world singleton
    uintptr_t gameWorld = (uintptr_t)*g_pGameWorldSingleton;
    void* cleanupMgr = *(void**)(gameWorld + 0xe8);

    if (entityRef != nullptr) {
        // Copy to local for CheckPtr
        void* localRef = entityRef;

        fn_InternalBasePtr_CheckPtr(&localRef);

        // Check bit 10 of flags at entity+0x40 (already marked for removal?)
        uint32_t flags = *(uint32_t*)((uint8_t*)localRef + 0x40);
        if (((flags >> 10) & 1) == 0) {
            fn_InternalBasePtr_CheckPtr(&localRef);

            // Mark entity for removal
            s_fn_MarkEntityRemoval(localRef);

            // Push to cleanup array.
            // B.3.2 bridge: FUN_140003110 == Collection::Array<Ptr8>::SetSize(n,fill)
            // (3-arg form; ActHunt_DynArraySetCount_3110 in action_base_hunt.cpp).
            int count = *(int*)((uint8_t*)cleanupMgr + 0xd8);
            GameColl::AsArray<GameColl::Ptr8>((uint8_t*)cleanupMgr + 0xd0)
                .SetSize(count + 1, *reinterpret_cast<const GameColl::Ptr8*>(&localRef));
        }
    }
    return 0;
}

// ============================================================
// Slot 8: CheckEntity (0x1400ca760) -- DISABLED hook (RIP-relative CALL in prologue)
// Clears "in use" bit (bit 20) on entity's model component flags.
// If the low 16 bits of flags changed, notifies parent entity.
// ============================================================
void RemoveAnimal_CheckEntity_Impl(RemoveAnimal* self) {
    RemoveAnimal_ResolveFuncs();   // lazy-resolve (exe-ctor-built action)
    uint8_t* s = (uint8_t*)self;
    void* localIBP[4];

    fn_InternalBasePtr_CheckPtr(s + 0x20);
    void* entity20 = *(void**)(s + 0x20);
    s_fn_GetModelComp(entity20, localIBP);

    fn_InternalBasePtr_CheckPtr(localIBP);
    uint8_t* modelComp = (uint8_t*)(localIBP[0]);
    uint32_t flagsOld = *(uint32_t*)(modelComp + 0x20);

    if ((flagsOld >> 20) & 1) {
        fn_InternalBasePtr_CheckPtr(localIBP);
        // Re-read (CheckPtr may have updated)
        modelComp = (uint8_t*)(localIBP[0]);
        flagsOld = *(uint32_t*)(modelComp + 0x20);
        uint32_t flagsNew = flagsOld & 0xFFEFFFFF;  // clear bit 20
        *(uint32_t*)(modelComp + 0x20) = flagsNew;

        if (((flagsNew ^ flagsOld) & 0xFFFF) != 0) {
            if (*(void**)(modelComp + 0x10) != nullptr) {
                fn_InternalBasePtr_CheckPtr(modelComp + 0x10);
                s_fn_NotifyEntityChanged(*(void**)(modelComp + 0x10));
            }
        }
    }
}

// ============================================================
// Slot 9: GetTarget (0x1400ca720)
// Returns true if *param matches the entity at +0x20.
// ============================================================
bool RemoveAnimal_GetTarget_Impl(RemoveAnimal* self, void* param) {
    RemoveAnimal_ResolveFuncs();   // lazy-resolve (exe-ctor-built action)
    uint8_t* s = (uint8_t*)self;
    fn_InternalBasePtr_CheckPtr(s + 0x20);
    return *(void**)param == *(void**)(s + 0x20);
}

// ============================================================
// Standalone factory + vtable — Action family (notes/FACTORY_CASCADE_145_BRIEF.md).
// Vtable @ 0x14015ffe0 (10 slots), size 0x28, parent "Action".
// Shared slot helpers live in action_base_standalone.{h,cpp}.
// ============================================================
#include "action_base_standalone.h"
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t RemoveAnimal_GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("RemoveAnimal");
    return c;
}

static void* g_RemoveAnimal_Vtable[10] = {
    (void*)&Action_ScalarDtor,        // +0x00 ScalarDtor
    (void*)&Action_Slot1,             // +0x08 slot1 (returns null)
    (void*)&RemoveAnimal_GetTypeId,           // +0x10 GetTypeId
    (void*)&RemoveAnimal_Serialize_Impl,      // +0x18 Serialize
    (void*)&RemoveAnimal_AbstractImpl_Impl,   // +0x20 AbstractImpl
    (void*)&Action_BaseExecute,   // +0x28 Execute
    (void*)&Action_Enter,  // +0x30 Enter
    (void*)&Action_Exit,   // +0x38 Exit
    (void*)&RemoveAnimal_CheckEntity_Impl,  // +0x40 CheckEntity
    (void*)&RemoveAnimal_GetTarget_Impl, // +0x48 GetTarget
};

extern "C" void RegisterRemoveAnimalVtable() {
    RegisterExeAddr(0x14015ffe0, (uintptr_t)g_RemoveAnimal_Vtable);
}
extern "C" void* RemoveAnimal_CreateInPlace(void* mem) {
    RemoveAnimal_ResolveFuncs();
    return Action_InitInPlace(mem, 0x28, g_RemoveAnimal_Vtable);
}
extern "C" void* RemoveAnimal_Factory(void) {
    RemoveAnimal_ResolveFuncs();
    return Action_Alloc(0x28, g_RemoveAnimal_Vtable);
}
REGISTER_TYPE(RemoveAnimal, "Action", &RemoveAnimal_Factory, &RemoveAnimal_CreateInPlace);
