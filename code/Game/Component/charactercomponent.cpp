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

#include "Component/charactercomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

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

static bool CharAnim_TraceOn();

void CharacterComponent_Update(CharacterComponent* self, int64_t deltaTime) {
    EnsureHelpers();

    if (CharAnim_TraceOn()) {
        static long long calls = 0, idle = 0;
        ++calls;
        if (self->data60 < 0) ++idle;
        if (calls <= 3 || (calls % 20000) == 0) {
            std::fprintf(stderr, "[ActionTick] CharacterComponent::Update calls=%lld "
                                 "idle(top<0)=%lld walker=%p\n",
                         calls, idle, (void*)s_fn_Character_UpdateAnimation);
            std::fflush(stderr);
        }
    }
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
// ACTION-TICK CENSUS (BANISHED_TRACKER_TRACE=1). Slot +0x30 (an action's Enter) is
// dispatched ONLY from here, so "Enter never ran" has exactly two shapes: this walk never
// visits the node, or it visits it with the +0x18 entered-flag still clear. Count both —
// a silent zero at the Enter site cannot tell them apart.
static bool CharAnim_TraceOn() {
    static int t = -1;
    if (t < 0) { const char* e = std::getenv("BANISHED_TRACKER_TRACE");
                 t = (e && e[0] && e[0] != '0') ? 1 : 0; }
    return t == 1;
}

void Character_UpdateAnimation_Reimpl(void* arrayBase, void* ptrRef, int64_t deltaTime) {
    int64_t node = *reinterpret_cast<int64_t*>(ptrRef);
    while (node != 0) {
        fn_InternalBasePtr_CheckPtr(ptrRef);
        if (CharAnim_TraceOn()) {
            static long long seen = 0, gated = 0, fired = 0;
            ++seen;
            if (*reinterpret_cast<char*>(*reinterpret_cast<int64_t*>(ptrRef) + 0x18) == '\0') ++gated;
            else ++fired;
            if (seen <= 3 || (seen % 20000) == 0) {
                std::fprintf(stderr, "[ActionTick] nodes seen=%lld fired(+0x18 set)=%lld "
                                     "gated(+0x18 clear)=%lld\n", seen, fired, gated);
                std::fflush(stderr);
            }
        }
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

// Char spatial-hash manager registrar (charmanager.cpp) — allocates the char render-index.
extern "C" int64_t CharMgr_sub_83f0(float* mgr, int64_t* ibp);

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

    // --- OnAdd (0x1401099c0): char render-index allocation — GATES the move-tick TAIL ---
    // The real engine runs OnAdd as a later lifecycle phase; we fold it into Startup-time
    // body-init because it only depends on +0x68 (the ModelComponent, just resolved) and the
    // char spatial-hash manager (GameWorld+0x88, seeded earlier in Load), and the effect — a
    // one-time slot allocation returning the render-index — is order-identical. Gated on
    // BANISHED_LIVE_CHARACTER_MOVETICK so default builds keep +0x40 = -1 and the transform
    // tail stays dormant (byte-unchanged). Without this, +0x40 stays -1 and the move-tick
    // early-returns before ever writing the ModelComponent transform.
    static int mtEn = -1;
    if (mtEn < 0) {
        mtEn = ::ComponentFactory::LiveEnabled("BANISHED_LIVE_CHARACTER_MOVETICK") ? 1 : 0;
    }
    if (mtEn) {
        // PickingComponent (+0x70) is COMPONENT_INERT_FACTORY in standalone: its memory is
        // zeroed, so its pick-id (+0x28) is 0, not the real ctor's -1 "unregistered" sentinel.
        // The citizen is NOT registered in the Picking::Scene here (that scene's object array
        // is never populated until the picking subsystem is brought live), so the move-tick's
        // `pickId >= 0` guard would WRONGLY pass and call Picking::Scene::SetTransform on an
        // empty scene -> null object-array deref. Force the honest "unregistered" -1 so the
        // tail skips the picking update (the render-transform write via Geometry::SetTransform
        // is unaffected). When the picking subsystem registers citizens via AddMesh, +0x28
        // will hold a real id and this becomes a no-op.
        void* pickComp = *reinterpret_cast<void**>(p + 0x70);
        if (pickComp) {
            *reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(pickComp) + 0x28) = -1;
            // PickingComponent::Startup (exe 0x140128c30) copies the component's entity
            // backref (+0x10) into +0x30 — this is the InternalBasePtr value handed to
            // Picking::Scene::AddMesh as the pick userData (Entity_impl_8f50 param_1[6]),
            // and returned verbatim by GetUserData when the citizen is clicked. The inert
            // standalone factory never runs slot +0x20, so do the copy here so the citizen
            // pick mesh carries a non-null owner (else the click resolves to a null entity).
            uint8_t* pc = reinterpret_cast<uint8_t*>(pickComp);
            *reinterpret_cast<void**>(pc + 0x30) = *reinterpret_cast<void**>(pc + 0x10);
        }

        int64_t model = *reinterpret_cast<int64_t*>(p + 0x68);
        void* gw = g_pGameWorldSingleton ? *g_pGameWorldSingleton : nullptr;
        void* charMgr = gw ? *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(gw) + 0x88)
                           : nullptr;
        if (model && charMgr && fn_InternalBasePtr_Ctor) {
            // facing init: CharacterComponent+0x3c = ModelComponent+0x168.
            *reinterpret_cast<int32_t*>(p + 0x3c) = *reinterpret_cast<int32_t*>(model + 0x168);
            // wrap self as an InternalBasePtr, register with the manager -> char-index.
            int64_t selfIbp = 0;
            fn_InternalBasePtr_Ctor(&selfIbp, self);
            int64_t ibpArg = selfIbp;
            *reinterpret_cast<int32_t*>(p + 0x40) =
                static_cast<int32_t>(CharMgr_sub_83f0(reinterpret_cast<float*>(charMgr), &ibpArg));
            // BANISHED_MOVETICK_TRACE: one line per char-index allocation. The owner
            // entity ptr correlates with [Herd]/[Pasture] SPAWN traces to prove whether
            // ANIMAL characters ever register (the [MoveTick] trace caps at 4 subjects
            // and cannot answer that).
            if (std::getenv("BANISHED_MOVETICK_TRACE")) {
                fprintf(stderr, "[CharIdx] idx=%d char=%p entity=%p model=%p\n",
                        *reinterpret_cast<int32_t*>(p + 0x40), (void*)self,
                        (void*)owner, (void*)model);
            }
        }
        else if (std::getenv("BANISHED_MOVETICK_TRACE")) {
            fprintf(stderr, "[CharIdx] SKIPPED char=%p entity=%p model=%p charMgr=%p\n",
                    (void*)self, (void*)owner, (void*)*reinterpret_cast<int64_t*>(p + 0x68),
                    (void*)charMgr);
        }
    }
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
        en = ::ComponentFactory::LiveEnabled("BANISHED_LIVE_CHARACTER_MOVETICK") ? 1 : 0;
    }
    if (!en || !self) return;
    CharacterComponent_MoveTick_1094c0(reinterpret_cast<uint8_t*>(self), time);
}

// Hand-rolled factory (macro variants only fill slots 4/6/9) — RETAIL SLOT WIRING.
// Retail vtable 0x140162370:
//   slot 8 (+0x40) = 0x1401094c0 move-tick — dispatched EVERY FRAME by the
//                    EntityList update-TREE walk (component flag bit 2);
//   slot 9 (+0x48) = 0x140109480 Update — dispatched at the THROTTLED 5-bucket
//                    round-robin rate (component flag bit 3, processCount quotas).
// The original port installed the move-tick at slot 9, so every character
// integrated movement at the BUCKET rate: citizens at roughly half speed, and
// Fifth-priority animals (quota 2/frame, ~120 animals) at ~1/60 speed —
// visually frozen. Slot 8 full-rate dispatch is the fix (2026-07-30).
// Slot 9's faithful body (CharacterComponent_Update -> action +0x30 tick) is
// GRADUATED (2026-08-18): BANISHED_LIVE_CHARACTER_UPDATE9 is in kDefaultOnFlags and
// the env var is now a kill-switch (=0). It is the ONLY dispatcher of an action's
// +0x30 slot, so while it was opt-in every action's Enter was dead code — most
// visibly HuntAnimal::Enter, which is the kill step: hunters chased deer forever and
// the hunting cabin's production panel read 0. A/B evidence is in kDefaultOnFlags.
static uint32_t CharacterComponent_LiveGetTypeId(void*) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("CharacterComponent");
    return cached;
}
static void* CharacterComponent_LiveVtable[::ComponentFactory::kInertVtableSlots];
static void CharacterComponent_MoveTickVSlot(void* s, void* t) {
    CharacterComponent_UpdateAdapter(reinterpret_cast<CharacterComponent*>(s),
                                     reinterpret_cast<int64_t*>(t));
}
static void CharacterComponent_StartupVSlot(void* s) {
    CharacterComponent_BodyInit(reinterpret_cast<CharacterComponent*>(s));
}
static void CharacterComponent_Update9VSlot(void* s, void* t) {
    // Retail forwards the Time operand RAW: slot 9 (0x140109480) does MOV R8,RDX at
    // 0x1401094a8, and the slot-8 move-tick likewise leaves RDX untouched across its
    // CALL to the executor. Character_UpdateAnimation then hands that same value to the
    // action's +0x30 as param_3. Dereferencing it here put a Time VALUE where every
    // action's tick expects whatever retail's RDX holds.
    CharacterComponent_Update(reinterpret_cast<CharacterComponent*>(s),
                              reinterpret_cast<int64_t>(t));
}
// ------------------------------------------------------------------
// Slot 7 (+0x38) — CharacterComponent::OnRemove (retail 0x140109980), the RELEASE half
// of the char-index allocation BodyInit folds in above. Retail body is exactly:
//     FUN_1400c5400(self + 0x48);                       // tear down the action stack
//     CharMgr_impl_8560(*(gw + 0x88), *(int*)(self+0x40));  // unlink from its cell list
//     *(int*)(self + 0x40) = -1;
//
// Without it we allocate a char-index for every character and never give one back, so a
// dead character's slot keeps a dangling InternalBasePtr AND stays linked in its spatial
// cell. Nothing walked those lists until the action +0x30 tick went live (both callers of
// the proximity query 0x140092260 are +0x30 slots), which is why this was invisible: the
// query then reads the stale slot's entity +0x68 -> +0x160 and faults on freed memory.
// Gated with the same flag as the allocation so the two halves can never be on separately.
// ------------------------------------------------------------------
typedef void (*fn_ActStackClear_t)(void* stackAt48);
extern "C" void CharMgr_impl_8560(int64_t mgr, int idx);

static void CharacterComponent_OnRemoveVSlot(void* s) {
    if (!s) return;
    uint8_t* p = reinterpret_cast<uint8_t*>(s);

    static fn_ActStackClear_t s_clear = nullptr;
    static int s_resolved = 0;
    if (!s_resolved) {
        s_resolved = 1;
        s_clear = reinterpret_cast<fn_ActStackClear_t>(ResolveAddr(Module::Exe, 0x1400c5400));
    }
    if (s_clear) s_clear(p + 0x48);

    void* gw = g_pGameWorldSingleton ? *g_pGameWorldSingleton : nullptr;
    int64_t charMgr = gw ? *reinterpret_cast<int64_t*>(reinterpret_cast<uintptr_t>(gw) + 0x88)
                         : 0;
    int idx = *reinterpret_cast<int32_t*>(p + 0x40);
    // DEVIATION (ours, not retail's): retail passes +0x40 through unguarded, and
    // impl_8560 writes `slots + idx*0x18` and pushes idx onto the free list without
    // checking it -- so a -1 corrupts memory ahead of the slot array. Retail can do that
    // because its OnAdd always allocated an index. Ours cannot: BodyInit only allocates
    // when it found both a ModelComponent and a seeded gw+0x88, so a CharacterComponent
    // can legitimately still hold the -1 sentinel here. Guard both operands for that
    // reason alone; a character that never registered has nothing to release.
    if (charMgr && idx >= 0) CharMgr_impl_8560(charMgr, idx);

    *reinterpret_cast<int32_t*>(p + 0x40) = -1;
}

extern "C" void CharacterComponent_Serialize(void* self, void* stream);
static void CharacterComponent_SerializeVSlot(void* s, void* stream) {
    CharacterComponent_Serialize(s, stream);
}
static void CharacterComponent_FinishLiveVtable() {
    // Save/load wave (2026-08-27): slot 3 is the stream Serialize — wired
    // unconditionally (not behind a feature env) so save/load round-trips this
    // component's real state regardless of which other CHARACTER live flags are set.
    CharacterComponent_LiveVtable[3] =
        reinterpret_cast<void*>(&CharacterComponent_SerializeVSlot);
    if (::ComponentFactory::LiveEnabled("BANISHED_LIVE_CHARACTER")) {
        CharacterComponent_LiveVtable[4] =
            reinterpret_cast<void*>(&CharacterComponent_StartupVSlot);
        CharacterComponent_LiveVtable[8] =
            reinterpret_cast<void*>(&CharacterComponent_MoveTickVSlot);
    }
    if (::ComponentFactory::LiveEnabled("BANISHED_LIVE_CHARACTER_MOVETICK"))
        CharacterComponent_LiveVtable[7] =
            reinterpret_cast<void*>(&CharacterComponent_OnRemoveVSlot);
    if (::ComponentFactory::LiveEnabled("BANISHED_LIVE_CHARACTER_UPDATE9"))
        CharacterComponent_LiveVtable[9] =
            reinterpret_cast<void*>(&CharacterComponent_Update9VSlot);
}
extern "C" void* CharacterComponent_Factory(void) {
    void* p = ::ComponentFactory::MakeInertTyped(0x80, 0x8f,
        CharacterComponent_LiveVtable,
        reinterpret_cast<void*>(&CharacterComponent_LiveGetTypeId));
    CharacterComponent_FinishLiveVtable();
    return p;
}
static void* CharacterComponent_CreateInPlace(void* mem) {
    if (mem) ::ComponentFactory::InitInertTyped(mem, 0x80, 0x8f,
        CharacterComponent_LiveVtable,
        reinterpret_cast<void*>(&CharacterComponent_LiveGetTypeId));
    CharacterComponent_FinishLiveVtable();
    return mem;
}
REGISTER_TYPE(CharacterComponent, "Component",
              &CharacterComponent_Factory, &CharacterComponent_CreateInPlace);
