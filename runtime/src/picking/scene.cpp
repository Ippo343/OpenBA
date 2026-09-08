// ============================================================================
// runtime/src/picking/scene.cpp — Wave 5 lane D — Picking::Scene / PickingMesh
// ============================================================================
//
// Ports the nine `Picking` exports our tree calls (docs/runtime_port/PLAN.md
// §4.2 wave 5, §6). Layouts and the evidence for every offset are in
// runtime/types/picking/Scene.h; the per-function write-up, the oracle
// arguments and everything this lane could NOT verify are in
// docs/runtime_port/WAVE5_LANE_D.md.
//
//   | Export                          | Retail      | Ord  | Oracle          |
//   |---------------------------------|-------------|------|-----------------|
//   | ??0Scene@Picking@@QEAA@XZ       | 0x1801258e0 |  376 | FULL (scratch)  |
//   | ??1Scene@Picking@@QEAA@XZ       | 0x180125890 |  672 | post-state only |
//   | ?AddMesh@Scene@Picking@@...     | 0x1801252e0 | 1571 | FULL (scratch)  |
//   | ?GetUserData@Scene@Picking@@... | 0x1800722d0 | 2998 | FULL            |
//   | ?Intersect@Scene@Picking@@...   | 0x1801246c0 | 3081 | FULL, tol 0     |
//   | ?RemoveMesh@Scene@Picking@@...  | 0x180125290 | 3560 | post-state only |
//   | ?SetMeshUserData@Scene@...      | 0x180125240 | 3917 | FULL (replay)   |
//   | ?SetTransform@Scene@Picking@@...| 0x1800721b0 | 3993 | FULL (replay)   |
//   | ?Vertices@PickingMesh@Picking...| 0x180071e10 | 4536 | FULL            |
//
// EVERY ONE IS REGISTERED TWICE — under its mangled name AND under its retail
// address. The two registries are separate, and this namespace's call sites use
// both forms: pickingcomponent.cpp:171 resolves AddMesh by NAME while
// floorcomponent.cpp:303 and citizenai_executor.cpp:406 resolve the SAME
// function by ADDRESS, and both RemoveMesh call sites are address-only. A
// name-only registration would have sent some of one live Scene's mesh
// registrations to this port and the rest to the DLL — two implementations
// taking turns on one free list.
//
// All nine names were checked against the shipped export directory of
// C:\GOG Games\Banished\Runtime-x64.dll with pefile BEFORE any body was
// written (reference_resolveexport_phantom_names.md: a name the DLL does not
// contain resolves to NULL in silence and the census still counts the port).
// The addresses and ordinals above are that query's output.
//
// WHAT STAYS IN THE DLL, AND WHY
// ------------------------------
// `Picking::Scene` is a spatial hash. Eight of its internals are `FUN_`-named,
// shared with other classes, and allocate or free through
// `System::MemoryAllocator` under that allocator's own critical section --
// which is Win32 and therefore cannot appear in this file (PLAN §4.1 decision 1
// keeps runtime/src/ platform-neutral outside a `_win` twin). This lane ports
// the nine EXPORTED bodies faithfully and calls those internals at their retail
// addresses, exactly as retail's own code does. They are named `Rt*` below and
// each one says what it is. This is the same shape Wave 4 lane C used for
// `Map_Find` / `Map_Insert`.
//
// IS THE CLASS SPLIT?  No -- every Scene method this process ever invokes is
// now ours. `Picking::Scene` exports 21 symbols; our tree calls nine of them
// (the table above), and a grep across code/, engine/, standalone/, runtime/
// and dx11/ finds NO call site for any of the other twelve -- not Update, not
// Clip, not GetMesh / GetAABB / GetLocalAABB / GetTransform / GetUserValue, not
// the copy ctor or operator=. And `get_xrefs_to` on Update, on Clip and on all
// nine ported addresses returns only DATA references (the export address and
// name tables, plus .pdata unwind records): Runtime-x64.dll never calls any of
// them internally either. So there is no path by which the DLL and this port
// both mutate one Scene, which is the state-corruption hazard PLAN §4.1
// decision 5 is about. The unported twelve are dormant code in a DLL nothing
// dispatches into.
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/picking/Scene.h"

using RtPicking::Scene;
using RtPicking::Entry;
using RtPicking::Link;
using RtPicking::Cell;
using RtPicking::SubMesh;
using RtPicking::Triangle;
using RtPicking::HitCallback;
using RtPicking::AddDesc;

// ----------------------------------------------------------------------------
// ResolveAddr, forward-declared rather than pulled in from standalone/resolve.h
// ----------------------------------------------------------------------------
// Same reasoning rt_port.h gives for declaring RegisterRuntimeExport locally:
// resolve.h drags in <windows.h>, and <wingdi.h>'s `Rectangle` function and
// <windef.h>'s `near`/`far` macros collide with the engine's own type and
// member names. This declaration must stay byte-identical to resolve.h's; the
// linker checks the two against each other.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace {

// ============================================================================
// Mangled names for the diff-oracle calls. The registrations at the bottom of
// this file deliberately repeat each name as its own literal instead of using
// these constants: tools/runtime_census.py cannot see through a NAMED CONSTANT
// and would under-count the lane, which is exactly how Wave 3's Object count
// stood at 21/41 when it was really 27/41
// (reference_census_cannot_see_nonliteral_registrations.md).
//
// CORRECTION to what this project has been telling agents (PLAN §6 step 8).
// The advice "spell every name as a SINGLE literal on ONE line, the census
// cannot see a multi-line literal" is now STALE — but it was true until this
// wave. Wave 5 lane E added a `concat_literal()` helper to BOTH tools
// (tools/runtime_census.py:53, tools/export_name_audit.py:41) that joins
// adjacent string literals exactly as the preprocessor does; before that, the
// split AddMesh literal at code/Game/Component/pickingcomponent.cpp:171 was one
// of five FALSE phantom reports. Today `export_name_audit.py` reports 11
// phantoms and none of them is a Picking name. The remaining blind spot is a
// non-literal name, and the census now WARNS on that rather than swallowing it.
// ============================================================================
const char* const kSceneCtor    = "??0Scene@Picking@@QEAA@XZ";
const char* const kSceneDtor    = "??1Scene@Picking@@QEAA@XZ";
const char* const kAddMesh      =
    "?AddMesh@Scene@Picking@@QEAAIV?$ExternalPtr@VPickingMesh@Picking@@@Object@@AEBVTransform@Math@@PEAXHHP6AX2HHAEBVVector3@6@33@Z@Z";
const char* const kGetUserData  = "?GetUserData@Scene@Picking@@QEBAPEAXI@Z";
const char* const kIntersect    =
    "?Intersect@Scene@Picking@@QEBAHAEBVVector3@Math@@0AEBVScalar@4@AEAV34@2@Z";
const char* const kSetMeshUD    =
    "?SetMeshUserData@Scene@Picking@@QEAAXIPEAXHHP6AX0HHAEBVVector3@Math@@11@Z@Z";
const char* const kSetTransform = "?SetTransform@Scene@Picking@@QEAAXIAEBVTransform@Math@@@Z";
const char* const kVertices     =
    "?Vertices@PickingMesh@Picking@@QEBAAEBV?$Array@VVector3@Math@@@Collection@@H@Z";
const char* const kRemoveMesh   = "?RemoveMesh@Scene@Picking@@QEAAXI@Z";

// ============================================================================
// The float constants retail loads from .rdata, verified by reading the bytes
// out of the shipped DLL rather than trusting Ghidra's rendering ("-NAN" in a
// decompile is 0xffffffff, not a float).
//   DAT_18016169c = 0x3f800000 =  1.0f
//   DAT_1801616fc = 0x41800000 = 16.0f      (the Scene's cell size)
//   DAT_180161740 = 0x4cbebc20 =  1.0e8f    (RtMathConst::kBigPos)
//   DAT_18016177c = 0xccbebc20 = -1.0e8f    (RtMathConst::kBigNeg)
//   DAT_1801617c0 = 0x80000000              (sign-flip mask, i.e. unary minus)
// ============================================================================
// ============================================================================
// WAVE 9 LANE N — THE DETERMINISTIC RAY FIXTURE FOR Picking::Scene::Intersect
// ============================================================================
// Lane M found 90 divergences over 20,868 calls in ONE stress run of FOUR, and
// correctly refused to call the clean runs an exoneration: the call COUNTS
// differ between runs (20,868 vs 22,668), so the runs are not the same runs.
// `Picking::Scene::Intersect` is driven by where the ray is cast, which on the
// stress route depends on the real mouse cursor, so a clean re-run only means
// those rays were not cast — the calls=0-reads-as-clean failure
// (reference_uncalled_port_is_not_verified.md) wearing a different costume.
//
// This fixture removes the cursor from the experiment. On the Nth real call it
// casts a RAY SET BUILT FROM CONSTANTS over the live scene, through this very
// function, so every ray is diffed by the ordinary oracle. Same seed, same
// flags, same frame => the same rays, the same count, every run. That makes
// the DENOMINATOR stable, which is the precondition for any verdict at all.
//
//   BANISHED_PICK_SWEEP=<n>       fire after the nth real Intersect call
//   BANISHED_PICK_SWEEP_HALF=<f>  sweep half-extent in world units (default 60)
//   BANISHED_PICK_SWEEP_STEP=<f>  spacing (default 6) => (2*half/step+1)^2 origins
//   BANISHED_PICK_SWEEP_Z=<f>     ray origin height (default 200)
//   BANISHED_PICK_SWEEP_DIST=<f>  maxDist (default 600)
//   BANISHED_PICK_NEGCTL=1        NEGATIVE CONTROL, see below
//
// The five directions are near-vertical by design: the observed divergence had
// outPos.x and .y BIT-IDENTICAL and only .z moved, by 3.8e-6 — which is what a
// ~1-ULP difference in hitT does to a ray whose dir.z dominates (z moves by
// delta, x by delta*dir.x, which stays inside one ULP of x ~ 41.5). Rays of
// that shape are the ones worth casting.
//
// EFFECTIVENESS CONTROL: the summary prints `hits=`, not just `rays=`. A sweep
// that hits nothing has tested nothing, however green it looks — the same
// mistake as reading divergences= without calls=.
//
// NEGATIVE CONTROL: BANISHED_PICK_NEGCTL=1 moves ONE association in the hitT
// expression — `(a + b + c)` becomes `(a + (b + c))` — which is precisely the
// defect class the port's own no-tolerance comment is guarding against
// ("anything less than bit-exact means a term was moved"). If the sweep comes
// back green under the control, the fixture cannot fail and proves nothing;
// lane E2 found exactly such a control this wave.
// ============================================================================
int PickSweepAfterCalls() {
    static int n = -1;
    if (n < 0) { const char* e = getenv("BANISHED_PICK_SWEEP"); n = e ? atoi(e) : 0; }
    return n;
}
float PickSweepEnvF(const char* name, float dflt) {
    const char* e = getenv(name);
    return (e && *e) ? (float)atof(e) : dflt;
}
int PickNegCtl() {
    static int on = -1;
    if (on < 0) { const char* e = getenv("BANISHED_PICK_NEGCTL"); on = e ? atoi(e) : 0; }
    return on;
}
RtDiffSite* g_pickIntersectSite = nullptr;
bool        g_pickSweepRunning  = false;
bool        g_pickSweepFired    = false;
long long   g_pickRealCalls     = 0;

const float kOne       =  1.0f;
const float kCellSize  = 16.0f;
const float kBigPos    =  1.0e8f;
const float kBigNeg    = -1.0e8f;

// ============================================================================
// The six DLL internals this port calls, each resolved once per call site.
// ResolveAddr and NOT ResolveExport/RtOriginal: none of these is exported, and
// RtOriginal is GetProcAddress, which must never sit on a normal-run path
// (reference_rtoriginal_is_getprocaddress.md — two such calls cost Wave 2 five
// points of sim time, invisible to both profilers).
// ============================================================================

// Array<int>::SetSize(10000), filling new slots with -1. Retail 0x18007b960;
// the Scene ctor's only callee besides the field stores. Reads/writes the
// {data,size,capacity} triple in place, so it takes &scene->_cellHeads.
void RtCellHeadsInit(void* cellHeadsArray) {
    using Fn = void (*)(void*);
    static Fn fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x18007b960));
    fn(cellHeadsArray);
}

// Array<T>::~Array for a trivially-destructible T: Free(_data) under the
// allocator lock, then zero {_data, _size, _capacity}. Retail has one COMDAT
// per instantiation and the Scene dtor calls three of them plus one that also
// destroys each element. All four are called here at their own addresses.
//
// *** WAVE 9 LANE Q, 2026-09-07: THREE OF THE FOUR NO LONGER REACH THE DLL. ***
// 0x1800737f0, 0x1800739e0 and 0x180073bb0 are lane Q's J1 registrations
// (runtime/src/collection/array.cpp) and ResolveAddr consults the port registry
// first. So "the retail body and not a re-derivation of it" -- which is what
// this comment said until lane Q landed -- is now true only of 0x180073c90, the
// J2 element-destroying variant, which is still the DLL's. The other three ARE
// a re-derivation, verified against the shipped bodies by the synthetic-subject
// replay in runtime/src/collection/array_probe.cpp.
void RtFreeArrayInt(void* a) {          // scene+0x08, Array<int>
    using Fn = void (*)(void*);
    static Fn fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x1800737f0));
    fn(a);
}
void RtFreeArrayCell(void* a) {         // scene+0x18, Array<Cell>
    using Fn = void (*)(void*);
    static Fn fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x1800739e0));
    fn(a);
}
void RtFreeArrayLink(void* a) {         // scene+0x30, Array<Link>
    using Fn = void (*)(void*);
    static Fn fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x180073bb0));
    fn(a);
}
void RtFreeArrayEntry(void* a) {        // scene+0x48, Array<Entry> — this one
    using Fn = void (*)(void*);         // first destroys each Entry's inner
    static Fn fn = reinterpret_cast<Fn>(   // Array<Transform> (FUN_180074100)
        ResolveAddr(Module::Runtime, 0x180073c90));
    fn(a);
}

// AddMesh's scratch Array<Transform>. Retail frees that buffer with a bare
// `MemoryAllocator::Free(_singleton, buf)` between an Enter/LeaveCriticalSection
// pair, which cannot be written here without <windows.h>. Retail 0x1800739e0 is
// the same Free under the same lock, followed by a clear of the triple — and
// the triple being cleared is a dying local, so the effect is identical. It is
// the Array<Cell> instantiation by origin, but every Array<T>::~Array for a
// trivially-destructible T is byte-identical, and Math::Transform is one.
// (Wave 9 lane Q re-read all eleven and confirms "byte-identical" in the strong
// form: identical instructions AND identical resolved operands. This call now
// lands in lane Q's port -- see the note on RtFreeArrayInt above.)
void RtFreeScratchTransforms(void* a) {
    using Fn = void (*)(void*);
    static Fn fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x1800739e0));
    fn(a);
}

// Array<Transform>::SetSize(n). Retail 0x180059140. Element stride 0x1c, and
// the default element it fills new slots with is the identity transform
// (0,0,0, 1.0f, 0,0,0) — visible as the `0x3f800000` store at +0x0c in that
// body. AddMesh uses it for its scratch buffer.
void RtTransformArraySetSize(void* a, int n) {
    using Fn = void (*)(void*, int);
    static Fn fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x180059140));
    fn(a, n);
}

// The grid insert: allocate an Entry from the free list (or grow the array),
// deep-copy the descriptor into it, compute its cell rectangle from the mesh's
// projected AABB, and thread it onto every cell it covers. Retail 0x18007af70,
// returns the entry index. Everything it touches is either the Scene's own
// arrays or the allocator, so it is layout-compatible with this port.
uint32_t RtGridAdd(Scene* scene, const AddDesc* desc) {
    using Fn = uint32_t (*)(Scene*, const AddDesc*);
    static Fn fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x18007af70));
    return fn(scene, desc);
}

// The grid re-index behind SetTransform: recompute the entry's cell rectangle
// and, only if it changed, unthread it from its old cells and rethread it.
// Retail 0x1800724d0.
void RtGridReindex(Scene* scene, uint32_t id) {
    using Fn = void (*)(Scene*, uint32_t);
    static Fn fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x1800724d0));
    fn(scene, id);
}

// Unthread an entry from every cell of its rectangle. Retail 0x180072950; takes
// &entry->_cellX (it reads the y from _cellX[1]), the two uint16 extents, and
// the entry index. Walks the same hash the cell iterator does and calls
// FUN_180072d10 to unlink the entry's Link from each cell's list.
void RtGridUnthread(Scene* scene, int32_t* cellXY, uint16_t w, uint16_t h, int32_t id) {
    using Fn = void (*)(Scene*, int32_t*, uint16_t, uint16_t, int32_t);
    static Fn fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x180072950));
    fn(scene, cellXY, w, h, id);
}

// Release an entry back to the free list. Retail 0x18007b390: assigns an
// all-zero Entry over it (which frees the inner Array<Transform> through the
// Entry assignment operator), destroys the temporary, then pushes the slot onto
// the free list — `entry->_nextFree = scene->_freeEntry; scene->_freeEntry = id`.
void RtGridFreeEntry(Scene* scene, uint32_t id) {
    using Fn = void (*)(Scene*, uint32_t);
    static Fn fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x18007b390));
    fn(scene, id);
}

// The cell-rectangle iterator Intersect drives. Retail 0x18007b6d0. Advances
// `it` to the next (cell, link) pair whose entry has not already been stamped
// with `it.stamp`, and sets it.link = -1 when the rectangle is exhausted.
// State layout is pinned by Intersect's own frame at [RSP+0x30]; see CellIter.
struct CellIter {
    int32_t minX, minY;   // +0x00 +0x04
    int32_t maxX, maxY;   // +0x08 +0x0c
    int32_t curX, curY;   // +0x10 +0x14
    int32_t stamp;        // +0x18
    int32_t link;         // +0x1c
    Scene*  scene;        // +0x20
};
static_assert(sizeof(CellIter) == 0x28, "Picking cell iterator must be 0x28 bytes");
static_assert(offsetof(CellIter, scene) == 0x20, "CellIter::scene @ +0x20");

void RtCellIterAdvance(CellIter* it) {
    using Fn = void (*)(CellIter*);
    static Fn fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x18007b6d0));
    fn(it);
}

// ============================================================================
// Math helpers, transliterated from the retail bodies rather than called.
//
// Both of these ARE ported and registered by Wave 1 (runtime/src/math/
// transform.cpp), and retail CALLs both. They are duplicated here on purpose:
// this lane's job is to be correct standing alone against the DLL, and routing
// its arithmetic through another lane's registration would mean a divergence
// here could not be attributed. Wave 1's `Quaternion::operator*` is the
// project's recorded example of a wrong port that nothing called
// (reference_uncalled_port_is_not_verified.md); both bodies below were
// re-derived from the retail decompile and then checked to agree with Wave 1's
// term-for-term, which is an independent confirmation of that fix rather than
// a dependency on it.
// ============================================================================

// ??DQuaternion@Math@@QEBA?AV01@AEBV01@@Z  retail 0x18001e280.
// MSVC ABI at every call site in this file: RCX = this, RDX = hidden return
// slot, R8 = rhs. Written exactly as retail associates the terms.
void QuatMul(const Quaternion* a, Quaternion* ret, const Quaternion* b) {
    const float bx = b->x, by = b->y, bz = b->z, bw = b->w;
    const float ax = a->x, ay = a->y, az = a->z, aw = a->w;
    const float x = (ax * bw + bx * aw + az * by) - ay * bz;
    const float y = (ay * bw - az * bx) + by * aw + bz * ax;
    const float w = ((bw * aw - ax * bx) - ay * by) - bz * az;
    const float z = ((az * bw + ay * bx) - by * ax) + bz * aw;
    ret->x = x; ret->y = y; ret->w = w; ret->z = z;
}

// ?Inverse@Transform@Math@@QEBA?AV12@XZ  retail 0x18005db50.
// Rigid inverse: rotation' = conjugate(q), translation' from a quaternion
// rotate of -translation. Retail's exact term order, not the textbook form.
void TransformInverse(const ::Transform* self, ::Transform* ret) {
    const float qx = self->rotation.x, qy = self->rotation.y;
    const float qz = self->rotation.z, qw = self->rotation.w;
    const float nqx = -qx, nqy = -qy, nqz = -qz;
    const float ntx = -self->translation.x;
    const float nty = -self->translation.y;
    const float ntz = -self->translation.z;

    const float t10 = (ntx * qw + nqx * 0.0f + nty * nqz) - ntz * nqy;
    const float t11 = (nqy * 0.0f - ntx * nqz) + nty * qw + ntz * nqx;
    const float t7  = ((ntx * nqy + nqz * 0.0f) - nty * nqx) + ntz * qw;
    const float t6  = ((qw * 0.0f - ntx * nqx) - nty * nqy) - ntz * nqz;

    ret->rotation.x = nqx;
    ret->rotation.y = nqy;
    ret->rotation.z = nqz;
    ret->rotation.w = qw;
    ret->translation.x = (t6 * qx + t10 * qw + t7 * qy) - t11 * qz;
    ret->translation.y = (t11 * qw - t7 * qx) + t6 * qy + t10 * qz;
    ret->translation.z = ((t11 * qx + t7 * qw) - t10 * qy) + t6 * qz;
}

// The sqrtf retail calls at 0x180128110 is the CRT's, which for a finite
// non-negative argument is the SQRTSS instruction (its whole prologue is the
// NaN / negative / infinity error path into `_invoke_matherr`). Intersect only
// ever hands it a sum of three squares, so this is bit-identical. Written with
// __builtin_sqrtf so no libm call appears on the pick path — the MinGW libm
// detour that cost this project real time (project_stress_perf_rootonly_and_libcalls.md).
inline float RtSqrtf(float v) { return __builtin_sqrtf(v); }

// ============================================================================
// Self-describing post-state predicates.
//
// A shadow verdict is only as good as its message. This file previously built
// its `a`/`b` strings from three hand-picked fields while the verdict itself
// was a conjunction of fourteen, so a failure printed two IDENTICAL strings and
// said nothing about which term fired -- which is how the RemoveMesh
// uninitialised-field finding below cost a whole extra integration run to
// localise. Every shadow site in this file now goes through these two helpers,
// which report the FIRST failing term by name, with its actual and expected
// value. If you add a term, add it here and it describes itself.
// ============================================================================
struct PostCheck {
    const char* name;
    long long   got;
    long long   want;
};

// Returns the index of the first failing check, or -1 if all pass. Fills `a`
// and `b` with a rendering of that one term (ours / expected).
int RtFirstFailure(const PostCheck* checks, int n,
                   char* a, size_t na, char* b, size_t nb) {
    for (int i = 0; i < n; ++i) {
        if (checks[i].got != checks[i].want) {
            std::snprintf(a, na, "%s=%lld", checks[i].name, checks[i].got);
            std::snprintf(b, nb, "%s=%lld", checks[i].name, checks[i].want);
            return i;
        }
    }
    return -1;
}

// Byte offset of the first difference between two buffers, or -1.
long long RtFirstByteDiff(const void* x, const void* y, size_t n) {
    const uint8_t* a = (const uint8_t*)x;
    const uint8_t* b = (const uint8_t*)y;
    for (size_t i = 0; i < n; ++i)
        if (a[i] != b[i]) return (long long)i;
    return -1;
}

// Index of the first differing element between two int arrays, or -1.
long long RtFirstIntDiff(const int32_t* x, const int32_t* y, int n) {
    for (int i = 0; i < n; ++i)
        if (x[i] != y[i]) return i;
    return -1;
}

// Scene+0x48 accessors, spelled once so a wrong stride cannot be written twice.
inline Entry* EntryAt(Scene* s, int32_t idx) { return &s->_entries._data[idx]; }
inline int32_t LinkEntry(Scene* s, int32_t link) { return s->_links._data[link]._entry; }
inline int32_t LinkNext(Scene* s, int32_t link)  { return s->_links._data[link]._next; }

}  // namespace

// ============================================================================
// Picking::Scene::Scene()
// retail 0x1801258e0, ordinal 376   ??0Scene@Picking@@QEAA@XZ
//
// The whole retail body is `FUN_18007aef0(this, 16.0f); return this;`, and
// FUN_18007aef0 is a shared grid initialiser whose fourteen stores are
// transcribed below (see the header's whole-object witness). It is inlined
// here rather than called so the ctor is a real port with a real oracle — a
// forwarder would have nothing to diff.
//
// NOTE what it does NOT write: +0x2c and +0x44 are left uninitialised, in
// retail and here. Any byte-wise comparison of a Scene must skip them, and the
// oracle below does.
// ============================================================================
extern "C" Scene* RtPicking_Scene_Ctor(Scene* self) {
    RT_DIFF_BEGIN("Picking::Scene::Scene()");

    self->_cellSize    = kCellSize;
    self->_invCellSize = kOne / kCellSize;

    self->_cellHeads._data = nullptr;
    self->_cellHeads._size = 0;
    self->_cellHeads._capacity = 0;

    self->_cells._data = nullptr;
    self->_cells._size = 0;
    self->_cells._capacity = 0;
    self->_freeCell = -1;

    self->_links._data = nullptr;
    self->_links._size = 0;
    self->_links._capacity = 0;
    self->_freeLink = -1;

    self->_entries._data = nullptr;
    self->_entries._size = 0;
    self->_entries._capacity = 0;
    self->_freeEntry = -1;

    self->_queryStamp = 0;

    RtCellHeadsInit(&self->_cellHeads);

    // ---- ORACLE: full, against a private scratch Scene -----------------
    // The ctor mutates only the object it is handed, so the original can be
    // run for real into a stack Scene and compared field for field. The two
    // bucket buffers are different heap allocations, so the comparison is
    // "every field except the four Array _data pointers, plus the 10,000
    // bucket heads by content". Theirs is destroyed with the DLL's own dtor
    // so the scratch does not leak its 40 KB on every construction.
    if (rt_diff_on__) {
        Scene theirs;
        std::memset(&theirs, 0xCD, sizeof(theirs));
        typedef Scene* (*CtorFn)(Scene*);
        RtOriginal<CtorFn>(kSceneCtor)(&theirs);

        // Every term names itself, so a failure says WHICH field differed
        // rather than printing two strings that agree (see RtFirstFailure).
        // The four Array _data pointers are excluded on purpose -- they are
        // different heap allocations -- except that the bucket buffer's
        // CONTENT is compared element by element as its own term.
        const long long headsDiff =
            (self->_cellHeads._data && theirs._cellHeads._data &&
             self->_cellHeads._size == theirs._cellHeads._size)
                ? RtFirstIntDiff(self->_cellHeads._data, theirs._cellHeads._data,
                                 self->_cellHeads._size)
                : ((self->_cellHeads._data == nullptr) ==
                   (theirs._cellHeads._data == nullptr) ? -1 : -2);
        const PostCheck checks[] = {
            { "_cellSize(bits)",      (long long)*(const uint32_t*)&self->_cellSize,
                                      (long long)*(const uint32_t*)&theirs._cellSize },
            { "_invCellSize(bits)",   (long long)*(const uint32_t*)&self->_invCellSize,
                                      (long long)*(const uint32_t*)&theirs._invCellSize },
            { "_cellHeads._size",     self->_cellHeads._size,     theirs._cellHeads._size },
            { "_cellHeads._capacity", self->_cellHeads._capacity, theirs._cellHeads._capacity },
            { "_cellHeads[i] first differing i", headsDiff, -1 },
            { "_cells._data",     (long long)(intptr_t)self->_cells._data,
                                  (long long)(intptr_t)theirs._cells._data },
            { "_cells._size",     self->_cells._size,     theirs._cells._size },
            { "_cells._capacity", self->_cells._capacity, theirs._cells._capacity },
            { "_freeCell",        self->_freeCell,        theirs._freeCell },
            { "_links._data",     (long long)(intptr_t)self->_links._data,
                                  (long long)(intptr_t)theirs._links._data },
            { "_links._size",     self->_links._size,     theirs._links._size },
            { "_links._capacity", self->_links._capacity, theirs._links._capacity },
            { "_freeLink",        self->_freeLink,        theirs._freeLink },
            { "_entries._data",   (long long)(intptr_t)self->_entries._data,
                                  (long long)(intptr_t)theirs._entries._data },
            { "_entries._size",     self->_entries._size,     theirs._entries._size },
            { "_entries._capacity", self->_entries._capacity, theirs._entries._capacity },
            { "_freeEntry",       self->_freeEntry,       theirs._freeEntry },
            { "_queryStamp",      self->_queryStamp,      theirs._queryStamp },
        };
        char a[96], b[96];
        const int bad = RtFirstFailure(checks, (int)(sizeof(checks) / sizeof(checks[0])),
                                       a, sizeof(a), b, sizeof(b));
        if (bad < 0) {
            std::snprintf(a, sizeof(a), "cell=%g inv=%g heads=%d", self->_cellSize,
                          self->_invCellSize, self->_cellHeads._size);
            std::snprintf(b, sizeof(b), "cell=%g inv=%g heads=%d", theirs._cellSize,
                          theirs._invCellSize, theirs._cellHeads._size);
        }
        RT_DIFF_SHADOW(bad < 0, a, b);

        typedef void (*DtorFn)(Scene*);
        RtOriginal<DtorFn>(kSceneDtor)(&theirs);
    }
    return self;   // retail returns `this` in RAX (MOV RAX,RBX at 0x180125904)
}

// ============================================================================
// Picking::Scene::~Scene()
// retail 0x180125890, ordinal 672   ??1Scene@Picking@@QEAA@XZ
//
// Four calls and nothing else, in this order — the fourth is a tail JMP, which
// is why the function returns void and not `this`. Each callee is one
// instantiation of Array<T>::~Array; the +0x48 one additionally destroys every
// Entry's inner Array<Transform> before freeing the entry buffer. They stay in
// the DLL (see the file banner).
//
// ORACLE: post-state only, and that is all it can be. Running the original
// afterwards would double-free. What is checked is what retail's body must
// leave behind: all four Arrays cleared. That catches a wrong offset or a
// missed call; it does NOT prove the frees themselves happened correctly —
// only the allocator watchdog (BANISHED_REDZONE=watch) can say that.
// ============================================================================
extern "C" void RtPicking_Scene_Dtor(Scene* self) {
    RT_DIFF_BEGIN("Picking::Scene::~Scene()");

    RtFreeArrayEntry(&self->_entries);   // +0x48
    RtFreeArrayLink(&self->_links);      // +0x30
    RtFreeArrayCell(&self->_cells);      // +0x18
    RtFreeArrayInt(&self->_cellHeads);   // +0x08

    if (rt_diff_on__) {
        const PostCheck checks[] = {
            { "_entries._data",     (long long)(intptr_t)self->_entries._data,   0 },
            { "_entries._size",     self->_entries._size,      0 },
            { "_entries._capacity", self->_entries._capacity,  0 },
            { "_links._data",       (long long)(intptr_t)self->_links._data,     0 },
            { "_links._size",       self->_links._size,        0 },
            { "_links._capacity",   self->_links._capacity,    0 },
            { "_cells._data",       (long long)(intptr_t)self->_cells._data,     0 },
            { "_cells._size",       self->_cells._size,        0 },
            { "_cells._capacity",   self->_cells._capacity,    0 },
            { "_cellHeads._data",   (long long)(intptr_t)self->_cellHeads._data, 0 },
            { "_cellHeads._size",   self->_cellHeads._size,    0 },
            { "_cellHeads._capacity", self->_cellHeads._capacity, 0 },
        };
        char a[96], b[96];
        const int bad = RtFirstFailure(checks, (int)(sizeof(checks) / sizeof(checks[0])),
                                       a, sizeof(a), b, sizeof(b));
        if (bad < 0) {
            std::snprintf(a, sizeof(a), "all four arrays cleared");
            std::snprintf(b, sizeof(b), "all four arrays cleared");
        }
        RT_DIFF_SHADOW(bad < 0, a, b);
    }
}

// ============================================================================
// Picking::Scene::GetUserData(unsigned int) const
// retail 0x1800722d0, ordinal 2998   ?GetUserData@Scene@Picking@@QEBAPEAXI@Z
//
// One load. Retail does NOT range-check `id`, and neither does this — CLAUDE.md
// "Don't fix a faithful crash with a null check": an out-of-range id here is an
// upstream bug and a guard would hide it.
//
// ORACLE: full. Pure read, so both run and the pointers are compared.
// ============================================================================
extern "C" void* RtPicking_Scene_GetUserData(const Scene* self, uint32_t id) {
    RT_DIFF_BEGIN("Picking::Scene::GetUserData");
    void* ours = self->_entries._data[id]._userData;
    if (rt_diff_on__) {
        typedef void* (*Fn)(const Scene*, uint32_t);
        void* theirs = RtOriginal<Fn>(kGetUserData)(self, id);
        char a[64], b[64];
        RtDiffFormat(a, sizeof(a), (const void*)ours);
        RtDiffFormat(b, sizeof(b), (const void*)theirs);
        RtDiffReport(&rt_diff_site__, ours == theirs, a, b);
    }
    return ours;
}

// ============================================================================
// Picking::PickingMesh::Vertices(int) const
// retail 0x180071e10, ordinal 4536
//   ?Vertices@PickingMesh@Picking@@QEBAAEBV?$Array@VVector3@Math@@@Collection@@H@Z
//
// Returns a REFERENCE, so there is NO hidden return slot — the whole body is
// `return _nodes + 0x40 + i*0x80` in RAX. This is the distinction
// reference_hidden_retslot_wild_write.md is about, and getting it backwards
// here would be a wild write on every call: the caller would pass its buffer
// in RDX and we would treat RDX as the sub-mesh index.
// `tools/abi_retslot_audit.py` is clean on this file.
//
// ORACLE: full. Pure read, both run, pointers compared.
// ============================================================================
extern "C" const Collection::Array<Vector3>*
RtPicking_PickingMesh_Vertices(const RtPicking::PickingMesh* self, int subMesh) {
    RT_DIFF_BEGIN("Picking::PickingMesh::Vertices");
    const Collection::Array<Vector3>* ours = &self->_nodes[subMesh]._vertices;
    if (rt_diff_on__) {
        typedef const Collection::Array<Vector3>* (*Fn)(const RtPicking::PickingMesh*, int);
        const Collection::Array<Vector3>* theirs = RtOriginal<Fn>(kVertices)(self, subMesh);
        char a[64], b[64];
        RtDiffFormat(a, sizeof(a), (const void*)ours);
        RtDiffFormat(b, sizeof(b), (const void*)theirs);
        RtDiffReport(&rt_diff_site__, ours == theirs, a, b);
    }
    return ours;
}

// ============================================================================
// Picking::Scene::SetMeshUserData(unsigned int, void*, int, int, callback)
// retail 0x180125240, ordinal 3917
//
// Four stores. NOTE the argument count: five after `this`, so the callback is
// the SIXTH register argument and lands on the stack. A signature that dropped
// it would leave entry+0x40 holding whatever the caller happened to have there
// — the "a pointer parameter typed int is only wrong from the 5th argument on"
// trap (reference_hidden_retval_and_int_pointer_args.md), which this lane's
// brief flagged for exactly these two functions.
//
// ORACLE: full, by IDEMPOTENT REPLAY. The four stores depend on nothing but
// their arguments, so running the original immediately afterwards with the
// same arguments must be a no-op. Snapshot the whole 0x48-byte entry after
// ours, run theirs, compare. If ours wrote the right values to the WRONG
// offsets, theirs then writes them to the right ones and the bytes differ.
// The one thing this cannot catch is ours writing to a wrong ENTRY (a wrong
// 0x48 stride) that theirs would also write to — the static_asserts and
// GetUserData's full oracle cover that.
// ============================================================================
extern "C" void RtPicking_Scene_SetMeshUserData(Scene* self, uint32_t id, void* userData,
                                                int userValue, int userValue2,
                                                HitCallback callback) {
    RT_DIFF_BEGIN("Picking::Scene::SetMeshUserData");

    Entry* e = &self->_entries._data[id];
    e->_userData   = userData;
    e->_userValue  = userValue;
    e->_userValue2 = userValue2;
    e->_callback   = callback;

    if (rt_diff_on__) {
        uint8_t before[sizeof(Entry)];
        std::memcpy(before, e, sizeof(Entry));
        typedef void (*Fn)(Scene*, uint32_t, void*, int, int, HitCallback);
        RtOriginal<Fn>(kSetMeshUD)(self, id, userData, userValue, userValue2, callback);
        RT_DIFF_BYTES(before, e, sizeof(Entry));
    }
}

// ============================================================================
// Picking::Scene::SetTransform(unsigned int, const Transform&)
// retail 0x1800721b0, ordinal 3993
//
// Seven dwords into the entry's ROOT sub-mesh transform (transforms[0]), then
// the grid re-index. It does NOT touch the other sub-mesh transforms, so a
// mesh whose root moves keeps stale child transforms until AddMesh rebuilds
// them — that is retail's behaviour and is reproduced here.
//
// ORACLE: full, by idempotent replay, as for SetMeshUserData. Here the replay
// is doubly informative: theirs re-runs the grid re-index, which is a no-op
// only if ours left the entry's cell rectangle where retail would have. The
// re-index itself is the DLL's, so what is really under test is the seven
// stores and the entry address — which is exactly the risk.
// ============================================================================
extern "C" void RtPicking_Scene_SetTransform(Scene* self, uint32_t id,
                                             const ::Transform* xform) {
    RT_DIFF_BEGIN("Picking::Scene::SetTransform");

    uint32_t* dst = reinterpret_cast<uint32_t*>(self->_entries._data[id]._transforms._data);
    const uint32_t* src = reinterpret_cast<const uint32_t*>(xform);
    dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2]; dst[3] = src[3];
    dst[4] = src[4]; dst[5] = src[5]; dst[6] = src[6];
    RtGridReindex(self, id);

    if (rt_diff_on__) {
        Entry* e = &self->_entries._data[id];
        uint8_t before[sizeof(Entry) + sizeof(::Transform)];
        std::memcpy(before, e, sizeof(Entry));
        std::memcpy(before + sizeof(Entry), e->_transforms._data, sizeof(::Transform));
        typedef void (*Fn)(Scene*, uint32_t, const ::Transform*);
        RtOriginal<Fn>(kSetTransform)(self, id, xform);
        uint8_t after[sizeof(Entry) + sizeof(::Transform)];
        std::memcpy(after, e, sizeof(Entry));
        std::memcpy(after + sizeof(Entry), e->_transforms._data, sizeof(::Transform));
        RT_DIFF_BYTES(before, after, sizeof(before));
    }
}

// ============================================================================
// Picking::Scene::AddMesh(ExternalPtr<PickingMesh>, const Transform&, void*,
//                         int, int, callback)
// retail 0x1801252e0, ordinal 1571
//
// Flattens the mesh's sub-mesh hierarchy into a scratch Array<Transform> —
// node 0 gets the caller's root transform verbatim, node i gets
// worldOf(parent(i)) composed with node i's local transform — then hands the
// scratch, plus the user data, to the grid inserter, which DEEP-COPIES it into
// the new entry. That deep copy is why the scratch is freed on the way out.
//
// ABI: the ExternalPtr is passed BY VALUE and is one qword, so it occupies RDX
// like any pointer; the callback is the seventh argument and is on the stack.
//
// ORACLE: full on the only thing this body computes — the flattened transform
// array. The insert mutates the live Scene, so the original cannot be run on
// it; instead theirs runs into a PRIVATE scratch Scene (constructed once by
// the DLL's own ctor) and the two entries' transform arrays are compared
// bit-for-bit, after which the scratch entry is removed again so the scratch
// stays bounded. The scratch Scene itself leaks one 0x60 object plus its
// buckets for the life of the process, under BANISHED_RT_DIFF only.
// ============================================================================
namespace {
// Lazily constructed by the DLL's own ctor, so the oracle never depends on the
// port under test. Diff-mode only.
Scene* DiffScratchScene() {
    // Guard the helper ITSELF, not just its callers.  Every caller today is a
    // diff path and the comment above says so -- but that invariant lives in
    // several places, and the RtOriginal below is the RAW door: an unguarded
    // call is a live cross-DLL call on an ordinary run and a call through NULL
    // once Runtime-x64.dll is gone.  Identical to the repair made to
    // RtDiffRandomShadow in value_types.cpp, and rawdoor_audit recognises this
    // early-out as a guard (its diff_only_fn_depth idiom) where it cannot
    // recognise one living in the CALLER.
    if (!RtDiffEnabled()) return nullptr;

    static Scene* scratch = nullptr;
    if (!scratch) {
        static Scene storage;
        typedef Scene* (*CtorFn)(Scene*);
        RtOriginal<CtorFn>(kSceneCtor)(&storage);
        scratch = &storage;
    }
    return scratch;
}
}  // namespace

extern "C" uint32_t RtPicking_Scene_AddMesh(Scene* self, void* mesh,
                                            const ::Transform* rootXform, void* userData,
                                            int userValue, int userValue2,
                                            HitCallback callback) {
    RT_DIFF_BEGIN("Picking::Scene::AddMesh");

    const RtPicking::PickingMesh* pm = reinterpret_cast<const RtPicking::PickingMesh*>(mesh);

    AddDesc desc;
    desc._transforms._data = nullptr;
    desc._transforms._size = 0;
    desc._transforms._capacity = 0;
    desc._userValue  = userValue;
    desc._userValue2 = userValue2;
    desc._callback   = callback;
    desc._mesh       = mesh;
    desc._userData   = userData;

    RtTransformArraySetSize(&desc._transforms, pm->_nodeCount);
    ::Transform* dst = desc._transforms._data;

    // Node 0 is the root: the caller's transform, copied dword for dword.
    {
        uint32_t* d = reinterpret_cast<uint32_t*>(dst);
        const uint32_t* s = reinterpret_cast<const uint32_t*>(rootXform);
        d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = s[3];
        d[4] = s[4]; d[5] = s[5]; d[6] = s[6];
    }

    // worldOf(i) = worldOf(parent(i)) * localOf(i), transliterated from retail.
    // The rotation goes through the quaternion product; the translation is an
    // inlined quaternion-rotate of the local offset by the parent rotation,
    // added to the parent's translation. Retail's association order is kept
    // verbatim — this is float arithmetic under a bit-exact oracle.
    if (desc._transforms._size > 1) {
        for (int i = 1; i < desc._transforms._size; ++i) {
            const SubMesh* node = &pm->_nodes[i];
            const ::Transform* parent = &dst[node->_parent];

            const float pw = parent->rotation.w;
            const float pz = parent->rotation.z, npz = -pz;
            const float py = parent->rotation.y, npy = -py;
            const float px = parent->rotation.x, npx = -px;

            const float lx = node->_local.translation.x;
            const float ly = node->_local.translation.y;
            const float lz = node->_local.translation.z;

            const float t19 = ((pw * 0.0f - lx * px) - ly * py) - lz * pz;
            const float t18 = ((lx * py + pz * 0.0f) - ly * px) + lz * pw;
            const float t17 = (py * 0.0f - lx * pz) + ly * pw + lz * px;
            const float t23 = (lx * pw + px * 0.0f + ly * pz) - lz * py;

            const float ptz = parent->translation.z;
            const float pty = parent->translation.y;
            const float ptx = parent->translation.x;

            QuatMul(&parent->rotation, &dst[i].rotation, &node->_local.rotation);

            dst[i].translation.x = ((t23 * pw + npx * t19 + t18 * npy) - t17 * npz) + ptx;
            dst[i].translation.y = (t17 * pw - npx * t18) + t19 * npy + t23 * npz + pty;
            dst[i].translation.z = ((npx * t17 + t18 * pw) - t23 * npy) + t19 * npz + ptz;
        }
    }

    const uint32_t id = RtGridAdd(self, &desc);

    // ---- ORACLE, before the scratch is released -------------------------
    if (rt_diff_on__) {
        Scene* scratch = DiffScratchScene();
        typedef uint32_t (*AddFn)(Scene*, void*, const ::Transform*, void*, int, int,
                                  HitCallback);
        const uint32_t theirId = RtOriginal<AddFn>(kAddMesh)(
            scratch, mesh, rootXform, userData, userValue, userValue2, callback);

        const Entry* ourEntry   = &self->_entries._data[id];
        const Entry* theirEntry = &scratch->_entries._data[theirId];
        // Same self-describing shape as the other shadow sites: the first
        // failing term names itself. The transform arrays live at different
        // addresses, so their CONTENT is compared as one term reporting the
        // byte offset of the first difference.
        long long xformDiff = -1;
        if (ourEntry->_transforms._size == theirEntry->_transforms._size &&
            ourEntry->_transforms._size > 0) {
            const uint8_t* x = (const uint8_t*)ourEntry->_transforms._data;
            const uint8_t* y = (const uint8_t*)theirEntry->_transforms._data;
            const size_t n = (size_t)ourEntry->_transforms._size * sizeof(::Transform);
            for (size_t k = 0; k < n; ++k)
                if (x[k] != y[k]) { xformDiff = (long long)k; break; }
        }
        const PostCheck checks[] = {
            { "_transforms._size", ourEntry->_transforms._size, theirEntry->_transforms._size },
            { "transforms first differing byte", xformDiff, -1 },
            { "_mesh",       (long long)(intptr_t)ourEntry->_mesh,
                             (long long)(intptr_t)theirEntry->_mesh },
            { "_userData",   (long long)(intptr_t)ourEntry->_userData,
                             (long long)(intptr_t)theirEntry->_userData },
            { "_userValue",  ourEntry->_userValue,  theirEntry->_userValue },
            { "_userValue2", ourEntry->_userValue2, theirEntry->_userValue2 },
            { "_callback",   (long long)(intptr_t)ourEntry->_callback,
                             (long long)(intptr_t)theirEntry->_callback },
        };
        char a[96], b[96];
        const int bad = RtFirstFailure(checks, (int)(sizeof(checks) / sizeof(checks[0])),
                                       a, sizeof(a), b, sizeof(b));
        if (bad < 0) {
            std::snprintf(a, sizeof(a), "id=%u nodes=%d", id, ourEntry->_transforms._size);
            std::snprintf(b, sizeof(b), "id=%u nodes=%d", theirId, theirEntry->_transforms._size);
        }
        RT_DIFF_SHADOW(bad < 0, a, b);

        typedef void (*RemoveFn)(Scene*, uint32_t);
        RtOriginal<RemoveFn>(kRemoveMesh)(scratch, theirId);
    }

    RtFreeScratchTransforms(&desc._transforms);   // see the helper's comment
    return id;
}

// ============================================================================
// Picking::Scene::RemoveMesh(unsigned int)
// retail 0x180125290, ordinal 3560   ?RemoveMesh@Scene@Picking@@QEAAXI@Z
//
// NOT in this lane's original assignment. It was found by lane E's sweep of the
// 246 `ResolveAddr(Module::Runtime, …)` sites and confirmed here: our tree calls
// it from `floorcomponent.cpp:308` and `pickingcomponent.cpp:344`, both by RAW
// ADDRESS and never by export name, which is exactly why a scope list built by
// grepping `ResolveExport` could not see it. Wave 5's real Picking scope is 9.
//
// It has to be ported rather than left behind. `AddMesh` and `RemoveMesh` are
// the two halves of one free list — `AddMesh` pops `Scene::_freeEntry` and
// `RemoveMesh` pushes onto it. Leaving one on the DLL while the other is ours
// would not fail at the seam; it would hand out a colliding or stale mesh id
// and surface later as a wrong pick or a `_userData` pointing at a freed
// entity. `pickingcomponent.cpp:336-342` records that this class has ALREADY
// produced that exact crash once (2026-07-21, a null RemoveMesh left a removed
// building's pick mesh dangling and the foundation click died in
// EditTool_SelectEntity_Impl).
//
// The retail body is two calls and nothing else, both to shared grid internals
// that stay in the DLL for the reason in the file banner.
//
// ORACLE: post-state, and that is the ceiling. Running the original afterwards
// would release an already-free entry and corrupt the free list — the same
// double-free shape as the dtor. What is checked is everything retail's two
// callees must leave behind: the entry fully zeroed (0x18007b390 assigns an
// all-zero Entry over it) and relinked at the head of the free list with the
// previous head in its `_nextFree`. That catches a wrong `id`, a wrong 0x48
// stride and a missed call. It does NOT check that the entry was unthreaded
// from its cells — that would mean walking the whole cell hash, and a stale
// Link is precisely the failure this port is meant to avoid, so it is stated
// here rather than left implied.
// ============================================================================
extern "C" void RtPicking_Scene_RemoveMesh(Scene* self, uint32_t id) {
    RT_DIFF_BEGIN("Picking::Scene::RemoveMesh");

    const int32_t prevFree = rt_diff_on__ ? self->_freeEntry : 0;

    Entry* e = &self->_entries._data[id];
    const uint16_t w = e->_cellW;
    const uint16_t h = e->_cellH;
    RtGridUnthread(self, &e->_cellX, w, h, (int32_t)id);
    RtGridFreeEntry(self, id);

    if (rt_diff_on__) {
        // *** ONE TERM OF THIS PREDICATE USED TO BE WRONG ABOUT RETAIL. ***
        // It asserted `_userValue2 == 0` and reported 16 divergences in 17
        // calls on a tutorial run while every field it PRINTED agreed. The port
        // was right and the predicate was wrong, and the reason is in retail's
        // own bytes rather than in ours.
        //
        // 0x18007b390 builds an all-zero 0x48 Entry on its stack at RSP+0x28
        // and copies it over the entry. Its eleven initialising stores are, in
        // order (bytes read out of the shipped DLL at 0x18007b3af):
        //
        //   89 4C 24 28              [RSP+0x28] = 0        template +0x00
        //   C7 44 24 2C FFFFFFFF     [RSP+0x2c] = -1       template +0x04
        //   49 89 4B B8              [R11-0x48] = 0 (q)    template +0x08
        //   89 4C 24 38              [RSP+0x38] = 0        template +0x10
        //   49 89 4B C8/D0/D8/E0     four qwords = 0       template +0x18..+0x37
        //   89 4C 24 60              [RSP+0x60] = 0        template +0x38
        //   49 89 4B F0              [R11-0x10] = 0 (q)    template +0x40
        //
        // The dword at RSP+0x64 -- template +0x3c, `_userValue2` -- is NEVER
        // WRITTEN. And 0x18007b590, the field-by-field Entry copy, DOES copy it
        // (`MOV EAX,[RBX+0x3c]; MOV [RDI+0x3c],EAX` at 0x18007b5e5). So every
        // retail RemoveMesh stores an UNINITIALISED STACK DWORD into
        // `Entry::_userValue2`. That is retail's behaviour, not a port defect,
        // and it is why call #1 passed (a fresh stack slot that happened to be
        // zero) and #2 onward failed.
        //
        // It is harmless, which is presumably why it shipped: `_userValue2` has
        // no exported getter -- `GetUserValue` (0x1800722e0) reads +0x38, not
        // +0x3c -- and the entry allocator 0x18007b210 overwrites +0x3c from
        // the descriptor before the slot is handed out again. Nothing can
        // observe the garbage. So the term is DROPPED, not weakened: there is
        // no value it could assert.
        //
        // +0x14 is likewise not checked, for a different reason: 0x18007b590
        // skips +0x14..+0x17 entirely, so the entry KEEPS its old bytes there.
        const PostCheck checks[] = {
            { "scene._freeEntry",           self->_freeEntry,               (long long)id },
            { "entry._nextFree",            e->_nextFree,                   prevFree },
            { "entry._queryStamp",          e->_queryStamp,                 0 },
            { "entry._cellX",               e->_cellX,                      0 },
            { "entry._cellY",               e->_cellY,                      0 },
            { "entry._cellW",               e->_cellW,                      0 },
            { "entry._cellH",               e->_cellH,                      0 },
            { "entry._mesh",                (long long)(intptr_t)e->_mesh,  0 },
            { "entry._transforms._data",    (long long)(intptr_t)e->_transforms._data, 0 },
            { "entry._transforms._size",    e->_transforms._size,           0 },
            { "entry._transforms._capacity", e->_transforms._capacity,      0 },
            { "entry._userData",            (long long)(intptr_t)e->_userData, 0 },
            { "entry._userValue",           e->_userValue,                  0 },
            { "entry._callback",            (long long)(intptr_t)e->_callback, 0 },
            // _userValue2 (+0x3c) deliberately absent -- see above.
        };
        char a[96], b[96];
        const int bad = RtFirstFailure(checks, (int)(sizeof(checks) / sizeof(checks[0])),
                                       a, sizeof(a), b, sizeof(b));
        if (bad < 0) {
            std::snprintf(a, sizeof(a), "id=%u released, free list head", id);
            std::snprintf(b, sizeof(b), "id=%u released, free list head", id);
        }
        RT_DIFF_SHADOW(bad < 0, a, b);
    }
}

// ============================================================================
// Picking::Scene::Intersect(const Vector3& start, const Vector3& dir,
//                           const Scalar& maxDist, Vector3& outPos,
//                           Vector3& outNormal) const
// retail 0x1801246c0, ordinal 3081
//
// A ray cast over the grid. For every entry whose cell rectangle the ray's
// XY-projected extent touches, for every sub-mesh, the ray is pulled into the
// sub-mesh's local space by the INVERSE of its world transform and tested
// against every triangle with a Moller-Trumbore variant. The nearest hit
// within `maxDist` wins; the return value is the ENTRY index (what GetUserData
// and SetTransform take), or -1.
//
// *** IT IS DECLARED const AND IT MUTATES THE SCENE. *** `INC dword ptr
// [RCX+0x5c]` at 0x1801247bf allocates a fresh query stamp, and each visited
// entry's +0x00 is set to it so an entry spanning several cells is tested
// once. That is why the port takes a const Scene* and casts: the constness is
// retail's, and it is a lie.
//
// ORACLE: FULL, tolerance 0. The double-run is safe precisely BECAUSE of the
// stamp: every call allocates a new one before use, so the second run sees a
// state indistinguishable from a virgin one, and the only lasting difference
// is that _queryStamp advanced by two instead of one. The out-parameters are
// snapshotted before ours runs and theirs is given that snapshot, so a MISS —
// where retail writes neither output — compares equal instead of comparing
// our caller's buffer against a zeroed scratch.
//
// Bit equality is demanded and no tolerance is passed: this is a faithful
// transliteration of retail's own float expressions, in retail's association
// order, and anything less than bit-exact means a term was moved.
// ============================================================================
extern "C" int RtPicking_Scene_Intersect(const Scene* cself, const Vector3* start,
                                         const Vector3* dir, const Scalar* maxDist,
                                         Vector3* outPos, Vector3* outNormal);

namespace {
// The deterministic ray fixture; see the header block above kOne. Casts a ray
// set built entirely from constants through the real Intersect, so the oracle
// diffs each one and the denominator is identical on every run of the same
// seed. Re-entrancy is blocked by g_pickSweepRunning, so the sweep's own calls
// do not re-arm it.
void RtPickingRunSweep(Scene* self) {
    g_pickSweepRunning = true;

    const float cx   = PickSweepEnvF("BANISHED_PICK_SWEEP_CX",    0.0f);
    const float cy   = PickSweepEnvF("BANISHED_PICK_SWEEP_CY",    0.0f);
    const float half = PickSweepEnvF("BANISHED_PICK_SWEEP_HALF", 60.0f);
    const float step = PickSweepEnvF("BANISHED_PICK_SWEEP_STEP", 6.0f);
    const float z0   = PickSweepEnvF("BANISHED_PICK_SWEEP_Z",   200.0f);
    const float dist = PickSweepEnvF("BANISHED_PICK_SWEEP_DIST", 600.0f);

    // CAMERA-SHAPED RAYS, not vertical drops. The first version of this fixture
    // fired 2,205 straight-down rays and its effectiveness control said so —
    // 8 hits. The second aimed them at the right place and got 525 hits and no
    // divergences, which was still the wrong experiment: a real pick ray comes
    // from an oblique camera, and the one diverging ray recorded on this route
    // hit a face whose normal was (0.464, 0, 0.886) — 27.7 degrees off vertical.
    // So the ray set is now an ORIGIN RING crossed with a TARGET GRID: 24
    // origins on a hemisphere of radius `ringR` (3 elevations x 8 azimuths),
    // each aimed at every point of the target grid. That is the shape of a
    // camera pick, and it is still built entirely from constants.
    static const float kElevCos[3] = { 0.86602539f, 0.70710678f, 0.5f };  // 30/45/60 deg
    static const float kElevSin[3] = { 0.5f, 0.70710678f, 0.86602539f };
    static const float kAzC[8] = { 1.0f, 0.70710678f, 0.0f, -0.70710678f,
                                  -1.0f, -0.70710678f, 0.0f, 0.70710678f };
    static const float kAzS[8] = { 0.0f, 0.70710678f, 1.0f, 0.70710678f,
                                   0.0f, -0.70710678f, -1.0f, -0.70710678f };
    const float ringR = PickSweepEnvF("BANISHED_PICK_SWEEP_RING", 60.0f);

    long long rays = 0, hits = 0;
    const unsigned long long divBefore =
        g_pickIntersectSite ? g_pickIntersectSite->divergences : 0;
    int printed = 0;

    for (int d = 0; d < 24; ++d) {
        const int el = d / 8, az = d % 8;
        // Origin on the ring: horizontal offset ringR*cosElev, height ringR*sinElev.
        const Vector3 org = { cx + ringR * kElevCos[el] * kAzC[az],
                              cy + ringR * kElevCos[el] * kAzS[az],
                              z0 + ringR * kElevSin[el] };
        const int n = (int)(2.0f * half / step) + 1;
        for (int iy = 0; iy < n; ++iy) {
            for (int ix = 0; ix < n; ++ix) {
                // Integer-stepped so the grid is identical bit for bit on every
                // run; a `for (float x = -half; x <= half; x += step)` loop
                // accumulates rounding and can even change its trip count.
                const float x = cx - half + (float)ix * step;
                const float y = cy - half + (float)iy * step;
                const float vx = x - org.x, vy = y - org.y, vz = 0.0f - org.z;
                const float vlen = RtSqrtf(vx * vx + vy * vy + vz * vz);
                Vector3 dir   = { vx / vlen, vy / vlen, vz / vlen };
                Vector3 start = org;
                Scalar  md    = { dist };
                Vector3 p = { 0.0f, 0.0f, 0.0f }, n = { 0.0f, 0.0f, 0.0f };
                const unsigned long long before =
                    g_pickIntersectSite ? g_pickIntersectSite->divergences : 0;
                const int id = RtPicking_Scene_Intersect(self, &start, &dir, &md, &p, &n);
                ++rays;
                if (id >= 0) ++hits;
                const unsigned long long after =
                    g_pickIntersectSite ? g_pickIntersectSite->divergences : 0;
                if (after != before && printed < 12) {
                    ++printed;
                    std::fprintf(stderr,
                        "[PickSweep] DIVERGED ray#%lld dir=(%.9g,%.9g,%.9g) "
                        "start=(%.9g,%.9g,%.9g) maxDist=%.9g id=%d "
                        "ours p=(%.9g,%.9g,%.9g)\n",
                        rays, dir.x, dir.y, dir.z, start.x, start.y, start.z,
                        md.v, id, p.x, p.y, p.z);
                }
            }
        }
    }

    const unsigned long long divAfter =
        g_pickIntersectSite ? g_pickIntersectSite->divergences : 0;
    // Self-evidencing output: only a binary built from THIS tree can print it.
    std::fprintf(stderr,
        "[PickSweep] fired after %lld real calls: rays=%lld hits=%lld "
        "divergences=%llu c=(%.9g,%.9g) half=%.9g step=%.9g z=%.9g dist=%.9g negctl=%d\n",
        g_pickRealCalls, rays, hits, divAfter - divBefore,
        cx, cy, half, step, z0, dist, PickNegCtl());
    std::fflush(stderr);

    g_pickSweepRunning = false;
}
}  // namespace

extern "C" int RtPicking_Scene_Intersect(const Scene* cself, const Vector3* start,
                                         const Vector3* dir, const Scalar* maxDist,
                                         Vector3* outPos, Vector3* outNormal) {
    RT_DIFF_BEGIN("Picking::Scene::Intersect");
    Scene* self = const_cast<Scene*>(cself);

    if (rt_diff_on__ && !g_pickSweepRunning) {
        g_pickIntersectSite = &rt_diff_site__;
        ++g_pickRealCalls;
        const int arm = PickSweepAfterCalls();
        if (arm > 0 && !g_pickSweepFired && g_pickRealCalls >= arm) {
            g_pickSweepFired = true;
            RtPickingRunSweep(self);
        }
    }

    Vector3 theirPos{}, theirNormal{};
    if (rt_diff_on__) { theirPos = *outPos; theirNormal = *outNormal; }

    // ---- the ray's XY extent, clamped into the +/-1e8 world box ---------
    float oy = start->y;
    float ox = start->x;
    float maxY = oy;      if (oy < kBigNeg)  maxY = kBigNeg;
    float maxX = kBigNeg; if (kBigNeg <= ox) maxX = ox;
    float minY = oy;      if (kBigPos < oy)  minY = kBigPos;
    float minX = kBigPos; if (ox <= kBigPos) minX = ox;

    const float endY = oy + maxDist->v * dir->y;
    const float endX = ox + maxDist->v * dir->x;
    if (maxY <= endY) maxY = endY;
    if (maxX <= endX) maxX = endX;
    if (endY <= minY) minY = endY;
    if (endX <= minX) minX = endX;

    const float inv = self->_invCellSize;
    float bestT = maxDist->v;

    self->_queryStamp = self->_queryStamp + 1;
    const int32_t stamp = self->_queryStamp;
    int bestId = -1;

    // Cell indices are TRUNCATED, not floored (CVTTSS2SI at 0x180124818).
    CellIter it;
    it.minX = (int32_t)(inv * minX);
    it.minY = (int32_t)(inv * minY);
    it.maxX = (int32_t)(inv * maxX);
    it.maxY = (int32_t)(inv * maxY);
    it.curX = it.minX - 1;
    it.curY = it.minY;
    it.stamp = stamp;
    it.link = -1;
    it.scene = self;

    RtCellIterAdvance(&it);
    if (it.link >= 0) EntryAt(self, LinkEntry(self, it.link))->_queryStamp = stamp;
    if (it.link == -1) {
        if (rt_diff_on__) {
            typedef int (*Fn)(const Scene*, const Vector3*, const Vector3*, const Scalar*,
                              Vector3*, Vector3*);
            const int theirs = RtOriginal<Fn>(kIntersect)(cself, start, dir, maxDist,
                                                          &theirPos, &theirNormal);
            const PostCheck checks[] = {
                { "return", -1, theirs },
                { "outPos first differing byte",
                  RtFirstByteDiff(outPos, &theirPos, sizeof(Vector3)), -1 },
                { "outNormal first differing byte",
                  RtFirstByteDiff(outNormal, &theirNormal, sizeof(Vector3)), -1 },
            };
            char a[96], b[96];
            const int bad = RtFirstFailure(checks, 3, a, sizeof(a), b, sizeof(b));
            if (bad < 0) {
                std::snprintf(a, sizeof(a), "miss (-1), out-params untouched");
                std::snprintf(b, sizeof(b), "miss (-1), out-params untouched");
            }
            RT_DIFF_SHADOW(bad < 0, a, b);
        }
        return -1;
    }

    for (;;) {
        const int32_t curLink = it.link;
        Entry* entry = EntryAt(self, LinkEntry(self, curLink));

        int32_t sub = 0;
        if (entry->_transforms._size > 0) {
            do {
                // Re-read the entry through the link each iteration, as retail
                // does (it reloads links[curLink]._entry at the top of every
                // sub-mesh pass rather than hoisting it).
                Entry* e = EntryAt(self, LinkEntry(self, curLink));
                const RtPicking::PickingMesh* pm =
                    reinterpret_cast<const RtPicking::PickingMesh*>(e->_mesh);
                const ::Transform* world = &e->_transforms._data[sub];

                ::Transform invX;
                TransformInverse(world, &invX);
                const float iqx = invX.rotation.x, iqy = invX.rotation.y;
                const float iqz = invX.rotation.z, iqw = invX.rotation.w;
                const float niqx = -iqx, niqy = -iqy, niqz = -iqz;

                // ray ORIGIN into sub-mesh local space
                float px = start->x, py = start->y, pz = start->z;
                float c26 = (iqw * px + iqx * 0.0f + py * iqz) - pz * iqy;
                float c23 = (iqy * 0.0f - px * iqz) + iqw * py + pz * iqx;
                float c19 = ((iqw * 0.0f - px * iqx) - py * iqy) - pz * iqz;
                float c20 = ((px * iqy + iqz * 0.0f) - py * iqx) + iqw * pz;
                // *** THE TRANSLATION IS ADDED LAST, TO THE FULLY ACCUMULATED
                // ROTATED PART — Wave 9 lane N, and this was the Finding 2
                // defect. Retail 0x180124B56-0x180124BB4 loads each translation
                // component and performs exactly ONE `addss` against a register
                // that already holds the whole rotated sum:
                //     movss xmm0,[rbp-0x58] / addss xmm0,xmm3   -> local.x
                //     movss xmm0,[rbp-0x54] / addss xmm0,xmm2   -> local.y
                //     movss xmm0,[rbp-0x50] / addss xmm0,xmm7   -> local.z
                // `localOx` already had that shape. `localOy` and `localOz` did
                // NOT: they read `t + a + b + c`, i.e. ((((t+a)+b)+c) — the
                // translation folded in FIRST and the remaining terms
                // accumulated on top of it. That is one moved term, in two of
                // three components, and it is invisible on almost every ray:
                // it only changes the rounded result where `t` and the rotated
                // part nearly cancel. Measured at 21 divergences in 29,867
                // hitting rays before this change. The parenthesisation below
                // is load-bearing; do not "simplify" it.
                const float localOx = invX.translation.x +
                    ((c19 * niqx + iqw * c26 + c20 * niqy) - c23 * niqz);
                const float localOy = invX.translation.y +
                    (((iqw * c23 - c20 * niqx) + c19 * niqy) + c26 * niqz);
                const float localOz = invX.translation.z +
                    ((((c23 * niqx + iqw * c20) - c26 * niqy) + c19 * niqz));

                // ray DIRECTION into sub-mesh local space (rotation only)
                px = dir->x; py = dir->y; pz = dir->z;
                float d19 = (iqw * px + iqx * 0.0f + py * iqz) - pz * iqy;
                float d20 = (iqy * 0.0f - px * iqz) + iqw * py + pz * iqx;
                float d23 = ((iqw * 0.0f - px * iqx) - py * iqy) - pz * iqz;
                float d17 = ((px * iqy + iqz * 0.0f) - py * iqx) + iqw * pz;
                const float localDx = (d23 * niqx + iqw * d19 + d17 * niqy) - d20 * niqz;
                const float localDy = (iqw * d20 - d17 * niqx) + d23 * niqy + d19 * niqz;
                const float localDz = ((d20 * niqx + iqw * d17) - d19 * niqy) + d23 * niqz;

                const SubMesh* node = &pm->_nodes[sub];
                const Vector3*  verts = node->_vertices._data;
                const Triangle* tris  = node->_triangles._data;
                const int32_t triCount = node->_triangles._size;

                for (int32_t t = 0; t < triCount; ++t) {
                    const uint32_t i0 = tris[t].i0;
                    const uint32_t i1 = tris[t].i1;
                    const uint32_t i2 = tris[t].i2;

                    const float v0y = verts[i0].y;
                    const float v0z = verts[i0].z;
                    const float v0x = verts[i0].x;

                    const float tvx = localOx - v0x;
                    const float e1z = verts[i1].z - v0z;
                    const float e1x = verts[i1].x - v0x;
                    const float e2x = verts[i2].x - v0x;
                    const float e2y = verts[i2].y - v0y;
                    const float e2z = verts[i2].z - v0z;
                    const float tvy = localOy - v0y;
                    const float e1y = verts[i1].y - v0y;

                    const float p27 = e2z * localDy - e2y * localDz;
                    const float p25 = e2x * localDz - e2z * localDx;
                    const float p22 = e2y * localDx - e2x * localDy;
                    const float det = p25 * e1y + p27 * e1x + p22 * e1z;

                    const float tvz = localOz - v0z;
                    const float q23 = tvy * e1z - tvz * e1y;
                    const float q20 = tvz * e1x - tvx * e1z;
                    const float q32 = tvx * e1y - tvy * e1x;

                    const float u = tvy * p25 + tvx * p27 + tvz * p22;
                    const float v = q20 * localDy + q23 * localDx + q32 * localDz;

                    if (0.0f < det && 0.0f <= u && u <= det && 0.0f <= v &&
                        v + u <= det) {
                        // Retail 0x180124ecc-0x180124ef1, read this lane:
                        //   movss xmm0,[1.0f] / mulss xmm3,xmm9 / mulss xmm4,xmm11
                        //   addss xmm3,xmm4 / divss xmm0,xmm10 / mulss xmm13,xmm12
                        //   addss xmm3,xmm13 / mulss xmm3,xmm0
                        // i.e. ((t1 + t2) + t3) * (1.0f / det) — which is what
                        // `a + b + c` means in C. The port matches.
                        float hitT = (q20 * e2y + q23 * e2x + q32 * e2z) * (kOne / det);

                        // NEGATIVE CONTROLS, off by default. See the fixture
                        // header above kOne for why there are TWO of them.
                        //   =1  ONE-ULP BUMP of hitT. Cannot be absorbed by any
                        //       geometry, and it is the EXACT magnitude of the
                        //       real defect (lane N measured Δz = 2^-18 on all
                        //       three observations, which is one ULP of hitT at
                        //       these ranges). This is the control that decides
                        //       whether the fixture can see the thing it hunts.
                        //   =2  the association move `(a+b+c)` -> `(a+(b+c))`.
                        //       Kept because it is the hypothesis under test,
                        //       NOT because it is a valid control: at 525 hits
                        //       it came back green, and an effect whose rate is
                        //       1 in 23,654 is expected 0.02 times in 525. It
                        //       is UNDERPOWERED, not disproved.
                        const int negctl = PickNegCtl();
                        if (negctl == 1) {
                            uint32_t bits;
                            std::memcpy(&bits, &hitT, 4);
                            bits += 1;                       // next float up
                            std::memcpy(&hitT, &bits, 4);
                        } else if (negctl == 2) {
                            hitT = (q20 * e2y + (q23 * e2x + q32 * e2z)) * (kOne / det);
                        }
                        // *** THE TIE-BREAK IS `<=`, AND IT MATCHES RETAIL. ***
                        // Lane M's cheapest hypothesis for its Finding 2 was
                        // that retail might use `<` here, so two coplanar
                        // triangles with equal hitT would pick a different one.
                        // TESTED against the disassembly by lane N and
                        // DISPROVED — retail 0x180124ef5:
                        //   comiss xmm3,xmm8(0.0) / jb  skip   => 0 <= hitT
                        //   movss  xmm0,[rsp+0x28](bestT)
                        //   comiss xmm0,xmm3      / jb  skip   => hitT <= bestT
                        // Both comparisons are inclusive, in this order. The
                        // port is byte-faithful here; the hypothesis must not
                        // be recorded as the cause of anything.
                        if (0.0f <= hitT && hitT <= bestT) {
                            const float dz = dir->z, dy = dir->y;
                            const float sz = start->z, sy = start->y;
                            outPos->x = hitT * dir->x + start->x;
                            outPos->y = hitT * dy + sy;
                            outPos->z = hitT * dz + sz;

                            // local-space face normal, then normalised
                            float nY = e2x * e1z - e2z * e1x;
                            float nZ = e2y * e1x - e2x * e1y;
                            float nX = e2z * e1y - e2y * e1z;
                            const float len = RtSqrtf(nY * nY + nX * nX + nZ * nZ);
                            if (len != 0.0f) {
                                const float r = kOne / len;
                                nZ = r * nZ;
                                nX = r * nX;
                                nY = r * nY;
                            }

                            // rotate it into world space: q * (n,0) * conj(q),
                            // with q the sub-mesh's WORLD rotation (not the
                            // inverse computed above).
                            const Quaternion conj = { -world->rotation.x,
                                                      -world->rotation.y,
                                                      -world->rotation.z,
                                                       world->rotation.w };
                            const Quaternion pure = { nX, nY, nZ, 0.0f };
                            Quaternion tmp, rotated;
                            QuatMul(&world->rotation, &tmp, &pure);
                            QuatMul(&tmp, &rotated, &conj);
                            outNormal->x = rotated.x;
                            outNormal->y = rotated.y;
                            outNormal->z = rotated.z;

                            bestT  = hitT;
                            bestId = LinkEntry(self, curLink);
                        }
                    }
                }
                ++sub;
            } while (sub < EntryAt(self, LinkEntry(self, curLink))->_transforms._size);
        }

        // Walk the rest of this cell's link chain, skipping entries already
        // tested during this query.
        bool jumped = false;
        it.link = LinkNext(self, curLink);
        while (it.link >= 0) {
            if (EntryAt(self, LinkEntry(self, it.link))->_queryStamp != stamp) {
                jumped = true;
                break;
            }
            it.link = LinkNext(self, it.link);
        }
        if (jumped) {
            EntryAt(self, LinkEntry(self, it.link))->_queryStamp = stamp;
            continue;
        }

        RtCellIterAdvance(&it);
        if (it.link >= 0) EntryAt(self, LinkEntry(self, it.link))->_queryStamp = stamp;
        if (it.link == -1) break;
    }

    if (rt_diff_on__) {
        typedef int (*Fn)(const Scene*, const Vector3*, const Vector3*, const Scalar*,
                          Vector3*, Vector3*);
        const int theirs = RtOriginal<Fn>(kIntersect)(cself, start, dir, maxDist,
                                                      &theirPos, &theirNormal);
        // Three terms, each named. The out-NORMAL used to be compared but never
        // printed, so a normal-only divergence would have rendered two
        // identical strings -- the same defect that made RemoveMesh's failure
        // unreadable.
        const PostCheck checks[] = {
            { "hit id", bestId, theirs },
            { "outPos first differing byte",
              RtFirstByteDiff(outPos, &theirPos, sizeof(Vector3)), -1 },
            { "outNormal first differing byte",
              RtFirstByteDiff(outNormal, &theirNormal, sizeof(Vector3)), -1 },
        };
        char a[96], b[96];
        const int bad = RtFirstFailure(checks, 3, a, sizeof(a), b, sizeof(b));
        if (bad == 0) {
            std::snprintf(a, sizeof(a), "id=%d t-best", bestId);
            std::snprintf(b, sizeof(b), "id=%d", theirs);
        } else if (bad > 0) {
            // A value divergence: show the numbers, not just the byte offset.
            // Rendered through scratch buffers -- snprintf'ing a buffer into
            // itself is undefined behaviour, not a shortcut.
            char at[96], bt[96];
            std::snprintf(at, sizeof(at), "%s p=(%.9g,%.9g,%.9g) n=(%.9g,%.9g,%.9g)", a,
                          outPos->x, outPos->y, outPos->z,
                          outNormal->x, outNormal->y, outNormal->z);
            std::snprintf(bt, sizeof(bt), "%s p=(%.9g,%.9g,%.9g) n=(%.9g,%.9g,%.9g)", b,
                          theirPos.x, theirPos.y, theirPos.z,
                          theirNormal.x, theirNormal.y, theirNormal.z);
            std::memcpy(a, at, sizeof(a));
            std::memcpy(b, bt, sizeof(b));
        } else {
            std::snprintf(a, sizeof(a), "id=%d p=(%.9g,%.9g,%.9g)", bestId,
                          outPos->x, outPos->y, outPos->z);
            std::snprintf(b, sizeof(b), "id=%d p=(%.9g,%.9g,%.9g)", theirs,
                          theirPos.x, theirPos.y, theirPos.z);
        }
        RT_DIFF_SHADOW(bad < 0, a, b);
    }
    return bestId;
}

// ============================================================================
// Registration.
//
// Both the mangled NAME and the retail ADDRESS are registered for every
// function. The two registries are separate (standalone/resolve_standalone.cpp
// keeps RtExportReg and RtAddrReg apart), and this lane's call sites use both:
// edittool.cpp / mapquery.cpp / gameupdateloop_load.cpp go by name, while
// citizenai_executor.cpp:406 resolves AddMesh by ADDRESS
// (`ResolveAddr(Module::Runtime, 0x1801252e0)`). Registering only the name
// would leave that call on the DLL and the diff oracle would under-count.
//
// Every name below is a string LITERAL, not one of the `k*` constants above,
// because tools/runtime_census.py cannot see through a named constant. It CAN
// see through a multi-line literal (see the correction above the constants),
// so the two very long names are kept on one line for readability only, not
// because the tool requires it.
//
// The AddMesh name is present in the shipped DLL at 0x1801252e0, ordinal 1571
// — verified against the export directory with pefile, not inferred.
// ============================================================================
int RtPickingSceneRegister() {
    int count = 0;

    RegisterRuntimeExport("??0Scene@Picking@@QEAA@XZ", (void*)&RtPicking_Scene_Ctor); ++count;
    RegisterRuntimeAddr(0x1801258e0, (void*)&RtPicking_Scene_Ctor);

    RegisterRuntimeExport("??1Scene@Picking@@QEAA@XZ", (void*)&RtPicking_Scene_Dtor); ++count;
    RegisterRuntimeAddr(0x180125890, (void*)&RtPicking_Scene_Dtor);

    RegisterRuntimeExport("?AddMesh@Scene@Picking@@QEAAIV?$ExternalPtr@VPickingMesh@Picking@@@Object@@AEBVTransform@Math@@PEAXHHP6AX2HHAEBVVector3@6@33@Z@Z", (void*)&RtPicking_Scene_AddMesh); ++count;
    RegisterRuntimeAddr(0x1801252e0, (void*)&RtPicking_Scene_AddMesh);

    RegisterRuntimeExport("?GetUserData@Scene@Picking@@QEBAPEAXI@Z", (void*)&RtPicking_Scene_GetUserData); ++count;
    RegisterRuntimeAddr(0x1800722d0, (void*)&RtPicking_Scene_GetUserData);

    RegisterRuntimeExport("?Intersect@Scene@Picking@@QEBAHAEBVVector3@Math@@0AEBVScalar@4@AEAV34@2@Z", (void*)&RtPicking_Scene_Intersect); ++count;
    RegisterRuntimeAddr(0x1801246c0, (void*)&RtPicking_Scene_Intersect);

    RegisterRuntimeExport("?SetMeshUserData@Scene@Picking@@QEAAXIPEAXHHP6AX0HHAEBVVector3@Math@@11@Z@Z", (void*)&RtPicking_Scene_SetMeshUserData); ++count;
    RegisterRuntimeAddr(0x180125240, (void*)&RtPicking_Scene_SetMeshUserData);

    RegisterRuntimeExport("?SetTransform@Scene@Picking@@QEAAXIAEBVTransform@Math@@@Z", (void*)&RtPicking_Scene_SetTransform); ++count;
    RegisterRuntimeAddr(0x1800721b0, (void*)&RtPicking_Scene_SetTransform);

    RegisterRuntimeExport("?RemoveMesh@Scene@Picking@@QEAAXI@Z", (void*)&RtPicking_Scene_RemoveMesh); ++count;
    RegisterRuntimeAddr(0x180125290, (void*)&RtPicking_Scene_RemoveMesh);

    RegisterRuntimeExport("?Vertices@PickingMesh@Picking@@QEBAAEBV?$Array@VVector3@Math@@@Collection@@H@Z", (void*)&RtPicking_PickingMesh_Vertices); ++count;
    RegisterRuntimeAddr(0x180071e10, (void*)&RtPicking_PickingMesh_Vertices);

    return count;
}
