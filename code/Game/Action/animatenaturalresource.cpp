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
// Serialize (0x1400c5660, vtable slot 3)
// Serializes status, entity ref with type check, animIndex, waitForAnim.
// ============================================================
void AnimateNaturalResource::Persist(void* stream) {
    // Reached through the registered vtable, which the factory path never
    // has to touch — an exe-ctor-built action arrives here with the file
    // statics still null. Lazy-resolve like the other entry points do.
    AnimateNaturalResource_ResolveFuncs();
    alignas(8) uint8_t ser1[24];
    alignas(8) uint8_t ser2[24];

    // Serialize status (version 0)
    ::new (static_cast<void*>(ser1)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser1)->Serialize(*reinterpret_cast<bool*>(&this->_status), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser1)->~Serializer();   // B.3.4 bridge

    // Serialize entity ref + extra fields (version 0)
    ::new (static_cast<void*>(ser2)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // Entity ref (+0x20) with type validation
    reinterpret_cast<Object::InternalBasePtr*>(&this->_field20)->Serialize(*reinterpret_cast<IO::Serializer*>(ser2));   // B.3.4 bridge
    if (this->_field20 != nullptr) {
        if (!fn_IsDerivedFrom(this->_field20, *s_TypeId_ComponentEntity)) {
            this->_field20 = nullptr;
        }
    }

    // Animation index (+0x28)
    reinterpret_cast<IO::Serializer*>(ser2)->Serialize(*reinterpret_cast<int*>(&this->animIndex), nullptr);   // B.3.4 bridge

    // Wait for anim flag (+0x2C)
    reinterpret_cast<IO::Serializer*>(ser2)->Serialize(*reinterpret_cast<bool*>(&this->waitForAnim), nullptr);   // B.3.4 bridge

    reinterpret_cast<IO::Serializer*>(ser2)->~Serializer();   // B.3.4 bridge
}

// ============================================================
// AbstractImpl (0x1400c5600, vtable slot 4)
// Gets NR component from entity at +0x20, calls SetAnimIndex with +0x28.
// Returns waitForAnim flag.
// ============================================================
int  AnimateNaturalResource::AbstractImpl(void* actionStack) {
    AnimateNaturalResource_ResolveFuncs();   // lazy-resolve (exe-ctor-built action)
    (void)actionStack;

    void* localIBP;
    s_fn_GetNRComp(this->_field20, &localIBP);
    s_fn_SetAnimIndex(localIBP, (uint32_t)this->animIndex);

    return (this->waitForAnim != 0) ? 1 : 0;
}

// ============================================================
// Execute (0x1400c55b0, vtable slot 5)
// Checks if entity's model is still animating. Returns true while animating.
// ============================================================
int  AnimateNaturalResource::Execute(void* actionStack, void* time) {
    (void)actionStack;
    (void)time;

    void* localIBP[4] = {nullptr, nullptr, nullptr, nullptr};
    s_fn_GetModelComp(this->_field20, localIBP);

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
bool AnimateNaturalResource::GetTarget(void* param) {
    AnimateNaturalResource_ResolveFuncs();   // lazy-resolve (exe-ctor-built action)
    return this->_field20 == *(void**)param;
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
// class AnimateNaturalResource (AnimateNaturalResource.h) — ENDGAME B.3.8 ACT.
// The hand-built g_AnimateNaturalResource_Vtable[10], its per-class type-id cache and the
// Action_Alloc / Action_InitInPlace factories are gone: the compiler emits
// the table, DECLARE_EXE_TYPE answers slots 1/2.
// ============================================================
#include "Object/factory_registration.h"
#include "address_registry.h"

// create 0x140021dc0 (inlined ctor). The tree's factory zeroed every field above
// +0x10; kept (not a behaviour change of this step).
AnimateNaturalResource::AnimateNaturalResource()
    : _field20(nullptr),
      animIndex(0),
      waitForAnim(0),
      _pad2D{} {}

// [0] 0x140021e30
Object::Base* AnimateNaturalResource::DeletingDtor(unsigned flags) {
    this->~AnimateNaturalResource();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// [3] 0x1400c5660
// [4] 0x1400c5600
// [5] 0x1400c55b0
// [8] 0x1400c55a0 — the leaf's own `RET 0` (same bytes as the base body)
void AnimateNaturalResource::CheckEntity() {}
// [9] 0x1400c5560

void* AnimateNaturalResource::CreateNewObject() {
    AnimateNaturalResource_ResolveFuncs();
    void* p = Object::detail::InternalMemAlloc(sizeof(AnimateNaturalResource), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) AnimateNaturalResource());
}
void* AnimateNaturalResource::CreateInPlaceObject(void* storage) {
    AnimateNaturalResource_ResolveFuncs();
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) AnimateNaturalResource());
}

DEFINE_EXE_VTABLE(AnimateNaturalResource, "_ZTV22AnimateNaturalResource")

// B38 D5: every by-address reader of 0x14015fb10 was inside this block and now
// builds the object with the real ctor, so the row is gone. The function stays
// until the coordinator's address_registry hunk drops its call.
extern "C" void RegisterAnimateNaturalResourceVtable() {}

DEFINE_EXE_TYPE(AnimateNaturalResource, "Action")
