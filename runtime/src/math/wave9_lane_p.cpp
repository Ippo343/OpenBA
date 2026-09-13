// ============================================================================
// runtime/src/math/wave9_lane_p.cpp — Wave 9 lane P (tier 0), Math namespace
// ============================================================================
//
// Two exports the earlier Math waves left on the DLL:
//
//   ?Lerp@Quaternion@Math@@QEBA?AV12@AEBV12@AEBVScalar@2@@Z   0x18001d500
//   ?AsTransform3@Transform@Math@@QEBA?AVTransform3@2@XZ      0x18001fcd0
//
// Both are transliterations of the RETAIL DISASSEMBLY, not of Ghidra's C —
// term order preserved (float addition is not associative), every register
// spill accounted for. See runtime/src/math/transform.cpp's banner for the ABI
// rules that govern this whole namespace (hidden return slot in RDX).
//
// A CORRECTION TO WAVE 9 LANE B'S INVENTORY, recorded here rather than only in
// the wave doc (PLAN §6 step 8). `wave9_laneB_inventory.csv` lists
// `Quaternion::Lerp` as `ghidra-exact, 8` instructions and calls it a "trivial
// one-liner (1 call)". It is **85 instructions** at 0x18001d500 — a
// shortest-path nlerp with a full renormalise — and the "8" cannot be right for
// a body that saves and restores eight XMM registers. The one thing lane B got
// right about it is that the RVA-DELTA estimate (400 bytes) also overshoots;
// the truth is in between. Treat the CSV's `ghidra-exact` column as unverified
// wherever it disagrees with a read of the bytes.
// ============================================================================

#include "rt_libm.h"
#include "rt_port.h"
#include "../../types/math/Math.h"

// transform.cpp, Wave 1 lane 2 — the same class family, registered together.
// Declared rather than resolved: see the note on CallAsMatrix() below for why
// this one is a direct call.
Matrix* RtMath_Transform_AsMatrix(const Transform* self, Matrix* ret);

namespace {

// Math::Transform::AsMatrix, reached the way retail reaches it (a direct CALL
// to 0x18001e5f0) but through the PORT-AWARE resolver so that our own Wave 1
// port is what runs. Going by ADDRESS here would be the exact defect
// `tools/port_bypass_audit.py` was written for: ResolveAddr consults only the
// address registry, and Wave 1 registered AsMatrix by NAME, so a by-address
// call inside runtime/ walks past our port into the DLL.
//
// Resolved once and cached.
//
// ENDGAME B.3.1 note: this whole indirection is a step-2 casualty. The DLL is
// gone, `Transform::AsMatrix` is an ordinary member of our own class, and the
// registry lookup can only ever land back on the shim in transform.cpp. It is
// left exactly as it was here because step 1 moves bodies, not call sites.
Matrix* CallAsMatrix(const Transform* self, Matrix* ret) {
    using Fn = Matrix* (*)(const Transform*, Matrix*);
    static Fn fn = reinterpret_cast<Fn>(
        ResolveExport("?AsMatrix@Transform@Math@@QEBA?AVMatrix@2@XZ"));
    return fn ? fn(self, ret) : RtMath_Transform_AsMatrix(self, ret);
}

}  // namespace

// ============================================================================
// Math::Quaternion Math::Quaternion::Lerp(const Quaternion&, const Scalar&) const
// ?Lerp@Quaternion@Math@@QEBA?AV12@AEBV12@AEBVScalar@2@@Z   retail 0x18001d500
//
// Member, struct return: RCX = this (the FROM quaternion), RDX = hidden return
// slot, R8 = the TO quaternion, R9 = &t. That register assignment was already
// pinned independently by runtime/src/graphics/scene.cpp:250 from
// GetInstanceTransform's call site, and the disassembly agrees.
//
// It is NOT a plain lerp and it is not a slerp either: it is a SHORTEST-PATH
// NORMALISED LERP.
//
//   dot = a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w   (that association order)
//   s   = 1 - t                                   (computed BEFORE t is touched)
//   if (!(dot > 0)) t = -t;                       (COMISS + JA: NaN takes this
//                                                  branch too, and a faithful
//                                                  port must)
//   r   = a*s + b*t   componentwise
//   r  /= |r|                                     (NO zero guard — retail
//                                                  divides unconditionally, so
//                                                  a degenerate input yields
//                                                  inf/NaN in retail as well
//                                                  and we must not "fix" it;
//                                                  CLAUDE.md, "don't fix a
//                                                  faithful crash")
//
// Negating `t` rather than negating `b` is the same result and is what the
// bytes do: `XORPS XMM3,[0x1801617c0]` with the -0.0f sign mask.
//
// ORACLE: full byte compare of the 16-byte return against the shipped body run
// into a private scratch. Pure function of (this, other, t) — nothing else is
// read and nothing is written outside the return slot.
// ============================================================================
//
// ENDGAME B.3.1 step 1: MOVED onto Math::Quaternion::Lerp unchanged. The
// hidden return slot is gone (no cross-compiler ABI left), which also retires
// the aliasing hazard the retired diff oracle needed snapshots for; the
// snapshot locals were dead and are removed, the reasoning is kept above.
Math::Quaternion Math::Quaternion::Lerp(const Quaternion& other, const Scalar& t) const {
    Quaternion out;
    Quaternion* const ret = &out;

    const float ax = this->x,  ay = this->y,  az = this->z,  aw = this->w;
    const float bx = other.x, by = other.y, bz = other.z, bw = other.w;

    float tv = t.v;
    // XMM4 = 1.0f - t, computed at 0x18001d597 — i.e. BEFORE the sign flip
    // below, so it always uses the caller's t, never the negated one.
    const float s = RtMathConst::kOne - tv;

    float dot = ax * bx + ay * by;   // 0x18001d572/d567/d576
    dot = dot + az * bz;             // 0x18001d588/d593
    dot = dot + aw * bw;             // 0x18001d59b/d5a0

    // 0x18001d5a7  COMISS XMM2,0 ; JA — the negation is SKIPPED only when
    // dot > 0. Unordered (a NaN dot) does not take the jump, so NaN negates.
    if (!(dot > 0.0f)) tv = -tv;

    const float rx = ax * s + bx * tv;
    const float ry = ay * s + by * tv;
    const float rz = az * s + bz * tv;
    const float rw = aw * s + bw * tv;

    // 0x18001d5dd..d613, in exactly this association order.
    float len2 = ry * ry + rx * rx;
    len2 = len2 + rz * rz;
    len2 = len2 + rw * rw;

    const float len = RtLibm::Sqrt(len2);          // CALL 0x180128110 (sqrtf)
    const float inv = RtMathConst::kOne / len;     // DIVSS XMM10,XMM0

    // ALIASING (historical): the old out-pointer signature let `ret` be the
    // same buffer as `self` or `other`. See the block comment on AsTransform3
    // below — the same trap cost 901 phantom divergences there before it was
    // understood. The by-value return makes it impossible.
    ret->x = rx * inv;
    ret->y = ry * inv;
    ret->z = rz * inv;
    ret->w = rw * inv;

    return out;   // retail returned the hidden slot in RAX
}

// ============================================================================
// Math::Transform3 Math::Transform::AsTransform3() const
// ?AsTransform3@Transform@Math@@QEBA?AVTransform3@2@XZ   retail 0x18001fcd0
//
// Member, struct return: RCX = this, RDX = hidden return slot.
//
// The whole body is `AsMatrix()` into a 0x40-byte stack scratch, then a drop of
// the 4th column: rows 0/1/2 of the matrix become axisX/axisY/axisZ and row 3
// becomes the translation. Retail's scratch is at RSP+0x20 and it reads
// +0x20/24/28 -> ret+0x00/04/08, +0x30/34/38 -> ret+0x0c/10/14,
// +0x40/44/48 -> ret+0x18/1c/20, +0x50/54/58 -> ret+0x24/28/2c — which is
// m[0..2], m[4..6], m[8..10], m[12..14]. m[3], m[7], m[11] and m[15] are
// discarded, which is exactly what makes a rigid Transform3 out of a Matrix.
//
// NO NEW LANE-D ROWS: its single callee is already ours (Wave 1's AsMatrix).
//
// ORACLE: full byte compare of the 0x30-byte return. Every one of the 48 bytes
// is written by both sides (four Vector3s, no padding), so there is no
// unwritten-byte trap here — the failure mode
// reference_oracle_must_not_compare_unwritten_bytes.md describes does not apply.
// ============================================================================
//
// ENDGAME B.3.1 step 1: MOVED onto Math::Transform::AsTransform3 unchanged,
// returning by value. The aliasing note below is kept because it is the
// history of a 901-divergence investigation, but a by-value return cannot
// alias its input at all.
Math::Transform3 Math::Transform::AsTransform3() const {
    Transform3 out;
    Transform3* const ret = &out;
    const Transform* const self = this;

    // *** `ret` CAN BE THE SAME BUFFER AS `self`, AND AT LEAST ONE CALL SITE
    // *** RELIES ON IT. code/Game/Game/gameupdateloop.cpp:1496, the per-frame
    // camera update:
    //
    //     uint8_t transform3[44];
    //     void* result = fn_PlayerController_CalcTransform(playerCtrl, transform3);
    //     void* t3     = fn_Transform_AsTransform3(result, transform3);
    //
    // `result` IS `transform3` (a hidden-return-slot call handing its own out
    // buffer back in RAX), so `self == ret` exactly.
    //
    // THE PORT IS SAFE: AsMatrix reads every field of `self` into `m` before the
    // first store to `ret`, which is precisely why retail is safe too — its
    // scratch matrix at RSP+0x20 is fully populated by the CALL at 0x18001fcff
    // before the twelve MOVSS stores at 0x18001fd5b onward.
    //
    // THE ORACLE WAS NOT, and this cost 901 divergences in ~8,650 calls (a stable
    // ~10.4% across two runs, libm-independent) that read exactly like a port
    // defect. Running the shipped body on `self` AFTER we have written `ret` — the
    // same memory — hands it input we just destroyed, so `theirs` is garbage and
    // `ours` is correct. The snapshot below is taken before any store, and the
    // shipped body is run on THAT.
    //
    Matrix m;
    CallAsMatrix(self, &m);

    ret->axisX       = { m.m[0],  m.m[1],  m.m[2]  };
    ret->axisY       = { m.m[4],  m.m[5],  m.m[6]  };
    ret->axisZ       = { m.m[8],  m.m[9],  m.m[10] };
    ret->translation = { m.m[12], m.m[13], m.m[14] };

    return out;
}

// ---------------------------------------------------------------------------
// TRANSITIONAL SHIMS — ENDGAME B.3.1 step 1. One line each onto the members
// above; they and their registry rows die in step 2 with their last caller.
// ---------------------------------------------------------------------------
Quaternion* RtMath_Quaternion_Lerp(const Quaternion* self, Quaternion* ret,
                                   const Quaternion* other, const Scalar* t) {
    *ret = self->Lerp(*other, *t);
    return ret;
}

Transform3* RtMath_Transform_AsTransform3(const Transform* self, Transform3* ret) {
    *ret = self->AsTransform3();
    return ret;
}

// ---- Registration ----------------------------------------------------------
// Called from RtMathPortInit() alongside RtMathTransformRegister().
//
// Lerp is registered BY ADDRESS as well as by name: its only in-tree caller
// today, runtime/src/graphics/scene.cpp's RtQuaternionLerp, reaches it through
// ResolveAddr(Module::Runtime, 0x18001d500), and the address registry is a
// separate table from the export registry. Registering the name alone would
// leave that one caller — the only one — still on the DLL, which is precisely
// the shape of the port-bypass defect lane R found in the same file.
int RtMathWave9LanePRegister() {
    int n = 0;

    RegisterRuntimeExport("?Lerp@Quaternion@Math@@QEBA?AV12@AEBV12@AEBVScalar@2@@Z",
                          (void*)&RtMath_Quaternion_Lerp); ++n;
    RegisterRuntimeAddr(0x18001d500, (void*)&RtMath_Quaternion_Lerp);

    RegisterRuntimeExport("?AsTransform3@Transform@Math@@QEBA?AVTransform3@2@XZ",
                          (void*)&RtMath_Transform_AsTransform3); ++n;
    RegisterRuntimeAddr(0x18001fcd0, (void*)&RtMath_Transform_AsTransform3);

    return n;
}
