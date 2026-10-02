// standalone/perf_terrain.cpp -- see perf_terrain.h.
#include "perf_terrain.h"
#include "perf_experimental.h"
#include "options.h"
#include <cstdio>
#include <cstdlib>

namespace PerfX {

uint8_t*       g_terrainSC      = nullptr;
const uint8_t* g_terrainSCBase  = nullptr;
int            g_terrainSCCount = 0;
int            g_terrainVerify  = 0;

namespace {
enum { kStride = 0x30, kTypeOff = 0x29 };
uint8_t* s_buf        = nullptr;   // owned allocation, reused across rebuilds
int      s_bufCap     = 0;
int      s_verifyInit = 0;
long     s_rebuilds = 0, s_writes = 0, s_verified = 0, s_mismatch = 0;

void Rebuild(const uint8_t* cells, int count) {
    if (count > s_bufCap) {
        std::free(s_buf);
        s_buf = static_cast<uint8_t*>(std::malloc((size_t)count));  // CAST-OK: win32
        s_bufCap = s_buf ? count : 0;
    }
    if (!s_buf) { g_terrainSC = nullptr; g_terrainSCBase = nullptr; g_terrainSCCount = 0; return; }
    for (int i = 0; i < count; ++i) s_buf[i] = cells[(size_t)i * kStride + kTypeOff];
    g_terrainSC = s_buf; g_terrainSCBase = cells; g_terrainSCCount = count;
    ++s_rebuilds;
    if (g_terrainVerify)
        std::fprintf(stderr, "[PerfX/terrain] rebuilt mirror base=%p count=%d (rebuild #%ld)\n",
                     (const void*)cells, count, s_rebuilds);
}
}  // namespace

uint8_t TerrainGetSlow(const uint8_t* cells, int count, int idx) {
    const uint8_t raw = cells[(size_t)idx * kStride + kTypeOff];
    if (!s_verifyInit) {
        s_verifyInit = 1;
        g_terrainVerify = Opt().perfTerrainVerify ? 1 : 0;
    }
    if (!TerrainSidecar() || !cells || count <= 0 || (unsigned)idx >= (unsigned)count) return raw;
    if (cells != g_terrainSCBase || count != g_terrainSCCount) Rebuild(cells, count);
    if (!g_terrainSC) return raw;
    if (g_terrainVerify) {
        ++s_verified;
        if (g_terrainSC[idx] != raw) {
            ++s_mismatch;
            if (s_mismatch <= 20)
                std::fprintf(stderr, "[PerfX/terrain] MISMATCH idx=%d mirror=%u real=%u (writes=%ld)\n",
                             idx, g_terrainSC[idx], raw, s_writes);
            g_terrainSC[idx] = raw;   // self-heal so one miss is counted once
        }
        return raw;
    }
    return g_terrainSC[idx];
}

void TerrainNoteWrite(const uint8_t* cellPtr) {
    if (!g_terrainSC || !cellPtr) return;
    const uint8_t* base = g_terrainSCBase;
    if (cellPtr < base) return;
    const size_t idx = static_cast<size_t>(cellPtr - base) / kStride;
    if (idx >= (size_t)g_terrainSCCount) return;
    g_terrainSC[idx] = cellPtr[kTypeOff];
    ++s_writes;
}

void TerrainInvalidate() {
    g_terrainSC = nullptr; g_terrainSCBase = nullptr; g_terrainSCCount = 0;
}

void TerrainReport() {
    if (!s_rebuilds && !s_verified) return;
    std::fprintf(stderr, "[PerfX/terrain] rebuilds=%ld writes=%ld verified=%ld mismatches=%ld\n",
                 s_rebuilds, s_writes, s_verified, s_mismatch);
}

}  // namespace PerfX
