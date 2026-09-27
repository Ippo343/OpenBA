// code/Game/Component/firecomponent_ignite.cpp
// The fire mechanic's leaf bodies — everything FireComponent::Update dispatches
// and everything the citizen bucket-brigade lands on.
//
//   0x140049610  FireComponent_Ignite            start a fire + MINT the FireWork
//   0x140049130  FireComponent_SpreadToNeighbors roll fire onto an adjacent building
//   0x1400494c0  FireComponent_Extinguish        put it out, tear the work down
//   0x140049090  FireComponent_Slot15            vtable +0x78: ONE bucket applied
//   0x1400499c0  FireComponent_sub_99c0          add/remove the fire-danger rect
//   0x14009fa80  FUN_14009fa80                   the rect stamp itself
//   (0x140053720 ParticleComponent fade-OUT and 0x140042960 audio-emitter move are
//    the TORNADO lane's ports — resolved here, not duplicated; see below.)
//   0x140042c60  FUN_140042c60                   start ONE audio emitter
//   0x1400e9bd0  FireWork_Ctor(mem, entityIBP, capacity)
//
// ── How the loop closes ──────────────────────────────────────────────────────
// Ignite mints a FireWork (0x1400e9bd0) and POSTS it to the work-reservation
// ledger via CitizenAI_impl_ec60(workComp, 0x4000, 0, &workIBP). There is no
// CreateWork slot-18 factory anywhere in this lane — xrefs prove Ignite is the
// ONLY caller of the FireWork ctor. A citizen taking the job runs
// FireWork::Execute (slot 4), whose action queue ends in a SendComponentMessage
// addressed to the BURNING entity with msgId = *DAT_1401c41e0 (the FireComponent
// type id). SendComponentMessage::AbstractImpl dispatches that to the matching
// component's vtable +0x78 — i.e. FireComponent_Slot15 below, which subtracts one
// bucket's worth of Time from the fire timer.
//
// ⚠ That message id is the reason the lane was inert even where it was ported:
// 0x1401c41e0 was never registered, so work_act_perform.cpp read msgId = 0, the
// dispatcher matched no component, and the water silently went nowhere. It is
// registered by standalone/reg_fire.cpp now.
//
// Faithful, offset-by-offset ports of the Ghidra decompilations. Every constant,
// every field offset and the order of every call were transcribed from the
// listings; do not "improve" the behaviour.

#include "Work/firework_funcs.h"
#include "Work/FireWork.h"   // B.3.8 CB1a (Vtable)
#include "Component/FireDescription.h"   // B.3.8 CB1a (TypeId)
#include "Component/AmbientEmitterDescription.h"   // B.3.8 CB1a (TypeId)
#include "Component/firecomponent_funcs.h"
#include "Object/factory_registration.h"
#include "engine/types/FireComponent.h"   // the forwarder: also carries FireDescriptionField
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "system/Value.h"   // ENDGAME B.3.4b System caller batch
#include "audio/Audio.h"     // ENDGAME B.3.7 callers (K4)
#include "Component/FireComponent.h"

// EventLog::AddEvent (0x140095580) — ported by the tornado lane (eventlog.cpp).
extern "C" void EventLog_AddEvent(void* eventLog, int eventType, int severity, void* entityIBP);

// Shared with the tornado lane — ported once in firecomponent_smstate.cpp.
extern "C" bool FireComponent_IsBeingDemolished_11e690(void* destroyComp);
extern "C" bool FireComponent_BeginBurning_11e7b0(void* destroyComp, const int64_t* burnTime);
extern "C" void FireComponent_StopBurning_11ea10(void* destroyComp);

// ============================================================
// Resolved dependencies
// ============================================================
namespace {

typedef void* (*fn_Find_t)(uintptr_t entity, void* retIBP);
typedef void  (*fn_PostWork_t)(uintptr_t workComp, uint32_t code, char flag, void* workIBP);
typedef void  (*fn_RemovalCalc_t)(int64_t townMgr, int id);
typedef void  (*fn_StopSlot_t)(uintptr_t workComp);
typedef void  (*fn_ParticleFade_t)(void* particleComp, int emitterIdx);
typedef void  (*fn_AudioMove_t)(void* ambientComp, void* newPos);
typedef void* (*fn_WorkCtor25_t)(void* work, void* pEntityIBP);
typedef bool  (*fn_Gate11e710_t)(void* destroyComp);

fn_Find_t         s_findDestroy   = nullptr;  // 0x140005780 DestroyComponent
fn_Find_t         s_findParticle  = nullptr;  // 0x140005200 ParticleComponent
fn_Find_t         s_findWorkComp  = nullptr;  // 0x140003050 WorkComponent
fn_Find_t         s_findMapComp   = nullptr;  // 0x140004680 MapComponent
fn_Find_t         s_findAmbient   = nullptr;  // 0x140002280 AmbientEmitterComponent
fn_Find_t         s_findXform     = nullptr;  // 0x140003f60 ModelComponent (world transform)
fn_PostWork_t     s_postWork      = nullptr;  // 0x14012ec60
fn_RemovalCalc_t  s_removalCalc   = nullptr;  // 0x1400f9ad0
fn_StopSlot_t     s_stopSlot      = nullptr;  // 0x14012eb50
fn_ParticleFade_t s_particleFadeIn  = nullptr;// 0x140053660
fn_ParticleFade_t s_particleFadeOut = nullptr;// 0x140053720 (tornado lane's port)
fn_AudioMove_t    s_audioMove       = nullptr;// 0x140042960 (tornado lane's port)
fn_WorkCtor25_t   s_workCtor25    = nullptr;  // 0x1400f72e0
fn_Gate11e710_t   s_gate11e710    = nullptr;  // 0x14011e710

const float* s_pK300 = nullptr;   // DAT_140172234 = 300.0f  (seconds-per-unit scale)
const float* s_pHalf = nullptr;   // DAT_140172114 = 0.5f

bool s_init = false;

inline float K300() { return s_pK300 ? *s_pK300 : 300.0f; }
inline float Half() { return s_pHalf ? *s_pHalf : 0.5f;   }

void EnsureInit() {
    if (s_init) return;
    s_findDestroy    = reinterpret_cast<fn_Find_t>(ResolveAddr(Module::Exe, 0x140005780));
    s_findParticle   = reinterpret_cast<fn_Find_t>(ResolveAddr(Module::Exe, 0x140005200));
    s_findWorkComp   = reinterpret_cast<fn_Find_t>(ResolveAddr(Module::Exe, 0x140003050));
    s_findMapComp    = reinterpret_cast<fn_Find_t>(ResolveAddr(Module::Exe, 0x140004680));
    s_findAmbient    = reinterpret_cast<fn_Find_t>(ResolveAddr(Module::Exe, 0x140002280));
    s_findXform      = reinterpret_cast<fn_Find_t>(ResolveAddr(Module::Exe, 0x140003f60));
    s_postWork       = reinterpret_cast<fn_PostWork_t>(ResolveAddr(Module::Exe, 0x14012ec60));
    s_removalCalc    = reinterpret_cast<fn_RemovalCalc_t>(ResolveAddr(Module::Exe, 0x1400f9ad0));
    s_stopSlot       = reinterpret_cast<fn_StopSlot_t>(ResolveAddr(Module::Exe, 0x14012eb50));
    s_particleFadeIn = reinterpret_cast<fn_ParticleFade_t>(ResolveAddr(Module::Exe, 0x140053660));
    s_particleFadeOut= reinterpret_cast<fn_ParticleFade_t>(ResolveAddr(Module::Exe, 0x140053720));
    s_audioMove      = reinterpret_cast<fn_AudioMove_t>(ResolveAddr(Module::Exe, 0x140042960));
    s_workCtor25     = reinterpret_cast<fn_WorkCtor25_t>(ResolveAddr(Module::Exe, 0x1400f72e0));
    s_gate11e710     = reinterpret_cast<fn_Gate11e710_t>(ResolveAddr(Module::Exe, 0x14011e710));
    s_pK300 = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x140172234));
    s_pHalf = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x140172114));
    s_init = true;
}

inline uintptr_t GameWorld() {
    return (g_pGameWorldSingleton && *g_pGameWorldSingleton)
         ? reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) : 0;
}

inline int64_t MakeTime(float seconds) {
    int64_t t = 0;
    if (fn_Time_Ctor_Float) fn_Time_Ctor_Float(&t, seconds);
    return t;
}

}  // namespace

// ============================================================
// Type ids this lane reads out of exe .rdata.
// ============================================================

// The +0x18 description, validated the way retail validates it.
//
// Retail: `obj = this->_unk18; desc = IsDerivedFrom(obj, FireDescTid) ? obj : 0;`
// and then dereferences `desc` UNCONDITIONALLY. We keep the unconditional deref —
// the earlier `if (desc)` guard here was the bug, not the fix: with the type-id
// global unregistered it silently deleted the whole ignition roll.
//
// `IsDerivedFromOrTrust` rather than raw `fn_IsDerivedFrom` for the same reason the
// sibling components use it (particlecomponent.cpp:126): the standalone factory
// tree can false-NEGATIVE on a description built through our own CreateInPlace
// cascade. Trusting a non-null +0x18 reproduces retail's effective behaviour —
// retail always has the real description here — instead of substituting a null
// that retail can never produce.
static inline void* FireDesc(const void* self) {
    void* obj = *(void**)((uintptr_t)self + 0x18);
    if (!obj) return nullptr;
    return ::Object::IsDerivedFromOrTrust(obj, FireDescription::TypeId()) ? obj : nullptr;
}

// ============================================================
// FUN_14009fa80 — stamp a value onto the fire-danger byte of a cell rect.
// ============================================================
// grid[0x20] (byte 0x80) is the fine-cell array, stride 0x30; +0x22 is the
// per-cell fire-danger counter, and grid byte 0x9c (grid[0x27] as int16) is the
// town-wide count of active fires. `delta` is +1 on ignite, -1 on extinguish.
// param_2 in the listing is dead (the caller passes yMax twice).
extern "C" void FireComp_StampDangerRect_9fa80(int* grid, uint64_t /*unused*/,
                                               int xMin, int yMin,
                                               int xMax, int yMax, short delta) {
    if (!grid) return;
    if (yMin <= yMax) {
        for (int y = yMin; y <= yMax; ++y) {
            for (int x = xMin; x <= xMax; ++x) {
                const int w = *grid;
                if (x >= 0 && y >= 0 && x < w && y < w) {
                    char* cell = reinterpret_cast<char*>(
                        static_cast<int64_t>(w * y + x) * 0x30 +
                        *reinterpret_cast<int64_t*>(reinterpret_cast<uintptr_t>(grid) + 0x80) +
                        0x22);
                    *cell = static_cast<char>(*cell + static_cast<char>(delta));
                }
            }
        }
    }
    int16_t* activeFires =
        reinterpret_cast<int16_t*>(reinterpret_cast<uintptr_t>(grid) + 0x9c);
    *activeFires = static_cast<int16_t>(*activeFires + delta);
}

// ============================================================
// FireComponent_sub_99c0 (0x1400499c0) — add (+1) / remove (-1) the danger rect.
// ============================================================
extern "C" void FireComp_MarkDanger_99c0(void* self, short delta) {
    EnsureInit();
    void* desc = FireDesc(self);
    if (!s_findMapComp) return;

    void* ibpSlot = reinterpret_cast<uint8_t*>(self) + 0x10;
    uintptr_t mapIbp = 0;
    s_findMapComp(*reinterpret_cast<uintptr_t*>(ibpSlot), &mapIbp);

    const int r = *reinterpret_cast<int*>(
        reinterpret_cast<uintptr_t>(desc) + FireDescriptionField::DangerRadius);
    const int16_t ox = *reinterpret_cast<int16_t*>(mapIbp + 0x28);
    const int16_t oy = *reinterpret_cast<int16_t*>(mapIbp + 0x2a);
    const int16_t w  = *reinterpret_cast<int16_t*>(mapIbp + 0x2c);
    const int16_t h  = *reinterpret_cast<int16_t*>(mapIbp + 0x2e);
    const uint32_t yMax = static_cast<uint32_t>(oy - 1 + h + r);

    const uintptr_t gw = GameWorld();
    if (!gw) return;
    int* grid = *reinterpret_cast<int**>(gw + 0xf8);
    FireComp_StampDangerRect_9fa80(grid, yMax, ox - r, oy - r, ox - 1 + w + r,
                                   static_cast<int>(yMax), delta);
}

// ── 0x140053720 (ParticleComponent fade-out) and 0x140042960 (move this entity's
// audio emitters) are NOT ported here. The TORNADO lane ported and registered both
// first (tornadocomponent.cpp:977 / :1002, standalone/reg_tornado.cpp:80-81) and
// its damage arm calls them for the same reason this lane does. Resolving them
// through ResolveAddr keeps ONE body per retail function
// (reference_one_retail_fn_ported_twice_diverges) — writing a second copy here
// would have been exactly that failure mode, with each copy faithful in a
// different half.

// ============================================================
// FUN_140042c60 — start ONE audio emitter (index `idx`) on this component.
// ============================================================
// Entry {+0x00 uint handle, +0x04 float volume, +0x08 ExternalPtr sound}, stride
// 0x10; the sound ref is lazily copied from the AmbientEmitterDescription's
// (+0x28) sound array. SoundType 3 = ambient looping.
extern "C" void FireComp_StartAudioEmitter_42c60(void* ambientComp, int idx) {
    if (!ambientComp) return;
    uintptr_t a = reinterpret_cast<uintptr_t>(ambientComp);
    const uintptr_t gw = GameWorld();
    if (!gw) return;
    void* scene = *reinterpret_cast<void**>(gw + 0x78);
    if (!scene) return;

    void* obj = *reinterpret_cast<void**>(a + 0x18);
    static uint32_t s_tidEmitterDesc = 0;
    if (s_tidEmitterDesc == 0 || s_tidEmitterDesc == 0xFFFFFFFFu)
        s_tidEmitterDesc = AmbientEmitterDescription::TypeId();
    void* desc = (obj && ::Object::IsDerivedFromOrTrust(obj, s_tidEmitterDesc)) ? obj : nullptr;

    int64_t base = *reinterpret_cast<int64_t*>(a + 0x38);
    if (!base) return;
    uint32_t* entry = reinterpret_cast<uint32_t*>(base + static_cast<int64_t>(idx) * 0x10);
    uint64_t* soundRef = reinterpret_cast<uint64_t*>(entry + 2);

    if (*soundRef == 0 && desc) {
        int64_t soundArr = *reinterpret_cast<int64_t*>(reinterpret_cast<uintptr_t>(desc) + 0x28);
        if (soundArr)
            *soundRef = *reinterpret_cast<uint64_t*>(soundArr + static_cast<int64_t>(idx) * 8);
    }

    uint32_t handle = reinterpret_cast<Audio::Scene*>(scene)->FindEffect3D(
        reinterpret_cast<Audio::SoundEffect*>(*soundRef));   // B.3.7 bridge
    *entry = handle;
    if (handle == 0) {
        int64_t t0 = MakeTime(0.0f);
        float   posZero[3] = { 0.0f, 0.0f, 0.0f };
        handle = reinterpret_cast<Audio::Scene*>(scene)->PlayEffect3D(
            reinterpret_cast<Audio::SoundEffect*>(*soundRef), static_cast<Audio::Scene::SoundType>(3),
            *reinterpret_cast<const System::Time*>(&t0),
            *reinterpret_cast<const Vector3*>(posZero));   // B.3.7 bridge
        *entry = handle;
    }
    reinterpret_cast<Audio::Scene*>(scene)->AddEmitter(
        *entry, *reinterpret_cast<const Vector3*>(a + 0x28),
        *reinterpret_cast<float*>(entry + 1));   // B.3.7 bridge
}

// ============================================================
// FireWork_Ctor (0x1400e9bd0) — the 3-arg ctor. The ONLY producer of a FireWork.
// ============================================================
extern "C" void* FireWork_CtorFull_e9bd0(void* mem, uint64_t* pEntityIBP, uint32_t capacity) {
    EnsureInit();
    // The Work's own leaf pointers + type-id globals are resolved by
    // FireWork_ResolveFuncs(), which is called ONLY from FireWork_Factory /
    // FireWork_CreateInPlace. Retail reaches a FireWork through the factory
    // cascade; WE do not — Ignite mints one here directly, which is the only
    // producer of a FireWork in the whole exe (0x1400e9bd0 has exactly one
    // caller). Without this call every s_* in firework.cpp stays null and
    // FireWork::Execute faults on *s_TypeId_CitizenComponent (READ of 0x0) the
    // first time a citizen picks the job up. This is the documented
    // "CreateWork must call the Work's *_ResolveFuncs()" rule — the factory
    // path is not the only construction path.
    FireWork_ResolveFuncs();
    if (!mem || !s_workCtor25) return nullptr;
    uint64_t local = pEntityIBP ? *pEntityIBP : 0;
    s_workCtor25(mem, &local);
    uintptr_t w = reinterpret_cast<uintptr_t>(mem);
    // The Work base ctor installs vtable_Work; retype to FireWork's.
    uintptr_t vt = reinterpret_cast<uintptr_t>(FireWork::Vtable());   // B.3.8: the compiler vtable (same bytes, B8-5)
    if (vt) *reinterpret_cast<void**>(w) = reinterpret_cast<void*>(vt);
    *reinterpret_cast<uint32_t*>(w + 0x48) = capacity;   // max firefighters
    *reinterpret_cast<uint64_t*>(w + 0x50) = 0;          // assignment BST root
    *reinterpret_cast<uint32_t*>(w + 0x58) = 0;          // assigned count
    return mem;
}

// ============================================================
// FireComponent_Extinguish (0x1400494c0)
// ============================================================
extern "C" void FireComponent_Extinguish_4c0(void* self) {
    EnsureInit();
    uintptr_t s = reinterpret_cast<uintptr_t>(self);
    void* ibpSlot = reinterpret_cast<void*>(s + 0x10);

    *reinterpret_cast<int64_t*>(s + 0x30) = 0;
    *reinterpret_cast<int64_t*>(s + 0x38) = 0;
    void* desc = FireDesc(self);

    // ParticleComponent: fade the flames out.
    if (s_findParticle) {
        uintptr_t partIbp = 0;
        s_findParticle(*reinterpret_cast<uintptr_t*>(ibpSlot), &partIbp);
        if (s_particleFadeOut)
            s_particleFadeOut(reinterpret_cast<void*>(partIbp),
                              *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(desc) +
                                                      FireDescriptionField::ParticleEmitter));
    }

    // DestroyComponent: leave the BURNING state.
    if (s_findDestroy) {
        uintptr_t destIbp = 0;
        s_findDestroy(*reinterpret_cast<uintptr_t*>(ibpSlot), &destIbp);
        FireComponent_StopBurning_11ea10(reinterpret_cast<void*>(destIbp));
    }

    // WorkComponent: release the firefighting work slot (this is what retires the
    // FireWork Ignite posted).
    if (s_findWorkComp && s_stopSlot) {
        uintptr_t workIbp = 0;
        s_findWorkComp(*reinterpret_cast<uintptr_t*>(ibpSlot), &workIbp);
        s_stopSlot(workIbp);
    }

    // AmbientEmitterComponent: stop the crackle.
    if (s_findAmbient) {
        uintptr_t ambIbp = 0;
        s_findAmbient(*reinterpret_cast<uintptr_t*>(ibpSlot), &ambIbp);
        int64_t base = ambIbp ? *reinterpret_cast<int64_t*>(ambIbp + 0x38) : 0;
        const uintptr_t gw = GameWorld();
        if (base && gw) {
            uint32_t* handle = reinterpret_cast<uint32_t*>(
                static_cast<int64_t>(*reinterpret_cast<int*>(
                    reinterpret_cast<uintptr_t>(desc) + FireDescriptionField::AudioEmitter)) * 0x10
                + base);
            void* scene = *reinterpret_cast<void**>(gw + 0x78);
            void* pos = reinterpret_cast<void*>(ambIbp + 0x28);
            if (scene) {
                reinterpret_cast<Audio::Scene*>(scene)->RemoveEmitter(
                    *handle, *reinterpret_cast<const Vector3*>(pos));   // B.3.7 bridge
                if (reinterpret_cast<Audio::Scene*>(scene)->GetEmitterCount(*handle) == 0)   // B.3.7 bridge
                    reinterpret_cast<Audio::Scene*>(scene)->StopEffect3D(*handle);   // B.3.7 bridge
            }
            *handle = 0;
        }
    }

    FireComp_MarkDanger_99c0(self, -1);

    // OUTCOME trace — say which of the two Slot15/Update conditions brought us here.
}

// ============================================================
// FireComponent_Ignite (0x140049610)
// ============================================================
extern "C" void FireComponent_Ignite_610(void* self) {
    EnsureInit();
    if (!self) return;
    uintptr_t s = reinterpret_cast<uintptr_t>(self);
    void* ibpSlot = reinterpret_cast<void*>(s + 0x10);
    const uintptr_t gw = GameWorld();
    if (!gw || !s_findDestroy) return;

    // --- the two abort gates, on the DestroyComponent ---
    uintptr_t destIbp = 0;
    s_findDestroy(*reinterpret_cast<uintptr_t*>(ibpSlot), &destIbp);
    void* destroyComp = reinterpret_cast<void*>(destIbp);

    if (FireComponent_IsBeingDemolished_11e690(destroyComp)) {
        return;
    }
    if (s_gate11e710 && s_gate11e710(destroyComp)) {
        return;
    }

    void* desc = FireDesc(self);
    const uintptr_t d = reinterpret_cast<uintptr_t>(desc);

    // Burn duration = Time(BurnScale * 300 + BurnBase); the DestroyComponent gets
    // HALF of it (its own damage clock).
    const int64_t burnFull = MakeTime(
        *reinterpret_cast<float*>(d + FireDescriptionField::BurnScale) * K300() +
        *reinterpret_cast<float*>(d + FireDescriptionField::BurnBase));
    const float   burnSecs = reinterpret_cast<const System::Time*>(const_cast<int64_t*>(&burnFull))->AsSeconds();   // B.3.4b bridge
    const int64_t burnHalf = MakeTime(burnSecs * Half());

    if (!FireComponent_BeginBurning_11e7b0(destroyComp, &burnHalf)) {
        return;
    }

    *reinterpret_cast<int64_t*>(s + 0x30) = burnFull;
    *reinterpret_cast<int64_t*>(s + 0x38) =
        MakeTime(*reinterpret_cast<float*>(d + FireDescriptionField::SpreadInterval));
    *reinterpret_cast<int64_t*>(s + 0x28) = 0;      // bucketsApplied + bucketsAtSpread

    // ParticleComponent: fade the flames in.
    if (s_findParticle && s_particleFadeIn) {
        uintptr_t partIbp = 0;
        s_findParticle(*reinterpret_cast<uintptr_t*>(ibpSlot), &partIbp);
        s_particleFadeIn(reinterpret_cast<void*>(partIbp),
                         *reinterpret_cast<int*>(d + FireDescriptionField::ParticleEmitter));
    }

    // --- MINT the FireWork and POST it to the reservation ledger ---
    uintptr_t workCompIbp = 0;
    if (s_findWorkComp) {
        s_findWorkComp(*reinterpret_cast<uintptr_t*>(ibpSlot), &workCompIbp);
    }

    void* work = nullptr;
    if (fn_MemAlloc && g_pMemAllocSingleton) {
        void* mem = fn_MemAlloc(g_pMemAllocSingleton, 0x60, 8);
        if (mem) {
            uint64_t entityIbp = *reinterpret_cast<uint64_t*>(s + 0x10);
            work = FireWork_CtorFull_e9bd0(
                mem, &entityIbp,
                *reinterpret_cast<uint32_t*>(d + FireDescriptionField::MaxFirefighters));
        }
    }

    uint64_t workIbp = 0;
    if (fn_InternalBasePtr_Ctor) new (&workIbp) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(work));   // B.3.3 bridge

    if (s_postWork && workCompIbp) {
        s_postWork(workCompIbp, 0x4000, 0, &workIbp);
    }
    if (s_removalCalc && workCompIbp) {
        s_removalCalc(*reinterpret_cast<int64_t*>(gw + 0x118),
                      *reinterpret_cast<int*>(workCompIbp + 0x28));
    }

    // --- first-fire-in-town announcement ---
    // The exe inlines EventLog::AddEvent here rather than calling 0x140095580, but
    // the inline is byte-identical to the 4-arg overload; the tornado lane's
    // eventlog.cpp port covers it. Mapping, from the inlined literals:
    //   local_80[0] = 0xFFFF6060  == SeverityColor(3)   => severity  = 3
    //   local_70    = 0x1F        == entry+0x10         => eventType = 0x1F
    //   local_6c    = 3           == entry+0x14         => severity  = 3  (agrees)
    //   local_64    = 3           == IconIndex(0x1F)    => 3          (agrees)
    //   local_60    = -1          == PopupIndex(0x1F)   => -1         (agrees)
    //   EventArray_Grow(eventLog+0x18, *(int*)(eventLog+0x20) + 1, entry)
    // All five literals reproduce, so the call is the inline.
    int* grid = *reinterpret_cast<int**>(gw + 0xf8);
    if (grid && *reinterpret_cast<int16_t*>(reinterpret_cast<uintptr_t>(grid) + 0x9c) < 1) {
        void* eventLog = *reinterpret_cast<void**>(gw + 0x138);
        uint64_t entityIbp = *reinterpret_cast<uint64_t*>(s + 0x10);
        if (eventLog) EventLog_AddEvent(eventLog, 0x1f, 3, &entityIbp);
        // "pause the game on a disaster event" setting.
        if (g_pGameSettingsPtr && g_pGameSettingsPtr[0x17] != 0) {
            int64_t evt = *reinterpret_cast<int64_t*>(gw + 0x120);
            if (evt) *reinterpret_cast<uint32_t*>(evt + 0x34) = 1;
        }
    }

    // --- audio: move this entity's emitters to its world position, then start one ---
    if (s_findAmbient && s_findXform) {
        uintptr_t ambIbp = 0;
        s_findAmbient(*reinterpret_cast<uintptr_t*>(ibpSlot), &ambIbp);

        uintptr_t xformIbp = 0;
        s_findXform(*reinterpret_cast<uintptr_t*>(ibpSlot), &xformIbp);

        if (ambIbp && xformIbp) {
            float pos[3];
            pos[0] = *reinterpret_cast<float*>(xformIbp + 0x158);
            pos[1] = *reinterpret_cast<float*>(xformIbp + 0x15c);
            pos[2] = *reinterpret_cast<float*>(xformIbp + 0x160);
            if (s_audioMove) s_audioMove(reinterpret_cast<void*>(ambIbp), pos);
            FireComp_StartAudioEmitter_42c60(
                reinterpret_cast<void*>(ambIbp),
                *reinterpret_cast<int*>(d + FireDescriptionField::AudioEmitter));
        }
    }

    FireComp_MarkDanger_99c0(self, 1);

}

// ============================================================
// FireComponent_SpreadToNeighbors (0x140049130)
// ============================================================
// Spread census — see the comment at the roll site below.

extern "C" void FireComponent_Spread_130(void* self) {
    EnsureInit();
    if (!self) return;
    uintptr_t s = reinterpret_cast<uintptr_t>(self);
    const uintptr_t gw = GameWorld();
    if (!gw || !s_findMapComp) return;

    void* myDesc = FireDesc(self);
    int* grid = *reinterpret_cast<int**>(gw + 0xf8);
    if (!grid) return;

    void* ibpSlot = reinterpret_cast<void*>(s + 0x10);
    uintptr_t mapIbp = 0;
    s_findMapComp(*reinterpret_cast<uintptr_t*>(ibpSlot), &mapIbp);

    // Footprint-derived sample rect, expanded by min(w,h) on each side.
    int w  = static_cast<int>(*reinterpret_cast<int16_t*>(mapIbp + 0x2c));
    int h  = static_cast<int>(*reinterpret_cast<int16_t*>(mapIbp + 0x2e));
    const int16_t oy = *reinterpret_cast<int16_t*>(mapIbp + 0x2a);
    const int16_t ox = *reinterpret_cast<int16_t*>(mapIbp + 0x28);
    const int pad = (w < h) ? w : h;
    int xMax = w - 1 + pad + ox;
    int yMax = h - 1 + pad + oy;
    int xMin = ox - pad;
    if (xMin < 0) xMin = 0;
    const int gwidth = *grid;
    if (gwidth <= xMax) xMax = gwidth - 1;
    if (gwidth <= yMax) yMax = gwidth - 1;

    if (*reinterpret_cast<int64_t*>(s + 0x30) < 1) {
        *reinterpret_cast<int32_t*>(s + 0x2c) = *reinterpret_cast<int32_t*>(s + 0x28);
        return;
    }

    void* rng = *reinterpret_cast<void**>(gw + 0xd8);

    // Retail's `do { ... } while (fireTimer > 0)` relies on the spread interval
    // (desc+0x30 * 300) being non-zero — each pass adds it to +0x38 and the loop
    // exits as soon as +0x38 goes positive. HANG GUARD ONLY (not a behaviour
    // change): a .rsc with a zero interval would spin here forever and say nothing.
    // The cap is far above any real pass count; tripping it is a loud report.
    const int kMaxPasses = 4096;
    int passes = 0;

    do {
        if (*reinterpret_cast<int64_t*>(s + 0x38) > 0) break;
        if (++passes > kMaxPasses) {
            break;
        }

        *reinterpret_cast<int64_t*>(s + 0x38) += MakeTime(
            *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(myDesc) +
                                      FireDescriptionField::SpreadInterval) * K300());

        int cx = reinterpret_cast<System::Random*>(rng)->Rand(static_cast<int32_t>(xMin), static_cast<int32_t>(xMax));   // B.3.4b bridge
        int cy = reinterpret_cast<System::Random*>(rng)->Rand(static_cast<int32_t>(oy - pad), static_cast<int32_t>(yMax));   // B.3.4b bridge

        // Fine-cell array at grid+0x80, stride 0x30; +0x00 is the occupant.
        int64_t occupant = *reinterpret_cast<int64_t*>(
            *reinterpret_cast<int64_t*>(reinterpret_cast<uintptr_t>(grid) + 0x80) +
            (static_cast<int64_t>(*grid) * cy + cx) * 0x30);

        uint64_t neighbourFire = 0;
        if (occupant != 0) {
            const int n = *reinterpret_cast<int*>(occupant + 0x38);
            int64_t comps = *reinterpret_cast<int64_t*>(occupant + 0x30);
            for (int i = 0; i < n; ++i) {
                uint64_t* slot = reinterpret_cast<uint64_t*>(comps + static_cast<int64_t>(i) * 8);
                uint32_t tid = reinterpret_cast<Object::Base*>(*slot)->Type();   // B.3.8 bridge (slot 2)
                if (tid == FireComponent::TypeId()) {
                    neighbourFire = *slot;
                    break;
                }
            }
        }

        if (occupant != 0 && neighbourFire != 0) {
            void* nDescObj = *reinterpret_cast<void**>(neighbourFire + 0x18);
            void* nDesc = (nDescObj && ::Object::IsDerivedFromOrTrust(
                               nDescObj, FireDescription::TypeId())) ? nDescObj : nullptr;
            const uintptr_t nd = reinterpret_cast<uintptr_t>(nDesc);

            float chance = *reinterpret_cast<float*>(nd + FireDescriptionField::SpreadChance);
            // Half the chance while the game is at its slowest speed step.
            int64_t speedObj = *reinterpret_cast<int64_t*>(gw + 0x130);
            if (speedObj && *reinterpret_cast<int*>(speedObj + 0x10) == 0) chance *= Half();
            // Half it again once the brigade is visibly winning here.
            if (*reinterpret_cast<int*>(nd + FireDescriptionField::BucketsToOut) / 5 <
                *reinterpret_cast<int32_t*>(s + 0x28) - *reinterpret_cast<int32_t*>(s + 0x2c))
                chance *= Half();

            const float roll = reinterpret_cast<System::Random*>(rng)->RandFloat();   // B.3.4b bridge
            const bool caught = roll < chance;
            if (caught) {
                FireComponent_Ignite_610(reinterpret_cast<void*>(neighbourFire));
            }
        }
    } while (*reinterpret_cast<int64_t*>(s + 0x30) > 0);

    *reinterpret_cast<int32_t*>(s + 0x2c) = *reinterpret_cast<int32_t*>(s + 0x28);
}

// ============================================================
// FireComponent_Slot15 (0x140049090) — vtable +0x78, ONE bucket of water.
// ============================================================
// Reached from SendComponentMessage::AbstractImpl with msg == 0, the last action
// in the bucket-brigade queue PerformWork_Action_CopyCtor builds.

// [15] 0x140049090 — the water-bucket message (SendComponentMessage +0x78). The
// body reads only the message id.
void FireComponent::OnMessage(int message, void*, uint32_t, uint32_t) {
    if (message != 0) return;
    EnsureInit();
    uintptr_t s = reinterpret_cast<uintptr_t>(this);
    void* desc = FireDesc(this);
    const uintptr_t d = reinterpret_cast<uintptr_t>(desc);

    *reinterpret_cast<int32_t*>(s + 0x28) += 1;

    const int needed = *reinterpret_cast<int*>(d + FireDescriptionField::BucketsToOut);
    const int64_t step = MakeTime(
        (*reinterpret_cast<float*>(d + FireDescriptionField::SpreadInterval) * K300()) /
        static_cast<float>(needed));
    *reinterpret_cast<int64_t*>(s + 0x30) -= step;

    const int32_t buckets = *reinterpret_cast<int32_t*>(s + 0x28);
    const int64_t timer   = *reinterpret_cast<int64_t*>(s + 0x30);

    if (timer < 1 || needed <= buckets)
        FireComponent_Extinguish_4c0(this);
}
