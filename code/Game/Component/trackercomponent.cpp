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
#include "object/Internal.h"
#include "object/TypeInfo.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include "io/Stream.h"      // ENDGAME B.3.4 callers batch H: IO::Stream::GetDirection
#include <cstdint>
#include <cstdio>
#include <new>
#include "Action/exe_bodies.h"   // ActRm_ArrayResize16_da70 (0x14000da70)
#include "Game/exe_bodies.h"     // Reimpl_ExternalPtr_Serialize_StringTable (0x140002340)

// Helpers: FUN_14000da70 ActRm_ArrayResize16_da70 — resize a paired (IBP+int) array;
// FUN_140002340 Reimpl_ExternalPtr_Serialize_StringTable — serialize an IBP entry.

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
    uintptr_t selfAddr = reinterpret_cast<uintptr_t>(this);

    Component::Persist(stream);   // retail: CALL 0x1401189b0 (qualified)

    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // === Array 1 at +0x28/+0x30 ===
    int count1 = *(int*)(selfAddr + 0x30);
    int localCount1 = count1;
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(localCount1, nullptr);   // B.3.4 bridge

    // If loading, resize array
    if (static_cast<int>(reinterpret_cast<IO::Stream*>(stream)->GetDirection()) == 0) {   // B.3.4 bridge  (IO::Stream::GetDirection, retail 0x180006200)
        ActRm_ArrayResize16_da70((int64_t*)(selfAddr + 0x28), localCount1);   // 0x14000da70
    }

    // Serialize each (IBP + int) pair — each pair is 16 bytes (8 IBP + 4 int + 4 pad = 16)
    void* array1 = *(void**)(selfAddr + 0x28);
    if (localCount1 > 0) {
        uintptr_t entryPtr = (uintptr_t)array1;
        for (int i = 0; i < localCount1; i++) {
            Reimpl_ExternalPtr_Serialize_StringTable((void*)entryPtr, serializer);   // 0x140002340
            reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(entryPtr + 8), nullptr);   // B.3.4 bridge
            entryPtr += 16;  // sizeof(IBP) + sizeof(int) = 8 + 8
        }
    }

    // === Array 2 at +0x38/+0x40 ===
    int count2 = *(int*)(selfAddr + 0x40);
    int localCount2 = count2;
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(localCount2, nullptr);   // B.3.4 bridge

    if (static_cast<int>(reinterpret_cast<IO::Stream*>(stream)->GetDirection()) == 0) {   // B.3.4 bridge  (IO::Stream::GetDirection, retail 0x180006200)
        ActRm_ArrayResize16_da70((int64_t*)(selfAddr + 0x38), localCount2);   // 0x14000da70
    }

    void* array2 = *(void**)(selfAddr + 0x38);
    if (localCount2 > 0) {
        uintptr_t entryPtr = (uintptr_t)array2;
        for (int i = 0; i < localCount2; i++) {
            Reimpl_ExternalPtr_Serialize_StringTable((void*)entryPtr, serializer);   // 0x140002340
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
