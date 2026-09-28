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

#include "Component/TrackerComponent.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include "io/Stream.h"      // ENDGAME B.3.4 callers batch H: IO::Stream::GetDirection
#include <cstdint>
#include <cstdio>
#include <new>

// ============================================================
// Resolved helpers
// ============================================================

// FUN_14000da70 — resize paired (IBP+int) array
typedef void (*fn_ResizePairedArray_t)(void* arrayPtr, int newCount);
static fn_ResizePairedArray_t s_fn_ResizePairedArray = nullptr;

// FUN_140002340 — serialize IBP array entry
typedef void (*fn_SerializeIBPEntry_t)(void* entryPtr, void* serializer);
static fn_SerializeIBPEntry_t s_fn_SerializeIBPEntry = nullptr;

static bool s_initialized = false;

// ============================================================
// Init: resolve function pointers
// ============================================================
static void TrackerComponent_InitFuncs() {
    if (s_initialized) return;

    s_fn_ResizePairedArray = reinterpret_cast<fn_ResizePairedArray_t>(
        ResolveAddr(Module::Exe, 0x14000da70));
    s_fn_SerializeIBPEntry = reinterpret_cast<fn_SerializeIBPEntry_t>(
        ResolveAddr(Module::Exe, 0x140002340));

    s_initialized = true;
}

// ============================================================================
// ENDGAME B.3.8 VIS — TrackerComponent as a REAL class (TrackerComponent.h).
//
// Before: a 48-slot TrackerComponent_InertVtable filled per create, Inert_ScalarDtor
// in slot 0 (only ~Internal), +0x48 = -1 patched in after the zero-fill.
// After: the compiler's 16-slot vtable and a real ctor/dtor chain
// (B38_RULES D1, D2); slot bodies are the ones the old table installed.
// ============================================================================
#include "Object/factory_registration.h"

// The retail ctor (inlined in TrackerComponent_Factory 0x14000d7a0) writes
// +0x48 = -1. +0x48 is the "game-world item-count generation" stamp, and 0 is a
// LEGAL generation value: with 0 ProductionUI's per-frame update (0x140075a50)
// would compare gameWorld item count 0 == stamp 0 on the very first frame of a
// fresh world and SKIP the one-shot array rebuild the -1 sentinel forces.
TrackerComponent::TrackerComponent()
    : field28(nullptr), field30(nullptr), field38(nullptr), field40(nullptr),
      trackerId(-1), _pad4C{} {}

// [3] ============================================================
// TrackerComponent::Persist (vtable slot [3], 0x140059130)
// ============================================================
// Serialize two paired arrays of (InternalBasePtr, int) entries
// at +0x28/+0x30 and +0x38/+0x40, plus trackerId at +0x48.

void TrackerComponent::Persist(void* stream) {
    // resolver call added at top, save/load wave 2026-08-28: InitFuncs was declared
    // but never called anywhere, so every s_fn_* below stayed null and load silently
    // skipped this component's fields.
    TrackerComponent_InitFuncs();
    uintptr_t selfAddr = reinterpret_cast<uintptr_t>(this);

    Component::Persist(stream);   // retail: CALL 0x1401189b0 (qualified)

    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // === Array 1 at +0x28/+0x30 ===
    int count1 = *(int*)(selfAddr + 0x30);
    int localCount1 = count1;
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(localCount1, nullptr);   // B.3.4 bridge

    // If loading, resize array
    if (static_cast<int>(reinterpret_cast<IO::Stream*>(stream)->GetDirection()) == 0 && s_fn_ResizePairedArray) {   // B.3.4 bridge  (IO::Stream::GetDirection, retail 0x180006200)
        s_fn_ResizePairedArray((void*)(selfAddr + 0x28), localCount1);
    }

    // Serialize each (IBP + int) pair — each pair is 16 bytes (8 IBP + 4 int + 4 pad = 16)
    void* array1 = *(void**)(selfAddr + 0x28);
    if (localCount1 > 0) {
        uintptr_t entryPtr = (uintptr_t)array1;
        for (int i = 0; i < localCount1; i++) {
            if (s_fn_SerializeIBPEntry)
                s_fn_SerializeIBPEntry((void*)entryPtr, serializer);
            reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(entryPtr + 8), nullptr);   // B.3.4 bridge
            entryPtr += 16;  // sizeof(IBP) + sizeof(int) = 8 + 8
        }
    }

    // === Array 2 at +0x38/+0x40 ===
    int count2 = *(int*)(selfAddr + 0x40);
    int localCount2 = count2;
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(localCount2, nullptr);   // B.3.4 bridge

    if (static_cast<int>(reinterpret_cast<IO::Stream*>(stream)->GetDirection()) == 0 && s_fn_ResizePairedArray) {   // B.3.4 bridge  (IO::Stream::GetDirection, retail 0x180006200)
        s_fn_ResizePairedArray((void*)(selfAddr + 0x38), localCount2);
    }

    void* array2 = *(void**)(selfAddr + 0x38);
    if (localCount2 > 0) {
        uintptr_t entryPtr = (uintptr_t)array2;
        for (int i = 0; i < localCount2; i++) {
            if (s_fn_SerializeIBPEntry)
                s_fn_SerializeIBPEntry((void*)entryPtr, serializer);
            reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(entryPtr + 8), nullptr);   // B.3.4 bridge
            entryPtr += 16;
        }
    }

    // Serialize trackerId at +0x48
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(trackerId, nullptr);   // B.3.4 bridge

    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
}

// create 0x14000d7a0: MemAlloc(0x50, 8), then the ctor.
void* TrackerComponent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(TrackerComponent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) TrackerComponent());
}
// Retail's createInPlace is `Internal::CreateInPlaceObject` (returns null); the
// tree's in-place entry built the object, and that is kept (B38 B8-13).
void* TrackerComponent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) TrackerComponent());
}

DEFINE_EXE_TYPE(TrackerComponent, "Component")
