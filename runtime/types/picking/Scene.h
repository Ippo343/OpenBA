#pragma once
// ============================================================================
// runtime/types/picking/Scene.h — binary layouts for the Picking namespace
// ============================================================================
//
// Wave 5, lane D of the Runtime-x64.dll port (docs/runtime_port/PLAN.md §4.2,
// §6 step 3). Every offset below was read out of a decompiled or disassembled
// RETAIL Runtime-x64.dll body, and the specific function and instruction that
// pins it is named next to the field. Nothing here is inferred from the
// profile build: PLAN §6 warns that profile NAMES win but profile OFFSETS must
// not, and this lane took no offset from it.
//
// The full write-up is docs/runtime_port/WAVE5_LANE_D.md.
//
// SCOPE NOTE. The recovered source tree (docs/runtime_port/original_source_tree.txt)
// lists only `Picking\PickingMesh.h` and `Picking\PickingMesh_compile.cpp` for
// this directory, yet retail ships 49 Picking functions across TWO classes
// (`Picking::Scene` is roughly two thirds of them). That recovery is
// string-anchored, so it names only files that left a path string in the
// profile DLL — absence is not evidence a file did not exist.
// `runtime/src/picking/README.md` still says "Original files (1 cpp, 1 h)";
// read that as "1 cpp and 1 h RECOVERED", not as the directory's real contents.
//
// PORTABILITY: no <windows.h>, per PLAN.md §4.1 decision 1.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "../../../engine/types/common.h"     // Vector3
#include "../math/Math.h"                     // Scalar, Quaternion, Transform
#include "../volume/Volume.h"                 // Box
#include "../collection/Collection.h"         // Collection::Array

namespace RtPicking {

// ============================================================
// The per-mesh hit callback carried by every Scene entry.
//
// Spelled out by the mangled parameter of both AddMesh and SetMeshUserData:
//   P6AX 0 HH AEBVVector3@Math@@ 1 1 @Z
//   = void (__cdecl*)(void*, int, int, const Vector3&, const Vector3&,
//                     const Vector3&)
// (`0` back-references the leading void*, `1` the first `Vector3 const&`.)
// Retail never invokes it from any of the eight bodies this lane ported; it is
// stored at entry+0x40 and read back by callers. It is typed here so the
// AddMesh and SetMeshUserData signatures cannot silently lose their last
// argument — which is the exact shape of this project's recorded 5th-argument
// trap (reference_hidden_retval_and_int_pointer_args.md).
// ============================================================
typedef void (*HitCallback)(void* userData, int, int,
                            const Vector3&, const Vector3&, const Vector3&);

// ============================================================
// Picking::PickingMesh::Triangle — 12 bytes: SIX uint16 fields.
//
// The first three are pinned by Scene::Intersect (retail 0x1801246c0): the
// triangle walk advances by 0xc per iteration (`ADD RBX,0xc` at 0x1801250af)
// and reads exactly three ushorts at +0x00/+0x02/+0x04, each multiplied by
// 0xc to index the vertex Array.
//
// The remaining six bytes were "NOT pinned" until vtable batch 8
// (2026-09-12) ported PickingMesh::Serialize (retail 0x122960) and its
// per-SubMesh loop FUN_18007a350 (0x18007a350): it serializes a Triangle as
// TWO back-to-back `kArrayUShort(n=3)` calls, at elem+0x00 and elem+0x06 --
// i.e. the whole 12 bytes are six uint16s, no unknown tail. What j0..j2 MEAN
// is still unverified (a second index triple, e.g. normal/UV indices, is the
// obvious reading) but the BYTES are now fully accounted for.
// ============================================================
struct Triangle {
    uint16_t i0;  // +0x00  vertex index (Intersect)
    uint16_t i1;  // +0x02  vertex index (Intersect)
    uint16_t i2;  // +0x04  vertex index (Intersect)
    uint16_t j0;  // +0x06  second triple (Serialize FUN_18007a350) -- MEANING unverified
    uint16_t j1;  // +0x08
    uint16_t j2;  // +0x0a
};
static_assert(sizeof(Triangle) == 0xc, "PickingMesh::Triangle must be 12 bytes");

// ============================================================
// Picking::PickingMesh::SubMesh — 0x80 bytes, one node of a small hierarchy.
//
// Stride 0x80 is pinned three independent ways: GetNode (0x180071e90) returns
// `_nodes + i*0x80`; Vertices/Normals/Triangles (0x180071e10/e30/e50) return
// `_nodes + <field> + i*0x80`; and AddMesh (0x1801252e0) walks the array with
// `lVar14 += 0x80`.
//
// | Off  | Field       | Pinned by                                           |
// |------|-------------|-----------------------------------------------------|
// | 0x00 | _name       | PINNED by vtable batch 8 (2026-09-12): PickingMesh::
// |      |             | Serialize's per-SubMesh loop (0x18007ac20) opens with
// |      |             | `String::Serialize(elem+0x00, ser)`, and the fresh-tail
// |      |             | init in FUN_18007aa40 zeroes this qword -- a System::String
// | 0x08 | _parent     | AddMesh reads `*(int*)(nodes + 8 + i*0x80)` and uses |
// |      |             | it to index the WORLD transform array it is filling, |
// |      |             | so it is a parent index; its loop starts at i == 1,  |
// |      |             | i.e. node 0 is the root and is never composed        |
// | 0x0c | _local      | AddMesh: quaternion at +0x0c, translation at         |
// |      |             | +0x1c..+0x27 (`lVar15 = base+0xc`, then +0x10/+0x14/ |
// |      |             | +0x18 off it) — exactly a ::Transform            |
// | 0x28 | _aabb       | PickingMesh::AABB (0x180071dd0) copies six dwords    |
// |      |             | from +0x28..+0x3f; FUN_180125bc0 reads the same six  |
// | 0x40 | _vertices   | Vertices  (0x180071e10)                              |
// | 0x50 | _normals    | Normals   (0x180071e30)                              |
// | 0x60 | _triangles  | Triangles (0x180071e50). TriangleCount (0x180071e70) |
// |      |             | reads +0x68, which IS that Array's _size — the two   |
// |      |             | agreeing is the cross-check on the Array layout      |
// | 0x70 | _uvs        | PINNED by vtable batch 8 (2026-09-12): the tail of the |
// |      |             | same Serialize loop reads a size at elem+0x78, resizes  |
// |      |             | via FUN_18007a900 (stride 4, align 2 -- two uint16s),    |
// |      |             | and serializes `kArrayUShort(n=2)` per element -- an     |
// |      |             | Array<UV16>, not an opaque 16-byte blob                 |
// ============================================================
// Picking::PickingMesh::UV16 — 4 bytes, two uint16s. Named for what a
// per-vertex-adjacent 2-ushort array on a picking submesh most plausibly
// holds (a compact UV pair); MEANING is unverified, byte shape is not.
struct UV16 {
    uint16_t u;  // +0x00
    uint16_t v;  // +0x02
};
static_assert(sizeof(UV16) == 4, "PickingMesh::UV16 must be 4 bytes");

struct SubMesh {
    void*                        _name;           // +0x00  System::String
    int32_t                      _parent;         // +0x08
    ::Transform              _local;          // +0x0c  (0x1c bytes)
    ::Box                  _aabb;           // +0x28
    Collection::Array<Vector3>   _vertices;       // +0x40
    Collection::Array<Vector3>   _normals;        // +0x50
    Collection::Array<Triangle>  _triangles;      // +0x60
    Collection::Array<UV16>      _uvs;            // +0x70
};
static_assert(sizeof(SubMesh) == 0x80, "PickingMesh::SubMesh must be 0x80 bytes");
static_assert(offsetof(SubMesh, _parent)    == 0x08, "SubMesh::_parent @ +0x08");
static_assert(offsetof(SubMesh, _local)     == 0x0c, "SubMesh::_local @ +0x0c");
static_assert(offsetof(SubMesh, _aabb)      == 0x28, "SubMesh::_aabb @ +0x28");
static_assert(offsetof(SubMesh, _vertices)  == 0x40, "SubMesh::_vertices @ +0x40");
static_assert(offsetof(SubMesh, _normals)   == 0x50, "SubMesh::_normals @ +0x50");
static_assert(offsetof(SubMesh, _triangles) == 0x60, "SubMesh::_triangles @ +0x60");
static_assert(offsetof(SubMesh, _uvs)       == 0x70, "SubMesh::_uvs @ +0x70");

// ============================================================
// Picking::PickingMesh — only the two fields this lane needs are pinned.
//
// The class has an exported vtable (`??_7PickingMesh@Picking@@6B@`, data
// export at 0x180158fb0) and is an Object::External subclass, so +0x00.. is
// the External base. This lane ports exactly one PickingMesh method,
// `Vertices`, so it pins exactly the two fields that Vertices, Normals,
// Triangles, TriangleCount, GetNode, GetSubMeshCount and AABB all agree on.
// Everything below +0x38 is deliberately opaque: putting an unverified
// External base in a header is the failure mode Collection.h's banner warns
// about, and nothing here needs it.
//
//   +0x38  SubMesh* _nodes      — the base every accessor above indexes
//   +0x40  int      _nodeCount  — GetSubMeshCount (0x180071dc0) returns it,
//                                 and AddMesh (0x1801252e0) uses it as the
//                                 SetSize argument for the transform array
// ============================================================
struct PickingMesh {
    uint8_t  _opaque00[0x38];   // +0x00  External base + fields not pinned here
    SubMesh* _nodes;            // +0x38
    int32_t  _nodeCount;        // +0x40
    // sizeof is deliberately NOT pinned — never allocate one of these.
};
static_assert(offsetof(PickingMesh, _nodes)     == 0x38, "PickingMesh::_nodes @ +0x38");
static_assert(offsetof(PickingMesh, _nodeCount) == 0x40, "PickingMesh::_nodeCount @ +0x40");

// ============================================================
// Picking::Scene::Entry — 0x48 bytes, one registered pick mesh.
//
// The strongest single witness is the internal entry allocator at retail
// 0x18007b210, which builds a complete entry on the stack and hands it to the
// array grower: every field below is one store in that body. The exported
// accessors then confirm each one independently.
//
// | Off  | Field       | Pinned by                                           |
// |------|-------------|-----------------------------------------------------|
// | 0x00 | _queryStamp | Intersect (0x1801246c0) writes the Scene's stamp here|
// |      |             | so an entry spanning several cells is tested once;   |
// |      |             | 0x18007b210 initialises it to 0                      |
// | 0x04 | _nextFree   | 0x18007b210 stores -2 ("in use") on both paths and,  |
// |      |             | on the reuse path, pops the free list with           |
// |      |             | `scene->_freeEntry = entries[_freeEntry]._nextFree`  |
// | 0x08 | _cellX      | FUN_1800724d0 (the re-index behind SetTransform)     |
// | 0x0c | _cellY      | compares and stores all four of these                |
// | 0x10 | _cellW      | ...as UINT16: `*(ushort*)(base + 0x10 + i*0x48)`     |
// | 0x12 | _cellH      | ...and `+ 0x12`                                      |
// | 0x18 | _mesh       | GetMesh (0x180072200) returns this qword. Intersect  |
// |      |             | dereferences it directly to reach PickingMesh+0x38,  |
// |      |             | so ExternalPtr<T> is a bare pointer in this slot     |
// | 0x20 | _transforms | GetTransform (0x180072280) indexes `*(ptr+0x20)` by  |
// |      |             | 0x1c; Intersect bounds its submesh loop on           |
// |      |             | `*(int*)(entry+0x28)`, which IS that Array's _size   |
// | 0x30 | _userData   | GetUserData (0x1800722d0), SetMeshUserData arg 2     |
// | 0x38 | _userValue  | GetUserValue (0x1800722e0), SetMeshUserData arg 3    |
// | 0x3c | _userValue2 | SetMeshUserData arg 4 — no getter is exported        |
// | 0x40 | _callback   | SetMeshUserData arg 5 / AddMesh arg 7                |
//
// +0x14 is slack that 8-aligns _mesh; +0x2c is the transform Array's
// _capacity; +0x44 is slack that 8-aligns _callback.
// ============================================================
struct Entry {
    int32_t  _queryStamp;                            // +0x00
    int32_t  _nextFree;                              // +0x04  -2 while allocated
    int32_t  _cellX;                                 // +0x08
    int32_t  _cellY;                                 // +0x0c
    uint16_t _cellW;                                 // +0x10
    uint16_t _cellH;                                 // +0x12
    uint8_t  _pad14[4];                              // +0x14
    void*    _mesh;                                  // +0x18  ExternalPtr<PickingMesh>
    Collection::Array<::Transform> _transforms;  // +0x20
    void*       _userData;                           // +0x30
    int32_t     _userValue;                          // +0x38
    int32_t     _userValue2;                         // +0x3c
    HitCallback _callback;                           // +0x40
};
static_assert(sizeof(Entry) == 0x48, "Picking::Scene::Entry must be 0x48 bytes");
static_assert(offsetof(Entry, _nextFree)   == 0x04, "Entry::_nextFree @ +0x04");
static_assert(offsetof(Entry, _cellX)      == 0x08, "Entry::_cellX @ +0x08");
static_assert(offsetof(Entry, _cellW)      == 0x10, "Entry::_cellW @ +0x10");
static_assert(offsetof(Entry, _cellH)      == 0x12, "Entry::_cellH @ +0x12");
static_assert(offsetof(Entry, _mesh)       == 0x18, "Entry::_mesh @ +0x18");
static_assert(offsetof(Entry, _transforms) == 0x20, "Entry::_transforms @ +0x20");
static_assert(offsetof(Entry, _userData)   == 0x30, "Entry::_userData @ +0x30");
static_assert(offsetof(Entry, _userValue)  == 0x38, "Entry::_userValue @ +0x38");
static_assert(offsetof(Entry, _userValue2) == 0x3c, "Entry::_userValue2 @ +0x3c");
static_assert(offsetof(Entry, _callback)   == 0x40, "Entry::_callback @ +0x40");

// ============================================================
// Picking::Scene::Cell — 0x10 bytes, one occupied grid cell.
//
// Pinned entirely by the cell iterator at retail 0x18007b6d0, which is the
// traversal Intersect drives:
//     hash = (y * -0x27e9c7bf + x * -0x72594cbd) % scene->_cellHeads._size
//     if (hash < 0) hash += scene->_cellHeads._size
//     for (c = _cellHeads[hash]; c >= 0; c = cells[c]._nextInBucket)
//         if (cells[c]._x == x && cells[c]._y == y) -> cells[c]._firstLink
// ============================================================
struct Cell {
    int32_t _x;             // +0x00
    int32_t _y;             // +0x04
    int32_t _nextInBucket;  // +0x08  index into _cells, -1 = end of chain
    int32_t _firstLink;     // +0x0c  index into _links, -1 = empty
};
static_assert(sizeof(Cell) == 0x10, "Picking::Scene::Cell must be 16 bytes");

// ============================================================
// Picking::Scene::Link — 8 bytes. One (cell, entry) membership.
//
// Pinned by the cell iterator and by Intersect, which both read
// `*(int*)(_links + linkIdx*8)` as the ENTRY index and
// `*(int*)(_links + linkIdx*8 + 4)` as the next link, and by FUN_180072730
// (the add-to-cells pass), which writes the pair as one qword
// `CONCAT44(oldHead, entryIndex)` — entry index low, next high.
// ============================================================
struct Link {
    int32_t _entry;  // +0x00
    int32_t _next;   // +0x04  index into _links, -1 = end
};
static_assert(sizeof(Link) == 8, "Picking::Scene::Link must be 8 bytes");

// ============================================================
// Picking::Scene — 0x60 bytes. A uniform 2D spatial hash over components
// [0] and [1] of a Vector3 — the XY plane, NOT XZ. Two independent witnesses:
// Intersect builds its query rectangle from `origin[0]`/`origin[1]` and
// `dir[0]`/`dir[1]`, and FUN_180125bc0 projects a ::Box by taking
// min[0], min[1], max[0], max[1] (Box+0x00, +0x04, +0x0c, +0x10).
//
// Scene has NO vtable. Three witnesses: the ctor and dtor are both `QEAA`
// (non-virtual) in the export table; the ctor's first store is a float at
// +0x00, not a vptr; and no `??_7Scene@Picking@@6B@` exists in the export
// directory, while `??_7PickingMesh@Picking@@6B@` does. So the "port a class
// whole or its vtable ownership splits" hazard (PLAN §4.3 risk 3) does NOT
// apply here: a Scene may safely have some methods ported and some still in
// the DLL, provided this layout is exact.
//
// Whole-object witness: the ctor is `FUN_18007aef0(this, 16.0f)`, whose
// disassembly (0x18007af06..0x18007af55) writes, in order, +0x00, +0x04,
// +0x08, +0x10, +0x18, +0x20, +0x28 = -1, +0x30, +0x38, +0x40 = -1, +0x48,
// +0x50, +0x58 = -1, +0x5c = 0 — and nothing else. Note it never writes
// +0x2c or +0x44, so those two dwords are UNINITIALISED in a live Scene; any
// oracle that memcmps a whole Scene must skip them. The dtor (0x180125890)
// then frees exactly +0x48, +0x30, +0x18, +0x08 and clears each Array's
// {data,size,capacity}, leaving _cellSize, the three free heads and
// _queryStamp untouched.
//
// | Off  | Field        | Pinned by                                          |
// |------|--------------|----------------------------------------------------|
// | 0x00 | _cellSize    | ctor stores the 16.0f at DAT_1801616fc             |
// | 0x04 | _invCellSize | ctor stores 1.0f/_cellSize. EVERY cell index in     |
// |      |              | Intersect, FUN_18007af70 and FUN_1800724d0 is       |
// |      |              | `(int)(coord * *(float*)(scene+4))` — CVTTSS2SI,    |
// |      |              | i.e. TRUNCATION toward zero, not floor              |
// | 0x08 | _cellHeads   | Array<int>, SetSize(10000) filled with -1 by        |
// |      |              | FUN_18007b960; the iterator takes its modulus       |
// |      |              | against this Array's _size at +0x10                 |
// | 0x18 | _cells       | Array<Cell>                                         |
// | 0x28 | _freeCell    | ctor -1                                             |
// | 0x30 | _links       | Array<Link>; FUN_180072730 grows it                 |
// | 0x40 | _freeLink    | ctor -1; FUN_180072730 pops and pushes it           |
// | 0x48 | _entries     | Array<Entry>; FUN_18007b210 grows it                |
// | 0x58 | _freeEntry   | ctor -1; FUN_18007b210 pops it                      |
// | 0x5c | _queryStamp  | `INC dword ptr [RCX+0x5c]` at 0x1801247bf, the      |
// |      |              | first thing Intersect does after reading its args   |
// ============================================================
struct Scene {
    float                      _cellSize;      // +0x00
    float                      _invCellSize;   // +0x04
    Collection::Array<int32_t> _cellHeads;     // +0x08
    Collection::Array<Cell>    _cells;         // +0x18
    int32_t                    _freeCell;      // +0x28
    int32_t                    _pad2c;         // +0x2c  never written by the ctor
    Collection::Array<Link>    _links;         // +0x30
    int32_t                    _freeLink;      // +0x40
    int32_t                    _pad44;         // +0x44  never written by the ctor
    Collection::Array<Entry>   _entries;       // +0x48
    int32_t                    _freeEntry;     // +0x58
    int32_t                    _queryStamp;    // +0x5c
};
static_assert(sizeof(Scene) == 0x60, "Picking::Scene must be 0x60 (96) bytes");
static_assert(offsetof(Scene, _cellHeads)  == 0x08, "Scene::_cellHeads @ +0x08");
static_assert(offsetof(Scene, _cells)      == 0x18, "Scene::_cells @ +0x18");
static_assert(offsetof(Scene, _freeCell)   == 0x28, "Scene::_freeCell @ +0x28");
static_assert(offsetof(Scene, _links)      == 0x30, "Scene::_links @ +0x30");
static_assert(offsetof(Scene, _freeLink)   == 0x40, "Scene::_freeLink @ +0x40");
static_assert(offsetof(Scene, _entries)    == 0x48, "Scene::_entries @ +0x48");
static_assert(offsetof(Scene, _freeEntry)  == 0x58, "Scene::_freeEntry @ +0x58");
static_assert(offsetof(Scene, _queryStamp) == 0x5c, "Scene::_queryStamp @ +0x5c");

// ============================================================
// The descriptor AddMesh hands to the entry allocator FUN_18007b210. It is
// byte-for-byte the tail of an Entry (Entry+0x18 .. Entry+0x47), which is
// exactly why that allocator can copy it field for field.
// Pinned by AddMesh's own stack frame and by FUN_18007b210's reads of
// param_2[0], param_2+1, param_2[3], param_2[4], (char*)param_2+0x24 and
// param_2[5].
// ============================================================
struct AddDesc {
    void*                              _mesh;        // +0x00
    Collection::Array<::Transform> _transforms;  // +0x08
    void*                              _userData;    // +0x18
    int32_t                            _userValue;   // +0x20
    int32_t                            _userValue2;  // +0x24
    HitCallback                        _callback;    // +0x28
};
static_assert(sizeof(AddDesc) == 0x30, "Picking AddMesh descriptor must be 0x30 bytes");
static_assert(offsetof(AddDesc, _userData) == 0x18, "AddDesc::_userData @ +0x18");
static_assert(offsetof(AddDesc, _callback) == 0x28, "AddDesc::_callback @ +0x28");

}  // namespace RtPicking
