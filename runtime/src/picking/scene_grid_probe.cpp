// ============================================================================
// runtime/src/picking/scene_grid_probe.cpp — WAVE 9 LANE PICK's oracle
// ============================================================================
//
// WHY A PROBE AND NOT A LIVE DIFF
// -------------------------------
// Every function this lane ports MUTATES SHARED STATE, and four of the five
// destroy something. Running ours and then the shipped body at a live call site
// frees the same Entry twice, pushes the same slot on the free list twice, and
// starts the second run from the state the first one left — so it is not the
// same experiment and the first thing it produces is a corrupted allocator.
//
// So: SYNTHETIC SUBJECTS. Two private `Picking::Scene`s with byte-identical
// starting state, the SAME script of operations applied to both, ours through
// the ported bodies and theirs through the shipped ones, compared field by
// field after every step. Two scenes, two allocations, two frees — no double
// free, no shared state, and the comparison is against the DLL rather than
// against a restatement of our own rule
// (reference_synthetic_subject_beats_a_fake.md).
//
// THE SHIPPED BODIES ARE REACHED THROUGH ResolveRuntimeRaw, NEVER ResolveAddr.
// `ResolveAddr(Module::Runtime, …)` consults the port registry FIRST
// (reference_resolveaddr_consults_port_registry_first.md), and this lane
// registers all nineteen of these addresses — so ResolveAddr would hand back
// OUR function and the probe would compare ours to ours and always agree. That
// is a recorded failure mode on this project; the Raw door exists for it.
//
// THE POPULATION IS THE POINT
// ---------------------------
// A ported inline `strcmp` returned 0 for every distinct pair sharing a first
// byte and survived FOUR WAVES of green oracles, because 13 of the 14 values in
// the population were the same value
// (reference_uniform_population_is_not_a_verification.md). An all-single-cell,
// empty-free-list run of this probe would pass while proving nothing, so the
// script below deliberately builds:
//
//   * entries whose rectangle spans MANY cells (the mesh boxes are sized so the
//     rectangles are 1x1 up to 5x4) — the multi-cell unlink is the whole risk
//     in GridUnthread and a 1x1 population never exercises the row loop;
//   * cells SHARED by several entries, so a cell's link list has length > 1 and
//     the unlink has a middle node to remove rather than only a head;
//   * a NON-EMPTY free list before every reuse path runs — entries are removed
//     before the second batch is added, so EntryAlloc takes its reuse arm,
//     AddToCells takes its free-link arm, and FindOrCreateCell takes its
//     free-cell arm. The counters below report how many times each of those
//     arms actually ran, and a run where any of them is zero is reported as a
//     GAP, not as a pass;
//   * reindexes that BOTH move and do not move the rectangle, because
//     GridReindex's early-out means `calls` is not coverage (Wave 9 lane M's
//     Finding 1: calls=12896 on a site where ten calls did any work).
//
// WHAT IS NOT COMPARED, AND WHY
// -----------------------------
// reference_oracle_must_not_compare_unwritten_bytes.md: an oracle that memcmps
// bytes NEITHER side writes fails 100% of calls and reads like a catastrophic
// port defect. This lane has four such windows and every one of them was read
// out of a disassembly rather than guessed:
//
//   * `_data` of every Array. Two independent allocations, two addresses. The
//     CONTENTS are compared instead.
//   * Entry +0x14..+0x17. Neither 0x1800735c0 (copy-construct) nor 0x18007b590
//     (operator=) writes those four slack bytes; they hold whatever the
//     allocator or the caller's frame left.
//   * Entry +0x3c (`_userValue2`) OF A FREED ENTRY ONLY. 0x18007b390 builds its
//     zero Entry on the stack and never writes +0x3c, so the assignment copies
//     a dword of the DLL's frame into the entry. On a LIVE entry +0x3c is
//     written by AddMesh and IS compared — the exclusion is per-entry, keyed on
//     `_nextFree != -2`, not blanket. ("Never written" is meaningless without
//     "by WHOM": reference_never_written_needs_by_whom.md.)
//   * The extent past each Array's `_size`. `_capacity` never shrinks, so
//     [size, capacity) holds allocator fill, which differs after a realloc.
//
// CONTROLS
// --------
// EFFECTIVENESS (reference_probe_needs_an_effectiveness_control.md — one
// earlier probe had 64 green comparisons where zero did any work): the summary
// prints, per path, how many times that path ran. `entriesMultiCell`,
// `linkFreeReuse`, `cellFreeReuse`, `entryFreeReuse`, `cellsFreed`,
// `reindexMoved` and `reindexNoop` are the ones that matter; each is named and
// a zero is called out as a GAP in as many words.
//
// NEGATIVE (BANISHED_RT_PICKGRID_PROBE=neg): the same script is run a second
// time with a DELIBERATELY WRONG unlink — `UnlinkEntryFromCell` with the
// free-list push removed, i.e. exactly the "missing erase" defect that produced
// this project's teardown hang and `IsA` EXEC crash. The comparator MUST flag
// it. If the negative control reports zero mismatches, the probe cannot fail
// and the whole green result is worthless — which is a defect an earlier wave's
// negative control found in the PROBE rather than in the port.
//
// PORTABILITY: no <windows.h>, per PLAN.md §4.1 decision 1.
// ============================================================================

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

using rtpickgrid::Scene;
using rtpickgrid::Entry;
using rtpickgrid::Cell;
using rtpickgrid::Link;
using rtpickgrid::AddDesc;
using rtpickgrid::CellIter;

enum Mode { kOff, kOn, kNeg };

Mode ProbeMode() {
    const char* e = std::getenv("BANISHED_RT_PICKGRID_PROBE");
    if (!e || !*e) return kOff;
    if (std::strcmp(e, "0") == 0 || std::strcmp(e, "false") == 0) return kOff;
    if (std::strcmp(e, "neg") == 0) return kNeg;
    return kOn;
}

// ---------------------------------------------------------------------------
// The shipped bodies. Raw door only.
// ---------------------------------------------------------------------------
struct Dll {
    void*    (*boxTransform)(void*, const void*, const void*)                = nullptr;
    void*    (*meshBounds)(const void*, void*)                               = nullptr;
    uint32_t (*gridAdd)(Scene*, const AddDesc*)                              = nullptr;
    void     (*gridReindex)(Scene*, uint32_t)                                = nullptr;
    void     (*gridUnthread)(Scene*, int32_t*, uint16_t, uint16_t, int32_t)  = nullptr;
    void     (*gridFreeEntry)(Scene*, uint32_t)                              = nullptr;
    void     (*cellIterAdvance)(CellIter*)                                   = nullptr;
    void     (*cellHeadsInit)(void*)                                         = nullptr;
    bool ok = false;
};

Dll ResolveDll() {
    Dll d;
    d.boxTransform = reinterpret_cast<void* (*)(void*, const void*, const void*)>(
        ResolveRuntimeRaw(0x18001f470));
    d.meshBounds = reinterpret_cast<void* (*)(const void*, void*)>(
        ResolveRuntimeRaw(0x180125bc0));
    d.gridAdd = reinterpret_cast<uint32_t (*)(Scene*, const AddDesc*)>(
        ResolveRuntimeRaw(0x18007af70));
    d.gridReindex = reinterpret_cast<void (*)(Scene*, uint32_t)>(
        ResolveRuntimeRaw(0x1800724d0));
    d.gridUnthread =
        reinterpret_cast<void (*)(Scene*, int32_t*, uint16_t, uint16_t, int32_t)>(
            ResolveRuntimeRaw(0x180072950));
    d.gridFreeEntry = reinterpret_cast<void (*)(Scene*, uint32_t)>(
        ResolveRuntimeRaw(0x18007b390));
    d.cellIterAdvance = reinterpret_cast<void (*)(CellIter*)>(
        ResolveRuntimeRaw(0x18007b6d0));
    // Array<int>::SetSize(10000) filling with -1 — the Scene ctor's only
    // callee. Used to build BOTH subjects' cell-head tables identically, so a
    // difference in the table is never the reason a comparison fails.
    d.cellHeadsInit = reinterpret_cast<void (*)(void*)>(ResolveRuntimeRaw(0x18007b960));
    d.ok = d.gridAdd && d.gridReindex && d.gridUnthread && d.gridFreeEntry &&
           d.cellIterAdvance && d.cellHeadsInit && d.boxTransform && d.meshBounds;
    return d;
}

// ---------------------------------------------------------------------------
// A synthetic mesh. `Picking::PickingMesh` is an Object::External subclass and
// this probe never constructs a real one — it does not need to. Every body in
// this lane reads exactly two things through the mesh pointer: `_nodes` at
// +0x38 and, through that, each SubMesh's AABB at +0x28 (stride 0x80). So the
// subject is a 0x48-byte blob with those two fields at those offsets and
// nothing else, which is a SYNTHETIC SUBJECT rather than a fake of the class:
// the guard that reaches the AABB reads `_nodes`, so supplying `_nodes` is
// enough and no factory, vtable or type id is involved
// (reference_synthetic_subject_beats_a_fake.md).
// ---------------------------------------------------------------------------
struct FakeMesh {
    uint8_t  opaque[0x38];
    uint8_t* nodes;      // +0x38
    int32_t  nodeCount;  // +0x40
    int32_t  pad;
};
static_assert(sizeof(FakeMesh) == 0x48, "FakeMesh layout");
static_assert(offsetof(FakeMesh, nodes) == 0x38, "FakeMesh::nodes @ +0x38");

// One mesh definition shared by both subjects: N nodes, each with its own AABB.
struct MeshDef {
    FakeMesh  mesh;
    uint8_t   nodeStore[4 * 0x80];
    int       nodeCount;

    void Build(int nodes, float baseX, float baseY, float sizeX, float sizeY) {
        std::memset(&mesh, 0, sizeof(mesh));
        std::memset(nodeStore, 0, sizeof(nodeStore));
        nodeCount = nodes;
        mesh.nodes = nodeStore;
        mesh.nodeCount = nodes;
        for (int i = 0; i < nodes; ++i) {
            float box[6] = {
                baseX + i * 3.0f,        baseY + i * 2.0f,        -1.0f,
                baseX + i * 3.0f + sizeX, baseY + i * 2.0f + sizeY, 1.0f,
            };
            std::memcpy(nodeStore + static_cast<size_t>(i) * 0x80 + 0x28, box, sizeof(box));
        }
    }
};

// ---------------------------------------------------------------------------
// A subject: a Scene laid out exactly as the retail ctor leaves one.
//
// The ctor is FUN_18007aef0(this, 16.0f) and its stores are, in order: +0x00,
// +0x04, +0x08, +0x10, +0x18, +0x20, +0x28 = -1, +0x30, +0x38, +0x40 = -1,
// +0x48, +0x50, +0x58 = -1, +0x5c = 0 — and NOTHING ELSE, so +0x2c and +0x44
// are indeterminate in a live Scene. They are zeroed identically in both
// subjects here so they cannot be the reason a comparison fails, and they are
// excluded from the comparison anyway.
// ---------------------------------------------------------------------------
struct Subject {
    Scene s;

    void Build(const Dll& dll) {
        std::memset(&s, 0, sizeof(s));
        s._cellSize    = 16.0f;
        s._invCellSize = 1.0f / 16.0f;
        s._freeCell    = -1;
        s._freeLink    = -1;
        s._freeEntry   = -1;
        s._queryStamp  = 0;
        dll.cellHeadsInit(&s._cellHeads);   // SetSize(10000), filled -1
    }

    void Destroy() {
        // Free everything this subject owns, in the order the retail dtor does.
        if (s._entries._data) {
            for (int i = 0; i < s._entries._size; ++i) {
                uint8_t* e = reinterpret_cast<uint8_t*>(s._entries._data) +
                             static_cast<size_t>(i) * 0x48;
                void* buf; std::memcpy(&buf, e + 0x20, sizeof(buf));
                if (buf) rtcoll::MemFree(buf);
            }
            rtcoll::MemFree(s._entries._data);
        }
        if (s._links._data)     rtcoll::MemFree(s._links._data);
        if (s._cells._data)     rtcoll::MemFree(s._cells._data);
        if (s._cellHeads._data) rtcoll::MemFree(s._cellHeads._data);
        std::memset(&s, 0, sizeof(s));
    }
};

// ---------------------------------------------------------------------------
// Counters — the effectiveness control.
// ---------------------------------------------------------------------------
struct Totals {
    int comparisons      = 0;
    int mismatches       = 0;
    int adds             = 0;
    int removes          = 0;
    int reindexMoved     = 0;
    int reindexNoop      = 0;
    int unthreads        = 0;
    int iterSteps        = 0;
    int iterHits         = 0;
    int entriesMultiCell = 0;   // adds whose rectangle covered > 1 cell
    int maxLinkChain     = 0;   // longest link list seen in any cell
    int entryFreeReuse   = 0;   // EntryAlloc took its reuse arm
    int linkFreeReuse    = 0;   // AddToCells took its free-link arm
    int cellFreeReuse    = 0;   // FindOrCreateCell took its free-cell arm
    int cellsFreed       = 0;   // FreeEmptyCell actually ran
};
Totals g_t;
int g_negMismatches = 0;
int g_negComparisons = 0;

// ---------------------------------------------------------------------------
// The comparator.
//
// Reports the FIRST failing term BY NAME with its two values. A shadow verdict
// is only as good as its message: an earlier site in this very namespace
// computed a fourteen-term conjunction and printed two hand-picked fields, so a
// failure printed two IDENTICAL strings
// (reference_shadow_predicate_must_name_its_failing_term.md).
// ---------------------------------------------------------------------------
bool Fail(const char* step, const char* term, long long a, long long b) {
    ++g_t.mismatches;
    std::printf("[PickGridProbe] MISMATCH step=%s term=%s ours=%lld theirs=%lld\n",
                step, term, a, b);
    return false;
}

bool Compare(const char* step, const Scene& a, const Scene& b) {
    ++g_t.comparisons;

#define CMP(TERM, EXPR_A, EXPR_B)                                              \
    do {                                                                       \
        const long long va__ = (long long)(EXPR_A);                            \
        const long long vb__ = (long long)(EXPR_B);                            \
        if (va__ != vb__) return Fail(step, TERM, va__, vb__);                 \
    } while (0)

    CMP("_freeCell",   a._freeCell,   b._freeCell);
    CMP("_freeLink",   a._freeLink,   b._freeLink);
    CMP("_freeEntry",  a._freeEntry,  b._freeEntry);
    CMP("_queryStamp", a._queryStamp, b._queryStamp);

    CMP("cellHeads._size", a._cellHeads._size, b._cellHeads._size);
    for (int i = 0; i < a._cellHeads._size; ++i)
        if (a._cellHeads._data[i] != b._cellHeads._data[i])
            return Fail(step, "cellHeads[i]", a._cellHeads._data[i],
                        b._cellHeads._data[i]);

    CMP("cells._size",     a._cells._size,     b._cells._size);
    CMP("cells._capacity", a._cells._capacity, b._cells._capacity);
    for (int i = 0; i < a._cells._size; ++i) {
        const Cell& ca = a._cells._data[i];
        const Cell& cb = b._cells._data[i];
        if (ca._x != cb._x || ca._y != cb._y ||
            ca._nextInBucket != cb._nextInBucket || ca._firstLink != cb._firstLink)
            return Fail(step, "cells[i]",
                        ((long long)ca._x << 32) | (uint32_t)ca._firstLink,
                        ((long long)cb._x << 32) | (uint32_t)cb._firstLink);
    }

    CMP("links._size",     a._links._size,     b._links._size);
    CMP("links._capacity", a._links._capacity, b._links._capacity);
    for (int i = 0; i < a._links._size; ++i) {
        if (a._links._data[i]._entry != b._links._data[i]._entry)
            return Fail(step, "links[i]._entry", a._links._data[i]._entry,
                        b._links._data[i]._entry);
        if (a._links._data[i]._next != b._links._data[i]._next)
            return Fail(step, "links[i]._next", a._links._data[i]._next,
                        b._links._data[i]._next);
    }

    CMP("entries._size",     a._entries._size,     b._entries._size);
    CMP("entries._capacity", a._entries._capacity, b._entries._capacity);
    for (int i = 0; i < a._entries._size; ++i) {
        const uint8_t* ea = reinterpret_cast<const uint8_t*>(a._entries._data) +
                            static_cast<size_t>(i) * 0x48;
        const uint8_t* eb = reinterpret_cast<const uint8_t*>(b._entries._data) +
                            static_cast<size_t>(i) * 0x48;
        // +0x00..+0x13: stamp, nextFree, cellX/Y, cellW/H. All written.
        if (std::memcmp(ea, eb, 0x14) != 0)
            return Fail(step, "entry[0x00..0x13]", i, i);
        // +0x14..+0x17 EXCLUDED: never written by either Entry writer.
        // +0x18 _mesh: same pointer, both subjects share the MeshDef.
        if (std::memcmp(ea + 0x18, eb + 0x18, 8) != 0)
            return Fail(step, "entry._mesh", i, i);
        // +0x20 Array<Transform>: _data differs by construction, contents do not.
        int32_t na, nb, capa, capb;
        std::memcpy(&na,   ea + 0x28, 4); std::memcpy(&nb,   eb + 0x28, 4);
        std::memcpy(&capa, ea + 0x2c, 4); std::memcpy(&capb, eb + 0x2c, 4);
        if (na != nb)     return Fail(step, "entry._transforms._size", na, nb);
        if (capa != capb) return Fail(step, "entry._transforms._capacity", capa, capb);
        const uint8_t *ta, *tb;
        std::memcpy(&ta, ea + 0x20, sizeof(ta));
        std::memcpy(&tb, eb + 0x20, sizeof(tb));
        if ((ta == nullptr) != (tb == nullptr))
            return Fail(step, "entry._transforms._data nullness", ta != nullptr,
                        tb != nullptr);
        if (ta && std::memcmp(ta, tb, static_cast<size_t>(na) * 0x1c) != 0)
            return Fail(step, "entry._transforms contents", i, i);
        // +0x30 _userData, +0x38 _userValue: always written.
        if (std::memcmp(ea + 0x30, eb + 0x30, 0x0c) != 0)
            return Fail(step, "entry[0x30..0x3b]", i, i);
        // +0x3c _userValue2: EXCLUDED for an entry ON THE FREE LIST, because
        // 0x18007b390's stack temporary never writes it. Compared for a live
        // entry (_nextFree == -2), where AddMesh did write it.
        int32_t nextFree;
        std::memcpy(&nextFree, ea + 0x04, 4);
        if (nextFree == -2 && std::memcmp(ea + 0x3c, eb + 0x3c, 4) != 0)
            return Fail(step, "entry._userValue2 (live)", i, i);
        if (std::memcmp(ea + 0x40, eb + 0x40, 8) != 0)
            return Fail(step, "entry._callback", i, i);
    }
#undef CMP
    return true;
}

// ---------------------------------------------------------------------------
// Structural stats, taken from OUR subject only. These are the effectiveness
// numbers: a comparison that agreed on a scene where nothing was threaded into
// more than one cell agreed about nothing.
// ---------------------------------------------------------------------------
void NoteStructure(const Scene& s) {
    for (int i = 0; i < s._cells._size; ++i) {
        int len = 0;
        for (int l = s._cells._data[i]._firstLink; l >= 0; l = s._links._data[l]._next)
            ++len;
        if (len > g_t.maxLinkChain) g_t.maxLinkChain = len;
    }
}

int RectCells(const Scene& s, int entryId) {
    const uint8_t* e = reinterpret_cast<const uint8_t*>(s._entries._data) +
                       static_cast<size_t>(entryId) * 0x48;
    uint16_t w, h;
    std::memcpy(&w, e + 0x10, 2);
    std::memcpy(&h, e + 0x12, 2);
    return static_cast<int>(w) * static_cast<int>(h);
}

// ---------------------------------------------------------------------------
// THE FLOAT ORACLE — and why the grid-state comparison alone is NOT one.
//
// `BoxTransform` (0x18001f470) is the only float-exact body in this lane, and
// its output reaches the grid comparison ONLY after `(int)(f * _invCellSize)`.
// That truncation swallows anything smaller than a whole cell, so a 1-ULP error
// in the AABB would sail through the entire scene comparison green. Comparing
// the grid state and calling the float body verified would be exactly the
// "green comparisons where zero did any work" failure, one level down.
//
// So the two float bodies are compared DIRECTLY and BITWISE, on their own
// inputs, before the grid script runs. Both are pure functions of their
// arguments (BoxTransform writes only its out-parameter; MeshBoundsXY reads the
// mesh and writes only its rectangle), so they replay with no state to restore.
// Tolerance is ZERO: anything less than bit-exact means a term was moved, and a
// moved term in a float sum is algebraically identical and bit-different
// exactly where the terms nearly cancel
// (reference_moved_term_hides_where_terms_cancel.md).
//
// THE POPULATION IS ROTATED ON PURPOSE. With an identity rotation the
// absolute-valued basis is the identity too, every `RtFAbs` returns its
// argument unchanged, and the three extent sums each have two zero terms — a
// sign error, a transposed index and a dropped `fabs` would ALL compare equal.
// An identity-only sweep is the uniform population this project has already
// been burned by. The quaternions below are deliberately off-axis, of mixed
// sign, and one is deliberately NOT normalised (retail does not normalise
// either, and the non-unit case is where the |M| terms stop cancelling).
//
// Note what this sweep also covers for free: our BoxTransform calls Wave 9 lane
// P's ported `AsTransform3` while the DLL's calls the DLL's. Lane P proved that
// bit-exact; if it ever stopped being so, this sweep reports it here rather
// than as an unattributable picking divergence.
// ---------------------------------------------------------------------------
int g_floatCases = 0, g_floatMismatches = 0;

bool CompareFloats(const char* what, const float* a, const float* b, int n) {
    ++g_floatCases;
    for (int i = 0; i < n; ++i) {
        uint32_t ua, ub;
        std::memcpy(&ua, a + i, 4);
        std::memcpy(&ub, b + i, 4);
        if (ua != ub) {
            ++g_floatMismatches;
            std::printf("[PickGridProbe] MISMATCH step=%s term=[%d] "
                        "ours=%.9g (0x%08x) theirs=%.9g (0x%08x)\n",
                        what, i, (double)a[i], ua, (double)b[i], ub);
            return false;
        }
    }
    return true;
}

void SweepFloatBodies(const Dll& dll) {
    // 8 transforms x 6 boxes = 48 comparisons of 6 floats each.
    static const float kQuat[8][4] = {
        { 0.0f,       0.0f,       0.0f,      1.0f      },  // identity (control)
        { 0.5f,       0.5f,       0.5f,      0.5f      },  // 120 deg about (1,1,1)
        { 0.0f,       0.70710678f, 0.0f,     0.70710678f},  // 90 deg about Y
        {-0.2672612f, 0.5345225f,-0.8017837f, 0.0f      },  // 180 deg, off-axis
        { 0.1830127f,-0.6830127f, 0.1830127f, 0.6830127f},
        {-0.4082483f,-0.4082483f, 0.4082483f, 0.7071068f},
        { 0.03f,      0.97f,     -0.11f,     0.21f     },  // NOT normalised
        { 0.7071068f, 0.0f,       0.7071068f, 0.0f      },
    };
    static const float kTrans[8][3] = {
        {   0.0f,    0.0f,   0.0f }, {  13.5f,  -7.25f,  2.0f },
        { -91.0f,   44.0f, -13.0f }, {   0.125f, 0.125f, 0.125f },
        { 1e5f,   -1e5f,    3.0f }, {  -0.5f,   -0.5f,  -0.5f },
        {  37.0f,   61.0f, -22.0f }, {  -3.0f,    8.0f,  15.0f },
    };
    // Boxes chosen so the extent terms NEARLY CANCEL in a couple of cases --
    // which is the only place a moved term is visible at all.
    static const float kBox[6][6] = {
        { -1.0f, -1.0f, -1.0f,  1.0f,  1.0f,  1.0f },
        {  0.0f,  0.0f,  0.0f,  0.0f,  0.0f,  0.0f },   // degenerate
        { -0.5f, -20.0f, -0.5f, 60.0f, 20.0f,  0.5f },
        {  10.0f, 10.0f, 10.0f, 10.0000019f, 10.0f, 10.0f },  // 1 ULP wide
        { -1e7f, -1e-7f, -3.0f, 1e7f,  1e-7f,  3.0f },
        {  2.5f, -8.25f,  0.75f, 9.5f, -1.5f,  4.25f },
    };
    for (int q = 0; q < 8; ++q) {
        ::Transform xf;
        xf.rotation.x = kQuat[q][0];
        xf.rotation.y = kQuat[q][1];
        xf.rotation.z = kQuat[q][2];
        xf.rotation.w = kQuat[q][3];
        xf.translation.x = kTrans[q][0];
        xf.translation.y = kTrans[q][1];
        xf.translation.z = kTrans[q][2];
        for (int b = 0; b < 6; ++b) {
            float ours[6], theirs[6];
            for (int k = 0; k < 6; ++k) { ours[k] = 1.0f; theirs[k] = 1.0f; }
            rtpickgrid::BoxTransform(ours, &xf, kBox[b]);
            dll.boxTransform(theirs, &xf, kBox[b]);
            CompareFloats("BoxTransform", ours, theirs, 6);
        }
    }

    // MeshBoundsXY over the same rotations, driving the multi-node union and
    // the COMISS min/max chain. Four nodes, so the union has something to do.
    for (int q = 0; q < 8; ++q) {
        ::Transform xforms[4];
        for (int k = 0; k < 4; ++k) {
            xforms[k].rotation.x = kQuat[q][0];
            xforms[k].rotation.y = kQuat[(q + k) % 8][1];
            xforms[k].rotation.z = kQuat[q][2];
            xforms[k].rotation.w = kQuat[q][3];
            xforms[k].translation.x = kTrans[q][0] + k * 4.5f;
            xforms[k].translation.y = kTrans[q][1] - k * 3.25f;
            xforms[k].translation.z = kTrans[q][2];
        }
        MeshDef m;
        m.Build(4, -6.0f, -4.0f, 11.0f, 7.0f);
        AddDesc d;
        std::memset(&d, 0, sizeof(d));
        d._mesh = &m.mesh;
        d._transforms._data     = xforms;
        d._transforms._size     = 4;
        d._transforms._capacity = 4;

        float ours[4], theirs[4];
        for (int k = 0; k < 4; ++k) { ours[k] = 1.0f; theirs[k] = 1.0f; }
        rtpickgrid::MeshBoundsXY(&d, ours);
        dll.meshBounds(&d, theirs);
        CompareFloats("MeshBoundsXY", ours, theirs, 4);
    }

    // The EMPTY-mesh case: retail returns the inverted sentinel rectangle
    // (+1e8 / -1e8) rather than anything sane, and the callers then truncate it
    // into a huge negative rectangle. Compared explicitly, because a port that
    // "helpfully" returned a zero rectangle would still pass every other case.
    {
        MeshDef m;
        m.Build(1, 0.0f, 0.0f, 1.0f, 1.0f);
        AddDesc d;
        std::memset(&d, 0, sizeof(d));
        d._mesh = &m.mesh;
        d._transforms._size = 0;
        float ours[4], theirs[4];
        for (int k = 0; k < 4; ++k) { ours[k] = 1.0f; theirs[k] = 1.0f; }
        rtpickgrid::MeshBoundsXY(&d, ours);
        dll.meshBounds(&d, theirs);
        CompareFloats("MeshBoundsXY(empty)", ours, theirs, 4);
    }
}

// ---------------------------------------------------------------------------
// THE NEGATIVE CONTROL.
//
// A copy of `UnlinkEntryFromCell` with the free-list push REMOVED — the link is
// unlinked from the cell but never returned to `_freeLink` and never stamped
// `_entry = -1`. That is precisely the missing-erase defect class that produced
// this project's teardown 100%-CPU hang and `IsA` EXEC crash, and it is the
// defect a blind rewrite of this function is most likely to introduce.
//
// It is applied to a THIRD subject driven through the same script. The
// comparator must flag it; if it does not, the comparator cannot fail and the
// green result above means nothing.
// ---------------------------------------------------------------------------
void BadUnlink(Scene* scene, Cell* cell, int32_t entryId) {
    Link* links = scene->_links._data;
    int32_t cur = cell->_firstLink, prev = -1;
    int32_t next = links[cur]._next;
    while (next >= 0) {
        if (links[cur]._entry == entryId) break;
        prev = cur; cur = next; next = links[cur]._next;
    }
    if (prev != -1) links[prev]._next = links[cur]._next;
    else            cell->_firstLink  = links[cur]._next;
    // *** the free-list push is deliberately missing here ***
    if (cell->_firstLink < 0) rtpickgrid::FreeEmptyCell(scene, cell);
}

// The FLOAT negative control: one MOVED TERM in the extent sum,
// `(a + b) + c` -> `a + (b + c)`. Algebraically identical, bit-different only
// where the terms nearly cancel — which is precisely the defect class the
// zero-tolerance comparison above exists to catch, and precisely the one a
// clean re-run cannot exonerate. If the comparator does not flag this over the
// 48-case sweep, the sweep's inputs do not contain a cancelling case and the
// green result is weaker than it looks.
int BadBoxTransformSweep(const Dll& dll) {
    int flagged = 0, cases = 0;
    static const float kQ[3][4] = {
        { 0.5f, 0.5f, 0.5f, 0.5f },
        { 0.03f, 0.97f, -0.11f, 0.21f },
        {-0.2705981f, 0.6532815f,-0.2705981f, 0.6532815f},
    };
    static const float kB[3][6] = {
        { -1e7f, -1e-7f, -3.0f, 1e7f, 1e-7f, 3.0f },
        { -0.5f, -20.0f, -0.5f, 60.0f, 20.0f, 0.5f },
        { 10.0f, 10.0f, 10.0f, 10.0000019f, 10.0f, 10.0f },
    };
    for (int q = 0; q < 3; ++q) {
        ::Transform xf;
        xf.rotation.x = kQ[q][0]; xf.rotation.y = kQ[q][1];
        xf.rotation.z = kQ[q][2]; xf.rotation.w = kQ[q][3];
        xf.translation.x = 13.5f; xf.translation.y = -7.25f; xf.translation.z = 2.0f;
        for (int b = 0; b < 3; ++b) {
            float theirs[6];
            dll.boxTransform(theirs, &xf, kB[b]);
            // Re-derive ours with the association moved.
            float m[12];
            using AsT3Fn = void* (*)(const void*, void*);
            static AsT3Fn asT3 = reinterpret_cast<AsT3Fn>(ResolveRuntimeRaw(0x18001fcd0));
            asT3(&xf, m);
            const float* p = kB[b];
            const float ez = (p[5] - p[2]) * 0.5f, ey = (p[4] - p[1]) * 0.5f;
            const float ex = (p[3] - p[0]) * 0.5f;
            const float cy = (p[4] + p[1]) * 0.5f, cx = (p[3] + p[0]) * 0.5f;
            const float cz = (p[5] + p[2]) * 0.5f;
            const float tcx = ((m[3] * cy + m[0] * cx) + m[6] * cz) + m[9];
            const float tcy = ((m[4] * cy + m[1] * cx) + m[7] * cz) + m[10];
            const float tcz = ((m[5] * cy + m[2] * cx) + m[8] * cz) + m[11];
            const float fx0 = m[0] < 0 ? -m[0] : m[0], fx3 = m[3] < 0 ? -m[3] : m[3];
            const float fx6 = m[6] < 0 ? -m[6] : m[6], fy1 = m[1] < 0 ? -m[1] : m[1];
            const float fy4 = m[4] < 0 ? -m[4] : m[4], fy7 = m[7] < 0 ? -m[7] : m[7];
            const float fz2 = m[2] < 0 ? -m[2] : m[2], fz5 = m[5] < 0 ? -m[5] : m[5];
            const float fz8 = m[8] < 0 ? -m[8] : m[8];
            // *** THE MOVED TERM ***  retail: (a + b) + c
            const float ax = fx0 * ex + (fx3 * ey + fx6 * ez);
            const float ay = fy1 * ex + (fy4 * ey + fy7 * ez);
            const float az = fz2 * ex + (fz5 * ey + fz8 * ez);
            const float bad[6] = { tcx - ax, tcy - ay, tcz - az,
                                   ax + tcx, ay + tcy, az + tcz };
            ++cases;
            bool same = true;
            for (int k = 0; k < 6; ++k) {
                uint32_t ua, ub;
                std::memcpy(&ua, bad + k, 4);
                std::memcpy(&ub, theirs + k, 4);
                if (ua != ub) { same = false; break; }
            }
            if (!same) ++flagged;
        }
    }
    std::printf("[PickGridProbe] float negative control: %d/%d cases differ "
                "from the shipped body%s\n", flagged, cases,
                flagged == 0
                    ? "  *** no cancelling case in the sweep; the zero-tolerance "
                      "float result is weaker than it looks ***"
                    : "  (a moved term IS visible in this input set)");
    return flagged;
}

void BadUnthread(Scene* scene, int32_t* cellXY, uint16_t w, uint16_t h, int32_t id) {
    const int32_t x0 = cellXY[0];
    int32_t y = cellXY[1];
    const int32_t xLimit = static_cast<int32_t>(w) + x0;
    const int32_t yLimit = static_cast<int32_t>(h) + y;
    for (; y < yLimit; ++y) {
        for (int32_t x = cellXY[0]; x < xLimit; ++x) {
            // Re-uses the ported lookup, so only the UNLINK differs.
            int32_t key[2] = { x, y };
            Cell* c = rtpickgrid::FindOrCreateCell(scene, key);
            BadUnlink(scene, c, id);
        }
    }
}

// ---------------------------------------------------------------------------
// The script. Applied identically to every subject.
//
// `bad` selects the negative control's unthread for the removal step.
// ---------------------------------------------------------------------------
struct Ops {
    uint32_t (*add)(Scene*, const AddDesc*);
    void     (*reindex)(Scene*, uint32_t);
    void     (*unthread)(Scene*, int32_t*, uint16_t, uint16_t, int32_t);
    void     (*freeEntry)(Scene*, uint32_t);
    void     (*iterAdvance)(CellIter*);
};

// A descriptor plus the transform array it owns. Both subjects get the same
// values, and each gets its OWN buffer so neither can free the other's.
struct Desc {
    AddDesc      d;
    ::Transform  xforms[4];

    // `spin` indexes a small table of OFF-AXIS rotations. An identity-only
    // fixture would make every mesh's world AABB axis-aligned and its cell
    // rectangle a function of the translation alone — a uniform population that
    // exercises neither the |M| terms in BoxTransform nor the multi-node union
    // in MeshBoundsXY (reference_uniform_population_is_not_a_verification.md).
    void Build(MeshDef* m, int n, float tx, float ty, void* userData,
               int32_t v1, int32_t v2, int spin) {
        static const float kSpin[5][4] = {
            { 0.0f,      0.0f,      0.0f,      1.0f      },
            { 0.0f,      0.0f,      0.3826834f, 0.9238795f},   // 45 deg about Z
            { 0.5f,      0.5f,      0.5f,      0.5f      },
            {-0.2705981f, 0.6532815f,-0.2705981f, 0.6532815f},
            { 0.1830127f,-0.6830127f, 0.1830127f, 0.6830127f},
        };
        std::memset(&d, 0, sizeof(d));
        for (int i = 0; i < n; ++i) {
            const float* q = kSpin[(spin + i) % 5];
            xforms[i].rotation.x = q[0];
            xforms[i].rotation.y = q[1];
            xforms[i].rotation.z = q[2];
            xforms[i].rotation.w = q[3];
            xforms[i].translation.x = tx;
            xforms[i].translation.y = ty;
            xforms[i].translation.z = 0.0f;
        }
        d._mesh = &m->mesh;
        d._transforms._data     = xforms;
        d._transforms._size     = n;
        d._transforms._capacity = n;
        d._userData   = userData;
        d._userValue  = v1;
        d._userValue2 = v2;
        d._callback   = nullptr;
    }
};

// Ten meshes/descriptors, sized so the resulting cell rectangles are NOT
// uniform: with a cell size of 16 the boxes below produce rectangles from 1x1
// up to about 5x4, and the translations put several of them into overlapping
// cells so the per-cell link lists have length > 1.
struct Fixture {
    MeshDef meshes[10];
    void Build() {
        static const struct { int nodes; float sx, sy; } kShape[10] = {
            { 1,  4.0f,  4.0f }, { 1, 40.0f,  8.0f }, { 2, 20.0f, 20.0f },
            { 1,  8.0f, 60.0f }, { 3, 12.0f, 12.0f }, { 1, 70.0f, 50.0f },
            { 2,  6.0f,  6.0f }, { 4, 30.0f, 10.0f }, { 1, 16.0f, 16.0f },
            { 2, 48.0f, 34.0f },
        };
        for (int i = 0; i < 10; ++i)
            meshes[i].Build(kShape[i].nodes, 0.0f, 0.0f, kShape[i].sx, kShape[i].sy);
    }
};

Fixture g_fixture;

// Translations, chosen so rectangles overlap heavily around the origin and a
// few sit well away from it (including NEGATIVE coordinates — the cell index is
// a TRUNCATION toward zero, so the negative side is where a floor-vs-truncate
// mistake would show).
const float kTx[10] = { 0.0f, 20.0f, -35.0f, 8.0f, -70.0f, 5.0f, 41.0f, -12.0f, 96.0f, -3.0f };
const float kTy[10] = { 0.0f, -18.0f, 7.0f, 55.0f, -9.0f, 30.0f, -64.0f, 2.0f, -41.0f, 11.0f };

// ---------------------------------------------------------------------------
// The script, as INDEXED STEPS applied in LOCKSTEP.
//
// The first version of this probe ran the whole script on our subject, then the
// whole script on theirs, and compared ONCE at the end. That is a weaker
// experiment than it looks, in two ways, and both are failure modes this
// project has already paid for:
//
//   * a defect that CANCELS OUT compares equal. Push a link onto the free list
//     in the wrong order and pop it back in the wrong order and the FINAL state
//     is identical — but every query in between saw a different grid, and a
//     picking grid exists only to be queried.
//   * a failure at step 3 of 17 is reported as "the final states differ", which
//     is the unlocalised verdict that cost Wave 5's RemoveMesh shadow an extra
//     integration run to diagnose
//     (reference_shadow_predicate_must_name_its_failing_term.md).
//
// So each step is applied to BOTH subjects and the whole scene is compared
// immediately, named by step. `Runner` is one subject plus the descriptors it
// owns — per-subject on purpose, because `GridAdd` deep-copies the transform
// array and each subject has to own the copy it will later free.
// ---------------------------------------------------------------------------
struct Runner {
    Scene*   s = nullptr;
    Ops      ops = {};
    Desc     descs[10];
    uint32_t ids[10] = {};

    void Init(Scene* scene, const Ops& o) {
        s = scene;
        ops = o;
        for (int i = 0; i < 10; ++i)
            descs[i].Build(&g_fixture.meshes[i], g_fixture.meshes[i].nodeCount,
                           kTx[i], kTy[i], reinterpret_cast<void*>(0x1000 + i),
                           100 + i, 200 + i, i);
    }
};

// Which entries get removed, and in which order. Deliberately NOT index order
// and NOT most-recently-added: a strictly LIFO removal order is the uniform
// population of free-list testing, and it would never produce a free list whose
// order differs from the allocation order.
const int kRemove[3] = { 1, 4, 2 };

// 17 steps: 6 adds on an empty scene, 4 reindexes (3 that move, 1 that must
// take the early-out), 3 removals, then 4 more adds that now hit every reuse
// arm because all three free lists are non-empty.
const int kStepCount = 17;

const char* StepName(int step) {
    if (step < 6)  return "add(batch1)";
    if (step < 10) return "reindex";
    if (step < 13) return "remove";
    return "add(batch2)";
}

// `stats` is true only for OUR runner, so the effectiveness counters are
// counted once rather than twice (or three times under the negative control).
void RunStep(Runner& r, int step, bool stats) {
    Scene* s = r.s;

    if (step < 6) {
        const int i = step;
        r.ids[i] = r.ops.add(s, &r.descs[i].d);
        if (stats) {
            ++g_t.adds;
            if (RectCells(*s, static_cast<int>(r.ids[i])) > 1) ++g_t.entriesMultiCell;
        }
        return;
    }

    if (step < 10) {
        const int i = step - 6;
        if (i < 3) {
            // Move the mesh so the rectangle really changes. GridReindex reads
            // the ENTRY's own transform array, not the descriptor's, so the
            // move has to be written there.
            for (int k = 0; k < g_fixture.meshes[i].nodeCount; ++k) {
                r.descs[i].xforms[k].translation.x = kTx[i] + 100.0f + i * 37.0f;
                r.descs[i].xforms[k].translation.y = kTy[i] - 80.0f - i * 23.0f;
            }
            uint8_t* e = reinterpret_cast<uint8_t*>(s->_entries._data) +
                         static_cast<size_t>(r.ids[i]) * 0x48;
            uint8_t* t; std::memcpy(&t, e + 0x20, sizeof(t));
            int32_t n;  std::memcpy(&n, e + 0x28, 4);
            for (int k = 0; k < n; ++k)
                std::memcpy(t + static_cast<size_t>(k) * 0x1c,
                            &r.descs[i].xforms[k], 0x1c);
        }
        const int before = RectCells(*s, static_cast<int>(r.ids[i]));
        r.ops.reindex(s, r.ids[i]);
        if (stats) {
            const int after = RectCells(*s, static_cast<int>(r.ids[i]));
            // i == 3 deliberately did NOT touch the transforms, so GridReindex
            // must take its early-out. Counted separately, because a site whose
            // early-out swallows most calls reports a `calls` number that is
            // not its coverage (Wave 9 lane M's Finding 1).
            (void)after; (void)before;
            if (i == 3) ++g_t.reindexNoop;
            else        ++g_t.reindexMoved;
        }
        return;
    }

    if (step < 13) {
        const int id = static_cast<int>(r.ids[kRemove[step - 10]]);
        uint8_t* e = reinterpret_cast<uint8_t*>(s->_entries._data) +
                     static_cast<size_t>(id) * 0x48;
        uint16_t w, h;
        std::memcpy(&w, e + 0x10, 2);
        std::memcpy(&h, e + 0x12, 2);
        const int freeCellBefore = s->_freeCell;
        r.ops.unthread(s, reinterpret_cast<int32_t*>(e + 0x08), w, h, id);
        r.ops.freeEntry(s, static_cast<uint32_t>(id));
        if (stats) {
            ++g_t.removes; ++g_t.unthreads;
            if (s->_freeCell != freeCellBefore) ++g_t.cellsFreed;
        }
        return;
    }

    const int i = step - 13 + 6;
    const int freeEntryBefore = s->_freeEntry;
    const int freeLinkBefore  = s->_freeLink;
    const int freeCellBefore  = s->_freeCell;
    r.ids[i] = r.ops.add(s, &r.descs[i].d);
    if (stats) {
        ++g_t.adds;
        if (freeEntryBefore >= 0) ++g_t.entryFreeReuse;
        if (freeLinkBefore  >= 0) ++g_t.linkFreeReuse;
        if (freeCellBefore  >= 0) ++g_t.cellFreeReuse;
        if (RectCells(*s, static_cast<int>(r.ids[i])) > 1) ++g_t.entriesMultiCell;
        NoteStructure(*s);
    }
}

// The iterator is compared separately, because it MUTATES ONLY THE ITERATOR and
// so can be replayed: both bodies are driven over the same finished scene and
// the full visit SEQUENCE is compared, not just the end state. Retail is driven
// over THEIR scene and ours over OURS — the two scenes are already known
// identical at this point, which is what makes the sequences comparable.
bool CompareIteration(const Dll& dll, Scene* ours, Scene* theirs) {
    bool ok = true;
    for (int stamp = 0; stamp < 3 && ok; ++stamp) {
        // A stamp of 0 matches every untouched entry's _queryStamp, so pass 1
        // and 2 exercise the "skip already-stamped" chain walk that a scene
        // with one entry per cell would never reach.
        for (int i = 0; i < ours->_entries._size && stamp > 0; ++i) {
            const int32_t v = (i % 2) ? stamp : stamp - 1;
            std::memcpy(reinterpret_cast<uint8_t*>(ours->_entries._data) +
                            static_cast<size_t>(i) * 0x48, &v, 4);
            std::memcpy(reinterpret_cast<uint8_t*>(theirs->_entries._data) +
                            static_cast<size_t>(i) * 0x48, &v, 4);
        }
        CellIter a, b;
        std::memset(&a, 0, sizeof(a));
        a.minX = -8; a.minY = -8; a.maxX = 12; a.maxY = 10;
        a.curX = a.minX - 1; a.curY = a.minY;
        a.stamp = stamp; a.link = -1;
        b = a;
        a.scene = ours; b.scene = theirs;

        for (int step = 0; step < 4096; ++step) {
            rtpickgrid::CellIterAdvance(&a);
            dll.cellIterAdvance(&b);
            ++g_t.iterSteps;
            if (a.link >= 0) ++g_t.iterHits;
            ++g_t.comparisons;
            if (a.curX != b.curX || a.curY != b.curY || a.link != b.link) {
                ok = Fail("iterate", "cursor/link",
                          ((long long)a.curX << 40) | ((long long)(a.curY & 0xffff) << 24) |
                              (uint32_t)(a.link & 0xffffff),
                          ((long long)b.curX << 40) | ((long long)(b.curY & 0xffff) << 24) |
                              (uint32_t)(b.link & 0xffffff));
                break;
            }
            if (a.link < 0 && a.curY > a.maxY) break;   // exhausted
        }
    }
    return ok;
}

}  // namespace

// ============================================================================
void RtPickingGridProbe() {
    const Mode mode = ProbeMode();
    if (mode == kOff) return;

    const Dll dll = ResolveDll();
    if (!dll.ok) {
        std::printf("[PickGridProbe] SKIPPED -- the shipped bodies did not resolve "
                    "(Runtime-x64.dll absent?). This probe is a DLL-PRESENT "
                    "instrument by construction.\n");
        return;
    }

    g_fixture.Build();

    // The float bodies first, on their own inputs and bitwise. See the banner
    // above SweepFloatBodies for why the grid comparison alone does NOT verify
    // them: the truncation to cell indices swallows everything smaller than a
    // whole cell.
    SweepFloatBodies(dll);

    Subject ours, theirs;
    ours.Build(dll);
    theirs.Build(dll);

    const Ops oursOps = {
        &rtpickgrid::GridAdd, &rtpickgrid::GridReindex,
        &rtpickgrid::GridUnthread, &rtpickgrid::GridFreeEntry,
        &rtpickgrid::CellIterAdvance,
    };
    const Ops theirOps = {
        dll.gridAdd, dll.gridReindex, dll.gridUnthread, dll.gridFreeEntry,
        dll.cellIterAdvance,
    };

    // Two runners, advanced ONE STEP AT A TIME and compared after each. See the
    // banner above `Runner` for why comparing only the final state is a weaker
    // experiment than it looks.
    static Runner ourRun, theirRun;    // static: 10 Descs each, too big for the
    ourRun.Init(&ours.s, oursOps);     // stack of an arbitrary caller's frame
    theirRun.Init(&theirs.s, theirOps);

    bool ok = true;
    for (int step = 0; step < kStepCount && ok; ++step) {
        RunStep(ourRun,   step, true);
        RunStep(theirRun, step, false);
        char label[48];
        std::snprintf(label, sizeof(label), "step%02d:%s", step, StepName(step));
        for (int i = 0; i < 10 && ok; ++i)
            if (ourRun.ids[i] != theirRun.ids[i])
                ok = Fail(label, "returned entry id", ourRun.ids[i], theirRun.ids[i]);
        if (ok) ok = Compare(label, ours.s, theirs.s);
    }
    if (ok) ok = CompareIteration(dll, &ours.s, &theirs.s);

    // ---- the negative control ------------------------------------------
    if (mode == kNeg) {
        static Subject bad;
        bad.Build(dll);
        Ops badOps = oursOps;
        badOps.unthread = &BadUnthread;
        static Runner badRun;
        badRun.Init(&bad.s, badOps);
        const int before = g_t.mismatches;
        // The control runs the whole script and is compared ONCE, against the
        // clean subject's finished state. Localisation is not what is under
        // test here — the comparator's ABILITY TO FAIL is.
        for (int step = 0; step < kStepCount; ++step) RunStep(badRun, step, false);
        ++g_negComparisons;
        if (!Compare("negctl", bad.s, theirs.s)) ++g_negMismatches;
        g_t.mismatches = before;   // the control's failure is not the port's
        bad.Destroy();
        BadBoxTransformSweep(dll);
    }

    // ---- the report -----------------------------------------------------
    std::printf("[PickGridProbe] comparisons=%d mismatches=%d  "
                "floatCases=%d floatMismatches=%d\n",
                g_t.comparisons, g_t.mismatches, g_floatCases, g_floatMismatches);
    std::printf("[PickGridProbe] work: adds=%d multiCellAdds=%d removes=%d "
                "unthreads=%d reindexMoved=%d reindexNoop=%d cellsFreed=%d\n",
                g_t.adds, g_t.entriesMultiCell, g_t.removes, g_t.unthreads,
                g_t.reindexMoved, g_t.reindexNoop, g_t.cellsFreed);
    std::printf("[PickGridProbe] work: entryFreeReuse=%d linkFreeReuse=%d "
                "cellFreeReuse=%d maxLinkChain=%d iterSteps=%d iterHits=%d\n",
                g_t.entryFreeReuse, g_t.linkFreeReuse, g_t.cellFreeReuse,
                g_t.maxLinkChain, g_t.iterSteps, g_t.iterHits);

    // A zero in any of these is a GAP and is said in as many words, because a
    // green line over a path that never ran is the failure this control exists
    // to prevent.
    struct { const char* name; int v; } gaps[] = {
        { "multiCellAdds", g_t.entriesMultiCell },
        { "reindexMoved",  g_t.reindexMoved },
        { "reindexNoop",   g_t.reindexNoop },
        { "cellsFreed",    g_t.cellsFreed },
        { "entryFreeReuse", g_t.entryFreeReuse },
        { "linkFreeReuse",  g_t.linkFreeReuse },
        { "cellFreeReuse",  g_t.cellFreeReuse },
        { "iterHits",       g_t.iterHits },
    };
    for (const auto& g : gaps)
        if (g.v == 0)
            std::printf("[PickGridProbe] GAP: %s ran ZERO times -- this run "
                        "verified nothing about that path\n", g.name);
    if (g_t.maxLinkChain < 2)
        std::printf("[PickGridProbe] GAP: no cell ever held more than one link -- "
                    "the middle-of-list unlink was never exercised\n");

    if (mode == kNeg) {
        std::printf("[PickGridProbe] negative control: %d/%d flagged%s\n",
                    g_negMismatches, g_negComparisons,
                    g_negMismatches == 0
                        ? "  *** THE COMPARATOR CANNOT FAIL; the green result "
                          "above is worthless ***"
                        : "  (comparator proven able to fail)");
    }

    std::printf("[PickGridProbe] %s\n",
                (g_t.mismatches == 0 && g_floatMismatches == 0)
                    ? "PASS -- every ported grid body agreed with the shipped one"
                    : "FAIL -- see the MISMATCH lines above");

    ours.Destroy();
    theirs.Destroy();
}
