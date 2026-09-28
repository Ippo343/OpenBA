// ============================================================================
// runtime/src/object/factory_file_extension.cpp — Wave 24 lane INTERNAL8
// ============================================================================
//
// ?GetTypeIdForFileExtension@Factory@Object@@QEBAIPEBD@Z   retail 0x1800c04c0
//
// The EIGHTH of this lane's eight bodies, and the one that was never an
// `internal` row at all. Wave 23 lane INTERNAL found that three of the census's
// four `ordinal` gap rows are PHANTOMS — ordinals 443, 3214 and 4462 resolve to
// names that already carry a `ported=1` export row — and that the fourth,
// ordinal 2990, is a REAL row misfiled out of `Object`. This is that row.
//
// Re-derived on this tip rather than quoted:
//
//   tools/runtime_census.py --csv w24.csv
//   awk -F, '$3=="ordinal" && $4=="0"' w24.csv     -> 2990, 3214, 443, 4462
//   standalone/resolve_standalone.cpp:886          -> 2990 is this mangled name
//
// It was reached by ORDINAL from two exe call sites
// (code/Game/Component/buildcomponent.cpp, residencecomponent.cpp) and by
// ADDRESS from nowhere. It IS an export, so it gets a NAME registration — the
// door that survives a rebase and the one the census counts — plus the address
// door, because the mint and the ordinal table both reach it.
//
// EXTENT: 0x1800c04c0..0x1800c0574, 181 bytes, two RET sites (0x1800c0567 for
// the not-found return and 0x1800c0574 for the found return). Bounded by
// Ghidra's function body, NOT by `.pdata` — see the extent note in
// runtime/src/save/saveload_saveinfo.cpp for why `.pdata` is the wrong
// instrument for this question in this binary.
//
// WHAT IT IS: a full IN-ORDER TRAVERSAL of Factory's AVL tree, visiting every
// registered type and `_stricmp`-ing the caller's extension against each
// BaseTypeInfo's `fileExtension` (+0x18). NOT a keyed lookup — the tree is
// keyed by type id, and the extension is not the key, so there is nothing to
// descend on. First match wins; it returns the NODE's key (`*(uint32_t*)node`,
// not `info->typeId`), which the profile build asserts are equal.
//
// A type whose `fileExtension` is null is SKIPPED, not compared: the
// `TEST RCX,RCX / JZ` at 0x1800c04fb jumps to the successor step. Every
// Component/Description registration in the exe passes nullptr there, so on a
// live tree the overwhelming majority of nodes take that arm — which is why
// the effectiveness counter below counts COMPARISONS, not visits.
// ============================================================================

#include <cstdint>

#include "../../types/object/TypeInfo.h"
#include "../../funcs/rt_libm.h"   // B3R: rtcrt::StrICmp (0x18007eefc), called by name

using RtObject::Factory;
using RtObject::TypeNode;

// ============================================================================
// unsigned int Object::Factory::GetTypeIdForFileExtension(const char* ext) const
// ----------------------------------------------------------------------------
// The traversal, transcribed from the disassembly rather than from Ghidra's C.
// That distinction earns its keep here: the decompiler renders the ascent as a
// single loop, and the shipped body has FOUR separate arms with one of them
// dead. They are reproduced with the dead arm marked rather than removed, so
// the reasoning is checkable against the listing.
//
//   0x1800c04ca  node = this->root            ; MOV RBX,[RCX]
//   0x1800c04d3  if (!node) return 0
//   0x1800c04e0  while (node->left)  node = node->left      ; leftmost
//   0x1800c04f1  if (!node) return 0          ; unreachable: node was non-null
//
//   visit (0x1800c04f3):
//   0x1800c04f3    info = node->value.info    ; [RBX+0x10]
//   0x1800c04f7    ext  = info->fileExtension ; [RAX+0x18]
//   0x1800c04fe    if (ext && _stricmp(ext, wanted) == 0) return *(u32*)node
//
//   successor:
//   0x1800c050c    if (node->right) {                       ; [RBX+0x40]
//   0x1800c0515        node = node->right;
//                      while (node->left) node = node->left;
//                  } else {
//   0x1800c0523        p = node->parent;                    ; [RBX+0x30]
//   0x1800c0527        if (!p)            node = node->parent;   // -> null, ends
//   0x1800c052c        else if (p->left == node) node = p;
//   0x1800c0537        else if (!p)       node = node->parent;   // *** DEAD ***
//   0x1800c0540        else { while (p && p->right == node) { node = p; p = p->parent; }
//                             node = node->parent; }
//                  }
//   0x1800c0556    if (node) goto visit
//   0x1800c055b  return 0
//
// THE DEAD ARM IS AT 0x1800c0537 and it is a re-test of `p` that 0x1800c0527
// has already proved non-null. It is transcribed as an assertion-shaped
// comment rather than as code, because writing an `if` that can never be true
// is not more faithful — it compiles to nothing and it invites a later reader
// to "fix" the surrounding logic to make it reachable.
//
// THE LAST ARM'S EXIT IS `node = node->parent`, NOT `node = p`. Those differ:
// the loop advances BOTH `node` and `p`, so on exit `node` is the last child
// walked and `node->parent` is its parent — the same value `p` holds only if
// the loop ran at least once and stopped on the `p->right != node` test. When
// the loop stops because `p` went null, `node->parent` is null too and the
// traversal ends. Transcribing `node = p` there would read an already-advanced
// pointer and is exactly the kind of algebraically-plausible substitution that
// hides where terms cancel.
// ============================================================================
extern "C" uint32_t RtObject_Factory_GetTypeIdForFileExtension(const Factory* self,
                                                               const char* wanted) {

    // EFFECTIVENESS: `visited` counts nodes reached, `compared` counts nodes
    // that actually reached _stricmp. Only the second is coverage — on a live
    // Factory almost every type registers a NULL fileExtension, so a site that
    // reported only `visited` would show hundreds of nodes per call while
    // exercising the comparison zero times. That is the `calls=N` illusion one
    // level down (reference_probe_needs_an_effectiveness_control.md).
    int visited = 0, compared = 0;
    uint32_t ours = 0;

    TypeNode* node = self->root;
    if (node != nullptr) {
        while (node->left != nullptr) node = node->left;   // leftmost

        while (node != nullptr) {
            ++visited;
            const RtObject::BaseTypeInfo* info = node->value.info;      // +0x10
            const char* ext = info->fileExtension;                      // +0x18
            if (ext != nullptr) {
                ++compared;
                if (rtcrt::StrICmp(ext, wanted) == 0) {   // 0x18007eefc
                    ours = node->key;   // *(u32*)node, NOT info->typeId
                    break;
                }
            }

            // ---- in-order successor ---------------------------------------
            if (node->right != nullptr) {
                node = node->right;
                while (node->left != nullptr) node = node->left;
            } else {
                TypeNode* p = node->parent;
                if (p == nullptr) {
                    node = node->parent;             // null; the traversal ends
                } else if (p->left == node) {
                    node = p;
                } else {
                    // 0x1800c0537's `TEST RAX,RAX / JZ` is DEAD here: p is
                    // non-null on this path. Not transcribed as an `if`.
                    while (p != nullptr && p->right == node) {
                        node = p;
                        p = p->parent;
                    }
                    node = node->parent;             // NOT `node = p` — see banner
                }
            }
        }
    }

    // ---- A REAL TWO-SIDED ORACLE, not a shadow ----------------------------
    // ROUTE TO THE SHIPPED BODY, stated because for an address-keyed row it is
    // the whole question: **the mangled NAME, through the raw DLL door, which is
    // GetProcAddress.** NOT ResolveAddr — that consults the port registry FIRST
    // and this file registers 0x1800c04c0, so it would compare the port with
    // ITSELF. Wave 24 lane DIFF's armed sweep counted 115 existing sites with
    // exactly that shape, none of which announced it, and one such probe was
    // dark for five waves.
    // This row is the only one of this lane's eight that gets one, and it gets
    // it because the function is CONST and SIDE-EFFECT-FREE: it walks a tree it
    // never writes and returns an integer, so the shipped body can be run on
    // the same inputs and the two answers compared. The other seven mutate
    // state and are shadow sites (self-checks) for that reason; this file does
    // not pretend the two kinds of site are worth the same.
    //
    // the raw DLL door resolves by MANGLED NAME through the raw DLL door, which is
    // GetProcAddress and therefore does NOT consult the port registry — so the
    // "theirs" side is the shipped bytes and not this body
    // (reference_rtoriginal_is_getprocaddress.md; and note it is inside the
    // `the diff oracle` guard, so a normal run never pays for it).
    using Fn = uint32_t (*)(const Factory*, const char*);

    // A SECOND, separate line for the effectiveness story: `calls=N` on the
    // site above says the body was ENTERED, and `worked=` says a _stricmp
    // happened. Neither says a MATCH happened, and a run in which every call
    // misses still exercises the whole traversal — which is the useful part.
    if (visited == 0 && self->root != nullptr) {
        // Structurally impossible (a non-empty tree always visits its leftmost
        // node). Reported rather than asserted, because a silent impossible is
        // how a dead loop reads as a clean one.
    }
    return ours;
}
