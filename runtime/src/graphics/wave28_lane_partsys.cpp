// ============================================================================
// runtime/src/graphics/wave28_lane_partsys.cpp — WAVE 28 LANE PARTSYS
// ============================================================================
//
// | symbol                                                    | retail      |
// |-----------------------------------------------------------|-------------|
// | ?Update@ParticleSystemInstance@Graphics@@UEAAX…@Z          | 0x1800a5600 |
// | ?RemoveState@ParticleInstance@Graphics@@IEAAXXZ            | 0x1801040b0 |
// | ?Update@SunLight@Graphics@@UEAAX…@Z                        | 0x1801067a0 |
// | ?Draw@SunLight@Graphics@@UEBAX…MHHH@Z                      | 0x180106790 |
// | ?AddDrawItems@SunLight@Graphics@@UEBAX…@Z                  | 0x180106f50 |
//
// `?Update@ParticleSystemInstance@` is the whole particle simulation and the
// last unported slot of `??_7ParticleSystemInstance@Graphics@@6B@` (9/10 after
// Wave 27 lane GFXPART).
//
// ---------------------------------------------------------------------------
// PROVENANCE — objdump, not Ghidra
// ---------------------------------------------------------------------------
// Wave 27 found TWO GhidraMCP defects in `disassemble_function` alone (it can
// serve the PROFILE build while reporting retail as current, and it can serve a
// DIFFERENT overlapping function under the address asked for, with no tell).
// So every byte, address, extent and constant below was read with
//
//     objdump -d --start-address=… --stop-address=… "C:/GOG Games/Banished/Runtime-x64.dll"
//     pefile against the same file for the export table and .pdata
//
// Ghidra's decompiled C was used ONLY to recover the control-flow skeleton, and
// it was wrong about at least one thing that matters: it printed
// `Math::Transform::FromDirection(local_108, &local_118)` — TWO arguments — for
// a call whose own mangled name
// (`?FromDirection@Transform@Math@@SA?AV12@AEBVVector3@2@0@Z`) says
// `static Transform FromDirection(const Vector3&, const Vector3&)`, i.e. a
// hidden return slot plus TWO Vector3s. Retail loads all three
// (`LEA RCX,[RSP+0x60] / LEA RDX,[RSP+0x50] / LEA R8,[RSP+0x20]` at
// 0x1800a5fe5) and the third is a ZEROED Vector3 built two instructions
// earlier. Ghidra dropped it — exactly
// reference_ghidra_c_output_vs_disassembly.md.
//
// ---------------------------------------------------------------------------
// EXTENT — .pdata is a LOWER bound, and this body proves it
// ---------------------------------------------------------------------------
// `?Update@ParticleSystemInstance@` is FIVE chained .pdata fragments:
//
//   0x1800a5600..0x1800a569d  0x09d   flags 0
//   0x1800a569d..0x1800a57f6  0x159   UNW_FLAG_CHAININFO
//   0x1800a57f6..0x1800a62f1  0xafb   UNW_FLAG_CHAININFO
//   0x1800a62f1..0x1800a6867  0x576   UNW_FLAG_CHAININFO
//   0x1800a6867..0x1800a6a7a  0x213   UNW_FLAG_CHAININFO
//
// Reading only the first entry would have sized this body at 0x9d bytes — 0.7%
// of the truth. The real extent is 0x1800a5600..0x1800a6a79 = 0x147a bytes
// (the brief's 0x1479 counts the last byte inclusive), independently
// re-derived here rather than trusted.
//
// ---------------------------------------------------------------------------
// THE ELEVEN INLINED MT19937 DRAWS, AND EVERY CONSTANT LIFTED
// ---------------------------------------------------------------------------
// The brief says twelve. ELEVEN tempering sequences appear in this body, all in
// the spawn loop, counted by `and $0xff3a58ad` in the objdump listing (see
// §"RNG constant table" in docs/runtime_port/WAVE28_LANE_PARTSYS.md for every
// address). Every one of them is byte-identical to the others and to
// `?SetParticle@ParticleInstance@Graphics@@`'s, which Wave 27 already lifted:
//
//     y ^= y >> 11
//     y ^= (y & 0xff3a58ad) << 7        <- NOT the textbook 0x9d2c5680
//     y ^= (y & 0xffffdf8c) << 15       <- NOT the textbook 0xefc60000
//     y ^= y >> 18
//
// and the index advance is `INC EDX / CMP EDX,0x270 / CMOVE EDX,0` with a
// `?NextGroup@Random@System@@AEAAXXZ` call when the index is 0 BEFORE the read.
//
// The float constants are lifted as BIT PATTERNS read out of the DLL with
// pefile, never retyped:
//
//     0x180161658  0x00000000   0.0f     (the `qw * 0.0f` term in the quat mul)
//     0x18016165c  0x2f800000   2^-32    (the RNG word -> unit float scale)
//     0x18016167c  0x3c8efa35   pi/180
//     0x180161694  0x3f000000   0.5f
//     0x18016169c  0x3f800000   1.0f
//     0x1801616d8  0x40000000   2.0f
//     0x180161710  0x42b40000   90.0f
//     0x180161718  0x437f0000   255.0f
//     0x1801617c0  0x80000000   the float sign mask (XORPS negate)
//
// Every draw is turned into a signed-symmetric unit float the same way:
//
//     r = 1.0f - (float)(uint32)y * 2^-32 * 2.0f        // in (-1, +1]
//
// retail forms the two multiplies SEPARATELY (`MULSS xmm7` then `MULSS xmm8`)
// rather than folding them into a single 2^-31, and subtracts from a register
// holding 1.0f. Folding them is algebraically identical and numerically
// different (reference_moved_term_hides_where_terms_cancel.md), so the two
// multiplies are reproduced.
//
// ---------------------------------------------------------------------------
// A NAMING CORRECTION TO WAVE 27's RECORD MAP — and the licence, not just the
// sentence (kickoff warning 13)
// ---------------------------------------------------------------------------
// `wave27_lane_gfxpart.cpp` names the 0x58-byte particle record's fields from
// `ParticleInstance::Draw`, which only READS them. Two of its names are wrong,
// and the reason they are wrong is that a READER cannot tell a 3-float pair
// that is a position from one that is a colour:
//
//   kRec_PosB (+0x28) / kRec_PosBPrev (+0x34)  is the particle COLOUR (rgb).
//       THIS body writes it from `ParticleSystem[+0xdc..+0xe3]`, eight BYTES
//       divided by 255.0f and lerped between two endpoints. A byte/255 is not
//       a coordinate.
//   kRec_ScrollCur (+0x40) / kRec_ScrollPrv (+0x44)  is the particle ALPHA.
//       THIS body writes BOTH with the SAME value in one 8-byte store
//       (`MOV [r8+r9+0x98],RAX` at 0x1800a67f0 with a duplicated float), so the
//       "cur/prev" lerp Draw performs on it is a no-op by construction.
//   The field Wave 27 could not see at all is the rotation RATE, which lives in
//       the per-particle EXTRA array (+0x0c of a 0x14-byte stride), not in the
//       record. +0x48/+0x4c IS the rotation, and Draw's degrees reading stands.
//
// THE LICENCE, not the sentence: naming a field from a reader that can only see
// its SHAPE is the defect (reference_a_writer_beats_readers_agreeing_on_a_sign.md
// is the same shape one level down). Wave 27's kRec_* constants are NOT renamed
// here — that file's names are load-bearing for its own bodies and renaming
// them is a merge hazard for no behaviour change. The correction is recorded
// here and in the lane doc, on the surface the next particle lane will reach.
//
// ---------------------------------------------------------------------------
// OWNERSHIP (kickoff warning 6) — `RemoveState` frees CONDITIONALLY
// ---------------------------------------------------------------------------
// `ParticleInstance::RemoveState` frees `_boundsPtr` ONLY when
// `_resource->_active == 0`. That is not a null-safety check, it is an
// OWNERSHIP test, and it pairs exactly with
// `?SetParticle@ParticleSystemInstance@` (Wave 27): when `_active` is set the
// pool is BORROWED from `_resource->_boxes[_index]` and is owned by the
// resource; when it is clear the pool was `operator new`'d per instance and is
// owned here. A teardown that freed unconditionally would free a live shared
// object — the over-release direction, which is the dangerous one. Established
// by reading the WRITER (SetParticle), not by the element type.
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../funcs/rt_libm.h"
#include "../../types/graphics/Geometry.h"
#include "../../types/graphics/Particle.h"
#include "../../types/math/Math.h"

using RtGraphics::Geometry;
using RtGraphics::Particle;
using RtGraphics::ParticleInstance;
using RtGraphics::ParticleSystemInstance;

// Forward-declared rather than pulled in from standalone/resolve.h, for the
// reason rt_port.h gives: that header drags in <windows.h> and runtime/src/
// must stay platform-neutral.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

// Wave 25 lane ported this and registers it by name; retail CALLs it with a
// rel32, so this port calls OUR body directly rather than going through the
// registry — the two halves of one frame must come from one build.
Transform* RtMath_Transform_FromDirection(Transform* ret,
                                                    const Vector3* direction,
                                                    const Vector3* offset);

namespace {

// ---------------------------------------------------------------------------
// Attendance. A run that entered none of these says so rather than staying
// silent, and `calls=` is read separately from any comparison
// (reference_uncalled_port_is_not_verified.md).
// ---------------------------------------------------------------------------
uint64_t g_psiUpdate        = 0;   // entries
uint64_t g_psiUpdateEarly   = 0;   // took the "same frame already" SetBounds tail
uint64_t g_psiSpawned       = 0;   // particles spawned
uint64_t g_psiAged          = 0;   // particles stepped
uint64_t g_psiKilled        = 0;   // particles compacted out
uint64_t g_piRemoveState    = 0;
uint64_t g_piRemoveStateFree= 0;   // the arm that actually owns the pool
uint64_t g_sunUpdate        = 0;
uint64_t g_sunDrawMhhh      = 0;
uint64_t g_sunAddDrawItems  = 0;
uint64_t g_sunAddDrawEmit   = 0;

// ---------------------------------------------------------------------------
// The shipped float constants, as bit patterns read out of Runtime-x64.dll with
// pefile. `3.14159f/180.0f` and `1.0f/4294967296.0f` are DIFFERENT floats from
// what the DLL ships; these are the DLL's
// (reference_crt_coefficients_must_be_lifted_not_retyped.md).
// ---------------------------------------------------------------------------
inline float FromBits(uint32_t bits) {
    float f;
    std::memcpy(&f, &bits, sizeof f);
    return f;
}
const float kZero      = FromBits(0x00000000u);  // 0x180161658
const float kInv2p32   = FromBits(0x2f800000u);  // 0x18016165c
const float kDegToRad  = FromBits(0x3c8efa35u);  // 0x18016167c
const float kHalf      = FromBits(0x3f000000u);  // 0x180161694
const float kOne       = FromBits(0x3f800000u);  // 0x18016169c
const float kTwo       = FromBits(0x40000000u);  // 0x1801616d8
const float kNinety    = FromBits(0x42b40000u);  // 0x180161710
const float k255       = FromBits(0x437f0000u);  // 0x180161718

// Retail negates with `XORPS mem,xmm` against 0x1801617c0 = 0x80000000, i.e. a
// pure sign-bit flip. Reproduced as a bit operation rather than unary minus so
// the -0.0f case is the DLL's and not the compiler's.
inline float NegBits(float v) {
    uint32_t u;
    std::memcpy(&u, &v, sizeof u);
    u ^= 0x80000000u;
    float r;
    std::memcpy(&r, &u, sizeof r);
    return r;
}

inline uint8_t*       B(void* p)       { return static_cast<uint8_t*>(p); }
inline const uint8_t* B(const void* p) { return static_cast<const uint8_t*>(p); }
inline float   F(const void* p, size_t o) { float v;   std::memcpy(&v, B(p) + o, sizeof v); return v; }
inline void    SetF(void* p, size_t o, float v)       { std::memcpy(B(p) + o, &v, sizeof v); }
inline int32_t I32(const void* p, size_t o) { int32_t v; std::memcpy(&v, B(p) + o, sizeof v); return v; }
inline void    SetI32(void* p, size_t o, int32_t v)   { std::memcpy(B(p) + o, &v, sizeof v); }
inline int64_t I64(const void* p, size_t o) { int64_t v; std::memcpy(&v, B(p) + o, sizeof v); return v; }
inline void    SetI64(void* p, size_t o, int64_t v)   { std::memcpy(B(p) + o, &v, sizeof v); }
inline uint32_t U32(const void* p, size_t o) { uint32_t v; std::memcpy(&v, B(p) + o, sizeof v); return v; }
inline void    SetU32(void* p, size_t o, uint32_t v)  { std::memcpy(B(p) + o, &v, sizeof v); }

template <typename Fn>
Fn RtAddr(uintptr_t ghidra) {
    return reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, ghidra));
}

// ---------------------------------------------------------------------------
// Graphics::Particle's static MT19937, and the draw retail inlines.
// Identical in every respect to wave27_lane_gfxpart.cpp's private copy; it is
// repeated rather than exported because that file keeps it in an anonymous
// namespace and this lane must not reshape another wave's translation unit
// (the same decision that file records for its own Serialize helpers).
// ---------------------------------------------------------------------------
using NextGroupFn = void (*)(void* rng);

void* ParticleRng() {
    // DATA DOOR — SYMBOL: ?_rng@Particle@Graphics@@1VRandom@System@@A
    // (retail 0x1801fe7f0). Resolved BY NAME through ResolveExport, which is
    // registry-aware, so a minted copy is followed at cutover and the DLL's is
    // followed today. It is tagged RT_UNPROMOTABLE | HARMFUL on the
    // DLL-PRESENT route precisely because in-DLL RIP-relative readers keep
    // using the DLL's copy; that tag INVERTS to REQUIRED at cutover
    // (reference_unpromotable_is_route_scoped_not_permanent.md).
    // CONSEQUENCE of NULL: every draw returns 0 and every particle spawns
    // identically. Said once on stderr rather than faulting.
    static void* p = nullptr;
    static bool  tried = false;
    if (!tried) {
        tried = true;
        p = ResolveExport("?_rng@Particle@Graphics@@1VRandom@System@@A");
        if (!p)
            std::fprintf(stderr,
                "[PARTSYS28] ?_rng@Particle@Graphics@@ resolved NULL -- every "
                "ParticleSystemInstance::Update spawn will draw 0 and particle "
                "placement will not match retail\n");
    }
    return p;
}

// One draw, byte-for-byte retail's inlined sequence.
uint32_t RngNext(void* rngv) {
    uint8_t* rng = B(rngv);
    if (!rng) return 0u;
    int32_t index = I32(rng, 0);
    if (index == 0) {
        // NAME DOOR — SYMBOL: ?NextGroup@Random@System@@AEAAXXZ (0x1800c7dd0).
        // AEAA is PRIVATE, not unexported — this DLL exports its private
        // members (reference_aeaa_is_private_not_unexported.md) — so a name
        // door is the plain door. Wave 28 lane MISC ports and registers this
        // symbol; ResolveExport is registry-aware, so after that lane merges
        // this call lands in OUR body. Same algorithm either side (MISC
        // verified 0x1800c7dd0 instruction-by-instruction against the Wave 2
        // NextGroupImpl), so it is a no-behaviour-change swap — but any number
        // measured here before that merge was measured against the DLL.
        static NextGroupFn fn = nullptr;
        static bool tried = false;
        if (!tried) {
            tried = true;
            fn = reinterpret_cast<NextGroupFn>(
                ResolveExport("?NextGroup@Random@System@@AEAAXXZ"));
            if (!fn)
                std::fprintf(stderr, "[PARTSYS28] ?NextGroup@Random@System@@ resolved "
                                     "NULL -- the MT19937 state will never be twisted\n");
        }
        if (fn) fn(rng);
        index = I32(rng, 0);
    }
    uint32_t y = U32(rng, 4 + static_cast<size_t>(static_cast<uint32_t>(index)) * 4);
    y ^= y >> 11;
    y ^= (y & 0xff3a58adu) << 7;    // SHIPPED mask, lifted at 0x1800a589c et al
    y ^= (y & 0xffffdf8cu) << 15;   // SHIPPED mask, lifted at 0x1800a58a8 et al
    y ^= y >> 18;
    int32_t next = index + 1;
    if (next == 0x270) next = 0;    // CMP EDX,0x270 / CMOVE EDX,<zero reg>
    SetI32(rng, 0, next);
    return y;
}

// `1.0f - (float)y * 2^-32 * 2.0f`, in retail's exact association: CVTSI2SS
// from a 64-bit register holding the zero-extended word, then two separate
// MULSS, then SUBSS from a register holding 1.0f.
inline float RngSigned(void* rng) {
    const float t = static_cast<float>(static_cast<uint64_t>(RngNext(rng)));
    return kOne - ((t * kInv2p32) * kTwo);
}

// ---------------------------------------------------------------------------
// Timer conversions. Retail INLINES both, but the inlined sequences are
// instruction-identical to the two exported bodies, which Wave 9 lane B3 has
// ported -- so calling them is bit-exact and Timer::_secondsPerTick (a
// QPC-derived runtime global) never has to be duplicated here. This is the
// decision runtime/src/engine/loops.cpp already took, with its reasoning.
//   ?TicksToSeconds@Timer@System@@SAM_J@Z  CVTSI2SD / MULSD spt / CVTPD2PS
//   ?SecondsToTicks@Timer@System@@SA_JM@Z  CVTSS2SD / DIVSD spt / CVTTSD2SI
// ---------------------------------------------------------------------------
using TicksToSecondsFn = float   (*)(int64_t);
using SecondsToTicksFn = int64_t (*)(float);

inline float TicksToSeconds(int64_t t) {
    return RT_EXPORT_ONCE(TicksToSecondsFn, "?TicksToSeconds@Timer@System@@SAM_J@Z")(t);
}
inline int64_t SecondsToTicks(float s) {
    return RT_EXPORT_ONCE(SecondsToTicksFn, "?SecondsToTicks@Timer@System@@SA_JM@Z")(s);
}

// ---------------------------------------------------------------------------
// `Graphics::ParticleInstance::_frameIndex`
// (?_frameIndex@ParticleInstance@Graphics@@1HA, retail 0x1801fe77c) — the
// protected static int every ParticleInstance stamps into its pool record's
// frame tag so a system shared by several instances updates ONCE per frame.
// Also tagged RT_UNPROMOTABLE on the DLL-PRESENT route.
// ---------------------------------------------------------------------------
int32_t* FrameIndexPtr() {
    static int32_t* p = nullptr;
    static bool tried = false;
    if (!tried) {
        tried = true;
        p = reinterpret_cast<int32_t*>(
            ResolveExport("?_frameIndex@ParticleInstance@Graphics@@1HA"));
        if (!p)
            std::fprintf(stderr,
                "[PARTSYS28] ?_frameIndex@ParticleInstance@Graphics@@ resolved NULL -- "
                "ParticleSystemInstance::Update cannot de-duplicate per frame\n");
    }
    return p;
}

// `?_typeInfo@ParticleSystem@Graphics@@0VBaseTypeInfo@Object@@A` (0x1801f9a58),
// read for its FIRST DWORD only -- the typeId. That is the whole of what the
// DLL's consumers of a BaseTypeInfo read
// (reference_basetypeinfo_name_is_content_only.md).
uint32_t ParticleSystemTypeId() {
    static uint32_t id = 0;
    static bool tried = false;
    if (!tried) {
        tried = true;
        void* ti = ResolveExport("?_typeInfo@ParticleSystem@Graphics@@0VBaseTypeInfo@Object@@A");
        if (ti) std::memcpy(&id, ti, sizeof id);
        else    std::fprintf(stderr,
                    "[PARTSYS28] ?_typeInfo@ParticleSystem@Graphics@@ resolved NULL -- "
                    "every IsDerivedFrom will fail and the emitter pointer will be null\n");
    }
    return id;
}

using IsDerivedFromFn = bool (*)(const void* obj, uint32_t typeId);
using SetBoundsFn     = void (*)(void* geometry, const void* box);

// ---------------------------------------------------------------------------
// LAYOUTS THIS BODY READS
// ---------------------------------------------------------------------------
// `Graphics::Particle` fields — the base class Wave 6 lane G owns.
constexpr size_t kParticle_Active = 0x20;   // bool
constexpr size_t kParticle_Boxes  = 0x38;   // ParticleBoundsRecord** (the pool array)
static_assert(offsetof(Particle, _active) == kParticle_Active, "Particle::_active");
static_assert(offsetof(Particle, _f38)    == kParticle_Boxes,  "Particle pool array @ +0x38");

// The 0x40-byte pool record (`ParticleInstance::_boundsPtr`, +0xc8). Wave 27
// measured the whole of it; this body adds the two int64 accumulators at +0x30
// and +0x38, which its ctor zeroes and which nothing had read until now.
constexpr size_t kPool_BoxMin      = 0x00;  // 3 floats
constexpr size_t kPool_BoxMax      = 0x0c;  // 3 floats
constexpr size_t kPool_Records     = 0x18;  // uint8_t* — the 0x58-stride record array
constexpr size_t kPool_Capacity    = 0x20;  // int32 — that array's size
constexpr size_t kPool_LiveCount   = 0x28;  // int32
constexpr size_t kPool_FrameTag    = 0x2c;  // int32
constexpr size_t kPool_SpawnAccum  = 0x30;  // int64 ticks, MEASURED HERE
constexpr size_t kPool_TotalTicks  = 0x38;  // int64 ticks, MEASURED HERE

// The per-particle record, stride 0x58. Names corrected against Wave 27's — see
// the banner. The offsets are identical; only two of the NAMES change.
constexpr size_t kRec_Stride    = 0x58;
constexpr size_t kRec_Lifetime  = 0x00;  // int64 ticks
constexpr size_t kRec_Age       = 0x08;  // int64 ticks
constexpr size_t kRec_Pos       = 0x10;  // 3 floats, current
constexpr size_t kRec_PosPrev   = 0x1c;  // 3 floats
constexpr size_t kRec_Color     = 0x28;  // 3 floats, current   (Wave 27: "PosB")
constexpr size_t kRec_ColorPrev = 0x34;  // 3 floats            (Wave 27: "PosBPrev")
constexpr size_t kRec_Alpha     = 0x40;  // float, current      (Wave 27: "ScrollCur")
constexpr size_t kRec_AlphaPrev = 0x44;  // float               (Wave 27: "ScrollPrv")
constexpr size_t kRec_Rot       = 0x48;  // float, DEGREES
constexpr size_t kRec_RotPrev   = 0x4c;
constexpr size_t kRec_Size      = 0x50;
constexpr size_t kRec_SizePrev  = 0x54;

// `ParticleSystemInstance::_emitter` (+0xd8). Built by
// ?SetParticle@ParticleSystemInstance@ (Wave 27): a Collection::Array header
// followed by one float.
constexpr size_t kEmit_Data  = 0x00;   // uint8_t* — the 0x14-stride extra array
constexpr size_t kEmit_Rate  = 0x10;   // float, the spawn-rate accumulator

// The per-particle EXTRA record, stride 0x14, one per record, parallel array.
constexpr size_t kExt_Stride  = 0x14;
constexpr size_t kExt_Vel     = 0x00;  // 3 floats
constexpr size_t kExt_RotRate = 0x0c;  // float, degrees/sec, random SIGN
constexpr size_t kExt_SizeRate= 0x10;  // float

// `Graphics::ParticleSystem` (a Particle subclass; Particle's base is 0x48).
// This lane does NOT own this class's layout and does not declare a struct for
// it — every field below is a MEASURED offset with the instruction that reads
// it, exactly the shape Wave 27 used for kPool_*.
constexpr size_t kPS_RateAccelA   = 0x4c;  // MULSS 0x4c(%rdi) @ 0x1800a5758
constexpr size_t kPS_RateAccelB   = 0x50;  // MULSS 0x50(%rdi) @ 0x1800a5764
constexpr size_t kPS_DurationTicks= 0x58;  // MOV 0x58(%rdi),%rcx @ 0x1800a5781 (int64)
constexpr size_t kPS_EmitDir      = 0x60;  // 3 floats @ 0x1800a5f65/73/89
constexpr size_t kPS_SpreadX      = 0x6c;  // MULSS @ 0x1800a5a68 group
constexpr size_t kPS_SpreadY      = 0x70;
constexpr size_t kPS_SpreadZ      = 0x74;
constexpr size_t kPS_SpreadScale  = 0x78;  // MULSS 0x78(%rdi) @ 0x1800a5976/59f3/5a9c
constexpr size_t kPS_AngleBRange  = 0x7c;  // MULSS 0x7c(%rdi) @ 0x1800a5ec2
constexpr size_t kPS_AngleARange  = 0x80;  // MULSS 0x80(%rdi) @ 0x1800a5e48
constexpr size_t kPS_SpeedBase    = 0x84;  // ADDSS 0x84(%rdi) @ 0x1800a5dc6
constexpr size_t kPS_SpeedRange   = 0x88;  // MULSS 0x88(%rdi) @ 0x1800a5dbd
constexpr size_t kPS_AccelScale   = 0x8c;  // MOVSS 0x8c(%rdi) @ 0x1800a62f1
constexpr size_t kPS_Damping      = 0x90;  // MOVSS 0x90(%rdi) @ 0x1800a64ba
constexpr size_t kPS_RotBase      = 0x94;  // ADDSS 0x94(%rdi) @ 0x1800a5b98
constexpr size_t kPS_RotRange     = 0x98;  // MULSS 0x98(%rdi) @ 0x1800a5b90
constexpr size_t kPS_SpinBase     = 0x9c;  // ADDSS 0x9c(%rdi) @ 0x1800a5cbc
constexpr size_t kPS_SpinRange    = 0xa0;  // MULSS 0xa0(%rdi) @ 0x1800a5cb4
constexpr size_t kPS_SpinAccel    = 0xa4;  // MULSS 0xa4(%rdi) @ 0x1800a65eb
constexpr size_t kPS_SpinDamp     = 0xa8;  // MULSS 0xa8(%rdi) @ 0x1800a65fa
constexpr size_t kPS_LifeBase     = 0xac;  // ADDSS 0xac(%rdi) @ 0x1800a58e3
constexpr size_t kPS_LifeRange    = 0xb0;  // MULSS 0xb0(%rdi) @ 0x1800a58db
constexpr size_t kPS_GravScale    = 0xb4;  // MOVSS 0xb4(%rdi) @ 0x1800a6300
constexpr size_t kPS_GravX        = 0xb8;
constexpr size_t kPS_GravY        = 0xbc;
constexpr size_t kPS_GravZ        = 0xc0;
constexpr size_t kPS_SizeBase     = 0xc4;  // ADDSS 0xc4(%rdi) @ 0x1800a5c2a
constexpr size_t kPS_SizeRange    = 0xc8;  // MULSS 0xc8(%rdi) @ 0x1800a5c22
constexpr size_t kPS_SizeAccel    = 0xcc;  // MULSS 0xcc(%rdi) @ 0x1800a6627
constexpr size_t kPS_SizeDamp     = 0xd0;  // MULSS 0xd0(%rdi) @ 0x1800a6636
constexpr size_t kPS_ColorT0      = 0xd4;  // SUBSS 0xd4(%rdi) @ 0x1800a63db / 0x1800a6692
constexpr size_t kPS_ColorT1      = 0xd8;  // MOVSS 0xd8(%rdi) @ 0x1800a63d3
constexpr size_t kPS_Color0       = 0xdc;  // bytes +0xdc,+0xdd,+0xde,+0xdf
constexpr size_t kPS_Color1       = 0xe0;  // bytes +0xe0,+0xe1,+0xe2,+0xe3
constexpr size_t kPS_FadeInTicks  = 0xe8;  // CVTSI2SDQ 0xe8(%rdi) @ 0x1800a675c (int64)
constexpr size_t kPS_FadeOutTicks = 0xf0;  // MOV 0xf0(%rdi),%rcx @ 0x1800a672a (int64)

// ---------------------------------------------------------------------------
// `q * (v,0) * conj(q)`, in retail's EXACT association order.
//
// This sequence appears TWICE in the body, at 0x1800a5f33..0x1800a60db and
// 0x1800a60e0..0x1800a62b9, with identical term order and identical register
// shuffling; both were transcribed independently from objdump and agree, which
// is why one helper serves both.
//
// Three things a rewrite would silently change:
//   * `q.x * zero` and `q.y * zero` and `q.z * zero` are REAL multiplies retail
//     performs against a XORPS-zeroed register (the w component of the vector
//     quaternion). They vanish for finite inputs and do not for NaN.
//   * the `w` term is `q.w * [0x180161658]` — a MULSS against a 0.0f in .rdata,
//     not a fold to zero.
//   * the conjugate is formed with XORPS against 0x80000000, a sign-bit flip.
// ---------------------------------------------------------------------------
struct Vec3f { float x, y, z; };

Vec3f QuatRotate(float qx, float qy, float qz, float qw, const Vec3f& v) {
    const float zero = 0.0f;                   // XORPS xmm4,xmm4
    const float nx = NegBits(qx);              // XORPS with 0x1801617c0
    const float ny = NegBits(qy);
    const float nz = NegBits(qz);

    // t = q * (v, 0)
    const float tx = ((v.x * qw) + (qx * zero) + (v.y * qz)) - (v.z * qy);
    const float ty = ((qy * zero) - (v.x * qz)) + (v.y * qw) + (v.z * qx);
    const float tz = ((v.x * qy) + (qz * zero) - (v.y * qx)) + (v.z * qw);
    const float tw = (((qw * kZero) - (v.x * qx)) - (v.y * qy)) - (v.z * qz);

    // r = t * conj(q), only the vector part is kept
    Vec3f r;
    r.x = ((qw * tx) + (tw * nx) + (tz * ny)) - (ty * nz);
    r.y = ((qw * ty) - (tz * nx)) + (tw * ny) + (tx * nz);
    r.z = (((qw * tz) + (ty * nx)) - (tx * ny)) + (tw * nz);
    return r;
}

}  // namespace

// ============================================================================
// void Graphics::ParticleSystemInstance::Update(const System::Time&)
// ?Update@ParticleSystemInstance@Graphics@@UEAAXAEBVTime@System@@@Z
// retail 0x1800a5600 — 0x147a bytes, slot 1 of ??_7ParticleSystemInstance@…6B@
//
// Retail's shape, in order:
//   A  refresh _boundsPtr from the resource's pool array when the resource is
//      active, and return early (through Geometry::SetBounds) when this pool
//      was already stepped THIS frame;
//   B  stamp the frame tag, derive dt and dt², resolve the ParticleSystem;
//   C  advance the two pool accumulators and the spawn-rate accumulator, and
//      zero the rate when the system has expired;
//   D  the SPAWN loop — eleven inlined MT19937 draws per particle;
//   E  the AGE loop, backwards over the live particles: gravity, damping,
//      position integration, the bounding box, spin, size, colour and alpha;
//   F  the inlined Geometry::SetBounds compare-and-dirty;
//   G  the compacting swap-free removal pass.
//
// FAITHFUL UNGUARDED DEREFS, left alone on purpose (CLAUDE.md: don't "fix" a
// faithful crash with a null check — find the upstream null):
//   * `this->_resource` is dereferenced at the very first instruction pair
//     (MOV 0xc0(%rcx),%rcx / CMPB $0,0x20(%rcx)) with no test.
//   * `sys` is null whenever `IsDerivedFrom` says the resource is not a
//     ParticleSystem, and retail reads `sys + 0x4c` two instructions later
//     regardless (CMOVNE leaves EDI zero). Every retail caller hands a
//     ParticleSystem; if our standalone ever reaches that arm, the fault IS the
//     diagnosis.
//   * `this->_emitter` is dereferenced with no test.
// ============================================================================
extern "C" void RtGraphics_ParticleSystemInstance_Update(void* selfv, const void* timev) {
    ++g_psiUpdate;

    uint8_t* self = B(selfv);
    const int64_t deltaTicks = I64(timev, 0);

    // --- A -----------------------------------------------------------------
    uint8_t* res = *reinterpret_cast<uint8_t**>(self + 0xc0);     // _resource
    const bool resActive = (res[kParticle_Active] != 0);

    if (resActive) {
        uint8_t** boxes = *reinterpret_cast<uint8_t***>(res + kParticle_Boxes);
        const int32_t index = I32(self, 0xd0);                    // MOVSXD 0xd0(%rsi)
        *reinterpret_cast<uint8_t**>(self + 0xc8) = boxes[index];
    }

    int32_t* frameIndex = FrameIndexPtr();
    const int32_t frame = frameIndex ? *frameIndex : 0;

    uint8_t* pool = *reinterpret_cast<uint8_t**>(self + 0xc8);    // _boundsPtr

    if (resActive && I32(pool, kPool_FrameTag) == frame) {
        // Retail TAIL-JUMPS to Geometry::SetBounds after unwinding its frame.
        ++g_psiUpdateEarly;
        static SetBoundsFn setBounds = nullptr;
        static bool tried = false;
        if (!tried) {
            tried = true;
            // NAME DOOR — ?SetBounds@Geometry@Graphics@@QEAAXAEBVBox@Volume@@@Z
            // (0x180015700). Ported by Wave 6 lane D (scene.cpp) and registered
            // by name, so ResolveExport lands in OUR body.
            setBounds = reinterpret_cast<SetBoundsFn>(
                ResolveExport("?SetBounds@Geometry@Graphics@@QEAAXAEBVBox@Volume@@@Z"));
        }
        if (setBounds) setBounds(self, pool);
        return;
    }

    // --- B -----------------------------------------------------------------
    SetI32(pool, kPool_FrameTag, frame);

    uint8_t* resource = *reinterpret_cast<uint8_t**>(self + 0xc0);

    const float dt  = TicksToSeconds(deltaTicks);   // CVTSI2SDQ/MULSD/CVTPD2PS
    const float dt2 = dt * dt;

    static IsDerivedFromFn isDerived = nullptr;
    {
        static bool tried = false;
        if (!tried) {
            tried = true;
            isDerived = reinterpret_cast<IsDerivedFromFn>(
                ResolveExport("?IsDerivedFrom@Base@Object@@QEBA_NI@Z"));
        }
    }
    // CMOVNE: `sys` is the resource when the test passes and NULL when it does
    // not — and retail reads through it either way.
    uint8_t* sys = nullptr;
    if (isDerived && isDerived(resource, ParticleSystemTypeId())) sys = resource;

    // --- C -----------------------------------------------------------------
    SetI64(pool, kPool_SpawnAccum, I64(pool, kPool_SpawnAccum) + deltaTicks);
    SetI64(pool, kPool_TotalTicks, I64(pool, kPool_TotalTicks) + deltaTicks);

    uint8_t* emitter = *reinterpret_cast<uint8_t**>(self + 0xd8);
    {
        // rate += dt² * kPS_RateAccelA * kPS_RateAccelB, in retail's order:
        // the dt² register is multiplied by both fields, THEN added.
        float r = dt2;
        r *= F(sys, kPS_RateAccelA);
        r *= F(sys, kPS_RateAccelB);
        r += F(emitter, kEmit_Rate);
        SetF(emitter, kEmit_Rate, r);
    }
    {
        // COMISS 0.0, rate / JA -> zero;  else the duration test.
        const float rate = F(emitter, kEmit_Rate);
        bool zeroIt = (0.0f > rate);
        if (!zeroIt) {
            const int64_t duration = I64(sys, kPS_DurationTicks);
            if (duration > 0 && I64(pool, kPool_TotalTicks) > duration) zeroIt = true;
        }
        if (zeroIt) SetF(emitter, kEmit_Rate, 0.0f);
    }

    // interval = SecondsToTicks(1.0f / rate). Retail loads 1.0f from
    // [0x18016169c] and DIVSS by the rate — an infinity when the rate is 0,
    // whose CVTTSD2SI is 0x8000000000000000. Faithful.
    const int64_t interval = SecondsToTicks(kOne / F(emitter, kEmit_Rate));

    void* rng = ParticleRng();

    // --- D : the spawn loop ------------------------------------------------
    // `while (pool.spawnAccum > interval && pool.live < pool.capacity)`.
    // Retail tests the accumulator FIRST (JLE past the loop), then the capacity
    // at the head of every iteration (JGE out).
    while (I64(pool, kPool_SpawnAccum) > interval) {
        if (I32(pool, kPool_LiveCount) >= I32(pool, kPool_Capacity)) break;

        SetI64(pool, kPool_SpawnAccum, I64(pool, kPool_SpawnAccum) - interval);

        const int32_t slot = I32(pool, kPool_LiveCount);
        uint8_t* extBase = *reinterpret_cast<uint8_t**>(emitter + kEmit_Data);
        uint8_t* rec = *reinterpret_cast<uint8_t**>(pool + kPool_Records)
                       + static_cast<int64_t>(slot) * kRec_Stride;
        uint8_t* ext = extBase + static_cast<int64_t>(slot) * kExt_Stride;
        SetI32(pool, kPool_LiveCount, slot + 1);
        SetI64(rec, kRec_Age, 0);
        ++g_psiSpawned;

        // DRAW 1 @ 0x1800a5865 — lifetime
        {
            const float life = (RngSigned(rng) * F(sys, kPS_LifeRange)) + F(sys, kPS_LifeBase);
            SetI64(rec, kRec_Lifetime, SecondsToTicks(life));
        }
        // DRAWS 2,3,4 @ 0x1800a58fe / 0x1800a5981 / 0x1800a59fe — the spawn
        // offset. Each is `r * spreadScale * 0.5f`, and the axis each one feeds
        // is NOT in draw order: draw 2 -> Z, draw 3 -> Y, draw 4 -> X.
        const float oZ = (RngSigned(rng) * F(sys, kPS_SpreadScale)) * kHalf;
        const float oY = (RngSigned(rng) * F(sys, kPS_SpreadScale)) * kHalf;
        const float oX = (RngSigned(rng) * F(sys, kPS_SpreadScale)) * kHalf;
        const float px = F(sys, kPS_SpreadX) * oX;
        const float py = F(sys, kPS_SpreadY) * oY;
        const float pz = F(sys, kPS_SpreadZ) * oZ;

        const float c0 = static_cast<float>(B(sys)[kPS_Color0 + 2]) / k255;  // +0xde
        const float c1 = static_cast<float>(B(sys)[kPS_Color0 + 1]) / k255;  // +0xdd
        const float c2 = static_cast<float>(B(sys)[kPS_Color0 + 0]) / k255;  // +0xdc

        SetI64(rec, kRec_Alpha, 0);                 // MOV %r15,0x40(%r14): +0x40 AND +0x44
        SetF(rec, kRec_Color + 0,     c0);
        SetF(rec, kRec_Color + 4,     c1);
        SetF(rec, kRec_Color + 8,     c2);
        SetF(rec, kRec_ColorPrev + 0, c0);
        SetF(rec, kRec_ColorPrev + 4, c1);
        SetF(rec, kRec_ColorPrev + 8, c2);
        SetF(rec, kRec_Pos + 0,     px);
        SetF(rec, kRec_Pos + 4,     py);
        SetF(rec, kRec_Pos + 8,     pz);
        SetF(rec, kRec_PosPrev + 0, px);
        SetF(rec, kRec_PosPrev + 4, py);
        SetF(rec, kRec_PosPrev + 8, pz);

        // DRAW 5 @ 0x1800a5b1a — rotation (degrees), cur == prev
        {
            const float rot = (RngSigned(rng) * F(sys, kPS_RotRange)) + F(sys, kPS_RotBase);
            SetF(rec, kRec_Rot,     rot);
            SetF(rec, kRec_RotPrev, rot);
        }
        // DRAW 6 @ 0x1800a5bac — size, cur == prev
        {
            const float size = (RngSigned(rng) * F(sys, kPS_SizeRange)) + F(sys, kPS_SizeBase);
            SetF(rec, kRec_Size,     size);
            SetF(rec, kRec_SizePrev, size);
        }
        // DRAW 7 @ 0x1800a5c3e — spin rate, into the EXTRA record
        SetF(ext, kExt_RotRate,
             (RngSigned(rng) * F(sys, kPS_SpinRange)) + F(sys, kPS_SpinBase));
        // DRAW 8 @ 0x1800a5ccb — a COIN FLIP on the spin direction. Retail
        // TESTs bit 0 of the tempered word and flips the sign bit when it is
        // CLEAR; it does not use the word for anything else.
        if ((RngNext(rng) & 1u) == 0u)
            SetF(ext, kExt_RotRate, NegBits(F(ext, kExt_RotRate)));
        SetF(ext, kExt_SizeRate, 0.0f);

        // DRAW 9 @ 0x1800a5d45 — speed
        const float speed = (RngSigned(rng) * F(sys, kPS_SpeedRange)) + F(sys, kPS_SpeedBase);
        // DRAW 10 @ 0x1800a5dcf — elevation, as 90 degrees MINUS the draw
        const float angleADeg = kNinety - (RngSigned(rng) * F(sys, kPS_AngleARange));
        // DRAW 11 @ 0x1800a5e55 — azimuth, already in radians
        const float angleB = (RngSigned(rng) * F(sys, kPS_AngleBRange)) * kDegToRad;
        const float angleA = angleADeg * kDegToRad;

        // Retail calls the DLL's own sinf/cosf (0x180127d90 / 0x180126d10) in
        // this exact order: sin(B), sin(A), cos(B), cos(A). RtLibm is the
        // project's seam for exactly these two (runtime/funcs/rt_libm.h) and
        // RT_LIBM=dll routes it back to the DLL's CRT for an A/B.
        const float sinB = RtLibm::Sin(angleB);
        const float sinA = RtLibm::Sin(angleA);
        const float cosB = RtLibm::Cos(angleB);
        const float cosA = RtLibm::Cos(angleA);

        SetF(ext, kExt_Vel + 0, (NegBits(cosA) * sinB) * speed);
        SetF(ext, kExt_Vel + 4, (cosA * cosB) * speed);
        SetF(ext, kExt_Vel + 8, sinA * speed);

        // Rotate the system's emit direction by the instance's own orientation
        // (Geometry::_transform.rotation, +0x84..+0x90) and hand THAT to
        // FromDirection as the look direction — with a ZEROED Vector3 as the
        // second argument, the argument Ghidra dropped.
        const Vec3f emitDir = { F(sys, kPS_EmitDir + 0),
                                F(sys, kPS_EmitDir + 4),
                                F(sys, kPS_EmitDir + 8) };
        const Vec3f dir = QuatRotate(F(self, 0x84), F(self, 0x88),
                                     F(self, 0x8c), F(self, 0x90), emitDir);

        Vector3 dirArg;  dirArg.x = dir.x; dirArg.y = dir.y; dirArg.z = dir.z;
        Vector3 zeroArg; zeroArg.x = 0.0f; zeroArg.y = 0.0f; zeroArg.z = 0.0f;
        Transform basis;
        RtMath_Transform_FromDirection(&basis, &dirArg, &zeroArg);

        // Rotate the freshly drawn velocity into that basis, in place.
        const Vec3f v = { F(ext, kExt_Vel + 0), F(ext, kExt_Vel + 4), F(ext, kExt_Vel + 8) };
        const Vec3f rv = QuatRotate(basis.rotation.x, basis.rotation.y,
                                    basis.rotation.z, basis.rotation.w, v);
        SetF(ext, kExt_Vel + 0, rv.x);
        SetF(ext, kExt_Vel + 4, rv.y);
        SetF(ext, kExt_Vel + 8, rv.z);
    }

    // --- E : the age loop --------------------------------------------------
    // Retail hoists nine things out of the loop; they are hoisted here too, in
    // retail's own association, because folding them back into the loop body
    // would move the rounding.
    const float accelScale = F(sys, kPS_AccelScale);
    float aX = F(sys, kPS_GravScale); aX *= F(sys, kPS_GravX); aX *= accelScale; aX *= dt2;
    float aY = F(sys, kPS_GravScale); aY *= F(sys, kPS_GravY); aY *= accelScale; aY *= dt2;
    float aZ = F(sys, kPS_GravScale); aZ *= F(sys, kPS_GravZ); aZ *= accelScale; aZ *= dt2;

    const float base0 = static_cast<float>(B(sys)[kPS_Color0 + 2]) / k255;  // +0xde
    const float base1 = static_cast<float>(B(sys)[kPS_Color0 + 1]) / k255;  // +0xdd
    const float base2 = static_cast<float>(B(sys)[kPS_Color0 + 0]) / k255;  // +0xdc
    const float base3 = static_cast<float>(B(sys)[kPS_Color0 + 3]) / k255;  // +0xdf (alpha)
    const float end0  = static_cast<float>(B(sys)[kPS_Color1 + 2]) / k255;  // +0xe2
    const float end1  = static_cast<float>(B(sys)[kPS_Color1 + 1]) / k255;  // +0xe1
    const float end2  = static_cast<float>(B(sys)[kPS_Color1 + 0]) / k255;  // +0xe0
    const float end3  = static_cast<float>(B(sys)[kPS_Color1 + 3]) / k255;  // +0xe3

    // Six floats of the pool's Box, zeroed BEFORE the loop rebuilds it. Note
    // this is a ZERO fill, not the ctor's ±1e8 seeding — so an empty system
    // publishes an origin-centred degenerate box, which is retail's behaviour.
    for (size_t i = 0; i < 6; ++i) SetF(pool, kPool_BoxMin + i * 4, 0.0f);

    const float colorSpan = F(sys, kPS_ColorT1) - F(sys, kPS_ColorT0);
    const float invColorSpan = kOne / colorSpan;

    const float delta0 = end0 - base0;
    const float delta1 = end1 - base1;
    const float delta2 = end2 - base2;
    const float delta3 = end3 - base3;

    for (int64_t i = static_cast<int64_t>(I32(pool, kPool_LiveCount)) - 1; i >= 0; --i) {
        uint8_t* rec = *reinterpret_cast<uint8_t**>(pool + kPool_Records)
                       + i * kRec_Stride;
        uint8_t* ext = *reinterpret_cast<uint8_t**>(emitter + kEmit_Data)
                       + i * kExt_Stride;
        ++g_psiAged;

        SetI64(rec, kRec_Age, I64(rec, kRec_Age) + deltaTicks);

        const float damping = F(sys, kPS_Damping);
        const float vx = (aX + F(ext, kExt_Vel + 0)) * damping;
        const float vy = (aY + F(ext, kExt_Vel + 4)) * damping;
        const float vz = (aZ + F(ext, kExt_Vel + 8)) * damping;
        SetF(ext, kExt_Vel + 0, vx);
        SetF(ext, kExt_Vel + 4, vy);
        SetF(ext, kExt_Vel + 8, vz);

        const float nx = F(rec, kRec_Pos + 0) + dt * vx;
        const float ny = F(rec, kRec_Pos + 4) + dt * vy;
        const float nz = F(rec, kRec_Pos + 8) + dt * vz;
        // prev <- the OLD current, copied as raw dwords before the new writes
        SetU32(rec, kRec_PosPrev + 0, U32(rec, kRec_Pos + 0));
        SetU32(rec, kRec_PosPrev + 4, U32(rec, kRec_Pos + 4));
        SetU32(rec, kRec_PosPrev + 8, U32(rec, kRec_Pos + 8));
        SetF(rec, kRec_Pos + 0, nx);
        SetF(rec, kRec_Pos + 4, ny);
        SetF(rec, kRec_Pos + 8, nz);

        // The bounding box. Retail's COMISS/JA idiom keeps the EXISTING value
        // when the comparison is unordered, which `a < b ? a : b` does not
        // reproduce — so each is written as retail's `if (existing <= new)
        // existing = new` / `if (new <= existing) existing = new`.
        {
            float maxZ = F(pool, kPool_BoxMax + 8); if (maxZ <= nz) maxZ = nz;
            float maxY = F(pool, kPool_BoxMax + 4); if (maxY <= ny) maxY = ny;
            float maxX = F(pool, kPool_BoxMax + 0); if (maxX <= nx) maxX = nx;
            float minZ = F(pool, kPool_BoxMin + 8); if (nz <= minZ) minZ = nz;
            float minY = F(pool, kPool_BoxMin + 4); if (ny <= minY) minY = ny;
            float minX = F(pool, kPool_BoxMin + 0); if (nx <= minX) minX = nx;
            SetF(pool, kPool_BoxMin + 0, minX);
            SetF(pool, kPool_BoxMin + 4, minY);
            SetF(pool, kPool_BoxMin + 8, minZ);
            SetF(pool, kPool_BoxMax + 0, maxX);
            SetF(pool, kPool_BoxMax + 4, maxY);
            SetF(pool, kPool_BoxMax + 8, maxZ);
        }

        // Spin: rate' = (dt² * spinAccel + rate) * spinDamp; rot += rate' * dt
        {
            float rate = dt2 * F(sys, kPS_SpinAccel);
            rate += F(ext, kExt_RotRate);
            rate *= F(sys, kPS_SpinDamp);
            SetF(ext, kExt_RotRate, rate);
            const float prev = F(rec, kRec_Rot);
            SetF(rec, kRec_RotPrev, prev);
            SetF(rec, kRec_Rot, rate * dt + prev);
        }
        // Size: identical shape on its own accel/damp pair
        {
            float rate = dt2 * F(sys, kPS_SizeAccel);
            rate += F(ext, kExt_SizeRate);
            rate *= F(sys, kPS_SizeDamp);
            SetF(ext, kExt_SizeRate, rate);
            const float prev = F(rec, kRec_Size);
            SetF(rec, kRec_SizePrev, prev);
            SetF(rec, kRec_Size, rate * dt + prev);
        }

        // The colour parameter: (age/lifetime - t0) / (t1 - t0), clamped to
        // [0,1] with retail's COMISS ordering (0 first, then 1).
        float t = TicksToSeconds(I64(rec, kRec_Age)) / TicksToSeconds(I64(rec, kRec_Lifetime));
        t -= F(sys, kPS_ColorT0);
        t *= invColorSpan;
        if (0.0f > t) t = 0.0f;
        else if (t > kOne) t = kOne;

        SetU32(rec, kRec_ColorPrev + 0, U32(rec, kRec_Color + 0));
        SetU32(rec, kRec_ColorPrev + 4, U32(rec, kRec_Color + 4));
        SetU32(rec, kRec_ColorPrev + 8, U32(rec, kRec_Color + 8));
        SetF(rec, kRec_Color + 0, delta0 * t + base0);
        SetF(rec, kRec_Color + 4, delta1 * t + base1);
        SetF(rec, kRec_Color + 8, delta2 * t + base2);

        // The fade. TWO independent ramps, and retail picks ONE of them with a
        // CMOVB on `1.0f < fadeIn` — the fade-in until it saturates, the
        // fade-out afterwards — then clamps the winner to [0,1] with the 1.0f
        // comparison FIRST this time.
        {
            const int64_t age  = I64(rec, kRec_Age);
            const int64_t life = I64(rec, kRec_Lifetime);
            const int64_t out  = I64(sys, kPS_FadeOutTicks);

            const float fadeIn  = TicksToSeconds(age) / TicksToSeconds(I64(sys, kPS_FadeInTicks));
            const float fadeOut = kOne - (TicksToSeconds((out - life) + age) / TicksToSeconds(out));

            float fade = (kOne < fadeIn) ? fadeOut : fadeIn;
            if (fade > kOne) fade = kOne;
            else if (0.0f > fade) fade = 0.0f;

            const float alpha = ((delta3 * t) + base3) * fade;
            // ONE 8-byte store covering +0x40 AND +0x44 with the same float.
            SetF(rec, kRec_Alpha,     alpha);
            SetF(rec, kRec_AlphaPrev, alpha);
        }
    }

    // --- F : Geometry::SetBounds, inlined --------------------------------
    // Retail compares the six floats with UCOMISS and only writes (and sets
    // dirty bit 1) when one differs. UCOMISS's unordered case falls through to
    // the write, which `!=` on a NaN also does.
    {
        bool same = true;
        for (size_t k = 0; k < 6; ++k)
            if (F(self, 0x34 + k * 4) != F(pool, kPool_BoxMin + k * 4)) { same = false; break; }
        if (!same) {
            for (size_t k = 0; k < 6; ++k)
                SetU32(self, 0x34 + k * 4, U32(pool, kPool_BoxMin + k * 4));
            SetU32(self, 0x2c, U32(self, 0x2c) | 2u);   // Geometry::_dirtyFlags
        }
    }

    // --- G : the compacting removal pass ----------------------------------
    // `if (rec.age >= rec.lifetime) { live--; shift every later record and its
    // extra record down one; DON'T advance i }`.
    for (int32_t i = 0; i < I32(pool, kPool_LiveCount); ) {
        uint8_t* records = *reinterpret_cast<uint8_t**>(pool + kPool_Records);
        uint8_t* rec = records + static_cast<int64_t>(i) * kRec_Stride;
        if (I64(rec, kRec_Age) < I64(rec, kRec_Lifetime)) { ++i; continue; }

        SetI32(pool, kPool_LiveCount, I32(pool, kPool_LiveCount) - 1);
        ++g_psiKilled;

        // Retail re-reads pool->_records and emitter->_data on EVERY iteration
        // of this shift; reproduced, because a cached base would be a different
        // program if anything reallocated underneath (nothing does, today).
        for (int32_t j = i; j < I32(pool, kPool_LiveCount); ++j) {
            uint8_t* base = *reinterpret_cast<uint8_t**>(pool + kPool_Records);
            // Retail copies the 0x58 bytes field by field (two qwords, twelve
            // dwords, three qwords). Source and destination are 0x58 apart and
            // never overlap, so one memcpy is the same program.
            std::memcpy(base + static_cast<int64_t>(j) * kRec_Stride,
                        base + static_cast<int64_t>(j + 1) * kRec_Stride, kRec_Stride);
            uint8_t* ebase = *reinterpret_cast<uint8_t**>(emitter + kEmit_Data);
            std::memcpy(ebase + static_cast<int64_t>(j) * kExt_Stride,
                        ebase + static_cast<int64_t>(j + 1) * kExt_Stride, kExt_Stride);
        }
    }
}

// ============================================================================
// void Graphics::ParticleInstance::RemoveState()
// ?RemoveState@ParticleInstance@Graphics@@IEAAXXZ   retail 0x1801040b0
//
// Retail, transcribed from objdump 0x1801040b0..0x18010411d:
//   res = this->_resource;
//   if (res && res->_active == 0) {
//       pool = this->_boundsPtr;
//       if (pool) { 0x180062320(&pool[0x18]);   // Array<T>::~Array
//                   0x180061ae0(pool);          // per-TU operator delete
//                   this->_boundsPtr = 0; return; }
//   }
//   this->_boundsPtr = 0;
//
// See the banner for why the `_active == 0` test is an OWNERSHIP test and not a
// safety check. The `movq $-2,0x20(%rsp)` at the top is the MSVC unwind
// funclet's state slot and has no effect on the object.
// ============================================================================
extern "C" void RtGraphics_ParticleInstance_RemoveState(void* selfv) {
    ++g_piRemoveState;

    uint8_t* self = B(selfv);
    uint8_t* res  = *reinterpret_cast<uint8_t**>(self + 0xc0);

    if (res && res[kParticle_Active] == 0) {
        uint8_t* pool = *reinterpret_cast<uint8_t**>(self + 0xc8);
        if (pool) {
            ++g_piRemoveStateFree;
            // ADDRESS DOORS. Both are UNNAMED in the shipped export directory
            // (checked with pefile by VA, not assumed — AEAA means private, not
            // unexported), so an address door is the only door.
            //   0x180062320  Collection::Array<T>::~Array — one of the eleven
            //                identical bodies runtime/src/collection/array.cpp
            //                calls ArrayDtorImpl. It is NOT in that file's
            //                J1 registration list; see the lane doc's RESIDUAL
            //                for the one-line entry that would register it.
            //   0x180061ae0  the per-TU `operator delete` (locked MemFree).
            RtAddr<void (*)(void*)>(0x180062320)(pool + 0x18);
            RtAddr<void (*)(void*)>(0x180061ae0)(pool);
            *reinterpret_cast<void**>(self + 0xc8) = nullptr;
            return;
        }
    }
    *reinterpret_cast<void**>(self + 0xc8) = nullptr;
}

// ============================================================================
// The two SunLight slots that are a bare RET
//
//   ?Update@SunLight@Graphics@@UEAAXAEBVTime@System@@@Z        0x1801067a0
//   ?Draw@SunLight@Graphics@@UEBAX…MHHH@Z                      0x180106790
//
// MEASURED: both are `C2 00 00` (RET $0x0) followed by INT3 padding, and
// NEITHER has a .pdata entry — they are leaf functions with no prologue. They
// are not stubs this lane chose to leave empty; they are what the DLL ships.
// The class overrides both purely to REPLACE the Light/Geometry behaviour with
// nothing (SunLight's real work is in DrawShadowMap and the four-int Draw).
// ============================================================================
extern "C" void RtGraphics_SunLight_Update(void* /*self*/, const void* /*time*/) {
    ++g_sunUpdate;
}

extern "C" void RtGraphics_SunLight_Draw_MHHH(void* /*self*/, void* /*context*/,
                                              const void* /*camera*/, void* /*material*/,
                                              float /*alpha*/, int /*a*/, int /*b*/, int /*c*/) {
    ++g_sunDrawMhhh;
}

// ============================================================================
// void Graphics::SunLight::AddDrawItems(const Scene&, Renderer&) const
// ?AddDrawItems@SunLight@Graphics@@UEBAXAEBVScene@2@AEAVRenderer@2@@Z
// retail 0x180106f50..0x180106fff  (0xaf bytes, single .pdata entry)
//
// Retail, transcribed from objdump:
//   if (this->+0xf8 == 0) return;
//   Renderer::AddLight(renderer, this);                     // 0x1801051d0
//   Camera cam = SunLight::GetSunCamera(this, renderer);    // 0x180107010
//   int g = Renderer::BeginDrawGroup(renderer, this->+0x100,
//                                    *(Camera*)(renderer->+0x10 + 8), cam, 1, 1);
//   if (renderer->+0x3c != 0) {
//       Scene::AddGlobalDrawItems(scene, renderer);         // 0x180102330
//       if (renderer->+0x48 != 0)
//           Scene::AddCulledDrawItems(scene, renderer);     // 0x1801017a0
//   }
//   Renderer::EndDrawGroup(renderer, g);                    // 0x180104f50
//
// ALL SIX CALLEES ARE NAMED EXPORTS — checked with pefile by VA before the body
// was written, not assumed (a name the DLL lacks makes ResolveExport return
// NULL in silence and the ResolveAddr fallback hides it:
// reference_resolveexport_phantom_names.md). So this body opens SIX NAME doors
// and ZERO address doors.
//
// The `renderer->+0x3c` / `+0x48` gate is INDEPENDENTLY CORROBORATED: this
// lane read it out of the bytes, and `code/Game/Game/gameupdateloop.cpp:2171`
// already records `renderer+0x3c != 0` as SunLight's shadow gate from the
// caller's side. Two unrelated readings, one conclusion.
//
// `cam` is a by-value `Graphics::Camera` return: retail reserves 0xe0 bytes of
// stack at RSP+0x30 and `sizeof(Camera)` is 0xd8 (runtime/types/graphics/
// Camera.h, static_assert'd there). The hidden return slot goes in RDX because
// GetSunCamera is a MEMBER function — `this` takes RCX and the visible
// `Renderer&` is pushed out to R8.
// ============================================================================
extern "C" void RtGraphics_SunLight_AddDrawItems(void* selfv, const void* scenev, void* rendererv) {
    ++g_sunAddDrawItems;

    uint8_t* self = B(selfv);
    uint8_t* renderer = B(rendererv);

    if (*reinterpret_cast<void**>(self + 0xf8) == nullptr) return;
    ++g_sunAddDrawEmit;

    using AddLightFn        = void (*)(void* renderer, const void* light);
    using GetSunCameraFn    = void* (*)(const void* self, void* retCamera, void* renderer);
    using BeginDrawGroupFn  = int  (*)(void* renderer, const void* renderBuffer,
                                       const void* camA, const void* camB,
                                       int a, unsigned int b);
    using SceneDrawItemsFn  = void (*)(const void* scene, void* renderer);
    using EndDrawGroupFn    = void (*)(void* renderer, int group);

    RT_EXPORT_ONCE(AddLightFn,
        "?AddLight@Renderer@Graphics@@QEAAXPEBVLight@2@@Z")(renderer, self);

    alignas(16) uint8_t cam[0xd8];
    RT_EXPORT_ONCE(GetSunCameraFn,
        "?GetSunCamera@SunLight@Graphics@@QEBA?AVCamera@2@AEAVRenderer@2@@Z")(self, cam, renderer);

    const void* sceneCam = *reinterpret_cast<uint8_t**>(renderer + 0x10) + 8;
    const int group = RT_EXPORT_ONCE(BeginDrawGroupFn,
        "?BeginDrawGroup@Renderer@Graphics@@QEAAHPEBVRenderBuffer@Video@@AEBVCamera@2@1HI@Z")(
            renderer, *reinterpret_cast<void**>(self + 0x100), sceneCam, cam, 1, 1u);

    if (I32(renderer, 0x3c) != 0) {
        RT_EXPORT_ONCE(SceneDrawItemsFn,
            "?AddGlobalDrawItems@Scene@Graphics@@AEBAXAEAVRenderer@2@@Z")(scenev, renderer);
        if (I32(renderer, 0x48) != 0)
            RT_EXPORT_ONCE(SceneDrawItemsFn,
                "?AddCulledDrawItems@Scene@Graphics@@AEBAXAEAVRenderer@2@@Z")(scenev, renderer);
    }

    RT_EXPORT_ONCE(EndDrawGroupFn,
        "?EndDrawGroup@Renderer@Graphics@@QEAAXH@Z")(renderer, group);
}

// ============================================================================
// Registration
// ============================================================================
extern "C" int RtGraphicsWave28PartsysRegister() {
    int count = 0;
    RegisterRuntimeExport("?Update@ParticleSystemInstance@Graphics@@UEAAXAEBVTime@System@@@Z",
                          (void*)&RtGraphics_ParticleSystemInstance_Update); ++count;
    RegisterRuntimeExport("?RemoveState@ParticleInstance@Graphics@@IEAAXXZ",
                          (void*)&RtGraphics_ParticleInstance_RemoveState); ++count;
    RegisterRuntimeExport("?Update@SunLight@Graphics@@UEAAXAEBVTime@System@@@Z",
                          (void*)&RtGraphics_SunLight_Update); ++count;
    RegisterRuntimeExport("?Draw@SunLight@Graphics@@UEBAXAEAVContext@Video@@AEBVCamera@2@"
                          "V?$ExternalPtr@VMaterialInstance@Graphics@@@Object@@MHHH@Z",
                          (void*)&RtGraphics_SunLight_Draw_MHHH); ++count;
    RegisterRuntimeExport("?AddDrawItems@SunLight@Graphics@@UEBAXAEBVScene@2@AEAVRenderer@2@@Z",
                          (void*)&RtGraphics_SunLight_AddDrawItems); ++count;
    return count;
}

void RtGraphicsWave28PartsysSummary() {
    printf("[PARTSYS28] PSI::Update entries=%llu earlyOut=%llu spawned=%llu aged=%llu killed=%llu\n",
                (unsigned long long)g_psiUpdate, (unsigned long long)g_psiUpdateEarly,
                (unsigned long long)g_psiSpawned, (unsigned long long)g_psiAged,
                (unsigned long long)g_psiKilled);
    printf("[PARTSYS28] PI::RemoveState calls=%llu freedPool=%llu | "
                "SunLight Update=%llu Draw(MHHH)=%llu AddDrawItems=%llu emitted=%llu\n",
                (unsigned long long)g_piRemoveState, (unsigned long long)g_piRemoveStateFree,
                (unsigned long long)g_sunUpdate, (unsigned long long)g_sunDrawMhhh,
                (unsigned long long)g_sunAddDrawItems, (unsigned long long)g_sunAddDrawEmit);
}
