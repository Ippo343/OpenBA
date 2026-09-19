// ============================================================================
// runtime/src/graphics/animation.cpp — Wave 6 lane G
// Graphics::AnimationEvent + Graphics::AnimationManager + Graphics::Particle +
// Graphics::ParticleInstance + Graphics::ParticleSystemInstance +
// Graphics::FrameCopyImageBuffer
// ============================================================================
//
// Scope: docs/runtime_port/wave6_lanes.txt "=== LANE G ===", 18 rows (15 code
// functions + 3 data symbols this lane cannot register -- see the bottom of
// this file). Layouts and the evidence for every offset are in
// runtime/types/graphics/Animation.h and Particle.h; this file's own header
// comments carry the per-function oracle argument.
//
//   | Export                                    | Retail      | Ord  | Status   | Oracle          |
//   |--------------------------------------------|-------------|------|----------|-----------------|
//   | ??0AnimationEvent@Graphics@@QEAA@XZ         | 0x1800a2150 |   41 | PORTED   | FULL (scratch)  |
//   | ??1AnimationEvent@Graphics@@UEAA@XZ         | 0x1800a2120 |  526 | PORTED   | structural only |
//   | ??0AnimationManager@Graphics@@QEAA@XZ       | 0x1800fc430 |   45 | PORTED   | FULL (scratch)  |
//   | ??1AnimationManager@Graphics@@QEAA@XZ       | 0x1800fc400 |  528 | PORTED   | post-state only |
//   | ?Update@AnimationManager@Graphics@@...      | 0x1800fc240 | 4473 | HELD BACK | -- see below   |
//   | ?Serialize@AnimationManager@Graphics@@...   | 0x1800fc190 | 3623 | HELD BACK | -- see below   |
//   | ??0Particle@Graphics@@QEAA@XZ               | 0x180103e80 |  298 | PORTED   | FULL (scratch)  |
//   | ??0ParticleInstance@Graphics@@QEAA@XZ       | 0x180104130 |  300 | PORTED   | FULL (scratch)  |
//   | ?Update@ParticleInstance@Graphics@@...      | 0x180104180 | 4498 | PORTED   | idempotent replay|
//   | ?IncrementInstanceIndex@ParticleInstance@...| 0x1800201f0 | 3065 | PORTED   | FULL             |
//   | ??0ParticleSystemInstance@Graphics@@...     | 0x1800a55a0 |  304 | PORTED   | FULL (scratch)  |
//   | ??0FrameCopyImageBuffer@Graphics@@QEAA@XZ   | 0x1800a3d10 |  198 | PORTED*  | see correction |
//   | ??1FrameCopyImageBuffer@Graphics@@UEAA@XZ   | 0x1800a3c80 |  597 | PORTED*  | see correction |
//   | ?Create@FrameCopyImageBuffer@Graphics@@...  | 0x1800a3b70 | 1877 | PORTED*  | see correction |
//   | ?Generate@FrameCopyImageBuffer@Graphics@...  | 0x1800a3b20 | 2395 | PORTED*  | see correction |
//
// *** CORRECTION (2026-09-12, vtable batch 6; PLAN.md §6 step 8). *** The four
// FrameCopyImageBuffer rows above said HELD BACK and the long "WHY 6 OF 15 CODE
// FUNCTIONS ARE HELD BACK" section below still argues for holding them. THAT
// DECISION WAS REVERSED: Wave 23 lane GFXBODY ported and registered all four in
// runtime/src/graphics/wave23_lane_gfxbody.cpp, and vtable batch 6 has since
// ported the REST of that class's table (slots 1/2/3/5/6/7/8/11) plus its two
// siblings in runtime/src/graphics/imagebuffer_derived.cpp, reaching wave23's
// ctor through the registry door. The four rows are marked PORTED* here rather
// than rewritten so that a reader who meets the old claim elsewhere recognises
// it. The prose below is kept as the HISTORICAL record of why the hold was made
// and is NOT a current disposition.
//
// Every name above was checked against the shipped export directory of
// C:\GOG Games\Banished\Runtime-x64.dll with pefile BEFORE any body was
// written (reference_resolveexport_phantom_names.md). All 15 code exports and
// both data exports used internally (Object::External::~External, Graphics::
// Geometry::Geometry/SetBounds) are present; addresses/ordinals above are
// that query's output, not Ghidra's.
//
// WHY 6 OF 15 CODE FUNCTIONS ARE HELD BACK
// -----------------------------------------
// **FrameCopyImageBuffer (ctor, dtor, Create, Generate) -- all four, as one
// block.** This class exists only to grab a screenshot
// (code/Game/Game/w2_takescreenshot.cpp). Its ctor/Create/Generate allocate
// and drive `Video::RenderBuffer` and `Video::ImageBuffer` objects through
// VIRTUAL CALLS into vtables this lane does not own -- `Video::Device` /
// `Video::Context` / `Video::RenderBuffer` are lanes A/B/C's scope this wave,
// and `Video::RenderBuffer::PlatformConstruct` ultimately dispatches into
// `dx11/`, our OWN DX11 replacement, on the other side. Getting the RenderBuffer
// layout even one field wrong here would corrupt a live GPU resource object
// two other lanes' code also touches this same wave -- exactly the
// "half-ported class" hazard PLAN §4.1 decision 5 names, except worse: the
// half this lane would be porting is entangled with THREE other lanes'
// in-flight layouts, not the DLL's own dormant internals. There is also no
// memcmp oracle for "did the GPU texture get copied correctly" -- only a
// screenshot pixel comparison would tell, which is outside RT_DIFF's model.
// RE-CHECKED IN THE WAVE 8.5 CLOSE-OUT and still held, but the FRAMING above
// has aged badly and would mislead the next reader: "lanes A/B/C's scope this
// wave" reads as resolved now that Wave 6 has closed with 0 divergences. The
// entanglement did NOT resolve -- `Video::RenderBuffer`'s ctor/dtor and
// `Video::Context`'s ctor are still individually unported and held back by
// name in runtime/src/video/context.cpp and device.cpp. The GPU-oracle
// argument above is the load-bearing one and is unaffected either way.
// See docs/runtime_port/WAVE8_5_HOLDBACK_RECHECK.md.
// Held back as a whole class rather than split, for the same reason
// WAVE5_LANE_D.md gives for NOT splitting Picking::Scene: a method of ours
// and a method of the DLL's operating on the same RenderBuffer must agree on
// every byte of a layout this lane was never asked to pin.
//
// **AnimationManager::Update and ::Serialize.** Both mutate through a nested,
// unexported `AnimationState` record (0x30-byte groups of them, in turn
// containing 0x48-byte `AnimationState` elements referencing `Graphics::
// GraphicsMesh::Node` arrays -- lane E's scope this wave -- and an untyped
// `InterpolatedState<Transform>*`) that this lane did not have budget to
// fully pin. Update is also the single largest blast-radius function in this
// lane's whole scope: it runs on EVERY character's skeleton, EVERY frame
// (gameupdateloop.cpp:1293), and already sits behind a default-on non-retail
// batching path (standalone/perf_experimental.cpp, ANIM_BATCH_ALL) this
// lane's own brief said to read before touching it. A wrong argument-slot
// mapping in the 9-argument `AnimationState::Update` call this function makes
// would silently corrupt every animated model's pose with no oracle cheaper
// than full visual QA. Serialize is a save-path function serializing the same
// unpinned AnimationState array -- Wave 5 held back every serializer-
// constructing function for want of an oracle
// (reference_slot3_wrapper_must_resolve_first.md: a serializer that consumes
// the wrong byte count desyncs the stream and surfaces LATER on an innocent
// record), and this lane applies the same standard. Both stay on the DLL;
// their exe dispatch slot (0x140152050) and ResolveExport call site
// (gameupdateloop_serialize.cpp:163) are UNCHANGED by this lane.
//
// WHAT THIS LEAVES SAFELY PORTABLE
// ---------------------------------
// The five ctors, the two dtors that don't touch Video, IncrementInstanceIndex
// (a one-line static counter bump) and ParticleInstance::Update (whose only
// external dependency, Graphics::Geometry::SetBounds, is EXPORTED, pure, and
// already fully decompiled -- no unexported internal left unpinned). These are
// exactly the functions where this lane could verify every byte the body
// touches.
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>

#include "../../funcs/rt_port.h"
#include "../../types/graphics/Animation.h"
#include "../../types/graphics/Particle.h"

using RtGraphics::AnimationEvent;
using RtGraphics::AnimationManager;
// ENDGAME B.3.5 block H1 — the GREP HAZARD this file now lives with. The two
// `using` lines above bind the PODs at FILE scope, but inside the
// `namespace Graphics { ... }` block at the bottom of this file a bare
// `AnimationEvent` / `AnimationManager` binds the REAL CLASS instead. Every POD
// spelling inside that block is qualified `RtGraphics::` for exactly that
// reason (B34_BRIEFS section A.7, RULE B4b-5).
using RtGraphics::Particle;
using RtGraphics::ParticleInstance;
using RtGraphics::ParticleSystemInstance;
using RtGraphics::ParticleResource;
using RtGraphics::ParticleBoundsRecord;

// ----------------------------------------------------------------------------
// ResolveAddr / the raw DLL door, forward-declared rather than pulled in from
// standalone/resolve.h -- same reasoning rt_port.h gives (resolve.h drags in
// <windows.h>, which collides with the engine's own type/member names). This
// declaration must stay byte-identical to resolve.h's.
// ----------------------------------------------------------------------------
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace {

// ============================================================================
// Mangled names, each its own literal at the registration call for
// tools/runtime_census.py (reference_census_cannot_see_nonliteral_registrations.md);
// kept as named constants here only for the diff-oracle the raw DLL door<> calls.
// ============================================================================
const char* const kAnimEventCtor   = "??0AnimationEvent@Graphics@@QEAA@XZ";
const char* const kAnimEventDtor   = "??1AnimationEvent@Graphics@@UEAA@XZ";
const char* const kExternalDtor    = "??1External@Object@@UEAA@XZ";
const char* const kAnimMgrCtor     = "??0AnimationManager@Graphics@@QEAA@XZ";
const char* const kAnimMgrDtor     = "??1AnimationManager@Graphics@@QEAA@XZ";
const char* const kParticleCtor    = "??0Particle@Graphics@@QEAA@XZ";
const char* const kPIctor          = "??0ParticleInstance@Graphics@@QEAA@XZ";
const char* const kPIupdate        = "?Update@ParticleInstance@Graphics@@QEAA_NXZ";
const char* const kPIincrement     = "?IncrementInstanceIndex@ParticleInstance@Graphics@@SAXXZ";
const char* const kPSIctor         = "??0ParticleSystemInstance@Graphics@@QEAA@XZ";
const char* const kGeometryCtor    = "??0Geometry@Graphics@@QEAA@XZ";
const char* const kSetBounds       = "?SetBounds@Geometry@Graphics@@QEAAXAEBVBox@Volume@@@Z";
const char* const kFrameIndexData  = "?_frameIndex@ParticleInstance@Graphics@@1HA";

// ============================================================================
// Retail vtable constants -- resolved once through ResolveAddr(Module::Runtime,
// ...) so they carry the DLL's actual loaded (ASLR-relocated) address, not the
// static analysis-time address. Each was read off a LEA immediately before a
// vtable store in the retail ctor's disassembly (disassemble_function,
// program="Runtime-x64.dll"), not inferred from the decompile's `_vftable_`
// symbol name alone.
// ============================================================================
void* RtAnimationEventVtable() {
    static void* v = reinterpret_cast<void*>(ResolveAddr(Module::Runtime, 0x180154bf0));
    return v;
}
void* RtParticleVtable() {
    static void* v = reinterpret_cast<void*>(ResolveAddr(Module::Runtime, 0x180155130));
    return v;
}
void* RtParticleInstanceVtable() {
    static void* v = reinterpret_cast<void*>(ResolveAddr(Module::Runtime, 0x1801550e0));
    return v;
}
void* RtParticleSystemInstanceVtable() {
    static void* v = reinterpret_cast<void*>(ResolveAddr(Module::Runtime, 0x180155040));
    return v;
}

// AnimationManager::_singleton, the exported static this class' ctor/dtor
// maintain. Resolved to the DLL's OWN live global rather than kept as a
// private copy, so any DLL-internal reader (or another wave's code) agrees
// with this port on the value. Retail 0x1801fe7b0, ordinal 4799,
// ?_singleton@AnimationManager@Graphics@@0PEAV12@EA.
AnimationManager** RtAnimMgrSingletonSlot() {
    static AnimationManager** p = reinterpret_cast<AnimationManager**>(
        ResolveAddr(Module::Runtime, 0x1801fe7b0));
    return p;
}

// Graphics::ParticleInstance::_frameIndex, a protected STATIC int (mangling
// code `1`), not an instance field -- see Particle.h. Resolved once via
// the raw DLL door (a real export, unlike the .rdata data slot addresses this
// project has elsewhere found to resolve NULL -- checked present in the
// export table before this file was written).
int* RtFrameIndexSlot() {
    static int* p = reinterpret_cast<int*>(ResolveExport(kFrameIndexData));
    return p;
}

// ============================================================================
// Graphics::Geometry::Geometry(void) -- retail 0x1800a4270, ordinal 205,
// EXPORTED (??0Geometry@Graphics@@QEAA@XZ). NOT this lane's class (Wave 6
// lane D owns Graphics::Geometry), called at its own export exactly like
// scene.cpp calls Picking::Scene's unexported grid internals -- except this
// one IS exported, so the raw DLL door applies (never the raw DLL door on a normal
// run path -- reference_rtoriginal_is_getprocaddress.md).
// ============================================================================
using GeometryCtorFn = void* (*)(void*);
void RtGeometryCtor(void* self) {
    // RT_EXPORT_ONCE, not the raw DLL door (Wave 9 lane R): Graphics::Geometry's
    // ctor IS ported (runtime/src/graphics/geometry.cpp), and the raw door reached
    // the DLL's body regardless. Raw is for oracles; a dependency goes through the
    // port registry.
    RT_EXPORT_ONCE(GeometryCtorFn, kGeometryCtor)(self);
}

// Graphics::Geometry::SetBounds(const Volume::Box&) -- retail 0x180015700,
// ordinal 3836, EXPORTED. Also not this lane's class; called at its own
// export from ParticleInstance::Update below, exactly as retail does.
using SetBoundsFn = void (*)(void*, const void*);
void RtGeometrySetBounds(void* self, const void* box) {
    RT_EXPORT_ONCE(SetBoundsFn, kSetBounds)(self, box);   // ported; see RtGeometryCtor
}

}  // namespace

// ============================================================================
// Graphics::AnimationEvent::AnimationEvent()
// retail 0x1800a2150, ordinal 41
//
// Reduces to four stores. The retail body ALSO stores two intermediate base
// vtables (Object::Base's, then Object::External's) before its own -- both
// are Ghidra's rendering of Object::External::External() folded fully
// inline, and neither survives past the function's last instruction (see
// Animation.h). Reproducing only the LAST vtable value is therefore
// byte-exact with retail's observable result, not a shortcut around it.
//
// ORACLE: full, scratch-object. Geometry::Geometry-style ctors in this file
// are proven pure (no external calls, no globals touched -- confirmed by
// decompile) so a scratch instance can be constructed on the stack, memcmp'd
// whole, and simply discarded (External's default-constructed state holds no
// resource, so no dtor call is needed to avoid a leak here).
// ============================================================================
extern "C" AnimationEvent* RtGraphics_AnimationEvent_Ctor(AnimationEvent* self) {

    self->vtable = RtAnimationEventVtable();
    self->_f08 = 0;
    self->_f10 = 0;
    self->_f18 = 0;

    return self;
}

// ============================================================================
// Graphics::AnimationEvent::~AnimationEvent()
// retail 0x1800a2120, ordinal 526 (virtual, UEAA)
//
// Two instructions: stamp the AnimationEvent vtable, then TAIL-JMP into
// Object::External::~External (retail 0x1800bc950, EXPORTED, ordinal 588 --
// found by disassembling this dtor, not guessed). This port calls that same
// export rather than re-deriving External's teardown, which this lane does
// not own the layout of.
//
// ORACLE: structural, not runtime. The entire computed part of this function
// is delegation into the identical retail internal both implementations
// would call; there is no second run to diff without a double-free (External
// may release a resource), and nothing else in AnimationEvent's own state is
// owned by this lane to check independently. Registered so calls= is visible
// in the diff summary, but the verdict is not evidence of anything beyond
// "did not crash".
// ============================================================================
extern "C" void RtGraphics_AnimationEvent_Dtor(AnimationEvent* self) {

    self->vtable = RtAnimationEventVtable();
    typedef void (*ExternalDtorFn)(AnimationEvent*);
    RT_EXPORT_ONCE(ExternalDtorFn, kExternalDtor)(self);   // ported; see RtGeometryCtor

}

// ============================================================================
// Graphics::AnimationManager::AnimationManager()
// retail 0x1800fc430, ordinal 45
//
// Zeroes a CRITICAL_SECTION-shaped 0x14-byte prefix (never actually
// initialised as a real lock -- see Animation.h) then seeds a 624-entry
// Mersenne-Twister state array from `_time64(NULL)` using the textbook
// `init_genrand` recurrence, and finally publishes `this` into the exported
// `_singleton`.
//
// ORACLE: full, scratch-object -- with one necessary exception. `_time64`
// advances between the two calls (this port's and the diff run's), so the
// FIRST rngState word (the raw time seed) and everything the recurrence
// derives from it will legitimately differ call to call. The oracle
// therefore reproduces retail's own recurrence bit-for-bit using OUR seed
// fed into it (not comparing the seed itself), which is the only thing this
// function actually computes -- the MT recurrence's correctness, independent
// of what time it happened to run.
// ============================================================================
extern "C" AnimationManager* RtGraphics_AnimationManager_Ctor(AnimationManager* self) {

    // Retail zeroes +0x00..+0x13 with three stores (8 + 8 + 4). The first
    // twelve bytes are a Collection::Array<AnimationGroup> — NOT the
    // CRITICAL_SECTION this file used to name; see the CORRECTION banner in
    // runtime/types/graphics/Animation.h. Zeroing them IS constructing an empty
    // array, so the bytes written here are unchanged by that correction.
    self->_groupData = nullptr;
    self->_groupCount = 0;
    self->_groupCapacity = 0;
    self->_f10 = 0;

    const uint32_t seed = static_cast<uint32_t>(std::time(nullptr));
    self->_rngState[0] = seed;
    for (int i = 1; i < 624; ++i) {
        const uint32_t prev = self->_rngState[i - 1];
        self->_rngState[i] = (prev ^ (prev >> 30)) * 0x6c078965u + static_cast<uint32_t>(i);
    }

    AnimationManager** slot = RtAnimMgrSingletonSlot();
    if (slot) *slot = self;

    return self;
}

// ============================================================================
// Graphics::AnimationManager::~AnimationManager()
// retail 0x1800fc400, ordinal 528
//
// Nulls `_singleton`, then calls FUN_18005bf60(this) treating `this` as an
// LPCRITICAL_SECTION -- a defensive teardown against a lock this class never
// actually calls InitializeCriticalSection on (DebugInfo is always null, so
// the only live branch inside FUN_18005bf60 -- freeing DebugInfo's block --
// never fires). Called at its own retail address (unexported, Win32-shaped,
// PLAN §4.1 decision 1) rather than reproduced.
//
// ORACLE: post-state only. A second run would renull an already-null
// singleton and re-run the no-op lock teardown -- harmless, but nothing new
// to observe, so this checks what retail's body must leave behind instead:
// `_singleton` cleared.
// ============================================================================
extern "C" void RtGraphics_AnimationManager_Dtor(AnimationManager* self) {

    AnimationManager** slot = RtAnimMgrSingletonSlot();
    if (slot) *slot = nullptr;

    // ENDGAME B.3.2 step 2 (callers) — NOT converted. 0x18005bf60 is the same
    // retail address `runtime/src/collection/array_destroy.cpp` registers as
    // `RtColl_ArrayDtorElem_18005bf60` (a one-argument `~Array<T>` whose
    // element holds two nested Arrays), but THIS call site does not touch any
    // Collection container at all: `self` (the whole AnimationManager) is
    // passed as if it were an `LPCRITICAL_SECTION`, per the banner above. There
    // is no `Collection::Array`/`LinkedList`/`Set` member here to name — the
    // shared address is coincidental reuse of one small function, not two
    // views of the same container — so this stays the by-address call onto
    // the SAME ported body (registry-first, so it already reaches
    // `RtColl_ArrayDtorElem_18005bf60`, not the DLL's).
    using Fn = void (*)(void*);
    static Fn fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x18005bf60));
    fn(self);

}

// ============================================================================
// Graphics::Particle::Particle()
// retail 0x180103e80, ordinal 298
//
// Pure stores, no external calls (disassembly confirmed) -- the same
// inlined-base-ctor shape as AnimationEvent.
//
// *** BYTES [0x21,0x24) AND [0x2c,0x30) ARE TWO GAPS RETAIL NEVER WRITES. ***
// Disassembly of 0x180103e80, the full store list:
//   +0x20 byte  = 1     +0x24 dword = 1     +0x28 dword = 0xf
//   +0x30 qword = 0     +0x38 qword = 0     +0x40 qword = 0
// Nothing touches +0x21/+0x22/+0x23 between the +0x20 and +0x24 stores, and
// nothing touches +0x2c/+0x2d/+0x2e/+0x2f between the +0x28 and +0x30 stores.
// Both were MEASURED by diff runs, one at a time, each initially reported as
// a divergent oracle (see the first as calls=1/divergences=1, comparing the
// real object's leftover heap bytes against this file's own 0xCD scratch
// poison in a range NEITHER implementation writes -- ours never wrote there
// either). The second gap was found only after the first fix narrowed the
// comparison to `[0x24,0x48)` and a follow-up run showed THAT range still
// diverging -- the cheaper way to find every gap in one pass is to walk the
// store list once, as above, and exclude every address it skips, rather than
// re-deriving one gap per run. Fixed the same way scene.cpp's Scene ctor
// oracle excludes its own two never-written dwords: the comparison below
// skips both gaps instead of raw-memcmp'ing the whole struct.
//
// ORACLE: full, scratch-object, minus the [0x21,0x24) and [0x2c,0x30) gaps.
// ============================================================================
extern "C" Particle* RtGraphics_Particle_Ctor(Particle* self) {

    self->vtable = RtParticleVtable();
    self->_f08 = 0;
    self->_f10 = 0;
    self->_f18 = 0;
    self->_active = 1;
    self->_f24 = 1;
    self->_f28 = 0xf;
    self->_f30 = 0;
    self->_f38 = 0;
    self->_f40 = 0;

    return self;
}

// ============================================================================
// Graphics::ParticleInstance::ParticleInstance()
// retail 0x180104130, ordinal 300
//
// Calls the EXPORTED, pure Geometry::Geometry(this) (see file banner), then
// stamps its own vtable and zeroes/initialises its three own fields.
//
// *** THE GEOMETRY BASE [0x00, 0xc0) IS DELIBERATELY NOT DIFFED. *** It is
// written by the SAME retail `Geometry::Geometry` export in both "ours" (the
// real object) and "theirs" (the scratch), so it can never genuinely
// diverge -- but `Geometry::Geometry`'s own disassembly (0x1800a4270) leaves
// a real gap at +0x14..+0x17 (a DWORD store to +0x10 followed directly by a
// QWORD store to +0x18), and a raw memcmp there was MEASURED to report a
// false divergence on every call: the real object's pre-existing heap bytes
// there happened to already be zero while this scratch's 0xCD poison filled
// the same untouched bytes with 0xCD. Comparing that region only measures
// which memory arena's leftover content survived, not a behavioural
// difference -- the fix is to not compare bytes neither implementation
// writes, the same rule Particle's ctor oracle above and scene.cpp's Scene
// ctor oracle both follow. `+0xd4` (this struct's own trailing alignment
// pad, after the `_index` dword at +0xd0) is the same kind of gap for OUR
// added fields and is excluded for the same reason.
//
// ORACLE: full over [0xc0, 0xd4) -- the three fields this class actually
// adds (`_resource`, `_boundsPtr`, `_index`). Geometry::Geometry is proven
// pure (no external calls in its own disassembly) so building "theirs" on
// the stack and discarding it remains safe.
// ============================================================================
extern "C" ParticleInstance* RtGraphics_ParticleInstance_Ctor(ParticleInstance* self) {

    RtGeometryCtor(&self->base);   // Geometry base, +0x00..+0xbf
    *reinterpret_cast<void**>(self) = RtParticleInstanceVtable();  // vtable lives at +0x00,
                                                                    // inside the Geometry base
    self->_resource  = nullptr;    // +0xc0
    self->_boundsPtr = nullptr;    // +0xc8
    self->_index     = -1;         // +0xd0

    return self;
}

// ============================================================================
// Graphics::ParticleInstance::Update()
// retail 0x180104180, ordinal 4498
//
// Reads `_resource` (+0xc0); if its `_active` byte (+0x20) is set, refreshes
// the cached bounds-record pointer `_boundsPtr` from `_resource->_boxes[
// _index]`. If the resource is active AND that record's per-frame stamp
// (+0x2c) already equals the global `ParticleInstance::_frameIndex`, this
// instance was already refreshed this frame: call `Geometry::SetBounds` with
// the cached record (treated as a `const Volume::Box&`, matching retail's own
// mangled signature) and return false. Otherwise stamp the record with the
// current frame index and return true (the caller, e.g. tornado.cpp's
// TornadoInstance::Update, treats this as "needs its own bounds computed").
//
// ORACLE: idempotent replay, the same shape scene.cpp uses for SetTransform.
// `Geometry::SetBounds` is exported, pure and confirmed idempotent by its own
// decompile (it only writes when the six floats actually differ, and a
// second call with the SAME record is therefore a true no-op) so replaying
// retail's body on the REAL object immediately after ours is safe. What is
// compared is the state BOTH calls must agree on: `_boundsPtr` and the
// record's frame-tag byte. The one thing this cannot independently confirm
// is the boolean return on the branch that DOES advance the tag (the very
// first call in a frame) -- a replay of that call necessarily lands on the
// "already tagged" branch and returns false, so there is no second
// observation of "true" to compare against. That branch's condition is the
// same one the byte-state check exercises on every call, so a clean replay
// across many calls is strong evidence for it, but it is not a proof, and is
// stated here rather than implied.
//
// FAITHFULLY UNGUARDED, per CLAUDE.md "don't fix a faithful crash with a null
// check": retail dereferences `_resource->_active` with NO null check on
// `_resource`, and (on the "needs update" path) writes through `_boundsPtr`
// with no null check either -- if a caller reaches this with a resource that
// was never attached, retail crashes, and so does this port. Neither guard
// is added.
// ============================================================================
extern "C" bool RtGraphics_ParticleInstance_Update(ParticleInstance* self) {

    ParticleResource* resource = self->_resource;             // +0xc0, unguarded
    if (resource->_active != 0) {                              // +0x20, unguarded
        self->_boundsPtr = resource->_boxes[self->_index];     // +0x38 array, +0xd0 index
    }

    int* frameIndex = RtFrameIndexSlot();
    bool needsUpdate;
    if (resource->_active != 0 && self->_boundsPtr->frameTag == *frameIndex) {
        RtGeometrySetBounds(&self->base, self->_boundsPtr);
        needsUpdate = false;
    } else {
        self->_boundsPtr->frameTag = *frameIndex;              // unguarded, faithful
        needsUpdate = true;
    }

    return needsUpdate;
}

// ============================================================================
// Graphics::ParticleInstance::IncrementInstanceIndex()
// retail 0x1800201f0, ordinal 3065
//
// One line: `_frameIndex += 1`. Increments the SAME live static the DLL
// itself reads (RtFrameIndexSlot resolves the export's actual address), so
// this port and any code still reading the DLL's own `_frameIndex` directly
// (there is none in our tree today, but the DLL's internal callers might)
// stay in agreement.
//
// ORACLE: full. Reads the pre-state, runs ours, then runs retail's own body
// AGAIN immediately after (a second incrementing call is not idempotent by
// itself, but comparing "ours advanced it by exactly 1" against "retail also
// advances it by exactly 1" from the same starting point is): both are run
// from independent snapshots restored around the calls so neither call's
// result depends on the other having run first.
// ============================================================================
extern "C" void RtGraphics_ParticleInstance_IncrementInstanceIndex() {

    int* slot = RtFrameIndexSlot();
    const int before = slot ? *slot : 0;
    if (slot) *slot = before + 1;

}

// ============================================================================
// Graphics::ParticleSystemInstance::ParticleSystemInstance()
// retail 0x1800a55a0, ordinal 304
//
// Re-runs ParticleInstance's own field inits inline (Geometry::Geometry,
// then a transient stamp of ParticleInstance::_vftable_, then the same three
// field writes) before overwriting the vtable with its own and zeroing one
// more field -- the same base-ctor-inlining shape seen throughout this file.
//
// *** MEASURED FALSE DIVERGENCE, SAME CAUSE AS ParticleInstance's, FIXED THE
// SAME WAY. *** A run diffing this ctor raw (whole-struct memcmp) reported
// divergences on every call at +0x14..+0x17 -- inside the Geometry base this
// class does not own, and the exact gap `Geometry::Geometry` itself leaves
// unwritten (see ParticleInstance's ctor above). Both implementations call
// the identical retail `Geometry::Geometry` there, so it can never
// genuinely diverge; the report was this file's own 0xCD scratch poison
// disagreeing with the real object's pre-existing (here: zeroed) heap bytes
// in a region NEITHER writes. Fixed by comparing only the fields this class
// itself sets, exactly as ParticleInstance's ctor now does.
//
// ORACLE: full over [0xc0, 0xd4) (inherited from ParticleInstance) and
// [0xd8, 0xe0) (this class's own `_f_d8`) -- everything this ctor actually
// writes, and nothing it, or Geometry::Geometry beneath it, leaves untouched.
// ============================================================================
extern "C" ParticleSystemInstance* RtGraphics_ParticleSystemInstance_Ctor(
    ParticleSystemInstance* self) {

    RtGeometryCtor(&self->base.base);
    self->base._resource  = nullptr;
    self->base._boundsPtr = nullptr;
    self->base._index     = -1;
    *reinterpret_cast<void**>(self) = RtParticleSystemInstanceVtable();  // vtable @ +0x00
    self->_f_d8 = nullptr;

    return self;
}

// ############################################################################
// ENDGAME B.3.5 step 5, block H1 — `Graphics::AnimationEvent`'s member bodies.
//
// They sit in their own `namespace Graphics { }` block AFTER every other
// namespace in this file has closed, because C++ requires a member definition
// to appear in a namespace ENCLOSING the class and `Graphics` and `RtGraphics`
// are siblings (B34_BRIEFS section A.2).
//
// WHAT MOVED AND WHAT DID NOT, stated plainly rather than implied:
//
//  * `TypeName()` / `Type()` are MEMBERS THAT FORWARD, not doors that delegate.
//    Their bodies are `RtGraphics_AnimationEvent_TypeName` /
//    `_Type` in `runtime/src/graphics/graphics_typeinfo_accessors.cpp`, which is
//    NOT block H1's file set, so the direction is reversed for these two: the
//    registered door is untouched and the member calls it. Exactly the split
//    `runtime/src/object/external_class_doors.cpp` documents.
//
//  * `~AnimationEvent()` IS DUPLICATED, NOT DELEGATED, and the door below is
//    left exactly as it was. Retail 0x1800a2120 stamps AnimationEvent's own
//    vtable and then tail-jumps `~External`; a real destructor chain emits both
//    for free, naming GCC's tables. The DOOR must keep naming the MINTED table,
//    because every live `AnimationEvent` in the process was built by the ctor
//    door and wears it — B34_RULES section 4.3's test ("would the member form
//    change which vtable a live object wears?") answers YES, so the two
//    spellings coexist. This is the `Object::External` precedent verbatim
//    (`external_class.cpp:152` beside `external.cpp`'s `RtObject_External_Dtor`).
//
//  * THE CONSTRUCTOR DOOR IS NOT A DELEGATION EITHER (B4b-2). It is called on
//    raw memory by `code/Game/Component/animaudioevent.cpp:45`, which then
//    stamps its own subclass table; the class has no real constructor at all.
// ############################################################################

extern "C" uint32_t    RtGraphics_AnimationEvent_Type(const void*);
extern "C" const char* RtGraphics_AnimationEvent_TypeName(const void*);

namespace Graphics {

// [1] ?TypeName@AnimationEvent@Graphics@@UEBAPEBDXZ  retail 0x1800116f0
//     `MOV RAX,qword ptr [?_typeInfo@AnimationEvent@Graphics@@ + 8]; RET`
//     — `BaseTypeInfo::name`, CONTENT-only (never compared by pointer).
const char* AnimationEvent::TypeName() const {
    return RtGraphics_AnimationEvent_TypeName(this);
}

// [2] ?Type@AnimationEvent@Graphics@@UEBAIXZ  retail 0x1800116d0
//     `MOV EAX,dword ptr [?_typeInfo@AnimationEvent@Graphics@@ + 0]; RET` —
//     0xFFFFFFFF until `Object::Factory::Startup` runs.
uint32_t AnimationEvent::Type() const {
    return RtGraphics_AnimationEvent_Type(this);
}

// ??1AnimationEvent@Graphics@@UEAA@XZ  retail 0x1800a2120.
//
// EMPTY ON PURPOSE. The two instructions retail spends here are the vtable
// stamp (the chain emits it on entry to this destructor) and the tail jump into
// `~External` (the chain emits that on exit). Writing either by hand would name
// the minted table, which is the one thing a converted object must not carry.
AnimationEvent::~AnimationEvent() {}

}  // namespace Graphics

// ============================================================================
// Registration.
//
// Only the nine PORTED functions above are registered here. The six HELD
// BACK functions (FrameCopyImageBuffer's four, AnimationManager::Update and
// ::Serialize) are intentionally absent -- every call site for them keeps
// resolving to the DLL exactly as before this lane ran. Both mangled name
// and retail address are registered for every port, matching every prior
// wave's convention (this lane's call-site table in wave6_lanes.txt shows a
// mix of `export` and `addr` callers for several of these rows).
// ============================================================================
int RtGraphicsAnimationRegister() {
    int count = 0;

    RegisterRuntimeExport("??0AnimationEvent@Graphics@@QEAA@XZ", (void*)&RtGraphics_AnimationEvent_Ctor); ++count;
    RegisterRuntimeAddr(0x1800a2150, (void*)&RtGraphics_AnimationEvent_Ctor);

    RegisterRuntimeExport("??1AnimationEvent@Graphics@@UEAA@XZ", (void*)&RtGraphics_AnimationEvent_Dtor); ++count;
    RegisterRuntimeAddr(0x1800a2120, (void*)&RtGraphics_AnimationEvent_Dtor);

    RegisterRuntimeExport("??0AnimationManager@Graphics@@QEAA@XZ", (void*)&RtGraphics_AnimationManager_Ctor); ++count;
    RegisterRuntimeAddr(0x1800fc430, (void*)&RtGraphics_AnimationManager_Ctor);

    RegisterRuntimeExport("??1AnimationManager@Graphics@@QEAA@XZ", (void*)&RtGraphics_AnimationManager_Dtor); ++count;
    RegisterRuntimeAddr(0x1800fc400, (void*)&RtGraphics_AnimationManager_Dtor);

    RegisterRuntimeExport("??0Particle@Graphics@@QEAA@XZ", (void*)&RtGraphics_Particle_Ctor); ++count;
    RegisterRuntimeAddr(0x180103e80, (void*)&RtGraphics_Particle_Ctor);

    RegisterRuntimeExport("??0ParticleInstance@Graphics@@QEAA@XZ", (void*)&RtGraphics_ParticleInstance_Ctor); ++count;
    RegisterRuntimeAddr(0x180104130, (void*)&RtGraphics_ParticleInstance_Ctor);

    RegisterRuntimeExport("?Update@ParticleInstance@Graphics@@QEAA_NXZ", (void*)&RtGraphics_ParticleInstance_Update); ++count;
    RegisterRuntimeAddr(0x180104180, (void*)&RtGraphics_ParticleInstance_Update);

    RegisterRuntimeExport("?IncrementInstanceIndex@ParticleInstance@Graphics@@SAXXZ", (void*)&RtGraphics_ParticleInstance_IncrementInstanceIndex); ++count;
    RegisterRuntimeAddr(0x1800201f0, (void*)&RtGraphics_ParticleInstance_IncrementInstanceIndex);

    RegisterRuntimeExport("??0ParticleSystemInstance@Graphics@@QEAA@XZ", (void*)&RtGraphics_ParticleSystemInstance_Ctor); ++count;
    RegisterRuntimeAddr(0x1800a55a0, (void*)&RtGraphics_ParticleSystemInstance_Ctor);

    return count;
}
