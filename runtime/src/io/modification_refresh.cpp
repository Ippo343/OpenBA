// ============================================================================
// runtime/src/io/modification_refresh.cpp — WAVE 9 LANE MOD (second pass)
// ?RefreshList@Modification@IO@@QEAAXXZ, 0x1800af5f0 — the live cutover wall
// ============================================================================
//
// WHY THIS FILE EXISTS
// --------------------
// Attempt 19 of the DLL-less boot did not merely *store* a null for this
// symbol, it CALLED one: `[RtResolve] MISS "?RefreshList@Modification@IO@@
// QEAAXXZ" -> NULL`, then a call through RAX=0 with RCX=RDX=the Modification
// object and a return address inside `Engine::Startup+0x338`. Of the 27
// symbols still unsatisfied on that boot path, this is the one the boot
// actually reached; everything else was resolved-and-stored.
//
// The first lane-MOD pass (docs/runtime_port/WAVE9_LANE_MOD.md §6) HELD THIS
// BACK, and the holdback was correct when written: RefreshList stack-builds an
// `IO::PackageFile` and calls `ReadPackfile` on it, and on 2026-09-07
// `??0PackageFile@IO@@QEAA@XZ` resolved to NULL — it was attempt 14's wall.
// Porting a body that cannot be exercised produces a `calls=0` green line over
// unverified code, which is the failure `reference_uncalled_port_is_not_
// verified.md` exists to prevent.
//
// THAT BLOCKER IS GONE, AND IT WAS RE-DERIVED HERE RATHER THAN INHERITED
// (`reference_holdback_blockers_are_stale_check_them_first.md`). Lane PKG2
// ported the constructor and the leaf accessors (runtime/src/io/
// package_file.cpp:838 `RegisterRuntimeExport("??0PackageFile@IO@@QEAA@XZ")`
// plus `RegisterRuntimeAddr(0x1800b4440)`), and lane PKG3 ported ReadPackfile
// itself (runtime/src/io/package_file_read_win.cpp:999-1001). Both doors, both
// rows. So RefreshList's PackageFile dependency is satisfied and this body can
// be both run and diffed.
//
// ---------------------------------------------------------------------------
// WHAT THIS FILE PORTS — eight rows
// ---------------------------------------------------------------------------
//
//   | retail      | symbol / role                              | instrs |
//   |-------------|--------------------------------------------|--------|
//   | 0x1800af5f0 | ?RefreshList@Modification@IO@@QEAAXXZ      |   934  |
//   | 0x180028610 | Array<Mod>::SetSize(int, const Mod&)       |   101  |
//   | 0x1800274c0 | Set<Filename> insert fix-up (AVL retrace)  |    71  |
//   | 0x180027f50 | Set<Filename> rotate-right                 |    43  |
//   | 0x180027e20 | Set<Filename> rotate-left                  |    43  |
//   | 0x180025530 | ?GetIconData@PackageFile@IO@@              |    20  |
//   | 0x180025580 | ?GetPreviewData@PackageFile@IO@@           |    20  |
//   | 0x18007f034 | the registry-string integer parse helper   |     3  |
//
// RefreshList is 934 instructions, not the "~450" the first pass's holdback
// note estimated from a decompile's line count. The difference is what MSVC
// inlined into this one body: the whole `Set<Filename>` insert, the same tree's
// LOOKUP and in-order walk in the override pass, three `Collection::Array`
// grow-and-copy loops, the `PackageFile` constructor, and eleven copies of the
// refcounted-String release idiom. Everything that reads like a helper call in
// retail's C is open-coded in the instruction stream.
//
// ---------------------------------------------------------------------------
// WHAT THIS FILE DECLINES, WITH THE MEASURED REASON  (the honest half)
// ---------------------------------------------------------------------------
// RefreshList's call closure is 22 distinct addresses. 14 were already
// registered before this lane started; this file closes 5 of the remaining 8
// (0x180028610, 0x1800274c0, 0x180025530, 0x180025580, 0x18007f034) plus the
// two rotations the fix-up needs and RefreshList itself. THREE REMAIN, each
// declined for a reason measured today rather than inherited:
//
//   0x1800b4540  ??1PackageFile@IO@@UEAA@XZ (ord 639) — 87 instructions. NOT
//     THIS LANE'S ROW: runtime/src/io/package_file.cpp:925 already names it as
//     lane PKG2's next wall, "(5 unported callees)". Writing it here would
//     duplicate a scoped row, and a duplicate `RegisterRuntimeAddr` is
//     last-binding-wins — it silently makes one of the two bodies dead code
//     (`reference_grep_for_the_port_before_writing_it.md`).
//
//   0x1800accc0  ?CloseFile@FileSystem@IO@@ (ord 1726) — 56 instructions, and
//     lane PKG2's own document claims it (`WAVE9_LANE_PKG2.md:944`, "the
//     teardown half"). It also OPENS a door rather than closing one: it calls
//     0x1800ae020, which nothing in the tree registers. A lane that closes one
//     door and opens one has not moved the count.
//
//   0x180028760  the Array<Mod> introsort — NO LONGER DECLINED. BATCH 8b
//     ported it and its small-range pass (0x180028fc0) in
//     runtime/src/io/modification_sort.cpp; the third door it named,
//     0x1800c98a0 (`String::StrCompare`), was already ported in
//     runtime/src/system/string.cpp. The objection recorded here — that the
//     population ties and so the SWAP SEQUENCE, not sortedness, decides which
//     record wins — was right, and is the reason that port TRANSCRIBES the
//     shipped MSVC sort skeleton (../collection/introsort_msvc.h) rather than
//     substituting one. What was wrong was the conclusion that a
//     replayed-permutation probe had to come first: a transcription of the
//     bytes needs no corpus to justify it.
//
// So with the DLL PRESENT this body is complete and diffable; at cutover it
// moves the wall from `RefreshList` to those three. The set-diff is in
// docs/runtime_port/WAVE9_LANE_MOD2.md and is honest about that.
//
// ---------------------------------------------------------------------------
// THE ORACLE — and why RefreshList is replayable when it does not look it
// ---------------------------------------------------------------------------
// RefreshList destroys and rebuilds `this->mods` from scratch: it frees every
// record, re-scans the pack directory, re-reads every .pkm header and re-reads
// the registry. Nothing about it can be restored-and-replayed byte-wise, and a
// byte compare of the result is meaningless because the two runs allocate
// different heap blocks.
//
// It IS replayable in the sense that matters, for a reason visible in its first
// instruction: `CALL 0x180010b80` destroys the existing array UNCONDITIONALLY,
// before anything else. The function is therefore idempotent over the pair
// (pack directory, registry) — which is exactly what the game relies on when
// the mods dialog's Ok calls it a second time. So:
//
//     run OURS  -> photograph the resulting list BY VALUE
//     run THEIRS on the same object (it re-destroys and rebuilds)
//     photograph again -> compare the two photographs
//
// Retail's run is the one that LANDS, the same property SaveList's oracle has:
// a diffing run's live behaviour is retail's behaviour.
//
// The photograph is BY VALUE, never by pointer, because every String and every
// Set node in the second run is a different heap block:
//
//     count, and per mod: strcmp(name), strcmp(author), strcmp(description),
//     strcmp(file), enabled, order, version, userVersion, iconWidth,
//     iconHeight, crc, icon.size, preview.size, files.size, set.count,
//     the SET'S IN-ORDER KEY SEQUENCE (strcmp, node by node), and
//     the FILES ARRAY's contents (strcmp, element by element).
//
// The in-order key sequence is the term that makes this non-tautological for
// the part of the body hardest to get right. The set is an AVL tree built by an
// inlined insert plus a retrace (0x1800274c0) plus two rotations, and a
// rebalance bug does not change the SET of keys — it changes the tree.
// Comparing the in-order walk catches a tree that lost, duplicated or
// misordered a node; comparing `set.count` alongside catches a count that
// drifted from the tree. WHAT IT CANNOT CATCH is a wrong rotation that still
// leaves a valid BST over the same keys with the same count — a differently
// SHAPED but equally correct tree. That is stated rather than papered over.
//
// EFFECTIVENESS CONTROL. `the diff oracle` records how many calls actually built
// something (count > 0). A run whose pack directory holds no .pkm produces
// `divergences=0` over an empty list — a green line meaning "never ran", which
// is precisely `reference_uniform_population_is_not_a_verification.md`. The
// measurement section of WAVE9_LANE_MOD2.md quotes the work count, and a zero
// there invalidates the run rather than being explained away.
//
// NEGATIVE CONTROL. `the old DLL-era switch=<n>` perturbs OUR photograph after
// it is taken and before the compare — 1 flips mods[0].enabled, 2 flips a
// bit in the LAST KEY of the first set's in-order walk, 3 bumps
// mods[0].order, 4 drops a key from that walk. Crucially 2 is what reaches
// `set.inorder`, the term the AVL port rests on: the first draft of this
// probe had only the DROP case, which fires on the length check before the
// key walk ever runs, so the tree-shape term had no control at all. Each must
// make the shadow FAIL and name the term it broke. A control that cannot fire
// is worse than none (`reference_probe_needs_an_effectiveness_control.md`).
//
// ---------------------------------------------------------------------------
// THE DELIBERATE DEVIATIONS FROM THE INSTRUCTION STREAM
// ---------------------------------------------------------------------------
// (1) Retail INLINES the `PackageFile` constructor into RefreshList
// (0x1800af830..0x1800af8e6). This file CALLS the exported constructor at
// 0x1800b4440, which lane PKG2 ported. That is safe because the two instruction
// streams are THE SAME STORES IN THE SAME ORDER — checked store by store. Both
// write the two throw-away base vtables (0x180154788 then 0x1801547a0), zero
// +0x08/+0x10/+0x18, store 0x180155700 at +0x00, zero +0x20..+0x68, put the
// qword 0x2710 at +0x70 (priority 10000 AND platform 0), zero +0x80/+0x88/
// +0x98/+0xa0, dword-zero +0xa8, byte-one +0xac, zero +0xb0/+0xb8, dword -1 at
// +0xc0, zero +0xc8, dword-zero +0xd0, zero +0xe0/+0xe8, and qword -1 at +0xd8.
// NEITHER writes +0x78/+0x7c (the icon dimensions, which package_file.cpp
// already documents as left to ReadPackfile) NOR +0x90. Writing the store list
// out a second time would be a second place for it to drift.
//
// (2) The allocator, exactly as modification.cpp's banner sets out: retail
// inlines `MOV RBX,[0x1801fc718]; ADD RBX,0x640; EnterCriticalSection; …;
// LeaveCriticalSection` at every allocation and free here, and the composed
// statics 0x1800c7a50 / 0x1800c7ad0 are byte-for-byte that sequence. Same lock,
// same singleton, same order; only the inlining differs.
//
//
// NO NEW WIN32 API AND NO NEW IMPORT LIBRARY.
// ============================================================================

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/io/Modification.h"
#include "../../types/system/Filename.h"

namespace RtSystem {
Filename* RtSystem_Filename_CtorCopy(Filename* self, const Filename* other);   // 0x1800067d0
void      RtSystem_Filename_AdjustDelimiters(Filename* self);                  // 0x1800c6970
}  // namespace RtSystem

// Byte-identical to standalone/resolve.h; see modification.cpp's note on why
// resolve.h is not included directly (windows.h drags in `Rectangle`/`near`).
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace RtIO {

using RtSystem::Filename;
using RtSystem::RtSystem_Filename_CtorCopy;
using RtSystem::RtSystem_Filename_AdjustDelimiters;

// ---------------------------------------------------------------------------
// Borrowed from modification.cpp (same lane, same kill switch). Declared rather
// than re-implemented: two copies of a refcount idiom is how one of them
// drifts — the near-miss modification.cpp §4 records about RtString_AdoptInline.
// ---------------------------------------------------------------------------
void* ModMemAlloc(uint64_t bytes, int align);
void  ModMemFree(void* p);
void  AssignShared(char** dst, char* src);
void* RtColl_ArrayModDtor_180010b80(void* av);                          // 0x180010b80
void* RtColl_ArrayModAssign_180010110(void* dstv, const void* srcv);    // 0x180010110
void* RtColl_ArrayI32Assign_180011110(void* dstv, const void* srcv);    // 0x180011110
void* RtColl_ArrayU8Assign_180011220(void* dstv, const void* srcv);     // 0x180011220
void* RtIo_Mod_CopyCtor_1800101e0(void* dstv, const void* srcv);        // 0x1800101e0
void  RtIo_Mod_Dtor_180011460(void* selfv);                             // 0x180011460

static inline int32_t& RefCount(char* s) {
    return *reinterpret_cast<int32_t*>(s - 4);
}

// The release retail inlines eleven times in RefreshList, e.g.
// 0x1800af6bb..0x1800af6eb:
//     if (p) { if (--p[-1 dword] == 0) { slot = p - 8; MemFree(slot); }
//              else                    { slot = 0; } }
// The ELSE branch's store is not a tidy-up — it is what the field is left
// holding when the block survives, and it is why a caller that keeps a
// reference must take that reference BEFORE the release (see `nameRef`).
static void ReleaseInto(char** slot) {
    char* p = *slot;
    if (p == nullptr) return;
    if (--RefCount(p) == 0) {
        *slot = p - 8;
        ModMemFree(*slot);          // the null test lives inside 0x1800c7ad0
    } else {
        *slot = nullptr;
    }
}

// ===========================================================================
// PART 1 — the Set<Filename> AVL internals
//   0x180027f50 rotate-right, 0x180027e20 rotate-left, 0x1800274c0 the retrace
// ===========================================================================
//
// The node is `ModSetNode` (Modification.h): key +0x00, parent +0x08, left
// +0x10, right +0x18, and TWO int16 at +0x20 / +0x22. The pair is not a guess —
// all three bodies below compute them with the same five instructions:
//
//     lh = left  ? left->[+0x22]  + 1 : 0      (MOVZX ; INC r16)
//     rh = right ? right->[+0x22] + 1 : 0
//     [+0x22] = max(lh, rh)                    (CMP ; MOVZX ; CMOVG — SIGNED)
//     [+0x20] = rh - lh                        (SUB r16, r16)
//
// so +0x22 is a HEIGHT (a leaf is 0) and +0x20 is a BALANCE FACTOR, signed,
// right-heavy positive. The retrace's tests read them signed (`CMP CX,1 / JLE`,
// `CMP word ptr [RDX+0x20],-1 / JGE`, `TEST CX,CX / JS`) — an independent
// second pin on the signedness.
//
// The increments and the subtraction are 16-BIT and retail lets them wrap
// (`INC R8W`, `SUB CX,R8W`). Reproduced by computing in uint16_t and
// reinterpreting, so a pathological tree wraps where retail wraps.

struct AvlNode {
    char*    key;       // +0x00 — Filename::data
    AvlNode* parent;    // +0x08
    AvlNode* left;      // +0x10
    AvlNode* right;     // +0x18
    int16_t  balance;   // +0x20
    int16_t  height;    // +0x22
    uint32_t _pad24;    // +0x24
};
static_assert(sizeof(AvlNode) == sizeof(ModSetNode), "AvlNode is a view of ModSetNode");
static_assert(offsetof(AvlNode, parent)  == 0x08, "0x180027f81 MOV [R8+8],RAX");
static_assert(offsetof(AvlNode, left)    == 0x10, "0x180027f55 MOV RAX,[RDX+0x10]");
static_assert(offsetof(AvlNode, right)   == 0x18, "0x180027e25 MOV RAX,[RDX+0x18]");
static_assert(offsetof(AvlNode, balance) == 0x20, "0x180027529 MOV [RDX+0x20],CX");
static_assert(offsetof(AvlNode, height)  == 0x22, "0x180027525 MOV [RDX+0x22],AX");

static inline int16_t ChildHeight(const AvlNode* c) {
    if (c == nullptr) return 0;                                  // XOR r,r
    return static_cast<int16_t>(static_cast<uint16_t>(
        static_cast<uint16_t>(c->height) + 1u));                 // MOVZX ; INC r16
}

static void AvlUpdate(AvlNode* n) {
    const int16_t lh = ChildHeight(n->left);
    const int16_t rh = ChildHeight(n->right);
    n->height  = (lh > rh) ? lh : rh;                            // CMOVG, signed
    n->balance = static_cast<int16_t>(static_cast<uint16_t>(
        static_cast<uint16_t>(rh) - static_cast<uint16_t>(lh))); // SUB r16, r16
}

// 0x180027f50 — rotate RIGHT: the LEFT child (+0x10) is promoted.
// `CMP RDX,[RCX]` at entry is the root fix-up: RCX is `&set.root`, so a rotation
// at the root retargets the set's own root pointer. That is the whole reason
// the retrace is handed `&set.root` rather than the set.
static void* AvlRotateRight(AvlNode** root, AvlNode* n) {
    if (n == *root) *root = n->left;
    AvlNode* const up = n->parent;
    if (up != nullptr) {
        AvlNode* const l = n->left;
        if (up->left == n) up->left = l; else up->right = l;
    }
    AvlNode* const pivot = n->left;
    pivot->parent = n->parent;
    n->left = pivot->right;
    if (n->left != nullptr) n->left->parent = n;
    pivot->right = n;
    n->parent = pivot;
    // Retail recomputes the pivot's two children and then the pivot, in that
    // order, each guarded on non-null. `pivot->right` is `n`, so it can never
    // be null here; the guard is transcribed anyway because retail emits it.
    if (pivot->left  != nullptr) AvlUpdate(pivot->left);
    if (pivot->right != nullptr) AvlUpdate(pivot->right);
    AvlUpdate(pivot);
    return pivot;                                                // MOV RAX,R8
}

// 0x180027e20 — rotate LEFT: the RIGHT child (+0x18) is promoted. The mirror of
// the above; the only differences in the instruction stream are +0x10 <-> +0x18
// in the three places that pick the pivot and re-hang it.
static void* AvlRotateLeft(AvlNode** root, AvlNode* n) {
    if (n == *root) *root = n->right;
    AvlNode* const up = n->parent;
    if (up != nullptr) {
        AvlNode* const r = n->right;
        if (up->left == n) up->left = r; else up->right = r;
    }
    AvlNode* const pivot = n->right;
    pivot->parent = n->parent;
    n->right = pivot->left;
    if (n->right != nullptr) n->right->parent = n;
    pivot->left = n;
    n->parent = pivot;
    if (pivot->left  != nullptr) AvlUpdate(pivot->left);
    if (pivot->right != nullptr) AvlUpdate(pivot->right);
    AvlUpdate(pivot);
    return pivot;
}

// 0x1800274c0 — the retrace. `cursor` is a stack slot holding the node to start
// from, and the loop OVERWRITES it with each node's parent, so on return it is
// null. The parent is read BEFORE the rotations (0x1800274e6 `MOV RSI,[RAX+8]`),
// which is load-bearing: a rotation at `n` changes `n->parent`, and retracing
// from the NEW parent would revisit the subtree it just fixed.
//
// Both rebalance arms are guarded on the balance factor AND the child being
// non-null, in that order, and the second arm RE-READS `*cursor` because the
// first arm may have rotated that node out from under it.
extern "C" void RtColl_SetInsertFixup_1800274c0(void* rootv, void* cursorv) {
    AvlNode** const root   = static_cast<AvlNode**>(rootv);
    AvlNode** const cursor = static_cast<AvlNode**>(cursorv);
    if (*cursor == nullptr) return;                    // CMP qword [RDX],0 ; JZ
    do {
        AvlNode* n = *cursor;
        AvlNode* const up = n->parent;                 // captured first — see above
        AvlUpdate(n);

        if (n->balance > 1) {                          // CMP CX,1 ; JLE
            AvlNode* const c = n->right;               // MOV RAX,[RDX+0x18] ; JZ
            if (c != nullptr) {
                if (c->balance < 0) {                  // TEST CX,CX ; JG ; JNS
                    AvlRotateRight(root, c);
                    n = *cursor;                       // MOV RDX,[RDI]
                }
                AvlRotateLeft(root, n);
            }
        }

        n = *cursor;                                   // MOV RDX,[RDI]
        if (n->balance < -1) {                         // CMP word [RDX+0x20],-1 ; JGE
            AvlNode* const c = n->left;                // MOV RAX,[RDX+0x10] ; JZ
            if (c != nullptr) {
                if (c->balance > 0) {                  // TEST CX,CX ; JS ; JLE
                    AvlRotateLeft(root, c);
                    n = *cursor;
                }
                AvlRotateRight(root, n);
            }
        }

        *cursor = up;                                  // MOV [RDI],RSI
    } while (*cursor != nullptr);                      // TEST RSI,RSI ; JNZ
}

// ===========================================================================
// PART 2 — 0x180028610, Collection::Array<Mod>::SetSize(int n, const Mod& fill)
// ===========================================================================
//
// The growth rule is the one modification.cpp's ArrayAssignGrow documents
// (capacity tripled only on the exact push-one-element case), with stride 0x80
// and align 8. What is specific to this body is WHERE each new element's
// contents come from:
//
//   start = (oldData == newData) ? oldSize : 0        // CMP RBP,[R15] ; CMOVZ
//   for (i = start; i < n; ++i)
//       Mod::Mod(&new[i], i < oldSize ? &old[i] : &fill);
//
// i.e. on a REALLOCATION every surviving element is copy-constructed out of the
// old buffer and the tail is copy-constructed from `fill`; in place, only the
// tail is touched. Then the old elements are destroyed — from the old buffer if
// it was replaced, or from the truncated tail if it was not.
//
// `RefreshList` uses it exactly once, as `SetSize(size + 1, stackMod)`, which is
// this container's only append idiom.
//
// The `ADD RCX,RDI ; JZ` at 0x180028695 tests the ELEMENT address, not the
// buffer, and is retail's only protection against a failed MemAlloc.
// Transcribed, not hoisted — modification.cpp §4.
extern "C" void RtColl_ArrayModSetSizeFill_180028610(void* av, int n, const void* fillv) {
    ModArray* const a = static_cast<ModArray*>(av);
    const Mod* const fill = static_cast<const Mod*>(fillv);

    const int32_t oldSize = a->size;                    // MOV ESI,[RCX+8]
    if (n == oldSize) return;                           // CMP R14D,ESI ; JZ

    uint8_t* const oldData = static_cast<uint8_t*>(a->data);   // MOV RBP,[RCX]
    a->size = n;
    if (n > a->capacity) {
        a->capacity = n;
        if (n == oldSize + 1 && oldSize != 0)
            a->capacity = n + n * 2;                    // LEA EAX,[R14+R14*2]
        a->data = ModMemAlloc(
            static_cast<uint64_t>(static_cast<int64_t>(a->capacity)) << 7, 8);
    }

    uint8_t* const newData = static_cast<uint8_t*>(a->data);
    int32_t i = (oldData == newData) ? oldSize : 0;
    for (; i < n; ++i) {
        Mod* const dst = reinterpret_cast<Mod*>(newData + static_cast<size_t>(i) * 0x80);
        if (dst == nullptr) continue;                   // ADD RCX,RDI ; JZ
        const Mod* const src = (i < oldSize)
            ? reinterpret_cast<const Mod*>(oldData + static_cast<size_t>(i) * 0x80)
            : fill;                                     // CMP EBX,ESI ; JL ; MOV RDX,R12
        RtIo_Mod_CopyCtor_1800101e0(dst, src);          // CALL 0x1800101e0
    }

    if (oldData != newData) {                           // CMP RBP,[R15] ; JZ
        for (int32_t k = 0; k < oldSize; ++k)           // TEST ESI,ESI ; JLE
            RtIo_Mod_Dtor_180011460(oldData + static_cast<size_t>(k) * 0x80);
        ModMemFree(oldData);                            // retail's TEST RBP,RBP is inside
    } else if (oldSize > n) {                           // CMP ESI,R14D ; JLE
        const int32_t drop = oldSize - n;
        for (int32_t k = 0; k < drop; ++k)
            RtIo_Mod_Dtor_180011460(newData + (static_cast<size_t>(n) + k) * 0x80);
    }
}

// ===========================================================================
// PART 3 — 0x180025530 GetIconData / 0x180025580 GetPreviewData
// ===========================================================================
// Both are the same twenty instructions: zero the caller's 16-byte return slot,
// then, unless the slot IS the member being returned, run the member's
// Array::operator= into it. The zeroing is what makes the assign safe — the
// assign begins by freeing `dst->data`, and the slot is raw stack.
//
// The self-compare (`LEA RDX,[RCX+0x80]` / `CMP RBX,RDX`) can only be true if a
// caller passes the member's own address as the return slot. Transcribed, not
// dropped: it IS the guard, and neither Array::operator= carries one of its own
// (only the Filename instantiation does — modification.cpp).
//
// The element types come from the export names, not from the call site:
// `?$Array@I` (unsigned int) for the icon, `?$Array@E` (unsigned char) for the
// preview — which is why the two call different assigns.
extern "C" void* RtIo_PackageFile_GetIconData(const void* selfv, void* retv) {
    ModArray* const ret = static_cast<ModArray*>(retv);
    const uint8_t* const self = static_cast<const uint8_t*>(selfv);
    const void* const member = self + 0x80;             // LEA RDX,[RCX+0x80]
    ret->data = nullptr;                                // MOV [RBX],RAX
    ret->size = 0;                                      // MOV [RBX+8],RAX — one qword,
    ret->capacity = 0;                                  //   so capacity is zeroed too
    if (ret != member) RtColl_ArrayI32Assign_180011110(ret, member);   // 0x180011110
    return ret;
}

extern "C" void* RtIo_PackageFile_GetPreviewData(const void* selfv, void* retv) {
    ModArray* const ret = static_cast<ModArray*>(retv);
    const uint8_t* const self = static_cast<const uint8_t*>(selfv);
    const void* const member = self + 0x98;             // LEA RDX,[RCX+0x98]
    ret->data = nullptr;
    ret->size = 0;
    ret->capacity = 0;
    if (ret != member) RtColl_ArrayU8Assign_180011220(ret, member);    // 0x180011220
    return ret;
}

// ===========================================================================
// PART 4 — 0x18007f034, the registry-string integer parse helper
// ===========================================================================
//     XOR EDX,EDX ; LEA R8D,[RDX+0xa] ; JMP 0x180083e18
// a bare tail-jump to CRT `strtol(s, NULL, 10)`. That identification is not new
// here: WAVE4_LANE_C.md:167 and runtime/src/io/persistence.cpp:127 both record
// it, and keymap_load.cpp:270 and video/device.cpp:773 already call std::strtol
// for the same address. This row exists to give the ADDRESS a door — the three
// existing sites call std::strtol inline, so nothing had registered
// 0x18007f034 and a by-address caller (this one) would find null at cutover.
extern "C" int RtCrt_ParseInt10_18007f034(const char* s) {
    return static_cast<int>(std::strtol(s, nullptr, 10));
}

// ===========================================================================
// PART 5 — 0x1800af5f0, ?RefreshList@Modification@IO@@QEAAXXZ
// ===========================================================================

// Cross-file callees, BY ADDRESS. `ResolveAddr(Module::Runtime, …)` consults
// the PORT REGISTRY FIRST (reference_resolveaddr_consults_port_registry_first),
// so each reaches the owning lane's port where there is one and the DLL only
// where there is not — which is exactly the dependency graph retail has, since
// every one of these is a CALL in the instruction stream.
//
// Never-cache-a-null, per attempt 14: a caller that runs before the owning
// lane's registration must not freeze a null in for the life of the process.
//
// EACH OF THE SEVENTEEN IS SPELLED OUT WITH ITS LITERAL AT THE CALL SITE, and
// that verbosity is deliberate. The first draft of this file routed all
// seventeen through a two-line template, `RtAddr(cache, 0x1800b4540)`, which
// reads better and is INVISIBLE to tools/addr_door_audit.py -- the audit
// matches the literal text `ResolveAddr(Module::Runtime, 0x`, so seventeen new
// by-address doors moved its `by-address call sites` column by ZERO and its
// `distinct addresses` column by ZERO. Three of those doors are UNREGISTERED
// (the declined rows in the banner), and the instrument whose entire job is to
// find unregistered by-address doors could not see them.
// reference_literal_grep_blind_to_variables.md is the fourth recorded instance
// of exactly this; the tool even prints its own blind-spot count, and the
// wrapper form pushed TWO more sites into it. A door the census cannot see is
// worse than a verbose one.

using FnStringSet        = void*    (*)(void*, const char*);            // 0x1800c95d0
using FnStringFormat     = void*    (*)(void*, const char*, ...);       // 0x1800c9730
using FnGetPackDirectory = Filename*(*)(const void*, Filename*);        // 0x180007c10
using FnFindFiles        = void     (*)(const void*, const void*, const void*,
                                        bool, void*);                   // 0x1800ad7a0
using FnPkgCtor          = void*    (*)(void*);                         // 0x1800b4440
using FnPkgDtor          = void*    (*)(void*);                         // 0x1800b4540 (declined)
using FnReadPackfile     = bool     (*)(void*, const Filename*);        // 0x1800b46b0
using FnPkgGetString     = void*    (*)(const void*, void*);            // 0x1800253xx
using FnPkgGetFileName   = Filename*(*)(const void*, Filename*, int);   // 0x180025450
using FnCloseFile        = void     (*)(void*, void*);                  // 0x1800accc0 (declined)
using FnMapFind          = void*    (*)(void*, const void*);            // 0x18002ace0
using FnGetPathFilename  = Filename*(*)(const Filename*, Filename*, bool);  // 0x1800c6740
using FnStrICmp          = int      (*)(const char*, const char*);      // 0x18007eefc
using FnArrayFilenameFill= void     (*)(void*, int, const Filename*);   // 0x180026880
using FnModSort          = void     (*)(void*, int, void*);             // 0x180028760 (PORTED, BATCH 8b)

// The `Mod` int columns, named from the PackageFile fields RefreshList copies
// them out of (0x1800afaba..0x1800afade and 0x1800afc8e). Modification.h calls
// them i18/i1c/i24/i28/i2c because the lifecycle triple that pinned the layout
// only ever COPIES them; this is the body that gives them meaning, so the
// mapping is recorded here rather than renaming a header three files include:
//
//     Mod::i18 <- PackageFile +0x68   ?GetVersion@      (0x1800253d0)
//     Mod::i1c <- PackageFile +0x6c   ?GetUserVersion@  (0x1800253e0)
//     Mod::i24 <- PackageFile +0x78   ?GetIconWidth@    (0x180025510)
//     Mod::i28 <- PackageFile +0x7c   ?GetIconHeight@   (0x180025520)
//     Mod::i2c <- PackageFile +0xa8   ?GetCRC@          (0x180025500)
//     Mod::order starts at 0 and is overwritten from "ModIndex_<stem>".

// ---- the photograph, for the oracle ---------------------------------------
struct ModShot {
    char*   name;
    char*   author;
    char*   description;
    char*   file;
    int32_t enabled, order, version, userVersion, iconW, iconH, crc;
    int32_t intsSize, bytesSize, filesSize, setCount;
    // Owned copies of the by-value key sequences. The second run frees the
    // originals, so a shot that only POINTED at them would compare freed memory
    // (reference_oracle_must_snapshot_aliased_inputs.md).
    char**  setKeys;    int32_t setKeyCount;
    char**  fileKeys;   int32_t fileKeyCount;
};

static char* ShotDup(const char* s) {
    if (s == nullptr) return nullptr;
    const size_t n = std::strlen(s) + 1;
    char* p = static_cast<char*>(ModMemAlloc(n, 1));
    if (p != nullptr) std::memcpy(p, s, n);
    return p;
}

static void ShotWalkSet(const AvlNode* n, char** out, int32_t& k, int32_t cap) {
    if (n == nullptr) return;
    ShotWalkSet(n->left, out, k, cap);
    if (k < cap) out[k++] = ShotDup(n->key);
    ShotWalkSet(n->right, out, k, cap);
}

// One photograph of the whole list, by value. Returns the record count.
static int32_t TakeShot(const Modification* self, ModShot** outShots) {
    const int32_t n = self->count;
    *outShots = nullptr;
    if (n <= 0) return n;
    ModShot* s = static_cast<ModShot*>(
        ModMemAlloc(static_cast<uint64_t>(n) * sizeof(ModShot), 8));
    if (s == nullptr) return 0;
    std::memset(s, 0, static_cast<size_t>(n) * sizeof(ModShot));
    for (int32_t i = 0; i < n; ++i) {
        const Mod* m = self->mods + i;
        s[i].name        = ShotDup(m->name);
        s[i].author      = ShotDup(m->author);
        s[i].description = ShotDup(m->description);
        s[i].file        = ShotDup(m->file.data);
        s[i].enabled     = m->enabled;
        s[i].order       = m->order;
        s[i].version     = m->i18;
        s[i].userVersion = m->i1c;
        s[i].iconW       = m->i24;
        s[i].iconH       = m->i28;
        s[i].crc         = m->i2c;
        s[i].intsSize    = m->ints.size;
        s[i].bytesSize   = m->bytes.size;
        s[i].filesSize   = m->files.size;
        s[i].setCount    = m->set.count;

        // The in-order key walk, sized from set.count and CAPPED by it: if the
        // tree holds more nodes than the count says, the shot records the first
        // `count` and the `set.count` / walk-length terms are what fail. That
        // asymmetry is deliberate — a tree/count disagreement must not be able
        // to corrupt the probe's own memory.
        const int32_t cap = m->set.count > 0 ? m->set.count : 0;
        if (cap > 0) {
            s[i].setKeys = static_cast<char**>(
                ModMemAlloc(static_cast<uint64_t>(cap) * 8, 8));
            if (s[i].setKeys != nullptr) {
                std::memset(s[i].setKeys, 0, static_cast<size_t>(cap) * 8);
                ShotWalkSet(reinterpret_cast<const AvlNode*>(m->set.root),
                            s[i].setKeys, s[i].setKeyCount, cap);
            }
        }
        const int32_t fc = m->files.size > 0 ? m->files.size : 0;
        if (fc > 0) {
            s[i].fileKeys = static_cast<char**>(
                ModMemAlloc(static_cast<uint64_t>(fc) * 8, 8));
            if (s[i].fileKeys != nullptr) {
                const Filename* fn = static_cast<const Filename*>(m->files.data);
                for (int32_t k = 0; k < fc; ++k) s[i].fileKeys[k] = ShotDup(fn[k].data);
                s[i].fileKeyCount = fc;
            }
        }
    }
    *outShots = s;
    return n;
}

static void FreeShot(ModShot* s, int32_t n) {
    if (s == nullptr) return;
    for (int32_t i = 0; i < n; ++i) {
        ModMemFree(s[i].name);
        ModMemFree(s[i].author);
        ModMemFree(s[i].description);
        ModMemFree(s[i].file);
        for (int32_t k = 0; k < s[i].setKeyCount;  ++k) ModMemFree(s[i].setKeys[k]);
        for (int32_t k = 0; k < s[i].fileKeyCount; ++k) ModMemFree(s[i].fileKeys[k]);
        ModMemFree(s[i].setKeys);
        ModMemFree(s[i].fileKeys);
    }
    ModMemFree(s);
}

static bool ShotStrEq(const char* a, const char* b) {
    if (a == nullptr || b == nullptr) return a == b;
    return std::strcmp(a, b) == 0;
}

// Names the failing TERM, per
// reference_shadow_predicate_must_name_its_failing_term.md — the verdict and
// the message are computed by the SAME walk, so they cannot disagree.
static const char* ShotCompare(const ModShot* a, int32_t na,
                               const ModShot* b, int32_t nb, int32_t* badIndex) {
    *badIndex = -1;
    if (na != nb) return "count";
    for (int32_t i = 0; i < na; ++i) {
        *badIndex = i;
        if (!ShotStrEq(a[i].name, b[i].name))               return "name";
        if (!ShotStrEq(a[i].author, b[i].author))           return "author";
        if (!ShotStrEq(a[i].description, b[i].description)) return "description";
        if (!ShotStrEq(a[i].file, b[i].file))               return "file";
        if (a[i].enabled     != b[i].enabled)     return "enabled";
        if (a[i].order       != b[i].order)       return "order";
        if (a[i].version     != b[i].version)     return "version";
        if (a[i].userVersion != b[i].userVersion) return "userVersion";
        if (a[i].iconW       != b[i].iconW)       return "iconWidth";
        if (a[i].iconH       != b[i].iconH)       return "iconHeight";
        if (a[i].crc         != b[i].crc)         return "crc";
        if (a[i].intsSize    != b[i].intsSize)    return "icon.size";
        if (a[i].bytesSize   != b[i].bytesSize)   return "preview.size";
        if (a[i].filesSize   != b[i].filesSize)   return "files.size";
        if (a[i].setCount    != b[i].setCount)    return "set.count";
        if (a[i].setKeyCount != b[i].setKeyCount) return "set.walkLength";
        for (int32_t k = 0; k < a[i].setKeyCount; ++k)
            if (!ShotStrEq(a[i].setKeys[k], b[i].setKeys[k])) return "set.inorder";
        if (a[i].fileKeyCount != b[i].fileKeyCount) return "files.length";
        for (int32_t k = 0; k < a[i].fileKeyCount; ++k)
            if (!ShotStrEq(a[i].fileKeys[k], b[i].fileKeys[k])) return "files.element";
    }
    *badIndex = -1;
    return nullptr;
}

// ---- the body -------------------------------------------------------------
extern "C" void RtIo_Modification_RefreshList(void* selfv) {
    Modification* const self = static_cast<Modification*>(selfv);

    // `this + 8` is the Array<Mod> header {mods, count, capacity}. Retail holds
    // it in RDI for the whole body and spills it at [RBP+0x208].
    ModArray* const arr = reinterpret_cast<ModArray*>(&self->mods);

    // 0x1800af62c — UNCONDITIONAL. This is what makes the function idempotent
    // and therefore diffable; see the banner.
    RtColl_ArrayModDtor_180010b80(arr);

    // 0x1800af631: `CMP byte ptr [RBX+1],0 ; JNZ main`. With saving disabled the
    // list is restored from the backup EnableSavingSettings(false) made and the
    // function returns — no disk scan, no registry read.
    if (self->savingSettings == 0) {
        RtColl_ArrayModAssign_180010110(arr, &self->savedMods);   // 0x180010110
        return;
    }

    static FnStringSet         fnStringSet        = nullptr;
    static FnStringFormat      fnStringFormat     = nullptr;
    static FnGetPackDirectory  fnGetPackDirectory = nullptr;
    static FnFindFiles         fnFindFiles        = nullptr;
    static FnPkgCtor           fnPkgCtor          = nullptr;
    static FnPkgDtor           fnPkgDtor          = nullptr;
    static FnReadPackfile      fnReadPackfile     = nullptr;
    static FnPkgGetString      fnGetName          = nullptr;
    static FnPkgGetString      fnGetAuthor        = nullptr;
    static FnPkgGetString      fnGetDescription   = nullptr;
    static FnPkgGetFileName    fnGetFileName      = nullptr;
    static FnCloseFile         fnCloseFile        = nullptr;
    static FnMapFind           fnMapFind          = nullptr;
    static FnGetPathFilename   fnGetPathFilename  = nullptr;
    static FnStrICmp           fnStrICmp          = nullptr;
    static FnArrayFilenameFill fnFilesFill        = nullptr;
    static FnModSort           fnModSort          = nullptr;

    // The two singletons retail loads at 0x1800af648 / 0x1800af64f. Read through
    // the exported data slots rather than the raw addresses so a standalone run
    // sees the objects Engine::Startup actually constructed.
    void** const fsSlot = RT_DATA_SLOT(void**, "?_singleton@FileSystem@IO@@0PEAV12@EA");
    void** const psSlot = RT_DATA_SLOT(void**, "?_singleton@Persistence@IO@@0PEAV12@EA");
    void* const fs   = fsSlot ? *fsSlot : nullptr;
    void* const pers = psSlot ? *psSlot : nullptr;

    // 0x1800af65f..0x1800af669 — three zeroed slots. The pack-directory Filename
    // at [RBP+0x200] is NOT among them: GetPackDirectory constructs it.
    ModArray found = { nullptr, 0, 0 };
    Filename pattern; pattern.data = nullptr;

    if (fnStringSet == nullptr) fnStringSet = reinterpret_cast<decltype(fnStringSet)>(
        ResolveAddr(Module::Runtime, 0x1800c95d0));
    fnStringSet(&pattern, "*.pkm");                              // 0x1801557f0

    Filename packDir;
    if (fnGetPackDirectory == nullptr) fnGetPackDirectory = reinterpret_cast<decltype(fnGetPackDirectory)>(
        ResolveAddr(Module::Runtime, 0x180007c10));
    fnGetPackDirectory(fs, &packDir);

    if (fnFindFiles == nullptr) fnFindFiles = reinterpret_cast<decltype(fnFindFiles)>(
        ResolveAddr(Module::Runtime, 0x1800ad7a0));
    fnFindFiles(fs, &packDir, &pattern, false, &found);

    ReleaseInto(&packDir.data);                                  // 0x1800af6b4
    ReleaseInto(&pattern.data);                                  // 0x1800af6ef

    if (fnPkgCtor == nullptr) fnPkgCtor = reinterpret_cast<decltype(fnPkgCtor)>(
        ResolveAddr(Module::Runtime, 0x1800b4440));
    if (fnPkgDtor == nullptr) fnPkgDtor = reinterpret_cast<decltype(fnPkgDtor)>(
        ResolveAddr(Module::Runtime, 0x1800b4540));
    if (fnReadPackfile == nullptr) fnReadPackfile = reinterpret_cast<decltype(fnReadPackfile)>(
        ResolveAddr(Module::Runtime, 0x1800b46b0));
    if (fnGetName == nullptr) fnGetName = reinterpret_cast<decltype(fnGetName)>(
        ResolveAddr(Module::Runtime, 0x180025310));
    if (fnGetAuthor == nullptr) fnGetAuthor = reinterpret_cast<decltype(fnGetAuthor)>(
        ResolveAddr(Module::Runtime, 0x180025350));
    if (fnGetDescription == nullptr) fnGetDescription = reinterpret_cast<decltype(fnGetDescription)>(
        ResolveAddr(Module::Runtime, 0x180025390));
    if (fnGetFileName == nullptr) fnGetFileName = reinterpret_cast<decltype(fnGetFileName)>(
        ResolveAddr(Module::Runtime, 0x180025450));
    if (fnCloseFile == nullptr) fnCloseFile = reinterpret_cast<decltype(fnCloseFile)>(
        ResolveAddr(Module::Runtime, 0x1800accc0));
    if (fnMapFind == nullptr) fnMapFind = reinterpret_cast<decltype(fnMapFind)>(
        ResolveAddr(Module::Runtime, 0x18002ace0));
    if (fnGetPathFilename == nullptr) fnGetPathFilename = reinterpret_cast<decltype(fnGetPathFilename)>(
        ResolveAddr(Module::Runtime, 0x1800c6740));
    if (fnStrICmp == nullptr) fnStrICmp = reinterpret_cast<decltype(fnStrICmp)>(
        ResolveAddr(Module::Runtime, 0x18007eefc));
    if (fnStringFormat == nullptr) fnStringFormat = reinterpret_cast<decltype(fnStringFormat)>(
        ResolveAddr(Module::Runtime, 0x1800c9730));
    if (fnFilesFill == nullptr) fnFilesFill = reinterpret_cast<decltype(fnFilesFill)>(
        ResolveAddr(Module::Runtime, 0x180026880));
    if (fnModSort == nullptr) fnModSort = reinterpret_cast<decltype(fnModSort)>(
        ResolveAddr(Module::Runtime, 0x180028760));

    // -----------------------------------------------------------------------
    // LOOP 1 — one iteration per .pkm found (0x1800af790 .. 0x1800b019f)
    // -----------------------------------------------------------------------
    Filename* const foundData = static_cast<Filename*>(found.data);
    for (int32_t i = 0; i < found.size; ++i) {
        // The stack Mod, zeroed member by member at 0x1800af790..0x1800af7d7.
        // NOTE what is NOT zeroed: +0x24/+0x28/+0x2c (icon width, icon height,
        // crc) and the padding at +0x51..+0x57. Retail leaves them, because
        // every path that APPENDS the record writes all three first and the two
        // early-out paths destroy it without reading them. Raw storage rather
        // than `Mod m{}` so this port leaves exactly what retail leaves.
        alignas(16) uint8_t modStorage[sizeof(Mod)];
        Mod* const m = reinterpret_cast<Mod*>(modStorage);
        m->name = nullptr; m->author = nullptr; m->description = nullptr;
        m->i18 = 0; m->i1c = 0;                       // one qword store
        m->order = 0;
        m->ints.data = nullptr;  m->ints.size = 0;  m->ints.capacity = 0;
        m->bytes.data = nullptr; m->bytes.size = 0; m->bytes.capacity = 0;
        m->enabled = 0;
        m->file.data = nullptr;
        m->set.root = nullptr; m->set.count = 0;
        m->files.data = nullptr; m->files.size = 0; m->files.capacity = 0;

        // 0x1800af7df — m.file = found[i], by reference, then normalised.
        AssignShared(&m->file.data, foundData[i].data);
        RtSystem_Filename_AdjustDelimiters(&m->file);

        // 0x1800af830..0x1800af8e6 — the inlined PackageFile constructor. See
        // the banner for the store-by-store equivalence that lets this call the
        // exported body instead of transcribing the list twice.
        alignas(16) uint8_t pkgStorage[0xF0];
        uint8_t* const pkg = pkgStorage;
        fnPkgCtor(pkg);

        const bool read = fnReadPackfile(pkg, &foundData[i]);    // 0x1800b46b0
        // 0x1800af923 — `platform` (+0x74) non-zero means the package is not for
        // this build, and retail CLOSES THE FILE before discarding it. The
        // ReadPackfile-failed path does NOT close: there the file never opened.
        if (!read) {
            fnPkgDtor(pkg);                                      // 0x1800b4540
            RtIo_Mod_Dtor_180011460(m);                          // 0x180011460
            continue;
        }
        if (*reinterpret_cast<const int32_t*>(pkg + 0x74) != 0) {
            fnCloseFile(fs, pkg + 0xC0);                         // 0x1800accc0
            fnPkgDtor(pkg);
            RtIo_Mod_Dtor_180011460(m);
            continue;
        }

        // 0x1800af941 — the three String accessors. Each fills a temporary the
        // caller must release; the member takes a REFERENCE to the same block,
        // which is why the temporary's release comes after the member assign.
        {
            Filename tmp; tmp.data = nullptr;
            char** got = static_cast<char**>(fnGetName(pkg, &tmp));
            AssignShared(&m->name, *got);
            ReleaseInto(&tmp.data);
        }
        {
            Filename tmp; tmp.data = nullptr;
            char** got = static_cast<char**>(fnGetAuthor(pkg, &tmp));
            AssignShared(&m->author, *got);
            ReleaseInto(&tmp.data);
        }
        {
            Filename tmp; tmp.data = nullptr;
            char** got = static_cast<char**>(fnGetDescription(pkg, &tmp));
            AssignShared(&m->description, *got);
            ReleaseInto(&tmp.data);
        }

        // 0x1800afaba — the four ints that come straight off the header.
        m->i18 = *reinterpret_cast<const int32_t*>(pkg + 0x68);  // version
        m->i1c = *reinterpret_cast<const int32_t*>(pkg + 0x6c);  // userVersion
        m->i24 = *reinterpret_cast<const int32_t*>(pkg + 0x78);  // iconWidth
        m->i28 = *reinterpret_cast<const int32_t*>(pkg + 0x7c);  // iconHeight

        // 0x1800afae2 — the icon, then the preview. Each arrives in a temporary
        // Array this body hands to the member's operator= and then frees.
        {
            ModArray tmp;
            RtIo_PackageFile_GetIconData(pkg, &tmp);             // 0x180025530
            if (&m->ints != &tmp) RtColl_ArrayI32Assign_180011110(&m->ints, &tmp);
            ModMemFree(tmp.data);
            tmp.data = nullptr; tmp.size = 0; tmp.capacity = 0;
        }
        {
            ModArray tmp;
            RtIo_PackageFile_GetPreviewData(pkg, &tmp);          // 0x180025580
            if (&m->bytes != &tmp) RtColl_ArrayU8Assign_180011220(&m->bytes, &tmp);
            ModMemFree(tmp.data);
            tmp.data = nullptr; tmp.size = 0; tmp.capacity = 0;
        }

        m->i2c   = *reinterpret_cast<const int32_t*>(pkg + 0xa8); // crc  0x1800afc8e
        m->order = 0;                                             //      0x1800afc98

        // -------------------------------------------------------------------
        // LOOP 1a — every path inside the package, inserted into the mod's AVL
        // set (0x1800afcb0 .. 0x1800aff1d). The insert is INLINE in retail; only
        // the retrace is a call.
        //
        // The two comparison arms are ASYMMETRIC about nulls and both
        // asymmetries are retail's: a null NODE key stops the leftward descent
        // and sends the walk right, while a null SEARCH key descends LEFT in the
        // first arm and ends the walk with NO INSERT in the second.
        // -------------------------------------------------------------------
        const int32_t entryCount = *reinterpret_cast<const int32_t*>(pkg + 0xb8);
        for (int32_t e = 0; e < entryCount; ++e) {
            Filename key; key.data = nullptr;
            Filename* const k = fnGetFileName(pkg, &key, e);     // 0x180025450

            AvlNode** const root = reinterpret_cast<AvlNode**>(&m->set.root);
            if (*root == nullptr) {
                // 0x1800afccc — the empty-tree case. A failed allocation still
                // increments the count and stores a NULL root; transcribed,
                // because a guard here would leave a count and a tree that
                // disagree in a DIFFERENT way from retail's.
                AvlNode* n = static_cast<AvlNode*>(ModMemAlloc(0x28, 8));
                if (n != nullptr) {
                    RtSystem_Filename_CtorCopy(
                        reinterpret_cast<Filename*>(n), k);      // 0x1800067d0
                    n->parent = nullptr;
                    n->left = nullptr;
                    n->right = nullptr;
                    n->balance = 0; n->height = 0;               // one dword store
                }
                m->set.count += 1;
                *root = n;
            } else {
                AvlNode* cur = *root;
                bool inserted = false;
                for (;;) {
                    // --- left arm, 0x1800afd50 ---
                    bool goLeft;
                    if (cur->key == nullptr)      goLeft = false;   // JZ right arm
                    else if (k->data == nullptr)  goLeft = true;    // JZ go-left
                    else goLeft = (fnStrICmp(k->data, cur->key) < 0);   // SETS
                    if (goLeft) {
                        if (cur->left == nullptr) {
                            AvlNode* n = static_cast<AvlNode*>(ModMemAlloc(0x28, 8));
                            if (n != nullptr) {
                                RtSystem_Filename_CtorCopy(
                                    reinterpret_cast<Filename*>(n), k);
                                n->parent = cur;
                                n->left = nullptr; n->right = nullptr;
                                n->balance = 0; n->height = 0;
                            }
                            cur->left = n;                       // MOV [RDI+0x10],RSI
                            inserted = true;
                        } else {
                            cur = cur->left;
                            continue;
                        }
                        break;
                    }
                    // --- right arm, 0x1800afd81 ---
                    if (k->data == nullptr) break;               // no insert
                    if (cur->key != nullptr &&
                        !(fnStrICmp(cur->key, k->data) < 0)) break;   // no insert
                    if (cur->right == nullptr) {
                        AvlNode* n = static_cast<AvlNode*>(ModMemAlloc(0x28, 8));
                        if (n != nullptr) {
                            RtSystem_Filename_CtorCopy(
                                reinterpret_cast<Filename*>(n), k);
                            n->parent = cur;
                            n->left = nullptr; n->right = nullptr;
                            n->balance = 0; n->height = 0;
                        }
                        cur->right = n;                          // MOV [RDI+0x18],RSI
                        inserted = true;
                        break;
                    }
                    cur = cur->right;
                }
                if (inserted) {
                    // 0x1800afea2 — the retrace starts at the PARENT of the new
                    // node. Retail keeps that in [RSP+0x40], updated on every
                    // descent step, so it always equals the cursor at the point
                    // the child was hung.
                    AvlNode* slot = cur;
                    RtColl_SetInsertFixup_1800274c0(root, &slot);
                    m->set.count += 1;
                }
            }
            ReleaseInto(&key.data);                              // 0x1800afeb5
        }

        fnCloseFile(fs, pkg + 0xC0);                             // 0x1800aff2a

        // 0x1800aff3d — the registry key stem: the mod's path with its directory
        // stripped, kept alive across the temporary's release.
        Filename pathTmp; pathTmp.data = nullptr;
        Filename* const got = fnGetPathFilename(&m->file, &pathTmp, true);
        char* nameRef = got->data;
        if (nameRef != nullptr) ++RefCount(nameRef);             // take the ref FIRST
        ReleaseInto(&pathTmp.data);                              // then drop the temp

        // 0x1800affc9 — "ModEnabled_<stem>". The comparison against "true"
        // (0x180155870) is a byte-for-byte inlined strcmp in retail
        // (0x1800b0013..0x1800b0048) including its `value[0] != 't'` fast path;
        // `strcmp(v, "true") == 0` is the same predicate on every input. A null
        // value takes retail's `EDX = -1` arm and a missing entry leaves the
        // flag at the 0 the record was born with.
        {
            Filename keyStr; keyStr.data = nullptr;
            fnStringFormat(&keyStr, "%s%s", "ModEnabled_", nameRef);
            void* node = fnMapFind(pers, &keyStr);               // 0x18002ace0
            if (node != nullptr) {
                const char* v = *reinterpret_cast<const char* const*>(
                    static_cast<const uint8_t*>(node) + 8);
                const int cmp = (v == nullptr) ? -1 : std::strcmp(v, "true");
                m->enabled = static_cast<uint8_t>(cmp == 0);
            }
            ReleaseInto(&keyStr.data);
        }
        // 0x1800b0085 — "ModIndex_<stem>", parsed base 10. A missing entry
        // leaves `order` at 0.
        {
            Filename keyStr; keyStr.data = nullptr;
            fnStringFormat(&keyStr, "%s%s", "ModIndex_", nameRef);
            void* node = fnMapFind(pers, &keyStr);
            if (node != nullptr) {
                const char* v = *reinterpret_cast<const char* const*>(
                    static_cast<const uint8_t*>(node) + 8);
                m->order = RtCrt_ParseInt10_18007f034(v);        // 0x18007f034
            }
            ReleaseInto(&keyStr.data);
        }

        // 0x1800b00f9 — append. The only writer of the array in the whole body.
        RtColl_ArrayModSetSizeFill_180028610(arr, arr->size + 1, m);

        ReleaseInto(&nameRef);                                   // 0x1800b0113
        fnPkgDtor(pkg);                                          // 0x1800b0172
        RtIo_Mod_Dtor_180011460(m);                              // 0x1800b017c
    }

    // -----------------------------------------------------------------------
    // LOOP 2 — the override pass (0x1800b01a5 .. 0x1800b0332)
    //
    // For every ordered pair (i, j), i != j: walk mod i's path set IN ORDER and
    // look each key up in mod j's set. On the FIRST key they share, append mod
    // j's .pkm filename to mod i's `files` array and move on to the next j. That
    // array is therefore "the mods this one collides with", one entry per
    // colliding mod, in j order — not a list of colliding paths.
    //
    // The lookup is the same two-armed compare as the insert with the
    // allocation removed. The in-order walk is open-coded: descend to the
    // leftmost node, then take the successor repeatedly (leftmost of the right
    // subtree, else climb while we are a right child).
    // -----------------------------------------------------------------------
    for (int32_t i = 0; i < self->count; ++i) {
        for (int32_t j = 0; j < self->count; ++j) {
            if (i == j) continue;                                // CMP R13,R15 ; JZ
            Mod* const mods = static_cast<Mod*>(arr->data);      // re-read, as retail does
            AvlNode* node = reinterpret_cast<AvlNode*>(mods[i].set.root);
            if (node == nullptr) continue;
            while (node->left != nullptr) node = node->left;     // 0x1800b0210
            while (node != nullptr) {
                AvlNode* probe = reinterpret_cast<AvlNode*>(mods[j].set.root);
                bool hit = false;
                while (probe != nullptr) {
                    // 0x1800b0234 — go LEFT while probe->key is non-null and
                    // node->key sorts before it (a null node->key sorts first).
                    bool goLeft;
                    if (probe->key == nullptr)     goLeft = false;
                    else if (node->key == nullptr) goLeft = true;
                    else goLeft = (fnStrICmp(node->key, probe->key) < 0);
                    if (goLeft) { probe = probe->left; continue; }
                    // 0x1800b0258 — a null node->key here is a MATCH, and so is
                    // any probe->key that does not sort before it.
                    if (node->key == nullptr) { hit = true; break; }
                    if (probe->key == nullptr) { probe = probe->right; continue; }
                    if (fnStrICmp(probe->key, node->key) < 0) { probe = probe->right; continue; }
                    hit = true;
                    break;
                }
                if (hit) {
                    // 0x1800b02d2 — append mods[j].file to mods[i].files.
                    fnFilesFill(&mods[i].files, mods[i].files.size + 1,
                                &mods[j].file);                  // 0x180026880
                    break;                                       // next j
                }
                // in-order successor (0x1800b027f)
                if (node->right != nullptr) {
                    node = node->right;
                    while (node->left != nullptr) node = node->left;
                } else {
                    AvlNode* up = node->parent;
                    if (up == nullptr)         node = node->parent;   // null
                    else if (up->left == node) node = up;
                    else {
                        while (up != nullptr && up->right == node) {
                            node = up;
                            up = up->parent;
                        }
                        node = node->parent;
                    }
                }
            }
        }
    }

    // 0x1800b0338 — the sort. PORTED (BATCH 8b,
    // runtime/src/io/modification_sort.cpp); still reached by address, which is
    // enough, because ResolveAddr consults the port registry first. The third
    // argument is a stack slot the callee never reads.
    if (arr->size > 1) {
        void* scratch = nullptr;
        fnModSort(arr->data, arr->size, &scratch);
    }

    // 0x1800b0350 — release every found Filename, then the array itself.
    for (int32_t i = 0; i < found.size; ++i) ReleaseInto(&foundData[i].data);
    ModMemFree(found.data);
    found.data = nullptr; found.size = 0; found.capacity = 0;

    // -----------------------------------------------------------------------
    // The oracle. See the banner for why running theirs SECOND is legal here.
    // -----------------------------------------------------------------------
}

// ===========================================================================
// Registration
// ===========================================================================
// BOTH DOORS for the three rows that have a name in the export table, address
// door alone for the five that do not. RefreshList needs both: it is reached by
// NAME from runtime/src/engine/engine_lifecycle.cpp:605 (the Engine::Startup
// call attempt 19 died on) and by ADDRESS through engine/funcs/
// engine_funcs.cpp's `fn_Modification_RefreshList`, which
// code/Game/Dialog/moddialog.cpp calls on both the mods dialog's Cancel and its
// Ok. Registering one and not the other would leave the second resolving null
// with the name never mentioned in the log.
//
// EVERY MANGLED NAME AND ORDINAL BELOW WAS PRINTED OUT OF Runtime-x64.dll's OWN
// EXPORT DIRECTORY with pefile, not transcribed from a decompile. The first
// draft of this file guessed `?$Array@H` for GetIconData; the real export is
// `?$Array@I` (unsigned int) and `?BV` (const), which no compiler would have
// caught — the same class of error lane PKG2 records for its ordinals.
//
//   ?RefreshList@Modification@IO@@QEAAXXZ                            ord 3528
//   ?GetIconData@PackageFile@IO@@QEBA?BV?$Array@I@Collection@@XZ     ord 2697
//   ?GetPreviewData@PackageFile@IO@@QEBA?BV?$Array@E@Collection@@XZ  ord 2855
//
// Every address was grepped for an existing `RegisterRuntimeAddr(0x…` across
// runtime/, standalone/ and code/ before being claimed — the check
// `reference_grep_for_the_port_before_writing_it.md` exists for. All eight were
// unregistered.
//
// The mangled names are LITERALS: tools/runtime_census.py matches
// `RegisterRuntimeExport("<literal>"` and cannot see through a constant.
int RtIoModificationRefreshRegister() {
    int n = 0;

    RegisterRuntimeExport("?RefreshList@Modification@IO@@QEAAXXZ",
                          (void*)&RtIo_Modification_RefreshList); ++n;
    RegisterRuntimeExport("?GetIconData@PackageFile@IO@@QEBA?BV?$Array@I@Collection@@XZ",
                          (void*)&RtIo_PackageFile_GetIconData); ++n;
    RegisterRuntimeExport("?GetPreviewData@PackageFile@IO@@QEBA?BV?$Array@E@Collection@@XZ",
                          (void*)&RtIo_PackageFile_GetPreviewData); ++n;

    RegisterRuntimeAddr(0x1800af5f0, (void*)&RtIo_Modification_RefreshList);
    RegisterRuntimeAddr(0x180025530, (void*)&RtIo_PackageFile_GetIconData);
    RegisterRuntimeAddr(0x180025580, (void*)&RtIo_PackageFile_GetPreviewData);

    // Unexported in both shipped builds — address door only.
    RegisterRuntimeAddr(0x180028610, (void*)&RtColl_ArrayModSetSizeFill_180028610);
    RegisterRuntimeAddr(0x1800274c0, (void*)&RtColl_SetInsertFixup_1800274c0);
    RegisterRuntimeAddr(0x180027f50, (void*)&AvlRotateRight);
    RegisterRuntimeAddr(0x180027e20, (void*)&AvlRotateLeft);
    RegisterRuntimeAddr(0x18007f034, (void*)&RtCrt_ParseInt10_18007f034);

    printf("[RtPort]   IO::Modification::RefreshList (lane MOD2): "
           "%d exports + 8 addresses\n", n);
    return n;
}

}  // namespace RtIO
