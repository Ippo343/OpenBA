// code/Game/Component/orchardcomponent.cpp
// Clean reimplementation of OrchardComponent lifecycle methods from Application-x64.exe.
//
// OrchardComponent (0x68 bytes) extends ControllerComponent with:
//   +0x48: uint8  orchardFlag, +0x4C: int32 orchardId, +0x50-0x60: arrays
//
// Vtable at 0x14015a180. One hooked override:
//   Shutdown (0x140051de0) — Complex entity ref serialization
//

#include "Component/orchardcomponent_funcs.h"
#include "Entity/ComponentEntity.h"
#include "Component/NaturalResourceComponent.h"
#include "Component/FruitComponent.h"
#include "Component/FruitDescription.h"
#include "Component/OrchardDescription.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include "io/Stream.h"      // ENDGAME B.3.4 callers batch H: IO::Stream::GetDirection
#include <cstdint>
#include <cstdio>
#include <new>
#include <atomic>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "Component/exe_bodies.h"

// ============================================================
// Resolved helpers
// ============================================================

// FUN_140051830 — validate entity references in orchard
typedef void (*fn_ValidateOrchardRefs_t)(void* self);
static fn_ValidateOrchardRefs_t s_fn_ValidateOrchardRefs = nullptr;

// FUN_140009f80 — resize entity ref array at +0x50
typedef void (*fn_ResizeRefArray_t)(void* arrayPtr, int newCount);
static fn_ResizeRefArray_t s_fn_ResizeRefArray = nullptr;

// FUN_14000a230 — serialize entity ref array entries
typedef void (*fn_SerializeRefEntries_t)(void* serializer, int count, void* arrayBase);
static fn_SerializeRefEntries_t s_fn_SerializeRefEntries = nullptr;

// FUN_140002340 — serialize IBP array (for +0x60)
typedef void (*fn_SerializeIBPArray_t)(void* arrayPtr, void* serializer);
static fn_SerializeIBPArray_t s_fn_SerializeIBPArray = nullptr;

static bool s_initialized = false;


// ============================================================
// Reimplemented exe helpers (callee bodies for the guarded call sites above)
// ============================================================

// IO::Serializer::Serialize(int*) dispatch (reuse s_fn_SerializeInt) and InternalBasePtr::Serialize.
// Object::Base::IsA(self, typeId) -> bool.
typedef bool (*fn_BaseIsA_t)(void* self, uint32_t typeId);
static fn_BaseIsA_t s_fn_BaseIsA = nullptr;
// FUN_1400052c0 -- IBP component accessor used by ref validation.
typedef void* (*fn_IBPAccessor_t)(int64_t entity, void* outIBP);
static fn_IBPAccessor_t s_fn_IBPAccessor52c0 = nullptr;
// Type-id globals referenced by the serializer (entity-ref kind filters).
// ⚠ This was bound to 0x1401c79e0 (MapComponent) and named accordingly. Both retail sites
// that use it — the ref serializer (0x14000a230) and ValidateRefs (0x140051830) — read
// `dword [0x1401c5d70]` = ComponentEntity, verified from the disassembly. The old global is
// a real, registered, NON-ZERO id of the wrong class, so every null check passed and the
// filter simply rejected everything: reference_wrong_but_nonzero_typeid. The slot the two
// sites test is a tile record's +0x08, which holds the ENTITY standing on the tile (dc20
// stores the owner IBP from MapComponent+0x10) — an entity is never a MapComponent.
static void**    s_pWorldSingleton2 = nullptr;      // g_pGameWorldSingleton (0x1401c3dd8)

static bool s_subResolved = false;
static void EnsureSubHelpers() {
    if (s_subResolved) return;
    // Object::Base::IsA — retail calls it through the IAT dispatch slot at 0x140151410
    // (`CALL qword ptr [0x140151410]` at 0x1400518bd). It was bound to exe 0x140002d40,
    // which is not IsA at all but DynArray8_EraseByValue, and has no reimplementation, so
    // ResolveAddr returned 0 and ValidateRefs' `s_fn_BaseIsA &&` guard failed every time —
    // silently demoting every correctly-bound slot to "unusable" (measured: msg 0 bound
    // entity+natRes+fruit, the very next validate pass tore it back down, and the orchardist
    // replanted the same cell forever).
    {
        uintptr_t slot = ResolveAddr(Module::Exe, 0x140151410);
        if (slot) s_fn_BaseIsA = *reinterpret_cast<fn_BaseIsA_t*>(slot);
        if (!s_fn_BaseIsA) s_fn_BaseIsA = reinterpret_cast<fn_BaseIsA_t>(fn_IsA);
    }
    s_fn_IBPAccessor52c0  = reinterpret_cast<fn_IBPAccessor_t>(ResolveAddr(Module::Exe, 0x1400052c0));
    s_pWorldSingleton2    = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x1401c3dd8));
    s_subResolved = true;
}

// Reimpl of FUN_140009f80 (grow stride-0x28 entity-ref array at +0x50; copy old, zero new, free old)
void OrchardComponent_ResizeRefArray(void* arrayPtr, int newCount) {
    int64_t* a = reinterpret_cast<int64_t*>(arrayPtr);
    int oldCount = (int)a[1];
    if (newCount == oldCount) return;
    void* oldBuf = reinterpret_cast<void*>(a[0]);
    *reinterpret_cast<int*>(&a[1]) = newCount;
    int* capPtr = reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(arrayPtr) + 0xc);
    if (*capPtr < newCount) {
        *capPtr = newCount;
        if (newCount == oldCount + 1 && oldCount != 0) *capPtr = newCount * 3;
        void* newBuf = (fn_MemAlloc && g_pMemAllocSingleton)
                           ? fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)(*capPtr) * 0x28, 8) : nullptr;
        a[0] = reinterpret_cast<int64_t>(newBuf);
    }
    int keep = (oldBuf == reinterpret_cast<void*>(a[0])) ? oldCount : 0;
    uintptr_t newBase = (uintptr_t)a[0];
    uintptr_t oldBase = (uintptr_t)oldBuf;
    for (int idx = keep; idx < newCount; ++idx) {
        uint8_t* dst = reinterpret_cast<uint8_t*>(newBase + (uintptr_t)idx * 0x28);
        if (idx < oldCount) {
            uint8_t* src = reinterpret_cast<uint8_t*>(oldBase + (uintptr_t)idx * 0x28);
            *reinterpret_cast<uint32_t*>(dst + 0)  = *reinterpret_cast<uint32_t*>(src + 0);
            *reinterpret_cast<uint32_t*>(dst + 4)  = *reinterpret_cast<uint32_t*>(src + 4);
            *reinterpret_cast<uint64_t*>(dst + 8)  = *reinterpret_cast<uint64_t*>(src + 8);
            *reinterpret_cast<uint64_t*>(dst + 16) = *reinterpret_cast<uint64_t*>(src + 16);
            *reinterpret_cast<uint64_t*>(dst + 24) = *reinterpret_cast<uint64_t*>(src + 24);
            *reinterpret_cast<uint8_t*>(dst + 32)  = *reinterpret_cast<uint8_t*>(src + 32);
        } else {
            *reinterpret_cast<uint64_t*>(dst + 8)  = 0;
            *reinterpret_cast<uint64_t*>(dst + 16) = 0;
            *reinterpret_cast<uint64_t*>(dst + 24) = 0;
        }
    }
    if (oldBuf != reinterpret_cast<void*>(a[0]) && fn_MemFreeStatic) fn_MemFreeStatic(oldBuf);
}

// Reimpl of FUN_14000a230 (serialize stride-0x28 entity-ref array entries: 2 ints, 3 IBPs, 1 bool)
void OrchardComponent_SerializeRefEntries(void* serializer, int count, void* arrayBase) {
    EnsureSubHelpers();
    if (count <= 0) return;
    uint8_t* e = reinterpret_cast<uint8_t*>(arrayBase);
    for (int i = 0; i < count; ++i) {
        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(e + 0), nullptr);   // B.3.4 bridge
        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(e + 4), nullptr);   // B.3.4 bridge

        reinterpret_cast<Object::InternalBasePtr*>(e + 8)->Serialize(*reinterpret_cast<IO::Serializer*>(serializer));   // B.3.4 bridge
        if (*reinterpret_cast<void**>(e + 8) != nullptr &&
            !(fn_IsDerivedFrom && fn_IsDerivedFrom(*reinterpret_cast<void**>(e + 8), ComponentEntity::TypeId())))
            *reinterpret_cast<void**>(e + 8) = nullptr;

        reinterpret_cast<Object::InternalBasePtr*>(e + 0x10)->Serialize(*reinterpret_cast<IO::Serializer*>(serializer));   // B.3.4 bridge
        if (*reinterpret_cast<void**>(e + 0x10) != nullptr &&
            !(fn_IsDerivedFrom && fn_IsDerivedFrom(*reinterpret_cast<void**>(e + 0x10), NaturalResourceComponent::TypeId())))
            *reinterpret_cast<void**>(e + 0x10) = nullptr;

        reinterpret_cast<Object::InternalBasePtr*>(e + 0x18)->Serialize(*reinterpret_cast<IO::Serializer*>(serializer));   // B.3.4 bridge
        if (*reinterpret_cast<void**>(e + 0x18) != nullptr &&
            !(fn_IsDerivedFrom && fn_IsDerivedFrom(*reinterpret_cast<void**>(e + 0x18), FruitComponent::TypeId())))
            *reinterpret_cast<void**>(e + 0x18) = nullptr;

        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<bool*>(e + 0x20), nullptr);   // B.3.4 bridge
        e += 0x28;
    }
}

// Reimpl of FUN_140051830 (validate orchard entity refs against live world entities; clear stale)
void OrchardComponent_ValidateRefs(void* selfV) {
    EnsureSubHelpers();
    uintptr_t self = reinterpret_cast<uintptr_t>(selfV);
    uintptr_t world = s_pWorldSingleton2 ? reinterpret_cast<uintptr_t>(*s_pWorldSingleton2) : 0;
    if (!world) return;
    uintptr_t resMgr = *reinterpret_cast<uintptr_t*>(world + 0xF8);
    int count = *reinterpret_cast<int*>(self + 0x58);
    for (int i = 0; i < count; ++i) {
        uintptr_t arr = *reinterpret_cast<uintptr_t*>(self + 0x50);
        uintptr_t entry = arr + (uintptr_t)i * 0x28;
        if (*reinterpret_cast<int*>(entry + 4) != 1) continue;

        int32_t tile = *reinterpret_cast<int32_t*>(entry + 0);
        void* live = *reinterpret_cast<void**>(
            *reinterpret_cast<uintptr_t*>(resMgr + 0x80) + 8 + (uintptr_t)tile * 0x30);
        bool ok = false;
        if (live) {
            if (s_fn_BaseIsA &&
                s_fn_BaseIsA(live, ComponentEntity::TypeId())) {
                int64_t accBuf = 0;
                void* acc = s_fn_IBPAccessor52c0 ? s_fn_IBPAccessor52c0((int64_t)live, &accBuf) : nullptr;
                if (acc && *reinterpret_cast<int64_t*>(acc) != 0) {
                    if (live == *reinterpret_cast<void**>(entry + 8)) ok = true;
                }
            }
        }
        if (ok) continue;
        {   // This is the ONLY demotion out of state 1, and it silently undoes a successful
            // msg-0 bind — the orchard planted a tree, then reported the slot as unusable and
            // replanted the same cell forever. Four independent conditions decide it; say which
            // one said no.
            static int s_n = 0;
            if (s_n++ < 8) {
                void* bound = *reinterpret_cast<void**>(entry + 8);
                std::fprintf(stderr, "[OrchardValidate] slot=%d tile=%d DEMOTING 1->2: live=%p "
                             "bound=%p typeIdCompEntity=%u isA=%d liveEqBound=%d\n",
                             i, tile, live, bound, ComponentEntity::TypeId(),
                             (live && s_fn_BaseIsA)
                                 ? (int)s_fn_BaseIsA(live, ComponentEntity::TypeId()) : -1,
                             (int)(live == bound));
                std::fflush(stderr);
            }
        }
        // Stale -> mark removed (kind=2) and null the 3 IBP slots.
        *reinterpret_cast<int32_t*>(entry + 4) = 2;
        int64_t z = 0;
        if (fn_InternalBasePtr_Ctor) { new (&z) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
            *reinterpret_cast<int64_t*>(entry + 8) = z; }
        z = 0;
        if (fn_InternalBasePtr_Ctor) { new (&z) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
            *reinterpret_cast<int64_t*>(entry + 0x10) = z; }
        z = 0;
        if (fn_InternalBasePtr_Ctor) { new (&z) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
            *reinterpret_cast<int64_t*>(entry + 0x18) = z; }
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void OrchardComponent_InitFuncs() {
    if (s_initialized) return;

    s_fn_ValidateOrchardRefs = reinterpret_cast<fn_ValidateOrchardRefs_t>(
        ResolveAddr(Module::Exe, 0x140051830));
    s_fn_ResizeRefArray = reinterpret_cast<fn_ResizeRefArray_t>(
        ResolveAddr(Module::Exe, 0x140009f80));
    s_fn_SerializeRefEntries = reinterpret_cast<fn_SerializeRefEntries_t>(
        ResolveAddr(Module::Exe, 0x14000a230));
    s_fn_SerializeIBPArray = reinterpret_cast<fn_SerializeIBPArray_t>(
        ResolveAddr(Module::Exe, 0x140002340));

    s_initialized = true;
}

// ============================================================
// OrchardComponent::GetGrowth (0x140051640) -> float
// ============================================================
// Average growth ratio (0.0-1.0) across the orchard's planted tree slots.
// This is what OrchardUI::Slot7 renders as the panel's yield bar + "NN%" text;
// with it unresolved the percentage rendered blank (and, once the component
// finder started working, calling the null pointer crashed the frame).
//
// Slot array: base = comp+0x50, count = comp+0x58, stride 0x28. Per slot:
//   +0x00 int32  tile index into the map tile array
//   +0x04 int32  slot state — only state 1 (a grown/planted tree) counts
//   +0x08 Base*  the entity this slot believes it owns
//   +0x18 IBP    the growth-carrying object; its +0x30 is the ratio
// The tile lookup is `*(map+0x80) + 8 + tileIdx*0x30`, i.e. the entity pointer
// inside a 0x30-byte tile record, with map = *(gameWorld+0xf8).
//
// The `entity == slot+0x08` re-check is retail's staleness test: a slot whose
// tile now holds a DIFFERENT entity (tree chopped, something else built) is
// skipped rather than counted. Note the divisor is the FULL slot count, not the
// number of slots that scored — an orchard of saplings correctly reads low.
//
// The per-slot clamp is against the 1.0f constant at 0x14017214c (Ghidra names
// that float g_fUVScale; it is a plain 1.0f used here as the growth ceiling).
float OrchardComponent_GetGrowth(void* selfV) {
    uintptr_t comp = reinterpret_cast<uintptr_t>(selfV);
    if (!comp) return 0.0f;

    const int count = *reinterpret_cast<int32_t*>(comp + 0x58);
    if (count <= 0) return 0.0f;

    void* world = (g_pGameWorldSingleton && *g_pGameWorldSingleton)
                      ? *g_pGameWorldSingleton : nullptr;
    if (!world) return 0.0f;
    uintptr_t map = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(world) + 0xf8);
    if (!map) return 0.0f;
    uintptr_t tiles = *reinterpret_cast<uintptr_t*>(map + 0x80);
    if (!tiles) return 0.0f;

    const uint32_t tidEntity = ComponentEntity::TypeId();   // cell 0x1401c5d70 (D5)
    const float maxGrowth = g_fUVScale ? *g_fUVScale : 1.0f;

    // FUN_1400052c0 — FindComponent(NaturalResourceComponent), member ABI
    // (RCX = entity, RDX = out InternalBasePtr). Registered by reg_work_base.cpp.
    typedef void* (*fn_FindNatRes_t)(void* entity, void* outIbp);
    static fn_FindNatRes_t s_findNatRes = nullptr;
    if (!s_findNatRes)
        s_findNatRes = reinterpret_cast<fn_FindNatRes_t>(ResolveAddr(Module::Exe, 0x1400052c0));
    if (!s_findNatRes) return 0.0f;

    const uintptr_t slots = *reinterpret_cast<uintptr_t*>(comp + 0x50);
    if (!slots) return 0.0f;

    float total = 0.0f;
    for (int i = 0; i < count; ++i) {
        const uintptr_t slot = slots + static_cast<uintptr_t>(i) * 0x28;

        const int tileIdx = *reinterpret_cast<int32_t*>(slot + 0x00);
        void* entity = *reinterpret_cast<void**>(
            tiles + 8 + static_cast<uintptr_t>(tileIdx) * 0x30);
        if (!entity) continue;
        if (!fn_IsA || !fn_IsA(entity, tidEntity)) continue;

        uint64_t natRes = 0;
        s_findNatRes(entity, &natRes);
        if (!natRes) continue;

        void** owned = reinterpret_cast<void**>(slot + 0x08);
        if (entity != *owned) continue;
        if (*reinterpret_cast<int32_t*>(slot + 0x04) != 1) continue;

        void** growthObj = reinterpret_cast<void**>(slot + 0x18);
        if (!*growthObj) continue;   // SA: retail derefs unguarded; state 1 implies it
        float g = *reinterpret_cast<float*>(
            reinterpret_cast<uintptr_t>(*growthObj) + 0x30);
        if (g > maxGrowth) g = maxGrowth;
        total += g;
    }

    return total / static_cast<float>(count);
}

// ============================================================
// OrchardComponent::OnMessage — primary vtable idx15 (+0x78), retail 0x140051990.
// ABI confirmed by code/Game/Action/sendcomponentmessage.cpp:231, which dispatches
// vtable+0x78 as (comp, msgId, srcEntityRef, p1, p2). p1 is the SLOT INDEX.
//
// This is the writer the whole planting loop was missing. PlantOrchard::Slot4 skips any
// slot whose +0x08 already holds an entity and marks its own validity byte, and
// PlantOrchard::OnComplete clears that byte again on completion — so with nothing ever
// writing slot+0x08 the orchardist replanted the same cell forever (measured: 29 executed
// plant actions, slot histogram still 12x state 0, nonNullEntity=0).
//
//   msg 0 "planted"  — read the tile the slot points at, take the entity standing on it,
//                      and bind {entity, NaturalResourceComponent, FruitComponent} into the
//                      slot with state 1. If ANY of the three is missing the slot goes to
//                      state 2 (removed/unusable) instead — retail's own failure branch.
//   msg 1 "harvested" — flag the slot (+0x20) and reset the fruit component's yield
//                      (+0x30 = 0, +0x34 = 1).
//   msg 2 "boost"    — advance the tree's fruit growth by the orchard description's rate.
//
// ⚠ Two faithful landmines kept verbatim: msg 2 reads desc+0x2c through a pointer that
// retail nulls when IsDerivedFrom fails, and helper_9fc0 does the same with desc+0x20. Both
// are load-bearing — they only fire if a type-id global is unfilled, which is a bug to fix at
// the registration, not to paper over here (see reg_orchard_typeid.cpp / reg_component_fruit.cpp).
// ============================================================

// FUN_14000a0b0 — sibling FruitComponent finder (type id DAT_1401c4250).
typedef void* (*fn_FindFruit_a0b0_t)(int64_t entity, void* outIBP);
static fn_FindFruit_a0b0_t s_fn_FindFruit_a0b0 = nullptr;
static float*    s_pf_172234 = nullptr;         // 300.0f
static bool s_msgResolved = false;
static void EnsureMsgHelpers() {
    if (s_msgResolved) return;
    s_msgResolved = true;
    s_fn_FindFruit_a0b0 = reinterpret_cast<fn_FindFruit_a0b0_t>(
        ResolveAddr(Module::Exe, 0x14000a0b0));
    s_pf_172234        = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x140172234));
}

// Reimpl of OrchardComponent_helper_9fc0 (0x140049fc0) — advance a FruitComponent's growth
// by `scale` worth of its description's rate, by synthesising a Time and calling the
// component's own Update (vtable +0x48, slot 9).
static void OrchardComponent_BoostFruit_49fc0(int64_t* fruitComp, float scale) {
    EnsureMsgHelpers();
    void* desc = reinterpret_cast<void*>(fruitComp[3]);          // +0x18
    void* validated = nullptr;
    if (fn_IsDerivedFrom && fn_IsDerivedFrom(desc, FruitDescription::TypeId()))
        validated = desc;
    const float rate  = *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(validated) + 0x20);
    const float secs  = rate * (s_pf_172234 ? *s_pf_172234 : 300.0f) * scale;
    // System::Time is 8 bytes — a 4-byte target here is the documented clobber trap.
    uint64_t timeVal = 0;
    if (fn_Time_Ctor_Float) fn_Time_Ctor_Float(&timeVal, secs);
    reinterpret_cast<Component*>(fruitComp)->Update(&timeVal);   // B.3.8 bridge (slot 9)
}


// ============================================================
// The PANEL COMMAND PATH — primary vtable idx13 (CanCommand) and idx14 (Command)
// ============================================================
// Slot indices verified from the vtable BYTES at 0x14015a180: +13*8 = 0x14015a1e8 reads
// {0x140051c90, 0x140051ba0, 0x140051990, 0x1400026c0} — and the last two are the
// addresses this file already cites for OnMessage (idx15) and GetDefaultState (idx16),
// which is what pins the indexing.
//
// WHY THEY WERE MISSING: neither slot was ever assigned, so both kept the shared inert
// stub MakeInertTyped installs — "returns 0 for every non-dtor slot". CanCommand
// returning 0 unconditionally is exactly the reported symptom: every orchard panel
// button renders disabled regardless of state, and a click dies at the first hop. This
// is the same trap the slot-15 comment below documents, so the wiring hangs off the
// same PopulateFn hook.
namespace {

typedef void* (*fn_FindSM_t)(uintptr_t entity, void* outIBP);      // 0x140003ea0
typedef void* (*fn_Find_t)(uintptr_t entity, void* outIBP);        // shared finder shape
typedef char  (*fn_HasWorkSlot2_t)(uintptr_t wc);                  // 0x14012ead0
typedef uint64_t (*fn_SetWorkplaceActive2_t)(uintptr_t wp, char);  // 0x14012fc00

fn_FindSM_t              s_FindStateMachine = nullptr;   // 0x140003ea0
fn_Find_t                s_FindWork_3050    = nullptr;   // 0x140003050
fn_Find_t                s_FindWorkPlace2e10= nullptr;   // 0x140002e10
fn_HasWorkSlot2_t        s_HasWorkSlot2     = nullptr;   // 0x14012ead0
fn_SetWorkplaceActive2_t s_SetWpActive2     = nullptr;   // 0x14012fc00
bool                     s_cmdResolved      = false;

void EnsureCmdHelpers() {
    if (s_cmdResolved) return;
    s_cmdResolved = true;
    s_FindStateMachine = reinterpret_cast<fn_FindSM_t>(ResolveAddr(Module::Exe, 0x140003ea0));
    s_FindWork_3050    = reinterpret_cast<fn_Find_t>(ResolveAddr(Module::Exe, 0x140003050));
    s_FindWorkPlace2e10= reinterpret_cast<fn_Find_t>(ResolveAddr(Module::Exe, 0x140002e10));
    s_HasWorkSlot2     = reinterpret_cast<fn_HasWorkSlot2_t>(ResolveAddr(Module::Exe, 0x14012ead0));
    s_SetWpActive2     = reinterpret_cast<fn_SetWorkplaceActive2_t>(
                             ResolveAddr(Module::Exe, 0x14012fc00));
    if (!s_FindStateMachine || !s_FindWork_3050 || !s_FindWorkPlace2e10 ||
        !s_HasWorkSlot2 || !s_SetWpActive2) {
        std::fprintf(stderr, "[OrchardCmd] UNRESOLVED:%s%s%s%s%s\n",
            s_FindStateMachine  ? "" : " FindStateMachine(3ea0)",
            s_FindWork_3050     ? "" : " FindWorkComponent(3050)",
            s_FindWorkPlace2e10 ? "" : " FindWorkPlace(2e10)",
            s_HasWorkSlot2      ? "" : " HasWorkSlot(12ead0)",
            s_SetWpActive2      ? "" : " SetWorkplaceActive(12fc00)");
        std::fflush(stderr);
    }
}

// The SM's current state id: sm+0x52 (pending), falling back to sm+0x50 when negative.
// NB these are offsets on the StateMachineComponent, and the `sm` the state handlers
// use is that component + 0x48.
int OrchardCurrentStateId(uintptr_t smc) {
    int8_t v = *reinterpret_cast<int8_t*>(smc + 0x52);
    if (v < 0) v = *reinterpret_cast<int8_t*>(smc + 0x50);
    return v;
}

int OrchardPlantedCount(uintptr_t s) {
    const int count = *reinterpret_cast<int32_t*>(s + 0x58);
    uintptr_t slots = *reinterpret_cast<uintptr_t*>(s + 0x50);
    int n = 0;
    for (int i = 0; slots && i < count; ++i) {
        const int st = *reinterpret_cast<int32_t*>(slots + static_cast<uintptr_t>(i) * 0x28 + 4);
        if (st != 0 && st == 1) ++n;      // retail's redundant pair, kept verbatim
    }
    return n;
}

// Reports what the panel was told, and WHY, one line per change of the deciding inputs.
//
// ⚠ Budget PER COMMAND ID, and fire ON CHANGE rather than first-N. A first-N budget CANNOT
// answer "why is the harvest button greyed out": the panel polls CanCommand every frame from
// the moment the orchard exists, so the first three polls are spent hundreds of frames before
// the player ever opens the panel, and the values at the moment they actually looked are
// never logged. That is exactly what happened on 2026-08-10 — the trace reported state=9
// (still planting) for a question asked in state 10. Third instance of the saturating-counter
// trap in this wave. It also prints the RETURN, not just the inputs: a diagnostic that makes
// the reader re-derive the predicate is one the reader can get wrong.
void TraceCanCommand(int cmdId, int curState, int planted, int slotCount, bool hasType,
                     char masterSwitch, int lastHarvestYear, uint8_t result) {
    struct Key { int state, planted, slots, hasType, sw, year, result; bool valid; };
    static Key s_last[8] = {};
    const int bucket = (cmdId >= 0 && cmdId < 8) ? cmdId : 7;
    Key k{curState, planted, slotCount, (int)hasType, (int)masterSwitch, lastHarvestYear,
          (int)result, true};
    Key& prev = s_last[bucket];
    if (prev.valid && prev.state == k.state && prev.planted == k.planted &&
        prev.slots == k.slots && prev.hasType == k.hasType && prev.sw == k.sw &&
        prev.year == k.year && prev.result == k.result)
        return;
    prev = k;

    uintptr_t world = s_pWorldSingleton2 ? reinterpret_cast<uintptr_t>(*s_pWorldSingleton2) : 0;
    uintptr_t cal   = world ? *reinterpret_cast<uintptr_t*>(world + 0x128) : 0;
    const int curYear = cal ? *reinterpret_cast<int32_t*>(cal) : -1;

    // Name the gate that actually decided it, so a greyed button reports its own reason.
    const char* why = "enabled";
    if (result == 0) {
        if (!hasType)                    why = "no tree type selected";
        else if (cmdId == 1 && curState != 10) why = "not in state 10 (tend) — harvest only "
                                                     "offered on a grown orchard";
        else if (cmdId == 1)             why = "already harvested this year (+0x4c == curYear)";
        else if (cmdId == 3 && planted == 0)   why = "nothing planted";
        else if (cmdId == 3)             why = "already clearing (state 20)";
        else                             why = "busy or fully planted";
    }
    std::fprintf(stderr, "[OrchardCmd] CanCommand(id=%d) -> %u (%s): state=%d planted=%d/%d "
                 "hasType=%d switch=%d lastHarvestYear=%d curYear=%d\n",
                 cmdId, result, why, curState, planted, slotCount, (int)hasType,
                 (int)masterSwitch, lastHarvestYear, curYear);
    std::fflush(stderr);
}

}  // namespace

// 0x140051c90 door: called by name from standalone/orchard_cut_harness.cpp.
extern "C" uint8_t OrchardComponent_CanCommand(void* selfV, int cmdId) {
    if (!selfV) return 0;
    return static_cast<uint8_t>(reinterpret_cast<OrchardComponent*>(selfV)->OrchardComponent::CommandMask(cmdId));   // B.3.8 bridge
}

// 0x140051ba0 door: called by name from standalone/orchard_cut_harness.cpp.
extern "C" void OrchardComponent_Command(void* selfV, int cmdId) {
    if (!selfV) return;
    reinterpret_cast<OrchardComponent*>(selfV)->OrchardComponent::Command(cmdId);   // B.3.8 bridge
}

// ============================================================================
// ENDGAME B.3.8 MI-B — OrchardComponent as a REAL class (OrchardComponent.h).
//
// Before: CONTROLLER_LIVE_FACTORY_STATES (48-slot table filled per create, slots
// 3/4/13/14/15/18-21 written from InstallLive, Inert_ScalarDtor in slot 0,
// Dispatcher<OrchardCtrlTag> stamped into raw bytes at +0x28). Slots 18-21 held
// ControllerLive_ScalarDeletingDtor, the class's type info / id and `return 3`: that
// is OrchardDescription's table 0x14015a210 read past this 18-slot table's end
// (B38 D3.5) — gone; nothing in the tree dispatches a component past slot 17.
// After: the compiler's 18-slot vtable, a real ctor/dtor chain, the controller a
// member object (B38_RULES D1, D2, D6).
// ============================================================================
#include "Object/factory_registration.h"

// 0x140051f40. Component() is the CALL 0x1401188b0; the stamps are the compiler's;
// `_controller(this)` is the member's back-pointer; +0x48 = 0; +0x4c = -1; +0x50 =
// +0x58 = +0x60 = 0; `_flags |= 0x80` (already set by Component()).
// +0x4c is the last-harvest year the state-10 harvest gate compares with the current
// year: -1 means "never harvested". The tree's factory zero-filled it, which read as
// "already harvested in year 0".
OrchardComponent::OrchardComponent()
    : _controller(this), orchardFlag(0), _pad49{}, orchardId(-1), field50(nullptr),
      field58(nullptr), field60(nullptr) {
    OrFlagsInCtor(0x80);
}

// 0x140051ed0: MemFree(+0x50), +0x50 = +0x58 = 0, then the restamps and ~Internal
// (the compiler chain).
OrchardComponent::~OrchardComponent() {
    Object::detail::InternalMemFree(field50);
    field50 = nullptr;
    field58 = nullptr;
}

// create 0x140009870: MemAlloc(0x68, 8), then the ctor.
void* OrchardComponent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(OrchardComponent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) OrchardComponent());
}

// Retail's createInPlace is `Internal::CreateInPlaceObject` (returns null); the
// tree's in-place entry built the object, and that is kept (B38 B8-13).
void* OrchardComponent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) OrchardComponent());
}

// ============================================================
// OrchardComponent::Persist (vtable slot [3], 0x140051de0)
// ============================================================
// Component_Serialize + empty controller serializer + second serializer:
//   validate refs, serialize orchardFlag, orchardId, entity ref array, resource array

void OrchardComponent::Persist(void* stream) {
    // Slot-3 serializers are dispatched straight off the vtable by the entity
    // stream, so nothing else ever reaches this file. Resolving here is what
    // keeps the pointers below non-null -- when they were null the whole body
    // read ZERO bytes and silently desynced every object after it.
    OrchardComponent_InitFuncs();
    uintptr_t selfAddr = reinterpret_cast<uintptr_t>(this);

    Component::Persist(stream);   // retail: CALL 0x1401189b0 (qualified)

    // Empty controller serializer block
    alignas(8) uint8_t serializer1[24];
    ::new (static_cast<void*>(serializer1)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer1)->~Serializer();   // B.3.4 bridge

    // Main data serializer
    alignas(8) uint8_t serializer2[24];
    ::new (static_cast<void*>(serializer2)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // Validate entity references
    if (s_fn_ValidateOrchardRefs)
        s_fn_ValidateOrchardRefs((void*)selfAddr);

    // Serialize orchardFlag (bool at +0x48)
    reinterpret_cast<IO::Serializer*>(serializer2)->Serialize(*reinterpret_cast<bool*>(selfAddr + 0x48), nullptr);   // B.3.4 bridge

    // Serialize orchardId (int at +0x4C)
    reinterpret_cast<IO::Serializer*>(serializer2)->Serialize(*reinterpret_cast<int*>(selfAddr + 0x4C), nullptr);   // B.3.4 bridge

    // Serialize entity ref count from +0x58, resize +0x50 if loading
    int localCount = *(int*)(selfAddr + 0x58);
    reinterpret_cast<IO::Serializer*>(serializer2)->Serialize(localCount, nullptr);   // B.3.4 bridge

    if (static_cast<int>(reinterpret_cast<IO::Stream*>(stream)->GetDirection()) == 0 && s_fn_ResizeRefArray) {   // B.3.4 bridge  (IO::Stream::GetDirection, retail 0x180006200)
        s_fn_ResizeRefArray((void*)(selfAddr + 0x50), localCount);
    }

    // Serialize entity ref array entries
    if (s_fn_SerializeRefEntries)
        s_fn_SerializeRefEntries(serializer2, localCount, *(void**)(selfAddr + 0x50));

    // Serialize resource IBP array at +0x60
    if (s_fn_SerializeIBPArray)
        s_fn_SerializeIBPArray((void*)(selfAddr + 0x60), serializer2);

    reinterpret_cast<IO::Serializer*>(serializer2)->~Serializer();   // B.3.4 bridge
}

// [4] 0x140051dd0: `MOV qword [RCX+0x60],0` — one store, nothing else.
void OrchardComponent::Startup() { field60 = nullptr; }

// [13] 0x140051c90 / [14] 0x140051ba0 / [15] 0x140051990 below; the extern "C" doors
// of [13]/[14] stay above (standalone/orchard_cut_harness.cpp calls them).
// idx13 CanCommand — retail 0x140051c90. Returns the button's enabled state.
//   id 0 WORK    : (no work slot held && planted < slots) || slots == 0, ANDed with hasType
//   id 1 HARVEST : state == 10 && lastHarvestYear(+0x4c) != current year, ANDed with hasType
//   id 2 TOGGLE  : bit0 = hasType, bit1 = the +0x48 master switch (a BITFIELD, not a bool —
//                  the panel reads bit 1 to draw the on/off state)
//   id 3 CLEAR   : planted > 0 && state != 20
// `hasType = (*(self+0x60) != 0)` is ANDed into ids 0/1/2 — an untyped orchard is
// correctly inert in retail, which is why an unconfigured orchard shows dead buttons.
uint32_t OrchardComponent::CommandMask(int cmdId) {
    EnsureCmdHelpers();
    EnsureSubHelpers();
    const uintptr_t s = reinterpret_cast<uintptr_t>(this);

    uintptr_t smcIBP = 0;
    if (s_FindStateMachine) s_FindStateMachine(*reinterpret_cast<uintptr_t*>(s + 0x10), &smcIBP);
    uintptr_t wcIBP = 0;
    if (s_FindWork_3050) s_FindWork_3050(*reinterpret_cast<uintptr_t*>(s + 0x10), &wcIBP);
    if (!smcIBP) return 0;      // SA: retail derefs unguarded; every orchard entity has a
                                // StateMachineComponent, so this can only fire if the
                                // component was asked before its entity finished building.
    const int  curState   = OrchardCurrentStateId(smcIBP);
    const int  planted    = OrchardPlantedCount(s);
    const int  slotCount  = *reinterpret_cast<int32_t*>(s + 0x58);
    const bool hasType    = (*reinterpret_cast<uintptr_t*>(s + 0x60) != 0);

    uint8_t result = 0;
    switch (cmdId) {
    case 0: {
        const bool busy = s_HasWorkSlot2 && s_HasWorkSlot2(wcIBP) != 0;
        if ((!busy && planted < slotCount) || slotCount == 0) result = hasType ? 1 : 0;
        break;
    }
    case 1: {
        if (curState != 10) break;
        uintptr_t world = s_pWorldSingleton2 ? reinterpret_cast<uintptr_t>(*s_pWorldSingleton2) : 0;
        uintptr_t cal   = world ? *reinterpret_cast<uintptr_t*>(world + 0x128) : 0;
        const int year  = cal ? *reinterpret_cast<int32_t*>(cal) : 0;
        if (*reinterpret_cast<int32_t*>(s + 0x4c) == year) break;
        result = hasType ? 1 : 0;
        break;
    }
    case 2:
        result = static_cast<uint8_t>(
            (static_cast<uint8_t>(-(*reinterpret_cast<char*>(s + 0x48) != 0)) & 2u) |
            (hasType ? 1u : 0u));
        break;
    case 3:
        result = (planted > 0 && curState != 20) ? 1 : 0;
        break;
    default:
        break;
    }
    TraceCanCommand(cmdId, curState, planted, slotCount, hasType,
                    *reinterpret_cast<char*>(s + 0x48), *reinterpret_cast<int32_t*>(s + 0x4c),
                    result);
    return result;
}

// idx14 Command — retail 0x140051ba0. Maps a button id to a NODE EVENT and posts it.
//   0 -> event 3 (plant/work)   1 -> event 4 (harvest)   3 -> event 5 (clear)
//   2 -> no event at all: flip the +0x48 master switch and push it to the workplace.
// The post itself is node vtable slot 2 (+0x10):
//   (**(code**)(**(longlong**)(smc+0x48) + 0x10))(node, smc+0x48, (int)*(char*)(smc+0x51), eventId)
// i.e. curIdx comes from smc+0x51, which is (smc+0x48)+0x09 — the same sm+0x09 field
// OrchardRequestState reads. ControllerLive::Dispatcher::PostEvent implements that slot.
void OrchardComponent::Command(int cmdId) {
    EnsureCmdHelpers();
    const uintptr_t s = reinterpret_cast<uintptr_t>(this);

    uintptr_t smcIBP = 0;
    if (s_FindStateMachine) s_FindStateMachine(*reinterpret_cast<uintptr_t*>(s + 0x10), &smcIBP);

    int eventId;
    switch (cmdId) {
    case 0: eventId = 3; break;
    case 1: eventId = 4; break;
    case 3: eventId = 5; break;
    case 2: {
        // Toggle the master switch and mirror it onto the workplace. Retail returns here:
        // no event is posted for this command.
        *reinterpret_cast<char*>(s + 0x48) = (*reinterpret_cast<char*>(s + 0x48) == 0) ? 1 : 0;
        uintptr_t wpIBP = 0;
        if (s_FindWorkPlace2e10) s_FindWorkPlace2e10(*reinterpret_cast<uintptr_t*>(s + 0x10), &wpIBP);
        if (s_SetWpActive2) s_SetWpActive2(wpIBP, *reinterpret_cast<char*>(s + 0x48));
        std::fprintf(stderr, "[OrchardCmd] Command(2) toggle -> enabled=%d\n",
                     (int)*reinterpret_cast<char*>(s + 0x48));
        std::fflush(stderr);
        return;
    }
    default: return;
    }

    if (!smcIBP) return;
    uintptr_t sm   = smcIBP + 0x48;
    void*     node = *reinterpret_cast<void**>(sm);
    std::fprintf(stderr, "[OrchardCmd] Command(%d) -> post node event %d (node=%p curIdx=%d)\n",
                 cmdId, eventId, node, (int)*reinterpret_cast<int8_t*>(smcIBP + 0x51));
    std::fflush(stderr);
    if (!node) return;
    // node = the controller member the StateMachineComponent linked (D6); slot 2 PostEvent.
    reinterpret_cast<ControllerComponent::Controller*>(node)->PostEvent(   // B.3.8 bridge
        reinterpret_cast<void*>(sm),
        static_cast<int>(*reinterpret_cast<int8_t*>(smcIBP + 0x51)), eventId);
}

// Diagnostic counters of the [OrchardMsg] lines (B6-5: namespace-scope atomics,
// not function-local statics).
static std::atomic<int> s_orchardMsgSeen[8] = {};
static std::atomic<int> s_orchardMsg0Traced{0};

void OrchardComponent::OnMessage(int msgId, void* /*srcEntityRef*/, uint32_t slotIdx, uint32_t /*p2*/) {
    EnsureSubHelpers();
    EnsureMsgHelpers();
    {   // first few deliveries only — this is the handler the slot array depends on, and
        // "was it ever called at all" is the question its silence raises.
        // Per-message-id budget: a single shared counter let the 12 planting deliveries
        // (msg 0) hide every harvest delivery (msg 1) behind an exhausted budget.
        const int bucket = (msgId >= 0 && msgId < 8) ? msgId : 7;
        if (s_orchardMsgSeen[bucket].fetch_add(1, std::memory_order_relaxed) < 6) {
            std::fprintf(stderr, "[OrchardMsg] OnMessage(comp=%p msg=%d slot=%u)\n",
                         static_cast<void*>(this), msgId, slotIdx);
            std::fflush(stderr);
        }
    }
    const uintptr_t self = reinterpret_cast<uintptr_t>(this);
    const uintptr_t idx  = static_cast<uintptr_t>(slotIdx);
    uintptr_t world = s_pWorldSingleton2 ? reinterpret_cast<uintptr_t>(*s_pWorldSingleton2) : 0;
    if (!world) return;

    if (msgId == 0) {
        const uintptr_t slots = *reinterpret_cast<uintptr_t*>(self + 0x50);
        if (!slots) return;
        const uintptr_t slot = slots + idx * 0x28;
        const uintptr_t mapGrid = *reinterpret_cast<uintptr_t*>(world + 0xf8);
        // The tile record is stride 0x30 in *(mapGrid+0x80); its +0x08 is the entity
        // standing on it — the tree the plant action just created.
        int64_t entity = *reinterpret_cast<int64_t*>(
            *reinterpret_cast<uintptr_t*>(mapGrid + 0x80) + 8 +
            static_cast<int64_t>(*reinterpret_cast<int32_t*>(slot)) * 0x30);
        int64_t natResIBP = 0;
        if (s_fn_IBPAccessor52c0) s_fn_IBPAccessor52c0(entity, &natResIBP);
        int64_t fruitIBP = 0;
        if (s_fn_FindFruit_a0b0) s_fn_FindFruit_a0b0(entity, &fruitIBP);

        {   // msg 0 is the ONLY writer of the slot's {entity, natRes, fruit} triple, and its
            // two outcomes — bound (state 1) and unusable (state 2) — are indistinguishable
            // from the slot histogram alone. Say which of the three came back null.
            if (s_orchardMsg0Traced.fetch_add(1, std::memory_order_relaxed) < 8) {
                std::fprintf(stderr, "[OrchardMsg]   msg0 slot=%u tile=%d -> entity=%p "
                             "natRes=%p fruit=%p => state %d\n", slotIdx,
                             (int)*reinterpret_cast<int32_t*>(slot),
                             reinterpret_cast<void*>(entity), reinterpret_cast<void*>(natResIBP),
                             reinterpret_cast<void*>(fruitIBP),
                             (entity && natResIBP && fruitIBP) ? 1 : 2);
                std::fflush(stderr);
            }
        }
        if (entity != 0 && natResIBP != 0 && fruitIBP != 0) {
            *reinterpret_cast<int32_t*>(slot + 0x04) = 1;          // planted
            *reinterpret_cast<int64_t*>(slot + 0x08) = entity;
            *reinterpret_cast<int64_t*>(slot + 0x10) = natResIBP;
            *reinterpret_cast<int64_t*>(slot + 0x18) = fruitIBP;
            return;
        }
        *reinterpret_cast<int32_t*>(slot + 0x04) = 2;              // unusable
        return;
    }

    if (msgId == 1) {
        OrchardComponent_ValidateRefs(this);
        const uintptr_t slots = *reinterpret_cast<uintptr_t*>(self + 0x50);
        if (!slots) return;
        const uintptr_t slot = slots + idx * 0x28;
        *reinterpret_cast<uint8_t*>(slot + 0x20) = 1;
        if (*reinterpret_cast<int64_t*>(slot + 0x18) != 0 &&
            *reinterpret_cast<int64_t*>(slot + 0x08) != 0) {
            const uintptr_t fruit = *reinterpret_cast<uintptr_t*>(slot + 0x18);
            *reinterpret_cast<uint32_t*>(fruit + 0x30) = 0;
            *reinterpret_cast<uint8_t*>(fruit + 0x34) = 1;
        }
        return;
    }

    if (msgId == 2) {
        const uintptr_t mapGrid = *reinterpret_cast<uintptr_t*>(world + 0xf8);
        OrchardComponent_ValidateRefs(this);
        const uintptr_t slots = *reinterpret_cast<uintptr_t*>(self + 0x50);
        if (!slots) return;
        int64_t entity = *reinterpret_cast<int64_t*>(
            *reinterpret_cast<uintptr_t*>(mapGrid + 0x80) + 8 +
            static_cast<int64_t>(*reinterpret_cast<int32_t*>(slots + idx * 0x28)) * 0x30);
        if (entity == 0) return;
        int64_t fruitIBP = 0;
        if (s_fn_FindFruit_a0b0) s_fn_FindFruit_a0b0(entity, &fruitIBP);
        if (fruitIBP == 0) return;
        void* desc = *reinterpret_cast<void**>(self + 0x18);
        void* validated = nullptr;
        if (fn_IsDerivedFrom && fn_IsDerivedFrom(desc, OrchardDescription::TypeId()))
            validated = desc;
        OrchardComponent_BoostFruit_49fc0(reinterpret_cast<int64_t*>(fruitIBP),
            *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(validated) + 0x2c));
        return;
    }
}

DEFINE_EXE_TYPE(OrchardComponent, "ControllerComponent")
