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
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Resolved helpers
// ============================================================

typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t s_fn_Component_Serialize = nullptr;

typedef void* (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t s_fn_Serializer_Ctor = nullptr;

typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t s_fn_Serializer_Dtor = nullptr;

typedef void (*fn_SerializeBool_t)(void* self, bool* value, const char* name);
static fn_SerializeBool_t s_fn_SerializeBool = nullptr;

typedef void (*fn_SerializeInt_t)(void* self, int* value, const char* name);
static fn_SerializeInt_t s_fn_SerializeInt = nullptr;

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
// OrchardComponent::Shutdown (0x140051de0)
// ============================================================
// Component_Serialize + empty controller serializer + second serializer:
//   validate refs, serialize orchardFlag, orchardId, entity ref array, resource array

void OrchardComponent_Shutdown(OrchardComponent* self, void* stream) {
    uintptr_t selfAddr = (uintptr_t)self;

    if (s_fn_Component_Serialize)
        s_fn_Component_Serialize(self, stream);

    // Empty controller serializer block
    uint8_t serializer1[24];
    if (s_fn_Serializer_Ctor) s_fn_Serializer_Ctor(serializer1, stream, 0);
    if (s_fn_Serializer_Dtor) s_fn_Serializer_Dtor(serializer1);

    // Main data serializer
    uint8_t serializer2[24];
    if (s_fn_Serializer_Ctor) s_fn_Serializer_Ctor(serializer2, stream, 0);

    // Validate entity references
    if (s_fn_ValidateOrchardRefs)
        s_fn_ValidateOrchardRefs((void*)selfAddr);

    // Serialize orchardFlag (bool at +0x48)
    if (s_fn_SerializeBool)
        s_fn_SerializeBool(serializer2, (bool*)(selfAddr + 0x48), nullptr);

    // Serialize orchardId (int at +0x4C)
    if (s_fn_SerializeInt)
        s_fn_SerializeInt(serializer2, (int*)(selfAddr + 0x4C), nullptr);

    // Serialize entity ref count from +0x58, resize +0x50 if loading
    int localCount = *(int*)(selfAddr + 0x58);
    if (s_fn_SerializeInt)
        s_fn_SerializeInt(serializer2, &localCount, nullptr);

    if (*(int*)((uintptr_t)stream + 8) == 0 && s_fn_ResizeRefArray) {
        s_fn_ResizeRefArray((void*)(selfAddr + 0x50), localCount);
    }

    // Serialize entity ref array entries
    if (s_fn_SerializeRefEntries)
        s_fn_SerializeRefEntries(serializer2, localCount, *(void**)(selfAddr + 0x50));

    // Serialize resource IBP array at +0x60
    if (s_fn_SerializeIBPArray)
        s_fn_SerializeIBPArray((void*)(selfAddr + 0x60), serializer2);

    if (s_fn_Serializer_Dtor) s_fn_Serializer_Dtor(serializer2);
}

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
static uint32_t* s_pTypeMapComp = nullptr;  // 0x1401c79e0 = MapComponent (TypeInfo)
static uint32_t* s_pType7b30 = nullptr;             // DAT_1401c7b30
static uint32_t* s_pType4250 = nullptr;             // DAT_1401c4250
static void**    s_pWorldSingleton2 = nullptr;      // g_pGameWorldSingleton (0x1401c3dd8)

static bool s_subResolved = false;
static void EnsureSubHelpers() {
    if (s_subResolved) return;
    s_fn_BaseIsA          = reinterpret_cast<fn_BaseIsA_t>(ResolveAddr(Module::Exe, 0x140002d40));
    s_fn_IBPAccessor52c0  = reinterpret_cast<fn_IBPAccessor_t>(ResolveAddr(Module::Exe, 0x1400052c0));
    s_pTypeMapComp= reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c79e0));
    s_pType7b30           = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7b30));
    s_pType4250           = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c4250));
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
        if (s_fn_SerializeInt) s_fn_SerializeInt(serializer, reinterpret_cast<int*>(e + 0), nullptr);
        if (s_fn_SerializeInt) s_fn_SerializeInt(serializer, reinterpret_cast<int*>(e + 4), nullptr);

        if (fn_InternalBasePtr_Serialize) fn_InternalBasePtr_Serialize(e + 8, serializer);
        if (*reinterpret_cast<void**>(e + 8) != nullptr && s_pTypeMapComp &&
            !(fn_IsDerivedFrom && fn_IsDerivedFrom(*reinterpret_cast<void**>(e + 8), *s_pTypeMapComp)))
            *reinterpret_cast<void**>(e + 8) = nullptr;

        if (fn_InternalBasePtr_Serialize) fn_InternalBasePtr_Serialize(e + 0x10, serializer);
        if (*reinterpret_cast<void**>(e + 0x10) != nullptr && s_pType7b30 &&
            !(fn_IsDerivedFrom && fn_IsDerivedFrom(*reinterpret_cast<void**>(e + 0x10), *s_pType7b30)))
            *reinterpret_cast<void**>(e + 0x10) = nullptr;

        if (fn_InternalBasePtr_Serialize) fn_InternalBasePtr_Serialize(e + 0x18, serializer);
        if (*reinterpret_cast<void**>(e + 0x18) != nullptr && s_pType4250 &&
            !(fn_IsDerivedFrom && fn_IsDerivedFrom(*reinterpret_cast<void**>(e + 0x18), *s_pType4250)))
            *reinterpret_cast<void**>(e + 0x18) = nullptr;

        if (s_fn_SerializeBool) s_fn_SerializeBool(serializer, reinterpret_cast<bool*>(e + 0x20), nullptr);
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
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&live);
            if (s_fn_BaseIsA && s_pTypeMapComp &&
                s_fn_BaseIsA(live, *s_pTypeMapComp)) {
                if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&live);
                int64_t accBuf = 0;
                void* acc = s_fn_IBPAccessor52c0 ? s_fn_IBPAccessor52c0((int64_t)live, &accBuf) : nullptr;
                if (acc && *reinterpret_cast<int64_t*>(acc) != 0) {
                    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(entry + 8));
                    if (live == *reinterpret_cast<void**>(entry + 8)) ok = true;
                }
            }
        }
        if (ok) continue;
        // Stale -> mark removed (kind=2) and null the 3 IBP slots.
        *reinterpret_cast<int32_t*>(entry + 4) = 2;
        int64_t z = 0;
        if (fn_InternalBasePtr_Ctor) { fn_InternalBasePtr_Ctor(&z, nullptr);
            *reinterpret_cast<int64_t*>(entry + 8) = z; }
        z = 0;
        if (fn_InternalBasePtr_Ctor) { fn_InternalBasePtr_Ctor(&z, nullptr);
            *reinterpret_cast<int64_t*>(entry + 0x10) = z; }
        z = 0;
        if (fn_InternalBasePtr_Ctor) { fn_InternalBasePtr_Ctor(&z, nullptr);
            *reinterpret_cast<int64_t*>(entry + 0x18) = z; }
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void OrchardComponent_InitFuncs() {
    if (s_initialized) return;

    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));
    s_fn_ValidateOrchardRefs = reinterpret_cast<fn_ValidateOrchardRefs_t>(
        ResolveAddr(Module::Exe, 0x140051830));
    s_fn_ResizeRefArray = reinterpret_cast<fn_ResizeRefArray_t>(
        ResolveAddr(Module::Exe, 0x140009f80));
    s_fn_SerializeRefEntries = reinterpret_cast<fn_SerializeRefEntries_t>(
        ResolveAddr(Module::Exe, 0x14000a230));
    s_fn_SerializeIBPArray = reinterpret_cast<fn_SerializeIBPArray_t>(
        ResolveAddr(Module::Exe, 0x140002340));

    uintptr_t slot;
    slot = ResolveAddr(Module::Exe, 0x140152ad8);
    if (slot) s_fn_Serializer_Ctor = *reinterpret_cast<fn_Serializer_Ctor_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152ad0);
    if (slot) s_fn_Serializer_Dtor = *reinterpret_cast<fn_Serializer_Dtor_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152a58);
    if (slot) s_fn_SerializeBool = *reinterpret_cast<fn_SerializeBool_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152ac8);
    if (slot) s_fn_SerializeInt = *reinterpret_cast<fn_SerializeInt_t*>(slot);

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

    const uint32_t tidEntity = g_TypeId_ComponentEntity ? *g_TypeId_ComponentEntity : 0;
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
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&entity);
        if (!fn_IsA || !fn_IsA(entity, tidEntity)) continue;

        uint64_t natRes = 0;
        s_findNatRes(entity, &natRes);
        if (!natRes) continue;

        void** owned = reinterpret_cast<void**>(slot + 0x08);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(owned);
        if (entity != *owned) continue;
        if (*reinterpret_cast<int32_t*>(slot + 0x04) != 1) continue;

        void** growthObj = reinterpret_cast<void**>(slot + 0x18);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(growthObj);
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
static uint32_t* s_pTypeFruitDesc = nullptr;    // DAT_1401c4288 FruitDescription
static uint32_t* s_pTypeOrchardDesc = nullptr;  // DAT_1401c46e8 OrchardDescription
static float*    s_pf_172234 = nullptr;         // 300.0f
static bool s_msgResolved = false;
static void EnsureMsgHelpers() {
    if (s_msgResolved) return;
    s_msgResolved = true;
    s_fn_FindFruit_a0b0 = reinterpret_cast<fn_FindFruit_a0b0_t>(
        ResolveAddr(Module::Exe, 0x14000a0b0));
    s_pTypeFruitDesc   = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c4288));
    s_pTypeOrchardDesc = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c46e8));
    s_pf_172234        = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x140172234));
}

// Reimpl of OrchardComponent_helper_9fc0 (0x140049fc0) — advance a FruitComponent's growth
// by `scale` worth of its description's rate, by synthesising a Time and calling the
// component's own Update (vtable +0x48, slot 9).
static void OrchardComponent_BoostFruit_49fc0(int64_t* fruitComp, float scale) {
    EnsureMsgHelpers();
    void* desc = reinterpret_cast<void*>(fruitComp[3]);          // +0x18
    void* validated = nullptr;
    if (fn_IsDerivedFrom && s_pTypeFruitDesc && fn_IsDerivedFrom(desc, *s_pTypeFruitDesc))
        validated = desc;
    const float rate  = *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(validated) + 0x20);
    const float secs  = rate * (s_pf_172234 ? *s_pf_172234 : 300.0f) * scale;
    // System::Time is 8 bytes — a 4-byte target here is the documented clobber trap.
    uint64_t timeVal = 0;
    if (fn_Time_Ctor_Float) fn_Time_Ctor_Float(&timeVal, secs);
    typedef void (*Update_t)(void*, void*);
    void* vt = *reinterpret_cast<void**>(fruitComp);
    reinterpret_cast<Update_t>(*reinterpret_cast<uintptr_t*>(
        reinterpret_cast<uint8_t*>(vt) + 0x48))(fruitComp, &timeVal);
}

extern "C" void OrchardComponent_OnMessage(void* selfV, int msgId, void* /*srcEntityRef*/,
                                           uint32_t slotIdx, uint32_t /*p2*/) {
    EnsureSubHelpers();
    EnsureMsgHelpers();
    {   // first few deliveries only — this is the handler the slot array depends on, and
        // "was it ever called at all" is the question its silence raises.
        static int s_seen = 0;
        if (s_seen++ < 6) {
            std::fprintf(stderr, "[OrchardMsg] OnMessage(comp=%p msg=%d slot=%u)\n",
                         selfV, msgId, slotIdx);
            std::fflush(stderr);
        }
    }
    if (!selfV) return;
    const uintptr_t self = reinterpret_cast<uintptr_t>(selfV);
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
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&entity);
        int64_t natResIBP = 0;
        if (s_fn_IBPAccessor52c0) s_fn_IBPAccessor52c0(entity, &natResIBP);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&entity);
        int64_t fruitIBP = 0;
        if (s_fn_FindFruit_a0b0) s_fn_FindFruit_a0b0(entity, &fruitIBP);

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
        OrchardComponent_ValidateRefs(selfV);
        const uintptr_t slots = *reinterpret_cast<uintptr_t*>(self + 0x50);
        if (!slots) return;
        const uintptr_t slot = slots + idx * 0x28;
        *reinterpret_cast<uint8_t*>(slot + 0x20) = 1;
        if (*reinterpret_cast<int64_t*>(slot + 0x18) != 0 &&
            *reinterpret_cast<int64_t*>(slot + 0x08) != 0) {
            if (fn_InternalBasePtr_CheckPtr)
                fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(slot + 0x18));
            const uintptr_t fruit = *reinterpret_cast<uintptr_t*>(slot + 0x18);
            *reinterpret_cast<uint32_t*>(fruit + 0x30) = 0;
            *reinterpret_cast<uint8_t*>(fruit + 0x34) = 1;
        }
        return;
    }

    if (msgId == 2) {
        const uintptr_t mapGrid = *reinterpret_cast<uintptr_t*>(world + 0xf8);
        OrchardComponent_ValidateRefs(selfV);
        const uintptr_t slots = *reinterpret_cast<uintptr_t*>(self + 0x50);
        if (!slots) return;
        int64_t entity = *reinterpret_cast<int64_t*>(
            *reinterpret_cast<uintptr_t*>(mapGrid + 0x80) + 8 +
            static_cast<int64_t>(*reinterpret_cast<int32_t*>(slots + idx * 0x28)) * 0x30);
        if (entity == 0) return;
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&entity);
        int64_t fruitIBP = 0;
        if (s_fn_FindFruit_a0b0) s_fn_FindFruit_a0b0(entity, &fruitIBP);
        if (fruitIBP == 0) return;
        void* desc = *reinterpret_cast<void**>(self + 0x18);
        void* validated = nullptr;
        if (fn_IsDerivedFrom && s_pTypeOrchardDesc && fn_IsDerivedFrom(desc, *s_pTypeOrchardDesc))
            validated = desc;
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&fruitIBP);
        OrchardComponent_BoostFruit_49fc0(reinterpret_cast<int64_t*>(fruitIBP),
            *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(validated) + 0x2c));
        return;
    }
}

// ===== Standalone factory (Foundation D — component instantiation) =====
//
// Promoted off COMPONENT_INERT_FACTORY to a live ControllerComponent. While it was
// inert, the embedded state node at comp+0x28 was never installed and never linked
// into the entity's StateMachineComponent, so nothing ticked: the slot array at
// +0x50/+0x58 was never built, no PlantOrchard work was ever posted, no trees grew,
// and no HarvestOrchard work was ever scheduled. That was the whole "orchard produces
// no work or yield" symptom.
//
// The three macro arguments are byte-verified against retail, not inferred:
//   Size 0x68 / Flags 0x80 — OrchardComponent_Ctor (0x140051f40) allocates 0x68 and
//     ORs 0x80 into +0x20.
//   NodeOff 0x28 — the ctor writes vtable_OrchardComponent_Controller (0x14015a130)
//     to +0x28, and primary vtable slot 17 (CreateState) at 0x140004080 is literally
//     `48 8D 41 28 C3` = LEA RAX,[RCX+0x28]; RET. The macro also writes the owner
//     backptr at NodeOff+0x18 = +0x40, exactly as the ctor's `*(self+0x40) = self`.
//   DefState 2 — primary vtable slot 16 (GetDefaultState) at 0x1400026c0 is literally
//     `B8 02 00 00 00 C3` = MOV EAX,2; RET. (The `activeState=3` seen in the runtime
//     diagnostic is the ENTITY-level StateMachine state, a different thing — do not
//     conflate them.)
//
// Opt-in for now (BANISHED_LIVE_ORCHARD), NOT in kDefaultOnFlags: the state table
// currently registers null handlers, so this increment brings up the node and proves
// it links and ticks without changing any behaviour. It graduates to default-on once
// the per-state bodies land and produce verified planting/yield.
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"
#include "Component/controller_live_base.h"
#include "Component/controller_state_table.h"

extern "C" void** OrchardCtrl_NodeVtable();     // orchardcomponent_states.cpp
extern "C" void   OrchardCtrl_PopulateTable();

// Slot 15 (OnMessage) has to be wired from INSIDE InstallLive, not at static-init:
// MakeInertTyped -> FillTypedVtable rewrites slots 1..N with the inert stub on the first
// factory call, so anything written earlier is silently erased — the same "reversed order =
// silently un-wired" trap the stub-controller sweep documented, via a different mechanism.
// The macro only fills 16/17 itself, but it calls PopulateFn from InstallLive AFTER
// FillTypedVtable has run, so hanging the slot-15 write off the populate hook puts it in the
// one place that survives. Idempotent: InstallLive runs per component instance.
static void OrchardCtrl_PopulateTableAndWireSlot15();

CONTROLLER_LIVE_FACTORY_STATES(OrchardComponent, 0x68, 0x80, 0x28, 2,
                               "BANISHED_LIVE_ORCHARD",
                               OrchardCtrl_NodeVtable, OrchardCtrl_PopulateTableAndWireSlot15)

static void OrchardCtrl_PopulateTableAndWireSlot15() {
    OrchardComponent_LiveVtable[15] = reinterpret_cast<void*>(&OrchardComponent_OnMessage);
    OrchardCtrl_PopulateTable();
}
REGISTER_TYPE(OrchardComponent, "ControllerComponent",
              &OrchardComponent_Factory, &OrchardComponent_CreateInPlace);
