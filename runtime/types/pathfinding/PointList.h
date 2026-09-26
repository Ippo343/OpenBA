#pragma once
// ============================================================================
// runtime/types/pathfinding/PointList.h — Pathfinding::PointList, 0x50 bytes,
//                                          9 slots
// ============================================================================
//
// ENDGAME B.3 step 7, block B6 (RULE B7-9). No header existed for this class
// before: the 0x50 layout lived in prose and in an anonymous-namespace struct
// in runtime/src/pathfinding/pointlist.cpp. It is MOVED here (RULE B6-8) and the
// fields are named by the exported getters, read as RETAIL disassembly this
// pass (Runtime-x64.dll, 15290 functions):
//
//   ?Points@PointList@Pathfinding@@QEBAAEBV?$Array@UPoint@...@Collection@@XZ
//                                     0x180071190  LEA RAX,[RCX+0x30] ; RET
//   ?PathPoints@PointList@Pathfinding@@QEBAAEBV?$Array@UPoint@...@Collection@@XZ
//                                     0x1800711a0  LEA RAX,[RCX+0x40] ; RET
//   ?GetMeshName@PointList@Pathfinding@@QEBA?AVString@System@@XZ
//                                     0x1800711b0  MOV [RDX],0 ; RAX=[RCX+0x20] ;
//                                                  bump [RAX-4] ; RAX=RDX — a
//                                                  hidden RETSLOT copy of +0x20
//
//   sizeof 0x50   `MOV EDX,0x50` in its own slot-0 vector deleting destructor
//                 (0x1800713b0) — B37_SLOTS.md section 1 — and `Allocate(0x50, 8)`
//                 in the factory row (rt_factories_pathfinding.inc).
//   parent        "Object::External" (`?_ParentTypeName@PointList@Pathfinding@@`
//                 0x180070e10, B37_SLOTS.md section 3).
//   +0x28         a second System::String. Serialize (0x18010f6e0) hands it to
//                 String::Serialize right after +0x20 and ~PointList releases it;
//                 no getter names it, so it is named by offset (B6-8).
//
// LIVE (B37_RULES 6.2): the exe imports `?_typeInfo@PointList@Pathfinding@@`
// and every InteractDescription / ParticleDescription resource loads one
// through the factory. `code/Game/Component/interactdescription.cpp` reads
// `_points` (+0x30/+0x38) by RAW OFFSET — the asserts below are its contract.
//
// `??_7PointList@Pathfinding@@6B@` @ 0x180158f50, NINE slots (B37_SLOTS.md):
//   [0] +0x00 0x1800713b0  (unexported) vector deleting dtor — RT_DELDTOR row
//   [1] +0x08 0x180070e40  ?TypeName@PointList@Pathfinding@@UEBAPEBDXZ
//   [2] +0x10 0x180070e20  ?Type@PointList@Pathfinding@@UEBAIXZ
//   [3] +0x18 0x18010f880  ?GetFileVersion@PointList@Pathfinding@@UEBAHXZ  (MOV EAX,5)
//   [4] +0x20 0x18010f6d0  ?Compile@PointList@Pathfinding@@  OVERRIDDEN (B7-1.7)
//   [5] +0x28 0x18010f6e0  ?Serialize@PointList@Pathfinding@@UEAA_NAEAVStream@IO@@@Z
//   [6] +0x30 0x180071060  ?Reconstruct@PointList@Pathfinding@@UEAAPEAVExternal@Object@@XZ
//   [7] +0x38 0x180071150  ?Destruct@PointList@Pathfinding@@UEAAXXZ
//   [8] +0x40 0x1800710e0  ?Construct@PointList@Pathfinding@@UEAAPEAVExternal@Object@@XZ
//                          — INTRODUCED HERE (B7-1.6, the B5-16 resource shape)
//
// ABSTRACT (slot 0 `= 0`): nothing holds a PointList by value, and slot 0 is the
// `RT_DELDTOR` row the mint owns (runtime/src/object/deleting_dtor.cpp). NO
// REAL CONSTRUCTOR: live objects are built by the factory rows and the
// Construct/Reconstruct slots and wear the MINTED table (B4b-2). The protected
// `= default` constructor exists for the unit test's probe subclass only.
// ============================================================================

#include <cstdint>
#include <cstddef>

#include "../object/External.h"
#include "../collection/Collection.h"
#include "../system/String.h"

namespace System { class Filename; }

namespace Pathfinding {

class PointList : public Object::External {
public:
    // ------------------------------------------------------------------------
    // Pathfinding::PointList::Point — 0x18 bytes (retail `UPoint@PointList@`).
    // Pinned by Serialize 0x18010f6e0's element loop: String::Serialize at
    // elem+0, kArrayFloat(n=3) at elem+8, kScalarFloat at elem+0x14; the stride
    // 0x18 by the Array<Point> bodies 0x180071730 / 0x180071890 / 0x180071600.
    //
    // `name` STAYS A RAW POINTER: `Collection::Array<Point>` is grown by the
    // hand-transcribed 0x180071730 body, which moves elements byte-wise and
    // bumps the refcount itself; a member with a real copy ctor or destructor
    // would be silently bypassed there. The String release is written out in
    // pointlist.cpp (`ReleasePointsRange`, retail 0x180071890).
    // ------------------------------------------------------------------------
    struct Point {
        void*   name;      // +0x00  System::String (refcounted)
        float   pos[3];    // +0x08
        float   radius;    // +0x14
    };

    // ---- the nine slots, in measured order ----
    Object::Base* DeletingDtor(unsigned flags) override = 0;   // [0] 0x1800713b0

    const char*   TypeName() const override;                   // [1] 0x180070e40
    uint32_t      Type() const override;                       // [2] 0x180070e20
    int32_t       GetFileVersion() const override;             // [3] 0x18010f880
    CompileResult Compile(const System::Filename& path) override; // [4] 0x18010f6d0
    bool          Serialize(void* stream) override;            // [5] 0x18010f6e0
    Object::External* Reconstruct() override;                  // [6] 0x180071060
    void          Destruct() override;                         // [7] 0x180071150
    virtual Object::External* Construct();                     // [8] 0x1800710e0 — B7-1.6

    // ---- the non-slot exported surface (read this pass, see the banner) ----
    const Collection::Array<Point>& Points() const     { return _points; }      // 0x180071190
    const Collection::Array<Point>& PathPoints() const { return _pathPoints; }  // 0x1800711a0
    // 0x1800711b0 — RETSLOT in retail; a plain by-value return here (not a
    // vtable slot, so both ends of every C++ call are this GCC). The copy
    // bumps the refcount exactly as retail's `INC [RAX-4]` does.
    System::String GetMeshName() const { return _meshName; }

    // ---- exported, NOT PORTED, no caller: declared, deliberately undefined --
    PointList(const PointList& src);                          // 0x1800711f0
    PointList& operator=(const PointList& src);               // 0x1800712c0
    static uint32_t TypeId();                                 // 0x180070e30
    static void* operator new(std::size_t bytes);              // 0x180070e50
    static void* operator new(std::size_t bytes, void* place); // 0x180070ec0
    static void  operator delete(void* p);                     // 0x180070ed0
    // The two factory statics are ported table-driven by
    // runtime/src/pathfinding/rt_factories_pathfinding.inc (0x180070f30 /
    // 0x180070ff0); they are not members here.
    static Object::Base* CreateNewObject();                    // 0x180070f30
    static Object::Base* CreateInPlaceObject(void* place);     // 0x180070ff0

protected:
    // NOT retail's `??0PointList@Pathfinding@@QEAA@XZ` (0x18010f980) — see the
    // banner. It exists only so a probe subclass can be declared.
    PointList() = default;
    // `??1PointList@Pathfinding@@UEAA@XZ` 0x18010f8e0. NON-VIRTUAL, DEFINED
    // EMPTY: the registered dtor door (`RtPathfinding_PointList_Dtor`) releases
    // both arrays and both strings and chains to ~External by hand for the
    // minted objects that are the only live ones (the CharacterSet precedent).
    ~PointList();

public:
    // ---- the 0x30 bytes PointList adds (public: the doors and the exe-side
    // raw reader reach them) ----
    System::String           _meshName;    // +0x20  GetMeshName 0x1800711b0
    System::String           _string28;    // +0x28  serialized second; no getter
    Collection::Array<Point> _points;      // +0x30  Points 0x180071190
    Collection::Array<Point> _pathPoints;  // +0x40  PathPoints 0x1800711a0
};

static_assert(sizeof(PointList::Point) == 0x18, "Pathfinding::PointList::Point is 0x18 bytes");
static_assert(offsetof(PointList::Point, pos)    == 0x08, "Point::pos @ +0x08 (kArrayFloat n=3)");
static_assert(offsetof(PointList::Point, radius) == 0x14, "Point::radius @ +0x14 (kScalarFloat)");

static_assert(sizeof(PointList) == 0x50,
              "Pathfinding::PointList is 0x50 bytes (MOV EDX,0x50 in slot 0, 0x1800713b0)");
static_assert(alignof(PointList) == 8, "Pathfinding::PointList aligns to 8 (the vptr)");
static_assert(offsetof(PointList, _meshName)   == 0x20, "GetMeshName 0x1800711b0 reads +0x20");
static_assert(offsetof(PointList, _string28)   == 0x28, "Serialize's second String @ +0x28");
static_assert(offsetof(PointList, _points)     == 0x30, "Points 0x180071190 is LEA [RCX+0x30]");
static_assert(offsetof(PointList, _pathPoints) == 0x40, "PathPoints 0x1800711a0 is LEA [RCX+0x40]");
static_assert(offsetof(PointList, _meshName) == sizeof(Object::External),
              "PointList's own bytes begin right after Object::External");

}  // namespace Pathfinding
