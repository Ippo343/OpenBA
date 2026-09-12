// ============================================================================
// runtime/src/audio/wave29_lane_avbody.cpp — WAVE 29 lane AVBODY
//
// Three Audio bodies, all exported, all name doors:
//   ?GetVolumeForPosition@Scene@Audio@@AEBAMAEBVVector3@Math@@MM0@Z  0x1800dfed0  ord 3027
//   ?GetAttenuatedVolume@Scene@Audio@@AEBAMAEBUAttenutedEffect@12@AEBVVector3@Math@@M@Z
//                                                                   0x1800dfde0  ord 2453
//   ?OnSoundEnded@Scene@Audio@@SAXPEAUAudioChannel@Device@2@PEAXI@Z  0x1800dffe0  ord 3283
//
// `?PlayPlatform@Device@Audio@@` is Audio's remaining BODY row and is NOT
// ported. There is no audio_play_platform.cpp -- do not go looking for one.
// It is decoded in full in docs/runtime_port/WAVE29_LANE_AVBODY.md §6.1, and
// declined there as a stated size-and-evidence judgement (632 bytes, six
// indirect XAudio2 calls, effects that live in the driver), NOT dressed up as a
// blocker. §6.2 sketches the fake-backend oracle that would make it verifiable
// and names an addressee for it.
//
// ============================================================================
// GetVolumeForPosition WAS NOT ON THE WORK LIST, AND FINDING IT IS THE POINT
// ============================================================================
// The census lists THREE unported Audio bodies. This file ports FOUR things,
// and the extra one is the helper `GetAttenuatedVolume` calls twice.
//
// I was one keystroke from writing "0x1800dfed0 is an unexported internal, so
// it is inlined here as a file-local static". That would have been WRONG, and
// wrong in the exact documented way: **AEBA means PRIVATE, not unexported, and
// this DLL exports its private members** (reference_aeaa_is_private_not_
// unexported.md, which records three silent address doors that survived review
// on precisely that confusion). Checked instead of assumed, with a pefile
// lookup of the VA against the export directory:
//
//   0x1800dfed0  EXPORTED  ord 3027  ?GetVolumeForPosition@Scene@Audio@@AEBAMAEBVVector3@Math@@MM0@Z
//   0x1800a0d10  EXPORTED  ord 3417  ?Play@Stream@Audio@@QEAAXHH@Z
//   0x1801fe728  EXPORTED  ord 4797  ?_secondsPerTick@Timer@System@@0NA
//
// So this lane's whole subject has **ZERO address doors**: every callee is a
// name door, and nothing here opens a new cutover door.
//
// And the mangled signature then CONFIRMS the decode from the writer's side
// rather than the reader's. `AEBA M` = private const member returning float;
// `AEBVVector3@Math@@` = const Vector3&; `M M` = two floats; `0` is MSVC's
// back-reference to argument type 1, i.e. a second const Vector3&. That is
// exactly (srcPos, maxDist, minDist, listenerPos) -> float, which is what the
// instruction stream says. Only a WRITER names a field
// (reference_only_a_writer_names_a_field.md) and the export directory is the
// closest thing to a writer available here.
//
// ============================================================================
// `AttenutedEffect` IS THE STRUCT Audio.h CALLS `EffectSlot`
// ============================================================================
// Retail's own name for it -- typo and all, "Attenuted" -- is readable in the
// export directory: `AEBUAttenutedEffect@12@`, where `12` back-references
// `Scene@Audio`. So the type is `Audio::Scene::AttenutedEffect`, and
// runtime/types/audio/Audio.h's `EffectSlot` is an earlier wave's inferred name
// for the same struct. Five independent offsets agree, which is what licenses
// reusing the existing type rather than declaring a second one:
//
//   +0x00  Vector3 pos      <- helper reads [RDX+0], [RDX+4], [RDX+8]
//   +0x10  SoundEffect ref  <- `MOV RAX,[RDX+0x10]`
//   +0x28  baseVolume       <- `MULSS XMM6,[RBP+0x28]`
//   +0x30  positions._data   <- `MOV RBX,[RDX+0x30]`
//   +0x38  positions._size   <- `MOV EAX,[RDX+0x38]`
//
// The name is recorded here and NOT renamed in Audio.h: renaming a type six
// files depend on is not this lane's change to make. ADDRESSEE: the next lane
// that edits runtime/types/audio/Audio.h should rename `EffectSlot` to
// `AttenutedEffect` and cite this paragraph.
//
// TWO NEW SoundEffect FIELDS, and they are READER-ONLY evidence. +0x20 and
// +0x24 are two floats this body loads as the falloff radii. No writer was
// found, so they are named for their ROLE in the arithmetic (maxDistanceBase,
// minDistance) and that naming is explicitly weaker than the offsets
// themselves. A later lane that finds the writer should correct them.
//
// ============================================================================
// THE MOVED-TERM TRAP, WHICH IS LIVE IN GetAttenuatedVolume
// ============================================================================
// A moved term in a float sum is algebraically identical and numerically
// different (reference_moved_term_hides_where_terms_cancel.md). Two terms in
// this body are hoisted in retail and MUST STAY HOISTED:
//
//   1. `maxDist = fRadius + effect->def->maxDistanceBase` is computed ONCE,
//      before the loop (`ADDSS XMM7,[RAX+0x20]` at 0x1800dfe13, outside the
//      loop that starts at 0x1800dfe60). Recomputing it per iteration is the
//      same value every time here -- but writing it as `Falloff(..., fRadius +
//      base, ...)` inside the call invites a later edit that reassociates it.
//
//   2. `* effect->baseVolume` is applied ONCE, AFTER the max-reduction
//      (`MULSS XMM6,[RBP+0x28]` at 0x1800dfea8, after the loop exits). This one
//      is NOT merely a rounding question: multiplying each candidate by
//      baseVolume inside the loop and then taking the max REORDERS THE RESULT
//      whenever baseVolume is negative, and changes the number of roundings
//      even when it is positive. max(a,b)*v != max(a*v,b*v) for v<0.
//
// NaN: the reduction is `COMISS XMM0,XMM6 / JBE skip`, and COMISS sets
// ZF=CF=PF=1 for an unordered compare, so JBE is TAKEN and a NaN candidate is
// DISCARDED. C's `if (cand > best)` is false for NaN, so the plain spelling is
// faithful; it is spelled that way deliberately, not by luck.
//
// The count<0 case: `TEST EAX,EAX / JNZ / JLE` skips the loop entirely and
// leaves best = 0.0f -- which is then STILL multiplied by baseVolume. An early
// `return 0.0f` would be wrong: 0.0f * baseVolume is not 0.0f when baseVolume
// is NaN or an infinity. The multiply is kept unconditional.
//
// NO TRANSCENDENTALS beyond sqrtf. The CRT-coefficient rule
// (reference_crt_coefficients_must_be_lifted_not_retyped.md) does not bite:
// IEEE-754 specifies sqrt as correctly rounded, so our libm and the DLL's agree
// bit-for-bit on every input. That is why this is the one transcendental the
// sweep can compare without BANISHED_RT_LIBM=dll.
//
// ONE CONSTANT, LIFTED not retyped: 0x18016169c = 0x3f800000 = 1.0f, read with
// read_memory rather than recognised. It is written below as the bit pattern's
// value with the pattern beside it.
//
// ============================================================================
// OnSoundEnded WRITES ONE BYTE OUTSIDE THE ARRAY IT INDEXES, IN RETAIL
// ============================================================================
// This is the finding of this file and it is NOT a port defect. Retail:
//
//   1800e0008: OR  R8D,0xffffffff      ; R8D = -1
//   ...                                 ; the 8-slot search runs, finds nothing
//   1800e0028: MOVSXD RAX,R8D          ; RAX = -1, SIGN-extended
//   1800e002b: OR  byte [RAX + R11*1 + 0x60],0x2
//
// so the not-found path ORs 2 into `channel + 0x60 - 1` = **channel + 0x5f**.
// runtime/types/audio/Audio.h (derived in an earlier wave, from different
// functions) puts `platformChannel[8]` at +0x20..+0x5f and `channelFlags[8]` at
// +0x60..+0x67. So +0x5f is not padding: it is the MOST SIGNIFICANT BYTE of
// `platformChannel[7]`. On the not-found path retail sets bit 1 of the top byte
// of an interface pointer.
//
// Per CLAUDE.md this is TRANSCRIBED, not "fixed": a guard would skip the
// behaviour rather than restore it, and the write is what retail does. It is
// called out here because a reader of the C would otherwise assume an
// off-by-one in the PORT. The found path writes channelFlags[index] normally.
//
// WRITE WIDTH: `OR byte ptr`, a BYTE, on both paths -- and, CORRECTING WHAT
// THIS COMMENT FIRST SAID, the Camera::operator= width trap (kickoff §2.2) does
// NOT apply here. I asserted it did, built a control to prove it, and the
// control reported NOT CAUGHT. It was right to: `OR` with the constant 2
// touches only bit 1 of the LOWEST byte at ANY width, because the constant's
// upper bytes are zero, so the byte and dword spellings produce byte-identical
// memory. The width trap bites on a STORE -- the Video pair's 8-byte
// `MOV [RDX],R9` beside its 4-byte `INC dword [RCX-4]` -- not on a
// read-modify-write with a small constant. See Ctl_WrongFlagBit below.
//
// THE TAG CHECK. `(storedTag ^ tag) & 0xfffffe00` must be zero, i.e. bits 9..31
// must match while bits 0..8 are allowed to differ -- and bits 1..8 are exactly
// what `(tag>>1) & 0xff` used to pick the channel. So the high bits are a
// generation counter guarding against a stale handle, and bit 0 is not read by
// this body at all. Stated as what the arithmetic does; this body is a READER
// of the tag encoding and cannot name its fields on its own.
//
// ============================================================================
// VERIFICATION
// ============================================================================
// GetVolumeForPosition and GetAttenuatedVolume are PURE -- `this` is passed but
// NEVER READ by either (RCX is dead in the helper, and in the outer body it is
// only forwarded). So both verify by INPUT SWEEP against the shipped export,
// with no gameplay route and no caller
// (project_pure_functions_verify_by_sweep.md, the method that verified
// GetAlignedPosition after it reported calls=0 in a 16.9M-call run).
//
// OnSoundEnded is not pure, but it is fully synthesisable: it reads a Scene and
// a Channel table and writes one byte. The sweep builds both, runs ours and
// retail's on SEPARATE BUT IDENTICAL copies, and memcmps the ENTIRE channel
// table afterwards -- which is what makes the +0x5f write a COMPARED term
// rather than something the oracle would have to be told to look for.
// "No gameplay route" is not "no oracle" (reference_no_route_is_not_no_oracle.md).
//
// Every retail side goes through ResolveExportRaw, never ResolveExport: the
// latter consults the port registry first and would hand back OUR function once
// registration lands, making the oracle a self-check that agrees forever
// (reference_shadow_marker_is_not_an_oracle.md).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../funcs/rt_libm.h"
#include "../../types/audio/Audio.h"

namespace rtavbody29 {

using RtAudio::EffectSlot;      // retail's own name: Audio::Scene::AttenutedEffect
using RtAudio::EmitterInstance;
using RtAudio::Channel;
using RtAudio::Scene;

namespace {

// ---- attendance ------------------------------------------------------------
unsigned long long g_volForPosCalls  = 0;
unsigned long long g_volForPosFar    = 0;  // BRANCH: dist > maxDist -> 0.0f
unsigned long long g_volForPosNear   = 0;  // BRANCH: dist < minDist -> 1.0f
unsigned long long g_attenCalls      = 0;
unsigned long long g_attenSingle     = 0;  // BRANCH: count == 0, the no-emitter path
unsigned long long g_attenLoops      = 0;  // BRANCH: count > 0, iterations run
unsigned long long g_endedCalls      = 0;
unsigned long long g_endedFound      = 0;  // BRANCH: the sub-slot was found
unsigned long long g_endedNotFound   = 0;  // BRANCH: the +0x5f write fired
unsigned long long g_endedRejected   = 0;  // BRANCH: empty table or stale tag

// The falloff ceiling, LIFTED from .rdata at 0x18016169c and written as the bit
// pattern's value. Not "recognised as 1.0f" -- recognising an algorithm is what
// licenses you to stop reading the bytes.
inline float FalloffCeiling() {
    const uint32_t bits = 0x3f800000u;  // 1.0f
    float f;
    std::memcpy(&f, &bits, sizeof(f));
    return f;
}

// The first 12 bytes of a Math::Vector3, which is all either body reads.
struct Vec3 { float x, y, z; };

}  // namespace

// ---------------------------------------------------------------------------
// ?GetVolumeForPosition@Scene@Audio@@AEBAMAEBVVector3@Math@@MM0@Z   0x1800dfed0
//
//   RCX = this (const Scene*), NEVER READ -- the body is pure.
//   RDX = const Vector3& source position
//   XMM2 = maxDistance, XMM3 = minDistance
//   [RSP+0x28 @entry] = const Vector3& listener position
// ---------------------------------------------------------------------------
extern "C" float RtAudio_Scene_GetVolumeForPosition(const void* /*self*/,
                                                    const void* srcPos,
                                                    float maxDist,
                                                    float minDist,
                                                    const void* listenerPos) {
    ++g_volForPosCalls;
    const Vec3* const s = static_cast<const Vec3*>(srcPos);
    const Vec3* const l = static_cast<const Vec3*>(listenerPos);

    // SUBSS/MULSS/ADDSS in retail's order: (dy*dy + dx*dx) + dz*dz.
    // The ORDER IS TRANSCRIBED, not tidied into x+y+z: float addition is not
    // associative, and this is exactly the moved-term hazard. Retail computes
    // dy^2 first (0x1800dfedd loads [RDX+4] before [RDX+0]), adds dx^2 to it
    // (`ADDSS XMM0,XMM4` at 0x1800dff0d), then adds dz^2 (0x1800dff15).
    const float dy = s->y - l->y;
    const float dx = s->x - l->x;
    const float dz = s->z - l->z;
    const float d2 = (dy * dy + dx * dx) + dz * dz;

    const float dist = RtLibm::Sqrt(d2);        // CALL 0x180128110 (sqrtf)

    // COMISS XMM1,XMM7 / JBE -- so this is `dist > maxDist`, and an UNORDERED
    // compare (a NaN distance) takes the JBE and does NOT return 0 here.
    if (dist > maxDist) { ++g_volForPosFar; return 0.0f; }

    // COMISS XMM6,XMM1 / JA -- `minDist > dist` returns the ceiling untouched.
    const float ceiling = FalloffCeiling();
    if (minDist > dist) { ++g_volForPosNear; return ceiling; }

    // SUBSS/SUBSS/DIVSS/SUBSS, in retail's order and with retail's operands.
    return ceiling - ((dist - minDist) / (maxDist - minDist));
}

// ---------------------------------------------------------------------------
// ?GetAttenuatedVolume@Scene@Audio@@AEBAM...   0x1800dfde0
//
//   RCX = this, RDX = const AttenutedEffect&, R8 = const Vector3& listener,
//   XMM3 = radius bias (added to the effect's max-distance base)
// ---------------------------------------------------------------------------
extern "C" float RtAudio_Scene_GetAttenuatedVolume(const void* self,
                                                   const void* effectPtr,
                                                   const void* listenerPos,
                                                   float radiusBias) {
    ++g_attenCalls;
    const EffectSlot* const eff = static_cast<const EffectSlot*>(effectPtr);

    // MOV RAX,[RDX+0x10] -- the SoundEffect. Unguarded in retail; transcribed.
    const uint8_t* const def =
        *reinterpret_cast<const uint8_t* const*>(
            reinterpret_cast<const uint8_t*>(eff) + 0x10);

    float f20, f24;
    std::memcpy(&f20, def + 0x20, sizeof(f20));   // maxDistanceBase (reader-only name)
    std::memcpy(&f24, def + 0x24, sizeof(f24));   // minDistance     (reader-only name)

    // BOTH HOISTED, exactly as retail hoists them. See the header.
    const float minDist = f24;                    // MOVSS XMM8,[RAX+0x24]
    const float maxDist = radiusBias + f20;       // ADDSS XMM7,[RAX+0x20]

    float best = 0.0f;                            // XORPS XMM6,XMM6

    const int32_t count = static_cast<int32_t>(eff->positions._size);
    if (count == 0) {
        // TEST EAX,EAX / JNZ not taken: the effect's OWN position is used.
        ++g_attenSingle;
        best = RtAudio_Scene_GetVolumeForPosition(self, eff, maxDist, minDist,
                                                  listenerPos);
    } else if (count > 0) {
        // JLE after the JNZ: a NEGATIVE count skips the loop and leaves best=0.
        const EmitterInstance* e = eff->positions._data;
        for (int32_t i = 0; i < count; ++i, ++e) {
            ++g_attenLoops;
            // MULSS XMM0,[RDI] where RDI = element + 0xc -- the per-emitter
            // volume, applied to THIS candidate before the comparison.
            const float v =
                RtAudio_Scene_GetVolumeForPosition(self, e, maxDist, minDist,
                                                   listenerPos) * e->volume;
            // COMISS/JBE: strictly greater, and a NaN candidate is discarded.
            if (v > best) best = v;
        }
    }

    // MULSS XMM6,[RBP+0x28] -- ONCE, AFTER the reduction, and UNCONDITIONAL.
    return best * eff->baseVolume;
}

// ---------------------------------------------------------------------------
// ?OnSoundEnded@Scene@Audio@@SAXPEAUAudioChannel@Device@2@PEAXI@Z  0x1800dffe0
//
//   STATIC (SA): no `this`. RCX = Device::AudioChannel*, RDX = void* userData
//   (the Scene, as stored into the device slot by PlayPlatform's caller),
//   R8D = the channel handle tag.
// ---------------------------------------------------------------------------
extern "C" void RtAudio_Scene_OnSoundEnded(void* audioChannel, void* userData,
                                           uint32_t tag) {
    ++g_endedCalls;
    Scene* const scene = static_cast<Scene*>(userData);

    // CMP dword [RDX+0x18],0 / JZ -- Scene::channels._size
    if (scene->channels._size == 0) { ++g_endedRejected; return; }

    // MOV EAX,R8D / SHR EAX,1 / MOVZX R11D,AL -- a BYTE, so the index is
    // (tag>>1)&0xff and nothing wider. IMUL by 0x70 = sizeof(Channel).
    const uint32_t index = static_cast<uint8_t>(tag >> 1);
    uint8_t* const ch =
        reinterpret_cast<uint8_t*>(scene->channels._data) +
        static_cast<uint64_t>(index) * 0x70u;

    // MOV EAX,[R11+0x68] / XOR EAX,R8D / TEST EAX,0xfffffe00 / JNZ
    uint32_t storedTag;
    std::memcpy(&storedTag, ch + 0x68, sizeof(storedTag));
    if (((storedTag ^ tag) & 0xfffffe00u) != 0) { ++g_endedRejected; return; }

    // The 8-slot linear search over platformChannel[], +0x20 stride 8.
    // `CMP RCX,0x8 / JL` -- eight iterations, indices 0..7.
    int32_t found = -1;                                  // OR R8D,0xffffffff
    for (int32_t i = 0; i < 8; ++i) {
        void* slot;
        std::memcpy(&slot, ch + 0x20 + static_cast<size_t>(i) * 8, sizeof(slot));
        if (slot == audioChannel) { found = i; break; }  // JZ found
    }

    if (found >= 0) ++g_endedFound; else ++g_endedNotFound;

    // MOVSXD RAX,<found> / OR byte [RAX + R11 + 0x60],2
    //
    // *** found == -1 GIVES channel+0x5f, ONE BYTE BEFORE channelFlags[0], and
    // *** that is the MSB of platformChannel[7]. See the header. RETAIL DOES
    // *** THIS; it is transcribed, not guarded. The `int64_t` cast is the
    // *** MOVSXD -- an unsigned index here would address +0x60+0xFFFFFFFF.
    uint8_t* const flagByte = ch + 0x60 + static_cast<int64_t>(found);
    *flagByte = static_cast<uint8_t>(*flagByte | 0x02u);   // OR byte, NOT dword
}

// ============================================================================
// THE SWEEPS
// ============================================================================

namespace {

using VolForPosFn = float (*)(const void*, const void*, float, float, const void*);
using AttenFn     = float (*)(const void*, const void*, const void*, float);
using EndedFn     = void  (*)(void*, void*, uint32_t);

// Bit-exact float comparison, and it must be bit-exact: the whole point is to
// catch a reassociated sum, which differs in the last bit or two and which an
// epsilon compare would wave through. NaN==NaN must count as EQUAL here, so the
// comparison is on the BIT PATTERN, not on `==`.
inline bool SameFloat(float a, float b) {
    uint32_t x, y;
    std::memcpy(&x, &a, 4);
    std::memcpy(&y, &b, 4);
    return x == y;
}

// ---- GetVolumeForPosition --------------------------------------------------
int SweepVolForPosition(int& compared) {
    VolForPosFn theirs = reinterpret_cast<VolForPosFn>(ResolveExportRaw(
        "?GetVolumeForPosition@Scene@Audio@@AEBAMAEBVVector3@Math@@MM0@Z"));
    if (!theirs) {
        printf("[AVBODY29]   GetVolumeForPosition: SKIPPED -- "
               "ResolveExportRaw returned null\n");
        return -1;
    }

    struct C { Vec3 src; Vec3 lis; float maxD; float minD; const char* what; };
    static const C kCases[] = {
        {{0,0,0},    {0,0,0},     10.f, 1.f, "coincident -> inside minDist"},
        {{5,0,0},    {0,0,0},     10.f, 1.f, "mid-range, the lerp branch"},
        {{50,0,0},   {0,0,0},     10.f, 1.f, "beyond maxDist -> 0"},
        {{1,0,0},    {0,0,0},     10.f, 1.f, "exactly at minDist boundary"},
        {{10,0,0},   {0,0,0},     10.f, 1.f, "exactly at maxDist boundary"},
        {{3,4,0},    {0,0,0},     10.f, 1.f, "3-4-5 triangle, exact sqrt"},
        {{1,2,3},    {-4,-5,-6},  20.f, 2.f, "all three axes nonzero"},
        {{-7,11,-13},{2,-3,5},    40.f, 0.f, "minDist 0 -> divide by maxDist"},
        // minDist == maxDist makes the denominator ZERO: retail divides anyway.
        {{5,0,0},    {0,0,0},      5.f, 5.f, "minDist==maxDist, /0 path"},
        // Reassociation bait: terms that cancel differently per grouping.
        {{1e20f,1.f,-1e20f}, {0,0,0}, 1e30f, 0.f, "catastrophic cancellation"},
        {{1e-20f,1e20f,1e-20f},{0,0,0},1e30f,0.f,"mixed magnitudes"},
    };

    int diverged = 0;
    for (const C& c : kCases) {
        const float a = RtAudio_Scene_GetVolumeForPosition(nullptr, &c.src,
                                                           c.maxD, c.minD, &c.lis);
        const float b = theirs(nullptr, &c.src, c.maxD, c.minD, &c.lis);
        ++compared;
        if (!SameFloat(a, b)) {
            ++diverged;
            printf("[AVBODY29]   GetVolumeForPosition DIVERGED on \"%s\": "
                   "ours=%.9g theirs=%.9g\n", c.what, (double)a, (double)b);
        }
    }
    return diverged;
}

// ---- GetAttenuatedVolume ---------------------------------------------------
struct AttenFixture {
    uint8_t         def[0x40];       // the SoundEffect; only +0x20/+0x24 read
    EmitterInstance emitters[6];
    EffectSlot      slot;
};

void BuildAtten(AttenFixture& f, int32_t count, float maxBase, float minD,
                float baseVolume) {
    std::memset(&f, 0xCD, sizeof(f));   // poison, so an unread field is not 0
    std::memcpy(f.def + 0x20, &maxBase, 4);
    std::memcpy(f.def + 0x24, &minD, 4);
    for (int i = 0; i < 6; ++i) {
        f.emitters[i].x = (float)(i * 3 - 7);
        f.emitters[i].y = (float)(i * -2 + 1);
        f.emitters[i].z = (float)(i + 0.5f);
        f.emitters[i].volume = 0.25f * (float)(i + 1);
    }
    std::memset(&f.slot, 0, sizeof(f.slot));
    f.slot.posX = 2.f; f.slot.posY = -3.f; f.slot.posZ = 4.f;
    const uint8_t* defp = f.def;
    std::memcpy(reinterpret_cast<uint8_t*>(&f.slot) + 0x10, &defp, sizeof(defp));
    f.slot.baseVolume     = baseVolume;
    f.slot.positions._data = f.emitters;
    f.slot.positions._size = count;
    f.slot.positions._capacity = 6;
}

int SweepAttenuated(int& compared) {
    AttenFn theirs = reinterpret_cast<AttenFn>(ResolveExportRaw(
        "?GetAttenuatedVolume@Scene@Audio@@AEBAMAEBUAttenutedEffect@12@"
        "AEBVVector3@Math@@M@Z"));
    if (!theirs) {
        printf("[AVBODY29]   GetAttenuatedVolume: SKIPPED -- "
               "ResolveExportRaw returned null\n");
        return -1;
    }

    struct C { int32_t count; float maxBase, minD, vol, bias; const char* what; };
    static const C kCases[] = {
        { 0, 20.f, 2.f,  1.0f,  0.f, "count==0, the effect's own position"},
        { 0, 20.f, 2.f,  0.5f,  5.f, "count==0, nonzero radius bias"},
        { 1, 20.f, 2.f,  1.0f,  0.f, "one emitter"},
        { 6, 20.f, 2.f,  1.0f,  0.f, "six emitters, max-reduction"},
        { 6, 20.f, 2.f,  0.3f,  8.f, "six emitters, bias + baseVolume"},
        // baseVolume NEGATIVE: this is the case that separates
        // max(a,b)*v from max(a*v,b*v). If the multiply were moved into the
        // loop, THIS case diverges and the others do not.
        { 6, 20.f, 2.f, -1.0f,  0.f, "NEGATIVE baseVolume -- the moved-term case"},
        { 6, 20.f, 2.f, -0.75f, 3.f, "negative baseVolume + bias"},
        // count<0: the loop is skipped and 0.0f is STILL multiplied.
        {-1, 20.f, 2.f,  1.0f,  0.f, "NEGATIVE count -> skip, 0*vol"},
        {-9, 20.f, 2.f,  2.5f,  0.f, "negative count, vol!=1"},
        // Everything out of range -> every candidate 0.
        { 6,  0.1f,0.f,  1.0f,  0.f, "all beyond maxDist -> 0"},
        // Everything inside minDist -> every candidate is the ceiling.
        { 6, 1e9f, 1e9f, 1.0f,  0.f, "all inside minDist -> ceiling*volume"},
    };

    static const Vec3 kListener = { 1.5f, -2.5f, 3.5f };

    int diverged = 0;
    for (const C& c : kCases) {
        AttenFixture fa, fb;
        BuildAtten(fa, c.count, c.maxBase, c.minD, c.vol);
        BuildAtten(fb, c.count, c.maxBase, c.minD, c.vol);
        const float a = RtAudio_Scene_GetAttenuatedVolume(nullptr, &fa.slot,
                                                          &kListener, c.bias);
        const float b = theirs(nullptr, &fb.slot, &kListener, c.bias);
        ++compared;
        if (!SameFloat(a, b)) {
            ++diverged;
            printf("[AVBODY29]   GetAttenuatedVolume DIVERGED on \"%s\": "
                   "ours=%.9g theirs=%.9g\n", c.what, (double)a, (double)b);
        }
    }
    return diverged;
}

// ---- OnSoundEnded ----------------------------------------------------------
// 4 channels of 0x70 bytes. The WHOLE table is memcmp'd afterwards, which is
// what makes retail's out-of-array +0x5f write a COMPARED term rather than
// something the screen would have to be told about in advance.
struct EndedFixture {
    uint8_t channels[4 * 0x70];
    Scene   scene;
};

void BuildEnded(EndedFixture& f, uint32_t storedTag, int occupantSlot,
                void* occupant) {
    std::memset(&f, 0, sizeof(f));
    // Give every platformChannel entry a distinct non-null value so a wrong
    // match is visible, and so the +0x5f byte has a known prior value.
    for (int c = 0; c < 4; ++c) {
        uint8_t* ch = f.channels + c * 0x70;
        for (int i = 0; i < 8; ++i) {
            void* v = reinterpret_cast<void*>(
                static_cast<uintptr_t>(0x1000 + c * 0x100 + i * 0x10));
            std::memcpy(ch + 0x20 + i * 8, &v, sizeof(v));
        }
        std::memcpy(ch + 0x68, &storedTag, sizeof(storedTag));
    }
    if (occupantSlot >= 0)
        std::memcpy(f.channels + 0x20 + occupantSlot * 8, &occupant, sizeof(occupant));
    std::memset(&f.scene, 0, sizeof(f.scene));
    f.scene.channels._data     = reinterpret_cast<Channel*>(f.channels);
    f.scene.channels._size     = 4;
    f.scene.channels._capacity = 4;
}

int SweepOnSoundEnded(int& compared) {
    EndedFn theirs = reinterpret_cast<EndedFn>(ResolveExportRaw(
        "?OnSoundEnded@Scene@Audio@@SAXPEAUAudioChannel@Device@2@PEAXI@Z"));
    if (!theirs) {
        printf("[AVBODY29]   OnSoundEnded: SKIPPED -- "
               "ResolveExportRaw returned null\n");
        return -1;
    }

    // A channel index of 0 needs tag bits 1..8 == 0. Channel c is tag = c<<1,
    // plus a generation in bits 9+ that must match the stored tag.
    struct C { uint32_t storedTag, tag; int occupant; bool emptyTable;
               const char* what; };
    static const C kCases[] = {
        { 0x00000200u, 0x00000200u, 3, false, "found at slot 3"},
        { 0x00000200u, 0x00000200u, 0, false, "found at slot 0"},
        { 0x00000200u, 0x00000200u, 7, false, "found at slot 7 (last)"},
        // THE +0x5f CASE: the occupant is not in the table at all.
        { 0x00000200u, 0x00000200u,-1, false, "NOT FOUND -> retail's +0x5f write"},
        // A different channel index, to prove (tag>>1)&0xff selects it.
        { 0x00000402u, 0x00000402u, 2, false, "channel 1, found at slot 2"},
        { 0x00000604u, 0x00000604u,-1, false, "channel 2, NOT FOUND"},
        { 0x00000206u, 0x00000206u, 5, false, "channel 3, found at slot 5"},
        // Bits 0..8 may differ; bits 9+ may not.
        { 0x00000200u, 0x00000201u, 3, false, "bit 0 differs -> still accepted"},
        { 0x00000200u, 0x00000400u,-1, false, "bit 9 differs -> REJECTED, no write"},
        { 0x00008200u, 0x00000200u, 3, false, "high generation differs -> REJECTED"},
        { 0x00000200u, 0x00000200u, 3, true,  "empty channel table -> early out"},
    };

    int diverged = 0;
    for (const C& c : kCases) {
        void* const occupant = reinterpret_cast<void*>(
            static_cast<uintptr_t>(0xABCDEF00u));
        EndedFixture fa, fb;
        BuildEnded(fa, c.storedTag, c.occupant, occupant);
        BuildEnded(fb, c.storedTag, c.occupant, occupant);
        if (c.emptyTable) { fa.scene.channels._size = 0; fb.scene.channels._size = 0; }

        RtAudio_Scene_OnSoundEnded(occupant, &fa.scene, c.tag);
        theirs(occupant, &fb.scene, c.tag);
        ++compared;

        // The ENTIRE channel table, so +0x5f and channelFlags are both covered.
        if (std::memcmp(fa.channels, fb.channels, sizeof(fa.channels)) != 0) {
            ++diverged;
            int firstDiff = -1;
            for (size_t i = 0; i < sizeof(fa.channels); ++i)
                if (fa.channels[i] != fb.channels[i]) { firstDiff = (int)i; break; }
            printf("[AVBODY29]   OnSoundEnded DIVERGED on \"%s\": first differing "
                   "byte at channelTable+%#x (channel %d, +%#x): ours=%#04x "
                   "theirs=%#04x\n", c.what, firstDiff, firstDiff / 0x70,
                   firstDiff % 0x70, fa.channels[firstDiff], fb.channels[firstDiff]);
        }
    }
    return diverged;
}

// ---- negative controls, scored SEPARATELY ----------------------------------
// Each targets ONE compared term. A control scored on the union of two screens
// reads 3/3 and hides that one screen caught 1/3
// (reference_union_control_hides_a_blind_screen.md).

float Ctl_MovedTerm(const void* self, const void* effectPtr,
                    const void* listenerPos, float radiusBias) {
    // Multiplies by baseVolume INSIDE the loop instead of once after the
    // reduction -- the moved-term defect. Targets: the returned float, and
    // ONLY on the negative-baseVolume cases.
    const EffectSlot* const eff = static_cast<const EffectSlot*>(effectPtr);
    const uint8_t* const def = *reinterpret_cast<const uint8_t* const*>(
        reinterpret_cast<const uint8_t*>(eff) + 0x10);
    float f20, f24;
    std::memcpy(&f20, def + 0x20, 4);
    std::memcpy(&f24, def + 0x24, 4);
    const float maxDist = radiusBias + f20, minDist = f24;
    float best = 0.0f;
    const int32_t count = static_cast<int32_t>(eff->positions._size);
    if (count == 0)
        best = RtAudio_Scene_GetVolumeForPosition(self, eff, maxDist, minDist,
                                                  listenerPos) * eff->baseVolume;
    else if (count > 0) {
        const EmitterInstance* e = eff->positions._data;
        for (int32_t i = 0; i < count; ++i, ++e) {
            const float v = RtAudio_Scene_GetVolumeForPosition(
                self, e, maxDist, minDist, listenerPos) * e->volume
                * eff->baseVolume;
            if (v > best) best = v;
        }
    }
    return best;
}

void Ctl_GuardTheOutOfArrayWrite(void* audioChannel, void* userData, uint32_t tag) {
    // "Fixes" retail's +0x5f write with the null check CLAUDE.md forbids.
    // Targets: the channel-table memcmp. This control exists to prove the
    // screen can SEE the +0x5f byte -- a screen looking for the presence of a
    // write must be handed a case where the write is absent
    // (reference_absence_screen_must_be_handed_a_shape.md).
    Scene* const scene = static_cast<Scene*>(userData);
    if (scene->channels._size == 0) return;
    const uint32_t index = static_cast<uint8_t>(tag >> 1);
    uint8_t* const ch = reinterpret_cast<uint8_t*>(scene->channels._data) +
                        static_cast<uint64_t>(index) * 0x70u;
    uint32_t storedTag;
    std::memcpy(&storedTag, ch + 0x68, sizeof(storedTag));
    if (((storedTag ^ tag) & 0xfffffe00u) != 0) return;
    int32_t found = -1;
    for (int32_t i = 0; i < 8; ++i) {
        void* slot;
        std::memcpy(&slot, ch + 0x20 + (size_t)i * 8, sizeof(slot));
        if (slot == audioChannel) { found = i; break; }
    }
    if (found < 0) return;                       // <-- the "fix"
    uint8_t* const fb = ch + 0x60 + found;
    *fb = (uint8_t)(*fb | 0x02u);
}

void Ctl_WrongFlagBit(void* audioChannel, void* userData, uint32_t tag) {
    // Sets bit 2 (0x04) instead of bit 1 (0x02) in the SAME flag byte.
    // Targets: the channel-table memcmp, specifically channelFlags[index].
    //
    // THIS REPLACED A CONTROL THAT WAS A NO-OP, and the replacement is the
    // point. The first version of this control wrote the flag as a DWORD
    // instead of a BYTE -- the Camera::operator= width defect -- and the sweep
    // reported it NOT CAUGHT. That looked like a blind screen. It was not:
    //
    //     byte-OR :  a5 a5 a7 a5 a5 a5 a5 a5
    //     dword-OR:  a5 a5 a7 a5 a5 a5 a5 a5      <- identical
    //
    // `OR` with the constant 2 touches ONLY bit 1 of the lowest byte, at ANY
    // width, because the constant's upper bytes are zero and OR with 0 is the
    // identity. So the two spellings produce byte-identical memory and the
    // control corrupted NOTHING. "NOT CAUGHT" indicted the CONTROL, not the
    // screen (reference_control_must_pick_a_compared_subject.md, in its purest
    // form: a control that changes nothing at all).
    //
    // AND THE REAL CONCLUSION, which is about the BODY and not the control:
    // **the write-width trap does NOT apply to this body.** It applies where
    // retail STORES (the Video pair's 8-byte `MOV [RDX],R9` beside its 4-byte
    // `INC dword [RCX-4]`), not where retail does a read-modify-write with a
    // small constant. A blanket "check every write width" would have recorded a
    // hazard here that cannot exist.
    //
    // Bit 2 IS observable, so this control reaches channelFlags[index] and
    // proves the screen is not blind to it -- which the surviving +0x5f control
    // does NOT prove, since that one only ever differs on the not-found path.
    Scene* const scene = static_cast<Scene*>(userData);
    if (scene->channels._size == 0) return;
    const uint32_t index = static_cast<uint8_t>(tag >> 1);
    uint8_t* const ch = reinterpret_cast<uint8_t*>(scene->channels._data) +
                        static_cast<uint64_t>(index) * 0x70u;
    uint32_t storedTag;
    std::memcpy(&storedTag, ch + 0x68, sizeof(storedTag));
    if (((storedTag ^ tag) & 0xfffffe00u) != 0) return;
    int32_t found = -1;
    for (int32_t i = 0; i < 8; ++i) {
        void* slot;
        std::memcpy(&slot, ch + 0x20 + (size_t)i * 8, sizeof(slot));
        if (slot == audioChannel) { found = i; break; }
    }
    uint8_t* const p = ch + 0x60 + (int64_t)found;
    *p = (uint8_t)(*p | 0x04u);                  // <-- bit 2, not bit 1
}

}  // namespace

// ---- the entry point -------------------------------------------------------
int Sweep() {
    printf("[AVBODY29] ==== Audio body sweep ====\n");
    printf("[AVBODY29] SUBJECT: 3 bodies (GetVolumeForPosition 0x1800dfed0, "
           "GetAttenuatedVolume 0x1800dfde0, OnSoundEnded 0x1800dffe0); "
           "ours vs ResolveExportRaw (SHIPPED body, port registry BYPASSED); "
           "compared terms = {float bit pattern} for the two pure bodies, "
           "{the whole 4x0x70 channel table, memcmp} for OnSoundEnded\n");

    int compared = 0;
    const int d1 = SweepVolForPosition(compared);
    const int d2 = SweepAttenuated(compared);
    const int d3 = SweepOnSoundEnded(compared);

    // ---- controls -----------------------------------------------------
    printf("[AVBODY29] negative controls (each targets ONE compared term, "
           "scored SEPARATELY):\n");

    // Control 1: the moved term, against the ATTENUATED screen.
    {
        AttenFn theirs = reinterpret_cast<AttenFn>(ResolveExportRaw(
            "?GetAttenuatedVolume@Scene@Audio@@AEBAMAEBUAttenutedEffect@12@"
            "AEBVVector3@Math@@M@Z"));
        static const Vec3 kListener = { 1.5f, -2.5f, 3.5f };
        int caught = 0, n = 0;
        if (theirs) {
            const float vols[] = { 1.0f, -1.0f, -0.75f, 0.3f };
            for (float v : vols) {
                AttenFixture fa, fb;
                BuildAtten(fa, 6, 20.f, 2.f, v);
                BuildAtten(fb, 6, 20.f, 2.f, v);
                const float a = Ctl_MovedTerm(nullptr, &fa.slot, &kListener, 0.f);
                const float b = theirs(nullptr, &fb.slot, &kListener, 0.f);
                ++n;
                if (!SameFloat(a, b)) ++caught;
            }
        }
        printf("[AVBODY29]   control \"baseVolume multiplied INSIDE the loop\" "
               "(term: returned float): %s (%d/%d cases; it SHOULD be invisible "
               "on positive baseVolume and visible on negative -- that split is "
               "the finding, not the count)\n",
               caught ? "CAUGHT" : "*** NOT CAUGHT -- SCREEN IS BLIND ***",
               caught, n);
    }

    // Controls 2 and 3: against the OnSoundEnded screen.
    {
        EndedFn theirs = reinterpret_cast<EndedFn>(ResolveExportRaw(
            "?OnSoundEnded@Scene@Audio@@SAXPEAUAudioChannel@Device@2@PEAXI@Z"));
        struct Ctl { const char* name; EndedFn fn; };
        const Ctl ctls[] = {
            { "guard away retail's +0x5f out-of-array write", &Ctl_GuardTheOutOfArrayWrite },
            { "set bit 2 instead of bit 1 in channelFlags",   &Ctl_WrongFlagBit },
        };
        for (const Ctl& c : ctls) {
            int caught = 0, n = 0;
            if (theirs) {
                // Hand it BOTH a found case and a not-found case, so neither
                // control can pass by never reaching its own defect.
                const int occupants[] = { 3, -1 };
                for (int occ : occupants) {
                    void* const occupant =
                        reinterpret_cast<void*>(static_cast<uintptr_t>(0xABCDEF00u));
                    EndedFixture fa, fb;
                    BuildEnded(fa, 0x200u, occ, occupant);
                    BuildEnded(fb, 0x200u, occ, occupant);
                    c.fn(occupant, &fa.scene, 0x200u);
                    theirs(occupant, &fb.scene, 0x200u);
                    ++n;
                    if (std::memcmp(fa.channels, fb.channels,
                                    sizeof(fa.channels)) != 0) ++caught;
                }
            }
            printf("[AVBODY29]   control \"%s\" (term: channel table): %s "
                   "(%d/%d cases)\n", c.name,
                   caught ? "CAUGHT" : "*** NOT CAUGHT -- SCREEN IS BLIND ***",
                   caught, n);
        }
    }

    printf("[AVBODY29] RESULT: compared=%d divergences: GetVolumeForPosition=%d "
           "GetAttenuatedVolume=%d OnSoundEnded=%d\n", compared, d1, d2, d3);
    fflush(stdout);
    return (d1 > 0 || d2 > 0 || d3 > 0) ? 1 : 0;
}

// ---- registration ----------------------------------------------------------
int Register() {
    int count = 0;
    RegisterRuntimeExport("?GetVolumeForPosition@Scene@Audio@@AEBAMAEBVVector3@Math@@MM0@Z",
                          (void*)&RtAudio_Scene_GetVolumeForPosition); ++count;
    RegisterRuntimeExport("?GetAttenuatedVolume@Scene@Audio@@AEBAMAEBUAttenutedEffect@12@"
                          "AEBVVector3@Math@@M@Z",
                          (void*)&RtAudio_Scene_GetAttenuatedVolume); ++count;
    RegisterRuntimeExport("?OnSoundEnded@Scene@Audio@@SAXPEAUAudioChannel@Device@2@PEAXI@Z",
                          (void*)&RtAudio_Scene_OnSoundEnded); ++count;
    return count;
}

// ---- attendance, printed UNCONDITIONALLY -----------------------------------
// See shader_program_names.cpp's matching note: Wave 28 defect #7 (every
// lane's *Summary() is written and nothing calls it) REPRODUCED on this lane's
// subject, so this goes through a static destructor -- the mechanism that
// actually produced Wave 28's one measured figure -- and prints even when every
// counter is zero, so "never ran" and "ran zero times" are distinguishable.
namespace {
struct AvbodyReport {
    ~AvbodyReport() {
        printf("[AVBODY29] Audio ATTENDANCE: "
               "GetVolumeForPosition=%llu (far=%llu near=%llu) ; "
               "GetAttenuatedVolume=%llu (single=%llu loopIters=%llu) ; "
               "OnSoundEnded=%llu (found=%llu notFound=%llu rejected=%llu)\n",
               g_volForPosCalls, g_volForPosFar, g_volForPosNear,
               g_attenCalls, g_attenSingle, g_attenLoops,
               g_endedCalls, g_endedFound, g_endedNotFound, g_endedRejected);
        fflush(stdout);
    }
};
AvbodyReport g_avbodyReport;
}  // namespace

}  // namespace rtavbody29

int RtAudioWave29AvbodyRegister() { return rtavbody29::Register(); }
int RtAudioWave29AvbodySweep()    { return rtavbody29::Sweep(); }
