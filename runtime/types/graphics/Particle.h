#pragma once
// ============================================================================
// runtime/types/graphics/Particle.h — Wave 6 lane G — Graphics::Particle /
// Graphics::ParticleInstance / Graphics::ParticleSystemInstance layouts
// ============================================================================
//
// Every offset is MEASURED against Runtime-x64.dll (program="Runtime-x64.dll")
// via decompile_function + disassemble_function; the pinning function is
// named next to each field. Nothing here is copied from the profile DLL --
// this lane read the retail export table directly (tools/pe_exports.py-style
// pefile query) before writing any body
// (reference_resolveexport_phantom_names.md).
// ============================================================================

#include <cstdint>
#include <cstddef>

namespace RtGraphics {

// ============================================================================
// Graphics::Particle — 0x48 bytes KNOWN (the base class only; subclasses such
// as Tornado, which is already ported in code/Game/Game/tornado.cpp, add
// their own fields after it -- tornado.cpp's own comment confirms "Tornado
// (size 0x90) extends Graphics::Particle").
//
// Pinned by the ctor (retail 0x180103e80, disassembled). Exactly like
// AnimationEvent, the first two vtable stores (0x180154788 Object::Base,
// 0x1801547a0 Object::External) are the inlined base ctor and are immediately
// overwritten -- only the LAST store (0x180155130, Particle::_vftable_)
// persists. No external call happens anywhere in this ctor; it is pure
// stores, which is why this port needs no DLL internal for it at all.
//
// +0x30/+0x38/+0x40 are three zeroed qwords this lane could not identify any
// reader for (Particle's other 8 exported virtuals -- slots 6/7/8 in the
// vtable -- are all `_purecall`, per tornado.cpp's own comment, so the base
// class itself never touches them; only a live subclass would). Left
// UNVERIFIED rather than guessed.
//
// `Particle::_rng` (`?_rng@Particle@Graphics@@1VRandom@System@@A`, retail
// 0x1801fe7f0) and `Particle::_typeInfo`
// (`?_typeInfo@Particle@Graphics@@0VBaseTypeInfo@Object@@A`, retail
// 0x1801fa7b0) are PROTECTED STATIC class members (mangling code `1` and `0`
// respectively), not instance fields -- they do not appear in this struct.
// See animation.cpp for why they cannot be registered by this lane.
// ============================================================================
// *** [0x21,0x24) AND [0x2c,0x30) ARE TWO GAPS THE CTOR NEVER WRITES --
// MEASURED, not merely inferred from C++'s natural alignment padding. ***
// The full store list from disassembly of 0x180103e80:
//   +0x20 byte  = 1     +0x24 dword = 1     +0x28 dword = 0xf
//   +0x30 qword = 0     +0x38 qword = 0     +0x40 qword = 0
// Nothing touches +0x21/+0x22/+0x23 (between the +0x20 and +0x24 stores) or
// +0x2c/+0x2d/+0x2e/+0x2f (between the +0x28 and +0x30 stores). Two diff runs
// confirmed both the hard way, one gap at a time: an early revision of this
// port's ctor oracle raw-memcmp'd the whole struct and reported a divergence
// on every call, because the REAL object's leftover heap bytes in each gap
// differed from the diff harness's own 0xCD scratch-poison fill of the same
// untouched range -- not because either implementation actually wrote
// different values there. The cheap way to find every such gap in one pass
// is to walk the store list once (as above) and exclude every address it
// skips, rather than re-deriving one gap per run. animation.cpp's oracle now
// explicitly excludes both ranges from the comparison instead of masking
// them with initialized fields.
struct Particle {
    void*    vtable;   // +0x00  retail final value 0x180155130 (Particle::_vftable_)
    uint64_t _f08;     // +0x08  Object::External field, zeroed by the ctor
    uint64_t _f10;     // +0x10  Object::External field, zeroed by the ctor
    uint64_t _f18;     // +0x18  Object::External field, zeroed by the ctor
    uint8_t  _active;  // +0x20  ctor stores 1
    // +0x21..+0x23: UNINITIALISED by the ctor (measured gap, see above);
    // C++'s natural alignment padding before _f24 happens to cover it, but
    // that is coincidence, not the reason -- retail's own asm skips it.
    int32_t  _f24;     // +0x24  ctor stores 1
                       //        WAVE 27 LANE GFXPART, MEASURED: the POOL RECORD
                       //        COUNT. ?Serialize@Particle@Graphics@@ (0x180103f00)
                       //        passes it to Array<void*>::SetSize over +0x38 and
                       //        then builds exactly that many records;
                       //        ?SetParticle@ParticleInstance@ (0x180104a00) picks
                       //        a record with `rng % this->_f24`.
    int32_t  _f28;     // +0x28  ctor stores 0xf (15)
                       //        WAVE 27 LANE GFXPART, MEASURED: the PER-RECORD
                       //        SUB-ARRAY SIZE. The record ctor (0x180103bf0)
                       //        reads `particle[+0x28]` and hands it to a second
                       //        SetSize over the record's own +0x18 array.
    // +0x2c..+0x2f: UNINITIALISED by the ctor (measured gap, see above);
    // C++'s natural alignment padding before _f30 happens to cover it too,
    // same caveat as +0x21..+0x23.
    // *** THE THREE "UNVERIFIED" QWORDS BELOW WERE MEASURED BY WAVE 27 LANE
    // GFXPART. *** The names are LEFT ALONE on purpose: animation.cpp's ctor
    // oracles compare byte RANGES of this struct, and renaming is free while
    // re-shaping is not. Read the comments, not the identifiers.
    uint64_t _f30;     // +0x30  ExternalPtr<Graphics::MaterialInstance>.
                       //        TWO INDEPENDENT WITNESSES, which is why this is
                       //        MEASURED and not inferred:
                       //        (a) ?Serialize@Particle@Graphics@@ hands `&+0x30`
                       //            to the Object::ExternalBasePtr<T>::Serialize
                       //            instantiation at 0x180014520 -- the same body
                       //            shape goal_serialize.cpp already reaches;
                       //        (b) ?AddDrawItems@ParticleInstance@Graphics@@
                       //            (0x180104700) loads `*(void**)(particle+0x30)`
                       //            and passes it as Renderer::AddDrawItem's
                       //            `const MaterialInstance*` argument.
                       //        An ExternalPtr is one pointer wide and holds the
                       //        pointee at +0x00, which is why (b) is a plain load.
    uint64_t _f38;     // +0x38  Collection::Array<void*>::_data -- the pool-record
                       //        pointer array. Serialize calls SetSize (0x180062110)
                       //        on `&+0x38` and stores through `*(void**)(this+0x38)`;
                       //        the PROFILE build's ~Particle (0x18011cdf0) walks
                       //        `*(void**)(this+0x38) + i*8` and MemFrees each
                       //        element's own +0x18 buffer, then the block.
    uint64_t _f40;     // +0x40  that Array's _size (int32 @ +0x40) and _capacity
                       //        (int32 @ +0x44) as one qword -- SetSize writes
                       //        arr[+0x8] and arr[+0xc] with arr = this+0x38, and
                       //        the profile ~Particle's loop bound is
                       //        `*(int*)(this+0x40)`.
};
static_assert(sizeof(Particle) == 0x48, "Graphics::Particle base must be 0x48 bytes");
static_assert(offsetof(Particle, _active) == 0x20, "Particle::_active @ +0x20");
static_assert(offsetof(Particle, _f28) == 0x28, "Particle::_f28 @ +0x28");
static_assert(offsetof(Particle, _f40) == 0x40, "Particle::_f40 @ +0x40");

// ============================================================================
// The per-index record inside a ParticleResource's bounds array (see
// ParticleResource below). Geometry::SetBounds's mangled name
// (?SetBounds@Geometry@Graphics@@QEAAXAEBVBox@Volume@@@Z) says retail passes
// this pointer AS a `const Volume::Box&`, so its first 0x18 bytes are
// Box-shaped (Volume::Box is {Vector3 min; Vector3 max;}, see
// runtime/types/volume/Volume.h) -- but bytes 0x18..0x2b are UNKNOWN (this
// lane never needs to read or write them, only pass the pointer through), and
// +0x2c is NOT part of Volume::Box (which is only 0x18 bytes total) -- it is
// this record's own trailing field, a per-frame "last touched" stamp compared
// against Graphics::ParticleInstance::_frameIndex.
//
// Pinned by ParticleInstance::Update (retail 0x180104180, decompiled): reads
// and writes `*(int*)(record + 0x2c)`.
// ============================================================================
// *** SIZE CORRECTION, WAVE 27 LANE GFXPART, MEASURED. *** This record is
// 0x40 BYTES, not 0x30. Both of retail's allocation sites ask for exactly 0x40
// (the inlined `Enter/Allocate(0x40,8)/Leave` in ?Serialize@Particle@Graphics@@
// at 0x180103fed, and the per-TU `operator new(0x40)` at 0x180061a80 that
// ?SetParticle@ParticleInstance@ calls), and its constructor (0x180103bf0,
// UNNAMED in the export directory) writes through +0x38. The constructor's full
// store list, disassembled:
//     +0x00..+0x0c = 1e8f  (Box::min seeded to +infinity-ish)
//     +0x0c..+0x18 = -1e8f (Box::max seeded to -infinity-ish)
//     +0x18 = 0            the live-particle Array's _data
//     +0x20 = 0            that Array's _size (+0x24 its capacity)
//     +0x28 = 0            the LIVE PARTICLE COUNT -- read by both Draw bodies
//                          and by ?AddDrawItems@, which returns early on zero
//     +0x2c = -1           the per-frame tag (CONFIRMS the field below)
//     +0x30 = 0, +0x38 = 0
//   then SetSize(this+0x18, particle[+0x28]) through a second unnamed
//   instantiation at 0x180061bb0.
// The `frameTag` reading below is CONFIRMED by that -1 seed and by
// ?Update@ParticleSystemInstance@ comparing it against
// ?_frameIndex@ParticleInstance@Graphics@@ (0x1801fe77c).
// The struct is LEFT AT ITS OLD SHAPE deliberately -- widening it would change
// nothing about the bytes and would move the row's ownership away from the lane
// whose oracles compare it; wave27_lane_gfxpart.cpp uses named offset constants
// (kPool_*) for the four fields it reads.
struct ParticleBoundsRecord {
    uint8_t opaque[0x2c];   // Box-shaped prefix (0x18) + array/count fields, see above
    int32_t frameTag;       // +0x2c   ctor seeds -1
};
static_assert(offsetof(ParticleBoundsRecord, frameTag) == 0x2c, "ParticleBoundsRecord::frameTag @ +0x2c");

// ============================================================================
// The resource object a ParticleInstance references at +0xc0 (see below).
// This is NOT this lane's class to own the full layout of -- it is some
// Runtime resource/description type (a compiled ParticleSystem or similar)
// that this lane only reads two fields of, both pinned by
// ParticleInstance::Update:
//   +0x20 `_active`      -- a bool/flag byte; when zero, Update never derefs
//                           the array below or touches `_boxes`.
//   +0x38 `_boxes`        -- base pointer of an array of ParticleBoundsRecord*
//                           (read as `*(longlong*)(resource+0x38) + index*8`,
//                           i.e. an array of POINTERS, stride 8).
// Everything else about this type is opaque and unnamed.
// ============================================================================
struct ParticleResource {
    uint8_t                 _pad0[0x20];
    uint8_t                 _active;   // +0x20
    uint8_t                 _pad21[0x17];
    ParticleBoundsRecord**  _boxes;    // +0x38
};
static_assert(offsetof(ParticleResource, _active) == 0x20, "ParticleResource::_active @ +0x20");
static_assert(offsetof(ParticleResource, _boxes) == 0x38, "ParticleResource::_boxes @ +0x38");

// ============================================================================
// Graphics::Geometry — NOT this lane's class (Wave 6 lane D owns
// Graphics::Geometry + Scene, runtime/src/graphics/scene.cpp). Every field
// ParticleInstance/ParticleSystemInstance add sits AFTER a 0xc0-byte Geometry
// base, pinned by both ctors calling `Geometry::Geometry(this)` (retail
// 0x1800a4270, exported as `??0Geometry@Graphics@@QEAA@XZ`) and then writing
// their OWN first field at exactly +0xc0 with no gap. This lane calls that
// ctor at its own export rather than re-deriving Geometry's internals, the
// same "what stays in the DLL" shape scene.cpp uses for Picking::Scene's
// unexported grid helpers.
//
// *** +0x14..+0x17 IS A GAP `Geometry::Geometry` NEVER WRITES -- MEASURED. ***
// Disassembly of 0x1800a4270 shows a DWORD store to +0x10 (`*(undefined4*)
// (this+0x10) = 0xffffffff`) followed directly by a QWORD store to +0x18
// (`*(undefined8*)(this+0x18) = 0`), with +0x14..+0x17 untouched in between.
// A diff run confirmed this in practice: this port's ParticleInstance and
// ParticleSystemInstance ctor oracles originally raw-memcmp'd their whole
// struct (including this opaque base, on the theory that BOTH implementations
// call the identical retail Geometry::Geometry so it can never diverge --
// true, but irrelevant to a byte NEITHER call writes). The real object's
// pre-existing heap bytes there happened to be zero while the scratch
// oracle's own 0xCD poison filled the same gap with 0xCD, so every call
// reported a false divergence. animation.cpp's ctor oracles now compare only
// the fields ParticleInstance/ParticleSystemInstance themselves add
// ([0xc0,0xd4) and, for the latter, [0xd8,0xe0)) and do not compare this
// base at all -- it is written by the exact same export in both cases, so
// there is nothing this lane's oracle could add by re-checking it.
// ============================================================================
struct GeometryBase {
    uint8_t opaque[0xc0];
};

// ============================================================================
// Graphics::ParticleInstance : Graphics::Geometry — 0xd8 bytes.
//
// Pinned by the ctor (retail 0x180104130, decompiled + disassembled): after
// `Geometry::Geometry(this)`, it stamps `this->vtable = &ParticleInstance::
// _vftable_` (retail 0x1801550e0, LEA target confirmed by disassembly) and
// zeroes +0xc0/+0xc8, then sets +0xd0 = -1 (0xffffffff).
//
// +0xc0 `_resource`  -- a ParticleResource* (see above), null until a
//                       subclass or ParticleComponent attaches one.
// +0xc8 `_boundsPtr` -- a cached ParticleBoundsRecord*, pinned by Update.
// +0xd0 `_index`     -- the instance's index into `_resource->_boxes`,
//                       initialised to -1 ("no instance assigned yet").
//
// The ctor's diff oracle compares only [0xc0,0xd4) (see GeometryBase's
// banner above for why the base is excluded, and why +0xd4's alignment pad
// is too).
// ============================================================================
struct ParticleInstance {
    GeometryBase        base;        // +0x00, 0xc0 bytes, Graphics::Geometry
    ParticleResource*   _resource;   // +0xc0
    ParticleBoundsRecord* _boundsPtr;// +0xc8
    int32_t             _index;      // +0xd0
    int32_t             _pad_d4;     // +0xd4  alignment; not written by the ctor
};
static_assert(sizeof(ParticleInstance) == 0xd8, "Graphics::ParticleInstance must be 0xd8 bytes");
static_assert(offsetof(ParticleInstance, _resource) == 0xc0, "ParticleInstance::_resource @ +0xc0");
static_assert(offsetof(ParticleInstance, _boundsPtr) == 0xc8, "ParticleInstance::_boundsPtr @ +0xc8");
static_assert(offsetof(ParticleInstance, _index) == 0xd0, "ParticleInstance::_index @ +0xd0");

// ============================================================================
// Graphics::ParticleSystemInstance : Graphics::ParticleInstance — 0xe0 bytes.
//
// Pinned by the ctor (retail 0x1800a55a0, decompiled + disassembled): calls
// `Geometry::Geometry(this)`, transiently stamps `&ParticleInstance::
// _vftable_` (0x1801550e0, same constant as ParticleInstance's own ctor --
// this is retail re-running ParticleInstance's field inits inline rather
// than calling it, exactly like the AnimationEvent/Particle base-ctor
// inlining above), zeroes +0xc0/+0xc8, sets +0xd0 = -1, THEN overwrites the
// vtable with its own (0x180155040, ParticleSystemInstance::_vftable_) and
// zeroes one more field at +0xd8.
//
// The ctor's diff oracle compares [0xc0,0xd4) (inherited) and [0xd8,0xe0)
// (own) -- not the Geometry base, for the same measured reason given above.
// ============================================================================
struct ParticleSystemInstance {
    ParticleInstance base;   // +0x00, 0xd8 bytes
    void*            _f_d8; // +0xd8  UNVERIFIED beyond "zeroed by the ctor"
};
static_assert(sizeof(ParticleSystemInstance) == 0xe0, "Graphics::ParticleSystemInstance must be 0xe0 bytes");
static_assert(offsetof(ParticleSystemInstance, _f_d8) == 0xd8, "ParticleSystemInstance::_f_d8 @ +0xd8");

}  // namespace RtGraphics
