// ============================================================================
// tests/unit/worldgen_erosion_test.cpp -- EROSION: wg_f390_erosion == cc080, bit for bit
// ============================================================================
// Block EROSION (docs/extensions/handins/EROSION.md) made the droplet erosion stage
// (src/game/Game/worldgen_f390_erosion.cpp, retail 0x1400cc080) cheaper without changing a
// result bit. These tests run the tree's body and the pre-EROSION body (copied below, the
// reference: it lives here only, never in src/) on the same heightfield with the same Random
// seed, and compare the whole heightmap and the whole Random state byte for byte: the Random
// state proves the draws are the same in number and order.
//
// Inputs: value noise with sea (cells <= 0, droplets that stop at once), a ramp (droplets
// that walk to the border ring), heights quantised to a few levels (ties everywhere: the
// first-encountered neighbour must win), and fields salted with -0.0 / +0.0 / denormals /
// +-inf / NaN (not finite heights above ~2^27: see the special-values test); the smallest grid (dim 2) and the no-op arms (no size row, 0 iterations).
// ============================================================================
#include "unit.h"
#include "Game/TerrainHeightfield.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <vector>

namespace {

// ---- the reference: the pre-EROSION wg_f390_erosion, verbatim but for its name ----------
constexpr float kOne = 1.0f, kErodeFactor = 0.0625f, kErodeMaxXfer = 0.8f;

void ReferenceErosion(TerrainHeightfield* hfv) {
    int dim  = hfv->gridSize;
    int edge = dim + 1;
    const TerrainSizeRow* sizeRow = hfv->sizeRow;
    if (!sizeRow) return;
    int iters = sizeRow->erosionIterations;
    if (iters <= 0) {
        return;
    }
    System::Random* rng = &hfv->random;
    for (int it = 0; it < iters; ++it) {
        int col = (int)rng->Rand((uint32_t)(dim - 1)) + 1;
        int row = (int)rng->Rand((uint32_t)(dim - 1)) + 1;
        while (true) {
            float* hm = hfv->heightmap.data;
            int curIdx = row * edge + col;
            float hCur = hm[curIdx];
            if (hCur <= 0.0f) break;
            int bestCol = -1, bestRow = -1;
            int bestIdx = curIdx;
            float bestH = hCur;
            int rowTop = (row - 1) * edge;
            {
                float h = hm[rowTop + col - 1];
                if (h < bestH) { bestCol = col - 1; bestIdx = col - 1 + rowTop; bestRow = row - 1; bestH = h; }
                h = hm[rowTop + col];
                if (h < bestH) { bestIdx = rowTop + col; bestRow = row - 1; bestH = h; bestCol = col; }
                h = hm[rowTop + col + 1];
                if (h < bestH) { bestCol = col + 1; bestIdx = col + 1 + rowTop; bestRow = row - 1; bestH = h; }
            }
            int rowMid = row * edge;
            {
                float h = hm[rowMid + col - 1];
                if (h < bestH) { bestCol = col - 1; bestIdx = col - 1 + rowMid; bestH = h; bestRow = row; }
                h = hm[rowMid + col];
                if (h < bestH) { bestIdx = rowMid + col; bestH = h; bestCol = col; bestRow = row; }
                h = hm[rowMid + col + 1];
                if (h < bestH) { bestCol = col + 1; bestIdx = col + 1 + rowMid; bestH = h; bestRow = row; }
            }
            int rowBot = (row + 1) * edge;
            {
                float h = hm[rowBot + col - 1];
                if (h < bestH) { bestCol = col - 1; bestIdx = col - 1 + rowBot; bestH = h; bestRow = row + 1; }
                h = hm[rowBot + col];
                if (h < bestH) { bestIdx = rowBot + col; bestH = h; bestCol = col; bestRow = row + 1; }
                h = hm[rowBot + col + 1];
                if (h < bestH) { bestIdx = col + 1 + rowBot; bestH = h; bestCol = col + 1; bestRow = row + 1; }
            }
            float t = (hCur - bestH) * kErodeFactor;
            if (t > kErodeMaxXfer) t = kErodeMaxXfer;
            hm[curIdx] = (kOne - t) * hCur + t * bestH;
            hfv->heightmap.data[bestIdx] = (kOne - t) * bestH + t * hCur;
            col = bestCol;
            row = bestRow;
            if (bestIdx == curIdx || col < 1 || row < 1 || col >= dim || row >= dim)
                break;
        }
    }
}

// ---- the harness ----------------------------------------------------------------------------
// The heightfield is built in place and never destroyed: the stage reads gridSize, the
// heightmap, the Random and the size row only, and its Array members stay empty.
union FieldStorage {
    FieldStorage() {}
    ~FieldStorage() {}
    TerrainHeightfield hf;
};

struct Outcome {
    std::vector<float> heights;
    uint8_t random[sizeof(System::Random)];
};

// One run: `body` on a copy of `heights` (dim x dim grid, (dim+1)^2 cells), Random seeded
// with `seed`, `iters` droplets (sizeRow == nullptr when `noRow`).
Outcome Run(void (*body)(TerrainHeightfield*), int dim, const std::vector<float>& heights, uint32_t seed,
            int iters, bool noRow = false) {
    FieldStorage storage;
    TerrainHeightfield* hf = new (&storage.hf) TerrainHeightfield();
    Outcome out;
    out.heights = heights;
    TerrainSizeRow row{};
    row.erosionIterations = iters;
    hf->gridSize = dim;
    hf->heightmap.data = out.heights.data();
    hf->heightmap.size = static_cast<int32_t>(out.heights.size());
    hf->heightmap.capacity = hf->heightmap.size;
    new (&hf->random) System::Random(seed);
    hf->sizeRow = noRow ? nullptr : &row;
    body(hf);
    std::memcpy(out.random, &hf->random, sizeof(out.random));
    return out;
}

// True when both bodies leave the identical heightmap and Random state.
bool SameAsReference(int dim, const std::vector<float>& heights, uint32_t seed, int iters, bool noRow = false) {
    const Outcome ref = Run(&ReferenceErosion, dim, heights, seed, iters, noRow);
    const Outcome now = Run(&wg_f390_erosion, dim, heights, seed, iters, noRow);
    return std::memcmp(ref.heights.data(), now.heights.data(), ref.heights.size() * sizeof(float)) == 0 &&
           std::memcmp(ref.random, now.random, sizeof(ref.random)) == 0;
}

struct Rng {
    uint64_t s;
    uint64_t Next() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return s; }
    uint32_t U32() { return static_cast<uint32_t>(Next() >> 32); }
    float Uniform(float lo, float hi) {
        return lo + (hi - lo) * static_cast<float>(U32() >> 8) * (1.0f / 16777216.0f);
    }
    int Below(int n) { return static_cast<int>(U32() % static_cast<uint32_t>(n)); }
};

float FromBits(uint32_t b) { float f; std::memcpy(&f, &b, 4); return f; }

// Smooth value noise in [-lo, hi]: long downhill walks, valleys, a sea.
std::vector<float> Noise(int dim, uint64_t seed, float scale, float offset) {
    const int edge = dim + 1;
    Rng r{seed};
    const int cells = 8;
    std::vector<float> lattice((cells + 2) * (cells + 2));
    for (float& v : lattice) v = r.Uniform(0.0f, 1.0f);
    std::vector<float> h(static_cast<size_t>(edge) * edge);
    for (int y = 0; y < edge; ++y)
        for (int x = 0; x < edge; ++x) {
            const float fx = static_cast<float>(x) * cells / edge, fy = static_cast<float>(y) * cells / edge;
            const int xi = static_cast<int>(fx), yi = static_cast<int>(fy);
            const float tx = fx - xi, ty = fy - yi;
            const float a = lattice[yi * (cells + 2) + xi], b = lattice[yi * (cells + 2) + xi + 1];
            const float c = lattice[(yi + 1) * (cells + 2) + xi], d = lattice[(yi + 1) * (cells + 2) + xi + 1];
            const float v = (a + (b - a) * tx) + ((c + (d - c) * tx) - (a + (b - a) * tx)) * ty;
            h[static_cast<size_t>(y) * edge + x] = v * scale - offset + r.Uniform(0.0f, 0.5f);
        }
    return h;
}

}  // namespace

UNIT_TEST(erosion_noise_matches_reference) {
    for (uint32_t seed : {1u, 2u, 0xBADC0DEu, 0xFFFFFFFFu}) {
        CHECK(SameAsReference(64, Noise(64, seed * 7919ull + 1, 60.0f, 18.0f), seed, 60000));
        CHECK(SameAsReference(47, Noise(47, seed * 104729ull + 3, 30.0f, 5.0f), seed, 20000));
    }
}

UNIT_TEST(erosion_ramp_walks_to_border_like_reference) {
    const int dim = 96, edge = dim + 1;
    std::vector<float> ramp(static_cast<size_t>(edge) * edge);
    for (int y = 0; y < edge; ++y)
        for (int x = 0; x < edge; ++x) ramp[static_cast<size_t>(y) * edge + x] = 50.0f + 3.0f * x + 1.0f * y;
    CHECK(SameAsReference(dim, ramp, 99u, 3000));
    // The mirrored ramps: every direction of travel, and the 0.8 clamp (steep slopes).
    for (int variant = 0; variant < 4; ++variant) {
        std::vector<float> h(ramp.size());
        for (int y = 0; y < edge; ++y)
            for (int x = 0; x < edge; ++x) {
                const int xx = (variant & 1) ? dim - x : x, yy = (variant & 2) ? dim - y : y;
                h[static_cast<size_t>(y) * edge + x] = 10.0f + 40.0f * xx + 0.25f * yy;
            }
        CHECK(SameAsReference(dim, h, 1000u + variant, 2000));
    }
}

UNIT_TEST(erosion_ties_go_to_the_first_neighbour_like_reference) {
    // Heights on 4 levels: most 3x3 scans see equal neighbours (and plateaus: no move).
    const int dim = 40, edge = dim + 1;
    Rng r{12345};
    for (int round = 0; round < 6; ++round) {
        std::vector<float> h(static_cast<size_t>(edge) * edge);
        for (float& v : h) v = 1.0f + static_cast<float>(r.Below(4));
        CHECK(SameAsReference(dim, h, 77u + round, 20000));
    }
}

UNIT_TEST(erosion_special_values_match_reference) {
    // -0.0 / +0.0 / denormals / +-inf / NaN (both signs, quiet and signalling payloads)
    // salted into a land field: droplets stop on them, tie against them, step onto them.
    const int dim = 33, edge = dim + 1;
    const float specials[] = {
        0.0f, -0.0f, FromBits(0x00000001u), FromBits(0x80000001u), FromBits(0x007fffffu),
        std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(),
        FromBits(0x7fc00000u), FromBits(0xffc00000u), FromBits(0x7f800001u), FromBits(0xff812345u),
        std::numeric_limits<float>::min(), 1e-30f, 1.0e6f};
    // NOT salted: finite heights above ~2^27. There a 1-ulp gap is already > 12.8, so the transfer
    // clamps to 0.8 and two cells can trade a 1-ulp gap back and forth forever: cc080 (and so
    // both bodies) never ends a droplet caught between them. Worldgen heights are tens of units.
    Rng r{777};
    for (int round = 0; round < 40; ++round) {
        std::vector<float> h = Noise(dim, 31337ull + round, 20.0f, -2.0f);   // all land
        const int salt = 1 + r.Below(60);
        for (int i = 0; i < salt; ++i)
            h[static_cast<size_t>(r.Below(edge * edge))] = specials[r.Below(sizeof(specials) / sizeof(specials[0]))];
        CHECK(SameAsReference(dim, h, 5000u + round, 4000));
    }
}

UNIT_TEST(erosion_small_grid_and_noop_arms_match_reference) {
    // dim 2: the one interior cell is every droplet's start.
    std::vector<float> h = {5.0f, 4.0f, 6.0f, 3.0f, 9.0f, 2.0f, 7.0f, 1.0f, 8.0f};
    CHECK(SameAsReference(2, h, 3u, 50));
    // No size row / no iterations: nothing moves, nothing is drawn.
    const std::vector<float> field = Noise(16, 5, 10.0f, 1.0f);
    CHECK(SameAsReference(16, field, 9u, 100, /*noRow=*/true));
    CHECK(SameAsReference(16, field, 9u, 0));
    CHECK(SameAsReference(16, field, 9u, -5));
    const Outcome untouched = Run(&wg_f390_erosion, 16, field, 9u, 0);
    CHECK(std::memcmp(untouched.heights.data(), field.data(), field.size() * sizeof(float)) == 0);
}
