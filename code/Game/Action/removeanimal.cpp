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
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch C
#include "Component/coll_bridge.h"
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B

// ============================================================
// Local function pointers resolved at init time
// (NOT in engine_funcs.h -- RemoveAnimal-specific helpers)
// ============================================================

// Object::Internal::~Internal(this) -- via IAT at 0x140152770
static void (*s_fn_ObjectInternal_Dtor)(void* self) = nullptr;

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

static bool s_funcs_resolved = false;

// ============================================================
// Resolution -- called once from Install
// ============================================================
bool RemoveAnimal_ResolveFuncs() {
    if (s_funcs_resolved) return true;

    // Object::Internal dtor via IAT
    uintptr_t iatDtor = ResolveAddr(Module::Exe, 0x140152770);
    if (iatDtor) s_fn_ObjectInternal_Dtor = *reinterpret_cast<void(**)(void*)>(iatDtor);

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

    bool ok = s_fn_ObjectInternal_Dtor &&
              s_fn_MarkEntityRemoval &&
              s_fn_GetModelComp && s_fn_NotifyEntityChanged &&
              s_typeId_ComponentEntity && s_typeInfo_RemoveAnimal &&
              s_dat_VectorDtorReturn &&
              fn_InternalBasePtr_DeletePtr &&
              fn_IsDerivedFrom && fn_MemFreeStatic &&
              g_pGameWorldSingleton;

#ifdef _DEBUG
    printf("[RemoveAnimal] ResolveFuncs: ObjDtor=%p MarkRemove=%p\n",
           (void*)s_fn_ObjectInternal_Dtor,
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
// Slot 3: Serialize (0x1400ca870)
// Serializes status byte (+0x18) and entity ref at +0x20 with ComponentEntity
// type validation on deserialization.
// ============================================================
void RemoveAnimal::Persist(void* stream) {
    // Reached through the registered vtable, which the factory path never
    // has to touch — an exe-ctor-built action arrives here with the file
    // statics still null. Lazy-resolve like the other entry points do.
    RemoveAnimal_ResolveFuncs();
    uint8_t* s = reinterpret_cast<uint8_t*>(this);
    alignas(8) uint8_t localSerializer1[16];
    alignas(8) uint8_t localSerializer2[24];

    // Serialize status byte (+0x18)
    ::new (static_cast<void*>(localSerializer1)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(localSerializer1)->Serialize(_status, nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(localSerializer1)->~Serializer();   // B.3.4 bridge

    // Serialize entity ref at +0x20
    ::new (static_cast<void*>(localSerializer2)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<Object::InternalBasePtr*>(s + 0x20)->Serialize(*reinterpret_cast<IO::Serializer*>(localSerializer2));   // B.3.4 bridge

    // Type-check: if the deserialized ptr is not a ComponentEntity, null it
    void* entityRef = _field20;
    if (entityRef != nullptr) {
        if (!fn_IsDerivedFrom(entityRef, *s_typeId_ComponentEntity)) {
            _field20 = nullptr;
        }
    }

    reinterpret_cast<IO::Serializer*>(localSerializer2)->~Serializer();   // B.3.4 bridge
}

// ============================================================
// Slot 4: AbstractImpl (0x1400ca7f0) -- DISABLED hook (RIP-relative MOV in prologue)
// Marks the animal entity at +0x20 for removal, queues to cleanup list.
// Returns 0 (action complete).
// ============================================================
int  RemoveAnimal::AbstractImpl(void* actionStack) {
    RemoveAnimal_ResolveFuncs();   // lazy-resolve (exe-ctor-built action)
    (void)actionStack;
    void* entityRef = _field20;

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

        // Check bit 10 of flags at entity+0x40 (already marked for removal?)
        uint32_t flags = *(uint32_t*)((uint8_t*)localRef + 0x40);
        if (((flags >> 10) & 1) == 0) {

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
void RemoveAnimal::CheckEntity() {
    RemoveAnimal_ResolveFuncs();   // lazy-resolve (exe-ctor-built action)
    void* localIBP[4];

    void* entity20 = _field20;
    s_fn_GetModelComp(entity20, localIBP);

    uint8_t* modelComp = (uint8_t*)(localIBP[0]);
    uint32_t flagsOld = *(uint32_t*)(modelComp + 0x20);

    if ((flagsOld >> 20) & 1) {
        // Re-read (CheckPtr may have updated)
        modelComp = (uint8_t*)(localIBP[0]);
        flagsOld = *(uint32_t*)(modelComp + 0x20);
        uint32_t flagsNew = flagsOld & 0xFFEFFFFF;  // clear bit 20
        *(uint32_t*)(modelComp + 0x20) = flagsNew;

        if (((flagsNew ^ flagsOld) & 0xFFFF) != 0) {
            if (*(void**)(modelComp + 0x10) != nullptr) {
                s_fn_NotifyEntityChanged(*(void**)(modelComp + 0x10));
            }
        }
    }
}

// ============================================================
// Slot 9: GetTarget (0x1400ca720)
// Returns true if *param matches the entity at +0x20.
// ============================================================
bool RemoveAnimal::GetTarget(void* param) {
    RemoveAnimal_ResolveFuncs();   // lazy-resolve (exe-ctor-built action)
    return *(void**)param == _field20;
}

// ============================================================
// class RemoveAnimal (RemoveAnimal.h) — ENDGAME B.3.8 ACT.
// The hand-built g_RemoveAnimal_Vtable[10], its per-class type-id cache and the
// Action_Alloc / Action_InitInPlace factories are gone: the compiler emits
// the table, DECLARE_EXE_TYPE answers slots 1/2.
// ============================================================
#include "Object/factory_registration.h"
#include "address_registry.h"

// create 0x140022e70 (inlined ctor). The tree's factory zeroed every field above
// +0x10; kept (not a behaviour change of this step).
RemoveAnimal::RemoveAnimal()
    : _field20(nullptr) {}

// Retail has no out-of-line RemoveAnimal ctor: 0x1400bbea0 inlines it
// (Internal(), Action stamp, +0x10/+0x18 = 0, RemoveAnimal stamp, +0x20 = animal).
RemoveAnimal::RemoveAnimal(uint64_t animal)
    : _field20(reinterpret_cast<void*>(animal)) {}

// [0] 0x140022ee0
Object::Base* RemoveAnimal::DeletingDtor(unsigned flags) {
    this->~RemoveAnimal();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// [3] 0x1400ca870
// [4] 0x1400ca7f0
// [8] 0x1400ca760
// [9] 0x1400ca720

void* RemoveAnimal::CreateNewObject() {
    RemoveAnimal_ResolveFuncs();
    void* p = Object::detail::InternalMemAlloc(sizeof(RemoveAnimal), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) RemoveAnimal());
}
void* RemoveAnimal::CreateInPlaceObject(void* storage) {
    RemoveAnimal_ResolveFuncs();
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) RemoveAnimal());
}

DEFINE_EXE_VTABLE(RemoveAnimal, "_ZTV12RemoveAnimal")

DEFINE_EXE_TYPE(RemoveAnimal, "Action")
