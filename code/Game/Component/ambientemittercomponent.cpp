// code/Game/Component/ambientemittercomponent.cpp
// Clean reimplementation of AmbientEmitterComponent lifecycle methods.
//
// AmbientEmitterComponent (0x48 bytes) manages ambient sound emitters for entities.
// Fields: +0x28=emitter data ptr, +0x30=count, +0x38=emitter array ptr, +0x40=count2
//
// Vtable at 0x140158a60. Non-trivial overrides:
//   Shutdown (0x140042f30) -- Complex serialization with Vector3, emitter entries
//   Draw     (0x140042ba0) -- Resizes emitter list, calls audio setup
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Component/ambientemittercomponent_funcs.h"
#include "Component/AmbientEmitterDescription.h"   // B.3.8 CB1a (TypeId)
#include "system/Value.h"
#include "object/Internal.h"
#include "object/TypeInfo.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "math/Math.h"
#include "resolve.h"
#include "Object/factory_registration.h"   // Object::GetTypeIdByName
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include "audio/Audio.h"     // ENDGAME B.3.7 callers (K4)
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
#include "Component/exe_bodies.h"
#include "system/MemoryAllocator.h"

// FUN_140042e10(self) -- building ambient audio setup (clean reimpl below).
extern "C" void AmbientEmitterAudioSetup_Reimpl(void* self);

// ============================================================
// Type id for AmbientEmitterDescription (comp+0x18 building description).
// ============================================================
// The .rdata type-id global DAT_1401c3e98 is NOT resolvable via
// ResolveAddr(Module::Exe, .rdata) (it hits a not-implemented stub — see
// reference_standalone_rdata_null_fallback), so look the id up by name via the
// factory registry (the same CRC32 fn_IsDerivedFrom compares against). Latch only
// once the factory registry has frozen the ids (they start at the 0xFFFFFFFF sentinel).
static uint32_t s_tidEmitterDesc = 0;
static bool s_tidResolved = false;
static void EnsureEmitterDescTid() {
    if (s_tidResolved) return;
    uint32_t t = AmbientEmitterDescription::TypeId();
    if (t != 0 && t != 0xFFFFFFFFu) { s_tidEmitterDesc = t; s_tidResolved = true; }
}

// ============================================================
// Helper: Get building description component
// ============================================================
// Uses the shared fn_IsDerivedFrom engine func (resolved via engine_funcs) rather
// than the .rdata dispatch slot, so the building-emitter path works in standalone.
static void* GetBuildingData(void* self) {
    EnsureEmitterDescTid();
    void* obj = *(void**)((uintptr_t)self + 0x18);
    if (!obj || !s_tidEmitterDesc)
        return nullptr;
    return ::Object::IsDerivedFromNullSafe(obj, s_tidEmitterDesc) ? obj : nullptr;
}

// ============================================================
// Reimpl of FUN_140001770 (grow/shrink the 0x10-byte-element emitter array)
// ============================================================
// Resizes the array {ptr@arr+0, count@arr+8, cap@arr+0xc} (stride 0x10). On grow
// past capacity: cap = newCount (or newCount*3 when growing by exactly one from a
// non-empty array), MemAlloc(cap*0x10, 8), copy existing elements (2 qwords each),
// zero-init the new tail with element layout [u32 0][f32 1.0][u64 0]. The exe body
// is hand-unrolled x4; this is the equivalent scalar 1:1 port.
extern "C" void AmbientEmitterResize_Reimpl(void* arrayBase, int newCount) {
    uint8_t* arr = reinterpret_cast<uint8_t*>(arrayBase);
    int oldCount = *(int*)(arr + 8);
    if (newCount == oldCount) return;

    void* oldPtr = *(void**)(arr + 0);
    *(int*)(arr + 8) = newCount;
    if (*(int*)(arr + 0xc) < newCount) {
        *(int*)(arr + 0xc) = newCount;
        if (newCount == oldCount + 1 && oldCount != 0) {
            *(int*)(arr + 0xc) = newCount * 3;
        }
        if (::System::MemoryAllocator::_singleton) {
            void* newPtr = ::System::MemoryAllocator::MemAlloc((uint64_t)(*(int*)(arr + 0xc)) << 4, 8);
            *(void**)(arr + 0) = newPtr;
        }
    }

    int start = 0;
    if (oldPtr == *(void**)(arr + 0)) start = oldCount;  // grown in place

    uint8_t* base = *(uint8_t**)(arr + 0);
    for (int i = start; i < newCount; ++i) {
        uint8_t* dst = base + (intptr_t)i * 0x10;
        if (!dst) continue;
        if (i < oldCount) {
            const uint8_t* src = reinterpret_cast<const uint8_t*>(oldPtr) + (intptr_t)i * 0x10;
            *(uint64_t*)(dst + 0) = *(const uint64_t*)(src + 0);
            *(uint64_t*)(dst + 8) = *(const uint64_t*)(src + 8);
        } else {
            *(uint32_t*)(dst + 0) = 0;
            *(uint32_t*)(dst + 4) = 0x3f800000;   // 1.0f
            *(uint64_t*)(dst + 8) = 0;
        }
    }

    if (oldPtr != *(void**)(arr + 0) && oldPtr) {
        ::System::MemoryAllocator::MemFree(oldPtr);
    }
}

// ============================================================
// Building ambient audio setup — clean reimpl of FUN_140042e10.
// ============================================================
// For each emitter entry (count at self+0x40, 0x10-byte entries in the array at
// self+0x38), ensures a 3D looping SoundEffect is playing at the building's
// position (self+0x28) via the world Audio::Scene (gameWorld+0x78):
//   entry {+0x00 uint handle, +0x04 float volume, +0x08 ExternalPtr soundRef}
// The soundRef is lazily copied from the building's AmbientEmitterDescription
// (self+0x18, type DAT_1401c3e98) sound array at desc+0x28 (stride 8). FindEffect3D
// reuses an existing channel; PlayEffect3D (SoundType 3 = ambient) starts a new
// one; AddEmitter registers this building as a spatial source of that effect.
extern "C" void AmbientEmitterAudioSetup_Reimpl(void* self) {
    if (!self) return;
    uintptr_t s = reinterpret_cast<uintptr_t>(self);
    int count = *(int*)(s + 0x40);
    if (count <= 0) return;
    if (GameUpdateLoop::_singleton == nullptr) return;

    void* scene = *(void**)((uintptr_t)GameUpdateLoop::_singleton + 0x78);
    if (!scene) return;

    void* building = GetBuildingData(self);   // AmbientEmitterDescription (may be null)
    void* soundArray = building ? *(void**)((uintptr_t)building + 0x28) : nullptr;
    uint8_t* arrBase = *(uint8_t**)(s + 0x38);
    if (!arrBase) return;

    const void* pos = reinterpret_cast<const void*>(s + 0x28);   // Vector3 position

    for (int i = 0; i < count; ++i) {
        uint8_t* entry = arrBase + (intptr_t)i * 0x10;
        uint64_t* soundRef = reinterpret_cast<uint64_t*>(entry + 8);
        // Lazily bind the sound resource from the building description.
        if (*soundRef == 0 && soundArray)
            *soundRef = reinterpret_cast<const uint64_t*>(soundArray)[i];

        uint32_t handle = reinterpret_cast<Audio::Scene*>(scene)->FindEffect3D(
            reinterpret_cast<Audio::SoundEffect*>(*soundRef));   // B.3.7 bridge
        *(uint32_t*)(entry + 0) = handle;
        if (handle == 0) {
            int64_t timeZero = 0;                 // System::Time(0)
            float   posZero[3] = { 0.0f, 0.0f, 0.0f };
            handle = reinterpret_cast<Audio::Scene*>(scene)->PlayEffect3D(
                reinterpret_cast<Audio::SoundEffect*>(*soundRef), static_cast<Audio::Scene::SoundType>(3),
                *reinterpret_cast<const System::Time*>(&timeZero),
                *reinterpret_cast<const Vector3*>(posZero));   // B.3.7 bridge
            *(uint32_t*)(entry + 0) = handle;
        }
        reinterpret_cast<Audio::Scene*>(scene)->AddEmitter(
            handle, *reinterpret_cast<const Vector3*>(pos), *(float*)(entry + 4));   // B.3.7 bridge
    }
}

// ============================================================================
// ENDGAME B.3.8 CIT — AmbientEmitterComponent as a REAL class
// (AmbientEmitterComponent.h).
//
// Before: COMPONENT_LIVE_FACTORY_ONADDED + COMPONENT_INSTALL_SERIALIZE — a
// 48-slot hand-filled table (slot 3 the serializer, slot 6 OnAdded) with
// Inert_ScalarDtor in slot 0. After: the compiler's 16-slot vtable and a real
// ctor/dtor chain (B38_RULES D1, D2).
// ============================================================================
#include "Object/factory_registration.h"
#include "Component/AmbientEmitterComponent.h"

// 0x1400430c0. Component() is the CALL 0x1401188b0; the 0x140158a60 stamp is
// the compiler's; +0x28, +0x30 (dword), +0x38, +0x40 = 0; flags |= 0x43 (0xc3).
AmbientEmitterComponent::AmbientEmitterComponent()
    : field28(nullptr), field30(0), _pad34{}, field38(nullptr), field40(nullptr) {
    OrFlagsInCtor(0x43);
}

// [3] 0x140042f30 — the body resolves its own helpers first.
// ============================================================
// AmbientEmitterComponent::Shutdown (vtable slot [3], 0x140042f30)
// ============================================================
// Serializes:
//   1. Base Component_Serialize
//   2. Serializer(version=1):
//      - Vector3 at +0x28 (position)
//      - count from +0x40, serialize it
//      - If loading: resize array at +0x38
//      - Loop: serialize (InternalBasePtr + uint + float) per entry (16 bytes each)
//      - If version==0: resize from building data +0x30
//   3. ~Serializer
void AmbientEmitterComponent::Persist(void* stream) {
    Component::Persist(stream);   // retail's direct CALL 0x1401189b0
    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 1);   // B.3.4 bridge

    uintptr_t selfAddr = (uintptr_t)this;

    // Serialize Vector3 at +0x28
    reinterpret_cast<Math::Vector3*>(selfAddr + 0x28)->Serialize(serializer);   // B.3.1 bridge

    // Read count from +0x40 into local, serialize it
    uint32_t localCount = *(uint32_t*)(selfAddr + 0x40);
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(&localCount), nullptr);   // B.3.4 bridge

    // If loading (stream mode == 0), resize emitter array
    if (static_cast<int>(reinterpret_cast<IO::Serializer*>(serializer)->GetDirection()) == 0) {   // B.3.4 bridge  (IO::Serializer::GetDirection, retail 0x180006210)
        AmbientEmitterResize_Reimpl((void*)(selfAddr + 0x38), (int)localCount);   // 0x140001770
    }

    // Serialize each emitter entry: InternalBasePtr(8) + uint(4) + float(4) = 16 bytes per entry
    // Array base at +0x38 (ptr). Entries: float* pfVar4 starts at array+4
    if ((int)localCount > 0) {
        void* arrayBase = *(void**)(selfAddr + 0x38);
        // Each entry is 16 bytes. Loop pointer starts at +4 (the float field).
        // Layout per entry: [uint(4)][InternalBasePtr(8, at +4)][float(4, at +12)]...
        // Actually from decomp: pfVar4 starts at array+4, pfVar4+1 = IBP, pfVar4-1 = uint, pfVar4 = float
        // pfVar4 = (float*)(array + 4), stride = 4 floats = 16 bytes
        float* pfVar4 = (float*)((uintptr_t)arrayBase + 4);
        for (uint32_t i = 0; i < localCount; i++) {
            // Serialize InternalBasePtr at pfVar4+1 (offset +4 from pfVar4)
            AmbientExternalPtrSerialize_Reimpl((void*)(pfVar4 + 1), serializer);   // 0x1400019a0
            // Serialize uint at pfVar4-1 (offset -4 from pfVar4)
            reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(pfVar4 - 1), nullptr);   // B.3.4 bridge
            // Serialize float at pfVar4
            reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*pfVar4, nullptr);   // B.3.4 bridge
            pfVar4 += 4; // advance 16 bytes
        }
    }

    // Version-gated: if version==0, resize from building data
    int version = reinterpret_cast<IO::Serializer*>(serializer)->GetVersion();   // B.3.4 bridge
    if (version == 0) {
        void* building = GetBuildingData(this);
        if (building) {
            int buildingCount = *(int*)((uintptr_t)building + 0x30);
            AmbientEmitterResize_Reimpl((void*)(selfAddr + 0x38), buildingCount);   // 0x140001770
        }
    }

    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
}

// [6] 0x140042ba0 — dispatched by ComponentEntity_OnAddedPass at register time
// (flags bit 0): size the emitter array and start the building's ambient sounds.
// ============================================================
// AmbientEmitterComponent::Draw (vtable slot [6], 0x140042ba0)
// ============================================================
// Gets building data via IsDerivedFrom(+0x18, DAT_1401c3e98).
// Resizes emitter list from building[0x30].
// If building[0x20] != 0, calls audio setup FUN_140042e10(self).
void AmbientEmitterComponent::OnAdded() {
    void* building = GetBuildingData(this);
    if (!building) return;

    uintptr_t selfAddr = (uintptr_t)this;

    // Resize emitter list to the building description's emitter count. Use the
    // ported AmbientEmitterResize_Reimpl (not the resolved FUN_140001770) so this
    // works in standalone, where 0x140001770 is not in the address registry.
    int count = *(int*)((uintptr_t)building + 0x30);
    AmbientEmitterResize_Reimpl((void*)(selfAddr + 0x38), count);

    // If building has audio flag at +0x20, set up the 3D ambient emitters.
    bool audioFlag = *(char*)((uintptr_t)building + 0x20) != '\0';

    if (audioFlag) {
        AmbientEmitterAudioSetup_Reimpl(this);
    }
}

// [7] 0x140042b90 (`JMP 0x140042d70`). PORT GAP (retail 0x140042b90) — B38 D9:
// not ported; the tree's table held the inert stub here, kept.
void AmbientEmitterComponent::OnRemove() {}

// create 0x1400014b0: MemAlloc(0x48, 8), then the ctor.
void* AmbientEmitterComponent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(AmbientEmitterComponent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) AmbientEmitterComponent());
}
// Retail's createInPlace is `Internal::CreateInPlaceObject` (returns null); the
// tree's in-place entry built the object, and that is kept (B38 B8-13).
void* AmbientEmitterComponent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) AmbientEmitterComponent());
}

DEFINE_EXE_TYPE(AmbientEmitterComponent, "Component")
