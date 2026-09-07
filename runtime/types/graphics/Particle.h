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
    int32_t  _f28;     // +0x28  ctor stores 0xf (15)
    // +0x2c..+0x2f: UNINITIALISED by the ctor (measured gap, see above);
    // C++'s natural alignment padding before _f30 happens to cover it too,
    // same caveat as +0x21..+0x23.
    uint64_t _f30;     // +0x30  UNVERIFIED, zeroed by the ctor
    uint64_t _f38;     // +0x38  UNVERIFIED, zeroed by the ctor
    uint64_t _f40;     // +0x40  UNVERIFIED, zeroed by the ctor
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
struct ParticleBoundsRecord {
    uint8_t opaque[0x2c];   // Box-shaped prefix (0x18) + 0x14 UNKNOWN bytes
    int32_t frameTag;       // +0x2c
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
