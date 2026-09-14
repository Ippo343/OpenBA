// ============================================================================
// runtime/src/graphics/scene.cpp — Wave 6 lane D — Graphics::Geometry + Scene
// ============================================================================
//
// Ports the nineteen `Graphics::Geometry` / `Graphics::Scene` exports our tree
// calls (docs/runtime_port/PLAN.md §4.2 wave 6, §6). Layouts and the evidence
// for every offset are in runtime/types/graphics/Geometry.h and
// runtime/types/graphics/Scene.h; the per-function write-up, the oracle
// arguments and everything this lane could NOT verify are in
// docs/runtime_port/WAVE6_LANE_D.md.
//
//   | Export                                    | Retail      | Ord  | Oracle       |
//   |-------------------------------------------|-------------|------|--------------|
//   | ??0Geometry@Graphics@@QEAA@XZ             | 0x1800a4270 |  205 | FULL scratch |
//   | ??1Geometry@Graphics@@UEAA@XZ             | 0x1800a41c0 |  599 | post-state   |
//   | ?Attach@Geometry@…@QEAAXPEAV12@H@Z        | 0x1800a3fa0 | 1631 | post-state   |
//   | ?Detach@Geometry@…@QEAAXPEAV12@@Z         | 0x1800a3f20 | 2216 | post-state   |
//   | ?Clean@Geometry@Graphics@@UEAA_NXZ        | 0x1800a4420 | 1702 | FULL replay  |
//   | ?UpdateAttached@Geometry@…@UEAAXXZ        | 0x1800a4020 | 4512 | post-state   |
//   | ?GetSubTransform@Geometry@…@UEBA…H@Z      | 0x1800a43c0 | 2949 | FULL         |
//   | ?GetTransform@Geometry@…@QEBA?AVTransform | 0x180015800 | 2979 | FULL         |
//   | ?GetInstanceTransform@Geometry@…@UEBA…HM@Z| 0x1800154a0 | 2720 | FULL         |
//   | ?SetSharedInstanceData@Geometry@…@UEBAH…  | 0x180015590 | 3965 | FULL         |
//   | ?SetBounds@Geometry@…@QEAAXAEBVBox@…      | 0x180015700 | 3836 | FULL replay  |
//   | ?SetTransform@Geometry@…@QEAAXAEBVTransform| 0x1800155a0 | 3992 | replay/post  |
//   | ??0Scene@Graphics@@QEAA@XZ                | 0x180102730 |  372 | FULL scratch |
//   | ??1Scene@Graphics@@QEAA@XZ                | 0x1801026c0 |  670 | post-state   |
//   | ?AddObject@Scene@Graphics@@QEAAIPEAV…     | 0x180102670 | 1575 | post-state   |
//   | ?RemoveObject@Scene@Graphics@@QEAAXI@Z    | 0x180102600 | 3563 | post-state   |
//   | ?AddGlobalObject@Scene@Graphics@@QEAAX…   | 0x180102540 | 1559 | post-state   |
//   | ?RemoveGlobalObject@Scene@Graphics@@QEAAX…| 0x1801024c0 | 3555 | post-state   |
//   | ?Clean@Scene@Graphics@@QEAAXXZ            | 0x1801023f0 | 1703 | post-state   |
//
// Every address and ordinal above is the output of a pefile query against the
// export directory of the SHIPPED C:\GOG Games\Banished\Runtime-x64.dll, run
// BEFORE any body was written (reference_resolveexport_phantom_names.md: a name
// the DLL does not contain resolves to NULL in silence, the port still
// registers, and the census still counts it).
//
// TWO OF THE TWENTY NAMES docs/runtime_port/wave6_lanes.txt handed this lane
// ARE PHANTOMS, and both are live in `code/Game/`:
//
//     ?Attach@Geometry@Graphics@@QEAAXAEAV12@H@Z   NOT IN THE DLL
//     ?Detach@Geometry@Graphics@@QEAAXAEAV12@@Z    NOT IN THE DLL
//
// Both spell the Geometry argument as a REFERENCE (`AEAV12@`). There is no
// reference overload: `Graphics::Geometry` exports exactly one Attach and one
// Detach and both take a POINTER (`PEAV12@`). The only consumer of those two
// names was code/Game/Action/action_base_complete.cpp, where `s_GeomAttach` and
// `s_GeomDetach` had therefore been NULL since the day they were written and the
// `if (s_Geom*)` guards around them had never fired. That call site was
// corrected in the tree on 2026-09-07 (it now reads `PEAV12@`), so this lane's
// registrations reach it. This file registers the two CORRECT names only:
// registering a name the DLL does not contain would put a live implementation
// behind a spelling nothing should ever use again.
//
// WHAT STAYS IN THE DLL, AND WHY
// ------------------------------
// 1. `Graphics::Scene`'s grid. The ctor body, the insert, the unthread and the
//    re-index are `FUN_`-named internals shared with other containers, and they
//    allocate and free through `System::MemoryAllocator` under that allocator's
//    own critical section — which is Win32 and therefore cannot appear in this
//    file (PLAN §4.1 decision 1 keeps runtime/src/ platform-neutral outside a
//    `_win` twin). This lane ports the nineteen EXPORTED bodies faithfully and
//    calls those internals at their retail addresses, exactly as retail's own
//    code does. They are named `Rt*` below and each says what it is. Same shape
//    as Wave 5 lane D (runtime/src/picking/scene.cpp) and Wave 4 lane C.
// 2. `Math::Quaternion::Lerp`, `Math::Transform::AsMatrix` and the box-transform
//    helper `FUN_18001f470` are called at their retail ADDRESSES, not by export
//    name. By address on purpose: `AsMatrix` is a Wave 1 port registered by NAME
//    (runtime/src/math/transform.cpp:982), and routing this lane's arithmetic
//    through another lane's registration would mean a divergence here could not
//    be attributed to this lane. Neither address is in the runtime ADDRESS
//    registry today (grepped across runtime/, engine/, code/, standalone/).
//
// VTABLE OWNERSHIP — READ THIS BEFORE ASSUMING A VIRTUAL IS OURS
// --------------------------------------------------------------
// Five of the nineteen (`Clean`, `UpdateAttached`, `GetSubTransform`,
// `GetInstanceTransform`, `SetSharedInstanceData`) are VIRTUAL, and the
// registry seam does not redirect a virtual call. This port does NOT stamp a
// vtable of its own: the ported ctor stores the DLL's own exported
// `??_7Geometry@Graphics@@6B@` (0x180154e60), byte for byte what retail stores.
// Consequences, stated rather than assumed:
//
//   * Our five ported virtual bodies are reached ONLY through the direct
//     `fn_Geometry_*` pointers in engine/funcs/engine_funcs.cpp:5270-5278,
//     which resolve by name and therefore hit this file.
//   * Every VIRTUAL dispatch — including the ones INSIDE this file (Attach and
//     UpdateAttached call slot 5, SetTransform and UpdateAttached call slot 4,
//     Scene::Clean calls slot 8) — still lands in the DLL for a base Geometry,
//     and in the subclass for a subclass. That is exactly retail's behaviour
//     and is why porting the ctor without a vtable of our own is the safe
//     choice here (PLAN §4.3 risk 3).
//   * Keeping the DLL vtable is also what keeps
//     `StatusIconGeometry_BuildVtable` (code/Game/Component/
//     statusicon_geometry.cpp:336) working: it builds its billboard vtable by
//     COPYING slots out of whatever the Geometry ctor stamped. Stamping our own
//     table would change every slot it copies.
//
//     *** THE "STANDING BLOCKER ON MINTING ??_7Geometry@Graphics@@6B@" CLAIM
//     THAT USED TO END THIS BULLET IS STRUCK, Wave 25 lane BODY, on lane
//     VTGFX's report and RE-DERIVED HERE RATHER THAN TAKEN ON ITS WORD. ***
//
//     What it claimed: that this consumer overruns the table — it copied
//     `i < 12` slots out of a NINE-slot table, picking up three qwords of the
//     "ImageBuffer\0" string run — and that the overrun made the mint row
//     permanently unsafe. Three measurements on THIS tip:
//
//       1. code/Game/Component/statusicon_geometry.cpp:341 reads `i < 9` today
//          and installs nullptr from 9 up. Wave 22 lane GEOM fixed the loop.
//          THE OVERRUN DOES NOT EXIST ANY MORE.
//       2. standalone/rt_data_mint_tables.inc:1568 carries the row at NINE
//          slots, tagged `tail-not-a-pointer`.
//       3. standalone/reg_mint_promoted.cpp:1396 says it in its own words —
//          "this row's expiry condition FIRED ... the below is HISTORY, not a
//          live tag". So this file has been contradicted by its own tree since
//          Wave 22 and nobody noticed, because the contradiction lived in a
//          different directory. (An earlier pass HERE, Wave 23 lane GFXBODY,
//          corrected the 12-to-9 DETAIL and then wrote "nothing about the
//          decision changes" — which was true of the decision and false of the
//          BLOCKER, and left the stale word standing for two more waves.)
//
//     WHAT IS **NOT** STRUCK, because the evidence does not reach that far:
//     the row is still NOT PROMOTED, and reg_mint_promoted.cpp:1396 calls
//     promoting it "a separate, still-open step for whichever lane next
//     re-scores this row from scratch". The correct statement is **"the overrun
//     blocker is discharged; the row still needs its own re-score"**, NOT
//     "minting Geometry is now safe".
//
//     AND THIS FILE'S OWN DECISION IS UNCHANGED. The ctor still stores the
//     DLL's exported table, for the reason in the bullet above — that is
//     retail's dispatch behaviour — which never depended on the BuildVtable
//     consumer at all. A blocker being struck is not a licence to change the
//     thing it was wrongly cited as blocking.
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/graphics/Geometry.h"
#include "../../types/graphics/Scene.h"
#include "wave9_lane_gfxd.h"

using RtGraphics::Geometry;
using RtGraphics::Scene;
using RtGraphics::SceneNode;
using RtGraphics::SceneObjectId;

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
// and would under-count the lane, which is how Wave 3's Object count stood at
// 21/41 when it was really 27/41
// (reference_census_cannot_see_nonliteral_registrations.md).
// ============================================================================
const char* const kGeoCtor      = "??0Geometry@Graphics@@QEAA@XZ";
const char* const kGeoDtor      = "??1Geometry@Graphics@@UEAA@XZ";
const char* const kGeoClean     = "?Clean@Geometry@Graphics@@UEAA_NXZ";
const char* const kGeoGetSub    = "?GetSubTransform@Geometry@Graphics@@UEBA?AVTransform@Math@@H@Z";
const char* const kGeoGetXform  = "?GetTransform@Geometry@Graphics@@QEBA?AVTransform@Math@@XZ";
const char* const kGeoGetInst   = "?GetInstanceTransform@Geometry@Graphics@@UEBA?AVMatrix@Math@@HM@Z";
const char* const kGeoSetShared = "?SetSharedInstanceData@Geometry@Graphics@@UEBAHAEAVContext@Video@@HM@Z";
const char* const kGeoSetBounds = "?SetBounds@Geometry@Graphics@@QEAAXAEBVBox@Volume@@@Z";
const char* const kGeoSetXform  = "?SetTransform@Geometry@Graphics@@QEAAXAEBVTransform@Math@@@Z";
const char* const kSceneCtor    = "??0Scene@Graphics@@QEAA@XZ";
const char* const kSceneDtor    = "??1Scene@Graphics@@QEAA@XZ";

// ============================================================================
// The float constants retail loads from .rdata, verified by reading the BYTES
// out of the shipped DLL rather than trusting Ghidra's rendering of them.
//   DAT_18016169c = 0x3f800000 = 1.0f     (Geometry ctor: quaternion w, opacity)
//   DAT_180161694 = 0x3f000000 = 0.5f     (half-cell, and the box half-extent)
//   DAT_180161730 = 0x45000000 = 2048.0f  (Scene's grid cell size)
//   0x4cbebc20    = +1.0e8f               (empty-box min)
//   0xccbebc20    = -1.0e8f               (empty-box max)
// ============================================================================
const float kOne          = 1.0f;
const float kSceneCellSize = 2048.0f;
const float kBigPos       =  1.0e8f;
const float kBigNeg       = -1.0e8f;

// ============================================================================
// The DLL internals this port calls, each resolved once per call site.
// ResolveAddr and NOT ResolveExport/the raw DLL door: none of the FUN_ ones is
// exported, and the raw DLL door is GetProcAddress, which must never sit on a
// normal-run path (reference_rtoriginal_is_getprocaddress.md — two such calls
// cost Wave 2 five points of sim time, invisible to both profilers).
// ============================================================================

// The base Geometry vtable, ??_7Geometry@Graphics@@6B@ (0x180154e60,
// ordinal 1439). An exported DATA symbol, so this could be a ResolveExport —
// but ResolveExport is GetProcAddress and this is on the ctor's hot path, so
// it is resolved by address and cached. See the vtable-ownership note above for
// why the port stamps the DLL's table and not one of its own.
void* RtGeometryVtable() {
    static void* vt = reinterpret_cast<void*>(ResolveAddr(Module::Runtime, 0x180154e60));
    return vt;
}

// ENDGAME B.3.2 step 2 (callers) — the three Array forwarders that used to sit
// here (0x18001f990 SetSize(n,&fill), 0x180023830 the stride-4 sibling, and
// 0x18001bf50 ~Array<Geometry*>) are GONE. Their call sites now name the
// container they act on, which is already a real `Collection::Array<T>` member
// in runtime/types/graphics/{Geometry,Scene}.h:
//
//   0x18001f990  kSpec_Fill8  {8, 8, Assign8, init=null, destroy=null, byteCopy}
//                == `SpecFor<Geometry*>()` field for field, so `_attached`'s
//                trait-derived `Array<Geometry*>` IS that instantiation.
//   0x180023830  kSpec_Fill4  {4, 4, Assign4, init=null, destroy=null, byteCopy}
//                == `SpecFor<uint32_t>()`, so `_freeIds`'s `Array<uint32_t>` is.
//   0x18001bf50  a J1 `~Array` = `ArrayDtorSited` = `Array<uint8_t>::~Array`,
//                which for a trivially destructible element is exactly
//                `Array<Geometry*>::~Array` (spec->destroy is null in both).
//
// The three registered rows themselves stay in array.cpp: `code/` still reaches
// all three by address.

// MemoryAllocator::MemAlloc(size, align) — retail 0x1800c7a50, ordinal 3227.
// EnterCriticalSection(_singleton+0x640); Allocate(_singleton, size, align);
// LeaveCriticalSection. Scene::AddGlobalObject inlines exactly that sequence,
// so calling this static IS the retail body, minus the inlining, and it keeps
// <windows.h> out of runtime/src/.
//
// Called by ADDRESS, not by name: Wave 2 registers a port under
// "?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z"
// (runtime/src/system/allocator.cpp:984) that is armed only opt-in with a
// standing NO-GO verdict (docs/runtime_port/WAVE2_ALLOCATOR.md). Going by name
// would make this lane's list nodes come from whichever allocator that switch
// happened to select; going by address always reaches the DLL's, which is where
// the DLL's own free path (FUN_180009c30, below) will return them.
// ENDGAME B.3.2 step 2 — GONE. Its one caller was AddGlobalObject's inlined
// node allocation, which is now `LinkedList<Geometry*>::Add`; the class reaches
// the same 0x1800c7a50 through `Collection::detail::ListNodeAlloc`
// (runtime/src/collection/linkedlist.cpp), with the same retry-a-miss rule.

// ---------------------------------------------------------------------------
// WAVE 9 LANE GFX2, 2026-09-08 — THE SEVEN BY-ADDRESS DOORS IN THIS FILE, AND
// WHY THEIR `static Fn fn = ...` NOW RETRIES A NULL
//
// The integrator's address-door census (docs/runtime_port/
// wave9_unregistered_addr_doors.csv, commit ce1d8e9, corrected in 4ef55b6)
// lists seven unregistered `ResolveAddr(Module::Runtime, ...)` sites in this
// file. All seven were checked here and the verdict is the same for every one:
//
//   * NONE of the seven is exported (tools/pe_exports.py over the shipped DLL
//     returns nothing for any of the seven addresses), so no name-based
//     instrument — census, residue, unsatisfied_export_scan — can see them.
//   * EVERY one has a LIVE, non-diff-guarded caller in a ported body in this
//     file: Geometry::Clean (RtBoxTransform), Scene::Scene (RtSceneGridInit),
//     Scene::~Scene (RtSceneFreeGlobals), Scene::AddObject (RtSceneGridAdd),
//     Scene::RemoveObject (RtSceneGridUnthread), RemoveGlobalObject
//     (RtFreeLocked) and Scene::Clean (RtSceneGridReindex).
//   * So at cutover all seven resolve to nothing and every one is a call
//     through null, on the first frame that draws or the first scene built.
//     They are HARD cutover dependencies, not a watch list.
//
// *** ALL SEVEN ARE NOW CLOSED (2026-09-08). *** Read the paragraph below as
// the history it is, not as the current state:
//
//   0x18001f470  BoxTransform    — lane PICK, runtime/src/picking/scene_grid.cpp
//   0x180009c30  FreeLocked      — lane COLL, runtime/src/collection/linkedlist.cpp
//   0x18005e4a0  GridInit    \
//   0x18005e5e0  GridAdd      |
//   0x18005f090  GridUnthread |  lane GFX3, runtime/src/graphics/scene_grid.cpp
//   0x18005ee30  GridReindex  |  (with 0x18005f4b0 GridThread and 0x18005fb00
//   0x18005f1f0  FreeGlobals /    Array<Node>::SetSize, their closure)
//
// So every forwarder below now reaches a PORT, because ResolveAddr consults the
// port registry first. Each one's own comment says which lane owns it.
//
// Registering them was NOT sufficient and porting them was NOT lane GFX2's work:
// they are unexported Runtime internals with no port anywhere in the tree.
// What IS fixed here is the second half of the defect commit 309c015 found in
// this same file: `static Fn fn = ResolveAddr(...)` CACHES A NULL FOREVER, so
// even once a port exists a caller that ran before it was armed would freeze
// the null in for the life of the process. That is the `RtResolveCached`
// contract — cache a hit, retry a miss — and 309c015 applied it only to
// RtMemAlloc. It is applied to the other eight here (the seven plus
// Quaternion::Lerp) so that registering them later is a one-line change and
// not an ordering lottery.
// ---------------------------------------------------------------------------

// The allocator-lock Free wrapper retail's RemoveGlobalObject tail-calls —
// retail 0x180009c30: `if (p) { Enter; Free(_singleton, p); Leave; }`. Its
// first parameter is ignored by the body but is passed (`this`) by retail, so
// it is passed here too.
// *** PORTED AND REGISTERED (Wave 9 lane COLL) *** as
// `RtColl_FreeLocked_180009c30` in runtime/src/collection/linkedlist.cpp, which
// establishes instruction by instruction that it IS `MemFree` with an ignored
// leading argument. This forwarder therefore no longer reaches the DLL, and the
// row is closed for cutover. ONE registration, there: do not add a second here.
// ENDGAME B.3.2 step 2 — GONE. Its one caller was RemoveGlobalObject, which is
// now `LinkedList<Geometry*>::Remove`; the class's `FreeNode` is that same
// 0x180009c30 door through `Collection::detail::ListNodeFree`.

// Volume::Box transform — retail 0x18001f470, the only callee of
// Geometry::Clean's bounds branch. Converts the Transform to a Transform3, then
// builds the world-space AABB of the local box the standard center/half-extent
// way (`center' = M*center + t`, `half' = |M| * half`).
//
// *** THIS NO LONGER REACHES THE DLL (Wave 9 lane PICK, 2026-09-08). ***
// This comment used to say "left in the DLL because it is a float-exact routine
// whose rounding this lane has no independent way to verify". That premise is
// gone: the Picking grid needs the same body (0x180125bc0 calls it for every
// mesh node) and so lane PICK ported it in runtime/src/picking/scene_grid.cpp
// and registered 0x18001f470 there. `ResolveAddr` consults the port registry
// FIRST, so this forwarder now reaches that port, unchanged.
//
// The verification this lane said it could not do exists there: a BITWISE
// 48-case sweep against the shipped body over off-axis and non-normalised
// rotations and boxes whose extent terms nearly cancel, plus a moved-term
// negative control (scene_grid_probe.cpp, SweepFloatBodies). Tolerance zero.
// ONE registration, in that file: do not add a second here.
//   out: 6 floats {minX,minY,minZ,maxX,maxY,maxZ}; localBox: the same 6 floats.
float* RtBoxTransform(float* out, const Transform* xf, const float* localBox) {
    using Fn = float* (*)(float*, const Transform*, const float*);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x18001f470));
    return fn(out, xf, localBox);
}

// Math::Quaternion::Lerp — retail 0x18001d500, ordinal 3191,
// ?Lerp@Quaternion@Math@@QEBA?AV12@AEBV12@AEBVScalar@2@@Z. HIDDEN RETURN SLOT:
// RCX = this (the FROM quaternion), RDX = the return buffer, R8 = the TO
// quaternion, R9 = &t. Pinned by the disassembly of GetInstanceTransform at
// 0x1800154e1/0x1800154bd, which loads RCX = this+0xa0 and R8 = this+0x84 and
// spills XMM3 to the stack slot it passes in R9.
Quaternion* RtQuaternionLerp(const Quaternion* from, Quaternion* ret,
                             const Quaternion* to, const Scalar* t) {
    using Fn = Quaternion* (*)(const Quaternion*, Quaternion*, const Quaternion*, const Scalar*);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x18001d500));
    return fn(from, ret, to, t);
}

// Math::Transform::AsMatrix — retail 0x18001e5f0. HIDDEN RETURN SLOT in RDX.
// Asked BY NAME: Wave 1 ported AsMatrix (runtime/src/math/transform.cpp:982) and
// ResolveAddr consults the ADDRESS registry only, so the by-address form this
// line used until 2026-09-07 sent every Graphics::Scene transform through the
// shipped DLL instead of our own port — one port bypassing another inside
// runtime/.
// ENDGAME B.3.1 step 2: a direct member call; there is no door left to bypass.
Matrix* RtTransformAsMatrix(const Transform* xf, Matrix* ret) {
    *ret = xf->AsMatrix();
    return ret;
}

// ---------------------------------------------------------------------------
// THE FIVE GRID FORWARDERS BELOW NO LONGER REACH THE DLL (Wave 9 lane GFX3,
// 2026-09-08). All five addresses are ported and registered in
// runtime/src/graphics/scene_grid.cpp, and `ResolveAddr` consults the port
// registry FIRST — so every one of these now executes our body. Their closure
// went with them: 0x18005f4b0 (the thread-in, in that same file) and
// 0x18005fb00 (Array<Scene::Node>::SetSize, added to lane Q's family in
// runtime/src/collection/array.cpp). Kill switch `RT_GFXGRID=0` puts
// all five back on the DLL.
//
// The descriptions kept below are what each retail body does and are still
// accurate; what changed is WHO executes it.
// ---------------------------------------------------------------------------

// The Scene grid initialiser — retail 0x18005e4a0. Seeds cellSize / half-cell /
// level count, then SetSize()s the three int32 arrays (8 level offsets seeded
// {0,1,5,0x15,0x55,0x155,0x555,0x1555}, 0x5555 cell heads filled with -1 and
// 0x5555 cell counts filled with 0). ~350 KB of allocation, all through the
// allocator lock.
void RtSceneGridInit(Scene* self, float cellSize) {
    using Fn = void* (*)(Scene*, float);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x18005e4a0));
    fn(self, cellSize);
}

// The grid insert — retail 0x18005e5e0. Takes a slot off the free-id array (or
// appends a new node), stores the Geometry*, computes the object's quadtree
// level and cell from its world box, and threads the node onto that cell's
// chain. Returns the node id. Everything it touches is the Scene's own arrays
// or the allocator, so it is layout-compatible with this port.
uint32_t RtSceneGridAdd(Scene* self, Geometry* const* geo) {
    using Fn = uint64_t (*)(Scene*, Geometry* const*);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x18005e5e0));
    return static_cast<uint32_t>(fn(self, geo));
}

// The grid unthread — retail 0x18005f090. Splices node `id` out of its cell's
// chain (fixing the cell head at Scene+0x30 when the node was the head) and out
// of the two int32 links at node+0x08 / node+0x0c.
//
// THE CHAIN IS CIRCULAR, NOT NULL-TERMINATED — a lone node's `_next` and `_prev`
// both name the node itself (0x18005f4b0's empty-cell arm at 0x18005f51c). That
// is why the splice stores are unconditional and why there is a SECOND head fix.
// See scene_grid.cpp's banner; a null-terminated reading of it makes retail look
// like it writes sixteen bytes in front of the node buffer, and it does not.
void RtSceneGridUnthread(Scene* self, int32_t id) {
    using Fn = void (*)(Scene*, int32_t);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x18005f090));
    fn(self, id);
}

// The grid re-index — retail 0x18005ee30. Recomputes node `id`'s quadtree level
// and cell from the geometry's CURRENT world box (geo+0x4c..+0x60) and rethreads
// it if the cell changed. Scene::Clean calls it for every object whose Clean
// reported a change.
void RtSceneGridReindex(Scene* self, int32_t id) {
    using Fn = void (*)(Scene*, int32_t);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x18005ee30));
    fn(self, id);
}

// ENDGAME B.3.2 step 2 — the global-list teardown forwarder (0x18005f1f0) is
// gone: `Scene::~Scene` calls `_globals.Clear()`, and that member IS the body
// scene_grid.cpp's `FreeGlobals` transcribes — walk head->next reading `next`
// BEFORE the free, then tail, head, count in retail's store order. The
// 0x18005f1f0 row stays registered in scene_grid.cpp for `code/`.

// Array<int32>::~Array — retail 0x18001c750. The Scene destructor's four middle
// calls ALL target this one address (disassembly of 0x1801026c0: four
// `LEA RCX,[RBX+0x50/0x40/0x30/0x20]` + `CALL 0x18001c750`), which is Ghidra's
// `Collection_Array_FreeBufferAndZero`: Free(_data) under the allocator lock,
// then zero the {data,size,capacity} triple. The node Array at +0x10 is freed by
// FUN_18005fc20, a byte-identical body emitted for the Array<Node>
// instantiation and reached by a tail JMP; both are called at their own
// addresses.
//
// *** WAVE 9 LANE Q, 2026-09-07: THIS NO LONGER REACHES THE DLL. ***
// 0x18001c750 and 0x18005fc20 are lane Q's J1 registrations
// (runtime/src/collection/array.cpp), so these two calls now execute OUR
// Array<T>::~Array. The clause this comment used to end with -- "retail's body
// and not a re-derivation of it" -- was true until lane Q landed and is not any
// more. The re-derivation is verified against the shipped bodies by the
// synthetic-subject replay in runtime/src/collection/array_probe.cpp.
// ENDGAME B.3.2 step 2 — both forwarders are gone; the five call sites in
// `Scene::~Scene` are `Array<T>::~Array` on the five members themselves. Both
// addresses are a J1 body, i.e. `ArrayDtorSited` = `Array<uint8_t>::~Array`,
// whose only spec field is `destroy` — null for `int32_t`, `uint32_t` and
// `SceneNode` alike, so the trait-derived instantiations are those bodies. The
// two rows stay registered in array.cpp for the rest of the tree.

// ============================================================================
// Virtual dispatch helpers.
//
// Spelled once, so a wrong slot number cannot be written twice. The slot
// numbers come from runtime/types/graphics/Geometry.h, which reads them out of
// the nine-pointer vtable at 0x180154e60.
//
// EVERY ONE OF THESE LANDS IN THE DLL for an object the DLL's ctor built, and
// in the subclass for a subclass. See the vtable-ownership note in the banner.
// ============================================================================
inline void** VTable(const Geometry* g) {
    return *reinterpret_cast<void** const*>(g);
}

Transform* VGetSubTransform(const Geometry* self, Transform* ret, int node) {
    using Fn = Transform* (*)(const Geometry*, Transform*, int);
    return reinterpret_cast<Fn>(VTable(self)[RtGraphics::kVSlotGetSubTransform])(self, ret, node);
}

void VUpdateAttached(Geometry* self) {
    using Fn = void (*)(Geometry*);
    reinterpret_cast<Fn>(VTable(self)[RtGraphics::kVSlotUpdateAttached])(self);
}

bool VClean(Geometry* self) {
    using Fn = bool (*)(Geometry*);
    return reinterpret_cast<Fn>(VTable(self)[RtGraphics::kVSlotClean])(self);
}

// ============================================================================
// Self-describing post-state predicates.
//
// A shadow verdict is only as good as its message: if the verdict is a
// conjunction of N terms the message has to name all N, or a failure prints two
// strings that agree and says nothing
// (reference_shadow_predicate_must_name_its_failing_term.md — Wave 5's
// RemoveMesh cost a whole extra integration run to that exact defect). Every
// shadow site in this file goes through these helpers, which report the FIRST
// failing term by name with its actual and expected value.
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
    const uint8_t* a = static_cast<const uint8_t*>(x);
    const uint8_t* b = static_cast<const uint8_t*>(y);
    for (size_t i = 0; i < n; ++i)
        if (a[i] != b[i]) return static_cast<long long>(i);
    return -1;
}

// Index of the first differing element between two int arrays, or -1.
long long RtFirstIntDiff(const int32_t* x, const int32_t* y, int n) {
    for (int i = 0; i < n; ++i)
        if (x[i] != y[i]) return i;
    return -1;
}

// ----------------------------------------------------------------------------
// Geometry comparison for the replay oracles.
//
// A Geometry is 0xC0 self-contained bytes, so "did ours leave the object in the
// state theirs would have?" is a byte comparison — EXCEPT for the two holes the
// ctor never initialises (+0x14 and +0xbc, see Geometry.h). Those hold whatever
// the allocation happened to contain, identically in both runs of a replay, but
// they are excluded anyway so that a future body which starts writing one of
// them is not silently blessed by this comparison.
// ----------------------------------------------------------------------------
long long GeometryFirstDiff(const Geometry* x, const Geometry* y) {
    const uint8_t* a = reinterpret_cast<const uint8_t*>(x);
    const uint8_t* b = reinterpret_cast<const uint8_t*>(y);
    for (size_t i = 0; i < sizeof(Geometry); ++i) {
        if (i >= 0x14 && i < 0x18) continue;   // _pad14
        if (i >= 0xbc) continue;               // _padBC
        if (a[i] != b[i]) return static_cast<long long>(i);
    }
    return -1;
}

// Transform equality exactly as retail spells it: three translation terms
// first, then the four rotation terms, all `!=` on floats (so a NaN compares
// unequal to itself, which is retail's behaviour and must be preserved).
inline bool TransformDiffers(const Transform* a, const Transform* b) {
    return a->translation.x != b->translation.x ||
           a->translation.y != b->translation.y ||
           a->translation.z != b->translation.z ||
           a->rotation.x    != b->rotation.x    ||
           a->rotation.y    != b->rotation.y    ||
           a->rotation.z    != b->rotation.z    ||
           a->rotation.w    != b->rotation.w;
}

// ----------------------------------------------------------------------------
// Detach's body, shared by the exported Detach and by ~Geometry.
//
// Retail INLINES this loop into the destructor rather than calling Detach, so
// the two are written from one helper here and the exported entry point keeps
// its own diff site. If the dtor called the exported entry instead, Detach's
// `calls=` column would count teardown work it never does in retail, and that
// column is the one this project reads to decide whether a port was verified
// at all (reference_uncalled_port_is_not_verified.md).
//
// `self` is the PARENT, `child` the geometry to unhook. Retail's shift-remove
// is transcribed term for term, including the redundant `!= nullptr` test on an
// element address that cannot be null and the re-read of `self->_attached._data`
// on every iteration.
// ----------------------------------------------------------------------------
void DetachBody(Geometry* self, Geometry* child) {
    int idx = 0;
    long long walked = 0;
    if (self->_attached.GetSize() <= 0) return;

    Geometry** p = self->_attached.GetData();
    while (*p != child) {
        ++walked;
        ++idx;
        ++p;
        if (self->_attached.GetSize() <= walked) return;   // not attached to this parent
    }

    if (idx < self->_attached.GetSize()) {
        long long off = static_cast<long long>(idx);
        do {
            ++idx;
            if (idx < self->_attached.GetSize()) {
                Geometry** slot = self->_attached.GetData() + off;
                if (slot != nullptr) *slot = slot[1];
            }
            ++off;
        } while (idx < self->_attached.GetSize());
    }
    self->_attached._size -= 1;
    child->_attachParent = nullptr;
    child->_attachNode   = -1;
}

}  // namespace

// ============================================================================
// Graphics::Geometry::Geometry()
// retail 0x1800a4270, ordinal 205   ??0Geometry@Graphics@@QEAA@XZ
//
// Forty-six field stores and nothing else. Transcribed in retail's own order so
// the ported body is the retail body; the two holes it leaves (+0x14, +0xbc)
// are left alone here too, which is why the oracle below skips them.
//
// ORACLE: FULL, against a private scratch Geometry. The ctor mutates only the
// object it is handed and allocates nothing, so the original can be run for
// real into a stack Geometry and compared byte for byte. The scratch needs no
// destructor: its _attached Array is null.
// ============================================================================
extern "C" Geometry* RtGraphics_Geometry_Ctor(Geometry* self) {

    self->_vtable            = RtGeometryVtable();
    self->_attachParent      = nullptr;
    self->_attachNode        = -1;
    self->_attached._data    = nullptr;
    self->_attached._size    = 0;
    self->_attached._capacity= 0;
    self->_detailId          = 2;
    self->_dirtyFlags        = 7;
    self->_materialTypeFlags = 0;

    self->_localBounds.min.x = 0.0f;
    self->_localBounds.min.y = 0.0f;
    self->_localBounds.min.z = 0.0f;
    self->_localBounds.max.x = 0.0f;
    self->_localBounds.max.y = 0.0f;
    self->_localBounds.max.z = 0.0f;

    self->_worldBounds.min.x = kBigPos;
    self->_worldBounds.min.y = kBigPos;
    self->_worldBounds.min.z = kBigPos;
    self->_worldBounds.max.x = kBigNeg;
    self->_worldBounds.max.y = kBigNeg;
    self->_worldBounds.max.z = kBigNeg;

    self->_prevWorldBounds.min.x = kBigPos;
    self->_prevWorldBounds.min.y = kBigPos;
    self->_prevWorldBounds.min.z = kBigPos;
    self->_prevWorldBounds.max.x = kBigNeg;
    self->_prevWorldBounds.max.y = kBigNeg;
    self->_prevWorldBounds.max.z = kBigNeg;

    self->_opacity     = kOne;
    self->_opacityPrev = kOne;

    self->_transform.rotation.x = 0.0f;
    self->_transform.rotation.y = 0.0f;
    self->_transform.rotation.z = 0.0f;
    self->_transform.rotation.w = kOne;
    self->_transform.translation.x = 0.0f;
    self->_transform.translation.y = 0.0f;
    self->_transform.translation.z = 0.0f;

    self->_prevTransform.rotation.x = 0.0f;
    self->_prevTransform.rotation.y = 0.0f;
    self->_prevTransform.rotation.z = 0.0f;
    self->_prevTransform.rotation.w = kOne;
    self->_prevTransform.translation.x = 0.0f;
    self->_prevTransform.translation.y = 0.0f;
    self->_prevTransform.translation.z = 0.0f;

    return self;   // retail returns `this` in RAX
}

// ============================================================================
// Graphics::Geometry::~Geometry()
// retail 0x1800a41c0, ordinal 599   ??1Geometry@Graphics@@UEAA@XZ
//
// Re-stamp the base vtable, then detach every child (the Detach body inlined,
// always removing element 0), then free the child array. It does NOT detach
// itself from its own parent — a Geometry destroyed while attached leaves a
// dangling pointer in the parent's array, and that is retail's behaviour, not
// an oversight to be fixed here.
//
// ORACLE: post-state only, and that is all it can be. Running the original
// afterwards would double-free the child array. What is checked is what
// retail's body must leave behind.
// ============================================================================
extern "C" void RtGraphics_Geometry_Dtor(Geometry* self) {

    self->_vtable = RtGeometryVtable();
    while (self->_attached.GetSize() > 0)
        DetachBody(self, self->_attached[0]);
    self->_attached.~Array();          // retail 0x18001bf50, ~Geometry's tail call

}

// ============================================================================
// Graphics::Geometry::Detach(Geometry*)
// retail 0x1800a3f20, ordinal 2216   ?Detach@Geometry@Graphics@@QEAAXPEAV12@@Z
//
// `self` is the PARENT and the argument the child, which is the opposite of
// what the name suggests and is worth stating: ActCt_HideTool
// (code/Game/Action/action_base_complete.cpp:414) already calls it that way.
//
// NOTE the mangling: `PEAV12@` (pointer), not the `AEAV12@` (reference) that
// docs/runtime_port/wave6_lanes.txt lists. There is no reference overload.
//
// ORACLE: post-state. Detach mutates the parent's array in place and cannot be
// replayed (the original would remove a second element, or none). What is
// checked is the whole contract: the child is unlinked, the count dropped by
// one, and the child no longer appears anywhere in the parent's array.
// ============================================================================
extern "C" void RtGraphics_Geometry_Detach(Geometry* self, Geometry* child) {

    const int32_t sizeBefore = self->_attached.GetSize();
    bool wasPresent = false;
    for (int32_t i = 0; i < sizeBefore; ++i)
        if (self->_attached[i] == child) { wasPresent = true; break; }

    DetachBody(self, child);

}

// ============================================================================
// Graphics::Geometry::SetTransform(const Transform&)
// retail 0x1800155a0, ordinal 3992
//   ?SetTransform@Geometry@Graphics@@QEAAXAEBVTransform@Math@@@Z
//
// Compare all seven floats; if any differs, roll the current transform into the
// previous one, take the new one, raise the transform+bounds dirty bits, and —
// only when this geometry has children — TAIL JMP through vtable slot 4
// (UpdateAttached). Retail's compare order is translation first, then rotation,
// and the `!=` is a float compare, so a NaN in either always counts as changed.
//
// ============================================================================
extern "C" void RtGraphics_Geometry_SetTransform(Geometry* self, const Transform* xf) {

    bool tailCall = false;
    if (TransformDiffers(&self->_transform, xf)) {
        self->_prevTransform = self->_transform;
        self->_transform     = *xf;
        self->_dirtyFlags   |= 3u;
        tailCall = self->_attached.GetSize() > 0;
    }

    if (tailCall)
        VUpdateAttached(self);   // retail: tail JMP through vtable slot 4
}

// ============================================================================
// Graphics::Geometry::Attach(Geometry*, int)
// retail 0x1800a3fa0, ordinal 1631   ?Attach@Geometry@Graphics@@QEAAXPEAV12@H@Z
//
// `self` is the PARENT, `child` the geometry being hooked onto node `node`.
// Detach the child from whatever it was attached to, append it to this
// geometry's array, record parent+node on the child, then push the parent's
// sub-transform for that node into the child.
//
// Retail's inner calls are DIRECT (rel32) to its own Detach and SetTransform,
// so this port calls the ported entry points for both — the diff sites those
// carry then count the work retail actually does there.
//
// The `GetSubTransform` call is VIRTUAL (slot 5) and lands in the DLL for a base
// Geometry and in the subclass for a subclass. That is retail's dispatch, not a
// gap in this port.
//
// ORACLE: post-state.
// ============================================================================
extern "C" void RtGraphics_Geometry_Attach(Geometry* self, Geometry* child, int node) {

    const int32_t sizeBefore = self->_attached.GetSize();

    if (child->_attachParent != nullptr)
        RtGraphics_Geometry_Detach(child->_attachParent, child);

    Geometry* local = child;
    // retail 0x18001f990 — SetSize(size + 1, &fill), the append idiom.
    self->_attached.SetSize(self->_attached.GetSize() + 1, local);
    child->_attachParent = self;
    child->_attachNode   = node;

    Transform sub;
    Transform* p = VGetSubTransform(self, &sub, node);
    RtGraphics_Geometry_SetTransform(child, p);

}

// ============================================================================
// Graphics::Geometry::UpdateAttached()   [VIRTUAL, slot 4]
// retail 0x1800a4020, ordinal 4512   ?UpdateAttached@Geometry@Graphics@@UEAAXXZ
//
// For every attached child: ask this geometry (virtually, slot 5) for the
// sub-transform of the child's node, and if it differs from the child's current
// transform, roll and take it — SetTransform's body INLINED, not a call to it —
// raise the child's dirty bits, and recurse into the child if the child has
// children of its own.
//
// Then, UNCONDITIONALLY, call the child's UpdateAttached (slot 4) again. That is
// not a transcription slip: retail has the recursion both inside the
// "transform changed" branch and once per iteration outside it, so a child with
// children whose transform changed has its UpdateAttached invoked TWICE. The
// second call is a no-op for a child with no children, which is the common case.
//
// ORACLE: post-state. Every child's transform must now equal the parent's
// sub-transform for its node.
// ============================================================================
extern "C" void RtGraphics_Geometry_UpdateAttached(Geometry* self) {

    int i = 0;
    if (self->_attached.GetSize() > 0) {
        do {
            Geometry** data = self->_attached.GetData();
            Geometry*  child = data[i];

            Transform sub;
            Transform* p = VGetSubTransform(self, &sub, child->_attachNode);

            if (TransformDiffers(&child->_transform, p)) {
                child->_prevTransform = child->_transform;
                child->_transform     = *p;
                child->_dirtyFlags   |= 3u;
                if (child->_attached.GetSize() > 0)
                    VUpdateAttached(child);
            }
            // Retail re-reads the array pointer here rather than reusing `data`.
            VUpdateAttached(self->_attached[i]);
            ++i;
        } while (i < self->_attached.GetSize());
    }

}

// ============================================================================
// Graphics::Geometry::Clean()   [VIRTUAL, slot 8]
// retail 0x1800a4420, ordinal 1702   ?Clean@Geometry@Graphics@@UEAA_NXZ
//
// The two-stage dirty-flag machine that gives the renderer a PREVIOUS transform
// and a PREVIOUS world box to interpolate between for exactly one extra frame:
//
//   transform:  bit 0 set -> clear it, set bit 3, report "changed"
//               bit 0 clear, bit 3 set -> roll current into previous, clear
//                                         bit 3, report "changed"
//   bounds:     bit 1 set -> clear it, set bit 4, recompute the world box from
//                            the local box and the current transform, rolling
//                            the old world box into the previous one, and
//                            report "changed"
//               bit 1 clear, bit 4 set -> roll only, clear bit 4, report
//                                         "changed"
//   opacity:    bit 2 set -> clear it, set bit 5. DOES NOT affect the return
//                            value — a pure opacity change is not a reason to
//                            re-index the scene grid.
//               bit 2 clear -> clear bit 5.
//
// Retail's transform branches contain seven dead self-assignment stores
// (`this[0x84] = this[0x84]`, …). They are omitted: they have no observable
// effect and reproducing them would only obscure the machine. The bounds branch
// has the same six, likewise omitted.
//
// ORACLE: FULL REPLAY. Clean's entire effect is the object's own 0xC0 bytes and
// its only callee (the box transform) is pure, so ours and theirs can both run
// from the same snapshot and be compared byte for byte, return value included.
// ============================================================================
extern "C" bool RtGraphics_Geometry_Clean(Geometry* self) {

    Geometry before;

    uint32_t f = self->_dirtyFlags;
    bool changed = false;

    if ((f & 1u) == 0) {
        if ((f & 8u) != 0) {
            self->_prevTransform = self->_transform;
            self->_dirtyFlags &= ~8u;
            changed = true;
        }
    } else {
        self->_dirtyFlags = (f & ~1u) | 8u;
        changed = true;
    }

    f = self->_dirtyFlags;
    if ((f & 2u) == 0) {
        if ((f & 0x10u) != 0) {
            self->_prevWorldBounds = self->_worldBounds;
            self->_dirtyFlags &= ~0x10u;
            changed = true;
        }
        // (f & 0x10) == 0: nothing to do, and `changed` is NOT raised.
    } else {
        self->_dirtyFlags = (f & ~2u) | 0x10u;
        // Retail copies the transform to a local before handing its address to
        // the box helper; kept so the helper never sees the live object.
        const Transform xf = self->_transform;
        float box[6];
        const float* world = RtBoxTransform(box, &xf,
                                            reinterpret_cast<const float*>(&self->_localBounds));
        self->_prevWorldBounds = self->_worldBounds;
        self->_worldBounds.min.x = world[0];
        self->_worldBounds.min.y = world[1];
        self->_worldBounds.min.z = world[2];
        self->_worldBounds.max.x = world[3];
        self->_worldBounds.max.y = world[4];
        self->_worldBounds.max.z = world[5];
        changed = true;
    }

    f = self->_dirtyFlags;
    if ((f & 4u) != 0) self->_dirtyFlags = (f & ~4u) | 0x20u;
    else               self->_dirtyFlags = f & ~0x20u;

    return changed;
}

// ============================================================================
// Graphics::Geometry::SetBounds(const Volume::Box&)
// retail 0x180015700, ordinal 3836
//   ?SetBounds@Geometry@Graphics@@QEAAXAEBVBox@Volume@@@Z
//
// Compare six floats; if any differs, take the box and raise the bounds dirty
// bit. The compare short-circuits in retail's own order (min.x, min.y, min.z,
// max.x, max.y, max.z) — which is only observable through NaN, and is preserved.
//
// ORACLE: FULL REPLAY. Self-contained, no callees, no allocation.
// ============================================================================
extern "C" void RtGraphics_Geometry_SetBounds(Geometry* self, const Box* box) {

    Geometry before;

    if (self->_localBounds.min.x != box->min.x ||
        self->_localBounds.min.y != box->min.y ||
        self->_localBounds.min.z != box->min.z ||
        self->_localBounds.max.x != box->max.x ||
        self->_localBounds.max.y != box->max.y ||
        self->_localBounds.max.z != box->max.z) {
        self->_localBounds = *box;
        self->_dirtyFlags |= 2u;
    }

}

// ============================================================================
// Graphics::Geometry::GetTransform() const
// retail 0x180015800, ordinal 2979
//   ?GetTransform@Geometry@Graphics@@QEBA?AVTransform@Math@@XZ
//
// Returns Math::Transform BY VALUE, so there IS a hidden return slot: RCX =
// this, RDX = the buffer, and RAX = the buffer again. Getting that backwards
// would be a wild write on every call (reference_hidden_retslot_wild_write.md).
//
// ORACLE: FULL. Pure read; both run into private buffers and the 0x1c bytes are
// compared.
// ============================================================================
extern "C" Transform* RtGraphics_Geometry_GetTransform(const Geometry* self, Transform* ret) {
    *ret = self->_transform;
    return ret;   // retail returns the hidden slot in RAX
}

// ============================================================================
// Graphics::Geometry::GetSubTransform(int) const   [VIRTUAL, slot 5]
// retail 0x1800a43c0, ordinal 2949
//   ?GetSubTransform@Geometry@Graphics@@UEBA?AVTransform@Math@@H@Z
//
// The BASE implementation IGNORES the node index and hands back the geometry's
// own transform — a plain Geometry has no sub-nodes. Subclasses (GraphicsMesh's
// MeshInstance in particular) override it to look the node up in the mesh
// skeleton, which is why Attach and UpdateAttached call it virtually.
//
// Hidden return slot in RDX, node index in R8D.
//
// ORACLE: FULL. Pure read.
// ============================================================================
extern "C" Transform* RtGraphics_Geometry_GetSubTransform(const Geometry* self,
                                                          Transform* ret, int node) {
    (void)node;   // retail ignores it in the base body
    *ret = self->_transform;
    return ret;
}

// ============================================================================
// Graphics::Geometry::GetInstanceTransform(int, float) const  [VIRTUAL, slot 6]
// retail 0x1800154a0, ordinal 2720
//   ?GetInstanceTransform@Geometry@Graphics@@UEBA?AVMatrix@Math@@HM@Z
//
// Returns Math::Matrix (64 bytes) BY VALUE — hidden return slot in RDX, node
// index in R8D, the interpolation factor in XMM3. The base body ignores the
// node index and interpolates PREVIOUS -> CURRENT by `t`: the rotation through
// Quaternion::Lerp, the translation as `prev + (cur - prev) * t` component by
// component, in the order x, y, z as the disassembly spells it. The result is
// converted with Transform::AsMatrix.
//
// ORACLE: FULL. Pure read; both run into private Matrix buffers and all 64
// bytes are compared. Bit equality is demanded — the arithmetic is a
// transcription of the same instructions, and both sides call the same
// Quaternion::Lerp and AsMatrix bodies in the DLL.
// ============================================================================
extern "C" Matrix* RtGraphics_Geometry_GetInstanceTransform(const Geometry* self, Matrix* ret,
                                                            int node, float t) {
    (void)node;   // retail ignores it in the base body

    const float py = self->_prevTransform.translation.y;
    const float pz = self->_prevTransform.translation.z;
    const float cx = self->_transform.translation.x;
    const float cy = self->_transform.translation.y;
    const float cz = self->_transform.translation.z;
    const float px = self->_prevTransform.translation.x;

    const Scalar s = { t };
    Transform tmp;
    RtQuaternionLerp(&self->_prevTransform.rotation, &tmp.rotation,
                     &self->_transform.rotation, &s);
    tmp.translation.x = px + (cx - px) * t;
    tmp.translation.y = py + (cy - py) * t;
    tmp.translation.z = pz + (cz - pz) * t;
    RtTransformAsMatrix(&tmp, ret);

    return ret;
}

// ============================================================================
// Graphics::Geometry::SetSharedInstanceData(Video::Context&, int, float) const
//                                                            [VIRTUAL, slot 7]
// retail 0x180015590, ordinal 3965
//   ?SetSharedInstanceData@Geometry@Graphics@@UEBAHAEAVContext@Video@@HM@Z
//
// The whole retail body is `return 0;`. It is the base of an instanced-draw
// hook that only MeshInstance and the particle geometries implement; the base
// answering 0 is how "this geometry contributes no shared instance data" is
// spelled. Nothing is touched, including the Context.
//
// ORACLE: FULL. Pure, no side effects, so both run and the ints are compared.
// ============================================================================
extern "C" int RtGraphics_Geometry_SetSharedInstanceData(const Geometry* self, void* context,
                                                         int node, float t) {
    (void)context; (void)node; (void)t;
    const int ours = 0;
    return ours;
}

// ============================================================================
// Graphics::Scene::Scene()
// retail 0x180102730, ordinal 372   ??0Scene@Graphics@@QEAA@XZ
//
// Initialise the grid with a 2048.0f cell size, then clear the global-object
// list. The grid initialiser stays in the DLL (see the banner): it allocates
// ~350 KB through the allocator lock.
//
// ORACLE: FULL, against a private scratch Scene, built and torn down with the
// DLL's own ctor/dtor so the scratch does not leak its 350 KB on every
// construction. The two big buffers are separate heap allocations, so the
// comparison is "every field except the five Array _data pointers, plus the
// three seeded buffers by CONTENT".
//
// This is not a cheap oracle, and it does not need to be: Graphics::Scene is
// constructed a handful of times in a session (the game world's and the
// minimap's), not per frame.
// ============================================================================
extern "C" Scene* RtGraphics_Scene_Ctor(Scene* self) {

    RtSceneGridInit(self, kSceneCellSize);
    self->_globals._head  = nullptr;
    self->_globals._tail  = nullptr;
    self->_globals._count = 0;

    return self;   // retail returns `this` in RAX
}

// ============================================================================
// Graphics::Scene::~Scene()
// retail 0x1801026c0, ordinal 670   ??1Scene@Graphics@@QEAA@XZ
//
// Six calls and nothing else, in retail's order: the global list first, then
// the four int32/uint32 Arrays from +0x50 down to +0x20, then the node Array at
// +0x10. Each callee is one COMDAT instantiation of Array<T>::~Array (or, for
// the list, the walk-and-free at 0x18005f1f0).
//
// *** NONE OF THE SIX IS IN THE DLL ANY MORE. *** This comment used to end
// "they stay in the DLL because every one of them frees under the allocator's
// Win32 critical section", and that reason dissolved twice: lane Q ported the
// five `Array<T>::~Array` instantiations, and lane GFX3 ported 0x18005f1f0.
// Both reach the heap through the ported `MemFree` static, so nothing here
// needs <windows.h> and nothing here is a cutover door.
//
// ORACLE: post-state only, and that is all it can be. Running the original
// afterwards would double-free every buffer. What is checked is what retail's
// body must leave behind: all five Arrays cleared and the global list empty.
// That catches a wrong offset or a missed call; it does NOT prove the frees
// themselves were correct — only the allocator watchdog
// (BANISHED_REDZONE=watch) can say that.
// ============================================================================
extern "C" void RtGraphics_Scene_Dtor(Scene* self) {

    self->_globals.Clear();          // +0x60  retail 0x18005f1f0
    self->_cellCounts.~Array();      // +0x50  retail 0x18001c750
    self->_levelOffsets.~Array();    // +0x40  retail 0x18001c750
    self->_cellHeads.~Array();       // +0x30  retail 0x18001c750
    self->_freeIds.~Array();         // +0x20  retail 0x18001c750
    self->_nodes.~Array();           // +0x10  retail 0x18005fc20

}

// ============================================================================
// Graphics::Scene::AddObject(Geometry*)
// retail 0x180102670, ordinal 1575
//   ?AddObject@Scene@Graphics@@QEAAIPEAVGeometry@2@@Z
//
// If the geometry has ANY dirty bit set, clean it first (VIRTUAL, slot 8) so
// the grid insert sees a valid world box; then thread it into the grid and
// return the node id.
//
// Note the test: retail reads `_dirtyFlags` as a whole int and compares it
// against 0 — i.e. NeedsCleaning(), not IsDirty() (which masks with 7). A
// geometry carrying only a "rolled" bit is cleaned here too.
//
// ORACLE: post-state. The insert allocates and rethreads a live grid, so it
// cannot be replayed; what is checked is the node the returned id names.
// ============================================================================
extern "C" SceneObjectId RtGraphics_Scene_AddObject(Scene* self, Geometry* geo) {

    if (geo->_dirtyFlags != 0)
        VClean(geo);

    Geometry* local = geo;
    const SceneObjectId id = RtSceneGridAdd(self, &local);

    return id;
}

// ============================================================================
// Graphics::Scene::RemoveObject(unsigned int)
// retail 0x180102600, ordinal 3563   ?RemoveObject@Scene@Graphics@@QEAAXI@Z
//
// Unthread node `id` from its cell, clear the node, and push the id onto the
// free list. Retail computes the node address BEFORE the unthread call (it is
// in RBX across it); same effect, since the unthread never reallocates.
//
// Retail does NOT range-check `id`, and neither does this — CLAUDE.md
// "Don't fix a faithful crash with a null check": an out-of-range id here is an
// upstream bug and a guard would hide it rather than restore the behaviour.
//
// THE +0x14 WORD IS DELIBERATELY NOT WRITTEN. Retail builds `{-1, ?}` on the
// stack and stores it as one qword, where the high dword is an UNINITIALISED
// local (disassembly at 0x180102639-0x18010264b; see the note in Scene.h). A
// port cannot reproduce an indeterminate value, so it writes the half retail
// specifies and leaves the other half alone — and the oracle below does not
// look at it.
//
// ORACLE: post-state.
// ============================================================================
extern "C" void RtGraphics_Scene_RemoveObject(Scene* self, SceneObjectId id) {

    SceneNode* node = &self->_nodes[static_cast<int32_t>(id)];
    const SceneObjectId local = id;

    RtSceneGridUnthread(self, static_cast<int32_t>(id));

    node->_object = nullptr;
    node->_next   = 0;
    node->_prev   = 0;
    node->_cell   = -1;

    const int32_t freeBefore = self->_freeIds.GetSize();
    // retail 0x180023830 — SetSize(size + 1, &fill), the stride-4 append.
    self->_freeIds.SetSize(self->_freeIds.GetSize() + 1, local);

}

// ============================================================================
// Graphics::Scene::AddGlobalObject(Geometry*)
// retail 0x180102540, ordinal 1559
//   ?AddGlobalObject@Scene@Graphics@@QEAAXPEAVGeometry@2@@Z
//
// "Global" objects skip the grid entirely and are visited by every Clean and
// every draw — the sun, the sky dome, the weather emitters. They live on a
// plain doubly linked list whose nodes are 0x18 bytes {element, prev, next},
// exactly Collection::LinkedList<Geometry*>::Node.
//
// Retail inlines Enter/Allocate/Leave; this calls MemoryAllocator::MemAlloc,
// which IS that sequence, and keeps <windows.h> out of runtime/src/.
//
// FAITHFULLY UNGUARDED: when the allocation fails, retail leaves `node` null and
// then, on the non-empty-list path, writes through it (`puVar2[1] = tail`). The
// null check it does perform only decides whether the node's own three fields
// get initialised. That asymmetry is retail's, and reproducing it is the point
// (CLAUDE.md "Don't fix a faithful crash with a null check") — the allocator
// does not fail in practice, and a guard here would turn an allocation failure
// from a crash into a silently corrupt list.
//
// ORACLE: post-state.
// ============================================================================
extern "C" void RtGraphics_Scene_AddGlobalObject(Scene* self, Geometry* geo) {

    const int32_t countBefore = self->_globals.GetCount();

    // ENDGAME B.3.2 step 2 — the inlined node-allocate + tail-append IS
    // `LinkedList<Geometry*>::Add`: `NewNode` (retail 0x180032750) allocates
    // sizeof(Node)=0x18 at alignment 8 and returns null on failure, and `Add`
    // then takes the same two arms and writes through a null node on the
    // non-empty one — retail's asymmetry, kept (see the FAITHFULLY UNGUARDED
    // paragraph above). The only textual difference from what stood here is
    // `NewNode`'s dead `element = T()` store, which is retail's own.
    self->_globals.Add(geo);

}

// ============================================================================
// Graphics::Scene::RemoveGlobalObject(Geometry*)
// retail 0x1801024c0, ordinal 3555
//   ?RemoveGlobalObject@Scene@Graphics@@QEAAXPEAVGeometry@2@@Z
//
// Linear search for the node holding `geo`, unlink it (the four head/tail/prev/
// next fixups in retail's own order), drop the count and free the node under
// the allocator lock. A geometry that is not on the list is silently ignored,
// which is why every one of our four call sites can call it unconditionally.
//
// ORACLE: post-state.
// ============================================================================
extern "C" void RtGraphics_Scene_RemoveGlobalObject(Scene* self, Geometry* geo) {

    const int32_t countBefore = self->_globals.GetCount();

    // ENDGAME B.3.2 step 2 — this search-unlink-free IS
    // `LinkedList<Geometry*>::Remove`, retail 0x18007a010: the head test against
    // the head captured BEFORE the walk, the four unlink arms in retail's order,
    // the two link stores zeroed, the count decrement, and the tail call into
    // 0x180009c30 (the allocator-lock Free whose first argument is ignored —
    // which is why passing `self` here was harmless and why nothing is passed
    // now). A miss is still silent.
    self->_globals.Remove(geo);

}

// ============================================================================
// Graphics::Scene::Clean()
// retail 0x1801023f0, ordinal 1703   ?Clean@Scene@Graphics@@QEAAXXZ
//
// The per-frame refresh. Walk every LIVE node (`_cell >= 0`; free slots are
// skipped by a separate scanning loop retail writes twice, once before the main
// loop and once at the bottom of it), and for each geometry that needs cleaning,
// clean it (VIRTUAL, slot 8) and re-index its grid cell if Clean reported a
// change. Then walk the global list and clean those too — WITHOUT re-indexing,
// because global objects are not in the grid at all.
//
// Retail re-reads `_nodes._size` inside the loop on every iteration, so a Clean
// that adds objects to the scene is seen. Transcribed that way.
//
// This is the function standalone/scene_watch.h exists for: it dispatches
// through every live node's Geometry vtable, so a component freed while its
// scene id is still registered is an "EXEC of <heap address>" here.
//
// ORACLE: post-state. The real invariant is that no live geometry is left
// dirty; the grid re-indexing itself is checked by the AddObject/RemoveObject
// sites and by the scene watch, not here.
// ============================================================================
extern "C" void RtGraphics_Scene_Clean(Scene* self) {
    using List = Collection::LinkedList<Geometry*>;

    int32_t count = self->_nodes.GetSize();
    int32_t i = 0;
    if (count > 0) {
        while (i < count && self->_nodes[i]._cell < 0) ++i;
    }
    if (i < count) {
        do {
            Geometry* geo = self->_nodes[i]._object;
            if (geo->_dirtyFlags != 0 && VClean(geo))
                RtSceneGridReindex(self, i);

            count = self->_nodes.GetSize();
            ++i;
            if (count <= i) break;
            while (i < count && self->_nodes[i]._cell < 0) ++i;
        } while (i < count);
    }

    // ENDGAME B.3.2 step 2 — retail's `for (n = head; n; n = n->next)` over the
    // global list, spelled through the iterator, which IS the bare node pointer.
    for (List::Iterator it = self->_globals.GetFirst(); it != self->_globals.GetEnd(); ++it) {
        Geometry* geo = *it;
        if (geo->_dirtyFlags != 0)
            VClean(geo);
    }

}

// ============================================================================
// Registration.
//
// Both the mangled NAME and the retail ADDRESS are registered for every
// function. The two registries are separate (standalone/resolve_standalone.cpp
// keeps RtExportReg and RtAddrReg apart), and this lane's call sites use both
// doors: almost everything resolves by name, but
// code/Game/Action/showtool.cpp:171 resolves Attach by ADDRESS
// (`ResolveAddr(Module::Runtime, 0x1800a3fa0)`) and
// code/Game/Game/weather_particles.cpp:69 resolves ~Geometry by ADDRESS
// (0x1800a41c0). A name-only registration would have left those two call sites
// on the DLL while every other caller ran the port — two implementations taking
// turns on one live object, and a diff oracle that under-counts.
//
// Every name below is a string LITERAL, not one of the `k*` constants above,
// because tools/runtime_census.py cannot see through a named constant
// (reference_census_cannot_see_nonliteral_registrations.md).
//
// `count` counts NAMES, one per distinct exported function: 19.
// ============================================================================
int RtGraphicsSceneRegister() {
    int count = 0;

    // ---- Graphics::Geometry ----
    RegisterRuntimeExport("??0Geometry@Graphics@@QEAA@XZ", (void*)&RtGraphics_Geometry_Ctor); ++count;
    RegisterRuntimeAddr(0x1800a4270, (void*)&RtGraphics_Geometry_Ctor);

    RegisterRuntimeExport("??1Geometry@Graphics@@UEAA@XZ", (void*)&RtGraphics_Geometry_Dtor); ++count;
    RegisterRuntimeAddr(0x1800a41c0, (void*)&RtGraphics_Geometry_Dtor);

    RegisterRuntimeExport("?Attach@Geometry@Graphics@@QEAAXPEAV12@H@Z", (void*)&RtGraphics_Geometry_Attach); ++count;
    RegisterRuntimeAddr(0x1800a3fa0, (void*)&RtGraphics_Geometry_Attach);

    RegisterRuntimeExport("?Detach@Geometry@Graphics@@QEAAXPEAV12@@Z", (void*)&RtGraphics_Geometry_Detach); ++count;
    RegisterRuntimeAddr(0x1800a3f20, (void*)&RtGraphics_Geometry_Detach);

    RegisterRuntimeExport("?Clean@Geometry@Graphics@@UEAA_NXZ", (void*)&RtGraphics_Geometry_Clean); ++count;
    RegisterRuntimeAddr(0x1800a4420, (void*)&RtGraphics_Geometry_Clean);

    RegisterRuntimeExport("?UpdateAttached@Geometry@Graphics@@UEAAXXZ", (void*)&RtGraphics_Geometry_UpdateAttached); ++count;
    RegisterRuntimeAddr(0x1800a4020, (void*)&RtGraphics_Geometry_UpdateAttached);

    RegisterRuntimeExport("?GetSubTransform@Geometry@Graphics@@UEBA?AVTransform@Math@@H@Z", (void*)&RtGraphics_Geometry_GetSubTransform); ++count;
    RegisterRuntimeAddr(0x1800a43c0, (void*)&RtGraphics_Geometry_GetSubTransform);

    RegisterRuntimeExport("?GetTransform@Geometry@Graphics@@QEBA?AVTransform@Math@@XZ", (void*)&RtGraphics_Geometry_GetTransform); ++count;
    RegisterRuntimeAddr(0x180015800, (void*)&RtGraphics_Geometry_GetTransform);

    RegisterRuntimeExport("?GetInstanceTransform@Geometry@Graphics@@UEBA?AVMatrix@Math@@HM@Z", (void*)&RtGraphics_Geometry_GetInstanceTransform); ++count;
    RegisterRuntimeAddr(0x1800154a0, (void*)&RtGraphics_Geometry_GetInstanceTransform);

    RegisterRuntimeExport("?SetSharedInstanceData@Geometry@Graphics@@UEBAHAEAVContext@Video@@HM@Z", (void*)&RtGraphics_Geometry_SetSharedInstanceData); ++count;
    RegisterRuntimeAddr(0x180015590, (void*)&RtGraphics_Geometry_SetSharedInstanceData);

    RegisterRuntimeExport("?SetBounds@Geometry@Graphics@@QEAAXAEBVBox@Volume@@@Z", (void*)&RtGraphics_Geometry_SetBounds); ++count;
    RegisterRuntimeAddr(0x180015700, (void*)&RtGraphics_Geometry_SetBounds);

    RegisterRuntimeExport("?SetTransform@Geometry@Graphics@@QEAAXAEBVTransform@Math@@@Z", (void*)&RtGraphics_Geometry_SetTransform); ++count;
    RegisterRuntimeAddr(0x1800155a0, (void*)&RtGraphics_Geometry_SetTransform);

    // ---- Graphics::Scene ----
    RegisterRuntimeExport("??0Scene@Graphics@@QEAA@XZ", (void*)&RtGraphics_Scene_Ctor); ++count;
    RegisterRuntimeAddr(0x180102730, (void*)&RtGraphics_Scene_Ctor);

    RegisterRuntimeExport("??1Scene@Graphics@@QEAA@XZ", (void*)&RtGraphics_Scene_Dtor); ++count;
    RegisterRuntimeAddr(0x1801026c0, (void*)&RtGraphics_Scene_Dtor);

    RegisterRuntimeExport("?AddObject@Scene@Graphics@@QEAAIPEAVGeometry@2@@Z", (void*)&RtGraphics_Scene_AddObject); ++count;
    RegisterRuntimeAddr(0x180102670, (void*)&RtGraphics_Scene_AddObject);

    RegisterRuntimeExport("?RemoveObject@Scene@Graphics@@QEAAXI@Z", (void*)&RtGraphics_Scene_RemoveObject); ++count;
    RegisterRuntimeAddr(0x180102600, (void*)&RtGraphics_Scene_RemoveObject);

    RegisterRuntimeExport("?AddGlobalObject@Scene@Graphics@@QEAAXPEAVGeometry@2@@Z", (void*)&RtGraphics_Scene_AddGlobalObject); ++count;
    RegisterRuntimeAddr(0x180102540, (void*)&RtGraphics_Scene_AddGlobalObject);

    RegisterRuntimeExport("?RemoveGlobalObject@Scene@Graphics@@QEAAXPEAVGeometry@2@@Z", (void*)&RtGraphics_Scene_RemoveGlobalObject); ++count;
    RegisterRuntimeAddr(0x1801024c0, (void*)&RtGraphics_Scene_RemoveGlobalObject);

    RegisterRuntimeExport("?Clean@Scene@Graphics@@QEAAXXZ", (void*)&RtGraphics_Scene_Clean); ++count;
    RegisterRuntimeAddr(0x1801023f0, (void*)&RtGraphics_Scene_Clean);

    return count;
}
