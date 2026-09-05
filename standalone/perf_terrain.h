// standalone/perf_terrain.h -- [PerfX, NON-RETAIL] terrain-type side-car.
//
// The map cell array (Game+0x80) is dim*dim records of 0x30 bytes; the terrain TYPE is
// the byte at +0x29. The per-tick movement code reads exactly that one byte for the
// citizen's current cell (walk-speed / road test), so every citizen costs one cache
// line of a 7.8 MB array per iteration -- a miss in L1/L2 for all of them, and at
// 800x50 that is 40k random 48-byte-stride reads a frame. This keeps a 1-byte-per-cell
// mirror (256 KB on a 512 grid) that the hot readers use instead.
//
// Sync rules (the whole risk of a shadow copy is a stale byte):
//   * The mirror is keyed on the cell-array BASE POINTER and count: a new grid (new
//     game, load-time resize) rebuilds it on the next read.
//   * Every store to a cell's +0x29 calls TerrainNoteWrite(cellPtr) right after (7 sites,
//     grep for the +0x29 stores); bulk rewrites (worldgen classify, stream load) call
//     TerrainInvalidate().
//   * BANISHED_PERF_TERRAIN_VERIFY=1 makes every hot read compare the mirror with the
//     real byte and count/log mismatches -- the proof that the writer list is complete.
// Knob: BANISHED_PERF_TERRAIN_SIDECAR=0 (under the PerfX master switch).
#pragma once
#include <cstdint>

namespace PerfX {

extern uint8_t*       g_terrainSC;        // mirror, count bytes (null = off / not built)
extern const uint8_t* g_terrainSCBase;    // cell array the mirror was built from
extern int            g_terrainSCCount;
extern int            g_terrainVerify;    // BANISHED_PERF_TERRAIN_VERIFY

// Slow path: (re)build for a new grid, verify, or plain raw read when the knob is off.
uint8_t TerrainGetSlow(const uint8_t* cells, int count, int idx);

// Hot read of cells[idx].type. `cells`/`count` = the {data,count} header at Game+0x80.
inline uint8_t TerrainGet(const uint8_t* cells, int count, int idx) {
    if (cells == g_terrainSCBase && (unsigned)idx < (unsigned)g_terrainSCCount && !g_terrainVerify)
        return g_terrainSC[idx];
    return TerrainGetSlow(cells, count, idx);
}

// Call right AFTER storing cellPtr[0x29]; copies the new byte into the mirror.
void TerrainNoteWrite(const uint8_t* cellPtr);
// Bulk rewrite / grid freed: forget the mirror (rebuilt lazily on the next read).
void TerrainInvalidate();
// End-of-run report (verify mode); safe to call any time.
void TerrainReport();

}  // namespace PerfX
