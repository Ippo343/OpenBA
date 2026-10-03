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
#include "Object/factory_registration.h"   // Object::GetTypeIdByName
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include "audio/Audio.h"     // ENDGAME B.3.7 callers (K4)
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
#include "Component/exe_bodies.h"
#include "system/MemoryAllocator.h"

#include "Component/AmbientEmitterComponent.h"
#include "audio/SoundEffect.h"

// ============================================================
// Type id for AmbientEmitterDescription (comp+0x18 building description).
// ============================================================
// Latch only once the factory registry has frozen the ids (they start at the
// 0xFFFFFFFF sentinel).
static uint32_t s_tidEmitterDesc = 0;
static bool s_tidResolved = false;
static void EnsureEmitterDescTid() {
    if (s_tidResolved) return;
    uint32_t t = AmbientEmitterDescription::TypeId();
    if (t != 0 && t != 0xFFFFFFFFu) { s_tidEmitterDesc = t; s_tidResolved = true; }
}

// ============================================================
// Helper: the component's AmbientEmitterDescription, or null
// ============================================================
static AmbientEmitterDescription* GetBuildingData(AmbientEmitterComponent* self) {
    EnsureEmitterDescTid();
    Description* obj = self->_description;
    if (!obj || !s_tidEmitterDesc)
        return nullptr;
    return ::Object::IsDerivedFromNullSafe(obj, s_tidEmitterDesc) ? static_cast<AmbientEmitterDescription*>(obj) : nullptr;
}

// The world Audio::Scene. LEFT: GameUpdateLoop::m_audioScene is void* (W0 header).
static Audio::Scene* WorldAudioScene() {
    if (GameUpdateLoop::_singleton == nullptr) return nullptr;
    return reinterpret_cast<Audio::Scene*>(GameUpdateLoop::_singleton->m_audioScene);
}

// ============================================================
// Reimpl of FUN_140001770 (grow/shrink the 0x10-byte-element emitter array)
// ============================================================
// Resizes the array {ptr@arr+0, count@arr+8, cap@arr+0xc} (stride 0x10). On grow
// past capacity: cap = newCount (or newCount*3 when growing by exactly one from a
// non-empty array), MemAlloc(cap*0x10, 8), copy existing elements (2 qwords each),
// zero-init the new tail with element layout [u32 0][f32 1.0][u64 0]. The exe body
// is hand-unrolled x4; this is the equivalent scalar 1:1 port.
// LEFT: a Collection::Array<AmbientEmitterSlot>::SetSize instance whose element
// default is {0, 1.0f, null}; the template's SetSize zero-fills, so it is not the
// template's body.
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
// For each emitter slot, ensures a 3D looping SoundEffect is playing at the
// building's position via the world Audio::Scene (gameWorld+0x78). The slot's
// sound is lazily copied from the building's AmbientEmitterDescription
// (type DAT_1401c3e98) sound array at desc+0x28. FindEffect3D reuses an existing
// channel; PlayEffect3D (SoundType 3 = ambient) starts a new one; AddEmitter
// registers this building as a spatial source of that effect.
void AmbientEmitterComponent::StartEmitters() {
    int count = slotCount;
    if (count <= 0) return;

    Audio::Scene* scene = WorldAudioScene();
    if (!scene) return;

    AmbientEmitterDescription* building = GetBuildingData(this);   // may be null
    Audio::SoundEffect** soundArray = building ? building->_emitters : nullptr;
    AmbientEmitterSlot* arrBase = slots;
    if (!arrBase) return;

    for (int i = 0; i < count; ++i) {
        AmbientEmitterSlot* entry = &arrBase[i];
        // Lazily bind the sound resource from the building description.
        if (entry->sound == nullptr && soundArray)
            entry->sound = soundArray[i];

        uint32_t handle = scene->FindEffect3D(entry->sound);
        entry->handle = handle;
        if (handle == 0) {
            const System::Time timeZero{0};       // System::Time(0)
            const Math::Vector3 posZero(0.0f, 0.0f, 0.0f);
            handle = scene->PlayEffect3D(entry->sound, static_cast<Audio::Scene::SoundType>(3),
                                         timeZero, posZero);
            entry->handle = handle;
        }
        scene->AddEmitter(handle, position, entry->volume);
    }
}

// ---- sub_2c60: (re)play the effect for one emitter slot and add its emitter. ----
void AmbientEmitterComponent::PlaySlot(int slot) {
    Audio::Scene* scene = WorldAudioScene();
    if (!scene) return;
    AmbientEmitterDescription* desc = GetBuildingData(this);
    AmbientEmitterSlot* arr = slots;
    if (!arr) return;
    AmbientEmitterSlot* entry = &arr[slot];

    if (entry->sound == nullptr && desc) {
        Audio::SoundEffect** sounds = desc->_emitters;
        if (sounds) entry->sound = sounds[slot];
    }

    uint32_t handle = scene->FindEffect3D(entry->sound);
    entry->handle = handle;
    if (handle == 0) {
        const System::Time timeZero{0};
        const Math::Vector3 posZero(0.0f, 0.0f, 0.0f);
        handle = scene->PlayEffect3D(entry->sound, static_cast<Audio::Scene::SoundType>(3),
                                     timeZero, posZero);
        entry->handle = handle;
    }
    scene->AddEmitter(handle, position, entry->volume);
}

// ---- sub_2a10: set slot `slot` of the emitter component to `sound` @ `vol`. ----
void AmbientEmitterComponent::SetSlot(int slot, Audio::SoundEffect* sound, float vol) {
    AmbientEmitterDescription* desc = GetBuildingData(this);
    // Ensure the emitter array matches the description's emitter count.
    if (desc) {
        int wantCount = desc->_emitterCount;
        if (slotCount != wantCount)
            AmbientEmitterResize_Reimpl(&slots, wantCount);
    }

    AmbientEmitterSlot* arr = slots;
    if (!arr) return;
    AmbientEmitterSlot* entry = &arr[slot];

    uint32_t handle = entry->handle;

    if (handle == 0) {
        // Record the sound reference + volume, then START playback.
        //
        // DEVIATION FROM RETAIL: the exe's sub_2a10 only records here (return
        // without playing) because retail initializes the emitter handles earlier
        // via the AmbientEmitterComponent's OnAdded (FUN_140042e10). In standalone
        // that component is inert (OnAdded is a no-op stub), so nothing ever seeds
        // the handle and sub_2a10's play branches (which require handle!=0) would
        // never fire. We seed it on-demand here: PlaySlot does FindEffect3D /
        // PlayEffect3D + AddEmitter for this slot's biome sound. Once handle!=0 the
        // subsequent ticks follow the faithful volume-crossfade / sound-swap paths.
        entry->sound = sound;
        entry->volume = vol;
        if (sound != nullptr) PlaySlot(slot);
        return;
    }

    if (entry->sound != sound) {
        // Sound changed — tear down the old emitter, then play the new one.
        Audio::Scene* scene = WorldAudioScene();
        if (scene) {
            scene->RemoveEmitter(handle, position);
            int cnt = scene->GetEmitterCount(handle);
            if (cnt == 0) scene->StopEffect3D(handle);
        }
        entry->handle = 0;
        entry->volume = vol;
        entry->sound = sound;
        PlaySlot(slot);
        return;
    }

    // Same sound, live handle — just adjust volume if it moved.
    if (entry->volume == vol) return;
    entry->volume = vol;
    Audio::Scene* scene = WorldAudioScene();
    if (scene)
        scene->SetEmitterVolume(handle, position, vol);
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

// 0x1400430c0. Component() is the CALL 0x1401188b0; the 0x140158a60 stamp is
// the compiler's; +0x28, +0x30 (dword), +0x38, +0x40 = 0; flags |= 0x43 (0xc3).
AmbientEmitterComponent::AmbientEmitterComponent()
    : position(), _pad34{}, slots(nullptr), slotCount(0), slotCapacity(0) {
    OrFlagsInCtor(0x43);
}

// [3] 0x140042f30
// ============================================================
// Serializes:
//   1. Base Component_Serialize
//   2. Serializer(version=1):
//      - Vector3 at +0x28 (position)
//      - count from +0x40, serialize it
//      - If loading: resize array at +0x38
//      - per slot: the sound ExternalPtr (+0x08), the handle uint (+0x00), the volume (+0x04)
//      - If version==0: resize from building data +0x30
//   3. ~Serializer
void AmbientEmitterComponent::Persist(IO::Stream& stream) {
    Component::Persist(stream);   // retail's direct CALL 0x1401189b0
    IO::Serializer serializer(stream, 1);

    position.Serialize(&serializer);

    int localCount = slotCount;
    serializer.Serialize(localCount, nullptr);

    // If loading (stream mode == 0), resize emitter array
    if (static_cast<int>(serializer.GetDirection()) == 0) {   // IO::Serializer::GetDirection, retail 0x180006210
        AmbientEmitterResize_Reimpl(&slots, localCount);   // 0x140001770
    }

    if (localCount > 0) {
        AmbientEmitterSlot* arrayBase = slots;
        for (int i = 0; i < localCount; i++) {
            AmbientExternalPtrSerialize_Reimpl(&arrayBase[i].sound, &serializer);   // 0x1400019a0
            serializer.Serialize(*reinterpret_cast<int*>(&arrayBase[i].handle), nullptr);   // retail's int door for the uint
            serializer.Serialize(arrayBase[i].volume, nullptr);
        }
    }

    // Version-gated: if version==0, resize from building data
    int version = serializer.GetVersion();
    if (version == 0) {
        AmbientEmitterDescription* building = GetBuildingData(this);
        if (building) {
            AmbientEmitterResize_Reimpl(&slots, building->_emitterCount);   // 0x140001770
        }
    }
}

// [6] 0x140042ba0 — dispatched by ComponentEntity_OnAddedPass at register time
// (flags bit 0): size the emitter array and start the building's ambient sounds.
// Resizes emitter list from building[0x30]; if building[0x20] != 0, calls
// StartEmitters (FUN_140042e10).
void AmbientEmitterComponent::OnAdded() {
    AmbientEmitterDescription* building = GetBuildingData(this);
    if (!building) return;

    AmbientEmitterResize_Reimpl(&slots, building->_emitterCount);

    if (building->_enabled) {
        StartEmitters();
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
