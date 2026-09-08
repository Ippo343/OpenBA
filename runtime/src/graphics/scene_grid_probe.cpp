// ============================================================================
// runtime/src/graphics/scene_grid_probe.cpp — WAVE 9 LANE GFX3's oracle
// ============================================================================
//
// WHY A PROBE AND NOT A LIVE DIFF
// -------------------------------
// All six bodies in scene_grid.cpp mutate the ONE shared grid and three of them
// destroy something. Running ours and then the shipped body at a live call site
// threads the same node twice, pops the same free id twice, and starts the
// second run from the state the first left — a different experiment whose first
// product is a corrupted allocator.
//
// So: SYNTHETIC SUBJECTS. Two private `Graphics::Scene`s built from the same
// script, ours through the ported bodies and theirs through the shipped ones,
// compared field by field. Both subjects point at the SAME geometry blobs, so
// `SceneNode::_object` is directly comparable rather than excluded.
//
// AND COMPARED IN LOCKSTEP, NOT AT THE END. Lane PICK: "a defect that cancels
// out — push a link on the free list in the wrong order, pop it back in the
// wrong order — compares equal at the end while every query in between saw a
// different grid." Each of the 18 indexed steps is applied to both subjects and
// the WHOLE scene is compared immediately.
//
// THE SHIPPED BODIES COME THROUGH ResolveRuntimeRaw, NEVER ResolveAddr.
// `ResolveAddr(Module::Runtime, …)` consults the port registry FIRST
// (reference_resolveaddr_consults_port_registry_first.md) and this lane
// registers all six of these addresses, so ResolveAddr would hand back OUR
// function and the probe would compare ours to ours and always agree.
//
// THE POPULATION IS THE POINT
// ---------------------------
// A ported inline `strcmp` survived four waves of green oracles because 13 of
// the 14 values in its population were the same value
// (reference_uniform_population_is_not_a_verification.md). A one-object-per-cell
// script here would pass while proving nothing, so the script deliberately
// builds:
//
//   * FOUR objects in ONE level-7 cell, so the ring has a genuine MIDDLE node
//     and the three unthread arms (head / middle / lone-and-emptied) are all
//     reached. The lone arm is the one that exercises the second head fix, and
//     that is the arm a null-terminated reading of the chain would make dead —
//     the missing-erase shape that cost this project a teardown 100%-CPU hang
//     and an `IsA` execute crash (reference_workarea_bst_bad_entry.md);
//   * objects at THREE different quadtree levels (0, 1, 5 and 7), because the
//     level is where the float half of GridReindex shows up and a single-level
//     population never varies `_levelOffsets[level]` or `dim`;
//   * a re-index that does NOT move (the early-out arm, where `calls` is not
//     coverage) and one that does;
//   * a NON-EMPTY free list before three of the adds, so GridAdd's reuse arm
//     runs; the append arm ran seven times before it.
//
// The counters below say how many times each of those arms actually ran and a
// zero is reported as a GAP in as many words, not left to be inferred from a
// green line (reference_probe_needs_an_effectiveness_control.md). They are
// computed from the subject state BEFORE each call, using the same predicate
// the body uses — a re-derivation, which is fine for a COVERAGE counter and
// would not be for a verdict.
//
// WHAT IS NOT COMPARED, AND WHY
// -----------------------------
// reference_oracle_must_not_compare_unwritten_bytes.md: an oracle that memcmps
// bytes NEITHER side writes fails 100% of calls and reads like a catastrophic
// port defect. Two windows here, both derived from the disassembly rather than
// discovered from a failure:
//
//   * `SceneNode::_pad14` (+0x14). BOTH of GridAdd's arms build the
//     `{_cell, ?}` pair as one qword out of a stack slot whose high dword is
//     never written (`MOV dword [RSP+0x30],0xffffffff` then
//     `MOV RAX,qword [RSP+0x30]`, nothing writing [RSP+0x34]). Retail copies
//     uninitialised stack into it, every time.
//   * every Array's `_data` POINTER. Two independent allocations, two
//     addresses. The `_size`, `_capacity` and full CONTENTS are compared
//     instead — and the contents only out to `_size`, because `_capacity` never
//     shrinks and the slack holds allocator fill that differs after a realloc.
//
// PORTABILITY: no <windows.h>, per PLAN.md §4.1 decision 1.
// ============================================================================

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>

#include "../../funcs/rt_port.h"
#include "scene_grid.h"

// Byte-identical to standalone/resolve.h's declaration; the linker checks them.
uintptr_t ResolveRuntimeRaw(uintptr_t ghidra_addr);

namespace rtcoll {
void* MemAlloc(uint64_t bytes, int align);
void  MemFree(void* p);
}

namespace {

using rtgfxgrid::Scene;
using rtgfxgrid::SceneNode;
using rtgfxgrid::Geometry;
using rtgfxgrid::GlobalList;
using rtgfxgrid::GlobalNode;

enum Mode { kOff, kOn, kNeg };

Mode ProbeMode() {
    const char* e = std::getenv("BANISHED_RT_GFXGRID_PROBE");
    if (!e || !*e) return kOff;
    if (std::strcmp(e, "0") == 0 || std::strcmp(e, "false") == 0) return kOff;
    if (std::strcmp(e, "neg") == 0) return kNeg;
    return kOn;
}

// ---------------------------------------------------------------------------
// The shipped bodies. Raw door only.
// ---------------------------------------------------------------------------
struct Dll {
    void*    (*init)(Scene*, float)                 = nullptr;   // 0x18005e4a0
    uint64_t (*add)(Scene*, Geometry* const*)       = nullptr;   // 0x18005e5e0
    void     (*reindex)(Scene*, int32_t)            = nullptr;   // 0x18005ee30
    void     (*unthread)(Scene*, int32_t)           = nullptr;   // 0x18005f090
    void     (*thread)(Scene*, int32_t)             = nullptr;   // 0x18005f4b0
    void     (*freeGlobals)(GlobalList*)            = nullptr;   // 0x18005f1f0
    // The free-id push both subjects share, so it can never be the reason a
    // comparison fails: Array<uint32>::SetSize(n, &fill), 0x180023830.
    void     (*pushU32)(void*, int, const uint32_t*) = nullptr;
    bool ok = false;
};

Dll ResolveDll() {
    Dll d;
    d.init    = reinterpret_cast<void* (*)(Scene*, float)>(ResolveRuntimeRaw(0x18005e4a0));
    d.add     = reinterpret_cast<uint64_t (*)(Scene*, Geometry* const*)>(
                    ResolveRuntimeRaw(0x18005e5e0));
    d.reindex = reinterpret_cast<void (*)(Scene*, int32_t)>(ResolveRuntimeRaw(0x18005ee30));
    d.unthread= reinterpret_cast<void (*)(Scene*, int32_t)>(ResolveRuntimeRaw(0x18005f090));
    d.thread  = reinterpret_cast<void (*)(Scene*, int32_t)>(ResolveRuntimeRaw(0x18005f4b0));
    d.freeGlobals = reinterpret_cast<void (*)(GlobalList*)>(ResolveRuntimeRaw(0x18005f1f0));
    d.pushU32 = reinterpret_cast<void (*)(void*, int, const uint32_t*)>(
                    ResolveRuntimeRaw(0x180023830));
    d.ok = d.init && d.add && d.reindex && d.unthread && d.thread &&
           d.freeGlobals && d.pushU32;
    return d;
}

// ---------------------------------------------------------------------------
// A synthetic geometry. Nothing in this lane reads a Geometry through anything
// but its `_worldBounds` at +0x4c — no vtable, no factory, no type id — so the
// subject is a blob with that field at that offset
// (reference_synthetic_subject_beats_a_fake.md). BOTH scenes point at the same
// blobs, which is what makes `SceneNode::_object` comparable.
// ---------------------------------------------------------------------------
struct FakeGeo {
    uint8_t opaque[0x4c];
    float   world[6];      // +0x4c  {minx,miny,minz,maxx,maxy,maxz}
    uint8_t tail[0x18];
};
static_assert(offsetof(FakeGeo, world) == 0x4c, "FakeGeo::_worldBounds @ +0x4c");

void SetBox(FakeGeo& g, float cx, float cy, float half) {
    g.world[0] = cx - half; g.world[1] = cy - half; g.world[2] = -1.0f;
    g.world[3] = cx + half; g.world[4] = cy + half; g.world[5] =  1.0f;
}

// Ten geometries. The first four share ONE level-7 cell (centres 0,1,2,3 with
// half-extent 1 land in the same 16-unit cell once the +1024 half-cell shift is
// applied), which is what gives the ring a middle node.
struct { float cx, cy, half; } kGeo[10] = {
    {   0.0f,    0.0f,    1.0f },   // 0  level 7, cell A
    {   1.0f,    1.0f,    1.0f },   // 1  level 7, cell A   (moved away at step 9)
    {   2.0f,    2.0f,    1.0f },   // 2  level 7, cell A
    {   3.0f,    3.0f,    1.0f },   // 3  level 7, cell A
    {   0.0f,    0.0f, 1000.0f },   // 4  level 1
    {   0.0f,    0.0f, 2500.0f },   // 5  level 0 (root cell)
    { 300.0f, -200.0f,    1.0f },   // 6  level 7, its own cell
    {-500.0f,  700.0f,    4.0f },   // 7  level 7 after the clamp
    {   0.0f,    0.0f,   60.0f },   // 8  level 5
    { 900.0f,  900.0f,    1.0f },   // 9  level 7, its own cell
};
FakeGeo g_geo[10];

// Step 9 mutates a geometry box. Expressed as "set the ABSOLUTE box for this
// step index" rather than "move it", so every replay of the script (each
// negative control runs it again) sees exactly the same inputs.
void BuildGeometryForStep(int step);

void BuildGeometry() {
    std::memset(g_geo, 0, sizeof(g_geo));
    for (int i = 0; i < 10; ++i) SetBox(g_geo[i], kGeo[i].cx, kGeo[i].cy, kGeo[i].half);
}

// ---------------------------------------------------------------------------
// Counters — the effectiveness control.
// ---------------------------------------------------------------------------
struct Totals {
    int comparisons  = 0;
    int mismatches   = 0;
    int addAppend    = 0;
    int addReuse     = 0;
    int threadEmpty  = 0;
    int threadRing   = 0;
    int unthreadHead = 0;
    int unthreadMid  = 0;
    int unthreadEmpt = 0;
    int reindexFirst = 0;
    int reindexMoved = 0;
    int reindexNoop  = 0;
    int maxChain     = 0;
    int levelsSeen   = 0;   // bitmask popcount of distinct levels used
    uint32_t levelMask = 0;
};
Totals g_t;
int g_negFlagged = 0, g_negRuns = 0;

bool Fail(const char* step, const char* term, long long a, long long b) {
    ++g_t.mismatches;
    std::printf("[GfxGridProbe] MISMATCH step=%s term=%s ours=%lld theirs=%lld\n",
                step, term, a, b);
    return false;
}

// ---------------------------------------------------------------------------
// The comparator. Reports the FIRST failing term BY NAME with its two values —
// a shadow verdict is only as good as its message
// (reference_shadow_predicate_must_name_its_failing_term.md).
// ---------------------------------------------------------------------------
bool Compare(const char* step, const Scene& a, const Scene& b) {
    ++g_t.comparisons;

#define CMP(TERM, EA, EB)                                                      \
    do {                                                                       \
        const long long va__ = (long long)(EA);                                \
        const long long vb__ = (long long)(EB);                                \
        if (va__ != vb__) return Fail(step, TERM, va__, vb__);                 \
    } while (0)

    uint32_t ab, bb;
    std::memcpy(&ab, &a._cellSize, 4); std::memcpy(&bb, &b._cellSize, 4);
    CMP("_cellSize(bits)", ab, bb);
    std::memcpy(&ab, &a._halfCellX, 4); std::memcpy(&bb, &b._halfCellX, 4);
    CMP("_halfCellX(bits)", ab, bb);
    std::memcpy(&ab, &a._halfCellY, 4); std::memcpy(&bb, &b._halfCellY, 4);
    CMP("_halfCellY(bits)", ab, bb);
    CMP("_levelCount", a._levelCount, b._levelCount);

    CMP("_nodes._size",     a._nodes._size,     b._nodes._size);
    CMP("_nodes._capacity", a._nodes._capacity, b._nodes._capacity);
    for (int i = 0; i < a._nodes._size; ++i) {
        const SceneNode& na = a._nodes._data[i];
        const SceneNode& nb = b._nodes._data[i];
        // +0x14 EXCLUDED — see the banner.
        if (na._object != nb._object)
            return Fail(step, "node._object", (long long)(intptr_t)na._object,
                        (long long)(intptr_t)nb._object);
        if (na._next != nb._next) return Fail(step, "node._next", na._next, nb._next);
        if (na._prev != nb._prev) return Fail(step, "node._prev", na._prev, nb._prev);
        if (na._cell != nb._cell) return Fail(step, "node._cell", na._cell, nb._cell);
    }

    CMP("_freeIds._size",     a._freeIds._size,     b._freeIds._size);
    CMP("_freeIds._capacity", a._freeIds._capacity, b._freeIds._capacity);
    for (int i = 0; i < a._freeIds._size; ++i)
        if (a._freeIds._data[i] != b._freeIds._data[i])
            return Fail(step, "freeIds[i]", a._freeIds._data[i], b._freeIds._data[i]);

    CMP("_cellHeads._size",     a._cellHeads._size,     b._cellHeads._size);
    CMP("_cellHeads._capacity", a._cellHeads._capacity, b._cellHeads._capacity);
    for (int i = 0; i < a._cellHeads._size; ++i)
        if (a._cellHeads._data[i] != b._cellHeads._data[i])
            return Fail(step, "cellHeads[i]", a._cellHeads._data[i], b._cellHeads._data[i]);

    CMP("_levelOffsets._size", a._levelOffsets._size, b._levelOffsets._size);
    for (int i = 0; i < a._levelOffsets._size; ++i)
        if (a._levelOffsets._data[i] != b._levelOffsets._data[i])
            return Fail(step, "levelOffsets[i]", a._levelOffsets._data[i],
                        b._levelOffsets._data[i]);

    CMP("_cellCounts._size", a._cellCounts._size, b._cellCounts._size);
    for (int i = 0; i < a._cellCounts._size; ++i)
        if (a._cellCounts._data[i] != b._cellCounts._data[i])
            return Fail(step, "cellCounts[i]", a._cellCounts._data[i],
                        b._cellCounts._data[i]);

    CMP("_globals._count", a._globals._count, b._globals._count);
#undef CMP
    return true;
}

// ---------------------------------------------------------------------------
// The op table. `ours` is the six ports; `theirs` is the six shipped bodies;
// each negative control is `ours` with exactly ONE slot replaced.
// ---------------------------------------------------------------------------
struct Ops {
    void*    (*init)(Scene*, float);
    uint64_t (*add)(Scene*, Geometry* const*);
    void     (*reindex)(Scene*, int32_t);
    void     (*unthread)(Scene*, int32_t);
    void     (*thread)(Scene*, int32_t);
};

void*    OurInit(Scene* s, float c)             { return rtgfxgrid::GridInit(s, c); }
uint64_t OurAdd(Scene* s, Geometry* const* g)   { return rtgfxgrid::GridAdd(s, g); }
void     OurReindex(Scene* s, int32_t id)       { rtgfxgrid::GridReindex(s, id); }
void     OurUnthread(Scene* s, int32_t id)      { rtgfxgrid::GridUnthread(s, id); }
void     OurThread(Scene* s, int32_t id)        { rtgfxgrid::GridThread(s, id); }

// ---- the six negative controls --------------------------------------------
//
// ONE PER PORTED BODY, and that is deliberate: lane GFX2's first negative
// control perturbed one input, went red on one probe site, and left two others
// GREEN in the very run that was supposed to establish they are not vacuous.
// "A negative control that does not perturb every site it is supposed to cover
// is a partial control that reads like a complete one."
//
// Each defect is one this project has actually shipped or nearly shipped, not a
// random corruption:
//   N1 init      one wrong level offset      (a transcribed constant table)
//   N2 add       pop the free list from the FRONT (lane PICK's "cancels out")
//   N3 unthread  drop the lone-node head clear (THE MISSING ERASE)
//   N4 thread    make the new node the head instead of the ring tail
//   N5 reindex   swap x and y in the Morton interleave
//   N6 free      free only the first node of the list

void* BadInit(Scene* s, float c) {
    void* r = rtgfxgrid::GridInit(s, c);
    s->_levelOffsets._data[7] = 0x1554;                 // was 0x1555
    return r;
}

uint64_t BadAdd(Scene* s, Geometry* const* g) {
    const int32_t n = s->_freeIds._size;
    if (n <= 0) return rtgfxgrid::GridAdd(s, g);
    // Pop the FRONT instead of the back, then shuffle down — the free list ends
    // the script the same length with the same contents in a different order.
    const int32_t id = static_cast<int32_t>(s->_freeIds._data[0]);
    for (int i = 1; i < n; ++i) s->_freeIds._data[i - 1] = s->_freeIds._data[i];
    s->_freeIds._size = n - 1;
    SceneNode* nd = &s->_nodes._data[id];
    nd->_object = *g; nd->_next = 0; nd->_prev = 0; nd->_cell = -1;
    rtgfxgrid::GridReindex(s, id);
    return static_cast<uint64_t>(static_cast<uint32_t>(id));
}

void BadUnthread(Scene* s, int32_t id) {
    SceneNode* nodes = s->_nodes._data;
    int32_t* heads = s->_cellHeads._data;
    int32_t* slot = &heads[nodes[id]._cell];
    if (*slot == id) *slot = nodes[id]._next;
    nodes[nodes[id]._prev]._next = nodes[id]._next;
    nodes[nodes[id]._next]._prev = nodes[id]._prev;
    nodes[id]._prev = -1;
    nodes[id]._next = -1;
    /* THE DEFECT: the lone-node head clear at 0x18005f11e..123 is missing, so a
       cell whose last node leaves keeps a head pointing at a dead node. */
    int32_t cell = nodes[id]._cell;
    --s->_cellCounts._data[cell];
    while (cell != 0) { cell = (cell - 1) >> 2; --s->_cellCounts._data[cell]; }
}

void BadThread(Scene* s, int32_t id) {
    SceneNode* nodes = s->_nodes._data;
    int32_t* heads = s->_cellHeads._data;
    const int32_t cell = nodes[id]._cell;
    const int32_t head = heads[cell];
    if (head >= 0) {
        nodes[id]._prev = nodes[head]._prev;
        nodes[id]._next = head;
        nodes[nodes[id]._prev]._next = id;
        nodes[nodes[id]._next]._prev = id;
        heads[cell] = id;            /* THE DEFECT: retail leaves the head alone */
    } else {
        heads[cell] = id; nodes[id]._next = id; nodes[id]._prev = id;
    }
    int32_t c = cell;
    ++s->_cellCounts._data[c];
    while (c != 0) { c = (c - 1) >> 2; ++s->_cellCounts._data[c]; }
}

void BadReindex(Scene* s, int32_t id) {
    // The whole body, with gx and gy swapped in the interleave only. Everything
    // before and after is the port's own arithmetic, so a red here is about the
    // CELL INDEX and nothing else.
    SceneNode* const nodes = s->_nodes._data;
    const uint8_t* geo = reinterpret_cast<const uint8_t*>(nodes[id]._object);
    float minX, minY, maxX, maxY;
    std::memcpy(&minX, geo + 0x4c, 4); std::memcpy(&minY, geo + 0x50, 4);
    std::memcpy(&maxX, geo + 0x58, 4); std::memcpy(&maxY, geo + 0x5c, 4);
    const float cx = (minX + maxX) * 0.5f, cy = (minY + maxY) * 0.5f;
    const float hx = (maxX - minX) * 0.5f, hy = (maxY - minY) * 0.5f;
    float half = (hx > hy) ? hx : hy;
    const float x = cx + s->_halfCellX, y = cy + s->_halfCellY;
    half += 0.001f;
    const float ratio = s->_cellSize / half;
    uint32_t bits; std::memcpy(&bits, &ratio, 4);
    int32_t level = static_cast<int32_t>(bits >> 23) - 0x7f;
    if (level >= s->_levelCount) level = s->_levelCount - 1;
    if (level < 0) level = 0;
    const int32_t dim = static_cast<int32_t>(1u << level);
    const float inv = 1.0f / (s->_cellSize / static_cast<float>(dim));
    int32_t gx = static_cast<int32_t>(inv * x), gy = static_cast<int32_t>(inv * y);
    if (gx < 0) gx = 0;
    if (gy < 0) gy = 0;
    if (gx >= dim) gx = dim - 1;
    if (gy >= dim) gy = dim - 1;
    const uint32_t ux = static_cast<uint32_t>(gy), uy = static_cast<uint32_t>(gx); // SWAPPED
    const uint32_t x1 = ((ux << 8) | ux) & 0x00ff00ffu;
    const uint32_t x2 = ((x1 << 4) | x1) & 0x0f0f0f0fu;
    const uint32_t x3 = ((x2 << 2) | x2) & 0x33333333u;
    const uint32_t y1 = ((uy << 8) | uy) & 0x00ff00ffu;
    const uint32_t y2 = ((y1 << 4) | y1) & 0x0f0f0f0fu;
    const uint32_t y3 = ((y2 << 2) | y2) & 0x33333333u;
    const uint32_t a  = ((y3 << 1) | y3) << 1;
    const uint32_t cell = ((((a ^ x3) & 0x55555555u) ^ a) | ((x3 & 0x2aaaaaaau) << 1))
                          + static_cast<uint32_t>(s->_levelOffsets._data[level]);
    const int32_t was = nodes[id]._cell;
    if (was == static_cast<int32_t>(cell)) return;
    if (was >= 0) rtgfxgrid::GridUnthread(s, id);
    nodes[id]._cell = static_cast<int32_t>(cell);
    rtgfxgrid::GridThread(s, id);
}

// ---------------------------------------------------------------------------
// The runner.
// ---------------------------------------------------------------------------
const int kStepCount = 18;

const char* StepName(int s) {
    switch (s) {
        case 0:  return "init";
        case 8:  return "reindex-noop";
        case 9:  return "reindex-moved";
        case 10: return "unthread+thread";
        case 11: return "remove-middle";
        case 12: return "remove-head";
        case 13: return "remove-lone";
        case 17: return "reindex-all";
        default: return (s < 8) ? "add" : "add-reuse";
    }
}

struct Runner {
    Scene*   s = nullptr;
    Ops      ops{};
    const Dll* dll = nullptr;
    uint32_t ids[10] = {0};
    bool     live[10] = {false};
};

// How long is the ring node `id` sits in? Used only as a coverage counter.
int RingLength(const Scene& s, int32_t id) {
    if (id < 0 || id >= s._nodes._size) return 0;
    const int32_t cell = s._nodes._data[id]._cell;
    if (cell < 0 || cell >= s._cellHeads._size) return 0;
    const int32_t head = s._cellHeads._data[cell];
    if (head < 0) return 0;
    int n = 0; int32_t w = head;
    do { ++n; w = s._nodes._data[w]._next; } while (w != head && n < 64);
    return n;
}

void Remove(Runner& r, int slot, bool stats) {
    const int32_t id = static_cast<int32_t>(r.ids[slot]);
    Scene* s = r.s;
    if (stats) {
        const int32_t cell = s->_nodes._data[id]._cell;
        const bool wasHead = (s->_cellHeads._data[cell] == id);
        const bool lone    = (s->_nodes._data[id]._next == id);
        if (lone)         ++g_t.unthreadEmpt;
        else if (wasHead) ++g_t.unthreadHead;
        else              ++g_t.unthreadMid;
        const int len = RingLength(*s, id);
        if (len > g_t.maxChain) g_t.maxChain = len;
    }
    r.ops.unthread(s, id);
    SceneNode* nd = &s->_nodes._data[id];
    nd->_object = nullptr; nd->_next = 0; nd->_prev = 0; nd->_cell = -1;
    const uint32_t v = static_cast<uint32_t>(id);
    // The free-id push is the SAME shipped body on every subject, so it can
    // never be the reason a comparison fails.
    r.dll->pushU32(&s->_freeIds, s->_freeIds._size + 1, &v);
    r.live[slot] = false;
}

void Add(Runner& r, int slot, bool stats) {
    if (stats) {
        if (r.s->_freeIds._size > 0) ++g_t.addReuse; else ++g_t.addAppend;
    }
    Geometry* g = reinterpret_cast<Geometry*>(&g_geo[slot]);
    r.ids[slot] = static_cast<uint32_t>(r.ops.add(r.s, &g));
    r.live[slot] = true;
    if (stats) {
        const int32_t id = static_cast<int32_t>(r.ids[slot]);
        const int32_t cell = r.s->_nodes._data[id]._cell;
        // Which level did that cell come from? The offsets table names it.
        for (int L = r.s->_levelOffsets._size - 1; L >= 0; --L)
            if (cell >= r.s->_levelOffsets._data[L]) { g_t.levelMask |= (1u << L); break; }
        const int len = RingLength(*r.s, id);
        if (len > g_t.maxChain) g_t.maxChain = len;
        if (len == 1) ++g_t.threadEmpty; else ++g_t.threadRing;
        ++g_t.reindexFirst;
    }
}

void RunStep(Runner& r, int step, bool stats) {
    if (step == 0) { r.ops.init(r.s, 2048.0f); return; }
    if (step <= 7) { Add(r, step - 1, stats); return; }

    if (step == 8) {                       // re-index with the box UNCHANGED
        if (stats) ++g_t.reindexNoop;
        r.ops.reindex(r.s, static_cast<int32_t>(r.ids[1]));
        return;
    }
    if (step == 9) {                       // move it, then re-index
        SetBox(g_geo[1], 500.0f, 500.0f, 1.0f);
        if (stats) ++g_t.reindexMoved;
        r.ops.reindex(r.s, static_cast<int32_t>(r.ids[1]));
        return;
    }
    if (step == 10) {                      // drive unthread and thread directly
        const int32_t id = static_cast<int32_t>(r.ids[2]);
        if (stats) {
            const int32_t cell = r.s->_nodes._data[id]._cell;
            const bool wasHead = (r.s->_cellHeads._data[cell] == id);
            const bool lone    = (r.s->_nodes._data[id]._next == id);
            if (lone) ++g_t.unthreadEmpt; else if (wasHead) ++g_t.unthreadHead;
            else ++g_t.unthreadMid;
        }
        r.ops.unthread(r.s, id);
        if (stats) {
            const int32_t cell = r.s->_nodes._data[id]._cell;
            if (r.s->_cellHeads._data[cell] < 0) ++g_t.threadEmpty; else ++g_t.threadRing;
        }
        r.ops.thread(r.s, id);
        return;
    }
    if (step == 11) { Remove(r, 3, stats); return; }   // middle of the ring
    if (step == 12) { Remove(r, 0, stats); return; }   // the ring's head
    if (step == 13) { Remove(r, 2, stats); return; }   // the last node in the cell
    if (step <= 16) { Add(r, step - 7, stats); return; }   // slots 7, 8, 9 — reuse arm

    for (int i = 0; i < 10; ++i)                        // step 17
        if (r.live[i]) {
            if (stats) ++g_t.reindexNoop;
            r.ops.reindex(r.s, static_cast<int32_t>(r.ids[i]));
        }
}

// ---------------------------------------------------------------------------
// Subject storage and teardown.
// ---------------------------------------------------------------------------
void Destroy(Scene& s) {
    if (s._nodes._data)        rtcoll::MemFree(s._nodes._data);
    if (s._freeIds._data)      rtcoll::MemFree(s._freeIds._data);
    if (s._cellHeads._data)    rtcoll::MemFree(s._cellHeads._data);
    if (s._levelOffsets._data) rtcoll::MemFree(s._levelOffsets._data);
    if (s._cellCounts._data)   rtcoll::MemFree(s._cellCounts._data);
    std::memset(&s, 0, sizeof(s));
}

bool RunScript(Scene& ours, Scene& theirs, const Dll& dll, const Ops& oursOps,
               bool stats, const char* tag) {
    static Runner a, b;
    a = Runner(); b = Runner();
    a.s = &ours;   a.ops = oursOps; a.dll = &dll;
    b.s = &theirs; b.dll = &dll;
    b.ops.init     = dll.init;
    b.ops.add      = dll.add;
    b.ops.reindex  = dll.reindex;
    b.ops.unthread = dll.unthread;
    b.ops.thread   = dll.thread;

    bool ok = true;
    for (int step = 0; step < kStepCount && ok; ++step) {
        BuildGeometryForStep(step);
        RunStep(a, step, stats);
        RunStep(b, step, false);
        char label[64];
        std::snprintf(label, sizeof(label), "%s%02d:%s", tag, step, StepName(step));
        for (int i = 0; i < 10 && ok; ++i)
            if (a.live[i] && a.ids[i] != b.ids[i])
                ok = Fail(label, "returned node id", a.ids[i], b.ids[i]);
        if (ok) ok = Compare(label, ours, theirs);
    }
    return ok;
}

// ---------------------------------------------------------------------------
// FreeGlobals — and the honest statement of what its comparator can and cannot
// see.
//
// Comparing `{head, tail, count} == {0,0,0}` alone CANNOT distinguish "freed
// every node" from "freed the first node and then cleared the header": both
// leave the same post-state. That is exactly the shape of a control that says
// PASS while measuring nothing, so a second observation is added: after the
// call, ask the allocator for three blocks of the node's own size and count how
// many of the three ORIGINAL node addresses come back. A node that was never
// freed cannot be handed out again.
//
// Whether that second observation actually discriminates is not asserted — N6
// below runs a FreeGlobals that frees only the first node, and the report says
// in as many words whether it was flagged. If it was not, this oracle cannot
// fail and the green line above it is worthless.
// ---------------------------------------------------------------------------
struct GlobalFixture {
    GlobalList  list{};
    GlobalNode* nodes[3] = {nullptr, nullptr, nullptr};

    void Build() {
        std::memset(&list, 0, sizeof(list));
        for (int i = 0; i < 3; ++i) {
            nodes[i] = static_cast<GlobalNode*>(rtcoll::MemAlloc(0x18, 8));
            std::memset(nodes[i], 0, 0x18);
            nodes[i]->element = reinterpret_cast<void*>(static_cast<uintptr_t>(i + 1));
        }
        for (int i = 0; i < 3; ++i) {
            nodes[i]->prev = (i > 0) ? nodes[i - 1] : nullptr;
            nodes[i]->next = (i < 2) ? nodes[i + 1] : nullptr;
        }
        list.head = nodes[0]; list.tail = nodes[2]; list.count = 3;
    }

    int Reclaimed() const {
        void* got[3];
        int hits = 0;
        for (int i = 0; i < 3; ++i) got[i] = rtcoll::MemAlloc(0x18, 8);
        for (int i = 0; i < 3; ++i)
            for (int k = 0; k < 3; ++k)
                if (got[i] == static_cast<void*>(nodes[k])) { ++hits; break; }
        for (int i = 0; i < 3; ++i) rtcoll::MemFree(got[i]);
        return hits;
    }
};

void BadFreeGlobals(GlobalList* l) {
    if (l->head) rtcoll::MemFree(l->head);      // ONLY the first node
    l->tail = nullptr; l->head = nullptr; l->count = 0;
}

void BuildGeometryForStep(int step) { if (step == 0) BuildGeometry(); }

}  // namespace

// ============================================================================
void RtGraphicsGridProbe() {
    const Mode mode = ProbeMode();
    if (mode == kOff) return;

    const Dll dll = ResolveDll();
    if (!dll.ok) {
        std::printf("[GfxGridProbe] SKIPPED -- the shipped bodies did not resolve "
                    "(Runtime-x64.dll absent?). This probe is a DLL-PRESENT "
                    "instrument by construction.\n");
        return;
    }

    const Ops oursOps = { &OurInit, &OurAdd, &OurReindex, &OurUnthread, &OurThread };

    static Scene ours, theirs;
    std::memset(&ours, 0, sizeof(ours));
    std::memset(&theirs, 0, sizeof(theirs));
    const bool gridOk = RunScript(ours, theirs, dll, oursOps, true, "");

    // ---- FreeGlobals ------------------------------------------------------
    // *** THE TWO HALVES MUST NOT OVERLAP IN TIME. ***
    // The first version of this ran ours, then theirs, then measured both
    // reclamation counts — and reported `ours=0 theirs=3` on a correct port.
    // The allocator's free list is LIFO, so by the time ours' three blocks were
    // asked for, THEIRS' three frees were in front of them and satisfied the
    // request. The observation is only about the walk if each subject's build,
    // free and re-allocate are consecutive, so they are serialised here. A
    // harness that manufactures its own divergence is this project's standing
    // hazard (reference_longjmp_unwind_runs_retail_cleanup.md), and this is a
    // small instance of it caught by the run rather than by review.
    GlobalFixture fa, fb;
    bool fgPost = true, fgReclaim = true;

    fa.Build();
    rtgfxgrid::FreeGlobals(&fa.list);
    const int reclaimOurs = fa.Reclaimed();

    fb.Build();
    dll.freeGlobals(&fb.list);
    const int reclaimTheirs = fb.Reclaimed();

    ++g_t.comparisons;
    if (fa.list.head != nullptr || fa.list.tail != nullptr || fa.list.count != 0)
        fgPost = Fail("freeGlobals", "post-state {head,tail,count}",
                      (long long)fa.list.count, 0);
    if (reclaimOurs != reclaimTheirs)
        fgReclaim = Fail("freeGlobals", "blocks reclaimed after the walk",
                         reclaimOurs, reclaimTheirs);
    const bool fgOk = fgPost && fgReclaim;

    // ---- the six negative controls ---------------------------------------
    if (mode == kNeg) {
        struct Neg { const char* name; Ops ops; } negs[] = {
            { "N1 init: one wrong level offset",
              { &BadInit, &OurAdd, &OurReindex, &OurUnthread, &OurThread } },
            { "N2 add: free list popped from the front",
              { &OurInit, &BadAdd, &OurReindex, &OurUnthread, &OurThread } },
            { "N3 unthread: lone-node head clear removed (THE MISSING ERASE)",
              { &OurInit, &OurAdd, &OurReindex, &BadUnthread, &OurThread } },
            { "N4 thread: new node made the head",
              { &OurInit, &OurAdd, &OurReindex, &OurUnthread, &BadThread } },
            { "N5 reindex: x and y swapped in the Morton interleave",
              { &OurInit, &OurAdd, &BadReindex, &OurUnthread, &OurThread } },
        };
        for (const auto& n : negs) {
            static Scene bad, ref;
            std::memset(&bad, 0, sizeof(bad));
            std::memset(&ref, 0, sizeof(ref));
            const int before = g_t.mismatches;
            const bool clean = RunScript(bad, ref, dll, n.ops, false, "neg:");
            ++g_negRuns;
            if (!clean) ++g_negFlagged;
            else std::printf("[GfxGridProbe] NEGATIVE CONTROL NOT FLAGGED: %s\n", n.name);
            g_t.mismatches = before;      // a control's failure is not the port's
            Destroy(bad); Destroy(ref);
        }
        // N6 — FreeGlobals, run through the same two observations.
        GlobalFixture nb; nb.Build();
        const int before = g_t.mismatches;
        BadFreeGlobals(&nb.list);
        const bool post = (nb.list.head == nullptr && nb.list.count == 0);
        const int nbReclaim = nb.Reclaimed();
        ++g_negRuns;
        if (!post || nbReclaim != reclaimTheirs) ++g_negFlagged;
        else std::printf("[GfxGridProbe] NEGATIVE CONTROL NOT FLAGGED: "
                         "N6 freeGlobals frees only the first node -- the "
                         "post-state and the reclamation count BOTH agree with "
                         "a correct walk, so THIS ORACLE CANNOT FAIL and the "
                         "FreeGlobals result above is worthless\n");
        g_t.mismatches = before;
    }

    // ---- the report -------------------------------------------------------
    for (int L = 0; L < 8; ++L) if (g_t.levelMask & (1u << L)) ++g_t.levelsSeen;

    std::printf("[GfxGridProbe] comparisons=%d mismatches=%d\n",
                g_t.comparisons, g_t.mismatches);
    std::printf("[GfxGridProbe] work: addAppend=%d addReuse=%d threadEmpty=%d "
                "threadRing=%d\n",
                g_t.addAppend, g_t.addReuse, g_t.threadEmpty, g_t.threadRing);
    std::printf("[GfxGridProbe] work: unthreadHead=%d unthreadMiddle=%d "
                "unthreadEmptied=%d\n",
                g_t.unthreadHead, g_t.unthreadMid, g_t.unthreadEmpt);
    std::printf("[GfxGridProbe] work: reindexFirst=%d reindexMoved=%d "
                "reindexNoop=%d levels=%d maxRing=%d\n",
                g_t.reindexFirst, g_t.reindexMoved, g_t.reindexNoop,
                g_t.levelsSeen, g_t.maxChain);
    // The two observations are reported SEPARATELY. The first version printed
    // one verdict for both and said "post-state MISMATCH" when it was the
    // reclamation count that differed — a verdict is only as good as its
    // message (reference_shadow_predicate_must_name_its_failing_term.md).
    std::printf("[GfxGridProbe] freeGlobals: post-state %s, reclamation %s "
                "(blocks reclaimed ours=%d theirs=%d)\n",
                fgPost ? "clean" : "MISMATCH", fgReclaim ? "agrees" : "MISMATCH",
                reclaimOurs, reclaimTheirs);

    struct { const char* name; int v; } gaps[] = {
        { "addAppend",       g_t.addAppend },
        { "addReuse",        g_t.addReuse },
        { "threadEmpty",     g_t.threadEmpty },
        { "threadRing",      g_t.threadRing },
        { "unthreadHead",    g_t.unthreadHead },
        { "unthreadMiddle",  g_t.unthreadMid },
        { "unthreadEmptied", g_t.unthreadEmpt },
        { "reindexMoved",    g_t.reindexMoved },
        { "reindexNoop",     g_t.reindexNoop },
    };
    for (const auto& g : gaps)
        if (g.v == 0)
            std::printf("[GfxGridProbe] GAP: %s ran ZERO times -- this run "
                        "verified nothing about that path\n", g.name);
    if (g_t.maxChain < 3)
        std::printf("[GfxGridProbe] GAP: no cell ever held three nodes -- the "
                    "MIDDLE-of-ring unthread was never exercised\n");
    if (g_t.levelsSeen < 2)
        std::printf("[GfxGridProbe] GAP: every object landed on ONE quadtree "
                    "level -- the level and offset arithmetic is unverified\n");

    if (mode == kNeg)
        std::printf("[GfxGridProbe] negative controls: %d/%d flagged%s\n",
                    g_negFlagged, g_negRuns,
                    g_negFlagged == g_negRuns
                        ? "  (every body's comparator proven able to fail)"
                        : "  *** SOME COMPARATOR CANNOT FAIL -- see the lines "
                          "above; the green result is worthless for those ***");

    std::printf("[GfxGridProbe] %s\n",
                (g_t.mismatches == 0 && gridOk && fgOk)
                    ? "PASS -- every ported grid body agreed with the shipped one"
                    : "FAIL -- see the MISMATCH lines above");

    Destroy(ours);
    Destroy(theirs);
}
