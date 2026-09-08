// ============================================================================
// runtime/src/math/wave9_lane_gfxr.cpp — Wave 9 lane GFXR, Math namespace
// ============================================================================
//
//   ?Inverse@Matrix@Math@@QEBA?AV12@XZ      retail 0x1800bb310, ordinal 3082
//
// The one Math export left on the residue list, and it is not there for a
// decorative reason: it is an UNSATISFIED DOOR INSIDE OUR OWN ALREADY-PORTED
// `Video::Context::SetCamera` (runtime/src/video/context.cpp:352). Lane CTX
// flagged it as the thing that kills a renderer lane before it starts.
//
// 334 instructions, all scalar SSE, no calls. A cofactor/adjugate 4x4 inverse:
// six 2x2 minors from the BOTTOM two rows, four cofactors of the top row, one
// determinant, then `1.0f / det` ONCE and sixteen multiplies by that
// reciprocal. `det == 0.0f` exactly (and ORDERED — a NaN determinant does NOT
// take this branch) returns the IDENTITY, not garbage and not a zero matrix.
//
// ---------------------------------------------------------------------------
// WHY THIS FILE IS A TRANSLITERATION AND MUST STAY ONE
// ---------------------------------------------------------------------------
// Wave 9 lane N spent a session on `Picking::Scene::Intersect` because a
// translation term had been folded into a float sum FIRST instead of added
// LAST. That is algebraically identical and bit-different only where the terms
// nearly cancel — it presented at ONE CALL IN 23,654, so a clean re-run could
// never have exonerated it (reference_moved_term_hides_where_terms_cancel.md).
//
// A determinant IS a near-cancelling sum, and this body has sixteen more of
// them in the adjugate. So every sum below is parenthesised to match the
// ADDSS/SUBSS order in the retail schedule, each cell cites the address of the
// instruction that fixes its grouping, and the reciprocal is computed once and
// multiplied in exactly where retail multiplies it.
//
//   *** DO NOT SIMPLIFY THE ARITHMETIC IN THIS FILE. ***
//   Float addition is not associative. `(x + y) + z` and `x + (y + z)` are
//   different functions, and this is the shape where the difference shows.
//
// Multiplication and addition are both COMMUTATIVE in IEEE-754, so `a*b` vs
// `b*a` and `x+y` vs `y+x` are free; only the GROUPING is load-bearing. The
// operand order below still mirrors the disassembly, to keep the cross-check
// against the listing mechanical.
//
// Negation is `XORPS` against the 16-byte -0.0f mask at 0x1801617c0, i.e. a
// SIGN-BIT FLIP, and is applied AFTER the multiply by the reciprocal exactly
// where retail applies it. `NegBits` below does the bit flip rather than
// spelling `-x`, so the transcription is checkable against the bytes without
// an argument about what the compiler does to unary minus on a NaN.
//
// Constants LIFTED AS RAW BITS, never retyped as decimals
// (reference_crt_coefficients_must_be_lifted_not_retyped.md):
//   0x18016169c = 0x3f800000  (1.0f, the reciprocal's numerator AND the
//                              identity-matrix diagonal)
//   0x1801617c0 = 80000000 80000000 80000000 80000000  (the -0.0f sign mask)
//
// ---------------------------------------------------------------------------
// ORACLE
// ---------------------------------------------------------------------------
// `Inverse` is a pure function of `*this` writing only the 64-byte return
// slot, so a full byte compare against the shipped body is sound — with one
// precaution this project has already paid for once
// (reference_oracle_must_snapshot_aliased_inputs.md): `ret` is a hidden slot
// the caller chose and MAY alias `self`, so the oracle snapshots the input
// before our body writes anything.
//
// The diff site alone would be weak here: `SetCamera` calls this a handful of
// times per frame on well-conditioned view matrices, which is precisely the
// input class a near-cancellation defect is invisible on. So this file also
// ships an ADVERSARIAL CORPUS PROBE (`BANISHED_RT_INVERSE_PROBE`, below) that
// drives millions of matrices — including fine CONTIGUOUS SWEEPS through
// near-singularity, which is the diagnostic that finally cracked lane N's
// moved term — through both bodies at init while the DLL is still present.
// It carries an EFFECTIVENESS control (how many cases took each branch, and
// how many were near-singular) and a NEGATIVE control that must be seen to
// fire (reference_probe_needs_an_effectiveness_control.md).
// ============================================================================

#include "rt_port.h"
#include "../../types/math/Math.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

inline float BitsToFloat(uint32_t bits) { float f; std::memcpy(&f, &bits, 4); return f; }
inline uint32_t FloatToBits(float f) { uint32_t b; std::memcpy(&b, &f, 4); return b; }

// 0x18016169c. Used both as the reciprocal numerator (DIVSS at 0x1800bb5b3)
// and as the identity diagonal on the det==0 path (0x1800bb545).
const float kOneF = BitsToFloat(0x3f800000u);

// XORPS against 0x1801617c0 — a sign-bit flip, not a subtraction from zero.
inline float NegBits(float f) { return BitsToFloat(FloatToBits(f) ^ 0x80000000u); }

}  // namespace

// ============================================================================
// Math::Matrix Math::Matrix::Inverse() const
// ?Inverse@Matrix@Math@@QEBA?AV12@XZ      retail 0x1800bb310
//
// Member, struct return: RCX = this, RDX = hidden return slot. Returns RDX in
// RAX (0x1800bb8e4), which is what the ABI requires and what the call site in
// Video::Context::SetCamera already assumes (context.cpp's MatrixInverseFn).
//
// Cell naming: `aRC` is m[R*4 + C], matching the offsets the disassembly loads
//   a00 +0x00  a01 +0x04  a02 +0x08  a03 +0x0c
//   a10 +0x10  a11 +0x14  a12 +0x18  a13 +0x1c
//   a20 +0x20  a21 +0x24  a22 +0x28  a23 +0x2c
//   a30 +0x30  a31 +0x34  a32 +0x38  a33 +0x3c
// ============================================================================
Matrix* RtMath_Matrix_Inverse(const Matrix* self, Matrix* ret) {
    RT_DIFF_BEGIN("Math::Matrix::Inverse");

    // Snapshot BEFORE the first store to `ret`: the hidden return slot is
    // allowed to alias `self`, and feeding the shipped body an already-written
    // `self` would manufacture a divergence that is the oracle's fault.
    Matrix selfSnap{};
    if (rt_diff_on__) selfSnap = *self;

    const float a00 = self->m[0],  a01 = self->m[1],  a02 = self->m[2],  a03 = self->m[3];
    const float a10 = self->m[4],  a11 = self->m[5],  a12 = self->m[6],  a13 = self->m[7];
    const float a20 = self->m[8],  a21 = self->m[9],  a22 = self->m[10], a23 = self->m[11];
    const float a30 = self->m[12], a31 = self->m[13], a32 = self->m[14], a33 = self->m[15];

    // ---- Six 2x2 minors of the BOTTOM two rows (0x1800bb3e2 .. 0x1800bb48f)
    const float A = a20 * a31 - a21 * a30;   // XMM9   0x1800bb3e7 / 0x1800bb427
    const float B = a32 * a20 - a22 * a30;   // XMM10  0x1800bb3e2 / 0x1800bb435
    const float C = a32 * a21 - a22 * a31;   // XMM11  0x1800bb401 / 0x1800bb448
    const float D = a33 * a21 - a23 * a31;   // XMM12  0x1800bb3ec / 0x1800bb459
    const float E = a33 * a22 - a23 * a32;   // XMM13  0x1800bb406 / 0x1800bb46a
    const float F = a23 * a30 - a33 * a20;   // XMM14  0x1800bb422 / 0x1800bb48a

    // ---- The four top-row cofactors (0x1800bb47d .. 0x1800bb519).
    // Grouping fixed by the ADDSS/SUBSS order; the sign flips at 0x1800bb4d1
    // and 0x1800bb519 are XORPS, applied to the finished sum.
    const float C0 = (a11 * E - a12 * D) + a13 * C;              // XMM6
    const float C1 = NegBits((a10 * E + a12 * F) + a13 * B);     // XMM3
    const float C2 = (a10 * D + a11 * F) + a13 * A;              // XMM8
    const float C3 = NegBits((a10 * C - a11 * B) + a12 * A);     // XMM4

    // ---- Determinant (0x1800bb515 .. 0x1800bb53a), left-associated.
    const float det = ((a00 * C0 + a01 * C1) + a02 * C2) + a03 * C3;   // XMM5

    // 0x1800bb53e  UCOMISS XMM5,XMM2(=0) ; JP ; JNZ
    // The JP sends an UNORDERED compare (det is NaN) to the DIVIDE path, so
    // only an ordered, exactly-zero determinant returns the identity. Written
    // as `det == 0.0f` because that is false for NaN, which is the same branch
    // retail takes. It is true for -0.0f in both implementations.
    if (det == 0.0f) {
        // 0x1800bb545 .. 0x1800bb59c — written DIRECTLY to the return slot,
        // bypassing the zeroed local the divide path uses.
        for (int idx = 0; idx < 16; ++idx) ret->m[idx] = 0.0f;
        ret->m[0] = kOneF;
        ret->m[5] = kOneF;
        ret->m[10] = kOneF;
        ret->m[15] = kOneF;
        if (rt_diff_on__) {
            Matrix theirs{};
            typedef void (*OrigFn)(const Matrix*, Matrix*);
            RtOriginal<OrigFn>("?Inverse@Matrix@Math@@QEBA?AV12@XZ")(&selfSnap, &theirs);
            RT_DIFF_BYTES(ret, &theirs, sizeof(Matrix));
        }
        return ret;
    }

    // 0x1800bb5b3  DIVSS XMM7,XMM5 — ONE reciprocal, then sixteen MULSS. Not
    // sixteen divisions: `x * (1/det)` and `x / det` differ in the last bit.
    const float inv = kOneF / det;   // XMM7

    // ---- Six 2x2 minors of the TOP two rows (0x1800bb6d1 .. 0x1800bb757)
    const float G = a00 * a11 - a01 * a10;   // XMM6
    const float H = a00 * a12 - a02 * a10;   // XMM5
    const float I = a01 * a12 - a02 * a11;   // XMM4
    const float J = a01 * a13 - a03 * a11;   // XMM13
    const float K = a02 * a13 - a03 * a12;   // XMM11
    const float L = a03 * a10 - a00 * a13;   // XMM14

    // ---- The sixteen result cells, into a LOCAL first.
    // Retail builds the whole matrix in a stack local (RSP+0x10 .. RSP+0x4c)
    // and copies it out as four 16-byte MOVUPS at the very end (0x1800bb88f,
    // 0x1800bb8b2, 0x1800bb8bb, 0x1800bb8cc). That is what makes `ret`
    // aliasing `self` safe, so the port does the same rather than writing
    // through `ret` as it goes.
    float r[16];

    // Row 0. r[0] is the cofactor already computed for the determinant.
    r[0]  = C0 * inv;                                                     // 0x1800bb5c3
    r[1]  = NegBits(((a01 * E - a02 * D) + a03 * C) * inv);               // 0x1800bb627
    r[2]  = ((a31 * K - a32 * J) + a33 * I) * inv;                        // 0x1800bb783
    r[3]  = NegBits(((a21 * K - a22 * J) + a23 * I) * inv);               // 0x1800bb845

    // Row 1.
    r[4]  = C1 * inv;                                                     // 0x1800bb5bb
    r[5]  = ((a02 * F + a00 * E) + a03 * B) * inv;                        // 0x1800bb623
    r[6]  = NegBits(((a32 * L + a30 * K) + a33 * H) * inv);               // 0x1800bb7d7
    r[7]  = ((a22 * L + a20 * K) + a23 * H) * inv;                        // 0x1800bb870

    // Row 2.
    r[8]  = C2 * inv;                                                     // 0x1800bb5ec
    r[9]  = NegBits(((a00 * D + a01 * F) + a03 * A) * inv);               // 0x1800bb6b1
    r[10] = ((a30 * J + a31 * L) + a33 * G) * inv;                        // 0x1800bb7eb
    r[11] = NegBits(((a21 * L + a20 * J) + a23 * G) * inv);               // 0x1800bb892

    // Row 3.
    r[12] = C3 * inv;                                                     // 0x1800bb5bf
    r[13] = ((a00 * C - a01 * B) + a02 * A) * inv;                        // 0x1800bb6cd
    r[14] = NegBits(((a30 * I - a31 * H) + a32 * G) * inv);               // 0x1800bb804
    r[15] = ((a20 * I - a21 * H) + a22 * G) * inv;                        // 0x1800bb8bf

    std::memcpy(ret->m, r, sizeof(r));

    if (rt_diff_on__) {
        Matrix theirs{};
        typedef void (*OrigFn)(const Matrix*, Matrix*);
        RtOriginal<OrigFn>("?Inverse@Matrix@Math@@QEBA?AV12@XZ")(&selfSnap, &theirs);
        RT_DIFF_BYTES(ret, &theirs, sizeof(Matrix));
    }
    return ret;
}

// ============================================================================
// THE ADVERSARIAL CORPUS PROBE — BANISHED_RT_INVERSE_PROBE
// ============================================================================
//
// Rationale, in one line: the live call sites cannot falsify this port. The
// only in-tree caller is `Video::Context::SetCamera`, a handful of calls per
// frame on well-conditioned view matrices — exactly the input class on which a
// moved-term defect is invisible. Lane N's finding is that the tell for
// "arithmetic specific to a VALUE RANGE" rather than random rounding is
// CONTIGUOUS PATCHES of diverging inputs, and you only see patches if you
// SWEEP a parameter finely instead of sampling it randomly.
//
// So the corpus is five families, run against the shipped DLL body:
//
//   1. FIXED     — identity, zero, all-ones, a real projection, a real view
//                  matrix, and matrices built to make `det` exactly 0.
//   2. RANDOM    — a deterministic LCG over seven magnitude decades, so the
//                  cofactor products span the whole exponent range.
//   3. SWEEP     — a fine, CONTIGUOUS sweep of ONE cell across a range that
//                  carries the matrix through singularity. The determinant
//                  passes through zero, so the near-cancelling sums are
//                  evaluated at every conditioning, in order, and a divergence
//                  PATCH is legible as a run of adjacent steps.
//   4. NEARSING  — rows built as `row3 = alpha*row0 + beta*row1` (exactly
//                  singular) and then perturbed by an epsilon swept down
//                  through the binades to a few ULP.
//   5. INTEGERS  — small integer matrices, where every product is exact and
//                  the ONLY possible source of a difference is the grouping of
//                  the sums.
//
// CONTROLS, both mandatory:
//
//   EFFECTIVENESS — counted from the CASE, not inferred from the family: how
//   many cases took the DIVIDE path, how many took the det==0 IDENTITY path,
//   how many produced an all-finite reference result, and how many were
//   NEAR-SINGULAR (the four determinant terms cancelling to under a thousandth
//   of the largest). A run whose identityPath is 0 has not tested the identity
//   branch, whatever its divergence count says, and the report says so.
//
//   NEGATIVE — `BANISHED_RT_INVERSE_PROBE=neg` runs a COPY of the body with
//   ONE TERM MOVED in every three-term sum — `(x + z) + y` where the port has
//   `(x + y) + z`, which is lane N's exact defect class and is algebraically
//   identical — and must be SEEN TO FIRE. If it does not fire, the corpus
//   cannot distinguish groupings at all and the POS run proved nothing; the
//   report says that in those words rather than printing a reassuring zero.
//
// The probe runs ONCE, at RtMathPortInit time, with the DLL present. It is off
// unless the env var is set, and it can never run at cutover — there would be
// no shipped body to compare against, which is why it is built and run NOW.
// ============================================================================

namespace {

struct ProbeStats {
    uint64_t cases = 0;
    uint64_t dividePath = 0;
    uint64_t identityPath = 0;
    uint64_t nearSingular = 0;
    uint64_t allFinite = 0;
    uint64_t diverged = 0;
    uint64_t negFired = 0;
    bool     haveFirst = false;
    int      firstFamily = -1;
    double   firstParam = 0.0;
    // Contiguity read-out for family 3: the longest run of ADJACENT sweep
    // steps that differed. A long run says "value range"; scattered singles
    // say "random rounding" — the distinction that cracked lane N's defect.
    int      sweepRunCurrent = 0;
    int      sweepRunLongest = 0;
};

// The MOVED-TERM perturbation: same three products, same signs, one different
// grouping.
inline float MovedTerm3(float x, float y, float z) { return (x + z) + y; }

// A COPY of the body with that perturbation applied to every three-term sum.
// Deliberately a copy rather than a flag inside the real body: a probe that
// edits its subject cannot testify about its subject.
void InverseMovedTerm(const Matrix* self, Matrix* ret) {
    const float a00 = self->m[0],  a01 = self->m[1],  a02 = self->m[2],  a03 = self->m[3];
    const float a10 = self->m[4],  a11 = self->m[5],  a12 = self->m[6],  a13 = self->m[7];
    const float a20 = self->m[8],  a21 = self->m[9],  a22 = self->m[10], a23 = self->m[11];
    const float a30 = self->m[12], a31 = self->m[13], a32 = self->m[14], a33 = self->m[15];

    const float A = a20 * a31 - a21 * a30;
    const float B = a32 * a20 - a22 * a30;
    const float C = a32 * a21 - a22 * a31;
    const float D = a33 * a21 - a23 * a31;
    const float E = a33 * a22 - a23 * a32;
    const float F = a23 * a30 - a33 * a20;

    const float C0 = MovedTerm3(a11 * E, NegBits(a12 * D), a13 * C);
    const float C1 = NegBits(MovedTerm3(a10 * E, a12 * F, a13 * B));
    const float C2 = MovedTerm3(a10 * D, a11 * F, a13 * A);
    const float C3 = NegBits(MovedTerm3(a10 * C, NegBits(a11 * B), a12 * A));

    const float det = ((a00 * C0 + a01 * C1) + a02 * C2) + a03 * C3;
    if (det == 0.0f) {
        for (int idx = 0; idx < 16; ++idx) ret->m[idx] = 0.0f;
        ret->m[0] = kOneF; ret->m[5] = kOneF; ret->m[10] = kOneF; ret->m[15] = kOneF;
        return;
    }
    const float inv = kOneF / det;

    const float G = a00 * a11 - a01 * a10;
    const float H = a00 * a12 - a02 * a10;
    const float I = a01 * a12 - a02 * a11;
    const float J = a01 * a13 - a03 * a11;
    const float K = a02 * a13 - a03 * a12;
    const float L = a03 * a10 - a00 * a13;

    float r[16];
    r[0]  = C0 * inv;
    r[1]  = NegBits(MovedTerm3(a01 * E, NegBits(a02 * D), a03 * C) * inv);
    r[2]  = MovedTerm3(a31 * K, NegBits(a32 * J), a33 * I) * inv;
    r[3]  = NegBits(MovedTerm3(a21 * K, NegBits(a22 * J), a23 * I) * inv);
    r[4]  = C1 * inv;
    r[5]  = MovedTerm3(a02 * F, a00 * E, a03 * B) * inv;
    r[6]  = NegBits(MovedTerm3(a32 * L, a30 * K, a33 * H) * inv);
    r[7]  = MovedTerm3(a22 * L, a20 * K, a23 * H) * inv;
    r[8]  = C2 * inv;
    r[9]  = NegBits(MovedTerm3(a00 * D, a01 * F, a03 * A) * inv);
    r[10] = MovedTerm3(a30 * J, a31 * L, a33 * G) * inv;
    r[11] = NegBits(MovedTerm3(a21 * L, a20 * J, a23 * G) * inv);
    r[12] = C3 * inv;
    r[13] = MovedTerm3(a00 * C, NegBits(a01 * B), a02 * A) * inv;
    r[14] = NegBits(MovedTerm3(a30 * I, NegBits(a31 * H), a32 * G) * inv);
    r[15] = MovedTerm3(a20 * I, NegBits(a21 * H), a22 * G) * inv;
    std::memcpy(ret->m, r, sizeof(r));
}

// Deterministic 32-bit LCG. Not std::rand: the corpus must be identical on
// every run so a divergence is re-findable by case index.
struct Lcg {
    uint32_t s;
    explicit Lcg(uint32_t seed) : s(seed) {}
    uint32_t Next() { s = s * 1664525u + 1013904223u; return s; }
    // A float in (-scale, scale) with a uniform MANTISSA, so the corpus is not
    // clustered near the top of the exponent range.
    float Signed(float scale) {
        const uint32_t u = Next();
        const float mag = static_cast<float>(u >> 8) * (1.0f / 16777216.0f);
        return (u & 1u) ? -mag * scale : mag * scale;
    }
};

typedef void (*InverseFn)(const Matrix*, Matrix*);

void RunOne(ProbeStats& st, InverseFn theirs, bool negative,
            const Matrix& in, int family, double param) {
    Matrix ours{}, ref{};
    ++st.cases;

    if (negative) {
        InverseMovedTerm(&in, &ours);
    } else {
        RtMath_Matrix_Inverse(&in, &ours);
    }
    theirs(&in, &ref);

    // --- EFFECTIVENESS, computed from THIS case's own inputs ---------------
    {
        const float* m = in.m;
        const float A = m[8] * m[13] - m[9] * m[12];
        const float B = m[14] * m[8] - m[10] * m[12];
        const float C = m[14] * m[9] - m[10] * m[13];
        const float D = m[15] * m[9] - m[11] * m[13];
        const float E = m[15] * m[10] - m[11] * m[14];
        const float F = m[11] * m[12] - m[15] * m[8];
        const float C0 = (m[5] * E - m[6] * D) + m[7] * C;
        const float C1 = NegBits((m[4] * E + m[6] * F) + m[7] * B);
        const float C2 = (m[4] * D + m[5] * F) + m[7] * A;
        const float C3 = NegBits((m[4] * C - m[5] * B) + m[6] * A);
        const float det = ((m[0] * C0 + m[1] * C1) + m[2] * C2) + m[3] * C3;
        if (det == 0.0f) ++st.identityPath; else ++st.dividePath;

        const float terms[4] = { m[0] * C0, m[1] * C1, m[2] * C2, m[3] * C3 };
        float biggest = 0.0f;
        for (int t = 0; t < 4; ++t) {
            const float av = terms[t] < 0.0f ? -terms[t] : terms[t];
            if (av > biggest) biggest = av;
        }
        const float adet = det < 0.0f ? -det : det;
        // "Near-singular" means the four determinant terms CANCEL to almost
        // nothing — the regime where association changes the rounded answer.
        if (biggest > 0.0f && adet < biggest * 1e-3f) ++st.nearSingular;
    }

    bool finite = true;
    for (int t = 0; t < 16; ++t) {
        if ((FloatToBits(ref.m[t]) & 0x7f800000u) == 0x7f800000u) { finite = false; break; }
    }
    if (finite) ++st.allFinite;

    const bool differs = std::memcmp(ours.m, ref.m, sizeof(ours.m)) != 0;
    if (differs) {
        if (negative) ++st.negFired; else ++st.diverged;
        if (!st.haveFirst) {
            st.haveFirst = true;
            st.firstFamily = family;
            st.firstParam = param;
        }
    }
    if (family == 3) {
        if (differs) {
            ++st.sweepRunCurrent;
            if (st.sweepRunCurrent > st.sweepRunLongest) st.sweepRunLongest = st.sweepRunCurrent;
        } else {
            st.sweepRunCurrent = 0;
        }
    }
}

void FillIdentity(Matrix& m) {
    for (int t = 0; t < 16; ++t) m.m[t] = 0.0f;
    m.m[0] = m.m[5] = m.m[10] = m.m[15] = 1.0f;
}

void RunInverseProbe(bool negative) {
    typedef void (*OrigFn)(const Matrix*, Matrix*);
    OrigFn theirs = RtOriginal<OrigFn>("?Inverse@Matrix@Math@@QEBA?AV12@XZ");
    if (!theirs) {
        printf("[InverseProbe] VOID -- the shipped ?Inverse@Matrix@Math@@ did not resolve. "
               "There is nothing to compare against and this run proves NOTHING.\n");
        return;
    }

    ProbeStats st;

    // ---- family 1: fixed matrices ----------------------------------------
    {
        Matrix m{};
        FillIdentity(m);
        RunOne(st, theirs, negative, m, 1, 0);

        for (int t = 0; t < 16; ++t) m.m[t] = 0.0f;          // det == 0
        RunOne(st, theirs, negative, m, 1, 1);

        for (int t = 0; t < 16; ++t) m.m[t] = 1.0f;          // det == 0, rank 1
        RunOne(st, theirs, negative, m, 1, 2);

        // A real perspective projection (fovy 60, aspect 16:9, n 0.1, f 1000).
        for (int t = 0; t < 16; ++t) m.m[t] = 0.0f;
        m.m[0] = 0.974279f; m.m[5] = 1.732051f;
        m.m[10] = -1.000200f; m.m[11] = -1.0f; m.m[14] = -0.200020f;
        RunOne(st, theirs, negative, m, 1, 3);

        // A real view matrix: rotation about Y plus a translation — the shape
        // Video::Context::SetCamera actually hands us.
        FillIdentity(m);
        m.m[0] = 0.707107f;  m.m[2] = -0.707107f;
        m.m[8] = 0.707107f;  m.m[10] = 0.707107f;
        m.m[12] = 41.5f; m.m[13] = 12.25f; m.m[14] = -37.125f;
        RunOne(st, theirs, negative, m, 1, 4);
    }

    // ---- family 2: random, across magnitude decades -----------------------
    {
        Lcg rng(0x9E3779B9u);
        static const float kScales[7] = { 1e-6f, 1e-3f, 1.0f, 10.0f, 1e3f, 1e6f, 1e9f };
        for (int sIdx = 0; sIdx < 7; ++sIdx) {
            for (int caseIdx = 0; caseIdx < 40000; ++caseIdx) {
                Matrix m{};
                for (int t = 0; t < 16; ++t) m.m[t] = rng.Signed(kScales[sIdx]);
                RunOne(st, theirs, negative, m, 2, kScales[sIdx]);
            }
        }
    }

    // ---- family 3: a CONTIGUOUS sweep through singularity ------------------
    {
        Matrix base{};
        Lcg rng(0x1234567u);
        for (int t = 0; t < 16; ++t) base.m[t] = rng.Signed(4.0f);
        for (int cell = 0; cell < 16; ++cell) {
            const float centre = base.m[cell];
            st.sweepRunCurrent = 0;
            for (int step = -30000; step <= 30000; ++step) {
                Matrix m = base;
                m.m[cell] = centre + static_cast<float>(step) * 1e-4f;
                RunOne(st, theirs, negative, m, 3, static_cast<double>(step));
            }
        }
    }

    // ---- family 4: rows made exactly dependent, then eps swept to ULP ------
    {
        Lcg rng(0xBEEF0001u);
        for (int trial = 0; trial < 400; ++trial) {
            Matrix base{};
            for (int t = 0; t < 16; ++t) base.m[t] = rng.Signed(3.0f);
            const float alpha = rng.Signed(2.0f);
            const float beta  = rng.Signed(2.0f);
            for (int t = 0; t < 4; ++t)
                base.m[12 + t] = alpha * base.m[t] + beta * base.m[4 + t];
            for (int e = 0; e < 2000; ++e) {
                Matrix m = base;
                // eps walks down from 1.0 through the binades, four cells at a
                // time; at the far end it is denormal-adjacent.
                const float eps = BitsToFloat(0x3f800000u - static_cast<uint32_t>(e) * 4096u);
                for (int t = 0; t < 4; ++t) m.m[12 + t] += eps;
                RunOne(st, theirs, negative, m, 4, static_cast<double>(e));
            }
        }
    }

    // ---- family 5: small integers — every product exact --------------------
    {
        Lcg rng(0x0F0F0F0Fu);
        for (int caseIdx = 0; caseIdx < 200000; ++caseIdx) {
            Matrix m{};
            for (int t = 0; t < 16; ++t)
                m.m[t] = static_cast<float>(static_cast<int32_t>(rng.Next() % 21u) - 10);
            RunOne(st, theirs, negative, m, 5, 0);
        }
    }

    printf("[InverseProbe] mode=%s cases=%llu divergences=%llu negFired=%llu\n",
           negative ? "NEG" : "POS",
           (unsigned long long)st.cases,
           (unsigned long long)st.diverged,
           (unsigned long long)st.negFired);
    printf("[InverseProbe]   EFFECTIVENESS: dividePath=%llu identityPath=%llu "
           "nearSingular=%llu allFinite=%llu\n",
           (unsigned long long)st.dividePath,
           (unsigned long long)st.identityPath,
           (unsigned long long)st.nearSingular,
           (unsigned long long)st.allFinite);
    printf("[InverseProbe]   sweep contiguity: longest run of ADJACENT differing "
           "steps = %d\n", st.sweepRunLongest);
    if (st.haveFirst) {
        printf("[InverseProbe]   first difference: family=%d param=%g\n",
               st.firstFamily, st.firstParam);
    }
    if (negative) {
        if (st.negFired == 0) {
            printf("[InverseProbe]   *** NEGATIVE CONTROL DID NOT FIRE -- this probe "
                   "proves NOTHING. A moved term in every three-term sum produced ZERO "
                   "byte differences, so the corpus cannot distinguish groupings and a "
                   "clean POS run is worthless. ***\n");
        } else {
            printf("[InverseProbe]   negative control FIRED on %llu of %llu cases "
                   "(%.2f%%) -- the corpus is sensitive to ASSOCIATION alone.\n",
                   (unsigned long long)st.negFired, (unsigned long long)st.cases,
                   100.0 * (double)st.negFired / (double)(st.cases ? st.cases : 1));
        }
    } else if (st.identityPath == 0) {
        printf("[InverseProbe]   *** the det==0 IDENTITY branch was never taken -- "
               "that branch is UNTESTED by this run whatever the divergence count says. ***\n");
    }
}

}  // namespace

// ============================================================================
// Registration. Called from RtMathPortInit() AFTER the BANISHED_RT_MATH check,
// so the whole Math namespace stays behind one switch.
// ============================================================================
int RtMathWave9LaneGfxrRegister() {
    int count = 0;
    RegisterRuntimeExport("?Inverse@Matrix@Math@@QEBA?AV12@XZ", (void*)&RtMath_Matrix_Inverse); ++count;
    // Retail's own entry point, so a by-ADDRESS door cannot walk past the port
    // into the DLL (reference_bypassed_port_needs_a_source_checker.md). There
    // is no such door in the tree today; this is the cheap half of stopping one
    // from being written.
    RegisterRuntimeAddr(0x1800bb310, (void*)&RtMath_Matrix_Inverse);

    const char* probe = std::getenv("BANISHED_RT_INVERSE_PROBE");
    if (probe && std::strcmp(probe, "0") != 0) {
        RunInverseProbe(std::strcmp(probe, "neg") == 0);
    }
    return count;
}
