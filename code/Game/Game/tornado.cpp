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
// PARTIALLY DORMANT FACTORY. We run the real Graphics::Particle base ctor, fill the
// Tornado float block, and then install a COPY of the base vtable with the two slots
// this class actually needs overridden -- slot 3 GetFileVersion and slot 5 Serialize.
// We still do NOT install the exe subclass vtable 0x140160190 itself: those entries are
// exe->Runtime thunks that do not resolve standalone. Remaining divergence: GetTypeId
// (slot 2) still reports Particle, and slot 8 CreateInstance is still the base, so the
// instance is a plain ParticleInstance with no swirl -- there is a funnel, but it does
// not spin.
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

#include "Object/factory_registration.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// Kept from the diagnosis pass: it separates the two failures that both end in a null
// ExternalPtr. Factory never called => External::Load found no type at all. Factory
// called and the pointer still null => the object was built and then REJECTED, which is
// what an unread .rsc body looks like. With slot 5 installed the second line should stop
// appearing; if it comes back, the body is still not being consumed.
static void TornadoFactoryTrace(const char* who, void* p) {
    static int budget = 0;
    if (budget >= 8) return;
    budget++;
    fprintf(stderr, "[Tornado/particle] %s -> %p (slots 3/5/6/7/8/9 installed; the "
            "instance update is still a stub)\n", who, p);
    fflush(stderr);
}

static void* Tornado_Build(void* p);   // defined below; slots 6 and 9 re-run it

// ---------------------------------------------------------------------------
// Vtables.
//
// THE BASE TABLE IS NINE SLOTS AND THREE OF THEM ARE _purecall. Read out of the live
// DLL export ??_7Particle@Graphics@@6B@: slots 6, 7 and 8 all hold 0x18007ec28, and
// index 9 is already string data, so Graphics::Particle declares exactly nine virtuals
// of which the last three are abstract. exe vtable_Tornado (0x140160190) has TEN: the
// class adds slot 9. Two consequences that cost a crash each to learn:
//
//   * COPY EXACTLY NINE. An earlier version copied sixteen "to be safe", which dragged
//     seven quadwords of adjacent .rdata string bytes in as slots 9..15. Over-copying a
//     vtable is not conservative -- it is how you get a call into the middle of a
//     string literal.
//   * LEAVING A PURE SLOT ALONE IS NOT NEUTRAL. While Tornado.rsc failed to load,
//     nothing ever dispatched 6/7/8, so inheriting _purecall cost nothing. The moment
//     slot 5 Serialize made the resource load, ParticleComponent_Setup called slot 8
//     (CreateInstance) and the process died in the MSVC runtime with R6025 -- an abort
//     dialog, not an access violation, which is why the VEH crash log was empty and the
//     boot watchdog reported BLOCKED_DIALOG instead.
//
// So every slot the exe overrides is implemented here, from the retail bodies:
//   [0] 0x140024f00 dtor            - base kept (ours does not install an exe vtable)
//   [3] 0x1400d2370 GetFileVersion  - base | 1
//   [5] 0x1400d2210 Serialize       - the .rsc body
//   [6] 0x140024ee0 recreate        - vtable[0](self,0) then re-run the ctor
//   [7] 0x140001640 destruct        - vtable[0](self,0), tail
//   [8] 0x1400d21b0 CreateInstance  - alloc 0xe0, ParticleInstance ctor, +0xd8 = 0
//   [9] 0x140024ed0 createInPlace   - self ? re-run the ctor : null
// ---------------------------------------------------------------------------
namespace {
constexpr int kParticleSlots = 9;    // Graphics::Particle declares 0..8
constexpr int kTornadoSlots  = 10;   // Tornado adds slot 9
void* s_baseVtable[kParticleSlots];
void* s_tornadoVtable[kTornadoSlots];
bool  s_vtableReady = false;

// Graphics::ParticleInstance is 10 slots and slot 1 -- the per-frame update -- is
// _purecall too; TornadoInstance overrides it at 0x1400d2470 with the swirl sim.
// Our ParticleComponent port dispatches that slot from three places
// (particlecomponent.cpp Place, OnUpdate2 and UpdateTick), so handing back a plain
// base instance would abort on the first frame. Until 0x1400d2470 is ported the slot
// gets an explicit no-op: the funnel exists and does not spin. That is a DELIBERATE
// behavioural gap, recorded here because the old header claimed the base
// CreateInstance "yields a plain ParticleInstance" -- there is no base CreateInstance
// to yield one, and no usable base update either.
constexpr int kInstanceSlots = 10;
void* s_instVtable[kInstanceSlots];
bool  s_instVtableReady = false;

typedef void* (*fn_ParticleInstance_Ctor_t)(void* self);
fn_ParticleInstance_Ctor_t s_particleInstanceCtor = nullptr;

void TornadoInstance_Update_Stub(void*, void*) {
    static bool said = false;
    if (!said) {
        said = true;
        fprintf(stderr, "[Tornado/particle] instance update is a NO-OP stub: "
                "TornadoInstance::Update (0x1400d2470) is not ported, and the base "
                "Graphics::ParticleInstance slot 1 is _purecall. The funnel will not "
                "swirl or emit.\n");
        fflush(stderr);
    }
}

// Retail slot 3 (0x1400d2370): call the base GetFileVersion through dispatch slot
// 0x1401527c0, then OR in bit 0. Args are forwarded untouched, so forward two.
uint32_t Tornado_GetFileVersion(void* self, void* a2) {
    typedef uint32_t (*base_t)(void*, void*);
    base_t base = reinterpret_cast<base_t>(s_baseVtable[3]);
    return (base ? base(self, a2) : 0u) | 1u;
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
bool Tornado_Serialize(void* self, void* stream) {
    typedef void (*baseSer_t)(void*, void*);
    baseSer_t base = reinterpret_cast<baseSer_t>(s_baseVtable[5]);
    if (base) base(self, stream);            // retail discards the base return

    if (!g_fn_Serializer_Ctor || !g_fn_Serializer_Dtor || !fn_Serializer_Serialize_Float ||
        !fn_System_Color_Serialize || !g_fn_Time_Serialize) {
        // Any missing leaf leaves the body unread and the load fails exactly as it did
        // before this slot existed -- so say which one, rather than regress in silence.
        static bool warned = false;
        if (!warned) {
            warned = true;
            fprintf(stderr, "[Tornado/particle] Serialize UNRESOLVED leaf: serCtor=%p "
                    "serDtor=%p float=%p color=%p time=%p -- Tornado.rsc will fail to "
                    "load and its ExternalPtr will stay null\n",
                    (void*)g_fn_Serializer_Ctor, (void*)g_fn_Serializer_Dtor,
                    (void*)fn_Serializer_Serialize_Float, (void*)fn_System_Color_Serialize,
                    (void*)g_fn_Time_Serialize);
            fflush(stderr);
        }
        return true;
    }

    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    uint8_t  ser[32] = {};
    g_fn_Serializer_Ctor(ser, stream, 0);

    static const int kFirstFloats[9] = { 0x48, 0x4c, 0x50, 0x58, 0x5c, 0x60, 0x64, 0x68, 0x6c };
    for (int k = 0; k < 9; ++k)
        fn_Serializer_Serialize_Float(ser, reinterpret_cast<float*>(s + kFirstFloats[k]), nullptr);

    fn_System_Color_Serialize(s + 0x74, ser);   // start tint
    fn_System_Color_Serialize(s + 0x78, ser);   // end tint
    g_fn_Time_Serialize(s + 0x80, ser);         // fade in
    g_fn_Time_Serialize(s + 0x88, ser);         // fade out

    fn_Serializer_Serialize_Float(ser, reinterpret_cast<float*>(s + 0x54), nullptr);
    fn_Serializer_Serialize_Float(ser, reinterpret_cast<float*>(s + 0x70), nullptr);

    g_fn_Serializer_Dtor(ser);

    // Outcome, once. The ctor defaults are 4.0 / 20.0 / 5.0 at +0x48/+0x4c/+0x50; if the
    // .rsc body was really read these become its values (Tornado.rsc: 20 / 75 / 26).
    // Unchanged defaults mean Serialize ran but consumed nothing.
    {
        static int budget = 0;
        if (budget < 4) {
            budget++;
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

// Retail slot 8 (0x1400d21b0): alloc 0xe0/align 8, run the ParticleInstance ctor,
// install vtable_TornadoInstance, zero +0xd8, return it. `self` is genuinely unused --
// rcx is clobbered by the allocator args before anything reads it.
void* Tornado_CreateInstance(void*) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0xe0, 8);
    if (!p) return nullptr;                  // retail returns 0 on a failed alloc too
    if (s_particleInstanceCtor) s_particleInstanceCtor(p);
    if (!s_instVtableReady) {
        void** vt = *reinterpret_cast<void***>(p);
        if (vt) {
            for (int k = 0; k < kInstanceSlots; ++k) s_instVtable[k] = vt[k];
            s_instVtable[1] = reinterpret_cast<void*>(&TornadoInstance_Update_Stub);
            s_instVtableReady = true;
        }
    }
    if (s_instVtableReady) *reinterpret_cast<void**>(p) = s_instVtable;
    *(int32_t*)(reinterpret_cast<uint8_t*>(p) + 0xd8) = 0;
    return p;
}

// Retail slot 6 (0x140024ee0): vtable[0](self, 0) -- destruct without freeing --
// then tail-jump into the ctor. Retail reads slot 0 off the LIVE vtable, so do the same.
void* Tornado_Recreate(void* self) {
    if (!self) return nullptr;
    typedef void* (*dtor_t)(void*, uint32_t);
    void** vt = *reinterpret_cast<void***>(self);
    if (vt && vt[0]) reinterpret_cast<dtor_t>(vt[0])(self, 0);
    return Tornado_Build(self);
}

// Retail slot 7 (0x140001640): vtable[0](self, 0), tail. A shared thunk in retail.
void* Tornado_Destruct(void* self) {
    if (!self) return nullptr;
    typedef void* (*dtor_t)(void*, uint32_t);
    void** vt = *reinterpret_cast<void***>(self);
    if (vt && vt[0]) return reinterpret_cast<dtor_t>(vt[0])(self, 0);
    return self;
}

// Retail slot 9 (0x140024ed0): self ? ctor(self) : null.
void* Tornado_CreateInPlaceVSlot(void* self) {
    return self ? Tornado_Build(self) : nullptr;
}

// Capture the live base table on first use, then hand every Tornado the patched copy.
void InstallTornadoVtable(void* p) {
    void** vt = *reinterpret_cast<void***>(p);
    if (!vt) return;
    if (vt == s_tornadoVtable) return;        // re-entry from Recreate/CreateInPlace
    if (!s_vtableReady) {
        for (int k = 0; k < kParticleSlots; ++k) {
            s_baseVtable[k]    = vt[k];
            s_tornadoVtable[k] = vt[k];
        }
        s_tornadoVtable[3] = reinterpret_cast<void*>(&Tornado_GetFileVersion);
        s_tornadoVtable[5] = reinterpret_cast<void*>(&Tornado_Serialize);
        s_tornadoVtable[6] = reinterpret_cast<void*>(&Tornado_Recreate);
        s_tornadoVtable[7] = reinterpret_cast<void*>(&Tornado_Destruct);
        s_tornadoVtable[8] = reinterpret_cast<void*>(&Tornado_CreateInstance);
        s_tornadoVtable[9] = reinterpret_cast<void*>(&Tornado_CreateInPlaceVSlot);
        s_particleInstanceCtor = reinterpret_cast<fn_ParticleInstance_Ctor_t>(
            ResolveExport("??0ParticleInstance@Graphics@@QEAA@XZ"));
        s_vtableReady = true;
    }
    *reinterpret_cast<void**>(p) = s_tornadoVtable;
}
}  // namespace

static void* Tornado_Build(void* p) {
    if (!p) return nullptr;
    if (fn_Graphics_Particle_Ctor) fn_Graphics_Particle_Ctor(p);
    uint8_t* s = reinterpret_cast<uint8_t*>(p);
    // Tornado-specific defaults (FUN_1400d23c0). Field meanings are from
    // Tornado::Serialize (0x1400d2210, which names the types) cross-read against
    // TornadoInstance::Update (0x1400d2470, which consumes them).
    *(float*)(s + 0x48) = 4.0f;    // 0x40800000  radial swirl amplitude
    *(float*)(s + 0x4c) = 20.0f;   // 0x41a00000  vertical rise over a particle's life
    *(float*)(s + 0x50) = 5.0f;    // 0x40a00000  outer sweep amplitude
    *(float*)(s + 0x54) = 45.0f;   // 0x42340000  spin rate, degrees/sec
    *(float*)(s + 0x58) = 1.0f;    // 0x3f800000  per-particle angular frequency
    *(float*)(s + 0x5c) = 1.0f;    // 0x3f800000  emission RATE (interval = 1/x seconds)
    *(float*)(s + 0x60) = 10.0f;   // 0x41200000  particle lifetime, base
    *(float*)(s + 0x64) = 0.0f;    //             particle lifetime, random tolerance
    *(float*)(s + 0x68) = 0.2f;    // 0x3e4ccccd  min size clamp (also the initial size)
    *(float*)(s + 0x6c) = 3.0f;    // 0x40400000  max size clamp
    *(float*)(s + 0x70) = 30.0f;   // 0x41f00000  size scale, clamped into [+0x68,+0x6c]
    // +0x74 and +0x78 are two System::Colors — START and END tint — NOT "a qword -1".
    // Serialize reads them with System::Color::Serialize, and the instance update
    // divides their bytes by 255.0 (DAT_14017222c). All-0xFF = opaque white at both
    // ends. The bytes written here are unchanged; only the description was wrong.
    *(int32_t*)(s + 0x74) = -1;    // start colour RGBA = 0xFFFFFFFF
    *(int32_t*)(s + 0x78) = -1;    // end   colour RGBA = 0xFFFFFFFF
    // Two System::Time fields seeded with g_fUVScale (= 1.0f): fade-in and fade-out.
    if (fn_Time_Ctor_Float) {
        float uv = g_fUVScale ? *g_fUVScale : 1.0f;
        fn_Time_Ctor_Float(s + 0x80, uv);
        fn_Time_Ctor_Float(s + 0x88, uv);
    }
    InstallTornadoVtable(p);
    return p;
}
extern "C" void* Tornado_CreateInPlace_SA(void* mem) {
    TornadoFactoryTrace("CreateInPlace", mem);
    return Tornado_Build(mem);
}
extern "C" void* Tornado_Factory_SA(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x90, 8);
    void* built = Tornado_Build(p);
    TornadoFactoryTrace("Factory", built);
    return built;
}
REGISTER_TYPE(Tornado, "Graphics::Particle",
              &Tornado_Factory_SA, &Tornado_CreateInPlace_SA);
