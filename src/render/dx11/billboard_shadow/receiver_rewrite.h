#pragma once
// ============================================================================
// src/render/dx11/billboard_shadow/receiver_rewrite.h -- ext.billboard_shadow's vertex-shader
// rewriter (docs/extensions/TREE_SHADOW.md, handins/TS-CORE.md "TS-FIX2")
// ============================================================================
// NOT RETAIL. Pure: no D3D, no retail bytes (the unit tests build synthetic vs_4_0 blobs). It reads
// the blob it is handed -- the user's installed vertex shaders, at run time -- and emits a copy in
// memory.
//
// THE PROBLEM IT SOLVES. The billboard canopy's colour vertex shader builds each leaf card facing the
// PLAYER camera (from `gc.worldToCamera`, cb0 rows 4..7) and looks the sun shadow up at that drawn
// position P: `shadowProjection = lc.shadowProjection[0] * P` (cb2 rows 10..13). The shadow pass drew
// the same card facing the SUN, so P's depth along the light is unrelated to where the card's own
// caster is: half of every card shadows itself and the canopy shows a straight terminator that turns
// with the camera.
//
// THE MATCH (`Analyse`): a vs_4_x program with no flow control (`ret` once and last), no indexable
// temps, whose SHADOW OUTPUT (the output written from cb2[10..13]) depends on cb0[4..7] through one
// position temp P that the shadow-matrix instructions read with replicated swizzles, where the first
// camera-dependent write of P's xyz is `add P, C, offset` with exactly one camera-independent temp
// source C: the card CENTRE (retail: the instance-transformed vertex position, shared by the card's
// four corners, before the sway and corner offsets are added).
//
// THE LOOKUP (TS-FIX2, `Transform`): keep P's light-space position (the texel retail samples) and use
// a depth that no card's own caster can be in front of:
//     P' = P + (dot(C - P, L) - k |C - P|) L,      L = lc.nearPlane.xyz (the sun direction), k = 1
// so depth(P') = depth(C) - k |C - P|: the card centre's depth moved toward the light by the card's
// own half extent (the vertex's distance from its centre: the corner offset times the card scale, the
// card's half-diagonal at a corner). The card the shadow pass drew faces the sun up to the card code's
// tilt (each axis leans toward the view row by at most 0.5 / sqrt(1.25) = 0.447), so its own depth
// stays within 0.447 (|w.x s.x| + |w.y s.z|) <= 0.632 |C - P| of the centre's: with k = 1 a card is
// never shadowed by itself at any heading, only by casters in front of its centre by more than its
// half extent (the outer cards of the same crown, the neighbouring trees). The light-space
// projection is orthographic (cb2[10..13]'s w row is 0 0 0 1), so moving along L changes depth
// linearly and leaves the texel alone. Emitted as `mov rS.xyz, C` before that add and a 6-instruction
// block before the first shadow-matrix instruction; the three shadow-matrix reads of P are pointed at
// P'. `dcl_temps` + 2; every other token is the user's own.
// ============================================================================
#include "foliage_aa/dxbc.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace BbsRewrite {

constexpr uint32_t kGlobalCb      = 0;    // gc
constexpr uint32_t kWorldToCamera = 4;    // gc.worldToCamera rows 4..7
constexpr uint32_t kLightCb       = 2;    // lc
constexpr uint32_t kNearPlane     = 3;    // lc.nearPlane: xyz = the sun direction (SunLight::DrawLight)
constexpr uint32_t kShadowProj0   = 10;   // lc.shadowProjection[0] rows 10..13
constexpr float    kMargin        = 1.0f; // k above

struct Match {
    bool        container = false;   // parsed as DXBC with an SHDR / SHEX chunk
    bool        program   = false;   // its token stream parsed
    bool        vertex    = false;   // program type 1
    bool        candidate = false;   // reads cb2[10..13] AND cb0[4..7] somewhere (worth a log line when refused)
    bool        matched   = false;   // everything below is valid
    std::string why;                 // the refusal (matched == false)

    Dxbc::Container cont;
    Dxbc::Program   prog;
    uint32_t        temps         = 0;   // dcl_temps
    uint32_t        cb2Rows       = 0;   // dcl_constantbuffer cb2[n]
    uint32_t        shadowOut     = 0;   // the output register (o#)
    uint32_t        posReg        = 0;   // P: the temp the shadow-matrix instructions read
    int             insertAt      = -1;  // instruction index of the first shadow-matrix instruction
    int             centerIdx     = -1;  // instruction index of `add P, C, offset`
    int             centerOperand = 0;   // which of its operands is C (1 or 2)
};

// Parse and decide. Never throws, never touches the blob.
void Analyse(const uint8_t* blob, size_t size, Match* m);

// The transformed blob (fresh checksum). False when `m` did not match or the emitted copy fails its
// own re-parse (then nothing is used).
bool Transform(const Match& m, std::vector<uint8_t>* out);

}  // namespace BbsRewrite
