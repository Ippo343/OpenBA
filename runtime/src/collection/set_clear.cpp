// ============================================================================
// runtime/src/collection/set_clear.cpp — Wave 9 lane SAVE2
//   `Collection::Set<Pair<String,String>>::Clear` and the two bodies under it.
// ============================================================================
//
// WHY THIS IS A BOOT-PATH ROW AND NOT A CORNER
// --------------------------------------------
// `IO::Persistence::ReadSettings` is ported (Wave 9 lane BOOT2,
// runtime/src/io/persistence_registry_win.cpp) and its FIRST act is
// `Map_Clear(&this->_settings)` at retail 0x1800b84db — an address door onto
// 0x18000f7d0. That file's own comment says the door is deliberately
// unguarded so that its absence dies loudly:
//
//     "with the DLL absent it hands back 0 and the check would turn 'no port
//      for Map_Clear / Map_Insert yet' into SILENCE"
//
// So at cutover this is an EXEC-of-0 on the settings read, which runs from
// `Engine::Startup`. `tools/addr_door_audit.py` listed it as unregistered and
// no lane owned it. It is ported here.
//
// THE THREE BODIES, read at their addresses (disassembly, not Ghidra's C —
// reference_ghidra_c_output_vs_disassembly.md)
// ----------------------------------------------------------------------------
//   0x18000f7d0  Set<Pair<String,String>>::Clear(this)
//                  n = this->root
//                  if (n) {
//                      DestroySubtree(this, n->left);      ; RCX = this
//                      DestroySubtree(this, n->right);     ; RCX = this
//                      DestroyElement(n);
//                      NodeFree(<junk>, n);
//                  }
//                  this->root = 0; this->count = 0;
//
//   0x18000f890  DestroySubtree(ignored, n)
//                  if (!n) return;                          ; the test is the
//                                                           ; FIRST instruction,
//                                                           ; before the frame
//                  DestroySubtree(ignored, n->left);
//                  DestroySubtree(ignored, n->right);
//                  DestroyElement(n);
//                  NodeFree(<junk>, n);
//
//   0x18000fa90  DestroyElement(n)   — ~Pair<System::String, System::String>
//                  release the String at n+0x08, THEN the one at n+0x00.
//
// A POST-ORDER DESTROY. Both children first, then the node. The order matters
// for one reason only and it is worth naming: `NodeFree` hands the block back
// to the allocator, which writes a boundary tag INTO it, so touching
// `n->left`/`n->right` after the free reads a free-list link. Retail loads
// both children before the free in every body; so does this.
//
// THE FIRST ARGUMENT OF BOTH HELPERS IS NEVER READ, and retail proves it by
// passing junk. At 0x18000f8d1 (`MOV RDX,RBX ; CALL 0x180009c30`) RCX holds
// whatever 0x18000fa90 left in it — the element destructor's epilogue restores
// RBX and RSP and does not touch RCX. Same at 0x18000f812 in `Clear`. This is
// the `FUN_180023060` shape (reference_read_the_callee_before_holding_back_on_
// an_arg.md): a register that is `this` at one call site and garbage at another
// cannot be a parameter the callee reads. The ports below thread the Set
// through anyway, because that is what retail's register happens to hold most
// often and because `SetNodeFree`'s contract (set_avl.h) already says the first
// argument is ignored.
//
// THE ELEMENT DESTRUCTOR IS `ReleaseStringsCore` WITH ONE ELEMENT
// ---------------------------------------------------------------
// 0x18000fa90 is instruction for instruction the "Body B" string-release idiom
// `runtime/src/collection/array_destroy.cpp` already ports and probes: decrement
// `[p-4]`; if it did not reach zero store NULL into the slot; if it did, store
// `p-8` into the slot and free that. Both arms, including the asymmetry that
// leaves the slot holding two different things, and the offsets in RETAIL'S OWN
// ORDER — +0x08 (the value) BEFORE +0x00 (the key). It is called rather than
// restated, so the two cannot drift apart; a local copy would have been a second
// place to fix the next time the idiom is corrected.
//
// That reuse is also why the element type is safe to pin here. `~Pair<String,
// String>` is instantiation-specific — it is the one member of this trio that
// knows what the element IS — but "two Strings at +0x00 and +0x08, stride 0x10"
// is exactly what the settings map's node holds
// (runtime/types/io/Persistence.h) and exactly what MSVC folded at this address.
//
// ORACLE: DIFFERENTIAL ON THE ALLOCATOR'S OWN COUNTERS, against the shipped
// bodies, on two synthetic trees. See set_clear_probe.cpp. A destructor cannot
// be replayed (SYS2 wrote that up for ~SaveLoad) — but two IDENTICALLY BUILT
// private trees can be destroyed by the two implementations side by side, and
// `MemoryAllocator::allocCount` / `allocatedBytes` (+0x638 / +0x630, pinned by
// Wave 2) say exactly how many blocks and how many bytes each side handed back.
// That is a real comparison against retail, not a self-check.
// ============================================================================

#include <cstddef>
#include <cstdint>

#include "../../funcs/rt_port.h"
#include "set_avl.h"
#include "array_destroy_internal.h"


namespace rtsetclear {

using rtsetavl::Node;
using rtsetavl::Set;

// ---------------------------------------------------------------------------
// Effectiveness counters, kept INSIDE the bodies under test.
//
// WAVE9.md's rule, paid for three times by lane PKG2's control: count branches
// taken inside the function, not proxies observed from outside it. Every proxy
// available here is either structurally pinned (the settings map's real shape is
// whatever the registry happens to hold) or swamped by the common path. These
// four say which arms actually ran:
//
//   g_subtreeEnter  recursive entries with a non-null node
//   g_subtreeNull   recursive entries that returned on the first instruction
//   g_strFreed      string releases whose refcount reached 0  (the freeing arm)
//   g_strShared     string releases whose refcount did not    (the NULLing arm)
//
// Four unconditional increments on a path that is already calling the
// allocator; they are not gated on the probe because a counter that only exists
// while the probe is armed cannot report on the REAL calls, and the real calls
// are what the census asks about.
long g_subtreeEnter = 0;
long g_subtreeNull  = 0;
long g_strFreed     = 0;
long g_strShared    = 0;

// ---------------------------------------------------------------------------
// ~Pair<System::String, System::String>, retail 0x18000fa90.
//
// stride is irrelevant at count == 1 and is written as the real element size so
// the row reads correctly; `nOffs = 2` with `{0x08, 0x00}` is retail's order,
// value before key. NOT sorted (array_destroy_internal.h says why in general:
// three rows there walk their offsets downward and the order reaches the
// allocator).
// ---------------------------------------------------------------------------
const rtcoll::DestroySpec kD_PairStrStr = {
    "2 Strings @+0x008,+0x000, stride 0x010", 0x010, 2, { 0x008, 0x000 }
};

void DestroyElement(void* node) {
    // The arm each release took is readable from the slot afterwards, and that
    // asymmetry is deliberate in Body B: NULL means the string survived
    // (refcount did not reach zero), a non-null `p - 8` means it was freed.
    // Reading it here is how the arm counts come from INSIDE the body rather
    // than from a guess about the corpus.
    uint8_t* e = static_cast<uint8_t*>(node);
    void* const before8 = *reinterpret_cast<void**>(e + 0x08);
    void* const before0 = *reinterpret_cast<void**>(e + 0x00);
    rtcoll::ReleaseStringsCore(e, 1, &kD_PairStrStr);
    if (before8) (*reinterpret_cast<void**>(e + 0x08) ? ++g_strFreed : ++g_strShared);
    if (before0) (*reinterpret_cast<void**>(e + 0x00) ? ++g_strFreed : ++g_strShared);
}

// ---------------------------------------------------------------------------
// retail 0x18000f890 — the recursive post-order destroy.
// ---------------------------------------------------------------------------
void DestroySubtree(void* ignored, Node* n) {
    if (!n) { ++g_subtreeNull; return; }  // TEST RDX,RDX / JZ, first instruction
    ++g_subtreeEnter;
    DestroySubtree(ignored, n->left);     // [n+0x18]
    DestroySubtree(ignored, n->right);    // [n+0x20]
    DestroyElement(n);
    rtsetavl::SetNodeFree(ignored, n);    // 0x180009c30, first arg ignored
}

// ---------------------------------------------------------------------------
// retail 0x18000f7d0 — Clear.
//
// The `root == 0` arm still stores the two zeroes (retail branches to a second
// copy of them at 0x18000f824 rather than skipping), so an already-empty Set is
// written to. Reproduced: it is one qword and one dword either way, and a
// "cleanup" that skipped it would differ from retail on any Set whose count was
// stale.
// ---------------------------------------------------------------------------
void Clear(Set* set) {
    // Self-scheduling, the shape lane PICK's grid probe uses and the shape lane
    // PKG2's null+0x640 crash forced: a probe that ALLOCATES cannot run from a
    // *PortInit, because that runs before the allocator publishes its
    // singleton. The first real Clear is provably past that point — the caller
    // is IO::Persistence::ReadSettings, inside Engine::Startup.

    Node* n = set->root;
    if (n) {
        DestroySubtree(set, n->left);
        DestroySubtree(set, n->right);
        DestroyElement(n);
        rtsetavl::SetNodeFree(set, n);
    }
    set->root  = nullptr;
    set->count = 0;
}

}  // namespace rtsetclear

// ============================================================================
// Registration
//
// ADDRESS DOORS ONLY — none of the three is exported. Checked against the
// shipped export directory with pefile before this block was written (no name
// resolves to 0x18000f7d0, 0x18000f890 or 0x18000fa90), and said explicitly
// rather than left implied.
//
// GREPPED FIRST. Three lanes this wave nearly duplicated an existing port, so:
// before writing a line, all three addresses were grepped across code/,
// engine/, runtime/, standalone/ and dx11/. The only textual hits were
// persistence_registry_win.cpp's call site and its comments — no registration
// anywhere, so there is no shadowed binding here
// (set_avl.cpp's banner records what one looked like when there was).
// ============================================================================
int RtCollectionSetClearRegister() {
    int n = 0;
    RegisterRuntimeAddr(0x18000f7d0, (void*)&rtsetclear::Clear);          ++n;
    RegisterRuntimeAddr(0x18000f890, (void*)&rtsetclear::DestroySubtree); ++n;
    RegisterRuntimeAddr(0x18000fa90, (void*)&rtsetclear::DestroyElement); ++n;
    return n;
}
