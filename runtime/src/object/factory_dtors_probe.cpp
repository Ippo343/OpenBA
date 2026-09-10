// ============================================================================
// runtime/src/object/factory_dtors_probe.cpp — the SYNTHESISED oracle for the
//                       three Object factory destructors (Wave 11, lane OBJLIFE)
// ============================================================================
//
// WHY THIS EXISTS
// ---------------
// `??1Factory`, `??1InternalFactory` and `??1ExternalFactory` have exactly one
// caller in this whole tree — `RtEngine_Engine_Shutdown` — and that body itself
// has calls=0, because game_main.cpp runs `ShutdownEngineStaged()` instead and
// never calls the real Engine::Shutdown. So there is NO GAMEPLAY ROUTE to any
// of the three, in any run, with any env var.
//
// *** "NO ROUTE" IS NOT "NO ORACLE." *** A synthesised differential is exactly
// what found a missing third argument in a body provably unreachable in play
// (reference_no_route_is_not_no_oracle). This file builds two structurally
// identical subjects from scratch, tears one down with the SHIPPED body and one
// down with OURS, and compares. Nothing here is a self-check: the "theirs" side
// is `RtOriginal<>()`/`ResolveExportRaw`, never `ResolveExport`, which after
// registration would hand back our own function and compare ours to ours
// (reference_shadow_marker_is_not_an_oracle).
//
// WHAT IS COMPARED, AND WHY IT IS THE RIGHT THING
// -----------------------------------------------
// Two screens, scored SEPARATELY — a union score hides a blind screen
// (reference_union_control_hides_a_blind_screen):
//
//   SCREEN 1  final struct bytes. The whole subject header is memcmp'd. This
//             catches a wrong closing store: a qword where retail writes a
//             DWORD, a field zeroed that retail leaves alone, a member
//             destroyed in the wrong order leaving a different residue.
//
//   SCREEN 2  the DLL allocator's own accounting, read straight off its live
//             singleton: `_allocCount` (+0x638, live block count) and
//             `_allocatedBytes` (+0x630). The delta across each teardown says
//             HOW MANY BLOCKS AND HOW MANY BYTES that teardown handed back.
//
// Screen 2 is the one that matters for this row set, because the defect class
// the lane brief named — "a body that only frees the block instead of
// destroying every element", a leak shipped under a false byte-identity claim —
// is INVISIBLE to screen 1 and is precisely a block-count difference. A
// ~Factory that skipped the tree walk would leave identical struct bytes and
// return N fewer blocks.
//
// THE NEGATIVE CONTROL
// --------------------
// Doctrine: every probe needs one, and its subject must be something the
// comparison actually READS — a control that corrupts a value nothing compares
// reports NOT CAUGHT and indicts a sound instrument
// (reference_control_must_pick_a_compared_subject). The controls here are
// deliberately-wrong destructors run on a third subject:
//
//   CONTROL-A  ~Factory that frees the root node but NOT the subtree. This is
//              literally the "only frees the block" mistake. Screen 2 must
//              catch it. Screen 1 must NOT (the closing stores are the same) —
//              and that asymmetry is the point: it is what proves screen 2 is
//              carrying the load rather than riding on screen 1.
//   CONTROL-B  ~InternalFactory that destroys only `_persistIds` and forgets
//              `_objects`. Screen 2 must catch it.
//   CONTROL-C  ~ExternalFactory that widens the two closing DWORD stores to
//              qwords. Screen 1 must catch it; screen 2 must NOT. The mirror
//              image of CONTROL-A, so neither screen's score can be borrowed
//              from the other.
//
// WHAT THIS ORACLE CANNOT REACH — stated plainly rather than left implied:
//   * The trees it tears down are HAND-BUILT here, not built by the engine's
//     own inserts. Their SHAPE (7 nodes, both children populated, some child
//     arrays null and some real) is chosen to exercise every branch, but it is
//     not a shape retail ever produced.
//   * It does not observe the ORDER of the frees, only the total. A body that
//     freed the same blocks in a different order would pass.
//   * Recursion depth is 3 here; retail's would be ~20. Nothing about stack
//     depth is tested.
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>

#include "../../funcs/rt_port.h"
#include "../../types/object/TypeInfo.h"
#include "../../types/object/Internal.h"
#include "../../types/object/External.h"
#include "../../types/system/MemoryAllocator.h"
#include "../system/string.h"

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace RtObjFactoryDtor {
// The three ports and the helpers, from factory_dtors.cpp.
void RtObject_Factory_Dtor(void* self);
void RtObject_InternalFactory_Dtor(void* self);
void RtObject_ExternalFactory_Dtor(void* self);
void TypeSubtreeFree(void* set, RtObject::TypeNode* node);
}  // namespace RtObjFactoryDtor

namespace {

using RtSystem::RtMemAlloc;
using RtSystem::RtMemFree;
using RtObject::TypeNode;

// Same semantics as internal.cpp's RtOptIn (which is file-local there): a
// probe is OFF unless explicitly asked for, the mirror image of RtWaveEnabled's
// kill-switch sense. Spelled out rather than shared so that turning this probe
// on can never be a side effect of a wave switch.
bool ProbeOptIn(const char* name) {
    const char* e = std::getenv(name);
    return e && *e && std::strcmp(e, "0") != 0 && std::strcmp(e, "false") != 0;
}

// ---------------------------------------------------------------------------
// The live DLL allocator singleton, for screen 2.
//
// 0x1801fc718 is the pointer the J1 array destructors load with
// `MOV RBX,[0x1801fc718]`; the counters are at +0x630/+0x638
// (runtime/types/system/MemoryAllocator.h, both offsets static_asserted there).
// Read through ResolveAddr so ASLR is handled, and read DIRECTLY rather than
// through GetAllocatedSize so that arming the allocator port cannot make this
// instrument measure our own model instead of the real arena.
// ---------------------------------------------------------------------------
struct AllocSnap { int64_t bytes; int32_t count; bool valid; };

AllocSnap Snapshot() {
    AllocSnap s{0, 0, false};
    uintptr_t slot = ResolveAddr(Module::Runtime, 0x1801fc718);
    if (!slot) return s;
    uint8_t* a = *reinterpret_cast<uint8_t**>(slot);
    if (!a) return s;
    s.bytes = *reinterpret_cast<int64_t*>(a + 0x630);
    s.count = *reinterpret_cast<int32_t*>(a + 0x638);
    s.valid = true;
    return s;
}

// ---------------------------------------------------------------------------
// Subject builders. Everything is allocated through the DLL's own allocator,
// because the destructors under test hand it all back through the DLL's own
// free path and a foreign block would corrupt the arena rather than fail a
// comparison.
// ---------------------------------------------------------------------------

// A refcounted System::String block: [len][refcount=1][chars…], data = block+8.
char* MakeString(const char* text) {
    const int len = static_cast<int>(std::strlen(text));
    int32_t* block = static_cast<int32_t*>(
        RtMemAlloc(static_cast<uint64_t>(len) + 1 + 8, 8));
    if (!block) return nullptr;
    block[0] = len;
    block[1] = 1;
    char* data = reinterpret_cast<char*>(block + 2);
    std::memcpy(data, text, static_cast<size_t>(len) + 1);
    return data;
}

// ---- Factory: a 7-node TypeNode tree ---------------------------------------
// Shape (depth 3, both children populated at every internal node):
//
//            n0                 every node is 0x50 bytes from
//          /    +               MemAlloc(0x50, 8), exactly as retail's
//        n1      n2             0x180008440 allocates it.
//       /  +    /  +
//     n3   n4  n5   n6          ('+' is a right edge; a trailing backslash
//                                would continue the comment line.)
//
// `value.children` (node+0x20) is a real 3-pointer buffer on the ODD-indexed
// nodes and null on the even ones, so both arms of the J1 free are exercised.
// Node count and buffer count are returned so the expected block delta is a
// number this file computed rather than a number it read back.
struct FactorySubject { void* root; int32_t count; };

int BuildFactoryTree(FactorySubject* out) {
    TypeNode* n[7];
    int blocks = 0;
    for (int i = 0; i < 7; ++i) {
        n[i] = static_cast<TypeNode*>(RtMemAlloc(0x50, 8));
        if (!n[i]) return -1;
        std::memset(n[i], 0, 0x50);
        ++blocks;
        n[i]->key = static_cast<uint32_t>(0x1000 + i);
        n[i]->value.typeId = n[i]->key;
        if (i & 1) {
            n[i]->value.children =
                static_cast<RtObject::TypeRecord**>(RtMemAlloc(3 * sizeof(void*), 8));
            if (!n[i]->value.children) return -1;
            std::memset(n[i]->value.children, 0, 3 * sizeof(void*));
            n[i]->value.childCount = 0;
            n[i]->value.childCapacity = 3;
            ++blocks;
        }
    }
    n[0]->left = n[1];  n[0]->right = n[2];
    n[1]->left = n[3];  n[1]->right = n[4];
    n[2]->left = n[5];  n[2]->right = n[6];
    for (int i = 1; i < 7; ++i) n[i]->parent = n[(i - 1) / 2];

    out->root  = n[0];
    out->count = 7;
    return blocks;   // 7 nodes + 4 child buffers = 11
}

// ---- InternalFactory: two Array headers with real buffers ------------------
// Only the first 0x20 bytes are modelled: `_objects` (+0x00), `_persistIds`
// (+0x10). `_random` (+0x20, 0x9c4 bytes) is trivially destructible and the
// destructor never touches it, so the subject stops at 0x20 and the comparison
// covers exactly what the body can write.
struct InternalFactorySubject {
    void*   objData;   int32_t objSize;   int32_t objCap;    // +0x00 _objects
    void*   pidData;   int32_t pidSize;   int32_t pidCap;    // +0x10 _persistIds
};

int BuildInternalFactory(InternalFactorySubject* out) {
    std::memset(out, 0, sizeof(*out));
    out->objData = RtMemAlloc(8 * sizeof(void*), 8);
    out->pidData = RtMemAlloc(16 * sizeof(int32_t), 8);
    if (!out->objData || !out->pidData) return -1;
    out->objSize = 5;  out->objCap = 8;
    out->pidSize = 12; out->pidCap = 16;
    return 2;   // exactly two blocks to hand back
}

// ---- ExternalFactory: path stack + two trees -------------------------------
// The subject is a full 0x48-byte ExternalFactory image. The two Set trees use
// the DIFFERENT node shapes their destructors read:
//   _references  0x30-byte node, String at +0x08, children +0x18 / +0x20
//   _resources   0x38-byte node, String at +0x00, children +0x20 / +0x28
// Getting these two the same way round would be the exact mistake the lane
// brief warns about, so the builder writes each from the offsets its own
// destructor reads.
struct ExternalFactorySubject { uint8_t bytes[0x48]; };

void* MakeRefNode(const char* text, void* l, void* r, int* blocks) {
    uint8_t* n = static_cast<uint8_t*>(RtMemAlloc(0x30, 8));
    if (!n) return nullptr;
    std::memset(n, 0, 0x30);
    ++*blocks;
    *reinterpret_cast<char**>(n + 0x08) = MakeString(text);   // the released slot
    if (*reinterpret_cast<char**>(n + 0x08)) ++*blocks;
    *reinterpret_cast<void**>(n + 0x18) = l;
    *reinterpret_cast<void**>(n + 0x20) = r;
    return n;
}

void* MakeResNode(const char* text, void* l, void* r, int* blocks) {
    uint8_t* n = static_cast<uint8_t*>(RtMemAlloc(0x38, 8));
    if (!n) return nullptr;
    std::memset(n, 0, 0x38);
    ++*blocks;
    *reinterpret_cast<char**>(n + 0x00) = MakeString(text);   // the released slot
    if (*reinterpret_cast<char**>(n + 0x00)) ++*blocks;
    *reinterpret_cast<void**>(n + 0x20) = l;
    *reinterpret_cast<void**>(n + 0x28) = r;
    return n;
}

int BuildExternalFactory(ExternalFactorySubject* out) {
    std::memset(out, 0, sizeof(*out));
    uint8_t* p = out->bytes;
    int blocks = 0;

    // +0x00 _cleanBitIndex / _cleanBitMask / _generation — untouched by the
    // destructor; given non-zero values so a body that DID touch them shows up
    // on screen 1.
    *reinterpret_cast<int32_t*>(p + 0x00) = 3;
    *reinterpret_cast<uint32_t*>(p + 0x04) = 0x8u;
    *reinterpret_cast<int32_t*>(p + 0x08) = 77;
    *reinterpret_cast<int32_t*>(p + 0x0c) = 0x5A5A5A5A;   // the pad, deliberately dirty

    // +0x10 _resources: a 3-node tree
    void* rl = MakeResNode("res/left.rsc",  nullptr, nullptr, &blocks);
    void* rr = MakeResNode("res/right.rsc", nullptr, nullptr, &blocks);
    *reinterpret_cast<void**>(p + 0x10) = MakeResNode("res/root.rsc", rl, rr, &blocks);
    *reinterpret_cast<int32_t*>(p + 0x18) = 3;
    *reinterpret_cast<int32_t*>(p + 0x1c) = 0x5A5A5A5A;   // never written by retail

    // +0x20 _references: a 3-node tree of the OTHER shape
    void* fl = MakeRefNode("ref/left",  nullptr, nullptr, &blocks);
    void* fr = MakeRefNode("ref/right", nullptr, nullptr, &blocks);
    *reinterpret_cast<void**>(p + 0x20) = MakeRefNode("ref/root", fl, fr, &blocks);
    *reinterpret_cast<int32_t*>(p + 0x28) = 3;
    *reinterpret_cast<int32_t*>(p + 0x2c) = 0x5A5A5A5A;   // never written by retail

    // +0x30 _pathStack: 3 live entries in a buffer of 4, each two Strings.
    // The FOURTH entry is left as garbage on purpose: retail releases `_size`
    // elements, not `_capacity`, and a body that walked the capacity would
    // free a wild pointer here rather than pass quietly.
    RtObject::PathStackEntry* stack = static_cast<RtObject::PathStackEntry*>(
        RtMemAlloc(4 * sizeof(RtObject::PathStackEntry), 8));
    if (!stack) return -1;
    ++blocks;
    std::memset(stack, 0, 4 * sizeof(RtObject::PathStackEntry));
    const char* dirs[3] = { "dir/a", "dir/b", "dir/c" };
    const char* fns[3]  = { "f1.rsc", "f2.rsc", "f3.rsc" };
    for (int i = 0; i < 3; ++i) {
        char** slot = reinterpret_cast<char**>(&stack[i]);
        slot[0] = MakeString(dirs[i]); if (slot[0]) ++blocks;
        slot[1] = MakeString(fns[i]);  if (slot[1]) ++blocks;
    }
    *reinterpret_cast<void**>(p + 0x30)   = stack;
    *reinterpret_cast<int32_t*>(p + 0x38) = 3;
    *reinterpret_cast<int32_t*>(p + 0x3c) = 4;

    p[0x40] = 1; p[0x41] = 1; p[0x42] = 0;   // the three bools, untouched
    return blocks;
}

// ---------------------------------------------------------------------------
// The deliberately-wrong bodies used as negative controls.
// ---------------------------------------------------------------------------

// CONTROL-A: "only frees the block" — the exact defect class the brief names.
// Same closing stores as the real body, so screen 1 sees nothing; the six
// non-root nodes and their child buffers are never handed back, so screen 2
// must see it.
void ControlA_FactoryDtor_OnlyFreesRoot(void* self) {
    struct Hdr { TypeNode* root; int32_t count; };
    Hdr* s = static_cast<Hdr*>(self);
    if (s->root) {
        using Fn = void (*)(void*, void*);
        static Fn setFree = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x180009c30));
        setFree(self, s->root);
    }
    s->root = nullptr;
    s->count = 0;
}

// CONTROL-B: destroys `_persistIds` and forgets `_objects`.
void ControlB_InternalFactoryDtor_HalfDone(void* self) {
    using Fn = void* (*)(void*);
    static Fn d = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x18001c750));
    d(static_cast<uint8_t*>(self) + 0x10);
}

// CONTROL-C: the real body, but the two closing DWORD stores widened to
// qwords. Frees exactly what retail frees, so screen 2 must be silent; it
// clobbers +0x1c and +0x2c, which retail leaves alone, so screen 1 must fire.
void ControlC_ExternalFactoryDtor_WideStores(void* self) {
    RtObjFactoryDtor::RtObject_ExternalFactory_Dtor(self);
    uint8_t* p = static_cast<uint8_t*>(self);
    *reinterpret_cast<int64_t*>(p + 0x18) = 0;
    *reinterpret_cast<int64_t*>(p + 0x28) = 0;
}

// ---------------------------------------------------------------------------
// One differential cell.
// ---------------------------------------------------------------------------
struct Cell {
    const char* name;
    bool  bytesEqual;
    bool  allocEqual;
    int32_t theirBlocks, ourBlocks;
    int64_t theirBytes,  ourBytes;
    int32_t expectedBlocks;
};

int g_cells = 0;
int g_bytesFail = 0;
int g_allocFail = 0;

void Report(const Cell& c) {
    ++g_cells;
    if (!c.bytesEqual) ++g_bytesFail;
    if (!c.allocEqual) ++g_allocFail;
    printf("[FactoryDtorProbe] %-42s screen1(bytes)=%s screen2(alloc)=%s  "
           "blocks theirs=%d ours=%d expected=%d  bytes theirs=%lld ours=%lld\n",
           c.name,
           c.bytesEqual ? "SAME" : "*** DIFFER ***",
           c.allocEqual ? "SAME" : "*** DIFFER ***",
           c.theirBlocks, c.ourBlocks, c.expectedBlocks,
           (long long)c.theirBytes, (long long)c.ourBytes);
}

template <typename Subject, typename Build, typename TheirFn, typename OurFn>
Cell RunPair(const char* name, Build build, TheirFn theirs, OurFn ours) {
    Cell c{name, false, false, 0, 0, 0, 0, 0};

    // POISON BOTH SUBJECTS FIRST. `FactorySubject` is {void*, int32} = 16
    // bytes, so it has four padding bytes that NEITHER destructor writes — and
    // an oracle that memcmps bytes neither side writes fails on stack garbage
    // and reads like a catastrophic port defect
    // (reference_oracle_must_not_compare_unwritten_bytes). Poisoning to the
    // same pattern makes the untouched bytes compare EQUAL by construction
    // while still catching a body that writes them.
    Subject a, b;
    std::memset(&a, 0xCD, sizeof(Subject));
    std::memset(&b, 0xCD, sizeof(Subject));
    const int blocksA = build(&a);
    const int blocksB = build(&b);
    if (blocksA < 0 || blocksB < 0 || blocksA != blocksB) {
        printf("[FactoryDtorProbe] %-42s SKIPPED — subject build failed "
               "(a=%d b=%d)\n", name, blocksA, blocksB);
        c.expectedBlocks = -1;
        return c;
    }
    c.expectedBlocks = blocksA;

    // NOTE ON WHY SCREEN 1 COMPARES THE *AFTER* IMAGES: the two subjects are
    // NOT byte-equal on entry and cannot be, because their pointer fields are
    // different allocations. What both destructors do to those fields is write
    // constants (null / 0), so the AFTER images are comparable while the BEFORE
    // images are not. The non-pointer fields the destructors must NOT touch are
    // seeded to the same values in the builders, so they carry the comparison
    // for that half.

    const AllocSnap s0 = Snapshot();
    theirs(&a);
    const AllocSnap s1 = Snapshot();
    ours(&b);
    const AllocSnap s2 = Snapshot();

    if (!s0.valid || !s1.valid || !s2.valid) {
        printf("[FactoryDtorProbe] %-42s SKIPPED — allocator singleton not "
               "readable; screen 2 would be vacuous\n", name);
        c.expectedBlocks = -1;
        return c;
    }

    c.theirBlocks = s0.count - s1.count;
    c.ourBlocks   = s1.count - s2.count;
    c.theirBytes  = s0.bytes - s1.bytes;
    c.ourBytes    = s1.bytes - s2.bytes;

    c.bytesEqual = std::memcmp(&a, &b, sizeof(Subject)) == 0;
    c.allocEqual = (c.theirBlocks == c.ourBlocks) && (c.theirBytes == c.ourBytes);
    return c;
}

}  // namespace

// ============================================================================
// Entry point. Armed by BANISHED_RT_FACTORYDTOR_PROBE=1 and run once, late in
// init — never on by default: it allocates and frees ~40 real blocks in the
// DLL's live arena, which is harmless but is not something an ordinary run
// should do.
// ============================================================================
void RtObjectFactoryDtorsProbe() {
    if (!ProbeOptIn("BANISHED_RT_FACTORYDTOR_PROBE")) return;

    // CALLED TWICE ON PURPOSE: once from RtObjectPortInit (early, before the
    // window opens) and once from the exit summary. The early call is where a
    // boot-only run can see it, but it runs before some of the engine is up, so
    // if the DLL allocator singleton is not readable yet every cell would
    // report SKIPPED and the run would be wasted. The late call is the safety
    // net. Whichever one produces a real verdict first claims the slot; the
    // other returns immediately.
    static bool s_reported = false;
    if (s_reported) return;

    {
        // Refuse to report at all if the arena is not readable — a probe that
        // cannot run screen 2 would be a bytes-only oracle, and screen 1 is
        // STRUCTURALLY BLIND to the defect class this row set is about.
        const AllocSnap probe = Snapshot();
        if (!probe.valid) {
            printf("[FactoryDtorProbe] deferred — the DLL allocator singleton "
                   "at 0x1801fc718 is not readable yet, so screen 2 would be "
                   "vacuous and screen 1 alone cannot see the leak class. Will "
                   "retry from the exit summary.\n");
            return;
        }
    }
    s_reported = true;

    printf("[FactoryDtorProbe] ==== synthesised differential for the three "
           "Object factory destructors ====\n");
    printf("[FactoryDtorProbe] theirs = the SHIPPED body via RtOriginal (never "
           "ResolveExport); ours = the port in factory_dtors.cpp\n");

    using VoidThis = void (*)(void*);
    VoidThis theirFactory  = RtOriginal<VoidThis>("??1Factory@Object@@QEAA@XZ");
    VoidThis theirInternal = RtOriginal<VoidThis>("??1InternalFactory@Object@@QEAA@XZ");
    VoidThis theirExternal = RtOriginal<VoidThis>("??1ExternalFactory@Object@@QEAA@XZ");

    if (!theirFactory || !theirInternal || !theirExternal) {
        printf("[FactoryDtorProbe] ABORTED — one of the shipped bodies did not "
               "resolve (Factory=%p InternalFactory=%p ExternalFactory=%p). "
               "Without the DLL side this would be a SELF-CHECK, not an "
               "oracle, so nothing is reported.\n",
               (void*)theirFactory, (void*)theirInternal, (void*)theirExternal);
        return;
    }

    // ---- the three real comparisons ----------------------------------------
    Report(RunPair<FactorySubject>(
        "~Factory              ours vs shipped", BuildFactoryTree,
        theirFactory, RtObjFactoryDtor::RtObject_Factory_Dtor));

    Report(RunPair<InternalFactorySubject>(
        "~InternalFactory      ours vs shipped", BuildInternalFactory,
        theirInternal, RtObjFactoryDtor::RtObject_InternalFactory_Dtor));

    Report(RunPair<ExternalFactorySubject>(
        "~ExternalFactory      ours vs shipped", BuildExternalFactory,
        theirExternal, RtObjFactoryDtor::RtObject_ExternalFactory_Dtor));

    const int realBytesFail = g_bytesFail;
    const int realAllocFail = g_allocFail;
    const int realCells     = g_cells;

    // ---- the negative controls ---------------------------------------------
    printf("[FactoryDtorProbe] ---- negative controls (each MUST be caught by "
           "the screen named, and the OTHER screen's silence is part of the "
           "claim) ----\n");
    g_cells = g_bytesFail = g_allocFail = 0;

    Cell ca = RunPair<FactorySubject>(
        "CONTROL-A only-frees-root (want s2 fire)", BuildFactoryTree,
        theirFactory, ControlA_FactoryDtor_OnlyFreesRoot);
    Report(ca);

    Cell cb = RunPair<InternalFactorySubject>(
        "CONTROL-B half-done      (want s2 fire)", BuildInternalFactory,
        theirInternal, ControlB_InternalFactoryDtor_HalfDone);
    Report(cb);

    Cell cc = RunPair<ExternalFactorySubject>(
        "CONTROL-C wide-stores    (want s1 fire)", BuildExternalFactory,
        theirExternal, ControlC_ExternalFactoryDtor_WideStores);
    Report(cc);

    // Score each screen SEPARATELY. A union score of 3/3 cannot tell "both
    // screens work" from "one screen is blind and the other covered for it".
    const int s2Caught = (!ca.allocEqual ? 1 : 0) + (!cb.allocEqual ? 1 : 0);
    const int s1Caught = (!cc.bytesEqual ? 1 : 0);
    printf("[FactoryDtorProbe] CONTROL SCORE  screen1(bytes) %d/1 caught   "
           "screen2(alloc) %d/2 caught\n", s1Caught, s2Caught);
    if (ca.bytesEqual)
        printf("[FactoryDtorProbe]   (and CONTROL-A was INVISIBLE to screen 1, "
               "as predicted — that is what makes screen 2 load-bearing rather "
               "than decorative)\n");
    if (cc.allocEqual)
        printf("[FactoryDtorProbe]   (and CONTROL-C was INVISIBLE to screen 2, "
               "as predicted)\n");

    printf("[FactoryDtorProbe] ==== VERDICT: %d real cells, screen1 failures "
           "%d, screen2 failures %d ====\n", realCells, realBytesFail, realAllocFail);
}
