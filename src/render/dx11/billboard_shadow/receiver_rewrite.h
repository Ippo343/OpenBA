#pragma once
// ============================================================================
// src/render/dx11/billboard_shadow/receiver_rewrite.h -- ext.billboard_shadow's vertex-shader
// rewriter (docs/extensions/TREE_SHADOW.md §4.2, handins/TS-CORE.md)
// ============================================================================
// NOT RETAIL. Pure: no D3D, no retail bytes (the unit tests build synthetic vs_4_0 blobs). It reads
// the blob it is handed -- the user's installed vertex shaders, at run time -- and emits a copy in
// memory.
//
// THE PROBLEM IT SOLVES. The billboard canopy's colour vertex shader builds each leaf card facing the
// PLAYER camera from `gc.worldToCamera` (cb0 rows 4..7) and uses that one position both for the clip
// position and for the shadow lookup `shadowProjection = lc.shadowProjection[0] * worldPosition`
// (cb2 rows 10..13). The shadow pass built the same card facing the SUN (its worldToCamera was the
// sun camera's), so every fragment looks up the shadow map at a point its caster never occupied: the
// half-card self-shadow and the straight terminator that turns with the camera.
//
// THE MATCH (`Analyse`): a vs_4_x program with no flow control (`ret` once and last), no indexable
// temps, whose SHADOW OUTPUT -- the output register written by an instruction reading cb2[10..13] --
// depends (through the temps, component by component) on cb0[4..6], with cb2 declared >= 10 rows and
// cb0 >= 8. Nothing else: a blob that does not match is left alone.
//
// THE TRANSFORM (`Transform`): a second copy of the shadow output's dataflow slice runs in a second
// set of temps (r[N] -> r[N + T]) with cb0[4..7] read from cb2[6..9] instead -- the light block's
// `textureProjection`, which no retail shader reads and which ext.billboard_shadow fills with the
// sun camera's exact worldToCamera (Graphics::SunLight::DrawLight). At the first slice instruction
// that depends on cb0[4..7] the second set is seeded from the first (T `mov`s); from there every
// slice instruction is cloned beside the original, and the instructions that write the shadow output
// are REPLACED by their clones. The clip position, lighting, fog and texcoords keep the retail
// values. The clone yields T, the card's vertex rebuilt sun-facing exactly where the caster put it.
//
// THE PROJECTION (TS-FIX, handins/TS-CORE.md "TS-FIX"). T itself is NOT the lookup point: the twin
// of the viewer-facing offset (a, b) along the sun card's own axes lands at a light-space point whose
// direction from the card centre is unrelated to the drawn fragment's (the sun's up axis points away
// from the sun, the viewer's up points north: every card was lit on its lower side). The lookup is
// the drawn position P moved along the light direction f onto T's caster plane:
//     P' = P - f * ((P - T) . n) / (f . n)
// so it keeps P's light-space xy (retail's own lookup point, physically where the fragment is) and
// takes the depth of the card's caster there. The caster plane's normal follows from the card code's
// own construction (right' ~ R + f * 0.5 * (N . R), up' ~ U + f * 0.5 * (N . U)):
//     n = 0.5 * (N . R) * R + 0.5 * (N . U) * U - f
// with R / f / U the sun basis as the card code reads it from cb2[6..8] (.x / .y / .z columns) and N
// the world normal the card code multiplies by the camera rows (the temp source of its first
// camera-dependent instruction). Inserted right before the first shadow-matrix instruction, into the
// clone's copy of the position register; three extra temps.
// `dcl_temps` becomes 2T + 3; every other token is the user's own.
// ============================================================================
#include "foliage_aa/dxbc.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace BbsRewrite {

// The two constant-buffer windows (constants.srsl order; TREE_SHADOW.md §4.1).
constexpr uint32_t kGlobalCb       = 0;    // gc
constexpr uint32_t kWorldToCamera  = 4;    // gc.worldToCamera rows 4..7
constexpr uint32_t kLightCb        = 2;    // lc
constexpr uint32_t kTextureProj    = 6;    // lc.textureProjection rows 6..9 (the carrier)
constexpr uint32_t kShadowProj0    = 10;   // lc.shadowProjection[0] rows 10..13

struct Match {
    bool        container = false;   // parsed as DXBC with an SHDR / SHEX chunk
    bool        program   = false;   // its token stream parsed
    bool        vertex    = false;   // program type 1
    bool        candidate = false;   // reads cb2[10..13] AND cb0[4..7] somewhere (worth a log line when refused)
    bool        matched   = false;   // everything below is valid
    std::string why;                 // the refusal (matched == false)

    Dxbc::Container cont;
    Dxbc::Program   prog;
    uint32_t        temps      = 0;   // dcl_temps
    uint32_t        cb2Rows    = 0;   // dcl_constantbuffer cb2[n]
    uint32_t        shadowOut  = 0;   // the output register (o#)
    int             first      = -1;  // instruction index of the first tainted slice instruction
    std::vector<uint8_t> inSlice;     // per instruction: 1 = in the shadow output's slice
    std::vector<uint8_t> writesOut;   // per instruction: 1 = writes the shadow output (replaced)
    int             sliceCount = 0;   // slice instructions at or after `first` (the clones)
    int             cbReads    = 0;   // cb0[4..7] operands the clones redirect
    // TS-FIX: the projection's inputs
    uint32_t        posReg     = 0;   // the temp the shadow-matrix instructions read the position from (P)
    uint32_t        normalReg  = 0;   // the temp the first camera-dependent instruction multiplies (N)
    int             insertAt   = -1;  // instruction index of the first shadow-matrix instruction
};

// Parse and decide. Never throws, never touches the blob.
void Analyse(const uint8_t* blob, size_t size, Match* m);

// The transformed blob (fresh checksum). False when `m` did not match or the emitted copy fails its
// own re-parse (then nothing is used).
bool Transform(const Match& m, std::vector<uint8_t>* out);

}  // namespace BbsRewrite
