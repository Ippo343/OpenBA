// ============================================================================
// runtime/src/graphics/wave27_lane_gfxpart.cpp — Wave 27 lane GFXPART
//   the Graphics particle vtable slot bodies
// ============================================================================
//
// Triage for every row this lane was dealt — ported, deferred, and why — is
// docs/runtime_port/WAVE27_LANE_GFXPART.md. Read §4 before adding a body here.
//
// EXPORT TABLE, read with pefile out of the shipped
// C:\GOG Games\Banished\Runtime-x64.dll BEFORE a line of this file was written
// (reference_resolveexport_phantom_names.md — a name the DLL does not contain
// resolves NULL in silence):
//
//   | Export                                                 | Retail      |
//   |--------------------------------------------------------|-------------|
//   | ?Serialize@Particle@Graphics@@UEAA_NAEAVStream@IO@@@Z   | 0x180103f00 |
//   | ?Draw@ParticleInstance@Graphics@@UEBAX…MHHH@Z           | 0x180104230 |
//   | ?AddDrawItems@ParticleInstance@Graphics@@UEBAX…@Z       | 0x180104700 |
//   | ?GetInstanceTransform@ParticleInstance@Graphics@@…HM@Z  | 0x1801047f0 |
//   | ?SetParticle@ParticleInstance@Graphics@@UEAAX…@Z        | 0x180104a00 |
//   | ?SetParticle@ParticleSystemInstance@Graphics@@UEAAX…@Z  | 0x1800a6a80 |
//
// All six are vtable slots of `??_7ParticleInstance@Graphics@@6B@` (DLL
// 0x1801550e0), `??_7ParticleSystemInstance@Graphics@@6B@` (0x180155040) and
// `??_7Particle@Graphics@@6B@` (0x180155130). The three ParticleInstance bodies
// are SHARED slots: they appear at the same indices in the
// ParticleSystemInstance table, so one registration closes two rows.
//
// EVERY BODY BELOW WAS TRANSCRIBED FROM `disassemble_function`, NOT FROM
// GHIDRA'S C (reference_ghidra_c_output_vs_disassembly.md: the decompiler drops
// dereferences, prints MOVSXD as a zero extension, and drops whole arguments at
// a mistyped vtable call — and these bodies are nothing but mistyped vtable
// calls and float chains).
//
// ---------------------------------------------------------------------------
// THE SHARED RNG, AND WHY A NEW GENERATOR WOULD BE A SILENT SAVE DESYNC
// ---------------------------------------------------------------------------
// `?_rng@Particle@Graphics@@1VRandom@System@@A` (retail 0x1801fe7f0) is a
// PROTECTED STATIC `System::Random` — one MT19937 shared by every Particle in
// the process, seeded from `time()` at load by 0x180149650. It is tagged
// RT_UNPROMOTABLE | HARMFUL in standalone/reg_mint_promoted.cpp on the
// DLL-PRESENT route, precisely because in-DLL RIP-relative readers keep using
// the DLL's copy.
//
// `ParticleInstance::SetParticle` DRAWS FROM IT, and retail INLINES the draw —
// the tempering, the index advance and the 0x270 wrap are in SetParticle's own
// bytes, not behind a call. So this port must read and write THAT OBJECT, at
// that address, through the same door `code/Game/Game/tornado.cpp:253` already
// uses. Constructing our own generator would compile, run, look right, and
// desync every particle placement from the DLL's other consumers — a divergence
// that would not look like a port bug.
//
// The tempering constants below are the SHIPPED ones, lifted from the
// disassembly rather than retyped from an MT19937 reference: retail's masks are
// `0xff3a58ad` and `0xffffdf8c`, which are NOT the textbook `0x9d2c5680` /
// `0xefc60000`. They are those constants with the bits that the shift discards
// already cleared (`0x9d2c5680 & (0xffffffff >> 7)` style), which is what MSVC
// does when it can prove the high bits are shifted out. Retyping the textbook
// values would be arithmetically equivalent HERE and is still not what this file
// does, because nothing in the port proves the shift amounts it pairs them with
// (reference_crt_coefficients_must_be_lifted_not_retyped.md).
//
// ---------------------------------------------------------------------------
// FLOAT ASSOCIATION ORDER IS LOAD-BEARING
// ---------------------------------------------------------------------------
// Every lerp below is written in retail's own order — `prev + (cur - prev) * a`
// where retail subtracts, multiplies and adds in that sequence, and
// `((1 - a) * prev) + (a * cur)` where it does that instead. The two are
// algebraically identical and numerically different
// (reference_moved_term_hides_where_terms_cancel.md), and a particle system is
// exactly where a 1-ULP difference becomes a visibly different frame.
//
// `1.0f` appears in three of these bodies as `[0x18016169c]`. Its shipped bit
// pattern was read out of the DLL with read_memory (0x3F800000) rather than
// assumed from the mnemonic.
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/graphics/Geometry.h"
#include "../../types/graphics/Particle.h"
#include "../../types/io/Serializer.h"
#include "../../types/math/Math.h"

using RtGraphics::Geometry;
using RtGraphics::Particle;
using RtGraphics::ParticleInstance;
using RtGraphics::ParticleSystemInstance;
using rt::io::Encoding;
using rt::io::ReaderWriter;
using rt::io::Serializer;
using rt::io::Stream;

// Forward-declared rather than pulled in from standalone/resolve.h, for the
// reason rt_port.h gives: that header drags in <windows.h> and runtime/src/ must
// stay platform-neutral. Byte-identical to resolve.h's declaration; the linker
// checks the two spellings agree.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace rtcoll {
// Lane Q's `?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z` (0x1800c7a50) door:
// `EnterCriticalSection(_singleton+0x640); Allocate(_singleton, n, align);
// LeaveCriticalSection`. Declared, not re-derived, so lane E has ONE place to
// repoint when the allocator moves. Defined in runtime/src/collection/array.cpp.
void* MemAlloc(uint64_t bytes, int align);
}  // namespace rtcoll

// ============================================================================
// Attendance counters. A run that entered none of these says so rather than
// staying silent, and `calls=` is read separately from any comparison
// (reference_uncalled_port_is_not_verified.md).
// ============================================================================
namespace {

uint64_t g_particleSerialize   = 0;
uint64_t g_particleSerRecords  = 0;
uint64_t g_piDraw              = 0;
uint64_t g_piDrawQuads         = 0;
uint64_t g_piAddDrawItems      = 0;
uint64_t g_piAddDrawItemsEmit  = 0;
uint64_t g_piGetInstanceXform  = 0;
uint64_t g_piSetParticle       = 0;
uint64_t g_piSetParticleRand   = 0;
uint64_t g_psiSetParticle      = 0;

// ---------------------------------------------------------------------------
// Retail's shipped `1.0f`, at 0x18016169c. read_memory says 0x3F800000.
// ---------------------------------------------------------------------------
constexpr uint32_t kOneBits = 0x3F800000u;
inline float One() {
    float f;
    const uint32_t bits = kOneBits;
    std::memcpy(&f, &bits, sizeof f);
    return f;
}
// 0x18016167c = 0x3C8EFA35 = pi/180. ParticleInstance::Draw multiplies the
// particle's rotation field by it, so the field is stored in DEGREES. Lifted as
// a bit pattern, not retyped as `3.14159f / 180.0f`, which is a DIFFERENT float.
constexpr uint32_t kDegToRadBits = 0x3C8EFA35u;
inline float DegToRad() {
    float f;
    const uint32_t bits = kDegToRadBits;
    std::memcpy(&f, &bits, sizeof f);
    return f;
}

template <typename Fn>
Fn RtAddr(uintptr_t ghidra) {
    return reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, ghidra));
}

inline uint8_t*       B(void* p)        { return static_cast<uint8_t*>(p); }
inline const uint8_t* B(const void* p)  { return static_cast<const uint8_t*>(p); }
inline float  F(const void* p, size_t off) {
    float v; std::memcpy(&v, B(p) + off, sizeof v); return v;
}
inline void   SetF(void* p, size_t off, float v) { std::memcpy(B(p) + off, &v, sizeof v); }
inline int32_t I32(const void* p, size_t off) {
    int32_t v; std::memcpy(&v, B(p) + off, sizeof v); return v;
}

void** VTableOf(const void* obj) { return *reinterpret_cast<void* const*>(obj)
                                         ? *reinterpret_cast<void** const*>(obj) : nullptr; }
void*  VSlot(const void* obj, size_t byteOffset) {
    return VTableOf(obj)[byteOffset / sizeof(void*)];
}

// ---------------------------------------------------------------------------
// `Graphics::Particle`, the fields THIS lane pins. runtime/types/graphics/
// Particle.h owns the struct; these are the three offsets whose MEANING this
// lane measured for the first time (see that header's updated comments):
//   +0x30  ExternalPtr<MaterialInstance>  — corroborated from BOTH ends:
//          Particle::Serialize hands &+0x30 to the ExternalBasePtr::Serialize
//          instantiation at 0x180014520, and ParticleInstance::AddDrawItems
//          passes `*(void**)(particle+0x30)` as Renderer::AddDrawItem's
//          `const MaterialInstance*` argument.
//   +0x38  the record array's data pointer
//   +0x40  the record array's size (int32), +0x44 its capacity
//          — pinned by the profile build's ~Particle, which loops
//          `i < *(int*)(this+0x40)` over `*(void**)(this+0x38) + i*8` and
//          MemFrees each element's own +0x18 buffer, then the block.
// ---------------------------------------------------------------------------
constexpr size_t kParticle_Active      = 0x20;  // bool
constexpr size_t kParticle_Count       = 0x24;  // int, the record count
constexpr size_t kParticle_SubCount    = 0x28;  // int, ctor 0xf; each record's sub-array size
constexpr size_t kParticle_Material    = 0x30;  // ExternalPtr<MaterialInstance>
constexpr size_t kParticle_RecordArray = 0x38;  // Array<void*> {data, size@+0x40, cap@+0x44}

static_assert(offsetof(Particle, _active) == kParticle_Active, "Particle::_active");
static_assert(offsetof(Particle, _f24)    == kParticle_Count,  "Particle record count @ +0x24");
static_assert(offsetof(Particle, _f28)    == kParticle_SubCount, "Particle sub-count @ +0x28");
static_assert(offsetof(Particle, _f30)    == kParticle_Material, "Particle material @ +0x30");
static_assert(offsetof(Particle, _f38)    == kParticle_RecordArray, "Particle records @ +0x38");

// ---------------------------------------------------------------------------
// The per-instance particle pool (`ParticleInstance::_boundsPtr`, +0xc8) — the
// 0x40-byte object whose ctor is 0x180103bf0. Fields this lane reads:
//   +0x00..+0x18  a Volume::Box (min seeded 1e8, max seeded -1e8)
//   +0x18         the live particle Array's data pointer
//   +0x20         that array's size, +0x28 the live count
//   +0x2c         the per-frame tag compared against ParticleInstance::_frameIndex
// runtime/types/graphics/Particle.h's `ParticleBoundsRecord` describes the same
// object with an `opaque[0x2c]` prefix; this lane MEASURED the whole 0x40 and
// the fields below are that measurement, recorded here rather than by widening
// a struct another lane's oracles compare byte-for-byte.
// ---------------------------------------------------------------------------
constexpr size_t kPool_ParticleArrayData = 0x18;
constexpr size_t kPool_ParticleArraySize = 0x20;
constexpr size_t kPool_LiveCount         = 0x28;
constexpr size_t kPool_FrameTag          = 0x2c;

// Per-particle record inside that array, stride 0x58. Only the fields the two
// Draw bodies read are named; all are float pairs {current, previous}.
constexpr size_t kRec_Stride    = 0x58;
constexpr size_t kRec_PosA      = 0x10;  // 3 floats current
constexpr size_t kRec_PosAPrev  = 0x1c;
constexpr size_t kRec_PosB      = 0x28;  // 3 floats current
constexpr size_t kRec_PosBPrev  = 0x34;
constexpr size_t kRec_ScrollCur = 0x40;
constexpr size_t kRec_ScrollPrv = 0x44;
constexpr size_t kRec_RotCur    = 0x48;  // DEGREES (see DegToRad above)
constexpr size_t kRec_RotPrv    = 0x4c;
constexpr size_t kRec_SizeCur   = 0x50;
constexpr size_t kRec_SizePrv   = 0x54;

// ---------------------------------------------------------------------------
// `System::Random` — MT19937. Layout read out of SetParticle's own bytes:
//   +0x00        int  index (`mti`); zero means "regenerate before use"
//   +0x04..      uint32 state[624]
// `?NextGroup@Random@System@@AEAAXXZ` (0x1800c7dd0) is the twist. AEAA means
// PRIVATE, not unexported — this DLL exports its private members
// (reference_aeaa_is_private_not_unexported.md) — so it is reached BY NAME.
// ---------------------------------------------------------------------------
using NextGroupFn = void (*)(void* rng);

void* ParticleRng() {
    // ADDRESS DOOR / DATA DOOR — SYMBOL: ?_rng@Particle@Graphics@@1VRandom@System@@A
    // (retail 0x1801fe7f0).  CONDITION: resolved by NAME through ResolveExport,
    // which is registry-aware, so when standalone/rt_data_mint.cpp mints this
    // symbol at cutover we follow the minted storage and when the DLL is present
    // we follow the DLL's.  CONSEQUENCE: if it ever resolves NULL this body
    // returns index 0 for every particle (see the caller) instead of faulting,
    // and says so once on stderr. Same door as code/Game/Game/tornado.cpp:253.
    static void* p = nullptr;
    static bool  tried = false;
    if (!tried) {
        tried = true;
        p = ResolveExport("?_rng@Particle@Graphics@@1VRandom@System@@A");
        if (!p)
            std::fprintf(stderr,
                "[GFXPART27] ?_rng@Particle@Graphics@@ resolved NULL -- every "
                "ParticleInstance::SetParticle will pick index 0 and particle "
                "placement will not match retail\n");
    }
    return p;
}

// One MT19937 draw, exactly as retail inlines it (five call sites in
// SetParticle/Update share the identical instruction sequence). Returns the
// tempered word; the index advance and 0x270 wrap are part of the draw.
uint32_t RngNext(void* rngv) {
    uint8_t* rng = B(rngv);
    int32_t index = I32(rng, 0);
    if (index == 0) {
        // NAME DOOR — SYMBOL: ?NextGroup@Random@System@@AEAAXXZ (0x1800c7dd0).
        // CONDITION: private (AEAA) but EXPORTED, so a name door is the plain
        // door, not a workaround. CONSEQUENCE: null here means the state is never
        // regenerated and the generator repeats its last block forever; the
        // guard below makes that visible instead of a wild call.
        static NextGroupFn fn = nullptr;
        static bool tried = false;
        if (!tried) {
            tried = true;
            fn = reinterpret_cast<NextGroupFn>(ResolveExport("?NextGroup@Random@System@@AEAAXXZ"));
            if (!fn)
                std::fprintf(stderr, "[GFXPART27] ?NextGroup@Random@System@@ resolved "
                                     "NULL -- the MT19937 state will never be twisted\n");
        }
        if (fn) fn(rng);
        index = I32(rng, 0);
    }
    uint32_t y;
    std::memcpy(&y, rng + 4 + static_cast<size_t>(static_cast<uint32_t>(index)) * 4, sizeof y);
    // The shipped tempering, masks lifted from the bytes (see the banner).
    y ^= y >> 11;
    y ^= (y & 0xff3a58adu) << 7;
    y ^= (y & 0xffffdf8cu) << 15;
    y ^= y >> 18;
    int32_t next = index + 1;
    if (next == 0x270) next = 0;     // retail: CMP EDX,0x270 / CMOVZ EDX,<zero reg>
    std::memcpy(rng, &next, sizeof next);
    return y;
}

// The shared Serialize prologue/epilogue, the same two helpers
// runtime/src/achievement/goal_serialize.cpp already carries — repeated here
// rather than exported from there because that file keeps them in an anonymous
// namespace and this lane must not change another wave's translation unit.
ReaderWriter* MakeCodec(Stream* stream, Serializer* ser) {
    ser->stream = stream;
    ser->readerWriter = nullptr;
    Encoding* enc = stream->encoding;
    const int slot = (stream->direction == rt::io::kDirectionReading)
                         ? rt::io::encoding_vtbl::kCreateReader
                         : rt::io::encoding_vtbl::kCreateWriter;
    using FactoryFn = ReaderWriter* (*)(Encoding*, Serializer*, int);
    ReaderWriter* rw =
        reinterpret_cast<FactoryFn>(enc->vtable[slot / 8])(enc, ser, 0);
    ser->readerWriter = rw;
    return rw;
}
void DestroyCodec(ReaderWriter* rw) {
    if (!rw) return;
    using DtorFn = void (*)(ReaderWriter*, int);
    reinterpret_cast<DtorFn>(rw->vtable[rt::io::rw_vtbl::kDtor / 8])(rw, 1);
}
using ScalarFn = void (*)(ReaderWriter*, void*, const char*);
void SerScalar(ReaderWriter* rw, size_t slot, void* field) {
    reinterpret_cast<ScalarFn>(rw->vtable[slot / 8])(rw, field, nullptr);
}

}  // namespace

// ============================================================================
// bool Graphics::Particle::Serialize(IO::Stream&)
// ?Serialize@Particle@Graphics@@UEAA_NAEAVStream@IO@@@Z   retail 0x180103f00
// `??_7Particle@Graphics@@6B@` slot 5 — the LAST unported slot of that table.
//
// Retail, read at 0x180103f00 (33 instructions of prologue + a read-side loop):
//
//   Serializer ser{ &stream, nullptr };
//   rw = stream.direction == 0 ? enc->vt[0x18](enc,&ser,0)
//                              : enc->vt[0x20](enc,&ser,0);
//   rw->vt[0x78](rw, &this->_active,   0)     bool
//   rw->vt[0xa0](rw, &this->_count,    0)     int
//   rw->vt[0xa0](rw, &this->_subCount, 0)     int
//   0x180014520(&this->_material, &ser)       ExternalBasePtr<MaterialInstance>
//   if (stream.direction == 0 && this->_active) {
//       0x180062110(&this->_records, this->_count);        Array<void*>::SetSize
//       for (i = 0; i < this->_count; ++i) {
//           p = Enter/Allocate(0x40, 8)/Leave;
//           this->_records.data[i] = p ? 0x180103bf0(p, this) : nullptr;
//       }
//   }
//   if (rw) rw->vt[0](rw, 1);
//   return true;                               MOV AL,1 — no failure path
//
// FOUR THINGS THE DISASSEMBLY SAYS AND A DECOMPILE WOULD BLUR.
//
// 1. The read-side block is gated on `[stream+8] != 0 -> SKIP`. It runs when the
//    stream is READING. Inverting that would allocate a pool on every SAVE.
// 2. The loop bound is RE-READ from `this->_count` at the bottom
//    (`CMP EDI,[R15+0x24]`), and the array base is RE-READ from
//    `this->_records.data` at the top of each iteration. Both are transcribed as
//    re-reads. Nothing under the loop can change either today, so this is a
//    faithfulness choice with no observable effect — stated so a later reader
//    does not "optimise" it and quietly change one that does.
// 3. The element ctor's return value is what is stored, not the raw allocation:
//    `CALL 0x180103bf0; MOV RCX,RAX`. It returns its own first argument, so the
//    two agree today; the port stores what retail stores.
// 4. `MOV AL,0x1` — always true. There is no error path to reproduce.
//
// THREE ADDRESS DOORS, all commented at their call sites below.
// ============================================================================
extern "C" bool RtGraphics_Particle_Serialize(void* selfv, void* streamv) {
    ++g_particleSerialize;

    uint8_t* self  = B(selfv);
    Stream*  stream = static_cast<Stream*>(streamv);

    Serializer ser;
    ReaderWriter* rw = MakeCodec(stream, &ser);

    SerScalar(rw, rt::io::rw_vtbl::kScalarBool, self + kParticle_Active);
    SerScalar(rw, rt::io::rw_vtbl::kScalarInt,  self + kParticle_Count);
    SerScalar(rw, rt::io::rw_vtbl::kScalarInt,  self + kParticle_SubCount);

    // ADDRESS DOOR — SYMBOL: the `Object::ExternalBasePtr::Serialize` template
    // instantiation at retail 0x180014520 (UNNAMED in the export directory,
    // checked by VA with pefile — not assumed from the absence of a mangled
    // name). It is the same body shape as 0x1800042a0 / 0x180005dc0, which
    // runtime/src/achievement/goal_serialize.cpp already reaches the same way,
    // and it compares against `_typeInfo` at 0x1801fa778.
    // CONDITION: needs ResolveAddr(Module::Runtime, ...) to hand back something;
    // at cutover it needs whatever registers 0x180014520.
    // CONSEQUENCE: null here is a wild call, so it is checked. Skipping the call
    // instead would leave the ExternalPtr field unconsumed and desync every
    // later record in the stream — the silent save-desync failure family.
    using PtrSerializeFn = void (*)(void*, Serializer*);
    PtrSerializeFn extPtrSerialize = RtAddr<PtrSerializeFn>(0x180014520ull);
    if (extPtrSerialize) {
        extPtrSerialize(self + kParticle_Material, &ser);
    } else {
        std::fprintf(stderr, "[GFXPART27] Particle::Serialize: 0x180014520 unresolved -- "
                             "the material ExternalPtr record is NOT consumed and this "
                             "stream is now desynchronised\n");
    }

    if (stream->direction == rt::io::kDirectionReading && self[kParticle_Active] != 0) {
        // ADDRESS DOOR — SYMBOL: `Collection::Array<void*>::SetSize(this, int)`
        // at retail 0x180062110, UNNAMED (checked by VA). Its body reads
        // `SHL RCX,3 / MOV EDX,8` with a plain qword copy loop whose else-arm is
        // empty, i.e. stride 8, align 8, new tail UNINITIALISED — which is
        // kSpec_Fill8 in runtime/src/collection/array.cpp's J3 family exactly.
        // CONDITION: it is NOT registered as a J3 row today, so ResolveAddr hands
        // back the DLL's body and this call makes 0x180062110 newly `called`
        // (reference_porting_can_grow_the_cutover_surface.md).
        // CONSEQUENCE: null means the record array is never sized and the loop
        // below writes past a null base. Registering it as a J3 row is this
        // lane's declared debt — see WAVE27_LANE_GFXPART.md §6, ADDRESSEE named.
        using ArraySetSizeFn = void (*)(void*, int32_t);
        ArraySetSizeFn setSize = RtAddr<ArraySetSizeFn>(0x180062110ull);

        // ADDRESS DOOR — SYMBOL: the pool-record constructor at retail
        // 0x180103bf0, UNNAMED (checked by VA). It seeds a Volume::Box to
        // (+1e8,+1e8,+1e8)/(-1e8,-1e8,-1e8), zeroes +0x18..+0x28, writes -1 at
        // +0x2c and sizes its own sub-array at +0x18 to `particle->_subCount`
        // through a second unnamed SetSize at 0x180061bb0.
        // CONDITION / CONSEQUENCE: as above — null means no record is built and
        // every slot stays whatever SetSize left, so it is checked.
        using RecordCtorFn = void* (*)(void*, void*);
        RecordCtorFn recordCtor = RtAddr<RecordCtorFn>(0x180103bf0ull);

        if (!setSize || !recordCtor) {
            std::fprintf(stderr, "[GFXPART27] Particle::Serialize: setSize=%p ctor=%p -- "
                                 "the particle pool is NOT rebuilt on this load\n",
                         (void*)setSize, (void*)recordCtor);
        } else {
            setSize(self + kParticle_RecordArray, I32(self, kParticle_Count));
            for (int32_t i = 0; i < I32(self, kParticle_Count); ++i) {
                void* p = rtcoll::MemAlloc(0x40, 8);
                void* v = p ? recordCtor(p, self) : nullptr;
                void** base = *reinterpret_cast<void***>(self + kParticle_RecordArray);
                base[i] = v;
                ++g_particleSerRecords;
            }
        }
    }

    DestroyCodec(rw);
    return true;
}

// ============================================================================
// Math::Matrix Graphics::ParticleInstance::GetInstanceTransform(int, float) const
// ?GetInstanceTransform@ParticleInstance@Graphics@@UEBA?AVMatrix@Math@@HM@Z
// retail 0x1801047f0 — vtable slot 6 of BOTH particle instance tables.
//
// RCX = this, RDX = the hidden Matrix return slot, R8D = the int index,
// XMM3 = the interpolation alpha.
//
// *** THE INT ARGUMENT IS IGNORED. *** Retail's first act is
// `LEA R8,[RCX+0x84]`, which overwrites it before any read. That is not an
// oversight to "fix": the base `Geometry::GetInstanceTransform` (0x1800154a0)
// uses the index, and this override deliberately does not.
//
// Retail:
//   t   = alpha
//   pos = prevTransform.translation
//         + (transform.translation - prevTransform.translation) * t
//   q   = prevTransform.rotation.Lerp(transform.rotation, t)
//   m   = Transform{ q, pos }.AsMatrix()
//   out = { m[0..2], 1.0f,  m[4..6], 1.0f,  m[8..10], 1.0f,
//           m[12..14], ((1.0f - t) * _opacityPrev) + (t * _opacity) }
//
// The fourth column is NOT the matrix's own — retail overwrites all four
// w-components: three with the shipped 1.0f and the last with the interpolated
// opacity. A port that copied `m` wholesale and patched only +0x3c would leave
// AsMatrix's w-row in place and hand the shader a different matrix.
// ============================================================================
extern "C" Matrix* RtGraphics_ParticleInstance_GetInstanceTransform(
        void* selfv, Matrix* out, int32_t /*index — ignored, see banner*/,
        float alpha) {
    ++g_piGetInstanceXform;

    const Geometry* self = static_cast<const Geometry*>(selfv);
    const uint8_t*  cur  = B(&self->_transform);
    const uint8_t*  prv  = B(&self->_prevTransform);

    // NAME DOORS — SYMBOLS: ?Lerp@Quaternion@Math@@QEBA?AV12@AEBV12@AEBVScalar@2@@Z
    // (0x18001d500) and ?AsMatrix@Transform@Math@@QEBA?AVMatrix@2@XZ (0x18001e5f0).
    // CONDITION: both are ordinary public exports and both are PORTED (Wave 1
    // Math), so going by NAME reaches our own bodies. Going by ADDRESS would walk
    // past them into the DLL — the port-bypassing-a-port defect
    // tools/port_bypass_audit.py exists for.
    // CONSEQUENCE: a null leaves `out` untouched, so the caller would draw with
    // stack garbage; both are therefore checked once and named.
    // ENDGAME B.3.1 step 2: Quaternion::Lerp and Transform::AsMatrix are
    // members now. There is no resolve left to fail, so the "resolved to null"
    // arm that used to return the matrix UNWRITTEN is gone with it.

    const float t = alpha;

    // Retail's order exactly: subtract, scale, add back onto the PREVIOUS value.
    Transform blended;
    const float px = F(prv, 0x10), py = F(prv, 0x14), pz = F(prv, 0x18);
    blended.translation.x = (F(cur, 0x10) - px) * t + px;
    blended.translation.y = (F(cur, 0x14) - py) * t + py;
    blended.translation.z = (F(cur, 0x18) - pz) * t + pz;

    Quaternion q;
    // `this` is the PREVIOUS rotation and the argument is the CURRENT one, so the
    // lerp runs prev -> cur, matching the translation blend above.
    q = reinterpret_cast<const Quaternion*>(prv)->Lerp(
                *reinterpret_cast<const Quaternion*>(cur),
                reinterpret_cast<const Scalar&>(t));   // B.3.1 bridge
    blended.rotation = q;

    Matrix m;
    m = blended.AsMatrix();

    const float one = One();
    out->m[0]  = m.m[0];  out->m[1]  = m.m[1];  out->m[2]  = m.m[2];  out->m[3]  = one;
    out->m[4]  = m.m[4];  out->m[5]  = m.m[5];  out->m[6]  = m.m[6];  out->m[7]  = one;
    out->m[8]  = m.m[8];  out->m[9]  = m.m[9];  out->m[10] = m.m[10]; out->m[11] = one;
    out->m[12] = m.m[12]; out->m[13] = m.m[13]; out->m[14] = m.m[14];
    // ((1 - t) * prevOpacity) + (t * opacity) — retail computes the two products
    // and adds the t-term LAST. Folding it the other way is a different float.
    out->m[15] = ((one - t) * self->_opacityPrev) + (t * self->_opacity);
    // Hidden return slot: the by-value Matrix's buffer comes in RDX and its
    // address goes back in RAX. Declared void until 2026-09-13; RAX was garbage
    // at -O2 and DrawItemList's SetTransform read through it (perf build only).
    return out;
}

// ============================================================================
// void Graphics::ParticleInstance::AddDrawItems(const Scene&, Renderer&) const
// ?AddDrawItems@ParticleInstance@Graphics@@UEBAXAEBVScene@2@AEAVRenderer@2@@Z
// retail 0x180104700 — vtable slot 3 of BOTH particle instance tables.
//
// *** THE `Scene&` ARGUMENT IS NEVER READ. *** RDX is overwritten with `this`
// at 0x18010477c before any load through it. Faithful, not an omission.
//
// Retail:
//   pool = this->_boundsPtr
//   if (pool->liveCount == 0) return;                 // +0x28
//   group = renderer.drawGroups.data + renderer.currentDrawGroup * 0x1f0
//   d  = this->_transform.translation - *(Vector3*)(group + 0x90)
//   Renderer::AddDrawItem(
//       &renderer,
//       this,                                          // const Geometry*
//       *(MaterialInstance**)(this->_resource + 0x30),
//       0x8000,                                        // ushort
//       0,                                             // ushort
//       (d.x*d.x + d.y*d.y) + d.z*d.z,                 // float, retail's order
//       ((1.0f - renderer.frameAlpha) * _opacityPrev)
//           + (renderer.frameAlpha * _opacity),        // float
//       this->_resource->_active ? 0x34 : 0,           // int
//       pool,                                          // const void*
//       0);                                            // uint
//
// TWO THINGS WORTH SPELLING OUT.
//
// 1. `group + 0x90` is a THREE-FLOAT EYE POSITION inside RendererDrawGroup's
//    `_head[0xe0]`, which runtime/types/graphics/Camera.h declares opaque with
//    the note "not read by lane F". This body is the first to read it. It is
//    read here by documented offset rather than by widening that struct, because
//    lane F's renderer oracles memcmp RendererDrawGroup and splitting `_head`
//    would change nothing about the bytes but everything about which lane owns
//    the row. WAVE27_LANE_GFXPART.md §5 carries the finding with an ADDRESSEE.
// 2. The int flag is `(active ? -1 : 0) & 0x34` — retail computes it with
//    `NEG AL; SBB ECX,ECX; AND ECX,0x34`, a branchless mask, not a conditional.
//    0x34 and not 0x34-something: the immediate is in the instruction.
// ============================================================================
extern "C" void RtGraphics_ParticleInstance_AddDrawItems(
        void* selfv, const void* /*scene — never read, see banner*/, void* rendererv) {
    ++g_piAddDrawItems;

    const Geometry*         self = static_cast<const Geometry*>(selfv);
    const ParticleInstance* pi   = static_cast<const ParticleInstance*>(selfv);
    uint8_t*                rend = B(rendererv);

    const uint8_t* pool = B(pi->_boundsPtr);
    if (I32(pool, kPool_LiveCount) == 0) return;

    // renderer.drawGroups.data (+0x10) indexed by renderer.currentDrawGroup (+0x34).
    // MOVSXD — a SIGNED widening; Ghidra's C prints it as a zero extension
    // (reference_ghidra_c_output_vs_disassembly.md), and a negative index here
    // would address backwards, which is retail's behaviour and not ours to guard.
    uint8_t* const groups = *reinterpret_cast<uint8_t**>(rend + 0x10);
    const int64_t  gi     = static_cast<int64_t>(I32(rend, 0x34));
    const uint8_t* group  = groups + gi * 0x1f0;

    const uint8_t* xf = B(&self->_transform);
    float dx = F(xf, 0x10) - F(group, 0x90);
    float dy = F(xf, 0x14) - F(group, 0x94);
    float dz = F(xf, 0x18) - F(group, 0x98);
    dx *= dx; dy *= dy; dz *= dz;
    // Retail: (dy + dx) + dz. ADDSS XMM2,XMM0 then ADDSS XMM2,XMM1, where XMM2
    // held the Y term, XMM0 the X term and XMM1 the Z term.
    const float distSq = (dy + dx) + dz;

    const uint8_t* res = B(pi->_resource);
    void* material = *reinterpret_cast<void* const*>(res + kParticle_Material);
    const int32_t flags = (res[kParticle_Active] != 0 ? -1 : 0) & 0x34;

    const float a   = F(rend, 0x40);            // Renderer::frameAlpha
    const float one = One();
    const float opacity = ((one - a) * self->_opacityPrev) + (a * self->_opacity);

    // NAME DOOR — SYMBOL:
    // ?AddDrawItem@Renderer@Graphics@@QEAAXPEBVGeometry@2@PEBVMaterialInstance@2@GGMMHPEBXI@Z
    // (retail 0x1801057b0). CONDITION: a public export; by NAME so that a port of
    // it is reached rather than bypassed. CONSEQUENCE: null means this particle
    // system is silently never queued, which looks like a content bug rather than
    // a resolve failure — hence the one-shot message.
    using AddDrawItemFn = void (*)(void*, const void*, const void*, uint16_t, uint16_t,
                                   float, float, int32_t, const void*, uint32_t);
    static AddDrawItemFn add = nullptr;
    static bool tried = false;
    if (!tried) {
        tried = true;
        add = reinterpret_cast<AddDrawItemFn>(ResolveExport(
            "?AddDrawItem@Renderer@Graphics@@QEAAXPEBVGeometry@2@PEBVMaterialInstance@2@"
            "GGMMHPEBXI@Z"));
        if (!add)
            std::fprintf(stderr, "[GFXPART27] AddDrawItems: Renderer::AddDrawItem "
                                 "resolved NULL -- no particle system will draw\n");
    }
    if (!add) return;

    add(rendererv, selfv, material, 0x8000, 0, distSq, opacity, flags, pool, 0);
    ++g_piAddDrawItemsEmit;
}

// ============================================================================
// void Graphics::ParticleInstance::SetParticle(ExternalPtr<Particle>)
// ?SetParticle@ParticleInstance@Graphics@@UEAAXV?$ExternalPtr@VParticle@Graphics@@@Object@@@Z
// retail 0x180104a00 — vtable slot 9 of `??_7ParticleInstance@Graphics@@6B@`.
//
// The ExternalPtr is passed BY VALUE and is one pointer wide, so RDX is the
// Particle* itself. Retail spills it to its home slot and reloads — which
// matters, because RemoveState() runs in between and RDX is not preserved
// across a call. A port that kept it in a local is doing the same thing.
//
// Retail:
//   this->RemoveState();                                 // 0x1801040b0
//   this->_resource = p;
//   this->_materialTypeFlags = 1 << *(int*)(*(void**)(p + 0x30) + 0x20);
//   if (p->_active) {
//       n = p->_count;
//       idx = (n == 0) ? 0 : RngNext(_rng) % n;           // UNSIGNED remainder
//       this->_index = idx;
//       rec = ((void**)p->_records.data)[idx];
//   } else {
//       void* m = operator new(0x40);                     // 0x180061a80
//       rec = m ? 0x180103bf0(m, p) : nullptr;
//   }
//   this->_boundsPtr = rec;
//   if (any of _localBounds' six floats != 0.0f) {
//       zero all six; this->_dirtyFlags |= kDirtyBounds;
//   }
//
// THE SHIFT IS UNMASKED. Retail emits `MOV EDX,1; SHL EDX,CL` with CL taken from
// the material's +0x20, so a value >= 32 shifts by `cl & 31` on x86. Reproduced
// as `1u << (v & 31)` because C++ would otherwise be undefined behaviour, and
// the mask is what the HARDWARE does — that is the faithful reading, not a guard.
//
// THE BOUNDS TEST IS A SIX-WAY EXACT COMPARE AGAINST ZERO, with the NaN case
// falling into the "not equal" arm (retail's `JP` on each UCOMISS jumps to the
// reset). Written below with that exact shape rather than as `!= 0.0f`, which a
// compiler is free to reassociate.
// ============================================================================
// Defined with the rest of batch D2 at the bottom of this file; declared here
// because this is its first use.
extern "C" void* RtGraphics_ParticlePool_OperatorNew();

extern "C" void RtGraphics_ParticleInstance_SetParticle(void* selfv, void* particlev) {
    ++g_piSetParticle;

    ParticleInstance* pi   = static_cast<ParticleInstance*>(selfv);
    Geometry*         self = static_cast<Geometry*>(selfv);

    // NAME DOOR — SYMBOL: ?RemoveState@ParticleInstance@Graphics@@IEAAXXZ
    // (retail 0x1801040b0). IEAA = PROTECTED, and this DLL exports its protected
    // members (reference_aeaa_is_private_not_unexported.md), so a name door is
    // the plain door. CONDITION/CONSEQUENCE: null means the previous pool is
    // never detached and the old record leaks into the new instance; the message
    // makes that visible rather than letting it read as a content bug.
    using RemoveStateFn = void (*)(void*);
    static RemoveStateFn removeState = nullptr;
    static bool triedRemove = false;
    if (!triedRemove) {
        triedRemove = true;
        removeState = reinterpret_cast<RemoveStateFn>(
            ResolveExport("?RemoveState@ParticleInstance@Graphics@@IEAAXXZ"));
        if (!removeState)
            std::fprintf(stderr, "[GFXPART27] SetParticle: RemoveState resolved NULL "
                                 "-- the previous particle pool is NOT released\n");
    }
    if (removeState) removeState(selfv);

    uint8_t* p = B(particlev);
    pi->_resource = static_cast<RtGraphics::ParticleResource*>(particlev);

    // 1 << material->[0x20]. Faithful deref of a pointer retail does not check
    // (docs: don't "fix" a faithful crash with a null check).
    const uint8_t* material = *reinterpret_cast<const uint8_t* const*>(p + kParticle_Material);
    const int32_t  shift    = I32(material, 0x20);
    self->_materialTypeFlags = 1u << (static_cast<uint32_t>(shift) & 31u);

    void* rec = nullptr;
    if (p[kParticle_Active] != 0) {
        const int32_t n = I32(p, kParticle_Count);
        uint32_t idx = 0;
        if (n != 0) {
            void* rng = ParticleRng();
            if (rng) {
                idx = RngNext(rng) % static_cast<uint32_t>(n);
                ++g_piSetParticleRand;
            }
            // rng == nullptr keeps idx 0; ParticleRng() already said so once.
        }
        pi->_index = static_cast<int32_t>(idx);
        void** base = *reinterpret_cast<void***>(p + kParticle_RecordArray);
        rec = base[idx];
    } else {
        // ADDRESS DOOR — SYMBOL: the per-TU `operator new(0x40)` at retail
        // 0x180061a80, UNNAMED (checked by VA with pefile). Its body is
        // `Enter(cs); Allocate(_singleton, 0x40, 8); Leave(cs)` — byte for byte
        // ?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z (0x1800c7a50) with the
        // size and alignment already folded in, so this port calls lane Q's
        // rtcoll::MemAlloc instead of opening a fourteenth allocator door.
        // CONDITION: rtcoll::MemAlloc resolves 0x1800c7a50 and must be repointed
        // at lane E's allocator at cutover, exactly as array.cpp's banner says.
        // CONSEQUENCE: a null allocation is retail's own tolerated case — it
        // stores nullptr and the caller copes — so it is NOT an error here.
        void* m = RtGraphics_ParticlePool_OperatorNew();
        if (m) {
            using RecordCtorFn = void* (*)(void*, void*);
            // ADDRESS DOOR — SYMBOL: the pool-record ctor at 0x180103bf0,
            // UNNAMED; the same door Particle::Serialize above opens, described
            // in full there. CONSEQUENCE of null: an uninitialised 0x40-byte
            // record reaches the renderer, so it is checked.
            RecordCtorFn ctor = RtAddr<RecordCtorFn>(0x180103bf0ull);
            rec = ctor ? ctor(m, p) : nullptr;
            if (!ctor)
                std::fprintf(stderr, "[GFXPART27] SetParticle: 0x180103bf0 unresolved "
                                     "-- the new pool record is UNINITIALISED\n");
        }
    }
    pi->_boundsPtr = static_cast<RtGraphics::ParticleBoundsRecord*>(rec);

    // The six-float local-bounds reset. `!(a == 0)` rather than `a != 0` so the
    // NaN case takes the reset arm, which is what retail's JP-before-JNZ does.
    float* lb = reinterpret_cast<float*>(B(self) + offsetof(Geometry, _localBounds));
    const bool allZero = (lb[0] == 0.0f) && (lb[1] == 0.0f) && (lb[2] == 0.0f) &&
                         (lb[3] == 0.0f) && (lb[4] == 0.0f) && (lb[5] == 0.0f);
    if (!allZero) {
        lb[0] = 0.0f; lb[1] = 0.0f; lb[2] = 0.0f;
        lb[3] = 0.0f; lb[4] = 0.0f; lb[5] = 0.0f;
        self->_dirtyFlags |= RtGraphics::kDirtyBounds;
    }
}

// ============================================================================
// void Graphics::ParticleSystemInstance::SetParticle(ExternalPtr<Particle>)
// ?SetParticle@ParticleSystemInstance@Graphics@@UEAAXV?$ExternalPtr@…@Z
// retail 0x1800a6a80 — vtable slot 9 of `??_7ParticleSystemInstance@…6B@`.
//
// Retail:
//   ParticleInstance::SetParticle(this, p);           // CALL 0x180104a00
//   res = this->_resource;
//   if (res && !res->_active && this->_emitter) 0x180020d60(this->_emitter);
//   sys = IsDerivedFrom(p, ParticleSystem::_typeInfo.typeId) ? p : nullptr;
//   if (this->_resource->_active)
//       this->_emitter = ((void**)(sys + 0xf8))[this->_index];
//   else {
//       void* m = operator new(...);                   // 0x1800205a0
//       if (m) { *(u64*)m = 0; *(u64*)(m+8) = 0;
//                *(u32*)(m+0x10) = *(u32*)(sys+0x48);
//                0x180020e90(m, *(u32*)(sys+0x28)); }
//       this->_emitter = m;
//   }
//
// TWO FAITHFUL DEREFS OF A POINTER RETAIL DOES NOT CHECK, left unguarded on
// purpose (CLAUDE.md: don't "fix" a faithful crash — find the upstream null):
//   * `this->_resource->_active` after the first block, where `_resource` was
//     just written by the base call and is only null if `p` was.
//   * `sys` in BOTH arms. `IsDerivedFrom` returning false leaves it null and
//     retail reads `sys+0xf8` / `sys+0x48` anyway. In retail that arm is
//     unreachable because every caller hands a ParticleSystem; if our standalone
//     ever reaches it the fault is the diagnosis, not a bug to paper over.
//
// `?_typeInfo@ParticleSystem@Graphics@@0VBaseTypeInfo@Object@@A` (0x1801f9a58)
// is read for its FIRST DWORD only — the typeId. That is the whole of what the
// DLL's 525 consumers of a BaseTypeInfo read
// (reference_basetypeinfo_name_is_content_only.md), and it is read through
// ResolveExport so a minted copy is followed at cutover.
// ============================================================================
extern "C" void RtGraphics_ParticleSystemInstance_SetParticle(void* selfv, void* particlev) {
    ++g_psiSetParticle;

    ParticleSystemInstance* psi = static_cast<ParticleSystemInstance*>(selfv);
    ParticleInstance*       pi  = &psi->base;

    // Retail CALLs 0x180104a00 directly, so this port calls OUR body directly
    // too: going through the registry here would let the two halves of one
    // class come from different builds.
    RtGraphics_ParticleInstance_SetParticle(selfv, particlev);

    uint8_t* res = B(pi->_resource);
    if (res && res[kParticle_Active] == 0 && psi->_f_d8) {
        // ADDRESS DOOR — SYMBOL: the emitter release at retail 0x180020d60,
        // UNNAMED (checked by VA). CONDITION: reached only on the
        // inactive-resource path. CONSEQUENCE: null leaks the previous emitter,
        // which is a slow leak rather than a fault, so it is reported once and
        // the body continues — matching what retail does when the pointer is
        // already null (it skips the call).
        using ReleaseFn = void (*)(void*);
        ReleaseFn release = RtAddr<ReleaseFn>(0x180020d60ull);
        if (release) release(psi->_f_d8);
        else std::fprintf(stderr, "[GFXPART27] PSI::SetParticle: 0x180020d60 unresolved "
                                  "-- the previous emitter is LEAKED\n");
    }

    // NAME DOORS — ?IsDerivedFrom@Base@Object@@QEBA_NI@Z (0x1800c0390) and the
    // ParticleSystem typeinfo DATA row. CONSEQUENCE of either being null: the
    // derived-from test cannot be made, so this body bails rather than guessing
    // — guessing `true` would deref a non-ParticleSystem at +0xf8.
    using IsDerivedFn = bool (*)(const void*, uint32_t);
    static IsDerivedFn isDerived = nullptr;
    static const uint32_t* psTypeInfo = nullptr;
    static bool tried = false;
    if (!tried) {
        tried = true;
        isDerived = reinterpret_cast<IsDerivedFn>(
            ResolveExport("?IsDerivedFrom@Base@Object@@QEBA_NI@Z"));
        psTypeInfo = reinterpret_cast<const uint32_t*>(
            ResolveExport("?_typeInfo@ParticleSystem@Graphics@@0VBaseTypeInfo@Object@@A"));
        if (!isDerived || !psTypeInfo)
            std::fprintf(stderr, "[GFXPART27] PSI::SetParticle: IsDerivedFrom=%p "
                                 "ParticleSystem::_typeInfo=%p -- no emitter is built\n",
                         (void*)isDerived, (const void*)psTypeInfo);
    }
    if (!isDerived || !psTypeInfo) return;

    const uint8_t* sys = isDerived(particlev, psTypeInfo[0]) ? B(particlev) : nullptr;

    if (B(pi->_resource)[kParticle_Active] != 0) {
        void* const* table = *reinterpret_cast<void* const* const*>(sys + 0xf8);
        psi->_f_d8 = table[pi->_index];
        return;
    }

    // ADDRESS DOOR — SYMBOL: the emitter allocator at retail 0x1800205a0,
    // UNNAMED. CONDITION: the inactive-resource path only. CONSEQUENCE: retail
    // tolerates a null result (it stores null), so a null RESULT is not an error;
    // a null FUNCTION is, and is reported.
    using EmitterNewFn = void* (*)();
    EmitterNewFn emitterNew = RtAddr<EmitterNewFn>(0x1800205a0ull);
    if (!emitterNew) {
        std::fprintf(stderr, "[GFXPART27] PSI::SetParticle: 0x1800205a0 unresolved "
                             "-- no emitter allocated\n");
        psi->_f_d8 = nullptr;
        return;
    }
    void* m = emitterNew();
    if (m) {
        uint8_t* e = B(m);
        std::memset(e, 0, 16);                          // two zeroed qwords
        const uint32_t seed = *reinterpret_cast<const uint32_t*>(sys + 0x48);
        std::memcpy(e + 0x10, &seed, sizeof seed);
        // ADDRESS DOOR — SYMBOL: the emitter initialiser at 0x180020e90, UNNAMED.
        // CONSEQUENCE of null: a half-built emitter reaches Update, so it is
        // reported and the object is still stored — which is what retail does
        // with whatever that call leaves behind.
        using EmitterInitFn = void (*)(void*, uint32_t);
        EmitterInitFn init = RtAddr<EmitterInitFn>(0x180020e90ull);
        if (init) init(m, *reinterpret_cast<const uint32_t*>(sys + 0x28));
        else std::fprintf(stderr, "[GFXPART27] PSI::SetParticle: 0x180020e90 unresolved "
                                  "-- the emitter is HALF-BUILT\n");
    }
    psi->_f_d8 = m;
}

// ============================================================================
// void Graphics::ParticleInstance::Draw(Video::Context&, const Camera&,
//         ExternalPtr<MaterialInstance>, float, int, int, int) const
// ?Draw@ParticleInstance@Graphics@@UEBAX…MHHH@Z   retail 0x180104230
// vtable slot 2 of BOTH particle instance tables.
//
// *** THE `Camera&` AND THE `ExternalPtr` ARE NEVER READ, and neither are the
// fifth and sixth int arguments. *** Only the float (alpha) and the SEVENTH
// argument (the instance count) are used. Verified by walking every load: R8 and
// R9 are overwritten before use, and the only stack-argument reads are
// `[RBP+0x70]` (alpha, = entry RSP+0x28) and `[RBP+0x88]` (the last int,
// = entry RSP+0x40).
//
// Retail, in three parts:
//
//  A. if (instances <= 1) {
//         pos = prev.translation + (cur.translation - prev.translation) * alpha
//         q   = prev.rotation.Lerp(cur.rotation, alpha)
//         m   = Transform{q,pos}.AsMatrix()
//         v4  = { 1, 1, 1, ((1-alpha)*_opacityPrev) + (alpha*_opacity) }
//         Video::Context::SetTransformScale(ctx, 0, m, v4)
//     }
//     The gate is `CMP EDI,1 / JG`, i.e. the single-instance case sets the
//     transform and the multi-instance case leaves it to the caller.
//
//  B. n = this->_boundsPtr->liveCount
//     if (!ctxData->BeginInstancedIndexedVertices(4*n, 0x34, 6*n, instances))
//         return;
//
//  C. for (i = 0; i < n; ++i)        // n RE-READ from the pool each iteration
//         rec = pool.particles + (n - i - 1) * 0x58      // BACKWARD
//         ... four vertices of 0x34 bytes each, memmove'd into the dynamic
//             vertex cursor and the cursor advanced by the stride ...
//         ... six 16-bit indices (b-2, b, b-1, b-2, b+1, b) with b starting at
//             2 and advanced by 4 AS A 16-BIT ADD ...
//     ctxData->EndInstancedIndexedVertices(0)
//
// THE 16-BIT WRAP IN PART C IS LOAD-BEARING. Retail's `ADD R14W,0x4` wraps at
// 65536, and every index store is a 16-bit `MOV word`. A port that kept the base
// in an `int` would emit different indices past 16384 particles. Reproduced with
// a `uint16_t`.
//
// THE BACKWARD WALK IS ALSO LOAD-BEARING and is NOT a decompiler artefact: the
// index is computed as `(count - i - 1) * 0x58` with `CDQE` after the
// subtraction, while the index buffer is filled FORWARD. So particle k is drawn
// with quad (count-1-k). Any reordering changes the draw order of overlapping
// translucent sprites, which is visible.
//
// The vertex record is 0x34 bytes — the same 0x34 passed to
// BeginInstancedIndexedVertices as the stride, which is how the two agree:
//   +0x00 3f  posA lerped        +0x0c 1i  zero
//   +0x10 3f  posB lerped        +0x1c 1f  scroll  = ((1-a)*prev) + (a*cur)
//   +0x20 2f  uv corner          +0x28 2f  corner offset * size
//   +0x30 1f  rotation radians = (((1-a)*prevDeg)*k + (a*curDeg)*k), k = pi/180
// ============================================================================
namespace {

// The two corner tables retail materialises on its own stack immediately before
// the loop, as 14 immediate stores. Transcribed as the immediates, in order.
// uv:      (0,0) (1,0) (1,1) (0,1)
// corner:  (-1,1) (1,1) (1,-1) (-1,-1)
const float kCornerUV[8]     = { 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f };
const float kCornerOffset[8] = { -1.0f, 1.0f, 1.0f, 1.0f, 1.0f, -1.0f, -1.0f, -1.0f };

}  // namespace

extern "C" void RtGraphics_ParticleInstance_Draw(
        void* selfv, void* contextv, const void* /*camera — never read*/,
        void* /*material ExternalPtr — never read*/, float alpha,
        int32_t /*arg5 — never read*/, int32_t /*arg6 — never read*/,
        int32_t instances) {
    ++g_piDraw;

    const Geometry*         self = static_cast<const Geometry*>(selfv);
    const ParticleInstance* pi   = static_cast<const ParticleInstance*>(selfv);
    uint8_t*                ctx  = B(contextv);
    const float             one  = One();

    // ---- A: the single-instance transform --------------------------------
    if (instances <= 1) {
        // ENDGAME B.3.1 step 2: Quaternion::Lerp and Transform::AsMatrix are
        // members now; only Video::Context::SetTransformScale is still a door.
        // NAME DOOR — SYMBOL: ?SetTransformScale@Context@Video@@QEAAXHAEBVMatrix@Math@@
        // AEBVVector4@4@@Z (0x18001a400), plus the two Math exports described at
        // GetInstanceTransform above. CONDITION: all three are public exports and
        // the Math pair is ported, so NAME reaches our bodies.
        // CONSEQUENCE: a null skips part A only; parts B and C still run, which
        // is worse than a crash to debug, so it is reported.
        using SetTransformScaleFn = void (*)(void*, int32_t, const Matrix*,
                                             const float*);
        static SetTransformScaleFn setXform = nullptr;
        static bool resolved = false;
        if (!resolved) {
            resolved = true;
            setXform = reinterpret_cast<SetTransformScaleFn>(ResolveExport(
                "?SetTransformScale@Context@Video@@QEAAXHAEBVMatrix@Math@@AEBVVector4@4@@Z"));
            if (!setXform)
                std::fprintf(stderr, "[GFXPART27] PI::Draw part A: "
                                     "SetTransformScale=%p -- the particle transform is "
                                     "NOT set\n", (void*)setXform);
        }
        if (setXform) {
            const uint8_t* cur = B(&self->_transform);
            const uint8_t* prv = B(&self->_prevTransform);

            Transform blended;
            const float px = F(prv, 0x10), py = F(prv, 0x14), pz = F(prv, 0x18);
            blended.translation.x = (F(cur, 0x10) - px) * alpha + px;
            blended.translation.y = (F(cur, 0x14) - py) * alpha + py;
            blended.translation.z = (F(cur, 0x18) - pz) * alpha + pz;

            Quaternion q;
            const float t = alpha;          // retail spills alpha and passes &it
            q = reinterpret_cast<const Quaternion*>(prv)->Lerp(
                *reinterpret_cast<const Quaternion*>(cur),
                reinterpret_cast<const Scalar&>(t));   // B.3.1 bridge
            blended.rotation = q;

            Matrix m;
            m = blended.AsMatrix();

            // Retail: XMM0 = alpha * _opacity; XMM1 = (1 - alpha) * _opacityPrev;
            // XMM1 = XMM1 + XMM0. The three 1.0f are separate immediate stores.
            const float v4[4] = { one, one, one,
                                  ((one - alpha) * self->_opacityPrev)
                                      + (alpha * self->_opacity) };
            setXform(contextv, 0, &m, v4);
        }
    }

    // ---- B: open the dynamic instanced stream -----------------------------
    const uint8_t* pool = B(pi->_boundsPtr);
    void* ctxData = *reinterpret_cast<void**>(ctx + 0x2160);   // Context::_contextData
    const int32_t n0 = I32(pool, kPool_LiveCount);

    // ContextData vtable slot 18 (+0x90) = BeginInstancedIndexedVertices and
    // slot 19 (+0x98) = EndInstancedIndexedVertices, per
    // dx11/types/VideoDX11Data.h's slot map, which derived every name from the
    // Runtime thunk that dispatches to it. Called through the OBJECT's vtable,
    // which is where our own dx11/ backend already lives.
    using BeginFn = bool (*)(void*, int32_t, int32_t, int32_t, int32_t);
    using EndFn   = void (*)(void*, int32_t);
    if (!reinterpret_cast<BeginFn>(VSlot(ctxData, 0x90))(
            ctxData, 4 * n0, 0x34, 6 * n0, instances))
        return;

    // ---- C: four vertices + six indices per particle ----------------------
    const float oneMinus = one - alpha;      // retail: SUBSS XMM11,XMM10, once
    const float degToRad = DegToRad();
    uint16_t    base     = 2;                // retail: LEA R14D,[R15+2] with R15 = 0
    int32_t     count    = I32(pool, kPool_LiveCount);
    for (int32_t i = 0; i < count; ++i) {
        // (count - i - 1) * 0x58, with the subtraction done in 32 bits and THEN
        // widened (CDQE). `count` is the value read at the bottom of the previous
        // iteration, exactly as retail keeps it in EAX.
        const int32_t  slot = count - i - 1;
        const uint8_t* rec  = *reinterpret_cast<const uint8_t* const*>(pool + kPool_ParticleArrayData)
                              + static_cast<int64_t>(slot) * kRec_Stride;

        // size  = ((1-a) * prevSize) + (a * curSize)
        const float size = (oneMinus * F(rec, kRec_SizePrv)) + (alpha * F(rec, kRec_SizeCur));
        // scroll = ((1-a) * prevScroll) + (a * curScroll)
        const float scroll = (oneMinus * F(rec, kRec_ScrollPrv))
                             + (alpha * F(rec, kRec_ScrollCur));
        // rotation: each term is scaled by pi/180 BEFORE the add, which is how
        // retail emits it (MULSS XMM1,XMM12 on each side, then ADDSS).
        const float rot = ((oneMinus * F(rec, kRec_RotPrv)) * degToRad)
                          + ((alpha * F(rec, kRec_RotCur)) * degToRad);

        float posA[3], posB[3];
        for (int k = 0; k < 3; ++k) {
            const float pa = F(rec, kRec_PosAPrev + 4 * k);
            posA[k] = (F(rec, kRec_PosA + 4 * k) - pa) * alpha + pa;
            const float pb = F(rec, kRec_PosBPrev + 4 * k);
            posB[k] = (F(rec, kRec_PosB + 4 * k) - pb) * alpha + pb;
        }

        uint8_t vertex[0x34];
        std::memcpy(vertex + 0x00, posA, 12);
        const int32_t zero = 0;
        std::memcpy(vertex + 0x0c, &zero, 4);
        std::memcpy(vertex + 0x10, posB, 12);
        SetF(vertex, 0x1c, scroll);
        SetF(vertex, 0x30, rot);

        const int32_t stride = I32(ctx, 0x20e8);   // Context::_dynamicVertexStride
        for (int c = 0; c < 4; ++c) {
            SetF(vertex, 0x20, kCornerUV[c * 2 + 0]);
            SetF(vertex, 0x24, kCornerUV[c * 2 + 1]);
            SetF(vertex, 0x28, size * kCornerOffset[c * 2 + 0]);
            SetF(vertex, 0x2c, size * kCornerOffset[c * 2 + 1]);
            // 0x18008a290 is the CRT's `memmove` (disassembled for this lane:
            // the full rep-movsb / 32-byte-unrolled / MOVNTI ladder, ERMS bit
            // tested at 0x1801fc65c). std::memmove is the same contract.
            uint8_t* cursor = *reinterpret_cast<uint8_t**>(ctx + 0x20e0);
            std::memmove(cursor, vertex, static_cast<size_t>(stride));
            // Retail re-reads the stride after the call and adds it to the cursor.
            const int32_t adv = I32(ctx, 0x20e8);
            *reinterpret_cast<uint8_t**>(ctx + 0x20e0) = cursor + adv;
        }

        // Six 16-bit indices, and every arithmetic step is 16-bit.
        uint16_t* ip = *reinterpret_cast<uint16_t**>(ctx + 0x20f0);
        const uint16_t b = base;
        const uint16_t idx[6] = { static_cast<uint16_t>(b - 2), b,
                                  static_cast<uint16_t>(b - 1),
                                  static_cast<uint16_t>(b - 2),
                                  static_cast<uint16_t>(b + 1), b };
        for (int k = 0; k < 6; ++k) {
            *ip = idx[k];
            ++ip;
            *reinterpret_cast<uint16_t**>(ctx + 0x20f0) = ip;
        }
        base = static_cast<uint16_t>(base + 4);   // retail: ADD R14W,4 — wraps

        ++g_piDrawQuads;
        count = I32(pool, kPool_LiveCount);       // retail re-reads it here
    }

    reinterpret_cast<EndFn>(VSlot(ctxData, 0x98))(ctxData, 0);
}

// ============================================================================
// The emitter's allocate / destroy / free trio   (cutover closure batch D1)
// ============================================================================
// SetParticle above already reaches two of these by address (0x1800205a0 to
// make an emitter, 0x180020d60 to release one). All three were read out of
// their own disassembly:
//
//   0x1800205a0  `operator new` for the 0x18-byte emitter record. The size AND
//                the alignment are immediates: `MOV EDX,0x18` then
//                `LEA R8D,[RDX-0x10]` — i.e. align 8, computed from the size
//                rather than written out, which is why a reader who skims it
//                sees "0x18, 0x18" and gets the alignment wrong.
//   0x180020600  the matching per-class `operator delete`: the `if (p)` test
//                plus the Enter / Free(_singleton, p) / Leave triple, i.e. the
//                body runtime/src/system/closure_batch2b.cpp's banner names as
//                this family's single constant.
//   0x180020d60  `~X` then that `operator delete`, returning `this` — the
//                shape MSVC emits for a scalar deleting destructor whose flag
//                argument has been constant-folded away. The `~X` it calls,
//                0x180021490, is the twenty-somethingth copy of the
//                parameterless J1 `~Array` body and is registered as such in
//                runtime/src/collection/array.cpp; it is reached HERE by
//                address so the two cannot drift.
//
// The allocator is reached by address for lane Q's reason (array.cpp's "THE
// ALLOCATOR BOUNDARY"): a heap cannot be half ours.
extern "C" void* RtGraphics_Emitter_OperatorNew(void) {
    return rtcoll::MemAlloc(0x18, 8);                  // 1800205d1 / 1800205d6
}
extern "C" void RtGraphics_Emitter_OperatorDelete(void* p) {
    if (!p) return;                                    // 180020600 TEST/JZ
    using FreeFn = void (*)(void*);
    RtAddr<FreeFn>(0x1800c7ad0ull)(p);                 // 0x1800c7ad0 re-tests null
}
extern "C" void* RtGraphics_Emitter_DeleteDtor(void* p) {
    using DtorFn = void* (*)(void*);
    RtAddr<DtorFn>(0x180021490ull)(p);                 // 180020d76  ~X (a J1 body)
    RtGraphics_Emitter_OperatorDelete(p);              // 180020d7e
    return p;                                          // 180020d83  MOV RAX,RBX
}

// ============================================================================
// Registration
// ============================================================================
// ============================================================================
// CUTOVER CLOSURE BATCH D2 "helpers-high" (2026-09-12)
// ============================================================================
// The two unnamed Graphics::Particle bodies this file has been reaching by
// address since Wave 27. Both are now ported and registered, so the address
// doors above land on our code and at cutover they still exist.

// 0x180061a80 -- the per-TU `operator new(0x40)` for a pool record. The whole
// body is `EnterCriticalSection(_singleton + 0x640);
// MemoryAllocator::Allocate(_singleton, 0x40, 8); LeaveCriticalSection`, i.e.
// ?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z (0x1800c7a50) with the size and
// the alignment folded in. It takes NO arguments and returns the block; a null
// return is retail's own tolerated case (the caller checks), so there is no
// guard here.
extern "C" void* RtGraphics_ParticlePool_OperatorNew() {
    return rtcoll::MemAlloc(0x40, 8);
}

// 0x180103bf0 -- the pool record's constructor. RCX = the 0x40-byte block,
// RDX = the owning Graphics::Particle. Store list read from the disassembly and
// already tabulated above `ParticleBoundsRecord` in
// runtime/types/graphics/Particle.h:
//     +0x00..+0x08  three floats  0x4cbebc20 (+1e8f)   Volume::Box::min seed
//     +0x0c..+0x14  three floats  0xccbebc20 (-1e8f)   Volume::Box::max seed
//     +0x18         qword 0        the live-particle Array's _data
//     +0x20         qword 0        that Array's _size (+0x24 its capacity)
//     +0x28         dword 0        the live particle count
//     +0x2c         dword -1       the per-frame tag
//     +0x30         qword 0
//     +0x38         qword 0
// then `SetSize(this + 0x18, particle[+0x28])` through 0x180061bb0.
// The bit patterns are written as raw u32 rather than float literals so the
// transcription is the shipped immediate and not whatever the compiler rounds
// to; +1e8f/-1e8f is what they decode to.
extern "C" void* RtGraphics_ParticlePoolRecord_Ctor(void* recv, void* particlev) {
    uint32_t* r = static_cast<uint32_t*>(recv);
    r[0] = 0x4cbebc20u;  r[1] = 0x4cbebc20u;  r[2] = 0x4cbebc20u;
    r[3] = 0xccbebc20u;  r[4] = 0xccbebc20u;  r[5] = 0xccbebc20u;
    uint8_t* b = static_cast<uint8_t*>(recv);
    *reinterpret_cast<uint64_t*>(b + 0x18) = 0;
    *reinterpret_cast<uint64_t*>(b + 0x20) = 0;
    *reinterpret_cast<int32_t*> (b + 0x28) = 0;
    *reinterpret_cast<int32_t*> (b + 0x2c) = -1;
    *reinterpret_cast<uint64_t*>(b + 0x30) = 0;
    *reinterpret_cast<uint64_t*>(b + 0x38) = 0;

    // ADDRESS DOOR -- SYMBOL: 0x180061bb0, `Collection::Array<T>::SetSize` for
    // the record's 0x58-byte live-particle slot. Ported and registered by
    // runtime/src/collection/array.cpp in this same commit, so registry-first
    // makes this our body; it is still reached by address because that is how
    // retail reaches it and because array.cpp owns the family.
    using ArraySetSizeFn = void (*)(void*, int32_t);
    ArraySetSizeFn setSize = RtAddr<ArraySetSizeFn>(0x180061bb0ull);
    if (setSize) {
        setSize(b + 0x18, *reinterpret_cast<const int32_t*>(
                              static_cast<const uint8_t*>(particlev) + 0x28));
    } else {
        // Retail has no such branch. Ours can only be taken if the registry AND
        // the DLL both lack the address, which at cutover means a missing port
        // rather than a runtime condition -- say so instead of silently
        // shipping an unsized sub-array.
        std::fprintf(stderr, "[GFXPART27] pool-record ctor: 0x180061bb0 unresolved "
                             "-- the record's live-particle array is UNSIZED\n");
    }
    return recv;
}

int RtGraphicsWave27GfxpartRegister() {
    int count = 0;

    // Cutover closure batch D1. No exported name in either build; the address
    // is the only door. Registered unconditionally (DIRECTION.md §2).
    RegisterRuntimeAddr(0x1800205a0ull, (void*)&RtGraphics_Emitter_OperatorNew);    ++count;
    RegisterRuntimeAddr(0x180020600ull, (void*)&RtGraphics_Emitter_OperatorDelete); ++count;
    RegisterRuntimeAddr(0x180020d60ull, (void*)&RtGraphics_Emitter_DeleteDtor);     ++count;


    RegisterRuntimeExport("?Serialize@Particle@Graphics@@UEAA_NAEAVStream@IO@@@Z",
                          (void*)&RtGraphics_Particle_Serialize); ++count;
    RegisterRuntimeExport(
        "?Draw@ParticleInstance@Graphics@@UEBAXAEAVContext@Video@@AEBVCamera@2@"
        "V?$ExternalPtr@VMaterialInstance@Graphics@@@Object@@MHHH@Z",
        (void*)&RtGraphics_ParticleInstance_Draw); ++count;
    RegisterRuntimeExport(
        "?AddDrawItems@ParticleInstance@Graphics@@UEBAXAEBVScene@2@AEAVRenderer@2@@Z",
        (void*)&RtGraphics_ParticleInstance_AddDrawItems); ++count;
    RegisterRuntimeExport(
        "?GetInstanceTransform@ParticleInstance@Graphics@@UEBA?AVMatrix@Math@@HM@Z",
        (void*)&RtGraphics_ParticleInstance_GetInstanceTransform); ++count;
    RegisterRuntimeExport(
        "?SetParticle@ParticleInstance@Graphics@@UEAAXV?$ExternalPtr@VParticle@Graphics@@"
        "@Object@@@Z",
        (void*)&RtGraphics_ParticleInstance_SetParticle); ++count;
    RegisterRuntimeExport(
        "?SetParticle@ParticleSystemInstance@Graphics@@UEAAXV?$ExternalPtr@VParticle@"
        "Graphics@@@Object@@@Z",
        (void*)&RtGraphics_ParticleSystemInstance_SetParticle); ++count;

    // ---- CUTOVER CLOSURE BATCH D2 "helpers-high" (2026-09-12) -------------
    // Unexported in both builds, so RegisterRuntimeAddr only (these do not
    // move `count`, which is the EXPORT count this lane's attendance line
    // prints).
    RegisterRuntimeAddr(0x180061a80, (void*)&RtGraphics_ParticlePool_OperatorNew);
    RegisterRuntimeAddr(0x180103bf0, (void*)&RtGraphics_ParticlePoolRecord_Ctor);

    return count;   // 6 exports + 3 batch-D1 addresses + 2 batch-D2
}

// The attendance line names THIS lane, so a run log carries a line only this
// build emits — the on-disk md5 says which exe is on disk, not which one ran.
// Attendance is printed SEPARATELY from any comparison so a reader cannot
// mistake "the body ran" for "the body was verified"
// (reference_uncalled_port_is_not_verified.md).
void RtGraphicsWave27GfxpartSummary() {
    printf("[GFXPART27] ParticleSer=%llu (records=%llu) PIDraw=%llu (quads=%llu) "
                "AddDrawItems=%llu (emitted=%llu) GetInstXform=%llu "
                "PISetParticle=%llu (rngDraws=%llu) PSISetParticle=%llu\n",
                (unsigned long long)g_particleSerialize,
                (unsigned long long)g_particleSerRecords,
                (unsigned long long)g_piDraw,
                (unsigned long long)g_piDrawQuads,
                (unsigned long long)g_piAddDrawItems,
                (unsigned long long)g_piAddDrawItemsEmit,
                (unsigned long long)g_piGetInstanceXform,
                (unsigned long long)g_piSetParticle,
                (unsigned long long)g_piSetParticleRand,
                (unsigned long long)g_psiSetParticle);
}
