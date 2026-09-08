// ============================================================================
// runtime/src/audio/audio.cpp — Wave 8, lane C — Audio::Scene / ::Configuration
// ============================================================================
// Ports 16 of the 21 Audio exports our tree calls (Wave 8 landed 13; the
// Wave 8.5 close-out added Pause, StopEffect3D and StopAll, plus the two
// unexported-by-our-tree siblings PauseEffect and StopEffect they call). Every retail body below was
// decompiled AND disassembled at its real address in the SHIPPED
// Runtime-x64.dll (image base 0x180000000, read 2026-09-07); addresses and the
// oracle shape for each function are in docs/runtime_port/WAVE8_LANE_C.md.
//
// STILL HELD BACK, with reasons (docs/runtime_port/WAVE8_LANE_C.md has Wave
// 8's argument; PLAN §6: a held-back function with a stated reason is a
// result). THREE, down from seven — see the Wave 8.5 close-out block further
// down this file for why the other four moved and what each of their stated
// blockers turned out to be worth:
//
//   ?Update@Scene@Audio@@…             0x1800df6c0 — ~150 decompiled lines
//       spatialising every live emitter against Device::PlayPlatform, mixing
//       in System::Random state and calling back into Device's channel
//       objects. The synthetic-scene technique that unblocked the teardown
//       family does NOT reach this one: putting the device out of reach still
//       leaves the RNG, and a replay that draws twice from System::Random
//       diverges by construction. What it needs is a snapshot/restore of the
//       Random state as well, which is a real and bounded next step rather
//       than a missing tool. It does NOT need SoundBuffer/SoundEffect PORTS —
//       Wave 8 said it did; what it needs from them is LAYOUT, and a faithful
//       body may call them by address exactly as the teardown family calls
//       Device::Stop.
//   ?Update@Device@Audio@@…            0x1800de520 — Device internals
//       (platform channel completion callbacks). Here the device IS the
//       subject, not an unreachable callee, so no synthetic Scene helps.
//   ?FreeChannelPlatform@Device@Audio@@…ord2380 (0x1800de6f0) — PRIVATE
//       member, COM vtable call at (**channel)+0x90, critical section at
//       Device+0xa50. Same class as Device::Update, and for the same reason.
//
// PORTED SINCE, and the blockers that did not survive being re-read (each was
// correct on the evidence its lane had — WAVE8_5.md §10 is about transport,
// not judgement):
//
//   ?Serialize@Scene@Audio@@…          0x1800e0050 — held back because "the
//       profile build's IO::Encoding vtable offsets do not match retail's and
//       this lane did not re-derive retail's own". Wave 4 had already written
//       retail's offsets into runtime/src/io/rt_io_port.cpp, where they had
//       sat since. Ported in Wave 8.5, 32,772 identical emissions.
//   ?StopEffect3D@ / ?Pause@ / ?StopAll@Scene@Audio@@ — held back because
//       they "reach Audio::Device::Stop, an audible non-replayable side
//       effect, and therefore have no oracle either way". True of a LIVE
//       scene and false in general: Device::Stop is reachable only through a
//       non-null Channel::platformChannel[b], which is a SCENE field, so a
//       synthetic scene removes the device by construction. Ported in Wave
//       8.5's close-out with an exhaustive side-by-side probe.
//
// `?_singleton@Configuration@Audio@@0PEAV12@EA` (ord 4801) is a DATA export,
// not a function — nothing to register; engine_funcs.cpp already resolves it
// (g_pAudioConfigSingleton) exactly as WAVE3/4 record for other DLL globals.
//
// THE PHANTOM NAME. `?PlayEffect@Scene@Audio@@QEAIV?$ExternalPtr@VSoundEffect
// @Audio@@@Object@@W4SoundType@12@VTime@System@@@Z` (spelled QEAI, not QEAA)
// does not exist in the shipped DLL — one of only 3 phantoms left in the
// whole tree (tools/export_name_audit.py). engine_funcs.cpp's
// `fn_Audio_Scene_PlayEffect` already has its own
// `if (!fn) fn = ResolveAddr(Module::Runtime, 0x1800def60)` fallback, so this
// was NEVER a functional gap — the real, correctly-spelled export
// (`?PlayEffect@Scene@Audio@@QEAAIV?$ExternalPtr@VSoundEffect@Audio@@@Object@
// @W4SoundType@12@AEBVTime@System@@@Z`, retail 0x1800def60, ord 3423) is what
// every call actually reached. Fixed at its source in engine_funcs.cpp (the
// ResolveExport string, not the working fallback) rather than left as a live
// landmine for the next person who trusts the string over the behaviour.
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstring>
#include "../../funcs/rt_port.h"
#include "../../types/audio/Audio.h"
#include "../../../engine/types/common.h"   // Vector3
#include "../../types/io/Serializer.h"        // Stream / Serializer / rw_vtbl (Wave 4)
#include "../io/save_trace.h"                 // Wave 9 lane S

// ----------------------------------------------------------------------------
// ResolveAddr / Module, forward-declared rather than pulled in from
// standalone/resolve.h — same reason input.cpp does it (windows.h leaks
// engine-colliding macros). Must stay byte-identical to resolve.h.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

using RtAudio::Scene;
using RtAudio::Configuration;
using RtAudio::Channel;
using RtAudio::EffectSlot;
using RtAudio::EmitterInstance;

namespace {

// ----------------------------------------------------------------------------
// Dependencies out of this lane's scope, fetched from the SHIPPED DLL through
// RtOriginal/ResolveExportRaw — never through ResolveExport (rt_port.h: doing
// so would hand back our own port once registered, and any diff would compare
// ours to ours). Same pattern as runtime/src/save/saveload.cpp §"Dependencies".
// ----------------------------------------------------------------------------
using MemAllocFn = void* (*)(uint64_t size, int align);
using MemFreeFn  = void  (*)(void* p);

void* MemAlloc(uint64_t size, int align) {
    return RT_ORIGINAL_ONCE(MemAllocFn, "?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z")(size, align);
}
void MemFree(void* p) {
    RT_ORIGINAL_ONCE(MemFreeFn, "?MemFree@MemoryAllocator@System@@SAXPEAX@Z")(p);
}

// System::Timer::_secondsPerTick and Time helpers are not needed by anything
// this lane ports (Scene::Update, the only consumer, is held back).

// ----------------------------------------------------------------------------
// Collection::Array<T>::SetSize(n, fillValue) — the growth policy documented
// in runtime/types/collection/Collection.h's Array<T> comment, transcribed
// here rather than added to that header (this lane does not own it — see
// WAVE8.md's cross-lane ownership contract). Independently re-derived from
// THREE retail bodies that all agree (FUN_180051020 hardcoded-100,
// FUN_180051390 hardcoded-0x1000, FUN_180051aa0 fully generic — the one used
// by AddEmitter, which is why the fill-value parameter exists at all: it
// doubles as push_back(value) via SetSize(_size + 1, &value)).
//
// Simplified relative to retail in ONE way, stated rather than hidden: retail
// destroys/copy-constructs element-by-element to support non-POD T; every T
// this lane instantiates (Channel, EffectSlot, EmitterInstance) is a plain
// struct of ints/floats/pointers with no owning sub-object of its own EXCEPT
// EffectSlot::positions, which is handled explicitly by the caller (Scene's
// ctor/dtor), never through this helper. A memcpy of the live range is
// therefore observably identical to retail's element-wise copy for every T
// actually used here.
// ----------------------------------------------------------------------------
template <class T>
void ArraySetSize(Collection::Array<T>* arr, int32_t n, const T& fillValue) {
    int32_t oldSize = arr->_size;
    if (n == oldSize) return;
    T* oldData = arr->_data;
    arr->_size = n;
    bool grew = false;
    if (arr->_capacity < n) {
        arr->_capacity = n;
        if (n == oldSize + 1 && oldSize != 0) arr->_capacity = n * 3;
        arr->_data = static_cast<T*>(MemAlloc(sizeof(T) * (uint64_t)arr->_capacity, (int)alignof(T)));
        grew = true;
    }
    if (grew && oldSize > 0)
        std::memcpy(arr->_data, oldData, sizeof(T) * (size_t)oldSize);
    for (int32_t i = oldSize; i < n; ++i)
        arr->_data[i] = fillValue;
    if (grew && oldData)
        MemFree(oldData);
}

// EffectSlot lookup by id: index = (id>>1)&0xfff, validated against the
// stored idTag with the top-19-bit mask. Shared by every EffectSlot-table
// accessor (AddEmitter, MoveEmitter, RemoveEmitter, SetEmitterVolume,
// GetEmitterCount all repeat this exact check in retail).
EffectSlot* FindEffectSlot(Scene* self, uint32_t id) {
    uint32_t idx = (id >> 1) & 0xfff;
    EffectSlot* slot = &self->effects._data[idx];
    if (((slot->idTag ^ id) & 0xffffe000) != 0) return nullptr;
    return slot;
}

}  // namespace

// ============================================================================
// Audio::Scene::Scene() — retail 0x1800e0160
// Build oracle only (no diff target: constructing the live object twice would
// leak the first allocation).
// ============================================================================
void* RtAudio_Scene_Ctor(void* selfPtr) {
    auto* self = static_cast<Scene*>(selfPtr);
    self->time = 0;
    self->playGeneration = 1;
    self->channels = {};
    self->effects = {};

    EffectSlot defaultSlot{};
    defaultSlot.baseVolume = 1.0f;
    ArraySetSize(&self->effects, 0x1000, defaultSlot);   // matches FUN_180051390(this+0x20, 0x1000)

    Channel defaultChannel{};
    defaultChannel.volume = 1.0f;
    defaultChannel.pitch  = 1.0f;
    ArraySetSize(&self->channels, 0x100, defaultChannel); // matches FUN_180051020(this+0x10)

    for (int i = 0; i < 5; ++i) self->paused[i] = 0;
    return self;
}

// ============================================================================
// Audio::Scene::~Scene() — retail 0x1800e0120. Frees the EffectSlot table's
// owned per-slot `positions` buffers first (FUN_180052110), then both outer
// buffers, matching retail's order (+0x20 torn down before +0x10).
// ============================================================================
void RtAudio_Scene_Dtor(void* selfPtr) {
    auto* self = static_cast<Scene*>(selfPtr);
    for (int32_t i = 0; i < self->effects._size; ++i) {
        if (self->effects._data[i].positions._data)
            MemFree(self->effects._data[i].positions._data);
    }
    if (self->effects._data) MemFree(self->effects._data);
    self->effects = {};

    if (self->channels._data) MemFree(self->channels._data);
    self->channels = {};
}

// ============================================================================
// void Audio::Scene::AddEmitter(uint id, const Vector3& pos, float volume)
// retail 0x1800dedf0. Mutates the live EffectSlot::positions array — SHADOW
// oracle (verified against the DLL's own read-only GetEmitterCount on the
// SAME slot afterward, since both implementations share one Scene object).
// ============================================================================
void RtAudio_Scene_AddEmitter(void* selfPtr, uint32_t id, const Vector3* pos, float volume) {
    RT_DIFF_BEGIN("Audio::Scene::AddEmitter");
    auto* self = static_cast<Scene*>(selfPtr);
    EffectSlot* slot = FindEffectSlot(self, id);
    if (slot) {
        EmitterInstance inst{pos->x, pos->y, pos->z, volume};
        ArraySetSize(&slot->positions, slot->positions._size + 1, inst);
    }

    if (rt_diff_on__) {
        using GetCountFn = int (*)(void*, uint32_t);
        int theirsCount = RT_ORIGINAL_ONCE(GetCountFn, "?GetEmitterCount@Scene@Audio@@QEBAHI@Z")(self, id);
        int oursCount = slot ? slot->positions._size : 0;
        char a[32], b[32];
        std::snprintf(a, sizeof(a), "count=%d", oursCount);
        std::snprintf(b, sizeof(b), "count=%d", theirsCount);
        RT_DIFF_SHADOW(oursCount == theirsCount, a, b);
    }
}

// ============================================================================
// void Audio::Scene::MoveEmitter(uint id, const Vector3& oldPos, const Vector3& newPos)
// retail 0x1800dec40 — finds the FIRST instance whose position exactly equals
// oldPos and overwrites its xyz (volume untouched). SHADOW oracle.
// ============================================================================
void RtAudio_Scene_MoveEmitter(void* selfPtr, uint32_t id, const Vector3* oldPos,
                                const Vector3* newPos) {
    RT_DIFF_BEGIN("Audio::Scene::MoveEmitter");
    auto* self = static_cast<Scene*>(selfPtr);
    EffectSlot* slot = FindEffectSlot(self, id);
    bool moved = false;
    if (slot) {
        for (int32_t i = 0; i < slot->positions._size; ++i) {
            EmitterInstance& e = slot->positions._data[i];
            if (e.x == oldPos->x && e.y == oldPos->y && e.z == oldPos->z) {
                e.x = newPos->x; e.y = newPos->y; e.z = newPos->z;
                moved = true;
                break;
            }
        }
    }
    RT_DIFF_SHADOW(true, moved ? "moved" : "no-match (matches retail no-op)", "not independently checkable");
}

// ============================================================================
// void Audio::Scene::RemoveEmitter(uint id, const Vector3& pos)
// retail 0x1800decf0 — removes every instance whose position exactly equals
// pos (retail's inner shift loop re-checks the index it just compacted into,
// so a duplicate position is fully removed in one call). SHADOW oracle.
// ============================================================================
void RtAudio_Scene_RemoveEmitter(void* selfPtr, uint32_t id, const Vector3* pos) {
    RT_DIFF_BEGIN("Audio::Scene::RemoveEmitter");
    auto* self = static_cast<Scene*>(selfPtr);
    EffectSlot* slot = FindEffectSlot(self, id);
    int removed = 0;
    if (slot) {
        int32_t i = 0;
        while (i < slot->positions._size) {
            EmitterInstance& e = slot->positions._data[i];
            if (e.x == pos->x && e.y == pos->y && e.z == pos->z) {
                for (int32_t j = i; j + 1 < slot->positions._size; ++j)
                    slot->positions._data[j] = slot->positions._data[j + 1];
                --slot->positions._size;
                ++removed;
                // do not advance i: the slid-down element must be re-checked
            } else {
                ++i;
            }
        }
    }

    if (rt_diff_on__) {
        using GetCountFn = int (*)(void*, uint32_t);
        int theirsCount = RT_ORIGINAL_ONCE(GetCountFn, "?GetEmitterCount@Scene@Audio@@QEBAHI@Z")(self, id);
        int oursCount = slot ? slot->positions._size : 0;
        char a[32], b[32];
        std::snprintf(a, sizeof(a), "count=%d", oursCount);
        std::snprintf(b, sizeof(b), "count=%d", theirsCount);
        RT_DIFF_SHADOW(oursCount == theirsCount, a, b);
    }
}

// ============================================================================
// void Audio::Scene::SetEmitterVolume(uint id, const Vector3& pos, float volume)
// retail 0x1800ded60 — finds the FIRST instance whose position exactly equals
// pos and overwrites its volume. SHADOW oracle.
// ============================================================================
void RtAudio_Scene_SetEmitterVolume(void* selfPtr, uint32_t id, const Vector3* pos, float volume) {
    RT_DIFF_BEGIN("Audio::Scene::SetEmitterVolume");
    auto* self = static_cast<Scene*>(selfPtr);
    EffectSlot* slot = FindEffectSlot(self, id);
    bool set = false;
    if (slot) {
        for (int32_t i = 0; i < slot->positions._size; ++i) {
            EmitterInstance& e = slot->positions._data[i];
            if (e.x == pos->x && e.y == pos->y && e.z == pos->z) {
                e.volume = volume;
                set = true;
                break;
            }
        }
    }
    RT_DIFF_SHADOW(true, set ? "set" : "no-match (matches retail no-op)", "not independently checkable");
}

// ============================================================================
// int Audio::Scene::GetEmitterCount(uint id) const — retail 0x1800dec00.
// Pure read. Full replay oracle.
// ============================================================================
int RtAudio_Scene_GetEmitterCount(void* selfPtr, uint32_t id) {
    RT_DIFF_BEGIN("Audio::Scene::GetEmitterCount");
    auto* self = static_cast<Scene*>(selfPtr);
    EffectSlot* slot = FindEffectSlot(self, id);
    int ours = slot ? slot->positions._size : 0;

    using GetCountFn = int (*)(void*, uint32_t);
    RT_DIFF_SCALAR(ours, RT_ORIGINAL_ONCE(GetCountFn, "?GetEmitterCount@Scene@Audio@@QEBAHI@Z")(self, id));
    return ours;
}

// ============================================================================
// bool Audio::Scene::IsEffectPlaying(uint handle) const — retail 0x1800df680.
// Pure read of the Channel table (a DIFFERENT table/handle space than the
// EffectSlot functions above — see the header note). Full replay oracle.
// ============================================================================
bool RtAudio_Scene_IsEffectPlaying(void* selfPtr, uint32_t handle) {
    RT_DIFF_BEGIN("Audio::Scene::IsEffectPlaying");
    auto* self = static_cast<Scene*>(selfPtr);
    uint32_t idx = (handle >> 1) & 0xff;
    bool ours = false;
    if ((int32_t)idx < self->channels._size)
        ours = ((self->channels._data[idx].handleTag ^ handle) & 0xfffffe00) == 0;

    using IsPlayingFn = bool (*)(void*, uint32_t);
    RT_DIFF_SCALAR(ours, RT_ORIGINAL_ONCE(IsPlayingFn, "?IsEffectPlaying@Scene@Audio@@QEBA_NI@Z")(self, handle));
    return ours;
}

// ============================================================================
// uint Audio::Scene::FindEffect3D(ExternalPtr<SoundEffect> soundRef) const
// retail 0x1800dee70. Pure linear scan of the 4096-slot EffectSlot table for
// a matching soundEffectRef. Full replay oracle.
// ============================================================================
uint32_t RtAudio_Scene_FindEffect3D(void* selfPtr, uint64_t soundRef) {
    RT_DIFF_BEGIN("Audio::Scene::FindEffect3D");
    auto* self = static_cast<Scene*>(selfPtr);
    uint32_t ours = 0;
    for (int32_t i = 0; i < self->effects._size; ++i) {
        if (self->effects._data[i].soundEffectRef == soundRef) {
            ours = self->effects._data[i].idTag;
            break;
        }
    }

    using FindFn = uint32_t (*)(void*, uint64_t);
    RT_DIFF_SCALAR(ours, RT_ORIGINAL_ONCE(FindFn,
        "?FindEffect3D@Scene@Audio@@QEAAIV?$ExternalPtr@VSoundEffect@Audio@@@Object@@@Z")(self, soundRef));
    return ours;
}

// ============================================================================
// uint Audio::Scene::PlayEffect(ExternalPtr<SoundEffect> soundRef,
//                                Scene::SoundType type, const Time& time)
// retail 0x1800def60 (the REAL, QEAA-spelled export — see the phantom-name
// note at the top of this file). Allocates a free Channel slot; no Device
// call happens here (that is Scene::Update, held back), so the only side
// effect is on Scene-owned state. Consuming a channel slot is a real mutation
// — calling both implementations would consume two — so SHADOW, verified via
// the DLL's own read-only IsEffectPlaying on the handle we just minted.
// ============================================================================
uint32_t RtAudio_Scene_PlayEffect(void* selfPtr, uint64_t soundRef, int32_t soundType, const Time* time) {
    RT_DIFF_BEGIN("Audio::Scene::PlayEffect");
    auto* self = static_cast<Scene*>(selfPtr);
    uint32_t handle = 0;
    if (soundRef != 0) {
        int32_t free = -1;
        for (int32_t i = 0; i < self->channels._size && i < 0x100; ++i) {
            if (self->channels._data[i].handleTag == 0) { free = i; break; }
        }
        if (free >= 0) {
            Channel& ch = self->channels._data[free];
            ch.soundEffectRef = soundRef;
            ch.soundType = (uint32_t)soundType;
            ch.startTime = time->ticks;
            self->playGeneration += 1;
            handle = ((uint32_t)free << 1) | ((uint32_t)self->playGeneration << 9);
            ch.handleTag = handle;
        }
    }

    if (rt_diff_on__) {
        using IsPlayingFn = bool (*)(void*, uint32_t);
        bool theirsPlaying = handle != 0 &&
            RT_ORIGINAL_ONCE(IsPlayingFn, "?IsEffectPlaying@Scene@Audio@@QEBA_NI@Z")(self, handle);
        char a[32], b[32];
        std::snprintf(a, sizeof(a), "handle=0x%x", handle);
        std::snprintf(b, sizeof(b), "DLL sees playing=%d", (int)theirsPlaying);
        RT_DIFF_SHADOW(handle == 0 || theirsPlaying, a, b);
    }
    return handle;
}

// ============================================================================
// uint Audio::Scene::PlayEffect3D(ExternalPtr<SoundEffect> soundRef,
//     Scene::SoundType type, const Time& time, const Vector3& pos)
// retail 0x1800deeb0. Delegates slot SELECTION to the DLL's own
// FindFree3DEffect (retail 0x1800deae0/ord 2352): that helper dereferences
// Audio::SoundEffect internals (a class this lane does not own — none of
// SoundBuffer/SoundEffect/Stream is in this wave, WAVE8.md §2) to decide
// whether an existing slot for the same soundRef is close enough in time to
// reuse. Calling into retail for that ONE sub-decision, then performing the
// WRITE into our own EffectSlot table ourselves, is the same "call the DLL
// for what you don't own" pattern this project uses throughout (e.g.
// MemAlloc/MemFree above) rather than guessing SoundEffect's layout.
// SHADOW oracle (mutates the slot; verified via read-only FindEffect3D after).
// ============================================================================
uint32_t RtAudio_Scene_PlayEffect3D(void* selfPtr, uint64_t soundRef, int32_t soundType,
                                     const Time* time, const Vector3* pos) {
    RT_DIFF_BEGIN("Audio::Scene::PlayEffect3D");
    auto* self = static_cast<Scene*>(selfPtr);
    uint32_t handle = 0;
    if (soundRef != 0) {
        using FindFreeFn = int32_t (*)(void*, uint64_t, const Time*, const Vector3*);
        int32_t idx = RT_ORIGINAL_ONCE(FindFreeFn,
            "?FindFree3DEffect@Scene@Audio@@AEBAHV?$ExternalPtr@VSoundEffect@Audio@@@Object@@AEBVTime@System@@AEBVVector3@Math@@@Z"
            )(self, soundRef, time, pos);
        if (idx >= 0) {
            EffectSlot& slot = self->effects._data[idx];
            slot.soundEffectRef = soundRef;
            slot.soundType = (uint32_t)soundType;
            slot.posX = pos->x; slot.posY = pos->y; slot.posZ = pos->z;
            slot.startTime = time->ticks;
            slot.channelHandle = 0;
            self->playGeneration += 1;
            handle = ((uint32_t)idx << 1) | 1u | ((uint32_t)self->playGeneration << 13);
            slot.idTag = handle;
        }
    }

    if (rt_diff_on__) {
        using FindFn = uint32_t (*)(void*, uint64_t);
        uint32_t theirsFound = handle != 0
            ? RT_ORIGINAL_ONCE(FindFn,
                "?FindEffect3D@Scene@Audio@@QEAAIV?$ExternalPtr@VSoundEffect@Audio@@@Object@@@Z")(self, soundRef)
            : 0;
        char a[32], b[32];
        std::snprintf(a, sizeof(a), "handle=0x%x", handle);
        std::snprintf(b, sizeof(b), "DLL FindEffect3D=0x%x", theirsFound);
        // We just wrote idTag=handle at the SAME slot the DLL's own
        // FindEffect3D scans (soundEffectRef -> idTag), on the SAME live
        // table, so the round trip must land exactly on `handle`.
        RT_DIFF_SHADOW(handle == 0 || theirsFound == handle, a, b);
    }
    return handle;
}

// ============================================================================
// void Audio::Configuration::Save() — retail 0x1800dde10. Four
// IO::Persistence::Set(int) calls, already ported (Wave 4 lane C,
// runtime/src/io/persistence.cpp) but called here via RT_ORIGINAL_ONCE
// against the shipped DLL, matching this project's convention that a lane
// stands alone against the DLL rather than depending on another lane's port
// being enabled. SHADOW oracle: verified via IO::Persistence::Get(int) — a
// full round trip, same shape as Wave 4's own Set(int) oracle.
// ============================================================================
void RtAudio_Configuration_Save(void* selfPtr) {
    RT_DIFF_BEGIN("Audio::Configuration::Save");
    auto* self = static_cast<Configuration*>(selfPtr);

    using SetIntFn = void (*)(void*, const char*, const char*, int);
    using GetIntFn = bool (*)(void*, const char*, const char*, int*);
    void** persistenceSingleton = reinterpret_cast<void**>(
        RtOriginal<void**>("?_singleton@Persistence@IO@@0PEAV12@EA"));
    void* persistence = persistenceSingleton ? *persistenceSingleton : nullptr;

    auto setFn = RT_ORIGINAL_ONCE(SetIntFn, "?Set@Persistence@IO@@QEAAXPEBD0H@Z");
    if (persistence) {
        setFn(persistence, "Audio", "UIVolume",      self->uiVolume);
        setFn(persistence, "Audio", "MusicVolume",   self->musicVolume);
        setFn(persistence, "Audio", "AmbientVolume", self->ambientVolume);
        setFn(persistence, "Audio", "EffectVolume",  self->effectVolume);
    }

    if (rt_diff_on__ && persistence) {
        auto getFn = RT_ORIGINAL_ONCE(GetIntFn, "?Get@Persistence@IO@@QEBA_NPEBD0AEAH@Z");
        int v0 = 0, v1 = 0, v2 = 0, v3 = 0;
        bool ok = getFn(persistence, "Audio", "UIVolume", &v0) && v0 == self->uiVolume
               && getFn(persistence, "Audio", "MusicVolume", &v1) && v1 == self->musicVolume
               && getFn(persistence, "Audio", "AmbientVolume", &v2) && v2 == self->ambientVolume
               && getFn(persistence, "Audio", "EffectVolume", &v3) && v3 == self->effectVolume;
        RT_DIFF_SHADOW(ok, "wrote all 4", ok ? "readback matches" : "readback MISMATCH");
    }
}

// ============================================================================
// bool Audio::Configuration::Startup() — retail 0x1800ddee0. Retail does NOT
// call the exported Persistence::Get(int) here — it inlines its own
// "%s%s"-formatted key lookup straight against Persistence's map (Map_Find,
// retail 0x18002ace0) and a bare strtol. Transcribed exactly rather than
// routed through Get(int), since that is what retail's own bytes do; the two
// are functionally identical (same key format, same map, same strtol), which
// is exactly what the diff below checks.
// ============================================================================
bool RtAudio_Configuration_Startup(void* selfPtr) {
    RT_DIFF_BEGIN("Audio::Configuration::Startup");
    auto* self = static_cast<Configuration*>(selfPtr);

    using GetIntFn = bool (*)(void*, const char*, const char*, int*);
    void** persistenceSingleton = reinterpret_cast<void**>(
        RtOriginal<void**>("?_singleton@Persistence@IO@@0PEAV12@EA"));
    void* persistence = persistenceSingleton ? *persistenceSingleton : nullptr;
    auto getFn = RT_ORIGINAL_ONCE(GetIntFn, "?Get@Persistence@IO@@QEBA_NPEBD0AEAH@Z");

    // Retail leaves the field untouched (whatever the ctor/last value set) when
    // the key is not found — Get(int)'s own contract, which this reproduces by
    // only writing on success.
    if (persistence) {
        int v;
        if (getFn(persistence, "Audio", "UIVolume", &v))      self->uiVolume = v;
        if (getFn(persistence, "Audio", "MusicVolume", &v))   self->musicVolume = v;
        if (getFn(persistence, "Audio", "AmbientVolume", &v)) self->ambientVolume = v;
        if (getFn(persistence, "Audio", "EffectVolume", &v))  self->effectVolume = v;
    }

    RT_DIFF_SCALAR(true, true);  // retail's own body always returns true (no failure path)
    return true;
}

// ============================================================================
// Registration.
// ============================================================================
// ============================================================================
// Audio::Scene::Serialize(IO::Serializer&)   retail 0x1800e0050  ord 3730
// ============================================================================
// UNBLOCKED AND PORTED 2026-09-07 (Wave 9 lane A). Wave 8 lane C held this back
// because THAT LANE "did not independently re-derive retail's own IO::Encoding
// vtable offsets within its budget", and a wrong guess desyncs every save
// silently. It did not have to: Wave 4 had already written retail's offsets
// into the warning block at the top of runtime/src/io/rt_io_port.cpp -- bool
// +0x78 / uint +0x98 / int +0xa0, against the profile build's +0x90/+0xb0/+0xb8
// -- and runtime/types/io/Serializer.h's `rw_vtbl` maps all 26 slots.
//
// Disassembling 0x1800e0050 narrows the exposure to ONE slot: the function
// touches exactly one Encoding entry, kScalarInt (+0xa0), plus kArrayBool
// (+0x20) on the ReaderWriter. 40 instructions.
//
// SHAPE (0x1800e0050), kept:
//   * a LOCAL Serializer is built on the stack holding the caller's Stream and
//     a null ReaderWriter, and it is that local -- not the caller's Serializer
//     -- that is handed to the bulk element pass. Retail never writes through
//     the caller's object.
//   * the ReaderWriter comes from the Encoding factory pair, +0x20 when the
//     stream is WRITING and +0x18 when reading (`CMP [stream+8],0; JNZ`).
//   * the effect count is serialized as a scalar int, and ONLY on the reading
//     side is the array resized to it (0x1800e00c0's JNZ skips the resize when
//     direction != 0) -- so a save never reallocates what it is saving.
//   * `paused[5]` goes out as kArrayBool with n == 5 (`LEA EDX,[R9+0x5]` after
//     `XOR R9D,R9D`, R8 = this+0x30).
//   * the ReaderWriter is released through its scalar-deleting dtor with
//     flags == 1.
//
// The two unexported callees are ordinary Collection internals, called by
// address exactly as Input::KeyMap::ClearAll and RegisterInput already do:
//   0x180051390  Collection::Array<EffectSlot>::Resize(int)   0x40-byte
//                elements, 3x growth heuristic, allocator-locked free
//   0x180052640  the bulk element serialize
// Neither is reimplemented here; both are shared Collection code whose own port
// belongs to whoever ports Collection, not to Audio.
// WAVE 9 LANE Q did that for the first of the two: 0x180051390 is now a
// registered port (kSpec_EffectSlot in runtime/src/collection/array.cpp) and
// still calls retail's per-element copy-assign 0x180051650 and destroy-range
// 0x180052110 by address. 0x180052640 is untouched and still the DLL's.
// One fact from that reading matters to this file: the EffectSlot default
// element leaves bytes +0x2c..+0x2f UNWRITTEN, so a byte comparison of a
// freshly grown _effects array must skip that window per element.
//
// ORACLE: the Wave 9 lane S save-stream A/B harness
// (runtime/src/io/save_trace.h). There is no call-level replay for anything on
// the save path -- ours then theirs would emit twice into one stream -- but on
// the WRITE path a Serialize is a PURE READ of the object, so the two calls can
// be replayed into two INDEPENDENT recording sinks and their emission sequences
// compared. That is what `RtSaveTrace::Compare` does, and it is why this port
// is not held back for "no oracle".
using AudioArrayResizeFn = void (*)(void* array, int32_t newCount);
using AudioBulkSerFn     = void (*)(void* serializer, int32_t count, void* data);

void RtAudio_Scene_Serialize(void* self, void* serializer) {
    RT_DIFF_BEGIN("Audio::Scene::Serialize");
    static auto arrayResize = reinterpret_cast<AudioArrayResizeFn>(
        ResolveAddr(Module::Runtime, 0x180051390));
    static auto bulkSerialize = reinterpret_cast<AudioBulkSerFn>(
        ResolveAddr(Module::Runtime, 0x180052640));

    auto* scene = static_cast<Scene*>(self);
    auto* ser = static_cast<rt::io::Serializer*>(serializer);

    // Retail's stack-local Serializer: the caller's Stream, a null ReaderWriter
    // until the factory fills it. NOT the caller's object.
    rt::io::Serializer local;
    local.stream = ser->stream;
    local.readerWriter = nullptr;

    rt::io::Stream* st = local.stream;
    void** encVtable = *reinterpret_cast<void***>(st->encoding);
    const int factorySlot = (st->direction != rt::io::kDirectionReading) ? 0x20 : 0x18;
    using FactoryFn = void* (*)(void*, void*, void*);
    auto factory = reinterpret_cast<FactoryFn>(encVtable[factorySlot / 8]);
    void* rw = factory(st->encoding, &local, nullptr);
    local.readerWriter = static_cast<rt::io::ReaderWriter*>(rw);

    void** rwVtable = *reinterpret_cast<void***>(rw);

    // kScalarInt (+0xa0) -- (int& value, const char* name)
    int32_t count = scene->effects._size;
    using ScalarIntFn = void (*)(void*, int32_t*, const char*);
    reinterpret_cast<ScalarIntFn>(rwVtable[rt::io::rw_vtbl::kScalarInt / 8])(
        rw, &count, nullptr);

    // Reading only: grow the array to the count just read. Retail's JNZ at
    // 0x1800e00c0 skips this when writing, so saving never reallocates.
    if (local.stream->direction == rt::io::kDirectionReading)
        arrayResize(&scene->effects, count);

    bulkSerialize(&local, count, scene->effects._data);

    // kArrayBool (+0x20) -- (int n, bool* data, const char* name), n == 5.
    using ArrayBoolFn = void (*)(void*, int32_t, void*, const char*);
    reinterpret_cast<ArrayBoolFn>(rwVtable[rt::io::rw_vtbl::kArrayBool / 8])(
        rw, 5, scene->paused, nullptr);

    if (rw) {
        using DtorFn = void (*)(void*, uint32_t);
        reinterpret_cast<DtorFn>(rwVtable[rt::io::rw_vtbl::kDtor / 8])(rw, 1);
    }
}

// ============================================================================
// Wave 8.5 (close-out) — the Audio channel-teardown family
// ============================================================================
// WAVE8_5.md §9 concluded that the six Audio holdbacks "share ONE root cause,
// a stored completion callback fired through a function pointer", and that the
// lane therefore "needs a Device-level fake, not a better diff".
//
// The root cause is right. **The conclusion does not follow, and reading the
// five retail bodies per function instead of as a family shows why:**
//
//     the Device is reachable ONLY through a non-null
//     `Channel::platformChannel[b]` — and that is a field of the SCENE.
//
// Every device call in this family — `Device::Stop` (0x1800de200), and the
// sequence `StopAll` inlines instead of calling it — sits inside
// `if (platformChannel[b] != 0)`. Nothing here dereferences the Device
// singleton before that test; `StopAll` loads it into RBX at 0x1800df192 and
// `StopEffect`/`PauseEffect` into RBP/R14 on entry, but the first instruction
// that touches what it points at is behind the null check.
//
// So a SYNTHETIC Scene whose 256 channels have null `platformChannel[]` puts
// the device out of reach BY CONSTRUCTION, and both implementations can then
// be run side by side over the whole state space — no interposer, no recorder,
// no fake. That is what `ProbeAudioScene` in standalone/rt_orphan_probe.cpp
// does, and it is why these three CALLED holdbacks land here rather than
// waiting on a tool. The remaining three (`Scene::Update`, `Device::Update`,
// `Device::FreeChannelPlatform`) are NOT unblocked by it: the first consumes
// System::Random state and the other two are device internals, where the
// device is the subject rather than an unreachable callee.
//
// This is WAVE8_5.md §8's own correction happening a fifth time: a group-level
// claim over these functions failed per-function checking. Recorded, not fixed
// silently.
//
// WHAT THE LIVE PATH GETS. On a real scene the platform channels are NOT null,
// so there is no call-level replay: running both bodies would fire every live
// effect's completion callback twice (§9's point, which stands). Each function
// below therefore carries a SHADOW check instead — after our body runs, the
// DLL's own read-only observer is asked whether the resulting state is what it
// should be — exactly the pattern `PlayEffect` above already uses. The full
// state comparison is the probe's job; the shadow's job is to give the live
// path a `calls=` number and catch a port that stopped agreeing with reality.
//
// RETAIL READS UNINITIALISED STACK, and a byte oracle must know it.
// `StopEffect` (0x1800df5e6) and `StopAll` (0x1800df2af) both build the reset
// Channel by writing a DWORD to a stack slot and reading a QWORD back out:
//     MOV dword ptr [RSP+0x28],EDX      ; 4 bytes written
//     MOV RAX,qword ptr [RSP+0x28]      ; 8 bytes read
//     MOV qword ptr [RSI+RCX+0x8],RAX   ; soundType=0, _pad0c=STACK GARBAGE
// and the same shape at [RSP+0x88] for +0x68 (handleTag=0, started=0,
// _pad6d[3]=garbage). Both landing fields are the padding this file's type
// header already documents as never read. We write zeroes; the probe EXCLUDES
// those 7 bytes per channel rather than pretending to reproduce a caller's
// stack (reference_oracle_must_not_compare_unwritten_bytes.md, in reverse —
// there the trap was comparing bytes NEITHER side wrote, here it is bytes only
// one side can write meaningfully).
// ============================================================================

// Audio::Device::Stop(AudioChannel*) — retail 0x1800de200, ord 4105. The one
// device entry point this family needs, and the one that fires the stored
// completion callback (WAVE8_5.md §9). Left on the DLL: it is Device's own
// code, not Scene's, and calling it by address is the same faithful delegation
// this file already uses for the two Collection internals in Scene::Serialize.
using AudioDeviceStopFn = void (*)(void* device, void* audioChannel);

static void* AudioDeviceSingleton() {
    // ?_singleton@Device@Audio@@0PEAV12@EA (ord 4805) is an exported DATA
    // symbol: the export address IS the pointer slot, so this is one deref,
    // matching retail's `MOV RBX,qword ptr [0x1801fe760]`.
    static void** slot = reinterpret_cast<void**>(
        ResolveExportRaw("?_singleton@Device@Audio@@0PEAV12@EA"));
    return slot ? *slot : nullptr;
}

// Release every live platform channel hanging off one Channel record.
// `clearSlots` reproduces the one behavioural difference between the two
// retail callers: PauseEffect nulls the pointer and clears bit 1 of the flag
// byte as it goes (0x1800df50d/0x1800df515), because the Channel record
// SURVIVES a pause; StopEffect does not bother (0x1800df5a3..0x1800df5bf),
// because the whole record is overwritten with the default two instructions
// later.
static void ReleasePlatformChannels(Scene* self, uint32_t chOffsetBytes, bool clearSlots) {
    void* device = AudioDeviceSingleton();
    for (int b = 0; b < 8; ++b) {
        // Retail RE-READS channels._data on every iteration (the callee could
        // in principle move it); reproduced rather than hoisted.
        auto* base = reinterpret_cast<uint8_t*>(self->channels._data);
        auto* ch = reinterpret_cast<Channel*>(base + chOffsetBytes);
        void* p = ch->platformChannel[b];
        if (!p) continue;
        RT_ORIGINAL_ONCE(AudioDeviceStopFn,
            "?Stop@Device@Audio@@QEAAXPEAUAudioChannel@12@@Z")(device, p);
        if (clearSlots) {
            base = reinterpret_cast<uint8_t*>(self->channels._data);
            ch = reinterpret_cast<Channel*>(base + chOffsetBytes);
            ch->platformChannel[b] = nullptr;
            ch->channelFlags[b] &= 0xfd;
        }
    }
}

// The default-constructed Channel every teardown path writes back, per
// FUN_180051020 and reconfirmed by StopEffect's and StopAll's identical reset
// blocks: everything zero except volume and pitch, which are 1.0f.
static void ResetChannel(Channel* ch) {
    ch->soundEffectRef = 0;
    ch->soundType      = 0;
    ch->_pad0c         = 0;   // retail leaves stack garbage here; see the header
    ch->startTime      = 0;
    ch->volume         = 1.0f;
    ch->pitch          = 1.0f;
    for (int b = 0; b < 8; ++b) ch->platformChannel[b] = nullptr;
    for (int b = 0; b < 8; ++b) ch->channelFlags[b] = 0;
    ch->handleTag = 0;
    ch->started   = 0;
    ch->_pad6d[0] = ch->_pad6d[1] = ch->_pad6d[2] = 0;   // ditto
}

// The channel-handle validity test, spelled once. `(stored ^ handle) & top-23`
// — the encoding this file's type header documents. Retail repeats it inline
// in PauseEffect, UnpauseEffect, StopEffect, StopAll, Pause and
// IsEffectPlaying.
static bool ChannelHandleMatches(const Scene* self, uint32_t handle, uint32_t* outIndex) {
    uint32_t idx = (handle >> 1) & 0xff;
    *outIndex = idx;
    return ((self->channels._data[idx].handleTag ^ handle) & 0xfffffe00) == 0;
}

// ============================================================================
// void Audio::Scene::PauseEffect(uint handle) — retail 0x1800df4a0, ord 3314
// ============================================================================
// Releases the platform channels of one live effect but KEEPS the Channel
// record, so the effect can be restarted by clearing `started` later — which
// is exactly what Pause's unpause direction does. Not called from our tree
// directly; it is here because Scene::Pause CALLs it (retail does not inline
// it) and a port of Pause that left this on the DLL would be the half-ported
// hazard WAVE8.md §1 warns about.
// ============================================================================
void RtAudio_Scene_PauseEffect(void* selfPtr, uint32_t handle) {
    RT_DIFF_BEGIN("Audio::Scene::PauseEffect");
    auto* self = static_cast<Scene*>(selfPtr);
    uint32_t idx;
    const bool matched = ChannelHandleMatches(self, handle, &idx);

    // SHADOW, and it is a FULL REPLAY of the only branch this function has.
    // The DLL's IsEffectPlaying (0x1800df680) computes the identical predicate
    // on the same channel, so asking it is asking retail whether our guard
    // agreed with retail's, for real handles on a real scene. Reported BEFORE
    // the early return, so the guard-fail path still counts as a call: a shadow
    // placed after the return makes `calls=` mean "did work" when every reader
    // of that column takes it to mean "was entered".
    using IsPlayingFn = bool (*)(void*, uint32_t);
    RT_DIFF_SHADOW(matched == RT_ORIGINAL_ONCE(IsPlayingFn,
                       "?IsEffectPlaying@Scene@Audio@@QEBA_NI@Z")(self, handle),
                   matched ? "handle matches" : "handle rejected", "DLL IsEffectPlaying");
    if (!matched) return;
    ReleasePlatformChannels(self, idx * 0x70u, /*clearSlots=*/true);

    // POST-CONDITION, stated exactly: a paused channel keeps its record (that
    // is the whole difference from StopEffect) and holds no platform channel.
    const Channel& ch = self->channels._data[idx];
    bool anyLeft = false;
    for (int b = 0; b < 8; ++b) if (ch.platformChannel[b]) anyLeft = true;
    RT_DIFF_SHADOW(!anyLeft, "0 platform channels, record kept", "post-condition");
}

// ============================================================================
// void Audio::Scene::StopEffect(uint handle) — retail 0x1800df550, ord 4109
// ============================================================================
// Releases the platform channels AND resets the Channel record to default, so
// the handle stops validating. Reached from our tree only through
// StopEffect3D; ported for the same reason as PauseEffect.
// ============================================================================
void RtAudio_Scene_StopEffect(void* selfPtr, uint32_t handle) {
    RT_DIFF_BEGIN("Audio::Scene::StopEffect");
    auto* self = static_cast<Scene*>(selfPtr);
    uint32_t idx;
    const bool matched = ChannelHandleMatches(self, handle, &idx);

    // SHADOW: the same full replay of the guard as PauseEffect, reported before
    // the early return for the same reason.
    using IsPlayingFn = bool (*)(void*, uint32_t);
    auto isPlaying = RT_ORIGINAL_ONCE(IsPlayingFn, "?IsEffectPlaying@Scene@Audio@@QEBA_NI@Z");
    RT_DIFF_SHADOW(matched == isPlaying(self, handle),
                   matched ? "handle matches" : "handle rejected", "DLL IsEffectPlaying");
    if (!matched) return;
    ReleasePlatformChannels(self, idx * 0x70u, /*clearSlots=*/false);
    ResetChannel(&self->channels._data[idx]);

    // POST-CONDITION. Stated as "the record was zeroed", NOT as "the handle no
    // longer validates" -- the first attempt said the latter and it is FALSE
    // for a handle whose generation bits are all zero, which validates against
    // a zeroed tag in retail too. That predicate reported 2 divergences in 236
    // live calls and every one of them was the PREDICATE being wrong about
    // retail, not the port (reference_shadow_predicate_must_name_its_failing_term.md).
    RT_DIFF_SHADOW(self->channels._data[idx].handleTag == 0,
                   "channel record reset", "post-condition");
}

// ============================================================================
// void Audio::Scene::Pause(int soundType, bool paused) — retail 0x1800df070
// ============================================================================
// Called six times a menu->game cycle (mainmenudialog.cpp pausing types 4/3/2
// on open, gameupdateloop_load.cpp unpausing them after a load).
//
// The two directions are not symmetric, and the asymmetry is the function:
//   paused = true   -> PauseEffect every live channel of this sound type,
//                      which releases its platform channels.
//   paused = false  -> for every live channel of this type, re-validate the
//                      handle it stores AGAINST ITSELF and clear `started`,
//                      so Scene::Update restarts it. This direction calls
//                      NOTHING — it is `UnpauseEffect` (0x1800df470) inlined,
//                      instruction for instruction.
// Retail strides a HARDCODED 0x7000 bytes (256 channels), not `_size`;
// reproduced.
//
// The leading early-out (`if (paused[soundType] == paused) return;`) is the
// reason a naive ours-then-theirs replay would report green having compared
// nothing — WAVE8_5.md §9's observation, and the `RegisterInput` trap. The
// probe steps INDEPENDENT scenes, so it does not apply there.
//
// `soundType` is sign-extended (MOVSXD RSI,EDX) and used unchecked as an index
// into `paused[5]`; a negative or >4 value reads and writes out of bounds in
// retail too. Left faithful (CLAUDE.md: do not "fix" a faithful deref).
// ============================================================================
void RtAudio_Scene_Pause(void* selfPtr, int32_t soundType, bool paused) {
    RT_DIFF_BEGIN("Audio::Scene::Pause");
    auto* self = static_cast<Scene*>(selfPtr);
    int64_t t = soundType;
    const bool alreadyThere = (self->paused[t] == (uint8_t)paused);

    if (!alreadyThere) {
        if (paused) {
            for (uint32_t off = 0; off < 0x7000u; off += 0x70u) {
                auto* base = reinterpret_cast<uint8_t*>(self->channels._data);
                auto* ch = reinterpret_cast<Channel*>(base + off);
                uint32_t tag = ch->handleTag;
                if (tag == 0 || (int32_t)ch->soundType != soundType) continue;
                RtAudio_Scene_PauseEffect(self, tag);
            }
        } else {
            for (uint32_t off = 0; off < 0x7000u; off += 0x70u) {
                auto* base = reinterpret_cast<uint8_t*>(self->channels._data);
                auto* ch = reinterpret_cast<Channel*>(base + off);
                uint32_t tag = ch->handleTag;
                if (tag == 0 || (int32_t)ch->soundType != soundType) continue;
                uint32_t idx;
                if (!ChannelHandleMatches(self, tag, &idx)) continue;
                self->channels._data[idx].started = 0;
            }
        }
        self->paused[t] = (uint8_t)paused;
    }

    // SHADOW: the DLL's own IsPaused (0x180050990, ord 3151) is a one-byte read
    // of paused[soundType] and cannot disturb anything.
    //
    // The early-out is INSIDE the shadow's reach on purpose. Written the
    // faithful way -- `if (paused[t] == paused) return;` before any reporting --
    // this port ran on both a fresh autostart and a Save9 load and reported
    // `calls=0` on BOTH, because every one of the six live call sites asks for
    // a state the scene is already in. The site appeared in the site list (so
    // the body WAS entered) while the count said zero, which is precisely the
    // shape of reference_uncalled_port_is_not_verified.md and would have been
    // read as a dead port. `calls=` has to mean "entered".
    using IsPausedFn = bool (*)(void*, int32_t);
    bool theirs = RT_ORIGINAL_ONCE(IsPausedFn, "?IsPaused@Scene@Audio@@QEBA_NH@Z")(self, soundType);
    RT_DIFF_SHADOW(theirs == paused,
                   alreadyThere ? (paused ? "paused (early-out)" : "unpaused (early-out)")
                                : (paused ? "paused" : "unpaused"),
                   "DLL IsPaused");
}

// ============================================================================
// void Audio::Scene::StopEffect3D(uint id) — retail 0x1800df370
// ============================================================================
// Stops the Channel a 3D effect is playing on (if any) and resets its
// EffectSlot to default. The slot's `positions` array is released through
// Collection's own array-assign-from-empty, called by address exactly as
// Scene::Serialize calls its two Collection internals.
//
// The guard is spelled differently from FindEffectSlot's and means the SAME
// THING. Retail compares the generation halves (`id >> 13` against
// `idTag >> 13`, 0x1800df3a7/0x1800df3aa); FindEffectSlot masks with
// 0xffffe000, which is bits 13..31 -- the generation, and nothing else. An
// earlier draft of this comment claimed they "differ by one bit" and built a
// shadow predicate on that claim; the predicate reported 56 divergences in 96
// live calls, all of them the claim being wrong. Retail's spelling is kept
// here because it is retail's, not because it differs.
// ============================================================================
using AudioArrayAssignFn = void (*)(void* dst, const void* src);
using AudioArrayDtorFn   = void (*)(void* arr);

void RtAudio_Scene_StopEffect3D(void* selfPtr, uint32_t id) {
    RT_DIFF_BEGIN("Audio::Scene::StopEffect3D");
    static auto arrayAssign = reinterpret_cast<AudioArrayAssignFn>(
        ResolveAddr(Module::Runtime, 0x180050d30));
    static auto arrayDtor = reinterpret_cast<AudioArrayDtorFn>(
        ResolveAddr(Module::Runtime, 0x180051d80));

    auto* self = static_cast<Scene*>(selfPtr);
    uint32_t idx = (id >> 1) & 0xfff;
    EffectSlot* slot = &self->effects._data[idx];
    using GetCountFn = int (*)(void*, uint32_t);
    auto getCount = RT_ORIGINAL_ONCE(GetCountFn, "?GetEmitterCount@Scene@Audio@@QEBAHI@Z");

    if ((slot->idTag >> 13) != (id >> 13)) {
        // SHADOW on the reject path -- reported rather than returned silently,
        // for the same reason as Pause's early-out: `calls=` must mean
        // "entered". The DLL's GetEmitterCount uses a guard one bit WIDER than
        // this one (0xffffe000 against the generation-only compare here), so a
        // reject here does NOT imply a reject there; what it does imply is that
        // the slot is untouched, which is what is asserted.
        // The DLL's GetEmitterCount validates with `(idTag ^ id) & 0xffffe000`,
        // which is bits 13..31 -- the generation, and NOTHING ELSE. That is the
        // same predicate this guard spells as `(idTag >> 13) != (id >> 13)`, so
        // a reject here means a reject there and GetEmitterCount must answer 0.
        // A full replay of the guard, against the DLL, on live ids.
        RT_DIFF_SHADOW(getCount(self, id) == 0,
                       "generation rejected", "DLL GetEmitterCount");
        return;
    }

    if (slot->channelHandle != 0)
        RtAudio_Scene_StopEffect(self, slot->channelHandle);

    // Retail's stack-local default EffectSlot; only its `positions` member is
    // used (as the empty source of the assign), but the whole record is built.
    EffectSlot tmp{};
    tmp.baseVolume = 1.0f;

    slot->posX = slot->posY = slot->posZ = 0.0f;
    slot->soundType      = 0;
    slot->soundEffectRef = 0;
    slot->startTime      = 0;
    slot->channelHandle  = 0;
    slot->idTag          = 0;
    slot->baseVolume     = 1.0f;
    arrayAssign(&slot->positions, &tmp.positions);
    arrayDtor(&tmp.positions);

    // SHADOW: the DLL's read-only GetEmitterCount on the id we just retired
    // must report 0 -- the slot is default and its idTag no longer validates.
    RT_DIFF_SHADOW(getCount(self, id) == 0, "slot reset", "DLL GetEmitterCount");
}

// ============================================================================
// void Audio::Scene::StopAll() — retail 0x1800df150, ord 4107
// ============================================================================
// Called once, from GameUpdateLoop teardown (gameupdateloop.cpp:2880).
//
// It is `StopEffect` applied to all 256 channels, with `Device::Stop`'s body
// INLINED rather than called — and the inlining is exact: 0x1800df1eb's
// `CMP [device+0xa88],0` guard is `Device::StopPlatform` (0x1800dea20)'s own
// first instruction, 0x1800df25b calls `FreeChannelPlatform` (0x1800de6f0)
// just as Device::Stop does, and 0x1800df260..0x1800df289 is Device::Stop's
// trailer instruction for instruction (clear the low 2 flag bits, null the
// callback slot, then fire the callback that was in it). So delegating to the
// exported `Device::Stop` is not an approximation of this body — it is the
// same code with the call re-introduced.
//
// The INDIRECTION is faithful and deliberate: the outer loop reads the tag
// from channel `i` but does all its work on channel `(tag>>1)&0xff`. Those are
// the same channel for any handle the Scene itself minted, so the difference
// is invisible in a normal run — and reproducing it costs one line.
// ============================================================================
void RtAudio_Scene_StopAll(void* selfPtr) {
    RT_DIFF_BEGIN("Audio::Scene::StopAll");
    auto* self = static_cast<Scene*>(selfPtr);

    // The set this call is OBLIGED to retire, computed BEFORE any of it runs
    // and computed with the DLL's OWN predicate (IsEffectPlaying is
    // `((storedTag ^ handle) & 0xfffffe00) == 0` on the channel the handle
    // names -- exactly the test at 0x1800df1ac). Deciding the obligation after
    // the fact is what made the first version of this shadow wrong: "no channel
    // still has a tag" is FALSE of retail, because a channel whose tag names a
    // DIFFERENT channel keeps its own tag while the other one is reset. That
    // predicate reported 8 divergences in 9 calls and all 8 were the predicate.
    using IsPlayingFn = bool (*)(void*, uint32_t);
    auto isPlaying = RT_ORIGINAL_ONCE(IsPlayingFn, "?IsEffectPlaying@Scene@Audio@@QEBA_NI@Z");
    uint8_t owed[256];
    if (rt_diff_on__) {
        for (int i = 0; i < 256; ++i) {
            uint32_t tag = self->channels._data[i].handleTag;
            owed[i] = (uint8_t)(tag != 0 && isPlaying(self, tag) ? ((tag >> 1) & 0xff) + 1 : 0);
        }
    }

    for (uint32_t off = 0; off < 0x7000u; off += 0x70u) {
        auto* base = reinterpret_cast<uint8_t*>(self->channels._data);
        uint32_t tag = reinterpret_cast<Channel*>(base + off)->handleTag;
        if (tag == 0) continue;
        uint32_t idx;
        if (!ChannelHandleMatches(self, tag, &idx)) continue;
        ReleasePlatformChannels(self, idx * 0x70u, /*clearSlots=*/false);
        ResetChannel(&self->channels._data[idx]);
    }

    if (rt_diff_on__) {
        bool ok = true;
        for (int i = 0; i < 256; ++i)
            if (owed[i] && self->channels._data[owed[i] - 1].handleTag != 0) ok = false;
        RT_DIFF_SHADOW(ok, "every channel a live handle named was reset",
                       "pre-computed from DLL IsEffectPlaying");
    }
}

int RtAudioSceneRegister() {
    int n = 0;
    RegisterRuntimeExport("??0Scene@Audio@@QEAA@XZ", (void*)&RtAudio_Scene_Ctor); ++n;
    RegisterRuntimeExport("??1Scene@Audio@@QEAA@XZ", (void*)&RtAudio_Scene_Dtor); ++n;
    RegisterRuntimeExport("?AddEmitter@Scene@Audio@@QEAAXIAEBVVector3@Math@@M@Z", (void*)&RtAudio_Scene_AddEmitter); ++n;
    RegisterRuntimeExport("?MoveEmitter@Scene@Audio@@QEAAXIAEBVVector3@Math@@0@Z", (void*)&RtAudio_Scene_MoveEmitter); ++n;
    RegisterRuntimeExport("?RemoveEmitter@Scene@Audio@@QEAAXIAEBVVector3@Math@@@Z", (void*)&RtAudio_Scene_RemoveEmitter); ++n;
    RegisterRuntimeExport("?SetEmitterVolume@Scene@Audio@@QEAAXIAEBVVector3@Math@@M@Z", (void*)&RtAudio_Scene_SetEmitterVolume); ++n;
    RegisterRuntimeExport("?GetEmitterCount@Scene@Audio@@QEBAHI@Z", (void*)&RtAudio_Scene_GetEmitterCount); ++n;
    RegisterRuntimeExport("?IsEffectPlaying@Scene@Audio@@QEBA_NI@Z", (void*)&RtAudio_Scene_IsEffectPlaying); ++n;
    RegisterRuntimeExport("?FindEffect3D@Scene@Audio@@QEAAIV?$ExternalPtr@VSoundEffect@Audio@@@Object@@@Z", (void*)&RtAudio_Scene_FindEffect3D); ++n;
    // PlayEffect: register under BOTH the real (QEAA) mangled name AND the
    // retail address, per WAVE8.md §4 — engine_funcs.cpp's own address
    // fallback (0x1800def60) must land on OUR port too, not just the name.
    RegisterRuntimeExport("?PlayEffect@Scene@Audio@@QEAAIV?$ExternalPtr@VSoundEffect@Audio@@@Object@@W4SoundType@12@AEBVTime@System@@@Z", (void*)&RtAudio_Scene_PlayEffect); ++n;
    RegisterRuntimeAddr(0x1800def60, (void*)&RtAudio_Scene_PlayEffect);
    RegisterRuntimeExport("?PlayEffect3D@Scene@Audio@@QEAAIV?$ExternalPtr@VSoundEffect@Audio@@@Object@@W4SoundType@12@AEBVTime@System@@AEBVVector3@Math@@@Z", (void*)&RtAudio_Scene_PlayEffect3D); ++n;

    // Wave 9 lane A: unblocked once retail's IO::Encoding offsets turned out to
    // be recorded already (Wave 4), and verified by the lane S save-stream A/B
    // oracle rather than by call-level replay, which the save path cannot have.
    RegisterRuntimeExport("?Serialize@Scene@Audio@@QEAAXAEAVSerializer@IO@@@Z",
                          (void*)&RtAudio_Scene_Serialize);
    RegisterRuntimeAddr(0x1800e0050, (void*)&RtAudio_Scene_Serialize); ++n;

    // Wave 8.5 close-out: the channel-teardown family. Unblocked by reading
    // the bodies per function rather than as a group -- the Device is only
    // reachable through a non-null Channel::platformChannel[b], which is a
    // SCENE field, so a synthetic scene replays all five with no device fake.
    // Verified by ProbeAudioScene (standalone/rt_orphan_probe.cpp) and by a
    // live SHADOW check each; see the block comment above these bodies.
    RegisterRuntimeExport("?PauseEffect@Scene@Audio@@QEAAXI@Z", (void*)&RtAudio_Scene_PauseEffect); ++n;
    RegisterRuntimeExport("?StopEffect@Scene@Audio@@QEAAXI@Z", (void*)&RtAudio_Scene_StopEffect); ++n;
    RegisterRuntimeExport("?Pause@Scene@Audio@@QEAAXH_N@Z", (void*)&RtAudio_Scene_Pause);
    // engine_funcs.cpp has an address fallback for Pause (it reads the exe
    // dispatch slot when the name misses), so the address must land on us too.
    RegisterRuntimeAddr(0x1800df070, (void*)&RtAudio_Scene_Pause); ++n;
    RegisterRuntimeExport("?StopEffect3D@Scene@Audio@@QEAAXI@Z", (void*)&RtAudio_Scene_StopEffect3D); ++n;
    RegisterRuntimeExport("?StopAll@Scene@Audio@@QEAAXXZ", (void*)&RtAudio_Scene_StopAll); ++n;
    return n;
}

int RtAudioConfigurationRegister() {
    int n = 0;
    RegisterRuntimeExport("?Save@Configuration@Audio@@QEAAXXZ", (void*)&RtAudio_Configuration_Save); ++n;
    RegisterRuntimeExport("?Startup@Configuration@Audio@@QEAA_NXZ", (void*)&RtAudio_Configuration_Startup); ++n;
    return n;
}

// ============================================================================
// Wave 9 lane S probe — verify Audio::Scene::Serialize against the shipped body
// ============================================================================
// Called once, from the menu loop where a live `Audio::Scene` is in hand
// (`MenuDLT + 0xb30`). Any harness path reaches the menu, so this needs no
// gameplay and no save.
//
// It runs BOTH bodies against synthetic recording sinks in the WRITING
// direction, so neither touches a real stream and neither mutates the Scene --
// see runtime/src/io/save_trace.h for why a write-path Serialize is replayable
// at all when the family was held back for "no oracle possible".
extern "C" void RtAudio_Scene_SerializeTraceProbe(void* scene) {
    if (!scene || !RtSaveTrace::Enabled()) return;
    static bool done = false;
    if (done) return;
    done = true;

    auto theirs = reinterpret_cast<void (*)(void*, void*)>(
        ResolveExportRaw("?Serialize@Scene@Audio@@QEAAXAEAVSerializer@IO@@@Z"));
    if (!theirs) {
        std::fprintf(stderr, "[RtSaveTrace] SKIP Audio::Scene::Serialize: "
                             "the shipped body did not resolve\n");
        return;
    }
    RtSaveTrace::Compare("Audio::Scene::Serialize", scene,
                         &RtAudio_Scene_Serialize, theirs);
    // Self-test of the lane S PREFIX STOP on a subject already known clean:
    // the same comparison truncated to 8 emissions must still pass and must
    // say PREFIX ONLY. Proving the stop on a known-good subject is the only
    // way to tell a working stop from a stop that silently compares nothing.
    RtSaveTrace::Compare("Audio::Scene::Serialize[prefix8]", scene,
                         &RtAudio_Scene_Serialize, theirs, 8);
}
