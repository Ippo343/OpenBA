// code/Game/Component/charactercomponent.cpp
// Clean reimplementation of CharacterComponent::Update from Application-x64.exe.
//
// CharacterComponent (0x80 bytes) manages character animation state.
// Update checks if a character index (data60) is valid (>= 0),
// looks up a character object from an array at data48, and calls
// Character_UpdateAnimation to process the character's animation state.
//
// Vtable at 0x140162370. One non-trivial override:
//   Update (0x140109480) — conditional animation delegate
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h — portable for standalone exe.

#include "Component/charactercomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Resolved helpers
// ============================================================

// Character_UpdateAnimation(arrayBase*, InternalBasePtr*, Time)
// Located at 0x1400c4f80 in Application-x64.exe.
// Iterates a linked list of character objects from the InternalBasePtr,
// calling each object's vtable[6] (Draw/UpdateAnimation) method.
// RCX = arrayBase (this+0x48), RDX = &localPtr (InternalBasePtr*), R8 = Time (int64)
typedef void (*fn_Character_UpdateAnimation_t)(void* arrayBase, void* ptrRef, int64_t deltaTime);
static fn_Character_UpdateAnimation_t s_fn_Character_UpdateAnimation = nullptr;

static bool s_helpersResolved = false;

static void EnsureHelpers() {
    if (s_helpersResolved) return;

    s_fn_Character_UpdateAnimation = reinterpret_cast<fn_Character_UpdateAnimation_t>(
        ResolveAddr(Module::Exe, 0x1400c4f80));

    s_helpersResolved = true;
}

// ============================================================
// CharacterComponent::Update (vtable slot [9], 0x140109480)
// ============================================================
// If data60 >= 0, look up object from *(data48)[data60] and call
// Character_UpdateAnimation with (&this->data48, &obj, deltaTime).
//
// Original disassembly:
//   SUB RSP,0x28
//   MOVSXD RAX,[RCX+0x60]    ; sign-extend data60
//   ADD RCX,0x48              ; arrayBase = this+0x48
//   TEST EAX,EAX
//   JS skip                   ; skip if index < 0
//   MOV RDX,[RCX]             ; objArray = *(this+0x48)
//   LEA RDX,[RDX+RAX*8]      ; &objArray[index]
//   MOV R8,R9                 ; deltaTime (forwarded from caller)
//   CALL Character_UpdateAnimation
// skip:
//   ADD RSP,0x28
//   RET

void CharacterComponent_Update(CharacterComponent* self, int64_t deltaTime) {
    EnsureHelpers();

    if (self->data60 >= 0) {
        if (!s_fn_Character_UpdateAnimation) return;

        // data48 is a pointer to an array of object pointers
        // data60 is the index into that array
        void** objArray = *reinterpret_cast<void***>(&self->data48);
        void* obj = objArray[self->data60];

        // Character_UpdateAnimation expects a pointer to the object pointer
        // (InternalBasePtr is just a pointer-sized wrapper)
        void* localPtr = obj;
        s_fn_Character_UpdateAnimation(
            reinterpret_cast<void*>(&self->data48),
            &localPtr,
            deltaTime
        );
    }
}

// ============================================================
// Reimpl of Character_UpdateAnimation (0x1400c4f80) — Component reg wave "people"
// Walks the linked list of character objects starting at *param_2; for each node
// whose enabled-flag (+0x18) is set, invokes vtable slot +0x30 (Draw/UpdateAnimation)
// with (node, arrayBase, deltaTime); advances via node+0x10. Non-static for registration.
// ============================================================
void Character_UpdateAnimation_Reimpl(void* arrayBase, void* ptrRef, int64_t deltaTime) {
    int64_t node = *reinterpret_cast<int64_t*>(ptrRef);
    while (node != 0) {
        fn_InternalBasePtr_CheckPtr(ptrRef);
        if (*reinterpret_cast<char*>(*reinterpret_cast<int64_t*>(ptrRef) + 0x18) != '\0') {
            fn_InternalBasePtr_CheckPtr(ptrRef);
            int64_t* obj = *reinterpret_cast<int64_t**>(ptrRef);
            int64_t vt = *obj;
            typedef void (*slot30_t)(int64_t*, void*, int64_t);
            (*reinterpret_cast<slot30_t*>(vt + 0x30))(obj, arrayBase, deltaTime);
        }
        fn_InternalBasePtr_CheckPtr(ptrRef);
        node = *reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(ptrRef) + 0x10);
        *reinterpret_cast<int64_t*>(ptrRef) = node;
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void CharacterComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[CharacterComponent] Funcs resolved: UpdateAnimation=%p\n",
           (void*)s_fn_Character_UpdateAnimation);
#endif
}

// ===== Standalone factory (Foundation D — component instantiation) =====
#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"

// ------------------------------------------------------------------
// Phase 3 / Keystone D — CharacterComponent Startup-only live-wiring (Brick 4a).
//
// The citizen position/geometry pipeline: CharacterComponent::Startup
// (0x140109a30, slot +0x20) finds the sibling ModelComponent (+0x68, whose
// transform at +0x158 IS the citizen world position) + PickingComponent (+0x70)
// + adopts the CharacterDescription (+0x78). The Brick-2 CanFindHousing guard
// bails while CharacterComponent+0x68 is null; this brick populates it so the
// CitizenAI loop can actually evaluate spatial decisions (housing).
//
// Startup-ONLY (the heavy move/anim tick + action executor at 0x1401094c0 stays
// inert — that is Brick 4b): we set +0x60 = -1 (no actions) so even a future
// move-tick would skip the executor on the empty stack. Default (env off) =>
// inert => byte-unchanged. Finds are self-contained via GetTypeIdByName (the
// .rdata type-id globals c7a50/c7c10 are 0xFFFFFFFF in standalone).
// ------------------------------------------------------------------
static void* CharComp_FindSiblingByName(int64_t ownerEntity, const char* typeName) {
    if (!ownerEntity) return nullptr;
    uint32_t want = ::Object::GetTypeIdByName(typeName);
    if (want == 0 || want == 0xFFFFFFFF) return nullptr;
    uint8_t* e = reinterpret_cast<uint8_t*>(ownerEntity);
    int count = *reinterpret_cast<int*>(e + 0x38);
    int64_t arr = *reinterpret_cast<int64_t*>(e + 0x30);
    for (int i = 0; i < count; i++) {
        void** ibp = reinterpret_cast<void**>(arr + (int64_t)i * 8);
        fn_InternalBasePtr_CheckPtr(ibp);
        void* comp = *ibp;
        if (!comp) continue;
        uintptr_t vt = *reinterpret_cast<uintptr_t*>(comp);
        typedef uint32_t (*getType_t)(void*);
        uint32_t tid = (*reinterpret_cast<getType_t*>(vt + 0x10))(comp);
        if (tid == want)
            return fn_IsDerivedFrom(comp, want) ? comp : nullptr;
    }
    return nullptr;
}

static void CharacterComponent_BodyInit(CharacterComponent* self) {
    if (!self) return;
    uint8_t* p = reinterpret_cast<uint8_t*>(self);

    // ctor (0x140109d20) sentinels: +0x40 grid-index = -1, +0x60 action-stack top
    // = -1 (EMPTY stack — a freshly spawned citizen has no queued actions). This is
    // faithful AND required for Brick 3: the CitizenAI Phase-8 check reads
    // CharacterComponent+0x60 + 1 > 0 as "has actions => busy this tick"; with the
    // empty -1 it is 0 (not >0), so Phase 8 runs the state-selection path
    // (CheckEducation/CheckAge/TransitionState) to assign a state. (Brick 3 ports
    // that path + guards it crash-safe; the move-tick/executor is still inert, so
    // -1 never drives the executor on an empty stack.)
    *reinterpret_cast<int32_t*>(p + 0x40) = -1;
    *reinterpret_cast<int32_t*>(p + 0x60) = -1;

    // Startup (0x140109a30): find sibling components + adopt the description.
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(p + 0x10);
    int64_t owner = *reinterpret_cast<int64_t*>(p + 0x10);
    if (!owner) return;

    // +0x68 = ModelComponent (geometry; +0x158 = world position).
    *reinterpret_cast<void**>(p + 0x68) = CharComp_FindSiblingByName(owner, "ModelComponent");
    // +0x70 = PickingComponent.
    *reinterpret_cast<void**>(p + 0x70) = CharComp_FindSiblingByName(owner, "PickingComponent");
    // +0x78 = CharacterDescription (trust comp+0x18).
    *reinterpret_cast<void**>(p + 0x78) = *reinterpret_cast<void**>(p + 0x18);
}

// ------------------------------------------------------------------
// Brick 4b — per-tick move/anim tick + action executor (0x1401094c0).
// The Update slot (+0x48 / vtable slot 9) routes to the move-tick, which runs the
// action executor over the stack at CharacterComponent+0x48 when it is non-empty
// (+0x60 >= 0), then early-returns when the char render-index (+0x40) is < 0 —
// which it always is in standalone (BodyInit sets -1), so the inert-ModelComponent
// transform tail is skipped (crash-safe). With no actions pushed yet the move-tick
// is a per-frame no-op; it begins executing actions once the CitizenAI state
// handlers push them.
//
// Gated on its OWN env var (separate from BANISHED_LIVE_CHARACTER) so the existing
// Startup-only behaviour is byte-identical by default: the Update vslot is installed
// whenever BANISHED_LIVE_CHARACTER is on, but the body no-ops unless
// BANISHED_LIVE_CHARACTER_MOVETICK is also set.
// ------------------------------------------------------------------
extern "C" void CharacterComponent_MoveTick_1094c0(uint8_t* self, void* time);

static void CharacterComponent_UpdateAdapter(CharacterComponent* self, int64_t* time) {
    static int en = -1;
    if (en < 0) {
        const char* e = getenv("BANISHED_LIVE_CHARACTER_MOVETICK");
        en = (e && e[0] && e[0] != '0') ? 1 : 0;
    }
    if (!en || !self) return;
    CharacterComponent_MoveTick_1094c0(reinterpret_cast<uint8_t*>(self), time);
}

COMPONENT_LIVE_FACTORY_S(CharacterComponent, 0x80, 0x8f,
                         "BANISHED_LIVE_CHARACTER",
                         CharacterComponent_UpdateAdapter, CharacterComponent_BodyInit)
REGISTER_TYPE(CharacterComponent, "Component",
              &CharacterComponent_Factory, &CharacterComponent_CreateInPlace);
#endif // STANDALONE_EXE
