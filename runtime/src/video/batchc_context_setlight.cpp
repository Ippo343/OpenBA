// ============================================================================
// runtime/src/video/batchc_context_setlight.cpp — BATCH C
//   ?SetLight@Context@Video@@QEAAXAEBVVector3@Math@@0AEBVVector4@4@1MMH@Z
//   retail 0x18001a950, ordinal 3903
// ============================================================================
//
// EVERY Graphics light Draw arm ends here (lights_vtable.cpp's banner: "All
// five Draw arms end in it, and its EIGHT values — this + seven params — are
// what the whole of each body exists to compute"). Until this batch it was the
// last unported step of that chain; that comment is corrected at its source.
//
// SIGNATURE, counted against the mangled name AND the prologue (`SUB RSP,0x28`,
// so the incoming stack arguments sit at [RSP+0x50] .. [RSP+0x68]):
//
//   RCX        this                    Video::Context*
//   RDX        direction               const Math::Vector3&      (3 floats)
//   R8         attenuation             const Math::Vector3&      (3 floats)
//   R9         colour                  const Math::Vector4&      (4 dwords)
//   [RSP+0x50] position                const Math::Vector4&      (4 dwords)
//   [RSP+0x58] nearPlane               float
//   [RSP+0x60] farPlane                float
//   [RSP+0x68] shadowResolution        int
//
// No hidden return slot: the return type is `X` (void).
//
// ---------------------------------------------------------------------------
// FOUR THINGS THE DECOMPILER WOULD GET WRONG
// ---------------------------------------------------------------------------
// 1. THE TWO FLOAT SCALARS ARE NOT STORED IN ARGUMENT ORDER. The staging cell
//    at +0x1e18 gets `farPlane` and +0x1e1c gets **-nearPlane**, negated with
//    `XORPS XMM3, [0x1801617c0]` — a 16-byte constant read out of the image as
//    `80 00 00 00` x4, i.e. the IEEE sign mask, NOT a subtraction from zero.
//    (`0.0f - x` and `x ^ signbit` differ for x == 0.0f: the first gives +0.0f,
//    the second -0.0f. That is a real bit difference in a constant buffer.)
// 2. +0x1e20 IS A RECIPROCAL, and the DIVISION IS DONE IN THE ORDER RETAIL
//    WRITES IT: `XMM2 = 1.0f; XMM0 = farPlane; XMM0 -= nearPlane; XMM2 /= XMM0`
//    — one SUBSS then one DIVSS. Not `1.0f/(f-n)` folded, and not multiplied by
//    a precomputed reciprocal, which would round differently.
// 3. +0x1e14 IS A DOT PRODUCT WITH A FIXED ASSOCIATION ORDER:
//    `(att.y*dir.y + att.x*dir.x) + att.z*dir.z`. The y term is the LEFT
//    operand of the first ADDSS (0x18001aa7b adds XMM0 into XMM5, where XMM5
//    already holds the y product). Re-associating it is a different float.
//    (reference_moved_term_hides_where_terms_cancel.md: this project has
//    already lost a wave to exactly that.)
// 4. THE TWO COPY BLOCKS AT THE END ARE NOT A MEMCPY OF THE WHOLE STAGING
//    AREA. Retail copies exactly eight dwords from +0x1dd8 to +0x268 and
//    exactly four from +0x1e28 to +0x248, dword by dword, and touches nothing
//    between. Those two windows live inside Context::_pixelConstantStaging
//    (0x200..0x297) — the ranges `Reset` memsets and `BindResources` uploads —
//    so the light's colour/position pair and its shadow parameters are
//    published into the PIXEL constant staging as well as the light staging.
//
// ORACLE: A REAL ONE. This body is a pure function of its eight values into
// `this`: it reads no global, calls nothing, and writes only the three windows
// below plus one OR into the dirty mask. Re-running the shipped copy on the
// SAME object therefore writes the same bytes again — it is idempotent — so
// the harness snapshots our result, lets the DLL overwrite it, and compares.
// That is a genuine byte comparison, not an attendance marker, and every Draw
// arm drives it on every frame the DLL is present.
// ============================================================================

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "../../types/video/Context.h"

// ENDGAME B.3.5 block D1: the body MOVED onto `Video::Context::SetLight`; the
// registered door at the bottom kept its name, its mangled-name row and its
// address row and is a one-line QUALIFIED non-virtual delegation. The offsets
// stay RAW: every one of them is inside `_lightStaging` (+0x1dd8, 0x1a0 bytes)
// or `_pixelConstantStaging` (+0x200, 0x98 bytes), two members Context.h keeps
// undivided because retail's own `Reset` and `BindResources` treat them as
// single blocks. The four static_asserts below are what pin the raw offsets to
// those two members.
namespace {

using Video::Context;

// The three windows this body writes, and nothing else.
constexpr uint32_t kLightStage    = 0x1dd8;   // .. 0x1e38, 0x60 bytes
constexpr uint32_t kLightStageLen = 0x60;
constexpr uint32_t kPixelColour   = 0x268;    // .. 0x288, 8 dwords copied from +0x1dd8
constexpr uint32_t kPixelColourLen = 0x20;
constexpr uint32_t kPixelShadow   = 0x248;    // .. 0x258, 4 dwords copied from +0x1e28
constexpr uint32_t kPixelShadowLen = 0x10;
constexpr uint32_t kDirtyBits     = 0x12000;  // OR dword [this+0x10],0x12000

static_assert(offsetof(Context, _lightStaging) == kLightStage,
              "SetLight writes Context::_lightStaging");
static_assert(kLightStage + kLightStageLen <=
                  offsetof(Context, _lightStaging) + sizeof(Context::_lightStaging),
              "the light window must stay inside _lightStaging");
static_assert(kPixelShadow >= offsetof(Context, _pixelConstantStaging) &&
                  kPixelColour + kPixelColourLen <=
                      offsetof(Context, _pixelConstantStaging) +
                          sizeof(Context::_pixelConstantStaging),
              "both published windows must stay inside _pixelConstantStaging");
static_assert(offsetof(Context, _dirty) == 0x10, "SetLight ORs Context::_dirty");

// The immediates, as BIT PATTERNS read out of the image (lights_vtable.cpp's
// rule) rather than converted from decimal:
//   0x18016169c = 0x3f800000        = 1.0f
//   0x1801617c0 = 80000000 x4       = the IEEE sign mask (read with read_memory)
constexpr uint32_t kBits_one  = 0x3f800000u;
constexpr uint32_t kBits_sign = 0x80000000u;

float AsFloat(uint32_t bits) { float f; std::memcpy(&f, &bits, 4); return f; }
float NegateBySignMask(float v) {
    uint32_t b; std::memcpy(&b, &v, 4);
    b ^= kBits_sign;                        // XORPS, not `0.0f - v`
    float r; std::memcpy(&r, &b, 4); return r;
}

inline uint8_t* At(void* self, uint32_t off) {
    return static_cast<uint8_t*>(self) + off;
}
inline void StoreF(void* self, uint32_t off, float v) { std::memcpy(At(self, off), &v, 4); }
inline void CopyD(void* self, uint32_t dst, uint32_t src) {
    std::memcpy(At(self, dst), At(self, src), 4);
}

using SetLightFn = void (*)(void*, const float*, const float*,
                            const uint32_t*, const uint32_t*, float, float, int32_t);

}  // namespace

// ============================================================================
// ?SetLight@Context@Video@@QEAAXAEBVVector3@Math@@0AEBVVector4@4@1MMH@Z
//                                                   retail 0x18001a950, ord 3903
// ============================================================================
// The parameters are raw pointers, not `const Math::Vector3&` /
// `const Math::Vector4&`, and that is deliberate rather than laziness: the two
// Vector3s are read as three consecutive floats and the two Vector4s are COPIED
// as four consecutive DWORDS (retail never does arithmetic on the colour or the
// position, which is why the copy is a dword copy and not four float loads).
// Typing them would invite a later reader to do float arithmetic on bytes retail
// only ever moves. Context.h's declaration carries the same note.
void Video::Context::SetLight(const void* directionRaw, const void* attenuationRaw,
                              const void* colourRaw, const void* positionRaw,
                              float nearPlane, float farPlane,
                              int32_t shadowResolution) {
    Context* const self = this;
    const float*    direction   = static_cast<const float*>(directionRaw);
    const float*    attenuation = static_cast<const float*>(attenuationRaw);
    const uint32_t* colour      = static_cast<const uint32_t*>(colourRaw);
    const uint32_t* position    = static_cast<const uint32_t*>(positionRaw);

    // +0x1dd8 .. +0x1de4 — the colour, four dwords copied as dwords.
    std::memcpy(At(self, 0x1dd8), colour, 16);

    // +0x1de8 .. +0x1df4 — the position, four dwords.
    std::memcpy(At(self, 0x1de8), position, 16);

    // +0x1e18 / +0x1e1c / +0x1e20 / +0x1e24 — see banner points 1 and 2.
    const float invRange = AsFloat(kBits_one) / (farPlane - nearPlane);
    StoreF(self, 0x1e18, farPlane);
    StoreF(self, 0x1e1c, NegateBySignMask(nearPlane));
    StoreF(self, 0x1e20, invRange);
    StoreF(self, 0x1e24, 0.0f);

    // +0x1df8 .. +0x1e04 — the direction, with w = 1.0f.
    StoreF(self, 0x1df8, direction[0]);
    StoreF(self, 0x1dfc, direction[1]);
    StoreF(self, 0x1e00, direction[2]);
    StoreF(self, 0x1e04, AsFloat(kBits_one));

    // +0x1e08 .. +0x1e10 — the attenuation, then its dot with the direction at
    // +0x1e14. Association order per banner point 3.
    StoreF(self, 0x1e08, attenuation[0]);
    StoreF(self, 0x1e0c, attenuation[1]);
    StoreF(self, 0x1e10, attenuation[2]);
    const float dot = (attenuation[1] * direction[1] + attenuation[0] * direction[0]) +
                      attenuation[2] * direction[2];
    StoreF(self, 0x1e14, dot);

    // +0x1e28 — 1.0f / (float)shadowResolution. CVTDQ2PS: a SIGNED int-to-float
    // conversion, then a single DIVSS.
    StoreF(self, 0x1e28, AsFloat(kBits_one) / static_cast<float>(shadowResolution));
    StoreF(self, 0x1e2c, 0.0f);
    StoreF(self, 0x1e30, 0.0f);
    StoreF(self, 0x1e34, 0.0f);

    // The two publish blocks, dword by dword, exactly as retail (banner 4).
    for (uint32_t k = 0; k < 8; ++k) CopyD(self, kPixelColour + k * 4, 0x1dd8 + k * 4);
    for (uint32_t k = 0; k < 4; ++k) CopyD(self, kPixelShadow + k * 4, 0x1e28 + k * 4);

    *reinterpret_cast<uint32_t*>(At(self, 0x10)) |= kDirtyBits;
}
