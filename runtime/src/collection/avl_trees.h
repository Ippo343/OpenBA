// ============================================================================
// runtime/src/collection/avl_trees.h — Wave 9 lane COLL
//   The keyed `Collection::Set`/`Collection::Map` insert side, shared between
//   avl_trees.cpp and its lockstep probe.
// ============================================================================
//
// It exists for the same reason set_avl.h does: the probe must drive the SAME
// bodies the registered addresses drive, against the SAME node layouts. A
// probe that re-declared either would be testing a copy of the port rather
// than the port.
// ============================================================================

#pragma once

#include <cstdint>

#include "../../types/system/Filename.h"   // RtSystem::Filename (FilenamePair's key)

namespace RtObject { struct TypeNode; struct TypeRecord; struct Factory; }
namespace rtsetavl { struct Node; struct Set; }

namespace rtavl {

// ---------------------------------------------------------------------------
// EFFECTIVENESS COUNTERS — branches taken INSIDE the bodies under test.
//
// WAVE9.md's rule, learned three times over: "count branches taken INSIDE the
// function under test, not proxies observed from outside it." Every outside
// proxy this wave tried was either structurally pinned or swamped by the common
// path, and the control that was wrong twice said PASS both times.
//
// So these are incremented by the ported bodies themselves, at the exact
// branches whose absence would make a green run meaningless: a rotation that
// never ran, a pre-rotation the population never reached, a descent that only
// ever went one way, a comparator that only ever returned one sign.
//
// They are unconditional plain increments. The call volume is bounded (~400
// type registrations, ~20 settings keys, one insert per file per packfile) and
// a conditional would need a load anyway.
// ---------------------------------------------------------------------------
struct ProbeCounters {
    // the 0x50-node family
    uint64_t rot50L, rot50R, preRot50L, preRot50R, retrace50Iters;
    // descent shape, all three keyed trees
    uint64_t descLeft, descRight, equalHit, rootInsert;
    // comparator outcomes — a comparator that only ever returns one sign is
    // indistinguishable from a broken one
    uint64_t strcmpLt, strcmpEq, strcmpGt;
    uint64_t icmpLt,   icmpEq,   icmpGt;
    // the null arms of the two string descents, which are asymmetric and are
    // exactly what a "tidied" comparator would collapse
    uint64_t nullSearchKey, nullNodeKey;
    // Array<T*>::operator=
    uint64_t ptrAssignRealloc, ptrAssignTreble, ptrAssignSelf;
    // the recursive free
    uint64_t freeNodes, freeStringsReleased;
};
extern ProbeCounters g_pc;

// ---- group A: Set<Pair<uint32_t, TypeRecord>>, 0x50-byte node -------------
void*                PtrArrayDtor  (void* self);                       // 0x1800093e0
RtObject::TypeNode*  RotateLeft50 (RtObject::Factory*, RtObject::TypeNode*);   // 0x1800307b0
RtObject::TypeNode*  RotateRight50(RtObject::Factory*, RtObject::TypeNode*);   // 0x1800308e0
void                 Retrace50    (RtObject::Factory*, RtObject::TypeNode**);  // 0x180030540
// 0x180030140 (TypeSetInsert) — deleted in ENDGAME B.3.3 step 7; Object::Factory
// is a Collection::Set. 0x1800087c0 / 0x180008440 / 0x180030380 / 0x180009350
// (pair ctor, node alloc, insert core, Array<T*>::operator=) — deleted in B.4
// R-COLL: the template's insert reaches this group only through Retrace50.

// ---------------------------------------------------------------------------
// B.4 (R-COLL) — the two keyed families of the 0x30-byte node, typed.
//
// rtsetavl::Node (set_avl.h) is the ERASED node the shared retrace / rotations
// / erase take (element opaque, folded by MSVC across every 16-byte element).
// The two insert families below know their element, so they build and walk
// THESE, and hand them to the erased core at the one retrace call. Both node
// layouts are rtsetavl::Node's, asserted against it in avl_trees.cpp.
// ---------------------------------------------------------------------------

// Group B's element: two System::String data pointers (0x18000fb00 copies
// +0x00 then +0x08, AddRef'ing each).
struct StringPair {
    char* key;     // +0x00
    char* value;   // +0x08
};

// The node 0x18000f970 allocates (`MOV EDX,0x30`) and 0x18000fb00 builds:
// element at +0x00 / +0x08, parent +0x10, the two null children +0x18 / +0x20,
// and ONE dword zero at +0x28 over balance and height.
struct StringPairNode {
    StringPair      element;   // +0x00
    StringPairNode* parent;    // +0x10
    StringPairNode* left;      // +0x18
    StringPairNode* right;     // +0x20
    int16_t         balance;   // +0x28
    int16_t         height;    // +0x2a
    uint32_t        pad2c;     // +0x2c  never written by the ctor
};

// Group D's element: the Filename key and the packed {packIndex, fileIndex}
// qword (0x180008840: Filename copy ctor at +0x00, a raw qword move at +0x08).
struct FilenamePair {
    RtSystem::Filename key;     // +0x00
    uint64_t           value;   // +0x08
};

// The node 0x180008520 allocates (0x30, align 8) and 0x180008840 builds; the
// link stores are 0x18000fb00's, offset for offset.
struct FilenamePairNode {
    FilenamePair      element;   // +0x00
    FilenamePairNode* parent;    // +0x10
    FilenamePairNode* left;      // +0x18
    FilenamePairNode* right;     // +0x20
    int16_t           balance;   // +0x28
    int16_t           height;    // +0x2a
    uint32_t          pad2c;     // +0x2c  never written by the ctor
};

// The {root, count} container header both families' insert cores take
// (set_avl.h's `Set`, with the family's node).
template <class NodeT>
struct KeyedSet {
    NodeT*  root;    // +0x00
    int32_t count;   // +0x08
    int32_t pad0c;   // +0x0c
};

// ---- group B: Map<String,String>, 0x30-byte node --------------------------
StringPairNode*  StringPairCtor  (StringPairNode* node, const StringPair* src,
                                  StringPairNode* parent);              // 0x18000fb00
StringPairNode*  StringNodeAlloc (void* ignored, const StringPair* pair,
                                  StringPairNode* parent);              // 0x18000f970
StringPairNode** StringInsertCore(KeyedSet<StringPairNode>*, StringPairNode**,
                                  const StringPair* pair);              // 0x18002a170
rtsetavl::Node** StringMapInsert (rtsetavl::Set*, rtsetavl::Node**,
                                  char* const* key, char* const* value);  // 0x18002a090

// ---- group D: Set<Pair<Filename,uint64>>, 0x30-byte node ------------------
FilenamePairNode*  FilenamePairCtor  (FilenamePairNode* node, const FilenamePair* src,
                                      FilenamePairNode* parent);        // 0x180008840
FilenamePairNode*  FilenameNodeAlloc (void* ignored, const FilenamePair* pair,
                                      FilenamePairNode* parent);        // 0x180008520
FilenamePairNode** FilenameInsertCore(KeyedSet<FilenamePairNode>*, FilenamePairNode**,
                                      const FilenamePair* pair);        // 0x180025c70
rtsetavl::Node**   FilenameSetInsert (rtsetavl::Set*, rtsetavl::Node**,
                                      const void* key, const uint64_t* value);  // 0x180025a80

// ---- the recursive subtree free ------------------------------------------
void SubtreeFree(void* ignoredThis, rtsetavl::Node* node);             // 0x180008340

}  // namespace rtavl

