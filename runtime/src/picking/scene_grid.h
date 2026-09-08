#pragma once
// ============================================================================
// runtime/src/picking/scene_grid.h — Wave 9 lane PICK
// ============================================================================
//
// Shared between scene_grid.cpp (the nineteen ported bodies) and
// scene_grid_probe.cpp (the synthetic-subject oracle). It exists for exactly
// the reason array_internal.h exists: a probe that re-derives the body it is
// probing is testing a copy of the port, not the port.
//
// PORTABILITY: no <windows.h>, per PLAN.md §4.1 decision 1.
// ============================================================================

#include <cstdint>

#include "../../types/picking/Scene.h"

namespace rtpickgrid {

using RtPicking::Scene;
using RtPicking::Entry;
using RtPicking::Cell;
using RtPicking::Link;
using RtPicking::AddDesc;

// ---------------------------------------------------------------------------
// The cell-rectangle iterator `Picking::Scene::Intersect` drives.
//
// Layout pinned by Intersect's own frame at [RSP+0x30] and re-confirmed by
// every displacement in 0x18007b6d0's disassembly. This is the same struct
// scene.cpp declares for its forwarder; the two static_asserts below are the
// contract between them.
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// The nineteen bodies, in dependency order. The five the address-door census
// named are marked (*); the other fourteen are their transitive closure and
// were NOT reachable from our tree before this lane — porting the five without
// them would have swapped five unregistered address doors for fourteen new
// ones, which is not progress.
//
// Every one is registered by ADDRESS only (RegisterRuntimeAddr). None is
// exported: `tools/pe_exports.py` finds no name in either the retail or the
// profile export directory whose address is any of these nineteen, so
// RegisterRuntimeExport has nothing to register under. That is stated
// explicitly rather than left implied, because "a port reached by address
// needs BOTH doors" is the normal rule and this is the exception to it.
// ---------------------------------------------------------------------------

// 0x18001c100  Array<T>::~Array for trivially-destructible T (the J1 body).
void  ArrayDtorTransform(void* arrayHdr);
// 0x18001b360  Array<Math::Transform>::operator=
void* TransformArrayAssign(void* dst, const void* src);
// 0x180074100  destroy `count` Entries' inner Array<Transform>, in place
void  DestroyEntryTransforms(void* ignored, void* base, int count);
// 0x1800735c0  Entry copy-CONSTRUCT (used by the Array<Entry> grower)
void* EntryCopyConstruct(void* dst, const void* src);
// 0x18007b590  Entry::operator=
void* EntryAssign(void* dst, const void* src);
// 0x18001f470  Volume::Box transformed by a Math::Transform -> AABB
void* BoxTransform(void* outBox, const void* transform, const void* inBox);
// 0x180125bc0  project a mesh's node AABBs through their transforms, XY only
void* MeshBoundsXY(const void* addDescTail, void* outRect);
// 0x1800738c0  Array<Cell>::SetSize(n, fill)
void  CellArraySetSize(void* arrayHdr, int newSize, const void* fill);
// 0x18007bc30  Array<Entry>::SetSize(n, fill)
void  EntryArraySetSize(void* arrayHdr, int newSize, const void* fill);
// 0x180072e70  unlink a now-empty Cell from its bucket, push on the free list
void  FreeEmptyCell(Scene* scene, Cell* cell);
// 0x180072d10  unlink one entry's Link from one Cell's list
void  UnlinkEntryFromCell(Scene* scene, Cell* cell, int32_t entryId);
// 0x180072b30  find the Cell for (x,y), creating it if absent
Cell* FindOrCreateCell(Scene* scene, const int32_t* xy);
// 0x180072730  thread an entry into every cell of a rectangle
void  AddToCells(Scene* scene, const int32_t* xy, uint16_t w, uint16_t h,
                 int32_t entryId);
// 0x18007b210  allocate an Entry (free list or grow) and fill it in
int32_t EntryAlloc(Scene* scene, const AddDesc* desc, const int32_t* xy,
                   uint16_t w, uint16_t h);

// (*) The five census rows.
uint32_t GridAdd(Scene* scene, const AddDesc* desc);                  // 0x18007af70
void     GridReindex(Scene* scene, uint32_t id);                      // 0x1800724d0
void     GridUnthread(Scene* scene, int32_t* cellXY, uint16_t w,
                      uint16_t h, int32_t id);                        // 0x180072950
void     GridFreeEntry(Scene* scene, uint32_t id);                    // 0x18007b390
void     CellIterAdvance(CellIter* it);                               // 0x18007b6d0

// ---------------------------------------------------------------------------
// Probe scheduling — the same hazard lane Q documents in array_internal.h.
//
// The probe MUST NOT run from the registration function. That runs immediately
// after ResolveInit()'s LoadLibrary, and the DLL's `MemoryAllocator` has been
// CONSTRUCTED by then but not STARTED — `Engine::Startup` does that later.
// Allocating from it at that moment is exactly the "works on my boot order"
// hazard this project keeps paying for, and this probe allocates a lot (two
// 10,000-entry cell-head tables for a start).
//
// So it self-schedules: `g_probePending` is armed at registration and the first
// call into any of the five top-level entry points runs it. That moment is
// provably safe, because the caller is itself in the middle of using the grid.
// `g_probeRunning` breaks the recursion, since the probe drives those same
// entry points.
extern bool g_probePending;
extern bool g_probeRunning;

}  // namespace rtpickgrid

// The probe. Runs only when BANISHED_RT_PICKGRID_PROBE is set; see
// scene_grid_probe.cpp.
void RtPickingGridProbe();
