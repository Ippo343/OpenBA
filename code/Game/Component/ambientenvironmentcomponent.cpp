// code/Game/Component/ambientenvironmentcomponent.cpp
// Clean reimplementation of AmbientEnvironmentComponent::Update (exe 0x140043310)
// plus its two audio helpers (sub_2a10 0x140042a10, sub_2c60 0x140042c60).
//
// This is the biome-driven ambient "bed" audio — the wind/nature loops that
// crossfade between a day set and a night set based on temperature, sampled over
// the tile-grid region the entity covers. It drives the sibling
// AmbientEmitterComponent on the same entity through the world Audio::Scene
// (gameWorld+0x78) using the 3D emitter API.
//
// FAITHFUL PORT (verified in Ghidra against the three exe functions):
//
//   Update(self):
//     grid   = *(g_pGameWorld+0xf8)              // Game object; +0x00 gridWidth, +0x80 cells
//     desc   = IsDerivedFrom(self+0x18, AmbientEnvironmentDescription)   // biome cfg
//     count  = desc+0x28                          // biome-table entry count
//     hist   = int[count] (zeroed)
//     for y in [0, self+0x34): for x in [0, self+0x30):
//         cell = cells[(self.gridY(+0x2c)+y)*gridWidth + (self.gridX(+0x28)+x)]  (stride 0x30)
//         bits = cell+0x29                         // biome bitmask index
//         ent  = cell+0x00 ? : cell+0x08           // cell's entity ref (IBP)
//         for k in [0, count):  bt = desc.table(+0x20)[k]  (stride 0x28)
//             if (bt+0x00 mask & (1<<bits)) && (ent filter matches bt+0x08):
//                 hist[k] += bt+0x20 (weight); break
//     dominant = argmax(hist)
//     f = clamp( (weather.temp(+0x130,+0x18) - desc+0x30) / (desc+0x34 - desc+0x30), 0, 1)
//     emitter = the entity's AmbientEmitterComponent (scan owner+0x30 list, type match)
//     SetSlot(emitter, 0, table[dominant]+0x10 /*day  sound*/, f)
//     SetSlot(emitter, 1, table[dominant]+0x18 /*night sound*/, 1 - f)
//
//   SetSlot == sub_2a10, PlayNew == sub_2c60 (see bodies below).
//
// Prerequisites confirmed present in standalone: the tile grid (Game+0x80, built
// by worldgen), the weather sim (gameWorld+0x130), and the type-id globals
// (registered by the factory table). The Audio::Scene is live and ticked in-game.
//
// Gated by BANISHED_AMBIENT_AUDIO (default ON; =0 restores the inert no-op for
// A/B). Set BANISHED_AMBIENT_TRACE=1 for per-tick diagnostics.
//
// This file must NOT depend on hooks.h -- portable for standalone exe.

#include "Component/ambientenvironmentcomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"   // Object::GetTypeIdByName
#endif

// Emitter-array resize helper, shared with the sibling component reimpl
// (exe FUN_140001770 / AmbientEmitterComponent_sub_1770).
extern "C" void AmbientEmitterResize_Reimpl(void* arrayBase, int newCount);

namespace {

// Type-id values (CRC32 of the type name; populated after Engine::Startup).
// In standalone the .rdata type-id globals are NOT resolvable via
// ResolveAddr(Module::Exe, addr) (they hit a not-implemented stub), so we look
// them up by name via the factory registry — the same value fn_IsDerivedFrom and
// the inert GetTypeId vtable slot compare against. In DLL mode the exe is mapped,
// so read the live .rdata slot directly.
uint32_t s_tidEnvDesc     = 0;  // AmbientEnvironmentDescription (0x1401c3f08)
uint32_t s_tidEmitterComp = 0;  // AmbientEmitterComponent       (0x1401c3e60)
uint32_t s_tidEmitterDesc = 0;  // AmbientEmitterDescription     (0x1401c3e98)
bool s_resolved = false;

void EnsureResolved() {
    if (s_resolved) return;
#ifdef STANDALONE_EXE
    s_tidEnvDesc     = ::Object::GetTypeIdByName("AmbientEnvironmentDescription");
    s_tidEmitterComp = ::Object::GetTypeIdByName("AmbientEmitterComponent");
    s_tidEmitterDesc = ::Object::GetTypeIdByName("AmbientEmitterDescription");
#else
    auto rd = [](uintptr_t a) -> uint32_t {
        uintptr_t p = ResolveAddr(Module::Exe, a);
        return p ? *reinterpret_cast<uint32_t*>(p) : 0;
    };
    s_tidEnvDesc     = rd(0x1401c3f08);
    s_tidEmitterComp = rd(0x1401c3e60);
    s_tidEmitterDesc = rd(0x1401c3e98);
#endif
    // Only latch as resolved once the factory registry has frozen the ids
    // (they start at the 0xFFFFFFFF sentinel before Factory::Startup).
    if (s_tidEnvDesc != 0 && s_tidEnvDesc != 0xFFFFFFFFu)
        s_resolved = true;
}

inline void* WorldScene() {
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return nullptr;
    return *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0x78);
}

// Get the AmbientEmitterDescription from a component's +0x18, or null.
inline void* EmitterDesc(void* comp) {
    void* obj = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(comp) + 0x18);
    if (!obj || !fn_IsDerivedFrom || !s_tidEmitterDesc) return nullptr;
    return fn_IsDerivedFrom(obj, s_tidEmitterDesc) ? obj : nullptr;
}

// ---- sub_2c60: (re)play the effect for one emitter slot and add its emitter. ----
// entry (stride 0x10 in comp+0x38 array): +0x00 uint handle, +0x04 float vol,
// +0x08 ExternalPtr soundRef.
void PlayNewSlot(void* comp, int slot) {
    void* scene = WorldScene();
    if (!scene) return;
    void* desc = EmitterDesc(comp);
    uint8_t* arr = *reinterpret_cast<uint8_t**>(reinterpret_cast<uintptr_t>(comp) + 0x38);
    if (!arr) return;
    uint8_t* entry = arr + (intptr_t)slot * 0x10;
    uint64_t* soundRef = reinterpret_cast<uint64_t*>(entry + 8);

    if (*soundRef == 0 && desc) {
        void* sounds = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(desc) + 0x28);
        if (sounds) *soundRef = reinterpret_cast<const uint64_t*>(sounds)[slot];
    }

    uint32_t handle = fn_Audio_Scene_FindEffect3D ? fn_Audio_Scene_FindEffect3D(scene, *soundRef) : 0;
    *reinterpret_cast<uint32_t*>(entry + 0) = handle;
    if (handle == 0 && fn_Audio_Scene_PlayEffect3D) {
        int64_t timeZero = 0;
        float   posZero[3] = { 0.0f, 0.0f, 0.0f };
        handle = fn_Audio_Scene_PlayEffect3D(scene, *soundRef, 3, &timeZero, posZero);
        *reinterpret_cast<uint32_t*>(entry + 0) = handle;
    }
    if (fn_Audio_Scene_AddEmitter) {
        const void* pos = reinterpret_cast<const void*>(reinterpret_cast<uintptr_t>(comp) + 0x28);
        fn_Audio_Scene_AddEmitter(scene, handle, pos, *reinterpret_cast<float*>(entry + 4));
    }
    static int s_trace = -1;
    if (s_trace < 0) { const char* e = std::getenv("BANISHED_AMBIENT_TRACE"); s_trace = (e && e[0] && e[0] != '0') ? 1 : 0; }
    if (s_trace) {
        static int n = 0;
        if (n++ < 12)
            fprintf(stderr, "[ambientenv] PlayNewSlot comp=%p slot=%d soundRef=%llx handle=%u vol=%.3f\n",
                    comp, slot, (unsigned long long)*soundRef, handle, *reinterpret_cast<float*>(entry + 4));
    }
}

// ---- sub_2a10: set slot `slot` of the emitter component to `soundRef` @ `vol`. ----
void SetSlot(void* comp, int slot, uint64_t soundRef, float vol) {
    void* desc = EmitterDesc(comp);
    // Ensure the emitter array matches the description's emitter count.
    if (desc) {
        int wantCount = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(desc) + 0x30);
        if (*reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(comp) + 0x40) != wantCount)
            AmbientEmitterResize_Reimpl(reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(comp) + 0x38), wantCount);
    }

    uint8_t* arr = *reinterpret_cast<uint8_t**>(reinterpret_cast<uintptr_t>(comp) + 0x38);
    if (!arr) return;
    uint8_t* entry = arr + (intptr_t)slot * 0x10;
    const void* pos = reinterpret_cast<const void*>(reinterpret_cast<uintptr_t>(comp) + 0x28);

    uint32_t handle = *reinterpret_cast<uint32_t*>(entry + 0);
    uint64_t* curRef = reinterpret_cast<uint64_t*>(entry + 8);

    if (handle == 0) {
        // Record the sound reference + volume, then START playback.
        //
        // DEVIATION FROM RETAIL: the exe's sub_2a10 only records here (return
        // without playing) because retail initializes the emitter handles earlier
        // via the AmbientEmitterComponent's OnAdded (FUN_140042e10). In standalone
        // that component is inert (OnAdded is a no-op stub), so nothing ever seeds
        // the handle and sub_2a10's play branches (which require handle!=0) would
        // never fire. We seed it on-demand here: PlayNewSlot does FindEffect3D /
        // PlayEffect3D + AddEmitter for this slot's biome sound. Once handle!=0 the
        // subsequent ticks follow the faithful volume-crossfade / sound-swap paths.
        *curRef = soundRef;
        *reinterpret_cast<float*>(entry + 4) = vol;
        if (soundRef != 0) PlayNewSlot(comp, slot);
        return;
    }

    if (*curRef != soundRef) {
        // Sound changed — tear down the old emitter, then play the new one.
        void* scene = WorldScene();
        if (scene) {
            if (fn_Audio_Scene_RemoveEmitter) fn_Audio_Scene_RemoveEmitter(scene, handle, pos);
            int cnt = fn_Audio_Scene_GetEmitterCount ? fn_Audio_Scene_GetEmitterCount(scene, handle) : 0;
            if (cnt == 0 && fn_Audio_Scene_StopEffect3D) fn_Audio_Scene_StopEffect3D(scene, handle);
        }
        *reinterpret_cast<uint32_t*>(entry + 0) = 0;
        *reinterpret_cast<float*>(entry + 4) = vol;
        *curRef = soundRef;
        PlayNewSlot(comp, slot);
        return;
    }

    // Same sound, live handle — just adjust volume if it moved.
    if (*reinterpret_cast<float*>(entry + 4) == vol) return;
    *reinterpret_cast<float*>(entry + 4) = vol;
    void* scene = WorldScene();
    if (scene && fn_Audio_Scene_SetEmitterVolume)
        fn_Audio_Scene_SetEmitterVolume(scene, handle, const_cast<void*>(pos), vol);
}

inline float saturate(float x, float hi) { return x < 0.0f ? 0.0f : (x > hi ? hi : x); }

// Find the entity's AmbientEmitterComponent by scanning its component list.
// owner (self+0x10) -> +0x30 array of component IBPs (stride 8), +0x38 count.
void* FindSiblingEmitter(void* self) {
    if (!fn_InternalBasePtr_CheckPtr) return nullptr;
    fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(self) + 0x10));
    void* owner = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(self) + 0x10);
    if (!owner) return nullptr;
    int count = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(owner) + 0x38);
    void** list = *reinterpret_cast<void***>(reinterpret_cast<uintptr_t>(owner) + 0x30);
    if (!list || !s_tidEmitterComp) return nullptr;
    for (int i = 0; i < count; ++i) {
        void** ibp = &list[i];
        fn_InternalBasePtr_CheckPtr(ibp);
        void* comp = *ibp;
        if (!comp) continue;
        void** vt = *reinterpret_cast<void***>(comp);
        // component->GetTypeId() is vtable slot +0x10.
        uint32_t tid = reinterpret_cast<uint32_t(*)(void*)>(vt[2])(comp);
        if (tid == s_tidEmitterComp)
            return (fn_IsDerivedFrom && fn_IsDerivedFrom(comp, s_tidEmitterComp)) ? comp : nullptr;
    }
    return nullptr;
}

}  // namespace

// ============================================================
// AmbientEnvironmentComponent::Update (vtable slot [9], 0x140043310)
// ============================================================
// The component Update ABI is (self, Time*); the original reads only `self`.
void AmbientEnvironmentComponent_Update(AmbientEnvironmentComponent* selfC, int64_t* /*time*/) {
    // Opt-in: the Update vtable slot is only wired when BANISHED_AMBIENT_AUDIO
    // (or the global BANISHED_LIVE_COMPONENTS) is set, so reaching here already
    // means enabled. Secondary guard keeps a =0 override inert.
    static int s_enabled = -1;
    if (s_enabled < 0) {
        const char* e = std::getenv("BANISHED_AMBIENT_AUDIO");
        s_enabled = (e && (e[0] == '0' || e[0] == 'f' || e[0] == 'F')) ? 0 : 1;
    }
    if (!s_enabled || !selfC) return;

    EnsureResolved();

    static int s_dbg = -1;
    if (s_dbg < 0) { const char* e = std::getenv("BANISHED_AMBIENT_TRACE"); s_dbg = (e && e[0] && e[0] != '0') ? 1 : 0; }
    static int s_calls = 0;
    if (s_dbg && s_calls < 6) {
        ++s_calls;
        void* dr = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(selfC) + 0x18);
        fprintf(stderr, "[ambientenv] ENTER self=%p desc=%p tidEnv=%u tidEmC=%u tidEmD=%u\n",
                (void*)selfC, dr, s_tidEnvDesc, s_tidEmitterComp, s_tidEmitterDesc);
        fflush(stderr);
    }

    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;
    if (!fn_IsDerivedFrom || !s_tidEnvDesc || !fn_MemAlloc || !g_pMemAllocSingleton) return;

    uint8_t* self = reinterpret_cast<uint8_t*>(selfC);
    uint8_t* gw   = reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton);

    // Grid (Game object at gameWorld+0xf8): +0x00 width, +0x80 cells (stride 0x30).
    uint8_t* grid = *reinterpret_cast<uint8_t**>(gw + 0xf8);
    if (!grid) return;
    int gridWidth = *reinterpret_cast<int*>(grid + 0x00);
    uint8_t* cells = *reinterpret_cast<uint8_t**>(grid + 0x80);

    // Biome config description at self+0x18.
    void* descRaw = *reinterpret_cast<void**>(self + 0x18);
    void* desc = fn_IsDerivedFrom(descRaw, s_tidEnvDesc) ? descRaw : nullptr;
    if (!desc) return;
    int count = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(desc) + 0x28);
    uint8_t* table = *reinterpret_cast<uint8_t**>(reinterpret_cast<uintptr_t>(desc) + 0x20);
    if (count <= 0 || !table) return;

    int* hist = reinterpret_cast<int*>(fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)count * 4, 4));
    if (!hist) return;
    for (int i = 0; i < count; ++i) hist[i] = 0;

    int gridX = *reinterpret_cast<int*>(self + 0x28);
    int gridY = *reinterpret_cast<int*>(self + 0x2c);
    int rw    = *reinterpret_cast<int*>(self + 0x30);
    int rh    = *reinterpret_cast<int*>(self + 0x34);

    if (cells) {
        for (int y = 0; y < rh; ++y) {
            for (int x = 0; x < rw; ++x) {
                intptr_t ci = (intptr_t)(gridY + y) * gridWidth + (gridX + x);
                uint8_t* cell = cells + ci * 0x30;
                uint8_t bits = cell[0x29];

                // Cell's entity reference (cell+0x00, else cell+0x08).
                void* ent = *reinterpret_cast<void**>(cell + 0x00);
                if (!ent) ent = *reinterpret_cast<void**>(cell + 0x08);

                for (int k = 0; k < count; ++k) {
                    uint8_t* bt = table + (intptr_t)k * 0x28;
                    uint32_t mask = *reinterpret_cast<uint32_t*>(bt + 0x00);
                    if ((mask & (1u << (bits & 0x1f))) == 0) continue;
                    // Entity filter: no filter, or the cell entity matches bt+0x08.
                    void* filt = *reinterpret_cast<void**>(bt + 0x08);
                    bool match = (ent == nullptr) || (filt == nullptr);
                    if (!match) {
                        // ent+0x28 == filter target?
                        match = (*reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(ent) + 0x28) == filt);
                    }
                    if (match) {
                        hist[k] += *reinterpret_cast<int*>(bt + 0x20);
                        break;
                    }
                }
            }
        }
    }

    // Dominant biome = argmax(hist).
    int dominant = 0, best = hist[0];
    for (int k = 1; k < count; ++k) {
        if (hist[k] > best) { best = hist[k]; dominant = k; }
    }

    // Temperature crossfade factor: (temp - lo) / (hi - lo), clamped [0,1].
    float unit = g_fUVScale ? *g_fUVScale : 1.0f;
    void* weather = *reinterpret_cast<void**>(gw + 0x130);
    float f = 0.0f;
    if (weather) {
        float temp = *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(weather) + 0x18);
        float lo = *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(desc) + 0x30);
        float hi = *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(desc) + 0x34);
        float denom = hi - lo;
        f = saturate((denom != 0.0f) ? (temp - lo) / denom : 0.0f, unit);
    }

    // Drive the sibling AmbientEmitterComponent: slot 0 = day, slot 1 = night.
    void* emitter = FindSiblingEmitter(self);
    if (emitter) {
        uint64_t daySound   = *reinterpret_cast<uint64_t*>(table + (intptr_t)dominant * 0x28 + 0x10);
        uint64_t nightSound = *reinterpret_cast<uint64_t*>(table + (intptr_t)dominant * 0x28 + 0x18);
        SetSlot(emitter, 0, daySound,   f);
        SetSlot(emitter, 1, nightSound, unit - f);
    }

    static int s_trace = -1;
    if (s_trace < 0) {
        const char* e = std::getenv("BANISHED_AMBIENT_TRACE");
        s_trace = (e && e[0] && e[0] != '0') ? 1 : 0;
    }
    if (s_trace) {
        static int n = 0;
        if ((n++ % 120) == 0) {
            fprintf(stderr, "[ambientenv] region=(%d,%d,%d,%d) count=%d dominant=%d "
                    "f=%.3f emitter=%p cells=%p\n",
                    gridX, gridY, rw, rh, count, dominant, f, emitter, (void*)cells);
            fflush(stderr);
        }
    }

    if (fn_MemFreeStatic) fn_MemFreeStatic(hist);
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void AmbientEnvironmentComponent_InitFuncs() {
    EnsureResolved();
}

// ===== Standalone factory (Foundation D — component instantiation) =====
#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"
// Route the per-tick Update slot (vtable +0x48 / index 9) to the biome-ambient
// reimpl when BANISHED_AMBIENT_AUDIO (or the global BANISHED_LIVE_COMPONENTS) is
// set — default ON via the env check inside Update. The factory opt-in below uses
// the same env so the vtable slot is only wired when the feature is enabled.
COMPONENT_LIVE_FACTORY(AmbientEnvironmentComponent, 0x48, 0x88,
                       "BANISHED_AMBIENT_AUDIO", AmbientEnvironmentComponent_Update)
REGISTER_TYPE(AmbientEnvironmentComponent, "Component",
              &AmbientEnvironmentComponent_Factory, &AmbientEnvironmentComponent_CreateInPlace);
#endif // STANDALONE_EXE
