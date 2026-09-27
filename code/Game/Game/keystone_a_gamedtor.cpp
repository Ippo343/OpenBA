// code/Game/Game/keystone_a_gamedtor.cpp
// Keystone A (Game::Dtor + worker-thread teardown cluster) — faithful 1:1 ports of
// the exe Game destructor (0x14009ed60) and its private teardown sub-callees:
//   0x1400a6690  worker-queue drain + Thread::Start/WaitForFinish (stop the worker)
//   0x1400a6720  Thread (worker-queue object) destructor
//   0x1400a8570  Game sub-object (+0x70) destructor — frees the path/cmd pools
//   0x14001a450  tree+array destructor (+0x78 of Game)
//   0x14001a5f0  free-list + node-chain destructor (the +0xa8 / a8570+0xc8 pool)
//   0x14001ced0  cell-BST clear (recycles nodes onto the free list, frees the slab chain)
//   0x14001dac0  array-free (stride 0x18; frees element[0] of each, then the block)
//   0x14001e3a0  array-free (stride 0x20; frees element[0] of each, then the block)
//
// This runs ONLY at shutdown, which the autostart A/B path never reaches — so a wrong
// transcription is undetectable silent corruption. Every offset / branch / arg below is
// cross-checked against BOTH the Ghidra decompile AND the disassembly. Do NOT "improve"
// or simplify.
//
// Already-registered sub-callees this cluster reuses (NOT re-ported here):
//   0x140014c30  WorkBase_TreeNodeFree_14c30  (reg_work_base; children +0x10/+0x18)
//   0x14000e320  WorkBase_TreeNodeFree_e320   (reg_work_base; children +0x18/+0x20)
//   0x14001d400  Pf_BstFreeRecursive_d400     (reg_pf_pools; recursive cell-BST node free)
//   0x14001e490  Pf_FreeArray30_e490          (reg_pf_pools; array-free stride 0x30)
//
// Runtime-DLL exports used (resolved by mangled name, identical strings to the import
// name table behind exe IAT dispatch slots 0x14015252x/253x/255x/256x):
//   ?YieldToThreads@Application@1@QEAAXXZ             (slot 0x140152560)
//   ??1CriticalSection@System@@QEAA@XZ               (slot 0x140152530, a6720 inner dtor)
//   (the a6720 tail-call, ??1Thread@System@@ via slot 0x140152520, is the
//    compiler's base chain since ENDGAME B.3.9 VS: a6720 is ~PathfinderThread)
//   ??1Scene@Picking@@QEAA@XZ                         (slot 0x140151fb8, ed60 tail-call)
//
// NOTE on the StackCriticalSection in 0x1400a6690: the original uses a stack-scoped
// RAII guard (ctor=Lock at slot 0x140152538, dtor=Unlock at slot 0x140152548). The
// guard's ctor calls CriticalSection::Lock and stashes the CS*; its dtor calls Unlock.
// We model it directly with raw Lock/Unlock on (queue + 0x30), which is behaviourally
// identical and matches the loop's lock/unlock placement byte-for-byte.
//
// SPIN/WAIT concern: 0x1400a6690 spins (CS-guarded count check + YieldToThreads) until
// the worker queue (count @+0x70) drains to 0, then WaitForFinish on the worker thread.
// It starts the thread (if not already running, flag bit0 @+0x08) so the worker can
// process the queue. In standalone the Runtime Thread infra is real, so this drains
// normally; but it is shutdown-only and not autostart-exercised. See FINAL REPORT.
//
// Registered by standalone/reg_keystone_a_gamedtor.cpp.

// ENDGAME B.3.2 step 2 (COLL-GAME sweep): every `MemFree(data); data = 0; one
// qword over size+capacity = 0` triple below was a hand-inlined
// `Collection::Array<T>::~Array` for a trivially-destructible T
// (`rtcoll::ArrayDtorImpl`, byte for byte) and is now `->~Array()` on the
// header, spelled `rtcoll::ArrayHdr` because none of these element types is
// pinned in this file. In 1dac0 / 1e3a0 only the two `~Array`s are converted
// (the inner header at element+0x08 / +0x10 and the outer block); the
// per-element walk stays as written because the outer element's grow arms are
// not pinned, so no `ArraySpec` row can honestly be written for it yet. Slab
// chains, free lists, node chains, the cell BST and the `MemFree` with no
// header stores (a6720's [thread+0x158]) are not arrays and are untouched.
#include "Game/PathfinderThread.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "collection/Collection.h"
#include <cstdint>
#include "system/Thread.h"     // ENDGAME B.3.4b System caller batch (CriticalSection/Thread)
#include "application/Application.h"   // ENDGAME B.3.7 callers (K2)

namespace {

// ---- Runtime-DLL exports (resolved by mangled name; same strings as keystone_a_zone.cpp).
typedef void (*fn_CSDtor_t)(void* cs);
typedef void (*fn_SceneDtor_t)(void* scene);

static fn_CSDtor_t        s_fn_CSDtor       = nullptr;
static fn_SceneDtor_t     s_fn_SceneDtor    = nullptr;

// ---- Already-registered exe sub-callees (resolved via the standalone registry). ----
typedef void (*fn_TreeFreeE320_t)(void* avlHdr, void* node);   // 0x14000e320 (children +0x18/+0x20)
typedef void (*fn_BstFreeRec_d400_t)(uintptr_t avlHdr, uint64_t* node); // 0x14001d400 (cell-BST recursive)
typedef void (*fn_FreeArray30_e490_t)(uint64_t unused, uintptr_t base, int count); // 0x14001e490
typedef void (*fn_TreeFree14c30_t)(void* slot, void* node);    // 0x140014c30 (children +0x10/+0x18)

static fn_TreeFreeE320_t    s_fn_TreeFreeE320 = nullptr;
static fn_BstFreeRec_d400_t s_fn_BstFreeD400  = nullptr;
static fn_FreeArray30_e490_t s_fn_FreeArr30   = nullptr;
static fn_TreeFree14c30_t   s_fn_TreeFree14c30 = nullptr;

// ---- Application::_singleton exref (slot 0x140152368), for YieldToThreads. ----
// engine_funcs.cpp resolves g_pAppSingleton from this slot; reuse it.
static bool s_init = false;
static void EnsureFns() {
    if (s_init) return;
    s_fn_CSDtor      = reinterpret_cast<fn_CSDtor_t>(ResolveExport("??1CriticalSection@System@@QEAA@XZ"));
    s_fn_SceneDtor   = reinterpret_cast<fn_SceneDtor_t>(ResolveExport("??1Scene@Picking@@QEAA@XZ"));

    s_fn_TreeFreeE320 = reinterpret_cast<fn_TreeFreeE320_t>(ResolveAddr(Module::Exe, 0x14000e320));
    s_fn_BstFreeD400  = reinterpret_cast<fn_BstFreeRec_d400_t>(ResolveAddr(Module::Exe, 0x14001d400));
    s_fn_FreeArr30    = reinterpret_cast<fn_FreeArray30_e490_t>(ResolveAddr(Module::Exe, 0x14001e490));
    s_fn_TreeFree14c30 = reinterpret_cast<fn_TreeFree14c30_t>(ResolveAddr(Module::Exe, 0x140014c30));
    s_init = true;
}

inline void MemFree(void* p) { if (fn_MemFreeStatic) fn_MemFreeStatic(p); }

// B.3.2 bridge: the owning structs are still untyped bytes.
inline rtcoll::ArrayHdr& Arr(int64_t base, int64_t off) {
    return *reinterpret_cast<rtcoll::ArrayHdr*>(base + off);
}

} // namespace

// ============================================================================
// 0x14001a5f0 — free-list + slab-chain destructor.
//   param_1 is a pool header. Walks the in-use node chain at [param_1] (next @+0x10),
//   recycling each node onto the free list (head @param_1+0x20), decrementing the
//   in-use count (@param_1+0x34) and incrementing the free count (@param_1+0x38).
//   Then zeroes head/tail/count, and frees the slab chain (head @param_1+0x18,
//   next = first qword of each slab).
//   Field map (BYTE offsets): +0x00 inuse-head, +0x08 inuse-tail, +0x10 (int) count,
//     +0x18 slab-chain head, +0x20 free-list head, +0x34 (int) inuse-count,
//     +0x38 (int) free-count.
// 1:1 port of FUN_14001a5f0.
// ============================================================================
extern "C" void GameDtor_FreeListDtor_1a5f0(uint64_t* param_1) {
    EnsureFns();
    uint64_t* node = reinterpret_cast<uint64_t*>(param_1[0]);   // [param_1]
    while (node != nullptr) {
        uint64_t* next = reinterpret_cast<uint64_t*>(node[2]);  // node[+0x10]
        *reinterpret_cast<int*>(reinterpret_cast<int64_t>(param_1) + 0x34) -= 1; // DEC [+0x34]
        *reinterpret_cast<int*>(reinterpret_cast<int64_t>(param_1) + 0x38) += 1; // INC [+0x38]
        node[0] = param_1[4];                                   // *node = [+0x20] (free head)
        param_1[4] = reinterpret_cast<uint64_t>(node);          // [+0x20] = node
        node = next;
    }
    param_1[1] = 0;                                             // [+0x08] = 0
    param_1[0] = 0;                                             // [+0x00] = 0
    *reinterpret_cast<int*>(reinterpret_cast<int64_t>(param_1) + 0x10) = 0; // [+0x10] = 0
    // Free the slab chain: head @[+0x18], next = *slab.
    while (param_1[3] != 0) {                                   // [+0x18] != 0
        uint64_t nextSlab = *reinterpret_cast<uint64_t*>(param_1[3]); // *(slab)
        MemFree(reinterpret_cast<void*>(param_1[3]));
        param_1[3] = nextSlab;
    }
}

// ============================================================================
// 0x14001a450 — tree + array destructor.
//   param_1 layout (BYTE offsets):
//     +0x18 array data block, +0x20 (count?), +0x28 tree root, +0x30 (int) tree count.
//   Frees the tree at +0x28 (children @node+0x18 / node+0x20, via the registered
//   recursive 0x14000e320), then the array block at +0x18.
// 1:1 port of FUN_14001a450.
// ============================================================================
extern "C" void GameDtor_TreeArrayDtor_1a450(int64_t param_1) {
    EnsureFns();
    void** treeSlot = reinterpret_cast<void**>(param_1 + 0x28);
    void* root = *treeSlot;
    if (root != nullptr) {
        // FUN_14000e320(treeSlot, root->+0x18); FUN_14000e320(treeSlot, root->+0x20)
        void* childA = *reinterpret_cast<void**>(reinterpret_cast<int64_t>(root) + 0x18);
        void* childB = *reinterpret_cast<void**>(reinterpret_cast<int64_t>(root) + 0x20);
        if (s_fn_TreeFreeE320) {
            s_fn_TreeFreeE320(treeSlot, childA);
            s_fn_TreeFreeE320(treeSlot, childB);
        }
        MemFree(root);
    }
    *treeSlot = nullptr;                                        // [+0x28] = 0
    *reinterpret_cast<int*>(param_1 + 0x30) = 0;                // [+0x30] = 0
    Arr(param_1, 0x18).~Array();   // free array @[+0x18]; [+0x18] = 0; [+0x20] = 0
}

// ============================================================================
// 0x14001ced0 — cell-BST clear.
//   param_1 layout (BYTE offsets):
//     +0x00 BST root, +0x08 (int) node count, +0x10 slab-chain head,
//     +0x18 free-list head, +0x2c (int) in-use count, +0x30 (int) free count.
//   If root != 0: recursively recycles both subtrees onto the free list (via the
//   registered 0x14001d400, children @node+0x18 / node+0x20), pushes root onto the
//   free list (@+0x18) adjusting the +0x2c/+0x30 counts. Then zeroes root/count and
//   frees the slab chain (head @+0x10, next = *slab).
// 1:1 port of BuildStructure_impl_ced0 (FUN_14001ced0).
// ============================================================================
extern "C" void GameDtor_CellBstClear_1ced0(int64_t* param_1) {
    EnsureFns();
    int64_t* root = reinterpret_cast<int64_t*>(param_1[0]);    // *param_1
    if (root != nullptr) {
        // FUN_14001d400(param_1, root[3]); FUN_14001d400(param_1, root[4])
        if (s_fn_BstFreeD400) {
            s_fn_BstFreeD400(reinterpret_cast<uintptr_t>(param_1),
                             reinterpret_cast<uint64_t*>(root[3]));   // root->+0x18
            s_fn_BstFreeD400(reinterpret_cast<uintptr_t>(param_1),
                             reinterpret_cast<uint64_t*>(root[4]));   // root->+0x20
        }
        *reinterpret_cast<int*>(reinterpret_cast<int64_t>(param_1) + 0x2c) -= 1; // DEC [+0x2c]
        *reinterpret_cast<int*>(reinterpret_cast<int64_t>(param_1) + 0x30) += 1; // INC [+0x30]
        root[0] = *reinterpret_cast<int64_t*>(reinterpret_cast<int64_t>(param_1) + 0x18); // *root = [+0x18]
        *reinterpret_cast<int64_t*>(reinterpret_cast<int64_t>(param_1) + 0x18) =
            reinterpret_cast<int64_t>(root);                  // [+0x18] = root
    }
    param_1[0] = 0;                                            // *param_1 = 0
    *reinterpret_cast<int*>(reinterpret_cast<int64_t>(param_1) + 0x08) = 0; // [+0x08] = 0
    // Free the slab chain at +0x10 (next = *slab).
    while (*reinterpret_cast<int64_t*>(reinterpret_cast<int64_t>(param_1) + 0x10) != 0) {
        int64_t slab = *reinterpret_cast<int64_t*>(reinterpret_cast<int64_t>(param_1) + 0x10);
        int64_t nextSlab = *reinterpret_cast<int64_t*>(slab);  // *slab
        MemFree(reinterpret_cast<void*>(slab));
        *reinterpret_cast<int64_t*>(reinterpret_cast<int64_t>(param_1) + 0x10) = nextSlab;
    }
}

// ============================================================================
// 0x14001dac0 — array-free, element stride 0x18.
//   param_1: +0x00 array base, +0x08 (int) count.
//   For each of `count` elements (base + i*0x18), frees element->+0x08 (read as the
//   first qword of the +0x08 sub-record), then zeroes that record's two qwords.
//   Then frees the array base and zeroes base/count.
//   (Disasm: RBX = base+0x08; frees [RBX], zeroes [RBX]/[RBX+8]; RBX += 0x18 per iter.)
// 1:1 port of FUN_14001dac0.
// ============================================================================
extern "C" void GameDtor_FreeArray18_1dac0(int64_t* param_1) {
    EnsureFns();
    int64_t base = param_1[0];                                 // *param_1
    int64_t count = static_cast<int>(param_1[1] & 0xffffffff); // (int)[+0x08], sign-extended
    if (count > 0) {
        uint64_t* cur = reinterpret_cast<uint64_t*>(base + 0x08); // RBX = base + 8
        do {
            Arr(reinterpret_cast<int64_t>(cur), 0).~Array();   // free [RBX]; [RBX] = [RBX+8] = 0
            cur = reinterpret_cast<uint64_t*>(reinterpret_cast<int64_t>(cur) + 0x18); // RBX += 0x18
            --count;
        } while (count != 0);
    }
    Arr(reinterpret_cast<int64_t>(param_1), 0).~Array();        // free base; zero header
}

// ============================================================================
// 0x14001e3a0 — array-free, element stride 0x20.
//   Same shape as 0x14001dac0 but the freed sub-record sits at element + 0x10 and the
//   stride is 0x20. (Disasm: RBX = base+0x10; frees [RBX], zeroes [RBX]/[RBX+8];
//   RBX += 0x20 per iter.) Then frees base and zeroes base/count.
// 1:1 port of FUN_14001e3a0.
// ============================================================================
extern "C" void GameDtor_FreeArray20_1e3a0(int64_t* param_1) {
    EnsureFns();
    int64_t base = param_1[0];                                 // *param_1
    int64_t count = static_cast<int>(param_1[1] & 0xffffffff); // (int)[+0x08]
    if (count > 0) {
        uint64_t* cur = reinterpret_cast<uint64_t*>(base + 0x10); // RBX = base + 0x10
        do {
            Arr(reinterpret_cast<int64_t>(cur), 0).~Array();   // free [RBX]; [RBX] = [RBX+8] = 0
            cur = reinterpret_cast<uint64_t*>(reinterpret_cast<int64_t>(cur) + 0x20); // RBX += 0x20
            --count;
        } while (count != 0);
    }
    Arr(reinterpret_cast<int64_t>(param_1), 0).~Array();        // free base; zero header
}

// ============================================================================
// 0x1400a8570 — Game sub-object (+0x70) destructor.
//   Tears down the path/command pools held at +0x08 .. +0xc8 of the sub-object.
//   Sequence (BYTE offsets into param_1):
//     1a5f0(param_1 + 0xc8)                         free-list dtor
//     MemFree [+0xb8]; [+0xb8]=[+0xc0]=0
//     MemFree [+0xa8]; [+0xa8]=[+0xb0]=0
//     MemFree [+0x98]; [+0x98]=[+0xa0]=0
//     MemFree [+0x88]; [+0x88]=[+0x90]=0
//     1e3a0(param_1 + 0x78)                          array-free stride 0x20
//     1e490(<unused>, [+0x68], (int)[+0x70])         array-free stride 0x30
//     MemFree [+0x68]; [+0x68]=[+0x70]=0
//     ced0(param_1 + 0x18)                           cell-BST clear
//     MemFree [+0x08]; [+0x08]=[+0x10]=0
// 1:1 port of TradeComponent_impl_8570 (FUN_1400a8570).
// ============================================================================
extern "C" void GameDtor_SubObjDtor_a8570(int64_t param_1) {
    EnsureFns();
    GameDtor_FreeListDtor_1a5f0(reinterpret_cast<uint64_t*>(param_1 + 0xc8));

    Arr(param_1, 0xb8).~Array();   // MemFree [+0xb8]; [+0xb8]=[+0xc0]=0
    Arr(param_1, 0xa8).~Array();   // MemFree [+0xa8]; [+0xa8]=[+0xb0]=0
    Arr(param_1, 0x98).~Array();   // MemFree [+0x98]; [+0x98]=[+0xa0]=0
    Arr(param_1, 0x88).~Array();   // MemFree [+0x88]; [+0x88]=[+0x90]=0

    GameDtor_FreeArray20_1e3a0(reinterpret_cast<int64_t*>(param_1 + 0x78));

    if (s_fn_FreeArr30)
        s_fn_FreeArr30(0, static_cast<uintptr_t>(*reinterpret_cast<int64_t*>(param_1 + 0x68)),
                       *reinterpret_cast<int*>(param_1 + 0x70));

    // The element step (1e490, stride 0x30) ran above; this is the outer header.
    Arr(param_1, 0x68).~Array();   // MemFree [+0x68]; [+0x68]=[+0x70]=0

    GameDtor_CellBstClear_1ced0(reinterpret_cast<int64_t*>(param_1 + 0x18));

    Arr(param_1, 0x08).~Array();   // MemFree [+0x08]; [+0x08]=[+0x10]=0
}

// ============================================================================
// 0x1400a6720 — worker-queue Thread object destructor.
//   thread layout (BYTE offsets):
//     +0x00 vtable, +0x30 CriticalSection, +0x60 cmd-node chain head (next @node+0x18),
//     +0x68 (?), +0x70 (int) count, +0x78 base of a {+0x08 array, +0x18 cell-BST, ...}
//       sub-block, +0x80 (under +0x78) ptr freed, +0x90/+0x18 cell-BST cleared,
//     +0x158 array block (under +0x78+0xe0), +0x160 array-free stride 0x18 target.
//   Sequence:
//     [thread] = &PTR_FUN_14015ef18                  set (now-defunct) vtable
//     1dac0(thread + 0x160)                           array-free stride 0x18
//     MemFree [thread + 0x158]   (= [(+0x78)+0xe0])
//     ced0((thread+0x78) + 0x18 = thread + 0x90)      cell-BST clear
//     MemFree [(+0x78)+0x08 = thread + 0x80]; zero +0x80/+0x88
//     walk cmd-node chain @+0x60 (next @node+0x18), MemFree each; zero +0x68/+0x60, +0x70
//     CriticalSection::~CriticalSection(thread + 0x30)
//     Thread::~Thread(thread)                         (tail-call)
// 1:1 port of BuildStructure_impl_6720 (FUN_1400a6720).
//
// ENDGAME B.3.8 LOOP: the restamp stores PathfinderThread's table (0x14015ef18 is
// that class). Nothing reads it before the Thread/CS dtors run (they don't touch
// +0x00); it was a null store while 0x14015ef18 was unregistered.
//
// ENDGAME B.3.9 VS (B9-13 foreign site, SYS W3): 0x1400a6720 IS
// `PathfinderThread::~PathfinderThread` and is now that member. The table store
// on entry is the compiler's, and the tail call is the compiler's chain into
// `System::Thread::~Thread()` -- it was the `??1Thread@System@@QEAA@XZ` door,
// which re-stamped the minted base table and ran the same body; the door and
// the minted row are retired.
// ============================================================================
PathfinderThread::~PathfinderThread() {
    EnsureFns();
    const int64_t thread = reinterpret_cast<int64_t>(this);

    GameDtor_FreeArray18_1dac0(reinterpret_cast<int64_t*>(thread + 0x160));

    // RBX = thread + 0x78 in the original; the next three derefs are off RBX.
    int64_t rbx = thread + 0x78;
    MemFree(*reinterpret_cast<void**>(rbx + 0xe0));            // MemFree [thread + 0x158]

    GameDtor_CellBstClear_1ced0(reinterpret_cast<int64_t*>(rbx + 0x18)); // ced0(thread + 0x90)

    Arr(rbx, 0x08).~Array();   // MemFree [thread + 0x80]; [thread + 0x80] = [thread + 0x88] = 0

    // Walk the cmd-node chain at +0x60 (RSI = thread + 0x60); next link @node+0x18.
    int64_t rsi = thread + 0x60;
    void* cur = *reinterpret_cast<void**>(rsi);
    while (cur != nullptr) {
        void* next = *reinterpret_cast<void**>(reinterpret_cast<int64_t>(cur) + 0x18);
        MemFree(cur);
        cur = next;
    }
    *reinterpret_cast<int64_t*>(rsi + 0x08) = 0;              // [thread + 0x68] = 0
    *reinterpret_cast<int64_t*>(rsi) = 0;                    // [thread + 0x60] = 0
    *reinterpret_cast<int*>(rsi + 0x10) = 0;                 // [thread + 0x70] = 0

    if (s_fn_CSDtor) s_fn_CSDtor(reinterpret_cast<void*>(thread + 0x30));
    // tail-call: System::Thread::~Thread() -- the compiler's base chain.
}

// ============================================================================
// 0x1400a6690 — worker-queue drain + worker-thread stop.
//   thread layout: +0x08 (byte) flags (bit0 = running), +0x30 CriticalSection,
//     +0x70 (int) queue count.
//   Loop:
//     Lock(thread + 0x30)
//     if (count > 0) { if ((flags & 1) == 0) Thread::Start(thread); count = [+0x70]; }
//     if (count == 0) { Unlock; break; }
//     YieldToThreads(*g_pAppSingleton); Unlock; loop
//   After loop: Unlock; WaitForFinish(thread)   (tail-call)
// 1:1 port of FUN_1400a6690.
//
// SPIN/WAIT: this blocks until the worker thread drains the queue to 0. It explicitly
// starts the worker (Thread::Start) when work is pending and the thread isn't running.
// Shutdown-only; not autostart-exercised. If in standalone the worker never drains
// (e.g. Thread::Start is a no-op), this would spin — see FINAL REPORT.
// ============================================================================
extern "C" void GameDtor_DrainQueue_a6690(int64_t thread) {
    EnsureFns();
    for (;;) {
        reinterpret_cast<System::CriticalSection*>(reinterpret_cast<void*>(thread + 0x30))->Lock();   // B.3.4b bridge
        int count = *reinterpret_cast<int*>(thread + 0x70);
        if (count > 0) {
            if ((*reinterpret_cast<uint8_t*>(thread + 0x08) & 1) == 0) {
                reinterpret_cast<System::Thread*>(reinterpret_cast<void*>(thread))->Start();   // B.3.4b bridge
            }
            count = *reinterpret_cast<int*>(thread + 0x70);
        }
        if (count == 0) break;
        // YieldToThreads(Application::_singleton)
        ::Application::Application::_singleton->YieldToThreads();
        reinterpret_cast<System::CriticalSection*>(reinterpret_cast<void*>(thread + 0x30))->Unlock();   // B.3.4b bridge
    }
    reinterpret_cast<System::CriticalSection*>(reinterpret_cast<void*>(thread + 0x30))->Unlock();   // B.3.4b bridge
    reinterpret_cast<System::Thread*>(reinterpret_cast<void*>(thread))->WaitForFinish(); // tail-call  B.3.4b bridge
}

// ============================================================================
// 0x14009ed60 — Game::Dtor.
//   param_1 = Game* (this).  Field map (BYTE offsets):
//     +0x08 Picking::Scene sub-object (dtor at tail), +0x68 worker-queue Thread*,
//     +0x70 sub-object pointer (a8570), +0x78 tree+array pool (1a450),
//     +0x80 array block (freed), +0xa8 free-list pool (1a5f0),
//     +0xe8 tree node (children @+0x10/+0x18 freed via 14c30), +0xf0 (int) tree count.
//   Sequence:
//     if ([+0x68]) { a6690([+0x68]); if ([+0x68]) { a6720([+0x68]); MemFree([+0x68]); } }
//     if ([+0x70]) { a8570([+0x70]); MemFree([+0x70]); }
//     if ([+0x78]) { 1a450([+0x78]); MemFree([+0x78]); }
//     node = [+0xe8]; if (node) { 14c30(&[+0xe8], node->+0x10); 14c30(&[+0xe8], node->+0x18); MemFree(node); }
//     [+0xe8] = 0; [+0xf0] = 0;
//     1a5f0(param_1 + 0xa8)
//     MemFree([+0x80]); [+0x80]=[+0x88]=0
//     Picking::Scene::~Scene(param_1 + 0x08)         (tail-call)
// 1:1 port of FUN_14009ed60 (Game::Dtor).
// ============================================================================
extern "C" void Game_Dtor_9ed60(int64_t param_1) {
    EnsureFns();

    int64_t queue = *reinterpret_cast<int64_t*>(param_1 + 0x68);
    if (queue != 0) {
        GameDtor_DrainQueue_a6690(queue);
        queue = *reinterpret_cast<int64_t*>(param_1 + 0x68);   // re-read [RBX + 0x68]
        if (queue != 0) {
            reinterpret_cast<PathfinderThread*>(queue)->~PathfinderThread();   // 0x1400a6720
            MemFree(reinterpret_cast<void*>(queue));
        }
    }

    void* subObj = *reinterpret_cast<void**>(param_1 + 0x70);
    if (subObj != nullptr) {
        GameDtor_SubObjDtor_a8570(reinterpret_cast<int64_t>(subObj));
        MemFree(subObj);
    }

    void* treeArr = *reinterpret_cast<void**>(param_1 + 0x78);
    if (treeArr != nullptr) {
        GameDtor_TreeArrayDtor_1a450(reinterpret_cast<int64_t>(treeArr));
        MemFree(treeArr);
    }

    // +0xe8 tree node: free children @+0x10/+0x18 via the registered 0x140014c30
    // (which takes (&slot, child) and recurses children @child+0x10 / child+0x18).
    void** nodeSlot = reinterpret_cast<void**>(param_1 + 0xe8);
    void* node = *nodeSlot;
    if (node != nullptr) {
        void* childA = *reinterpret_cast<void**>(reinterpret_cast<int64_t>(node) + 0x10);
        void* childB = *reinterpret_cast<void**>(reinterpret_cast<int64_t>(node) + 0x18);
        if (s_fn_TreeFree14c30) {
            s_fn_TreeFree14c30(nodeSlot, childA);
            s_fn_TreeFree14c30(nodeSlot, childB);
        }
        MemFree(node);
    }
    *nodeSlot = nullptr;                                        // [+0xe8] = 0
    *reinterpret_cast<int*>(param_1 + 0xf0) = 0;                // [+0xf0] = 0

    GameDtor_FreeListDtor_1a5f0(reinterpret_cast<uint64_t*>(param_1 + 0xa8));

    Arr(param_1, 0x80).~Array();   // free [+0x80]; [+0x80]=[+0x88]=0

    if (s_fn_SceneDtor) s_fn_SceneDtor(reinterpret_cast<void*>(param_1 + 0x08)); // tail-call
}
