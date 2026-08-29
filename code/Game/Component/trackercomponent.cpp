// code/Game/Component/trackercomponent.cpp
// Clean reimplementation of TrackerComponent lifecycle methods from Application-x64.exe.
//
// TrackerComponent (0x50 bytes) — tracks entities for path/resource tracking.
// Fields: +0x28=array1 ptr, +0x30=array1 count, +0x38=array2 ptr,
//         +0x40=array2 count, +0x48=trackerId (int, init -1)
//
// Vtable at 0x14015ab80. One non-trivial override:
//   Shutdown (0x140059130) — Serialize two paired (IBP+int) arrays + trackerId
//

#include "Component/trackercomponent_funcs.h"
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

typedef void (*fn_SerializeInt_t)(void* self, int* value, const char* name);
static fn_SerializeInt_t s_fn_SerializeInt = nullptr;

// FUN_14000da70 — resize paired (IBP+int) array
typedef void (*fn_ResizePairedArray_t)(void* arrayPtr, int newCount);
static fn_ResizePairedArray_t s_fn_ResizePairedArray = nullptr;

// FUN_140002340 — serialize IBP array entry
typedef void (*fn_SerializeIBPEntry_t)(void* entryPtr, void* serializer);
static fn_SerializeIBPEntry_t s_fn_SerializeIBPEntry = nullptr;

static bool s_initialized = false;

// ============================================================
// TrackerComponent::Shutdown (0x140059130)
// ============================================================
// Serialize two paired arrays of (InternalBasePtr, int) entries
// at +0x28/+0x30 and +0x38/+0x40, plus trackerId at +0x48.

void TrackerComponent_Shutdown(TrackerComponent* self, void* stream) {
    // resolver call added at top, save/load wave 2026-08-28: InitFuncs was declared
    // but never called anywhere, so every s_fn_* below stayed null and load silently
    // skipped this component's fields.
    TrackerComponent_InitFuncs();
    uintptr_t selfAddr = (uintptr_t)self;

    if (s_fn_Component_Serialize)
        s_fn_Component_Serialize(self, stream);

    uint8_t serializer[24];
    if (s_fn_Serializer_Ctor) s_fn_Serializer_Ctor(serializer, stream, 0);

    // === Array 1 at +0x28/+0x30 ===
    int count1 = *(int*)(selfAddr + 0x30);
    int localCount1 = count1;
    if (s_fn_SerializeInt)
        s_fn_SerializeInt(serializer, &localCount1, nullptr);

    // If loading, resize array
    if (*(int*)((uintptr_t)stream + 8) == 0 && s_fn_ResizePairedArray) {
        s_fn_ResizePairedArray((void*)(selfAddr + 0x28), localCount1);
    }

    // Serialize each (IBP + int) pair — each pair is 16 bytes (8 IBP + 4 int + 4 pad = 16)
    void* array1 = *(void**)(selfAddr + 0x28);
    if (localCount1 > 0) {
        uintptr_t entryPtr = (uintptr_t)array1;
        for (int i = 0; i < localCount1; i++) {
            if (s_fn_SerializeIBPEntry)
                s_fn_SerializeIBPEntry((void*)entryPtr, serializer);
            if (s_fn_SerializeInt)
                s_fn_SerializeInt(serializer, (int*)(entryPtr + 8), nullptr);
            entryPtr += 16;  // sizeof(IBP) + sizeof(int) = 8 + 8
        }
    }

    // === Array 2 at +0x38/+0x40 ===
    int count2 = *(int*)(selfAddr + 0x40);
    int localCount2 = count2;
    if (s_fn_SerializeInt)
        s_fn_SerializeInt(serializer, &localCount2, nullptr);

    if (*(int*)((uintptr_t)stream + 8) == 0 && s_fn_ResizePairedArray) {
        s_fn_ResizePairedArray((void*)(selfAddr + 0x38), localCount2);
    }

    void* array2 = *(void**)(selfAddr + 0x38);
    if (localCount2 > 0) {
        uintptr_t entryPtr = (uintptr_t)array2;
        for (int i = 0; i < localCount2; i++) {
            if (s_fn_SerializeIBPEntry)
                s_fn_SerializeIBPEntry((void*)entryPtr, serializer);
            if (s_fn_SerializeInt)
                s_fn_SerializeInt(serializer, (int*)(entryPtr + 8), nullptr);
            entryPtr += 16;
        }
    }

    // Serialize trackerId at +0x48
    if (s_fn_SerializeInt)
        s_fn_SerializeInt(serializer, (int*)(selfAddr + 0x48), nullptr);

    if (s_fn_Serializer_Dtor) s_fn_Serializer_Dtor(serializer);
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void TrackerComponent_InitFuncs() {
    if (s_initialized) return;

    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));
    s_fn_ResizePairedArray = reinterpret_cast<fn_ResizePairedArray_t>(
        ResolveAddr(Module::Exe, 0x14000da70));
    s_fn_SerializeIBPEntry = reinterpret_cast<fn_SerializeIBPEntry_t>(
        ResolveAddr(Module::Exe, 0x140002340));

    uintptr_t slot;
    slot = ResolveAddr(Module::Exe, 0x140152ad8);
    if (slot) s_fn_Serializer_Ctor = *reinterpret_cast<fn_Serializer_Ctor_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152ad0);
    if (slot) s_fn_Serializer_Dtor = *reinterpret_cast<fn_Serializer_Dtor_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152ac8);
    if (slot) s_fn_SerializeInt = *reinterpret_cast<fn_SerializeInt_t*>(slot);

    s_initialized = true;
}

// ===== Standalone factory (Foundation D — component instantiation) =====
// Inert-but-valid TrackerComponent: size 0x50 + Object::Internal base + shared stub
// vtable + capability flags +0x20=0x80 (from TrackerComponent_Ctor) + zeroed body.
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"

// NOT COMPONENT_INERT_FACTORY: that macro leaves the whole body zeroed, but the
// retail ctor (TrackerComponent_Factory 0x14000d7a0) writes +0x48 = -1. +0x48 is
// the "game-world item-count generation" stamp, and 0 is a LEGAL generation
// value: with a zeroed body ProductionUI's per-frame update (0x140075a50) would
// compare gameWorld item count 0 == stamp 0 on the very first frame of a fresh
// world and SKIP the one-shot array rebuild that the -1 sentinel is there to
// force. Documented project trap: "inert-component sentinel fields" — a zero
// where a -1 belongs. Everything else is the macro's inert body verbatim.
static uint32_t TrackerComponent_InertGetTypeId(void*) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("TrackerComponent");
    return cached;
}
static void* TrackerComponent_InertVtable[::ComponentFactory::kInertVtableSlots];

// Retail ctor tail: +0x28/+0x30/+0x38/+0x40 = 0 (already zeroed), +0x48 = -1.
static void TrackerComponent_ApplyCtorDefaults(void* p) {
    if (p) *reinterpret_cast<int32_t*>(static_cast<char*>(p) + 0x48) = -1;
}

static void TrackerComponent_SerVSlot(void* s, void* stream) {
    TrackerComponent_Shutdown(reinterpret_cast<TrackerComponent*>(s), stream);
}
// slot 3 wired unconditionally, save/load wave 2026-08-28 (fields previously reset silently on load).
static void TrackerComponent_FinishVtableSer() {
    TrackerComponent_InertVtable[3] = reinterpret_cast<void*>(&TrackerComponent_SerVSlot);
}

extern "C" void* TrackerComponent_Factory(void) {
    void* p = ::ComponentFactory::MakeInertTyped(0x50, 0x80,
        TrackerComponent_InertVtable,
        reinterpret_cast<void*>(&TrackerComponent_InertGetTypeId));
    TrackerComponent_ApplyCtorDefaults(p);
    TrackerComponent_FinishVtableSer();
    return p;
}
static void* TrackerComponent_CreateInPlace(void* mem) {
    if (mem) {
        ::ComponentFactory::InitInertTyped(mem, 0x50, 0x80,
            TrackerComponent_InertVtable,
            reinterpret_cast<void*>(&TrackerComponent_InertGetTypeId));
        TrackerComponent_ApplyCtorDefaults(mem);
        TrackerComponent_FinishVtableSer();
    }
    return mem;
}

REGISTER_TYPE(TrackerComponent, "Component",
              &TrackerComponent_Factory, &TrackerComponent_CreateInPlace);
