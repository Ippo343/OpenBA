// code/Game/Game/tornado.cpp
// Standalone factory for Tornado (factory cascade — last concrete misc landmine).
//
// Tornado (size 0x90) extends Graphics::Particle. The exe factory
// (Toolbar_StaticInit_sub_4e90 @ 0x140024e90):
//     p = MemAlloc(0x90, 8); return FUN_1400d23c0(p);
// and the ctor FUN_1400d23c0 @ 0x1400d23c0:
//     Graphics::Particle::Particle(p)        // base ctor, sets base vtable + [0x00,0x48)
//     *(p)        = &vtable_Tornado(0x140160190)
//     <float block at +0x48 .. +0x70>
//     *(qword p+0x74) = -1                   // = two System::Colors, both opaque white
//     System::Time::Time(p+0x80, g_fUVScale)
//     System::Time::Time(p+0x88, g_fUVScale)
//
// vtable_Tornado at 0x140160190 is TEN slots (0x140160190..0x1401601d8). What looks
// like slots 10-19 is the separate TornadoInstance vtable starting at 0x1401601e0 —
// the two share an identical 0x14003aXXX Runtime-thunk tail, which is exactly what
// makes them read as one 20-slot table. Slots, from the BYTES:
//     3 = 0x1400d2370  GetFileVersion | 1
//     5 = 0x1400d2210  Serialize        <- reads the .rsc tornado fields
//     8 = 0x1400d21b0  CreateInstance   -> TornadoInstance (vtable 0x1401601e0,
//                                          whose slot 1 = 0x1400d2470 is the swirl sim)
// the rest are Object boilerplate or Runtime thunks.
//
// The Tornado particle is PURELY VISUAL. All tornado gameplay — the path, the
// movement, and the destruction — lives in TornadoComponent
// (code/Game/Component/tornadocomponent.cpp, vtable 0x14015aa80), which does not
// touch this class at all.
//
// ENDGAME B.3.8 block LOOP: `Tornado` and `TornadoInstance` are REAL classes
// (code/Game/Game/Tornado.h) with compiler vtables. Gone: the vtable COPY site --
// InstallTornadoVtable / Tornado_CreateInstance captured a live Particle /
// ParticleInstance table and patched slots 1/2/3/5/6/7/8/9 and 1 into static
// copies -- the ctor doors for both bases (the compiler runs the real ctors), the
// lazy type-id cache and REGISTER_TYPE (DEFINE_EXE_TYPE).
//
// Slots 6/7/8 of Graphics::Particle are all _purecall (0x18007ec28) -- the base class
// is abstract, there is nothing to "fall back to" -- so the moment Serialize made the
// resource load, slot 8 fired and the process died with R6025. That aborts through the
// MSVC runtime dialog, not a VEH exception, so standalone_crash.log stays EMPTY and
// tools/boot_state.sh reports BLOCKED_DIALOG. Same for ParticleInstance slot 1.
//
// WHY SLOT 5 STOPPED BEING OPTIONAL (2026-08-22 follow-up wave)
// -------------------------------------------------------------
// While the ParticleComponent finder was broken this class was never asked to load, so
// leaving Serialize dormant cost nothing. Once the finder worked, the tornado spawn
// began crashing in ParticleComponent_Draw+0x177 on a READ of 0x7c -- geoArr[0].geo was
// null. The trace in ParticleComponent_Setup narrowed it to nullResource=1: the
// ParticleAttachment ExternalPtr ITSELF was null, one layer above the dormant slots.
// Two measurements pinned the cause:
//   * all 9 other ParticleComponents in the world report nullResource=0 nullInstance=0.
//     Every one of them points at a Runtime ParticleSystem; the tornado alone points at
//     this exe-side class. So external resolution works -- this class was special.
//   * a factory trace showed the object IS built at load time and the ExternalPtr is
//     STILL null at spawn. So External::Load constructs it and then REJECTS it.
// The rejection is what a dormant Serialize buys you: the base Particle::Serialize
// consumes only the base fields and leaves the remaining 15 (9+2 floats, 2 Colors,
// 2 Times) of every Tornado.rsc body unread, so the load fails and stores null.

// ENDGAME B.3.5 Video/Graphics caller batch: Graphics::ParticleInstance (and its
// Graphics::Geometry base) are real classes. FIRST in the block on purpose —
// "resolve.h" pulls <windows.h>, whose <wingdi.h> declares a global `Rectangle`
// FUNCTION that hides `Volume::Rectangle`'s class name inside Volume.h's own
// static_asserts (Geometry.h includes Volume.h).
#include "graphics/ParticleInstance.h"
#include "Game/Tornado.h"
#include "object/Internal.h"   // Object::detail::InternalMemAlloc / InternalMemFree
#include "engine_funcs.h"
#include "math/Math.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch B
#include <cstdint>
#include <cstdio>
#include <cstddef>
#include <cmath>
#include <new>
#include <atomic>
#include <mutex>
#include "system/Value.h"      // ENDGAME B.3.4b System caller batch (Random/Time/Color)

// Kept from the diagnosis pass: it separates the two failures that both end in a null
// ExternalPtr. Factory never called => External::Load found no type at all. Factory
// called and the pointer still null => the object was built and then REJECTED, which is
// what an unread .rsc body looks like.
namespace {
std::atomic<int> s_factoryTraceBudget{0};
std::atomic<int> s_serializeTraceBudget{0};
std::atomic<int> s_updateTicks{0};
std::atomic<int> s_updateTraceBudget{0};
std::atomic<bool> s_updateBailSaid{false};
}  // namespace

static void TornadoFactoryTrace(const char* who, void* p) {
    if (s_factoryTraceBudget.fetch_add(1) >= 8) return;
    fprintf(stderr, "[Tornado/particle] %s -> %p (real class; the instance swirl runs "
            "on TornadoInstance::Update)\n", who, p);
    fflush(stderr);
}

// ---------------------------------------------------------------------------
// Tornado::Tornado -- FUN_1400d23c0. Graphics::Particle() and the 0x140160190
// stamp are the compiler's; then the Tornado defaults. Field meanings are from
// Tornado::Serialize (0x1400d2210, which names the types) cross-read against
// TornadoInstance::Update (0x1400d2470, which consumes them).
// ---------------------------------------------------------------------------
Tornado::Tornado() {
    _tornadoWidth        = 4.0f;    // 0x40800000
    _tornadoHeight       = 20.0f;   // 0x41a00000  rise over a life
    _funnelWidth         = 5.0f;    // 0x40a00000  outer sweep
    _funnelSpin          = 45.0f;   // 0x42340000  degrees/sec
    _particleSpinSpeed   = 1.0f;    // 0x3f800000
    _emissionRate        = 1.0f;    // 0x3f800000  interval = 1/x seconds
    _particleLifeTime    = 10.0f;   // 0x41200000
    _particleLifeTimeTol = 0.0f;
    _particleStartSize   = 0.2f;    // 0x3e4ccccd  also the spawn size
    _particleEndSize     = 3.0f;    // 0x40400000
    _particleScaleSize   = 30.0f;   // 0x41f00000  clamped into [start, end]
    // Two System::Colors -- START and END tint -- all-0xFF = opaque white at both
    // ends; Update divides their bytes by 255.0 (DAT_14017222c).
    _startColor = 0xFFFFFFFFu;
    _endColor   = 0xFFFFFFFFu;
    // Two System::Time fields seeded with g_fUVScale (= 1.0f): fade-in and fade-out.
    if (fn_Time_Ctor_Float) {
        float uv = g_fUVScale ? *g_fUVScale : 1.0f;
        fn_Time_Ctor_Float(&_particleFadeIn, uv);
        fn_Time_Ctor_Float(&_particleFadeOut, uv);
    }
}

// [0] 0x140024f00: restamp, ~Particle (import 0x1401527b8), MemFree on flags & 1.
Object::Base* Tornado::DeletingDtor(unsigned flags) {
    this->~Tornado();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// Retail slot 3 (0x1400d2370): call the base GetFileVersion through dispatch slot
// 0x1401527c0, then OR in bit 0.
//
// The base call is `Graphics::Particle::GetFileVersion`, spelled QUALIFIED: this
// function IS slot 3 of the Tornado table, so an unqualified virtual call would
// dispatch straight back into it.
// ARITY: `?GetFileVersion@Particle@Graphics@@UEBAHXZ` takes NO argument, so `a2` — which
// the old typedef forwarded as a hedge against Ghidra dropping a parameter — is not
// passed. The mangled name is the authority, and retail's slot-3 body
// (`MOV EAX,0x1000; RET`) reads nothing but its return constant.
int32_t Tornado::GetFileVersion() const {
    return Graphics::Particle::GetFileVersion() | 1;
}

// Retail slot 5 (0x1400d2210). Faithful, in retail order -- and the ORDER IS THE
// FORMAT: the name argument is NULL on every call (r8=0), so the compiled .rsc body is
// positional and one transposed field silently shifts every value after it.
//
//   base Serialize (via base vtable slot 5, retail dispatch 0x1401527c8)
//   9 floats : +0x48 +0x4c +0x50 +0x58 +0x5c +0x60 +0x64 +0x68 +0x6c
//   2 Colors : +0x74 +0x78
//   2 Times  : +0x80 +0x88
//   2 floats : +0x54 +0x70          <- LAST, not with the other nine
//
// Returns true (retail sets al=1).
bool Tornado::Serialize(void* stream) {
    // ENDGAME B.3.5 batch K1 — same shape as GetFileVersion above: a QUALIFIED
    // non-virtual call to the base body, because this function IS slot 5 of the Tornado
    // table. `?Serialize@Particle@Graphics@@UEAA_NAEAVStream@IO@@@Z` returns bool and
    // retail discards it, so the result is discarded here too.
    (void)Graphics::Particle::Serialize(stream);

    uint8_t* s = reinterpret_cast<uint8_t*>(this);   // bridge: the body reads offsets
    alignas(8) uint8_t ser[32] = {};
    ::new (static_cast<void*>(ser)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    static const int kFirstFloats[9] = { 0x48, 0x4c, 0x50, 0x58, 0x5c, 0x60, 0x64, 0x68, 0x6c };
    for (int k = 0; k < 9; ++k)
        reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<float*>(s + kFirstFloats[k]), nullptr);   // B.3.4 bridge

    reinterpret_cast<System::Color*>(s + 0x74)->Serialize(*reinterpret_cast<IO::Serializer*>(ser));   // start tint  B.3.4b bridge
    reinterpret_cast<System::Color*>(s + 0x78)->Serialize(*reinterpret_cast<IO::Serializer*>(ser));   // end tint  B.3.4b bridge
    reinterpret_cast<System::Time*>(s + 0x80)->Serialize(*reinterpret_cast<IO::Serializer*>(ser));         // fade in  B.3.4b bridge
    reinterpret_cast<System::Time*>(s + 0x88)->Serialize(*reinterpret_cast<IO::Serializer*>(ser));         // fade out  B.3.4b bridge

    reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<float*>(s + 0x54), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<float*>(s + 0x70), nullptr);   // B.3.4 bridge

    reinterpret_cast<IO::Serializer*>(ser)->~Serializer();   // B.3.4 bridge

    // Outcome, once. The ctor defaults are 4.0 / 20.0 / 5.0 at +0x48/+0x4c/+0x50; if the
    // .rsc body was really read these become its values (Tornado.rsc: 20 / 75 / 26).
    // Unchanged defaults mean Serialize ran but consumed nothing.
    {
        if (s_serializeTraceBudget.fetch_add(1) < 4) {
            fprintf(stderr, "[Tornado/particle] Serialize read: +0x48=%.2f +0x4c=%.2f "
                    "+0x50=%.2f +0x54=%.2f +0x70=%.2f (ctor defaults were "
                    "4.00/20.00/5.00/45.00/30.00)\n",
                    *(float*)(s + 0x48), *(float*)(s + 0x4c), *(float*)(s + 0x50),
                    *(float*)(s + 0x54), *(float*)(s + 0x70));
            fflush(stderr);
        }
    }
    return true;
}

// Retail slot 6 (0x140024ee0): slot 0 with flags 0 -- destruct without freeing --
// then the ctor again in place.
Object::External* Tornado::Reconstruct() {
    DeletingDtor(0);
    return ::new (static_cast<void*>(this)) Tornado();
}

// Retail slot 7 (0x140001640): slot 0 with flags 0, tail. A shared thunk in retail.
void Tornado::Destruct() { DeletingDtor(0); }

// Retail slot 8 (0x1400d21b0): alloc 0xe0/align 8, run the ParticleInstance ctor,
// stamp vtable_TornadoInstance, zero +0xd8, return it (0 on a failed alloc).
Graphics::ParticleInstance* Tornado::CreateSystem() const {
    void* p = Object::detail::InternalMemAlloc(0xe0, 8);
    if (!p) return nullptr;
    return ::new (p) TornadoInstance();
}

// Retail slot 9 (0x140024ed0): self ? ctor(self) : null -- placement new.
Object::External* Tornado::Construct() {
    return ::new (static_cast<void*>(this)) Tornado();
}

// ---------------------------------------------------------------------------
// TornadoInstance
// ---------------------------------------------------------------------------
// Graphics::ParticleInstance() and the 0x1401601e0 stamp are the compiler's.
TornadoInstance::TornadoInstance() {
    *reinterpret_cast<int32_t*>(&_funnelSpinDegrees) = 0;   // retail: dword 0 at +0xd8
}

// [0] 0x140024f50: ~ParticleInstance (import 0x1401527d8), MemFree on flags & 1.
Graphics::Geometry* TornadoInstance::DeletingDtor(unsigned flags) {
    this->~TornadoInstance();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// ---------------------------------------------------------------------------
// TornadoInstance::Update -- retail 0x1400d2470, INSTANCE vtable slot 1.
//
// vtable_TornadoInstance (0x1401601e0) is exactly ten slots -- index 10 already holds
// string bytes ("Weather...") -- and slot 1 is the only one the subclass overrides.
// The base Graphics::ParticleInstance slot 1 is _purecall, so this body is not an
// optimisation: without it the first frame aborts with R6025.
//
// Layouts, all read off the retail body:
//   TornadoInstance  +0xc0 Tornado* resource  +0xc8 pool*  +0xd8 float funnel spin (deg)
//   pool             +0x00 Box(min xyz,max xyz)  +0x18 particle array (stride 0x58)
//                    +0x20 maxCount  +0x28 liveCount  +0x30 spawn accumulator (Time)
//                    +0x38 total accumulator (Time, written but never read here)
//   Tornado resource -- names below are the ToolKit ones, from
//   ToolKit/resource/ParticleSystems/Tornado/Tornado.rsc, which lists exactly the
//   15 fields Tornado::Serialize reads and in the same order, confirming the map:
//     +0x48 _tornadoWidth 20   +0x4c _tornadoHeight 75   +0x50 _funnelWidth 26
//     +0x54 _funnelSpin 30     +0x58 _particleSpinSpeed 400  +0x5c _emissionRate 20
//     +0x60 _particleLifeTime 30  +0x64 _particleLifeTimeTol 2
//     +0x68 _particleStartSize 2.5  +0x6c _particleEndSize 15
//     +0x70 _particleScaleSize 30   +0x74/+0x78 start/end Color
//     +0x80 _particleFadeIn 0.05    +0x88 _particleFadeOut 1.0
//   _particleStartSize/_particleEndSize are used as a CLAMP RANGE here, not as a
//   size ramp -- the ramp is _particleScaleSize times the radius profile.
//
// Each Color is unpacked BYTE +2 -> .x, +1 -> .y, +0 -> .z, with byte +3 carried
// separately as the alpha. Both call sites do it identically (spawn 0x1400d25f2,
// per-particle 0x1400d26b1), so the reversal is retail, not a transcription slip. It is
// almost certainly a BGRA->RGB unpack rather than a deliberate channel swap, but
// Tornado.rsc uses greys (0xFFC0C0C0 / 0x80909090) so the run cannot distinguish the
// two; only the alpha half is proven, by the measured 1.00 -> 0.50 fade.
// ---------------------------------------------------------------------------
namespace {
struct TornadoParticle {          // 0x58, stride proven by `imul $0x58`
    int64_t lifetime;             // +0x00
    int64_t age;                  // +0x08
    float   pos[3];               // +0x10
    float   prevPos[3];           // +0x1c
    float   color[3];             // +0x28
    float   prevColor[3];         // +0x34
    float   alpha;                // +0x40
    float   prevAlpha;            // +0x44
    float   angle;                // +0x48
    float   prevAngle;            // +0x4c
    float   size;                 // +0x50
    float   prevSize;             // +0x54
};
static_assert(sizeof(TornadoParticle) == 0x58, "TornadoParticle stride");
static_assert(offsetof(TornadoParticle, angle) == 0x48, "TornadoParticle angle");

struct TornadoPool {
    float            box[6];      // +0x00  min xyz / max xyz -- IS the Box for SetBounds
    TornadoParticle* items;       // +0x18
    int32_t          maxCount;    // +0x20
    int32_t          pad24;
    int32_t          liveCount;   // +0x28
    int32_t          pad2c;
    int64_t          spawnAccum;  // +0x30
    int64_t          totalAccum;  // +0x38
};
static_assert(offsetof(TornadoPool, items) == 0x18, "TornadoPool items");
static_assert(offsetof(TornadoPool, spawnAccum) == 0x30, "TornadoPool spawnAccum");

// Runtime leaves. Every one is resolved by mangled export name rather than through a
// .rdata dispatch slot: the slots this body uses (0x1401513f0, 0x140151320,
// 0x1401525e8, 0x1401527a8, 0x1401527d0, 0x1401527e0, 0x1401527e8,
// 0x1401527f0) are NOT in standalone/address_registry.cpp, so ResolveAddr would hand
// back a null slot.
// ENDGAME B.3.5 closing pass: the `ParticleInstance::Update` and
// `Geometry::SetBounds` door typedefs, struct members, resolves, readiness terms
// and diagnostic `%p`s are GONE. Both call sites are member calls (:289, :475),
// so the only thing left reading the two pointers was the readiness aggregate
// and the trace that printed them -- neither is a caller.
typedef bool   (*fn_IsDerived_t)(const void* obj, uint32_t tid);  // Object::Base::IsDerivedFrom
typedef void   (*fn_TimeCtor_t)(void* retTime, float seconds);
typedef void*  (*fn_BoxExtend_t)(const void* box, void* ret, const float* v3);

// ENDGAME B.3.5: `baseUpdate` and `setBounds` are no longer CALLED — both sites
// now call the members. The two fields, their resolves, the `f.ok` aggregate and
// the UNRESOLVED fprintf below still READ the pointers as a diagnostic, so the
// cache is not dead and is not removed here (the closing pass takes it with the
// remaining three non-Graphics doors).
struct TornadoInstFuncs {
    fn_IsDerived_t    isDerived;
    fn_TimeCtor_t     timeCtor;
    fn_BoxExtend_t    boxExtend;
    void*             rng;        // Graphics::Particle::_rng, the static the retail body uses
    bool              ok;
};

// Resolved once, thread-safely, at namespace scope (B6-5: no function-local
// first-call cache in a member).
TornadoInstFuncs s_instFuncs = {};
std::once_flag   s_instFuncsOnce;

void ResolveInstFuncs() {
    TornadoInstFuncs& f = s_instFuncs;
    f.isDerived    = (fn_IsDerived_t)   ResolveExport("?IsDerivedFrom@Base@Object@@QEBA_NI@Z");
    f.timeCtor     = (fn_TimeCtor_t)    ResolveExport("??0Time@System@@QEAA@M@Z");
    f.boxExtend    = (fn_BoxExtend_t)   ResolveExport("?Extend@Box@Volume@@QEBA?AV12@AEBVVector3@Math@@@Z");
    f.rng          =                    ResolveExport("?_rng@Particle@Graphics@@1VRandom@System@@A");
    f.ok = f.isDerived && f.timeCtor && f.boxExtend && f.rng;
    if (!f.ok) {
        fprintf(stderr, "[Tornado/particle] Update UNRESOLVED leaf: isDerived=%p "
                "timeCtor=%p extend=%p "
                "rng=%p -- the funnel will stand still\n",
                (void*)f.isDerived, (void*)f.timeCtor,
                (void*)f.boxExtend, f.rng);
        fflush(stderr);
    }
}

const TornadoInstFuncs& InstFuncs() {
    std::call_once(s_instFuncsOnce, &ResolveInstFuncs);
    return s_instFuncs;
}

inline float ResF(const void* res, int off) {
    return *reinterpret_cast<const float*>(reinterpret_cast<const uint8_t*>(res) + off);
}
inline const uint8_t* ResB(const void* res, int off) {
    return reinterpret_cast<const uint8_t*>(res) + off;
}

}  // namespace

// Retail's sinf/cosf are the exe's own arg-reduced libm bodies at 0x140131c90 /
// 0x140131090, already reimplemented as the CRT calls in Component/character_trig.cpp.
// The same precedent applies here: a sub-ULP difference moves a dust particle.
void TornadoInstance::Update(const System::Time& time) {
    const TornadoInstFuncs& f = InstFuncs();
    if (!f.ok) return;

    void*          self  = this;   // bridge: the body reads offsets
    const int64_t* dtPtr = reinterpret_cast<const int64_t*>(&time);   // B.3.4b bridge: Time is one int64 tick count
    uint8_t* inst = reinterpret_cast<uint8_t*>(self);

    // Base ParticleInstance::Update is the frame gate: it returns false when this
    // instance has already been stepped this frame.
    // B.3.5 bridge — ?Update@ParticleInstance@Graphics@@QEAA_NXZ, the NON-VIRTUAL
    // `bool Update()` (ParticleInstance.h's name-hiding note: the arg-less
    // overload, not slot 1's `Update(const Time&)`).
    if (!Graphics::ParticleInstance::Update()) return;

    void*        resource = *reinterpret_cast<void**>(inst + 0xc0);
    TornadoPool* pool     = *reinterpret_cast<TornadoPool**>(inst + 0xc8);

    // Retail: `cmovne rbx,rsi` -- rsi is the resource only when IsDerivedFrom(Tornado)
    // holds, else 0, and every later read (`divss 0x5c(%rsi)`) would fault on it. In
    // retail that branch is unreachable because slot 2 GetTypeId reports Tornado. In
    // standalone it is reachable if our vtable copy ever loses the slot-2 override, so
    // this is a DELIBERATE deviation: bail loudly instead of faulting on a null we
    // manufactured ourselves.
    const uint32_t tornadoTid = Tornado::TypeId();
    if (!resource || !f.isDerived(resource, tornadoTid)) {
        if (!s_updateBailSaid.exchange(true)) {
            fprintf(stderr, "[Tornado/particle] Update bailed: resource=%p is not a Tornado "
                    "(tid=%u). Retail cannot reach this -- Tornado::Type() must "
                    "be reporting the base Particle again.\n", resource, tornadoTid);
            fflush(stderr);
        }
        return;
    }
    const void* res = resource;

    const int64_t dt = *dtPtr;
    pool->spawnAccum += dt;
    pool->totalAccum += dt;

    // ---- spawn ------------------------------------------------------------
    int64_t period = 0;
    f.timeCtor(&period, 1.0f / ResF(res, 0x5c));       // 1 / _emissionRate, in ticks

    const uint8_t* cStart = ResB(res, 0x74);
    const uint8_t* cEnd   = ResB(res, 0x78);

    while (pool->spawnAccum > period) {
        if (pool->liveCount >= pool->maxCount) break;
        pool->spawnAccum -= period;

        TornadoParticle* p = &pool->items[pool->liveCount++];
        p->age = 0;

        int64_t life = 0;
        f.timeCtor(&life, ResF(res, 0x60) + reinterpret_cast<System::Random*>(f.rng)->RandFloatTol() * ResF(res, 0x64));   // B.3.4b bridge
        p->lifetime = life;

        p->alpha = p->prevAlpha = 0.0f;                 // retail: one 8-byte zero at +0x40

        p->color[0] = p->prevColor[0] = cStart[2] / 255.0f;
        p->color[1] = p->prevColor[1] = cStart[1] / 255.0f;
        p->color[2] = p->prevColor[2] = cStart[0] / 255.0f;

        Math::Vector3 z((Math::Scalar(0.0f)));   // Vector3(const Scalar&) splat
        p->pos[0] = p->prevPos[0] = z.x;
        p->pos[1] = p->prevPos[1] = z.y;
        p->pos[2] = p->prevPos[2] = z.z;

        p->angle = p->prevAngle = reinterpret_cast<System::Random*>(f.rng)->RandFloatTol() * 180.0f;   // B.3.4b bridge
        p->size  = p->prevSize  = ResF(res, 0x68);      // _particleStartSize
    }

    // ---- the growth box is rebuilt from scratch every frame, starting at the
    //      ORIGIN rather than at an inverted-infinite box. Retail, faithfully. ----
    {
        Math::Vector3 z1((Math::Scalar(0.0f)));
        Math::Vector3 z2((Math::Scalar(0.0f)));
        pool->box[0] = z2.x; pool->box[1] = z2.y; pool->box[2] = z2.z;
        pool->box[3] = z1.x; pool->box[4] = z1.y; pool->box[5] = z1.z;
    }

    const float startColor[3] = { cStart[2] / 255.0f, cStart[1] / 255.0f, cStart[0] / 255.0f };
    const float endColor[3]   = { cEnd[2]   / 255.0f, cEnd[1]   / 255.0f, cEnd[0]   / 255.0f };
    const float startAlpha    = cStart[3] / 255.0f;
    const float endAlpha      = cEnd[3]   / 255.0f;

    const float dtSec = reinterpret_cast<const System::Time*>(dtPtr)->AsSeconds();   // B.3.4b bridge

    // The whole funnel rotates: +0xd8 accumulates degrees at the resource's spin rate.
    const float spin = *reinterpret_cast<float*>(inst + 0xd8) + dtSec * ResF(res, 0x54);  // _funnelSpin
    *reinterpret_cast<float*>(inst + 0xd8) = spin;
    const float spinRad = spin * 0.01745329f;
    const float sinG = sinf(spinRad);
    const float cosG = cosf(spinRad);

    // 90 deg/sec is a HARDCODED constant in the retail body (0x140172204), not
    // _particleSpinSpeed -- that one (+0x58) drives the helical angle below instead.
    const float spinDelta = dtSec * 90.0f;             // per-particle billboard spin

    const float width      = ResF(res, 0x48);   // _tornadoWidth
    const float height     = ResF(res, 0x4c);   // _tornadoHeight
    const float funnelW    = ResF(res, 0x50);   // _funnelWidth
    const float spinSpeed  = ResF(res, 0x58);   // _particleSpinSpeed
    const float sizeFloor  = ResF(res, 0x68);   // _particleStartSize -- clamp LOW
    const float sizeCeil   = ResF(res, 0x6c);   // _particleEndSize   -- clamp HIGH
    const float sizeScale  = ResF(res, 0x70);   // _particleScaleSize
    const int64_t fadeOutTicks = *reinterpret_cast<const int64_t*>(ResB(res, 0x88));

    // ---- per particle, walked BACKWARDS (retail counts down from liveCount-1) ----
    for (int i = pool->liveCount - 1; i >= 0; --i) {
        TornadoParticle* p = &pool->items[i];
        p->age += dt;

        const float t = reinterpret_cast<const System::Time*>(&p->age)->AsSeconds() / reinterpret_cast<const System::Time*>(&p->lifetime)->AsSeconds();   // B.3.4b bridge

        // Lateral swirl: a cosine wobble that grows with age, biased so young
        // particles start inside the funnel (the -0.33 is retail's).
        const float wob   = cosf((t - 0.2f) * 3.1415927f * 2.0f);
        const float swirl = (wob * 0.5f + 0.5f) * (t + 0.5f) - 0.33f;
        const float offX  = sinG * funnelW * swirl;
        const float offY  = cosG * funnelW * swirl;

        // Radius profile: a cubic bulge around t=0.4 plus a quadratic flare past t=0.3.
        float tail = t - 0.3f;
        if (!(tail > 0.0f)) tail = 0.0f;
        const float c   = t - 0.4f;
        const float rad = fabsf(c * c * c) * 3.7f + 0.05f + tail * tail * 4.0f;

        const float a    = spinSpeed * 0.31830987f * t;   // spinSpeed / pi * t
        const float sinA = sinf(a);
        const float cosA = cosf(a);

        p->prevPos[0] = p->pos[0];
        p->prevPos[1] = p->pos[1];
        p->prevPos[2] = p->pos[2];

        const float px = width * (sinA * rad) + offX;
        const float py = width * (cosA * rad) + offY;
        // The billboard's own spin angle also lifts it: retail folds |angle|/40 into z.
        const float pz = fabsf(p->angle) / 40.0f + t * height;

        p->pos[0] = px;
        p->pos[1] = py;
        p->pos[2] = pz;

        {
            const float v[3] = { px, py, pz };
            float retBox[6];
            const float* nb = reinterpret_cast<const float*>(f.boxExtend(pool->box, retBox, v));
            for (int k = 0; k < 6; ++k) pool->box[k] = nb[k];
        }

        float newAngle = spinDelta + p->angle;
        if (newAngle > 180.0f) newAngle -= 360.0f;
        p->prevAngle = p->angle;
        p->angle     = newAngle;

        // Retail recomputes the age ratio here; the age has not changed since, so it is
        // the same value of t, reused as the colour/alpha interpolant.
        const float u = reinterpret_cast<const System::Time*>(&p->age)->AsSeconds() / reinterpret_cast<const System::Time*>(&p->lifetime)->AsSeconds();   // B.3.4b bridge

        float sz = rad * sizeScale;
        if (sizeFloor > sz)      sz = sizeFloor;
        else if (sz > sizeCeil)  sz = sizeCeil;
        p->prevSize = p->size;
        p->size     = sz;

        {
            Math::Vector3 cc = reinterpret_cast<const Math::Vector3*>(startColor)->Lerp(
                *reinterpret_cast<const Math::Vector3*>(endColor), u);
            p->prevColor[0] = p->color[0];
            p->prevColor[1] = p->color[1];
            p->prevColor[2] = p->color[2];
            p->color[0] = cc.x;
            p->color[1] = cc.y;
            p->color[2] = cc.z;
        }

        // Alpha envelope: fade in over resource +0x80, then fade out over +0x88
        // measured backwards from death. Retail computes BOTH and picks with a compare.
        const float   fadeInT  = reinterpret_cast<const System::Time*>(&p->age)->AsSeconds() / reinterpret_cast<const System::Time*>(ResB(res, 0x80))->AsSeconds();   // B.3.4b bridge
        const int64_t outAge   = fadeOutTicks - p->lifetime + p->age;
        const float   fadeOutT = reinterpret_cast<const System::Time*>(&outAge)->AsSeconds() / reinterpret_cast<const System::Time*>(ResB(res, 0x88))->AsSeconds();   // B.3.4b bridge

        float env = (1.0f < fadeInT) ? (1.0f - fadeOutT) : fadeInT;
        if (env > 1.0f)      env = 1.0f;
        else if (0.0f > env) env = 0.0f;

        Math::Scalar a0(startAlpha);
        Math::Scalar la = a0.Lerp(Math::Scalar(endAlpha), Math::Scalar(u));
        p->alpha = p->prevAlpha = env * la.v;
    }

    // B.3.5 bridge — ?SetBounds@Geometry@Graphics@@QEAAXAEBVBox@Volume@@@Z. The
    // door took `(void* geometry, const void* box)` and retail hands it the pool
    // pointer: the accumulated `Volume::Box` is the pool's first 0x18 bytes.
    reinterpret_cast<Graphics::Geometry*>(self)->SetBounds(
        *reinterpret_cast<const ::Box*>(pool));   // B.3.5 bridge

    // Outcome trace, budgeted. "the swirl is running" is not observable from the
    // absence of a crash: a pool that never spawns, a period that came out infinite
    // (emissionRate == 0) and a live sim all look identical in a quiet log. Print the
    // numbers that separate them -- live/max, the spawn period in ticks, the box the
    // renderer will cull against, and one particle's actual position.
    {
        // Spread the samples: the pool cannot spawn before the accumulator crosses
        // one period, so three consecutive frames would only ever show live=0.
        const int ticks = s_updateTicks.fetch_add(1) + 1;
        if (s_updateTraceBudget.load() < 8 && (ticks <= 2 || ticks % 120 == 0)) {
            s_updateTraceBudget.fetch_add(1);
            const TornadoParticle* s = (pool->liveCount > 0) ? &pool->items[0] : nullptr;
            fprintf(stderr, "[Tornado/particle] tick %d: live=%d/%d period=%lld ticks "
                    "spin=%.1fdeg box=(%.2f,%.2f,%.2f)-(%.2f,%.2f,%.2f) p0=(%.2f,%.2f,%.2f) "
                    "size=%.2f alpha=%.2f\n",
                    ticks, pool->liveCount, pool->maxCount, (long long)period, spin,
                    pool->box[0], pool->box[1], pool->box[2],
                    pool->box[3], pool->box[4], pool->box[5],
                    s ? s->pos[0] : 0.0f, s ? s->pos[1] : 0.0f, s ? s->pos[2] : 0.0f,
                    s ? s->size : 0.0f, s ? s->alpha : 0.0f);
            fflush(stderr);
        }
    }

    // ---- reap: erase every particle whose age reached its lifetime, shifting the
    //      tail down one slot. Retail re-tests the same index after a removal. ----
    for (int i = 0; i < pool->liveCount; ++i) {
        TornadoParticle* arr = pool->items;
        if (arr[i].age < arr[i].lifetime) continue;
        --pool->liveCount;
        for (int j = i; j < pool->liveCount; ++j) pool->items[j] = pool->items[j + 1];
        --i;
    }
}

// ---------------------------------------------------------------------------
// Reflection
// ---------------------------------------------------------------------------
void* Tornado::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(Tornado), 8);
    void* built = p ? static_cast<Object::Base*>(::new (p) Tornado()) : nullptr;
    TornadoFactoryTrace("Factory", built);
    return built;
}
void* Tornado::CreateInPlaceObject(void* memory) {
    TornadoFactoryTrace("CreateInPlace", memory);
    if (!memory) return nullptr;
    return static_cast<Object::Base*>(::new (memory) Tornado());
}

DEFINE_EXE_TYPE(Tornado, "Graphics::Particle")
