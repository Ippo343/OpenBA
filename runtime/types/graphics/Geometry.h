#pragma once
// ============================================================================
// runtime/types/graphics/Geometry.h — Graphics::Geometry binary layout
// ============================================================================
//
// Wave 6 lane D of the Runtime-x64.dll port (docs/runtime_port/PLAN.md §4.2
// wave 6, §6 step 3). Every offset below is pinned by a named retail body,
// listed next to it; nothing here is inferred from a sibling class.
//
// `Graphics::Geometry` is the POLYMORPHIC base of every drawable in the engine
// (MeshInstance, ParticleSystemInstance, the terrain geometry, and — on our
// side — StatusIconComponent's embedded billboard). Its exported vtable is
//
//     ??_7Geometry@Graphics@@6B@   retail 0x180154e60, ordinal 1439
//
// and it has exactly NINE slots. The count is not a guess: the bytes at
// 0x180154e60 are nine pointers followed immediately by the ASCII string
// "ImageBuffer\0", which is how a vtable's end is read in this project
// (reference_vtable_slot_count_from_the_name_string.md).
//
//   slot 0  +0x00  0x180015e90  scalar deleting destructor
//   slot 1  +0x08  0x18007ec28  _purecall  (Draw)
//   slot 2  +0x10  0x18007ec28  _purecall
//   slot 3  +0x18  0x18007ec28  _purecall  (AddDrawItems)
//   slot 4  +0x20  0x1800a4020  UpdateAttached
//   slot 5  +0x28  0x1800a43c0  GetSubTransform
//   slot 6  +0x30  0x1800154a0  GetInstanceTransform
//   slot 7  +0x38  0x180015590  SetSharedInstanceData
//   slot 8  +0x40  0x1800a4420  Clean
//
// Three bodies in this lane dispatch through that table rather than calling a
// sibling directly, and the slot numbers above are how they were read:
// Attach and UpdateAttached call slot 5 (`vt+0x28`), SetTransform and
// UpdateAttached tail-call slot 4 (`vt+0x20`), Scene::Clean calls slot 8
// (`vt+0x40`).
//
// ABI NOTE — see runtime/types/math/Math.h. GetTransform, GetSubTransform and
// GetInstanceTransform all return a class BY VALUE, so each carries a HIDDEN
// RETURN SLOT in RDX and returns it again in RAX. Getting that wrong is a wild
// write that still produces the right answer
// (reference_hidden_retslot_wild_write.md).
// ============================================================================

#include <cstdint>
#include <cstddef>

#include "../../../engine/types/common.h"   // Vector3
#include "../math/Math.h"                   // Quaternion, Transform, Matrix, Scalar
#include "../volume/Volume.h"               // Box
#include "../collection/Collection.h"       // Collection::Array

namespace RtGraphics {

// ============================================================
// Graphics::Geometry — 0xC0 bytes.
//
// Whole-object witness: ??0Geometry@Graphics@@QEAA@XZ (retail 0x1800a4270,
// ordinal 205) writes every field below except the four bytes at +0x14 and the
// four at +0xBC, which it leaves UNINITIALISED. Independently corroborated on
// the exe side by engine/types/StatusIconComponent.h, which reserves
// `uint8_t geometry[0xC0]` at +0x28 and has been live in-game for months.
//
// Per-field evidence:
//   +0x00  vtable          ctor stores &??_7Geometry@Graphics@@6B@; the dtor
//                          (0x1800a41c0) RE-STAMPS it before doing anything
//                          else, which is what makes a subclass dtor safe.
//   +0x08  _attachParent   Attach (0x1800a3fa0) writes `child+0x08 = this`;
//                          Detach (0x1800a3f20) and ~Geometry clear it.
//   +0x10  _attachNode     Attach writes the node index there; Detach and
//                          ~Geometry write -1. Ctor writes -1.
//   +0x14  NOT PINNED, NOT INITIALISED. No decompiled body reads or writes it.
//   +0x18  _attached       Array<Geometry*>: Attach grows it through
//                          FUN_18001f990 (the stride-8 Array<T>::SetSize),
//                          ~Geometry frees it through FUN_18001bf50
//                          (Array<T>::~Array), and SetTransform / Detach /
//                          UpdateAttached all read the count at +0x20.
//   +0x28  _detailId       ?GetDetailId@ (0x180015890) is `return *(int*)(this+0x28)`.
//                          Ctor default is 2.
//   +0x2c  _dirtyFlags     ?IsDirty@ (0x180015860) tests `& 7`;
//                          ?NeedsCleaning@ (0x180015870) tests `!= 0`;
//                          SetBounds ORs 2, SetTransform ORs 3, SetOpacity ORs 4,
//                          Clean (0x1800a4420) is the whole state machine.
//                          Ctor default is 7 (all three inputs dirty).
//   +0x30  _materialTypeFlags  ?GetMaterialTypeFlags@ (0x1800157f0).
//   +0x34  _localBounds    SetBounds (0x180015700) compares then copies six
//                          floats to +0x34..+0x48, and Clean feeds &this[0x34]
//                          to the box-transform helper FUN_18001f470.
//   +0x4c  _worldBounds    Clean writes the transformed box here; the ctor
//                          seeds it EMPTY (min = +1e8, max = -1e8).
//   +0x64  _prevWorldBounds  Clean rolls +0x4c into it before overwriting.
//                          Also the box Graphics::MeshInstance::AddDrawItems
//                          unions for its LOD pick (standalone/scene_watch.h).
//   +0x7c  _opacity        ?GetOpacity@ (0x1800156f0). Ctor 1.0f.
//   +0x80  _opacityPrev    ?SetOpacity@ (0x1800156c0) writes BOTH as one qword
//                          (`CONCAT44(v,v)`), so they only ever differ if
//                          something else writes one of them. Ctor 1.0f.
//   +0x84  _transform      SetTransform (0x1800155a0) compares all seven
//                          floats, then writes them.
//   +0xa0  _prevTransform  SetTransform rolls +0x84 into it first; Clean rolls
//                          it again on the flag-8 path; GetInstanceTransform
//                          lerps from it towards +0x84.
//   +0xbc  NOT PINNED, NOT INITIALISED — the ctor's last store is at +0xb8.
// ============================================================
struct Geometry {
    void*                     _vtable;            // +0x00
    Geometry*                 _attachParent;      // +0x08
    int32_t                   _attachNode;        // +0x10
    int32_t                   _pad14;             // +0x14  uninitialised, unused
    Collection::Array<Geometry*> _attached;       // +0x18 {data, size@0x20, cap@0x24}
    int32_t                   _detailId;          // +0x28
    uint32_t                  _dirtyFlags;        // +0x2c
    uint32_t                  _materialTypeFlags; // +0x30
    Box                       _localBounds;       // +0x34
    Box                       _worldBounds;       // +0x4c
    Box                       _prevWorldBounds;   // +0x64
    float                     _opacity;           // +0x7c
    float                     _opacityPrev;       // +0x80
    Transform                 _transform;         // +0x84
    Transform                 _prevTransform;     // +0xa0
    uint32_t                  _padBC;             // +0xbc  uninitialised, unused
};

static_assert(sizeof(Geometry) == 0xC0, "Graphics::Geometry must be 0xC0 bytes");
static_assert(offsetof(Geometry, _attachParent)      == 0x08, "Geometry::_attachParent @ +0x08");
static_assert(offsetof(Geometry, _attachNode)        == 0x10, "Geometry::_attachNode @ +0x10");
static_assert(offsetof(Geometry, _attached)          == 0x18, "Geometry::_attached @ +0x18");
static_assert(offsetof(Geometry, _detailId)          == 0x28, "Geometry::_detailId @ +0x28");
static_assert(offsetof(Geometry, _dirtyFlags)        == 0x2c, "Geometry::_dirtyFlags @ +0x2c");
static_assert(offsetof(Geometry, _materialTypeFlags) == 0x30, "Geometry::_materialTypeFlags @ +0x30");
static_assert(offsetof(Geometry, _localBounds)       == 0x34, "Geometry::_localBounds @ +0x34");
static_assert(offsetof(Geometry, _worldBounds)       == 0x4c, "Geometry::_worldBounds @ +0x4c");
static_assert(offsetof(Geometry, _prevWorldBounds)   == 0x64, "Geometry::_prevWorldBounds @ +0x64");
static_assert(offsetof(Geometry, _opacity)           == 0x7c, "Geometry::_opacity @ +0x7c");
static_assert(offsetof(Geometry, _opacityPrev)       == 0x80, "Geometry::_opacityPrev @ +0x80");
static_assert(offsetof(Geometry, _transform)         == 0x84, "Geometry::_transform @ +0x84");
static_assert(offsetof(Geometry, _prevTransform)     == 0xa0, "Geometry::_prevTransform @ +0xa0");

// ============================================================
// Geometry::_dirtyFlags bit meanings.
//
// Read entirely out of Clean (0x1800a4420) plus the three setters that raise
// them. The low three bits are the "input changed" bits IsDirty() tests; the
// three bits above them are the "changed LAST frame, so the previous-value copy
// is still fresh" bits Clean rolls them into and then clears on the next call.
// That two-stage roll is what gives the renderer a previous transform and a
// previous world box to interpolate between for one extra frame.
// ============================================================
enum GeometryDirty : uint32_t {
    kDirtyTransform     = 0x01,  // SetTransform / UpdateAttached raise it
    kDirtyBounds        = 0x02,  // SetBounds and SetTransform raise it
    kDirtyOpacity       = 0x04,  // SetOpacity raises it
    kRolledTransform    = 0x08,  // Clean: set when 0x01 was seen, cleared next call
    kRolledBounds       = 0x10,  // Clean: set when 0x02 was seen, cleared next call
    kRolledOpacity      = 0x20,  // Clean: set when 0x04 was seen, cleared next call
};

// ============================================================
// Vtable slot indices, for the three ported bodies that dispatch virtually.
// ============================================================
enum GeometryVSlot : int {
    // BATCH 4: Scene::AddCulledDrawItems (0x1801017a0) ends by dispatching this
    // slot on every surviving object as `(geometry, scene, renderer)` — the
    // `CALL [RAX+0x18]` at 0x18010226e. `?AddDrawItems@SunLight@Graphics@@UEBAX
    // AEBVScene@2@AEAVRenderer@2@@Z` (0x180106f50, ported in
    // wave28_lane_partsys.cpp) is one override of it, which is the independent
    // confirmation of both the index and the argument tuple.
    kVSlotAddDrawItems         = 3,   // vt + 0x18
    kVSlotUpdateAttached       = 4,   // vt + 0x20
    kVSlotGetSubTransform      = 5,   // vt + 0x28
    kVSlotGetInstanceTransform = 6,   // vt + 0x30
    kVSlotSetSharedInstanceData= 7,   // vt + 0x38
    kVSlotClean                = 8,   // vt + 0x40
};

}  // namespace RtGraphics
