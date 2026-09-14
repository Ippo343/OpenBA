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
// *** UPDATE, WAVE 9 LANE PICK (2026-09-08): FIVE OF THEM NO LONGER DO. ***
// The paragraph above is the reason the Wave 5 lane stopped where it did, and
// it was right at the time. It has been overtaken twice over:
//
//   1. The platform boundary is now a function call. Wave 9 lane Q's
//      `rtcoll::MemAlloc` / `rtcoll::MemFree` ARE the Enter/Allocate/Leave and
//      Enter/Free/Leave triples retail inlines, reached through the port-aware
//      address door, so an allocating body can live under runtime/src after all.
//   2. The ADDRESS-door census (ce1d8e9 / 4ef55b6) showed what leaving them
//      cost: five `ResolveAddr(Module::Runtime, 0x…)` sites in THIS FILE that
//      resolve to NULL with the DLL absent, invisible to every name-based
//      instrument because the call sites never mention a name.
//
// `RtGridAdd`, `RtGridReindex`, `RtGridUnthread`, `RtGridFreeEntry` and
// `RtCellIterAdvance` below are therefore no longer DLL calls. Their five
// addresses — and the fourteen more internals they reach — are ported and
// registered in runtime/src/picking/scene_grid.cpp, and `ResolveAddr` consults
// the port registry FIRST, so these forwarders reach that port with NO EDIT.
// The forwarders are deliberately left exactly as they are: their analysis is
// what the new lane was built from, and rewriting them would lose it.
//
// The five `RtFreeArray*` / `RtCellHeadsInit` / `RtTransformArraySetSize`
// forwarders further down are lane Q's and lane D2's, and were already ported.
// Nothing in this file still reaches Runtime-x64.dll by address.
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
#include "../collection/array_internal.h"   // ENDGAME B.3.2 step 2: kSpec_I32Neg1, AsArray

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

const float kOne       =  1.0f;
const float kCellSize  = 16.0f;
const float kBigPos    =  1.0e8f;
const float kBigNeg    = -1.0e8f;

// ============================================================================
// The six DLL internals this port calls, each resolved once per call site.
// ResolveAddr and NOT ResolveExport: none of these is exported, and the raw
// door is GetProcAddress, which must never sit on a normal-run path
// (reference_rtoriginal_is_getprocaddress.md — two such calls cost Wave 2 five
// points of sim time, invisible to both profilers).
// ============================================================================

// Array<int>::SetSize(10000), filling new slots with -1. Retail 0x18007b960;
// the Scene ctor's only callee besides the field stores. Reads/writes the
// {data,size,capacity} triple in place, so it takes &scene->_cellHeads.
// ENDGAME B.3.2 step 2 (callers) — GONE. `Scene::_cellHeads` is a real
// `Collection::Array<int32_t>` and its one call site names the hand-derived
// `kSpec_I32Neg1` row directly (the -1 tail fill is not trait-expressible).

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
// ENDGAME B.3.2 step 2 (callers) — all four forwarders are GONE; the Scene
// destructor calls `~Array()` on the four members themselves. The first three
// are J1 bodies (`ArrayDtorSited`, which reads only `spec->destroy`) over
// trivially destructible elements, so the trait-derived instantiation IS each
// of them. 0x180073c90 is the J2 variant whose element-destroy step frees each
// `Entry`'s inner `Collection::Array<::Transform>` — and `Entry` carries that
// as a real member (Scene.h), so `~Entry()` already does exactly that, in the
// same order, before the outer buffer is freed. All four rows stay registered.

// AddMesh's scratch Array<Transform>. Retail frees that buffer with a bare
// `MemoryAllocator::Free(_singleton, buf)` between an Enter/LeaveCriticalSection
// pair. Retail 0x1800739e0 is the same Free under the same lock, followed by a
// clear of the triple — and every Array<T>::~Array for a trivially-destructible
// T is byte-identical (Wave 9 lane Q re-read all eleven and confirms
// "byte-identical" in the strong form: identical instructions AND identical
// resolved operands), and Math::Transform IS trivially destructible (it has a
// user-provided default CONSTRUCTOR — Math.h's `constexpr Transform()` — but no
// user destructor, so `is_trivially_destructible` is unaffected).
//
// ENDGAME B.3.2 step 2 (callers) — the `RtFreeScratchTransforms` /
// `RtTransformArraySetSize` forwarders that used to sit here are GONE.
// `AddDesc::_transforms` (Scene.h) is a real `Collection::Array<::Transform>`,
// so the dtor is TRAIT-EQUIVALENT (see above) and becomes `.~Array()` directly.
//
// The SetSize call stays bridged through the hand-derived `kSpec_Transform`
// row rather than trusting the field's own (default, trait-derived) spec.
// NOTE FOR REVIEW: Collection.h's Array banner lists kSpec_Transform's
// identity-tail init among the rows a trait CANNOT express, but
// `Math::Transform` now has a real, non-trivial default constructor
// (`constexpr Transform() : rotation(), translation() {}`, added by ENDGAME
// B.3.1 after that banner was written) whose result — `Quaternion()` is
// `{0,0,0,1.0f}` (Math.h) and `Vector3()` is `{0,0,0}` — is bit-identical to
// `InitIdentityTransform` (array.cpp). `SpecFor<Transform>()` would therefore
// pick up `TInit<Transform>` (non-null, since the ctor is user-provided) and
// may in fact already BE this exact row. Bridging here is still CORRECT
// either way and does not depend on that claim; it is left as a finding for
// whoever next touches kSpec_Transform's banner, not resolved in this batch.

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
inline Entry* EntryAt(Scene* s, int32_t idx) { return &s->_entries[idx]; }
inline int32_t LinkEntry(Scene* s, int32_t link) { return s->_links[link]._entry; }
inline int32_t LinkNext(Scene* s, int32_t link)  { return s->_links[link]._next; }

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

    // ENDGAME B.3.2 step 2 (callers) — retail 0x18007b960, `kSpec_I32Neg1`.
    // NOT trait-equivalent (a fill of -1 is one of the twelve rows
    // Collection.h's Array banner names as inexpressible from traits), so
    // `_cellHeads` is bridged through the hand-derived row.
    rtcoll::AsArray<&rtcoll::kSpec_I32Neg1>(&self->_cellHeads)->SetSize(10000);

    // ---- ORACLE: full, against a private scratch Scene -----------------
    // The ctor mutates only the object it is handed, so the original can be
    // run for real into a stack Scene and compared field for field. The two
    // bucket buffers are different heap allocations, so the comparison is
    // "every field except the four Array _data pointers, plus the 10,000
    // bucket heads by content". Theirs is destroyed with the DLL's own dtor
    // so the scratch does not leak its 40 KB on every construction.
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

    // ENDGAME B.3.2 step 2 (callers) — all four TRAIT-EQUIVALENT once the
    // element type is the real one: `Entry` (Scene.h) carries a real
    // `Collection::Array<::Transform> _transforms` member, so `Entry`'s own
    // (implicit) destructor already destroys it — SpecFor<Entry>() calling
    // that destructor per element IS the 0x180073c90 J2 body's "destroy each
    // Entry's inner Array<Transform> first" behaviour. `Cell`, `Link` and
    // `int32_t` are plain POD, trivially destructible, matching the other
    // three J1 bodies exactly.
    self->_entries.~Array();   // +0x48  retail 0x180073c90
    self->_links.~Array();     // +0x30  retail 0x180073bb0
    self->_cells.~Array();     // +0x18  retail 0x1800739e0
    self->_cellHeads.~Array(); // +0x08  retail 0x1800737f0

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
    void* ours = self->_entries[id]._userData;
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
    const Collection::Array<Vector3>* ours = &self->_nodes[subMesh]._vertices;
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

    Entry* e = &self->_entries[id];
    e->_userData   = userData;
    e->_userValue  = userValue;
    e->_userValue2 = userValue2;
    e->_callback   = callback;

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

    uint32_t* dst = reinterpret_cast<uint32_t*>(self->_entries[id]._transforms.GetData());
    const uint32_t* src = reinterpret_cast<const uint32_t*>(xform);
    dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2]; dst[3] = src[3];
    dst[4] = src[4]; dst[5] = src[5]; dst[6] = src[6];
    RtGridReindex(self, id);

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
// buckets for the life of the process, under the diff-oracle flag only.
// ============================================================================
namespace {
// Lazily constructed by the DLL's own ctor, so the oracle never depends on the
}  // namespace

extern "C" uint32_t RtPicking_Scene_AddMesh(Scene* self, void* mesh,
                                            const ::Transform* rootXform, void* userData,
                                            int userValue, int userValue2,
                                            HitCallback callback) {

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

    // retail 0x180059140 — `kSpec_Transform`, NOT trait-equivalent (see the
    // banner above `RtGridAdd`): the tail init writes retail's identity
    // transform, which `Transform`'s own default ctor does not produce.
    rtcoll::AsArray<&rtcoll::kSpec_Transform>(&desc._transforms)
        ->SetSize(pm->_nodeCount);
    ::Transform* dst = desc._transforms.GetData();

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
    if (desc._transforms.GetSize() > 1) {
        for (int i = 1; i < desc._transforms.GetSize(); ++i) {
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

    desc._transforms.~Array();   // retail 0x1800739e0 (J1); see the banner above
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



    Entry* e = &self->_entries[id];
    const uint16_t w = e->_cellW;
    const uint16_t h = e->_cellH;
    RtGridUnthread(self, &e->_cellX, w, h, (int32_t)id);
    RtGridFreeEntry(self, id);

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


extern "C" int RtPicking_Scene_Intersect(const Scene* cself, const Vector3* start,
                                         const Vector3* dir, const Scalar* maxDist,
                                         Vector3* outPos, Vector3* outNormal) {
    Scene* self = const_cast<Scene*>(cself);


    Vector3 theirPos{}, theirNormal{};

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
        return -1;
    }

    for (;;) {
        const int32_t curLink = it.link;
        Entry* entry = EntryAt(self, LinkEntry(self, curLink));

        int32_t sub = 0;
        if (entry->_transforms.GetSize() > 0) {
            do {
                // Re-read the entry through the link each iteration, as retail
                // does (it reloads links[curLink]._entry at the top of every
                // sub-mesh pass rather than hoisting it).
                Entry* e = EntryAt(self, LinkEntry(self, curLink));
                const RtPicking::PickingMesh* pm =
                    reinterpret_cast<const RtPicking::PickingMesh*>(e->_mesh);
                const ::Transform* world = &e->_transforms[sub];

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
                const Vector3*  verts = node->_vertices.GetData();
                const Triangle* tris  = node->_triangles.GetData();
                const int32_t triCount = node->_triangles.GetSize();

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
