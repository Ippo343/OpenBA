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

#include "Component/ambientenvironmentcomponent_funcs.h"
#include "Component/AmbientEnvironmentDescription.h"   // B.3.8 CB1a (TypeId)
#include "Component/AmbientEmitterDescription.h"   // B.3.8 CB1a (TypeId)
#include "Component/AmbientEmitterComponent.h"   // B.3.8 CB1a (TypeId)
#include "Entity/ComponentEntity.h"
#include "perf_experimental.h"
#include <unordered_map>
#include <new>
#include "math/Math.h"
#include "system/Value.h"
#include "object/Internal.h"
#include "object/TypeInfo.h"
#include "Component/exe_bodies.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "Game/exe_constants.h"   // kRdata_* (B3R Leftovers: the .rdata constants)
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include "Object/factory_registration.h"   // Object::GetTypeIdByName
#include "system/MemoryAllocator.h"

namespace {

// Type-id values (CRC32 of the type name; populated after Engine::Startup).
uint32_t s_tidEnvDesc     = 0;  // AmbientEnvironmentDescription (0x1401c3f08)
uint32_t s_tidEmitterComp = 0;  // AmbientEmitterComponent       (0x1401c3e60)
bool s_resolved = false;

void EnsureResolved() {
    if (s_resolved) return;
    s_tidEnvDesc     = AmbientEnvironmentDescription::TypeId();
    s_tidEmitterComp = AmbientEmitterComponent::TypeId();
    // Only latch as resolved once the factory registry has frozen the ids
    // (they start at the 0xFFFFFFFF sentinel before Factory::Startup).
    if (s_tidEnvDesc != 0 && s_tidEnvDesc != 0xFFFFFFFFu)
        s_resolved = true;
}

inline float saturate(float x, float hi) { return x < 0.0f ? 0.0f : (x > hi ? hi : x); }

// Find the entity's AmbientEmitterComponent by scanning its component list.
// The tree's null and bad-id guards are its own; kept, not extended.
AmbientEmitterComponent* FindSiblingEmitter(AmbientEnvironmentComponent* self) {
    ComponentEntity* owner = self->_entity;
    if (!owner) return nullptr;
    int count = owner->_componentCount;
    if (!owner->_components || !s_tidEmitterComp) return nullptr;
    for (int i = 0; i < count; ++i) {
        Component* comp = owner->_components[i].Get();
        if (!comp) continue;
        uint32_t tid = comp->Type();   // slot 2
        if (tid == s_tidEmitterComp)
            return (::Object::IsDerivedFromNullSafe(comp, s_tidEmitterComp)) ? static_cast<AmbientEmitterComponent*>(comp) : nullptr;
    }
    return nullptr;
}

// The tile grid (the Game object at gameWorld+0xf8): +0x00 width, +0x80 cells.
// LEFT: GameUpdateLoop::m_game is void* (W0 header): the grid reads stay by offset.
inline uint8_t* TileGrid() {
    return reinterpret_cast<uint8_t*>(GameUpdateLoop::_singleton->m_game);
}

}  // namespace

// ============================================================
// AmbientEnvironmentComponent::InitRegion
// (region/position half of impl_36a0, exe 0x1400436a0)
// ============================================================
// impl_36a0(self, gridX, gridY, rw, rh):
//   self+0x28=gridX  self+0x2c=gridY  self+0x30=rw  self+0x34=rh
//   grid   = *(gameWorld+0xf8);  gridWidth = *(int*)grid
//   center = (gridY + rh/2)*gridWidth + gridX + rw/2       // center tile index
//   off    = (float)(gridWidth/2) - 0.5f                    // DAT_140172114 == 0.5f
//   pos    = ( center%gridWidth - off, center/gridWidth - off, 0 )
//   self+0x38 = pos    // the original samples terrain height into z, then
//                      // immediately overwrites z with 0 — so pos.z is always 0.
//   FUN_140042960(siblingEmitter, self+0x38)  // push pos onto the emitter (+0x28)
//   Update(self)                              // the per-frame slot already runs
//                                             // this every tick, so we skip it here
//                                             // to avoid seeding play before OnAdded.
void AmbientEnvironmentComponent::InitRegion(int gridX, int gridY, int rw, int rh) {
    EnsureResolved();

    regionX = gridX;
    regionY = gridY;
    regionWidth = rw;
    regionHeight = rh;

    if (GameUpdateLoop::_singleton == nullptr) return;
    uint8_t* grid = TileGrid();
    if (!grid) return;
    int gridWidth = *reinterpret_cast<int*>(grid + 0x00);
    if (gridWidth <= 0) return;

    long long center = (long long)(gridY + (rh >> 1)) * gridWidth + gridX + (rw >> 1);
    float off = (float)(gridWidth >> 1) - 0.5f;
    float worldX = (float)(center % gridWidth) - off;
    float worldY = (float)(center / gridWidth) - off;

    // Env component position (comp+0x38 Vector3), z forced to 0 as in the original.
    position.x = worldX;
    position.y = worldY;
    position.z = 0.0f;

    // Push the position onto the sibling AmbientEmitterComponent (+0x28 Vector3).
    // At spawn the emitter has no live handles yet (OnAdded runs at register time,
    // after this), so FUN_140042960's MoveEmitter loop is a no-op and it only
    // writes the position — which is all we do here.
    AmbientEmitterComponent* emitter = FindSiblingEmitter(this);
    if (emitter) {
        emitter->position.x = worldX;
        emitter->position.y = worldY;
        emitter->position.z = 0.0f;
    }
}

// B4 FORWARD: 1 outside caller (code/Game/Game/starttown.cpp)
void AmbientEnvironmentComponent_InitRegion(AmbientEnvironmentComponent* selfC,
                                            int gridX, int gridY, int rw, int rh) {
    if (!selfC) return;
    selfC->InitRegion(gridX, gridY, rw, rh);
}

// ============================================================================
// ENDGAME B.3.8 CIT — AmbientEnvironmentComponent as a REAL class
// (AmbientEnvironmentComponent.h).
//
// Before: a 48-slot hand-built AmbientEnvironmentComponent_LiveVtable filled at
// every create (slot 3 the serializer, slot 9 the biome-ambient Update),
// Inert_ScalarDtor in slot 0. After: the compiler's 16-slot vtable and a real
// ctor/dtor chain (B38_RULES D1, D2).
// ============================================================================
#include "Object/factory_registration.h"
#include "Component/AmbientEnvironmentComponent.h"


// 0x1400438d0. Component() is the CALL 0x1401188b0; the 0x140158b70 stamp is
// the compiler's; +0x38 = 0, +0x40 (dword) = 0; flags |= 0x88. Retail leaves
// +0x28/+0x30 unwritten; they are zeroed as the tree's factory did.
AmbientEnvironmentComponent::AmbientEnvironmentComponent()
    : regionX(0), regionY(0), regionWidth(0), regionHeight(0), position(), _pad44{} {
    OrFlagsInCtor(0x88);
}


// [9] 0x140043690 (`JMP 0x140043310`). RDX is a pointer to the elapsed
// System::Time (EntityList bucket tick 0x1400c42af).
// ============================================================
// AmbientEnvironmentComponent::Update (vtable slot [9], 0x140043310)
// ============================================================
// The component Update ABI is (self, Time*); the original reads only `self`.
// The per-emitter biome-scan throttle below (main thread only: slot 9 is the
// EntityList update walk). Namespace scope, not a function-local static (B6-5).
struct AmbientThrottleState { int dominant; int countdown; };
static std::unordered_map<void*, AmbientThrottleState> s_ambientThrottle;

void AmbientEnvironmentComponent::Update(void* /*time*/) {

    EnsureResolved();

    if (GameUpdateLoop::_singleton == nullptr) return;
    if (!s_tidEnvDesc || !::System::MemoryAllocator::_singleton) return;

    // Grid (Game object at gameWorld+0xf8): +0x00 width, +0x80 cells (stride 0x30).
    uint8_t* grid = TileGrid();
    if (!grid) return;
    int gridWidth = *reinterpret_cast<int*>(grid + 0x00);
    uint8_t* cells = *reinterpret_cast<uint8_t**>(grid + 0x80);

    // Biome config description at +0x18.
    Description* descRaw = _description;
    AmbientEnvironmentDescription* desc = ::Object::IsDerivedFromNullSafe(descRaw, s_tidEnvDesc)
                                              ? static_cast<AmbientEnvironmentDescription*>(descRaw) : nullptr;
    if (!desc) return;
    int count = desc->_entryCount;
    AmbientEnvironmentBiome* table = desc->_entries;
    if (count <= 0 || !table) return;

    // [PerfX AMBIENT_THROTTLE -- NON-RETAIL] the biome histogram over the emitter's tile
    // region only changes when the map does (trees cut, buildings placed) and the audio
    // crossfade it feeds is inaudible at tick granularity, so the scan is re-run every
    // n-th tick per emitter and the last dominant biome is reused in between. The
    // temperature crossfade factor is still recomputed every tick.
    auto& s_throttle = s_ambientThrottle;
    const int throttleN = PerfX::AmbientThrottle();
    int cachedDominant = -1;
    if (throttleN > 1) {
        auto it = s_throttle.find(this);
        if (it != s_throttle.end() && it->second.countdown > 0) {
            --it->second.countdown;
            cachedDominant = it->second.dominant;
            PerfX::Count(PerfX::kCntAmbientSkipped);
        }
    }

    int gridX = regionX;
    int gridY = regionY;
    int rw    = regionWidth;
    int rh    = regionHeight;

    int* hist = nullptr;
    if (cachedDominant < 0) {
    hist = static_cast<int*>(::System::MemoryAllocator::MemAlloc((uint64_t)count * 4, 4));
    if (!hist) return;
    for (int i = 0; i < count; ++i) hist[i] = 0;

    if (cells) {
        for (int y = 0; y < rh; ++y) {
            for (int x = 0; x < rw; ++x) {
                intptr_t ci = (intptr_t)(gridY + y) * gridWidth + (gridX + x);
                uint8_t* cell = cells + ci * 0x30;
                uint8_t bits = cell[0x29];

                // Cell's entity reference (cell+0x00, else cell+0x08).
                ComponentEntity* ent = *reinterpret_cast<ComponentEntity**>(cell + 0x00);
                if (!ent) ent = *reinterpret_cast<ComponentEntity**>(cell + 0x08);

                for (int k = 0; k < count; ++k) {
                    AmbientEnvironmentBiome* bt = &table[k];
                    uint32_t mask = bt->terrainMask;
                    if ((mask & (1u << (bits & 0x1f))) == 0) continue;
                    // Entity filter: no filter, or the cell entity matches bt+0x08.
                    ComponentDescription* filt = bt->entityFilter;
                    bool match = (ent == nullptr) || (filt == nullptr);
                    if (!match) {
                        // ent+0x28 == filter target?
                        match = (ent->_description == filt);
                    }
                    if (match) {
                        hist[k] += bt->weight;
                        break;
                    }
                }
            }
        }
    }

    } // cachedDominant < 0

    // Dominant biome = argmax(hist).
    int dominant = 0;
    if (cachedDominant >= 0 && cachedDominant < count) {
        dominant = cachedDominant;
    } else {
        int best = hist[0];
        for (int k = 1; k < count; ++k) {
            if (hist[k] > best) { best = hist[k]; dominant = k; }
        }
        if (throttleN > 1) s_throttle[this] = AmbientThrottleState{ dominant, throttleN - 1 };
    }

    // Temperature crossfade factor: (temp - lo) / (hi - lo), clamped [0,1].
    // LEFT: GameUpdateLoop::m_weather is void* (W0 header): the temperature read stays by offset.
    float unit = kRdata_14017214c;
    void* weather = GameUpdateLoop::_singleton->m_weather;
    float f = 0.0f;
    if (weather) {
        float temp = *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(weather) + 0x18);
        float lo = desc->_f30;
        float hi = desc->_f34;
        float denom = hi - lo;
        f = saturate((denom != 0.0f) ? (temp - lo) / denom : 0.0f, unit);
    }

    // Drive the sibling AmbientEmitterComponent: slot 0 = day, slot 1 = night.
    AmbientEmitterComponent* emitter = FindSiblingEmitter(this);
    if (emitter) {
        Audio::SoundEffect* daySound   = table[dominant].daySound;
        Audio::SoundEffect* nightSound = table[dominant].nightSound;
        emitter->SetSlot(0, daySound,   f);
        emitter->SetSlot(1, nightSound, unit - f);
    }
    if (hist) ::System::MemoryAllocator::MemFree(hist);
}

// create 0x140001ee0: MemAlloc(0x48, 8), then the ctor.
void* AmbientEnvironmentComponent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(AmbientEnvironmentComponent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) AmbientEnvironmentComponent());
}
// Retail's createInPlace is `Internal::CreateInPlaceObject` (returns null); the
// tree's in-place entry built the object, and that is kept (B38 B8-13).
void* AmbientEnvironmentComponent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) AmbientEnvironmentComponent());
}

DEFINE_EXE_TYPE(AmbientEnvironmentComponent, "Component")
