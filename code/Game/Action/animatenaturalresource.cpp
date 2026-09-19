// code/Game/Action/animatenaturalresource.cpp
// Clean C++ reimplementation of AnimateNaturalResource action methods.
// Source: Game/Action/AnimateNaturalResource.cpp (from profile build debug strings)
//
// AnimateNaturalResource — 0x30 bytes. Triggers and waits for animation on a natural resource.
// Inherits: Object::Internal -> Action (0x20) -> AnimateNaturalResource
// Ctor: 0x140021dc0 (factory), Vtable: 0x14015fb10
//
// Decompiled from Application-x64.exe using Ghidra.

#include "AnimateNaturalResource.h"
// The runtime type header comes FIRST: graphics/Mesh.h reaches
// runtime/types/volume/Volume.h, whose global-scope `struct Rectangle` is hidden
// by <wingdi.h>'s `Rectangle()` once <windows.h> has been seen through resolve.h.
#include "graphics/Mesh.h"   // ENDGAME B.3.5 batch K1
#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch C
#include <cstdio>
#include <cstdint>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B

// ============================================================
// Local function pointers resolved at init time
// (NOT in engine_funcs.h — AnimateNaturalResource-specific helpers)
// ============================================================

// FUN_1400052c0 — get NaturalResourceComponent from entity
static void* (*s_fn_GetNRComp)(void* entity, void* outIBP) = nullptr;

// FUN_140126440 — set animation index on NaturalResourceComponent
static void (*s_fn_SetAnimIndex)(void* nrComp, uint32_t animIdx) = nullptr;

// FUN_140003f60 — get ModelComponent from entity
static void* (*s_fn_GetModelComp)(void* entity, void* outIBP) = nullptr;

// TypeInfo address
static uint32_t* s_TypeId_ComponentEntity = nullptr;  // DAT_1401c5d70 — ComponentEntity
    // (was mistranscribed 0x1401c4218 = FireDescription; retail AnimateNaturalResource reads c5d70)

static bool s_funcs_resolved = false;

// ============================================================
// Resolution — called once from Install
// ============================================================
bool AnimateNaturalResource_ResolveFuncs() {
    if (s_funcs_resolved) return true;

    // Component lookup functions
    s_fn_GetNRComp = reinterpret_cast<void*(*)(void*, void*)>(
        ResolveAddr(Module::Exe, 0x1400052c0));
    s_fn_SetAnimIndex = reinterpret_cast<void(*)(void*, uint32_t)>(
        ResolveAddr(Module::Exe, 0x140126440));
    s_fn_GetModelComp = reinterpret_cast<void*(*)(void*, void*)>(
        ResolveAddr(Module::Exe, 0x140003f60));

    // ENDGAME B.3.5 batch K1: the `Graphics::MeshInstance::IsAnimating` cache that
    // used to be read out of exe dispatch slot 0x1401514b0 went with its one caller
    // in Execute, which now calls the member.

    // TypeInfo
    s_TypeId_ComponentEntity = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c5d70));

    bool ok = s_fn_GetNRComp && s_fn_SetAnimIndex &&
              s_fn_GetModelComp && s_TypeId_ComponentEntity &&
              fn_IsDerivedFrom &&
              fn_InternalBasePtr_DeletePtr && fn_ObjectInternal_Dtor && fn_MemFreeStatic;

#ifdef _DEBUG
    printf("[AnimateNaturalResource] ResolveFuncs: GetNR=%p SetAnim=%p GetModel=%p\n",
           (void*)s_fn_GetNRComp,
           (void*)s_fn_SetAnimIndex, (void*)s_fn_GetModelComp);
    if (!ok) printf("[AnimateNaturalResource] ERROR: some function pointers not resolved!\n");
#endif

    s_funcs_resolved = ok;
    return ok;
}

// ============================================================
// ScalarDtor (vtable slot 0)
// Sets vtables, calls DeletePtr on entity ref, ~Internal, optionally free.
// ============================================================
void* AnimateNaturalResource_ScalarDtor_Impl(AnimateNaturalResource* self, uint32_t flags) {
    // Set AnimateNaturalResource vtable
    self->vtable = (void*)ResolveAddr(Module::Exe, AnimateNaturalResourceAddr::Vtable);
    // Set Action base vtable (destructor chain)
    uintptr_t actionVtable = ResolveAddr(Module::Exe, 0x14015faa0);
    self->vtable = (void*)actionVtable;

    // DeletePtr on entity IBP at +0x10 if non-null
    if (self->entity != nullptr) {
        reinterpret_cast< ::Object::InternalBasePtr*>(&self->entity)->DeletePtr();   // B.3.3 bridge
    }

    fn_ObjectInternal_Dtor(self);

    if (flags & 1) {
        fn_MemFreeStatic(self);
    }
    return self;
}

// ============================================================
// Serialize (0x1400c5660, vtable slot 3)
// Serializes status, entity ref with type check, animIndex, waitForAnim.
// ============================================================
void AnimateNaturalResource_Serialize_Impl(AnimateNaturalResource* self, void* stream) {
    // Reached through the registered vtable, which the factory path never
    // has to touch — an exe-ctor-built action arrives here with the file
    // statics still null. Lazy-resolve like the other entry points do.
    AnimateNaturalResource_ResolveFuncs();
    alignas(8) uint8_t ser1[24];
    alignas(8) uint8_t ser2[24];

    // Serialize status (version 0)
    ::new (static_cast<void*>(ser1)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser1)->Serialize(*reinterpret_cast<bool*>(&self->status), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser1)->~Serializer();   // B.3.4 bridge

    // Serialize entity ref + extra fields (version 0)
    ::new (static_cast<void*>(ser2)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // Entity ref (+0x20) with type validation
    reinterpret_cast<Object::InternalBasePtr*>(&self->_field20)->Serialize(*reinterpret_cast<IO::Serializer*>(ser2));   // B.3.4 bridge
    if (self->_field20 != nullptr) {
        if (!fn_IsDerivedFrom(self->_field20, *s_TypeId_ComponentEntity)) {
            self->_field20 = nullptr;
        }
    }

    // Animation index (+0x28)
    reinterpret_cast<IO::Serializer*>(ser2)->Serialize(*reinterpret_cast<int*>(&self->animIndex), nullptr);   // B.3.4 bridge

    // Wait for anim flag (+0x2C)
    reinterpret_cast<IO::Serializer*>(ser2)->Serialize(*reinterpret_cast<bool*>(&self->waitForAnim), nullptr);   // B.3.4 bridge

    reinterpret_cast<IO::Serializer*>(ser2)->~Serializer();   // B.3.4 bridge
}

// ============================================================
// AbstractImpl (0x1400c5600, vtable slot 4)
// Gets NR component from entity at +0x20, calls SetAnimIndex with +0x28.
// Returns waitForAnim flag.
// ============================================================
int AnimateNaturalResource_AbstractImpl_Impl(AnimateNaturalResource* self, void* actionStack) {
    AnimateNaturalResource_ResolveFuncs();   // lazy-resolve (exe-ctor-built action)
    (void)actionStack;

    void* localIBP;
    s_fn_GetNRComp(self->_field20, &localIBP);
    s_fn_SetAnimIndex(localIBP, (uint32_t)self->animIndex);

    return (self->waitForAnim != 0) ? 1 : 0;
}

// ============================================================
// Execute (0x1400c55b0, vtable slot 5)
// Checks if entity's model is still animating. Returns true while animating.
// ============================================================
int AnimateNaturalResource_Execute_Impl(AnimateNaturalResource* self, void* actionStack, void* time) {
    (void)actionStack;
    (void)time;

    void* localIBP[4] = {nullptr, nullptr, nullptr, nullptr};
    s_fn_GetModelComp(self->_field20, localIBP);

    // Standalone-inert guard: on a tree whose ModelComponent find resolves null (inert
    // resource-entity render layer, unfilled type-id global), report "not animating" so the
    // action completes rather than dereferencing null+0x28 inside MeshInstance::IsAnimating.
    // Retail (live model) is unchanged.
    if (localIBP[0] == nullptr) return 0;

    // MeshInstance is at modelComp + 0x28, check if still animating
    return reinterpret_cast<const Graphics::MeshInstance*>(
               (uint8_t*)(localIBP[0]) + 0x28)->IsAnimating() ? 1 : 0;   // B.3.5 bridge
}

// ============================================================
// GetTarget (0x1400c5560, vtable slot 9)
// Returns true if param matches entity at +0x20.
// ============================================================
bool AnimateNaturalResource_GetTarget_Impl(AnimateNaturalResource* self, void* param) {
    AnimateNaturalResource_ResolveFuncs();   // lazy-resolve (exe-ctor-built action)
    return self->_field20 == *(void**)param;
}

// ============================================================
// CheckEntity (0x1400c55a0, vtable slot 8)
// 3 bytes total (XOR EAX,EAX; RET) — returns 0 (no-op).
// NOT HOOKED — too small. Standalone exe only.
// ============================================================
// int AnimateNaturalResource_CheckEntity_Impl(AnimateNaturalResource* self) {
//     return 0;
// }

// ============================================================
// Standalone factory + vtable — Action family (notes/FACTORY_CASCADE_145_BRIEF.md).
// Vtable @ 0x14015fb10 (10 slots), size 0x30, parent "Action".
// Shared slot helpers live in action_base_standalone.{h,cpp}.
// ============================================================
#include "action_base_standalone.h"
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t AnimateNaturalResource_GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("AnimateNaturalResource");
    return c;
}

static void* g_AnimateNaturalResource_Vtable[10] = {
    (void*)&Action_ScalarDtor,        // +0x00 ScalarDtor
    (void*)&Action_Slot1,             // +0x08 slot1 (returns null)
    (void*)&AnimateNaturalResource_GetTypeId,           // +0x10 GetTypeId
    (void*)&AnimateNaturalResource_Serialize_Impl,      // +0x18 Serialize
    (void*)&AnimateNaturalResource_AbstractImpl_Impl,   // +0x20 AbstractImpl
    (void*)&AnimateNaturalResource_Execute_Impl,   // +0x28 Execute
    (void*)&Action_Enter,  // +0x30 Enter
    (void*)&Action_Exit,   // +0x38 Exit
    (void*)&Action_BaseCheckEntity,  // +0x40 CheckEntity
    (void*)&AnimateNaturalResource_GetTarget_Impl, // +0x48 GetTarget
};

extern "C" void RegisterAnimateNaturalResourceVtable() {
    RegisterExeAddr(0x14015fb10, (uintptr_t)g_AnimateNaturalResource_Vtable);
}
extern "C" void* AnimateNaturalResource_CreateInPlace(void* mem) {
    AnimateNaturalResource_ResolveFuncs();
    return Action_InitInPlace(mem, 0x30, g_AnimateNaturalResource_Vtable);
}
extern "C" void* AnimateNaturalResource_Factory(void) {
    AnimateNaturalResource_ResolveFuncs();
    return Action_Alloc(0x30, g_AnimateNaturalResource_Vtable);
}
REGISTER_TYPE(AnimateNaturalResource, "Action", &AnimateNaturalResource_Factory, &AnimateNaturalResource_CreateInPlace);
