#pragma once
// ============================================================================
// runtime/types/graphics/Scene.h — Graphics::Scene binary layout
// ============================================================================
//
// Wave 6 lane D of the Runtime-x64.dll port (docs/runtime_port/PLAN.md §4.2
// wave 6, §6 step 3).
//
// `Graphics::Scene` is NOT polymorphic: the export directory of the shipped
// Runtime-x64.dll contains no `??_7Scene@Graphics@@6B@`, its ctor stores no
// vtable, and every one of its exported members is `QEAA`/`QEBA` (non-virtual)
// or `AEBA` (private non-virtual). It is a quadtree-style hash grid of
// `Graphics::Geometry*` with a separate doubly linked list of "global" objects
// that skip the grid and are always visited.
//
// The grid half of the object is shared with other classes: the ctor body,
// the insert, the unthread and the re-index are all `FUN_`-named internals that
// several containers call. This lane calls those at their retail addresses
// rather than re-deriving them, exactly as Wave 5 lane D did for
// `Picking::Scene` (runtime/src/picking/scene.cpp) — see the banner in
// runtime/src/graphics/scene.cpp for the argument.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <type_traits>                     // the destructor assert on Graphics::Scene

#include "../collection/Collection.h"      // Collection::Array, Collection::LinkedList
#include "Geometry.h"

namespace RtGraphics {

// ============================================================
// Graphics::Scene::Node — 0x18 bytes, the grid's per-object record.
//
// Pinned by three bodies that agree term for term:
//   Scene::RemoveObject (0x180102600) indexes `nodes + id*0x18`, zeroes the
//     first two qwords and writes -1 at +0x10.
//   Scene::Clean (0x1801023f0) skips a node whose `*(int*)(node+0x10)` is
//     negative and otherwise reads the Geometry* at +0x00.
//   FUN_18005f090 (the grid unthread) uses `*(int*)(node+0x10)` as an index
//     into the cell-head array at Scene+0x30, and splices the node out of that
//     cell's chain through the two int32 links at +0x08 (next) and +0x0c (prev).
//
// +0x14 IS A DON'T-CARE, AND RETAIL WRITES GARBAGE INTO IT.
// Both RemoveObject and the grid insert FUN_18005e5e0 build the `{-1, ?}` pair
// on the stack as `CONCAT44(uStack_14, 0xffffffff)` and store it as ONE qword —
// where `uStack_14` is an uninitialised local. Read out of the disassembly at
// 0x180102639: `MOV dword ptr [RSP+0x30], 0xffffffff` then
// `MOV RAX, qword ptr [RSP+0x30]`, with nothing ever writing [RSP+0x34]. This is
// the same shape as the shipped-DLL uninitialised read Wave 5 found in
// Picking::Scene::RemoveMesh (reference_shadow_predicate_must_name_its_failing_term.md).
// Consequence for this lane: no oracle may compare +0x14, and our port must not
// pretend to a value retail does not have.
// ============================================================
struct SceneNode {
    Geometry* _object;    // +0x00
    int32_t   _next;      // +0x08  next node index in this cell's chain
    int32_t   _prev;      // +0x0c  previous node index
    int32_t   _cell;      // +0x10  cell index; < 0 means the slot is free
    int32_t   _pad14;     // +0x14  DON'T CARE — retail stores uninitialised stack here
};
static_assert(sizeof(SceneNode) == 0x18, "Graphics::Scene::Node must be 0x18 bytes");
static_assert(offsetof(SceneNode, _next) == 0x08, "SceneNode::_next @ +0x08");
static_assert(offsetof(SceneNode, _cell) == 0x10, "SceneNode::_cell @ +0x10");

// ============================================================
// Graphics::Scene — 0x78 bytes.
//
// Whole-object witness: ??0Scene@Graphics@@QEAA@XZ (0x180102730) is
// `FUN_18005e4a0(this, 2048.0f); _globalHead = 0; _globalTail = 0;
//  _globalCount = 0;` — and the grid initialiser FUN_18005e4a0 writes every
// field from +0x00 to +0x5f. The destructor (0x1801026c0) then names the five
// containers by freeing them in reverse: the global list at +0x60, then the
// Arrays at +0x50, +0x40, +0x30, +0x20, then the node Array at +0x10.
//
//   +0x00  _cellSize        FUN_18005e4a0's float argument, from
//                           DAT_180161730 = 0x45000000 = 2048.0f (bytes read out
//                           of the shipped DLL, not a Ghidra rendering).
//   +0x04  _halfCellX       both set to _cellSize * 0.5f, written as one qword
//   +0x08  _halfCellY       (`CONCAT44(p*0.5f, p*0.5f)`).
//   +0x0c  _levelCount      8. FUN_18005ee30 compares the object's chosen level
//                           against it before clamping.
//   +0x10  _nodes           Array<Node>: Clean reads the count at +0x18,
//                           RemoveObject indexes the buffer at +0x10.
//   +0x20  _freeIds         Array<uint32>: RemoveObject appends the freed id
//                           through FUN_180023830 (the stride-4 SetSize) and
//                           the insert FUN_18005e5e0 pops the last entry.
//   +0x30  _cellHeads       Array<int32>, 0x5555 entries, filled with -1 by the
//                           ctor. FUN_18005f090 indexes it by node->_cell.
//   +0x40  _levelOffsets    Array<int32>, 8 entries, seeded
//                           {0, 1, 5, 0x15, 0x55, 0x155, 0x555, 0x1555} —
//                           the quadtree level base indices (4^n prefix sums).
//   +0x50  _cellCounts      Array<int32>, 0x5555 entries, filled with 0.
//   +0x60  _globals         LinkedList<Geometry*>: head +0x60, tail +0x68,
//                           count +0x70. AddGlobalObject (0x180102540)
//                           allocates a 0x18-byte node {element, prev, next} and
//                           links it at the tail; RemoveGlobalObject
//                           (0x1801024c0) unlinks and frees it. That is exactly
//                           the Collection::LinkedList shape recovered in Wave 2
//                           (runtime/types/collection/Collection.h).
//   +0x74  padding, never touched (the LinkedList's own _pad14).
// ============================================================
struct Scene {
    float                          _cellSize;      // +0x00
    float                          _halfCellX;     // +0x04
    float                          _halfCellY;     // +0x08
    int32_t                        _levelCount;    // +0x0c
    Collection::Array<SceneNode>   _nodes;         // +0x10 {data, size@0x18, cap@0x1c}
    Collection::Array<uint32_t>    _freeIds;       // +0x20
    Collection::Array<int32_t>     _cellHeads;     // +0x30
    Collection::Array<int32_t>     _levelOffsets;  // +0x40
    Collection::Array<int32_t>     _cellCounts;    // +0x50
    Collection::LinkedList<Geometry*> _globals;    // +0x60 {head, tail@0x68, count@0x70}
};

static_assert(sizeof(Scene) == 0x78, "Graphics::Scene must be 0x78 bytes");
static_assert(offsetof(Scene, _levelCount)   == 0x0c, "Scene::_levelCount @ +0x0c");
static_assert(offsetof(Scene, _nodes)        == 0x10, "Scene::_nodes @ +0x10");
static_assert(offsetof(Scene, _freeIds)      == 0x20, "Scene::_freeIds @ +0x20");
static_assert(offsetof(Scene, _cellHeads)    == 0x30, "Scene::_cellHeads @ +0x30");
static_assert(offsetof(Scene, _levelOffsets) == 0x40, "Scene::_levelOffsets @ +0x40");
static_assert(offsetof(Scene, _cellCounts)   == 0x50, "Scene::_cellCounts @ +0x50");
static_assert(offsetof(Scene, _globals)      == 0x60, "Scene::_globals @ +0x60");

// The grid's cell-head / cell-count arrays are both this many entries. Written
// as a literal because the ctor's fill loop is bounded by 0x15554 bytes, i.e.
// 0x5555 int32s (FUN_18005e4a0's `while (lVar1 < 0x15554)`).
constexpr int kSceneCellCount = 0x5555;

// The node index type AddObject returns and RemoveObject takes.
using SceneObjectId = uint32_t;

}  // namespace RtGraphics

// ############################################################################
// ENDGAME B.3 step 5 (block F) — `Graphics::Scene`, the REAL C++ class
// ############################################################################
//
// `B35_RULES.md` §2.4: 0x78 bytes, NON-POLYMORPHIC, a ROOT. The export directory
// of the shipped Runtime-x64.dll contains no `??_7Scene@Graphics@@6B@` and no
// `?_typeInfo@Scene@Graphics@@`, its constructor stores no vtable, and every one
// of its fifteen exports is `QEAA`/`QEBA` (public non-virtual), `SA` (static) or
// `AEBA` (private non-virtual — and this DLL exports its private members,
// reference_aeaa_is_private_not_unexported.md). The banner at the top of this
// file established all of that in wave 6 and block 0 re-read it from bytes.
//
// -------------------------------------------------------------------------
// B35_RULES §13 ITEM 5 — "WHETHER ANYTHING HOLDS `Graphics::Scene` BY VALUE IS
// NOT ESTABLISHED". CLOSED BY BLOCK F, and the answer is NOTHING DOES.
// -------------------------------------------------------------------------
// Block 0's search covered `runtime/types` + `engine/types` only and it called
// its own result "an absence of evidence from a grep, not a proof". Block F
// repeated it over `code/`, `engine/`, `standalone/`, `dx11/` and
// `runtime/types`, by BINDING rather than by name — `Graphics::Scene`,
// `RtGraphics::Scene`, a `0x78`-sized member, and the game loops block 0 named as
// the suspects. Every site that owns a Scene owns it INDIRECTLY:
//
//   code/Game/Game/gameupdateloop_load.cpp:1090  MemAlloc(0x78) then the ctor
//                                                door; the loop keeps a
//                                                `void* m_graphicsScene` at
//                                                GameUpdateLoop+0x60
//                                                (engine/types/GameUpdateLoop.h:61)
//   code/Game/Game/menudialogloopt.cpp:715       MemAlloc(0x78, 8) then the ctor
//                                                door; held as `void* scene`
//                                                (menudialogloopt_funcs.h:70)
//   code/Game/Game/minimap_paint.cpp:250         `alignas(16) uint8_t scene[256]`
//                                                — RAW STORAGE, not a typed
//                                                member: no default constructor
//                                                is required of `Scene` and no
//                                                copy is ever made of it
//   engine/types/{FencedTool,PlacementTool}.h    an int32 scene OBJECT ID, not a
//                                                Scene
//
// So no `Collection::Array<Scene>`, no embedded `Scene` member, no `Scene` copy
// anywhere. THE CONSEQUENCE FOR THE SHAPE: B4b-3's "concrete + protected
// `= default` + friend the holder" does not apply, and B35_RULES §2.4's "real
// ctor/dtor allowed? NO" stands unchanged for the reason B4b-2 gives rather than
// for a by-value holder. `??0Scene@Graphics@@QEAA@XZ` (0x180102730) and
// `??1Scene@Graphics@@QEAA@XZ` (0x1801026c0) keep their doors —
// `fn_Graphics_Scene_Ctor` has three call sites and `engine_funcs` resolves the
// destructor by name — and both are DECLARED below and deliberately left
// UNDEFINED, exactly as block E left `Graphics::Geometry::~Geometry`'s siblings.
//
// RULE B5-7 applies here as much as to `Camera` and `Renderer`: the
// `Engine::Loader` thread constructs the Scene (`gameupdateloop_load.cpp:1090`)
// and calls `Scene::AddGlobalObject` (`:1654`) while the main thread is inside
// `Draw`. Nothing below adds a static initialiser, a lazy singleton or a
// non-atomic first-call cache.
//
// -------------------------------------------------------------------------
// THE GRID INTERNALS ARE NOT MEMBERS, AND THAT IS DELIBERATE
// -------------------------------------------------------------------------
// The six `FUN_`-named grid bodies (`GridInit` 0x18005e4a0, `GridAdd`
// 0x18005e5e0, `GridReindex` 0x18005ee30, `GridUnthread` 0x18005f090,
// `GridThread` 0x18005f4b0, `FreeGlobals` 0x18005f1f0) are UNEXPORTED internals
// shared with other containers, ported in `runtime/src/graphics/scene_grid.cpp`
// against the `RtGraphics::Scene` POD and registered by ADDRESS. They are not
// `Graphics::Scene` members in retail — a member would have a mangled name — so
// they stay where they are, and the members below call them exactly where retail
// calls them. That is why the POD has to survive this conversion quite apart from
// B4b-1's rule: `scene_grid.cpp` and `scene_grid_probe.cpp` are written against
// it and `scene_grid.h` is their shared contract.
// ############################################################################

namespace Graphics {

class Renderer;   // block F (runtime/types/graphics/Camera.h)

// ============================================================
// Graphics::Scene::Node — 0x18 bytes, the grid's per-object record.
//
// Spelled at NAMESPACE scope and aliased into `Scene` as `Scene::Node`, so
// retail's own nested name resolves. (`Scene` holds an `Array<Node>` by value, so
// the type must be complete before `Scene` is, and `Node` cannot be defined
// inside it without also being complete first — the same nesting cycle
// `Renderer::SceneGroup` hits in Camera.h.)
//
// +0x14 IS A DON'T-CARE AND RETAIL WRITES GARBAGE INTO IT — both `RemoveObject`
// and the grid insert build the `{-1, ?}` pair on the stack as
// `CONCAT44(uStack_14, 0xffffffff)` and store it as ONE qword, where `uStack_14`
// is never written (disassembly at 0x180102639). No oracle may compare it and no
// port may pretend to a value retail does not have. The POD's banner above is the
// full derivation.
// ============================================================
class SceneNode {
public:
    Geometry* _object;    // +0x00
    int32_t   _next;      // +0x08  next node index in this cell's CIRCULAR chain
    int32_t   _prev;      // +0x0c  previous node index
    int32_t   _cell;      // +0x10  cell index; < 0 means the slot is free
    int32_t   _pad14;     // +0x14  DON'T CARE — retail stores uninitialised stack
};
static_assert(sizeof(SceneNode) == 0x18, "Graphics::Scene::Node is 0x18 bytes");
static_assert(alignof(SceneNode) == 8, "the Geometry* at +0x00");
static_assert(sizeof(SceneNode) == sizeof(RtGraphics::SceneNode),
              "the real class and the POD describe the same bytes");
static_assert(offsetof(SceneNode, _next)  == offsetof(RtGraphics::SceneNode, _next),  "alias _next");
static_assert(offsetof(SceneNode, _prev)  == offsetof(RtGraphics::SceneNode, _prev),  "alias _prev");
static_assert(offsetof(SceneNode, _cell)  == offsetof(RtGraphics::SceneNode, _cell),  "alias _cell");
static_assert(offsetof(SceneNode, _pad14) == offsetof(RtGraphics::SceneNode, _pad14), "alias _pad14");
static_assert(offsetof(SceneNode, _next)  == 0x08, "SceneNode::_next @ +0x08 (FUN_18005f090)");
static_assert(offsetof(SceneNode, _cell)  == 0x10, "SceneNode::_cell @ +0x10 (Scene::Clean 0x1801023f0)");

// ============================================================
// Graphics::Scene — 0x78 bytes. A quadtree-style hash grid of `Geometry*` plus a
// doubly linked list of "global" objects that skip the grid and are always
// visited. The field evidence is the POD's banner above, function by function.
//
// FIFTEEN exports (B35_SLOTS.md), and after this block ELEVEN of them are
// ported — the nine wave 6 lane D ported (`scene.cpp`), `AddCulledDrawItems`
// (`cull.cpp`), `AddGlobalDrawItems` (`wave29_lane_gfxbody.cpp`) and the public
// `AddDrawItems`, which block F read out of the DLL and ported this pass. The
// four that stay are the copy constructor, `operator=` and the three
// class-scoped allocation operators, none of which has a caller anywhere in the
// tree; each is declared below and deliberately left undefined.
// ============================================================
class Scene {
public:
    // Retail's nested name for the grid record.
    using Node     = SceneNode;
    // The node index type `AddObject` returns and `RemoveObject` takes.
    using ObjectId = RtGraphics::SceneObjectId;

    // ---- lifetime: BOTH STAY DOORS (B4b-2, B35_RULES §11) ------------------
    // `fn_Graphics_Scene_Ctor` has three call sites and `engine_funcs.cpp:3095`
    // resolves the destructor by name. Declared so no caller batch has to edit
    // this header; deliberately UNDEFINED.
    Scene();                                     // 0x180102730  door: scene.cpp
    ~Scene();                                    // 0x1801026c0  door: scene.cpp

    // Exported, NOT PORTED, no caller anywhere: declared, deliberately undefined.
    Scene(const Scene& src);                     // 0x18005e290  not ported
    Scene& operator=(const Scene& src);          // 0x18005e2e0  not ported
    static void* operator new(size_t bytes);              // 0x18005e120  not ported
    static void* operator new(size_t bytes, void* at);    // 0x18005e190  not ported
    static void  operator delete(void* p);                // 0x18005e1a0  not ported

    // ---- the grid ----------------------------------------------------------
    ObjectId AddObject(Geometry* geometry);      // 0x180102670
    void     RemoveObject(ObjectId id);          // 0x180102600
    void     Clean();                            // 0x1801023f0

    // ---- the global list ---------------------------------------------------
    void AddGlobalObject(Geometry* geometry);    // 0x180102540
    void RemoveGlobalObject(Geometry* geometry); // 0x1801024c0

    // ---- drawing -----------------------------------------------------------
    // `?AddDrawItems@Scene@Graphics@@QEBAXAEAVRenderer@2@@Z` (0x1801023b0) is the
    // PUBLIC entry and the only caller of the two private ones; block F read it
    // this pass (13 instructions) and ported it into `scene.cpp`.
    void AddDrawItems(Renderer& renderer) const;             // 0x1801023b0

    // Both are `AEBA` in retail — PRIVATE const members, which this DLL exports
    // anyway (reference_aeaa_is_private_not_unexported.md). They are declared
    // PUBLIC here for the same reason `SunLight::CreateTargets` (also `AEAA`) is:
    // access control is not ABI, and each one's REGISTERED DOOR is a free
    // `extern "C"` function that has to call it. Retail's access is recorded in
    // this comment rather than enforced by a `friend` list that would have to name
    // two door symbols in a layout header.
    void AddCulledDrawItems(Renderer& renderer) const;       // 0x1801017a0  cull.cpp
    void AddGlobalDrawItems(Renderer& renderer) const;       // 0x180102330  wave29_lane_gfxbody.cpp

public:
    float                             _cellSize;      // +0x00  2048.0f
    float                             _halfCellX;     // +0x04  written as one qword
    float                             _halfCellY;     // +0x08  with _halfCellX
    int32_t                           _levelCount;    // +0x0c  8
    Collection::Array<SceneNode>      _nodes;         // +0x10
    Collection::Array<uint32_t>       _freeIds;       // +0x20
    Collection::Array<int32_t>        _cellHeads;     // +0x30  0x5555 entries, -1
    Collection::Array<int32_t>        _levelOffsets;  // +0x40  8 entries, 4^n sums
    Collection::Array<int32_t>        _cellCounts;    // +0x50  0x5555 entries, 0
    Collection::LinkedList<Geometry*> _globals;       // +0x60  head/tail/count
};

static_assert(sizeof(Scene) == 0x78, "Graphics::Scene is 0x78 bytes");
static_assert(alignof(Scene) == 8, "Scene's widest members are pointers");
static_assert(sizeof(Scene) == sizeof(RtGraphics::Scene),
              "the real class and the POD describe the same bytes");
// Nothing holds a Scene by value (§13 item 5, closed above), but the Arrays and
// the LinkedList still own heap, so the destructor must NOT be trivial — that is
// what `??1Scene@` frees, in retail's order, through its door.
static_assert(!std::is_trivially_destructible_v<Scene>,
              "the five Arrays and the global list are what ??1Scene@ frees");

static_assert(offsetof(Scene, _cellSize)     == offsetof(RtGraphics::Scene, _cellSize),     "alias _cellSize");
static_assert(offsetof(Scene, _levelCount)   == offsetof(RtGraphics::Scene, _levelCount),   "alias _levelCount");
static_assert(offsetof(Scene, _nodes)        == offsetof(RtGraphics::Scene, _nodes),        "alias _nodes");
static_assert(offsetof(Scene, _freeIds)      == offsetof(RtGraphics::Scene, _freeIds),      "alias _freeIds");
static_assert(offsetof(Scene, _cellHeads)    == offsetof(RtGraphics::Scene, _cellHeads),    "alias _cellHeads");
static_assert(offsetof(Scene, _levelOffsets) == offsetof(RtGraphics::Scene, _levelOffsets), "alias _levelOffsets");
static_assert(offsetof(Scene, _cellCounts)   == offsetof(RtGraphics::Scene, _cellCounts),   "alias _cellCounts");
static_assert(offsetof(Scene, _globals)      == offsetof(RtGraphics::Scene, _globals),      "alias _globals");

// And against the retail bodies directly, so a POD edit cannot move both.
static_assert(offsetof(Scene, _levelCount)   == 0x0c, "Scene::_levelCount @ +0x0c (FUN_18005ee30)");
static_assert(offsetof(Scene, _nodes)        == 0x10, "Scene::_nodes @ +0x10 (Scene::Clean 0x1801023f0)");
static_assert(offsetof(Scene, _freeIds)      == 0x20, "Scene::_freeIds @ +0x20 (RemoveObject 0x180102600)");
static_assert(offsetof(Scene, _cellHeads)    == 0x30, "Scene::_cellHeads @ +0x30 (FUN_18005f090)");
static_assert(offsetof(Scene, _levelOffsets) == 0x40, "Scene::_levelOffsets @ +0x40");
static_assert(offsetof(Scene, _cellCounts)   == 0x50, "Scene::_cellCounts @ +0x50");
static_assert(offsetof(Scene, _globals)      == 0x60, "Scene::_globals @ +0x60 (AddGlobalObject 0x180102540)");

}  // namespace Graphics
