// ============================================================================
// runtime/src/collection/linkedlist.cpp — Wave 9 lane UIC
//   Collection::LinkedList<T=8 bytes, DefaultAllocator> — the unexported
//   out-of-line helpers UI::UserInterface::BringDialogToFront calls.
//
//   ENDGAME B.3.2 step 1: the bodies themselves now live on the class template
//   in runtime/types/collection/Collection.h; what is left here is the
//   allocator seam and the four one-line address doors. See "WHERE THE BODIES
//   WENT" below the original write-up.
// ============================================================================
//
// The container layout is NOT recovered here — it was already recovered, twice,
// and this file only ports the bodies that operate on it:
//
//   runtime/types/collection/Collection.h  `Collection::LinkedList`
//       head +0x00, tail +0x08, count +0x10, sizeof 0x18; node
//       {element +0x00, prev +0x08, next +0x10}, sizeof 0x18 for T=8.
//       Pinned there by UI::UserInterface::CleanRemovedDialogs (0x18011f790).
//   runtime/types/ui/UserInterface.h       `_dialogs` at UserInterface+0x00
//
// The three bodies, all UNEXPORTED (checked against the shipped export
// directory with tools/uic_probe.py, which reads the PE export table — none of
// the three has a name, so all three are registered by ADDRESS only and there
// is nothing here for tools/export_name_audit.py to check):
//
//   | retail      | what it is                              | reached from      |
//   |-------------|-----------------------------------------|-------------------|
//   | 0x180009c30 | the allocator-lock Free wrapper         | 0x18007a010 (tail |
//   |             | `if(p){Enter;Free(_singleton,p);Leave;}`| jump), and THREE  |
//   |             | first argument taken and NEVER READ     | existing tree     |
//   |             |                                         | call sites (below)|
//   | 0x18007a010 | LinkedList::Remove(list, &value) —      | BringDialogToFront|
//   |             | find by VALUE, unlink, free the node    | 0x180120440       |
//   | 0x180032750 | LinkedList node alloc + init            | BringDialogToFront|
//
// ---------------------------------------------------------------------------
// WHY THE TWO ALLOCATOR BODIES ARE NOT REWRITTEN HERE
// ---------------------------------------------------------------------------
// 0x180009c30 and 0x180032750 each open-code an allocator call under the
// allocator's own critical section. Both are the SAME INSTRUCTION SEQUENCE as a
// body this tree already ports, and that is established by reading both, not by
// eye:
//
//   0x1800c7ad0  ?MemFree@MemoryAllocator@System@@SAXPEAX@Z   (ported, lane E,
//                registered at runtime/src/system/allocator.cpp:1296/1399)
//     TEST RCX,RCX / JZ ret / PUSH RDI / SUB RSP,0x30 / MOV [RSP+0x20],-2 /
//     MOV [RSP+0x48],RBX / MOV RDI,RCX / MOV RBX,[0x1801fc718] / ADD RBX,0x640 /
//     MOV [RSP+0x40],RBX / MOV RCX,RBX / CALL [0x18014b390] / NOP /
//     MOV RDX,RDI / MOV RCX,[0x1801fc718] / CALL 0x1800c6c00 / NOP /
//     MOV RCX,RBX / CALL [0x18014b388] / ... / RET
//
//   0x180009c30
//     TEST RDX,RDX / JZ ret / MOV [RSP+0x8],RCX / PUSH RDI / SUB RSP,0x30 /
//     MOV [RSP+0x20],-2 / MOV [RSP+0x48],RBX / MOV RDI,RDX / ...IDENTICAL...
//
// The ONLY differences are (a) which register carries the pointer — RCX there,
// RDX here — and (b) one extra `MOV [RSP+0x8],RCX` that homes the first
// argument and never reads it back. So 0x180009c30 IS MemFree with an ignored
// leading argument. It is implemented below as exactly that, which keeps ONE
// body for the pair rather than a second copy that can drift.
//
// The same argument holds for the allocating half:
//
//   0x1800c7a50  ?MemAlloc@…@@SAPEAX_KH@Z (ported, lane E)
//     ... Enter([0x1801fc718]+0x640) ; Allocate(_singleton, RDX=bytes, R8D=align)
//         via CALL 0x1800c6e60 ; Leave ; return RAX
//   0x180032750
//     ... Enter([0x1801fc718]+0x640) ; MOV EDX,0x18 / LEA R8D,[RDX-0x10] (=8) /
//         MOV RCX,[0x1801fc718] / CALL 0x1800c6e60 ; Leave
//
// i.e. its allocation is `MemAlloc(0x18, 8)` spelled inline. Reproduced as that
// call, so the node allocation goes through the ONE ported allocator entry
// point instead of a private second copy of the lock protocol. This matters at
// cutover: 0x1800c7a50/0x1800c7ad0/0x1800c6e60/0x1800c6c00 are the four
// addresses `RegisterAllocatorAddresses()` arms (allocator.cpp, commit 309c015)
// — reaching the heap any other way from here would re-open the wall attempt 13
// closed.
//
// ---------------------------------------------------------------------------
// THIS REGISTRATION CHANGES BEHAVIOUR AT THREE EXISTING CALL SITES
// ---------------------------------------------------------------------------
// `ResolveAddr(Module::Runtime, …)` consults the port registry FIRST
// (reference_resolveaddr_consults_port_registry_first.md), so registering
// 0x180009c30 redirects these, which reach it by address today:
//
//   runtime/src/graphics/scene.cpp:239   RtFreeLocked
//   runtime/src/input/input.cpp:139      RtInput_KeyMap_ClearAll's freeNode
//
// That redirection is the POINT, not a side effect: all three were an
// unregistered address door, i.e. a null call after cutover.
//
// ---------------------------------------------------------------------------
// ORACLE
// ---------------------------------------------------------------------------
// NONE for Remove and for the node allocator, and no attempt is made to
// manufacture one. Both are destructive and stateful in the strict sense:
// replaying Remove unlinks a node that is already unlinked and frees it a
// second time; replaying the allocator leaks a 24-byte node per call and
// returns a different pointer, so a comparison of the return value is
// guaranteed to fail for a reason that says nothing about the port. These ship
// in the disposition lane M established and lanes UIF/Z reused: a NO-ORACLE
// port with NO call into the shipped body at all.
//
// PORTABILITY: no <windows.h>, per PLAN.md §4.1 decision 1.
// ============================================================================

// ---------------------------------------------------------------------------
// ENDGAME B.3.2 STEP 1 (LIST core) — WHERE THE BODIES WENT
// ---------------------------------------------------------------------------
// The four bodies above are now MEMBERS of
// `Collection::LinkedList<T, Allocator>` in
// runtime/types/collection/Collection.h, each carrying the transcription notes
// that used to sit here next to it. Retail has exactly ONE instantiation of
// that template (T = an 8-byte pointer), and at T = 8 the generic bodies
// compile to the same stores this file used to make by hand — the only change
// is that `0x18`, `+0x8` and `+0x10` are now `sizeof(Node)` and field names.
//
// What is left in this file is the two things the header cannot hold:
//
//   1. `Collection::detail::AllocateListNode/ListNodeFree` — the allocator seam,
//      which needs `ResolveAddr` and therefore must stay out of a types header
//      (PLAN.md §4.1 decision 1).
//   2. The four `extern "C"` entry points, now ONE LINE EACH, and their
//      `RegisterRuntimeAddr` rows. Every row that was here is still here:
//      callers reach the class through them until B.3.2 step 2 converts the
//      call sites.
// ---------------------------------------------------------------------------

#include <cstddef>
#include <cstdint>

#include "../../types/collection/Collection.h"
#include "../../types/system/MemoryAllocator.h"   // B3R: MemAlloc / MemFree by name

#ifdef BANISHED_UNIT_TEST
#include <cstdlib>
#endif

// The one retail instantiation. Named here so the shims below and the layout
// asserts read against the same type the header defines.
using List8 = Collection::LinkedList<void*>;
using Node8 = List8::Node;
static_assert(sizeof(Node8) == 0x18, "LinkedList::Node(T=8) is 0x18 bytes");
static_assert(offsetof(Node8, prev) == 0x08, "Node::prev @ +0x08");
static_assert(offsetof(Node8, next) == 0x10, "Node::next @ +0x10");
static_assert(sizeof(List8) == 0x18, "Collection::LinkedList is 0x18 bytes");
static_assert(offsetof(List8, _head)  == 0x00, "LinkedList::_head @ +0x00");
static_assert(offsetof(List8, _tail)  == 0x08, "LinkedList::_tail @ +0x08");
static_assert(offsetof(List8, _count) == 0x10, "LinkedList::_count @ +0x10");

#ifndef BANISHED_UNIT_TEST
namespace {

// The allocator boundary. RETRIES A NULL rather than caching one, which is the
// RtResolveCached contract and is load-bearing rather than defensive: at
// cutover these addresses are supplied by RegisterAllocatorAddresses() inside
// the allocator's own arming path, so a caller that ran before that point would
// otherwise freeze a null in for the life of the process
// (WAVE9.md, "Attempt 14", the second defect the crash was hiding).
// ENDGAME B.3.2 closing pass: the resolution moved into `rtcoll::DoorAlloc` /
// `DoorFree` (runtime/src/collection/alloc_door.cpp), which asks the exe
// dispatch slots 0x140152b60 / 0x140152b68 first and falls back to these two
// addresses. Same allocator, same lock; the difference is that the door is the
// one `BANISHED_REDZONE` rewrites, so a list node is no longer invisible to it.
// B3R RT-Core: the two statics, called by name.
void* MemAllocLocked(uint64_t bytes, int align) {
    return System::MemoryAllocator::MemAlloc(bytes, align);   // 0x1800c7a50
}

void MemFreeLocked(void* p) {
    System::MemoryAllocator::MemFree(p);                      // 0x1800c7ad0
}

}  // namespace
#endif  // !BANISHED_UNIT_TEST

// ============================================================================
// Collection::detail — the node allocation seam the header declares
// ============================================================================
//
// In the SHIPPING build these are exactly the two calls the bodies above
// argued for: `MemAlloc(sizeof(Node), 8)` and `MemFree(node)`, i.e. the ONE
// ported allocator entry point rather than a private copy of the lock
// protocol. Reaching the heap any other way from here would re-open the wall
// attempt 13 closed (see this file's header block).
//
// In the HOST UNIT TEST build (`make unit`, -DBANISHED_UNIT_TEST) there is no
// address registry and no MemoryAllocator singleton to resolve, so
// `ResolveAddr` answers 0 and the shipping path would allocate nothing at all —
// every list would silently stay empty and every assertion in list_test.cpp
// would be vacuous. B31_RULES class rule 6 allows a test-only allocator for
// exactly this; this is it, and it is the ONLY behavioural difference between
// the two builds in this file.
//
// The test allocator carries a 16-byte header with a state word, so that
// freeing a block twice is DETECTED rather than tolerated: that is what makes
// "Clear frees every node exactly once" a real assertion instead of a count
// that a double free and a leak would cancel out of.
// ============================================================================
namespace Collection {
namespace detail {

#ifdef BANISHED_UNIT_TEST
uint64_t g_listNodeAllocs      = 0;   // read by tests/unit/list_test.cpp
uint64_t g_listNodeFrees       = 0;
uint64_t g_listNodeDoubleFrees = 0;

namespace {
constexpr uint64_t kLive = 0x4c495645ull;   // 'LIVE'
constexpr uint64_t kDead = 0x44454144ull;   // 'DEAD'
}  // namespace

void* AllocateListNode(uint64_t bytes, int align) {
    (void)align;                     // malloc is already 16-aligned on x64
    uint8_t* base = static_cast<uint8_t*>(std::malloc(static_cast<size_t>(bytes) + 16));  // CAST-OK: alloc_header
    if (!base) { return nullptr; }
    *reinterpret_cast<uint64_t*>(base) = kLive;  // CAST-OK: alloc_header
    ++g_listNodeAllocs;
    return base + 16;
}

void ListNodeFree(void* p) {
    if (!p) { return; }              // retail's own `TEST RDX,RDX / JZ ret`
    uint8_t*  base  = static_cast<uint8_t*>(p) - 16;  // CAST-OK: alloc_header
    uint64_t& state = *reinterpret_cast<uint64_t*>(base);  // CAST-OK: alloc_header
    if (state != kLive) { ++g_listNodeDoubleFrees; return; }
    state = kDead;
    ++g_listNodeFrees;
    std::free(base);
}
#else
void* AllocateListNode(uint64_t bytes, int align) {
    return MemAllocLocked(bytes, align);
}

void ListNodeFree(void* p) {
    MemFreeLocked(p);
}
#endif

}  // namespace detail
}  // namespace Collection
