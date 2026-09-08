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
void*                PtrArrayAssign(void* dst, const void* src);       // 0x180009350
RtObject::TypeNode*  RotateLeft50 (RtObject::Factory*, RtObject::TypeNode*);   // 0x1800307b0
RtObject::TypeNode*  RotateRight50(RtObject::Factory*, RtObject::TypeNode*);   // 0x1800308e0
void                 Retrace50    (RtObject::Factory*, RtObject::TypeNode**);  // 0x180030540
RtObject::TypeNode*  TypePairCtor (void* node, const void* src,
                                   RtObject::TypeNode* parent);        // 0x1800087c0
RtObject::TypeNode*  TypeNodeAlloc(void* ignored, const void* pair,
                                   RtObject::TypeNode* parent);        // 0x180008440
RtObject::TypeNode** TypeInsertCore(RtObject::Factory*, RtObject::TypeNode**,
                                    const void* pair);                 // 0x180030380
RtObject::TypeNode** TypeSetInsert(RtObject::Factory*, RtObject::TypeNode**,
                                   const uint32_t* key,
                                   const RtObject::TypeRecord* value); // 0x180030140

// ---- group B: Map<String,String>, 0x30-byte node --------------------------
rtsetavl::Node*  StringPairCtor  (void* node, const void* src, rtsetavl::Node* parent);
rtsetavl::Node*  StringNodeAlloc (void* ignored, const void* pair, rtsetavl::Node* parent);
rtsetavl::Node** StringInsertCore(rtsetavl::Set*, rtsetavl::Node**, const void* pair);
rtsetavl::Node** StringMapInsert (rtsetavl::Set*, rtsetavl::Node**,
                                  char* const* key, char* const* value);

// ---- group D: Set<Pair<Filename,uint64>>, 0x30-byte node ------------------
rtsetavl::Node*  FilenamePairCtor  (void* node, const void* src, rtsetavl::Node* parent);
rtsetavl::Node*  FilenameNodeAlloc (void* ignored, const void* pair, rtsetavl::Node* parent);
rtsetavl::Node** FilenameInsertCore(rtsetavl::Set*, rtsetavl::Node**, const void* pair);
rtsetavl::Node** FilenameSetInsert (rtsetavl::Set*, rtsetavl::Node**,
                                    const void* key, const uint64_t* value);

// ---- the recursive subtree free ------------------------------------------
void SubtreeFree(void* ignoredThis, rtsetavl::Node* node);             // 0x180008340

}  // namespace rtavl

// The probe. Armed by RtCollectionAvlTreesPortInit, run on the first real call
// into any registered entry point — it ALLOCATES, and a probe that allocates
// cannot run from a *PortInit (WAVE9.md, lane PKG2: the allocator has not
// published its singleton yet and the first allocation faults at null+0x640).
void RtCollectionAvlProbeArm(bool on);
bool RtCollectionAvlProbeArmed();
void RtCollectionAvlProbeRun();
extern "C" void RtCollectionAvlProbeMaybeRun();

int RtCollectionAvlTreesRegister();
int RtCollectionSortRegister();
