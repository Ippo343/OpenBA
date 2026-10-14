// ============================================================================
// src/game/Game/worldgen_cells.h -- the per-cell loops of e5d0 and cf440 (block WGPAR)
// ============================================================================
// The cell loops of the base heightmap (e5d0, 0x1400ce5d0) and the blend / splat
// texture (cf440, 0x1400cf440), moved out of worldgen.cpp unchanged except that each
// runs its rows through System::ParallelFor (system/parallel_for.h). The callers in
// worldgen.cpp keep every RNG draw, every allocation and the Perlin construction, in
// retail's order; these functions draw nothing. Each says why its split is exact.
// A separate TU so the unit tests (tests/unit/wg_parallel_test.cpp) link the real
// loops against a sequential reference. docs/extensions/handins/WGPAR.md
// ============================================================================
#pragma once

#include <cstdint>

namespace RtUtility { struct PerlinNoise; }

// e5d0's loop: fills the (dim+1)^2 float heightmap from the two noise fields.
void WgCells_BaseHeightmap(float* heightmap, int dim,
                           const RtUtility::PerlinNoise* pn1, const RtUtility::PerlinNoise* pn2);

// cf440's three passes over the dim x dim texels. `scratch` holds dim*dim records of
// 6 floats {x, y, h, nx, ny, nz}; `out` dim*dim RGBA bytes. Call in this order.
void WgCells_BlendScratch(const float* hm, int dim, float* scratch);
void WgCells_BlendRockGrass(const float* scratch, int dim, const RtUtility::PerlinNoise* pn, uint8_t* out);
void WgCells_BlendOcclusion(const float* scratch, int dim, uint8_t* out);
