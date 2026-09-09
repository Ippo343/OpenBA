// ============================================================================
// runtime/src/collection/array.cpp — Wave 9 lane Q: Collection::Array<T>
// ============================================================================
//
// SCOPE: lane D's jobs J1 (`~Array`, 11 addresses) and J3 (`SetSize`, 16
// addresses) — 27 of the 66 unnamed DLL internals in
// docs/runtime_port/wave9_internals.csv, retired by TWO algorithms.
//
// `Collection::Array<T>` is a header-only template with ZERO exports in either
// shipped build (docs/runtime_port/WAVE9_LANE_D.md checked the profile DLL's
// 5,182-name export directory: no `Array@Collection` match). MSVC emitted one
// COMDAT per instantiation and the linker did not fold them, so what looks like
// 27 mysteries is two functions written 27 times. Every address below is
// therefore a `RegisterRuntimeAddr`, never a `RegisterRuntimeExport`: there is
// no name to register under.
//
// The layout — {_data +0x00, _size +0x08, _capacity +0x0c}, 16 bytes — is
// runtime/types/collection/Collection.h, pinned there by four independent
// inlined bodies. This file re-uses that struct rather than restating it.
//
// ---------------------------------------------------------------------------
// WHAT WAS RE-VERIFIED (and what lane D got wrong)
// ---------------------------------------------------------------------------
// Lane D proved the clustering by hashing MNEMONIC SEQUENCES. Lane Q re-read
// all 27 bodies as disassembly (never the decompiled C —
// reference_ghidra_c_output_vs_disassembly.md: three Wave 6 defects including a
// crash came from Ghidra printing a MOVSXD as a zero extension, and `SetSize`
// contains two of them, at the `_size` load and the `_capacity * stride`
// multiply).
//
// J1 — CONFIRMED, and STRONGER than claimed. All eleven bodies are identical
//      instruction for instruction AND in every resolved absolute operand
//      (`0x1801fc718`, `0x18014b390`, `0x18014b388`, `0x1800c6c00`). Lane D said
//      "identical mnemonics and immediates, differing only in RIP-relative
//      displacements"; the displacements differ but they resolve to the same
//      four addresses. So J1 has NO parameters at all — not even a stride. One
//      body, eleven registrations, nothing to get wrong per instantiation.
//      (WAVE 9 LANE UIF: make that TWELVE registrations. A twelfth address,
//      0x180037240, was added below with the same byte-level evidence; the
//      "no parameters at all" conclusion is unchanged and is what lets one
//      body serve the extra address for free.)
//
// J3 — the ALGORITHM is one algorithm, but "16 instantiations parameterized by
//      stride / alignment / element-assign" is WRONG about the SIGNATURE, and
//      the signature is the part that corrupts memory if you get it wrong:
//
//        * EIGHT of the sixteen are TWO-ARGUMENT `SetSize(this, int n)` with no
//          `fill` parameter at all. Registering them behind a three-argument
//          typedef and dereferencing R8 reads a register the caller never set.
//        * ONE (0x18007b960) is a fully constant-folded ONE-argument call:
//          n = 10000 and the fill value -1 are both immediates in the body.
//        * THREE (0x180004e30, 0x180051390, 0x180061480) are NOT
//          trivially-copyable element types. They call unexported per-element
//          helpers, and they have a SHRINK-PATH DESTROY BRANCH that does not
//          exist in the other thirteen bodies at all. Porting them creates five
//          new lane-D rows (named in `kSpec*` below and in WAVE9_LANE_Q.md).
//        * TWO (0x180008b90, 0x18002fea0) leave the NEW TAIL UNINITIALISED —
//          the `else` arm of their fill loop is empty. A port that helpfully
//          zeroed it would not be faithful, and an oracle that compared it
//          would fail 100% of calls
//          (reference_oracle_must_not_compare_unwritten_bytes.md).
//
//      The full per-address table is `kSpec*` below and the parameterization
//      table in docs/runtime_port/WAVE9_LANE_Q.md.
//
// ---------------------------------------------------------------------------
// THE ALLOCATOR BOUNDARY — why this cluster is portable NOW
// ---------------------------------------------------------------------------
// The whole cluster's only external dependency is the allocator, and it reaches
// it through two EXPORTED statics that are byte-for-byte the sequences retail
// inlines here:
//
//   ?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z   0x1800c7a50
//       EnterCriticalSection(_singleton+0x640); Allocate(_singleton, n, align);
//       LeaveCriticalSection.
//   ?MemFree@MemoryAllocator@System@@SAXPEAX@Z      0x1800c7ad0
//       if (p) { EnterCriticalSection(_singleton+0x640);
//                Free(_singleton, p); LeaveCriticalSection; }
//
// 0x1800c7ad0 was disassembled for this lane and is INSTRUCTION FOR INSTRUCTION
// the tail of every J1 body, plus the null guard J1 performs itself. So calling
// it IS retail's body minus the inlining — and it keeps <windows.h> out of
// runtime/src/ per PLAN.md §4.1, which is the reason
// runtime/src/picking/scene.cpp had to leave one such free in the DLL.
//
// CALLED BY ADDRESS, NOT BY NAME, and that is deliberate. Wave 2 registers
// ports under both mangled names (runtime/src/system/allocator.cpp) behind
// `BANISHED_RT_ALLOC`, which carries a standing NO-GO verdict
// (docs/runtime_port/WAVE2_ALLOCATOR.md). Going by name would make these
// buffers come from whichever allocator that switch happened to select while
// the DLL's own code keeps freeing them into the other one. A heap cannot be
// half-ours. runtime/src/graphics/scene.cpp:206 made the same call for the same
// reason and this file follows it.
//
// *** LANE E DEPENDENCY, STATED SO IT CANNOT BE MISSED. *** At cutover
// `ResolveAddr(Module::Runtime, 0x1800c7a50)` resolves nothing, because there is
// no DLL. These two accessors MUST be repointed at lane E's allocator in the
// same commit that arms it, and they are deliberately the only two places in
// this file that name a Runtime address other than the 27 being registered.
//
// ---------------------------------------------------------------------------
// ORACLE
// ---------------------------------------------------------------------------
// A destructor and an in-place resize cannot be replayed at a live call site:
// running ours then theirs frees the same buffer twice. The oracle for this
// lane is therefore a SYNTHETIC-SUBJECT replay in array_probe.cpp — two
// identical private Arrays, ours run on one and the shipped body on the other,
// the results compared — plus the cheap live checks below, which exist mostly
// to answer WHICH of the 27 addresses a given run actually reached.
//
// Read docs/runtime_port/WAVE9_LANE_Q.md "Oracle plan" before believing a green
// run: `calls=0` on a site reports clean and is NOT verified
// (reference_uncalled_port_is_not_verified.md), and one shared body serving
// eleven addresses means a run can verify the BODY while leaving ten
// REGISTRATIONS untouched.
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/collection/Collection.h"
#include "array_internal.h"

// ----------------------------------------------------------------------------
// ResolveAddr, forward-declared rather than pulled in from standalone/resolve.h
// ----------------------------------------------------------------------------
// Same reasoning rt_port.h gives for declaring RegisterRuntimeExport locally:
// resolve.h drags in <windows.h>, whose <wingdi.h> declares a function called
// `Rectangle` and whose <windef.h> #defines `near`/`far` away. This declaration
// must stay byte-identical to resolve.h's; the linker checks the two.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

// Wave 9 lane VID2 — the Collection::Set AVL probe's arming hook. Declared at
// file scope because `extern "C"` cannot appear inside a function body.
extern "C" void RtCollectionSetAvlProbeArm(bool on);
extern "C" void RtCollectionSetClearProbeArm(bool on);
// Wave 9 lane COLL — the keyed Set/Map probe's arming hook. Same file-scope
// reason: `extern "C"` cannot appear inside a function body.
void RtCollectionAvlProbeArm(bool on);

namespace rtcoll {

// The 16-byte container header (aliased in array_internal.h). sizeof(Array<T>)
// is T-independent and Collection.h static_asserts that; `uint8_t` is chosen so
// `_data` is already the byte cursor every body below wants.
static_assert(sizeof(ArrayHdr) == 0x10, "Collection::Array is 16 bytes");
static_assert(offsetof(ArrayHdr, _data)     == 0x00, "Array::_data @ +0x00");
static_assert(offsetof(ArrayHdr, _size)     == 0x08, "Array::_size @ +0x08");
static_assert(offsetof(ArrayHdr, _capacity) == 0x0c, "Array::_capacity @ +0x0c");

// ============================================================================
// The allocator boundary. See the banner for why these go by address.
// ============================================================================
// RETRIES A NULL rather than caching it, which is not defensive tidying: at
// cutover this address is supplied by RegisterAllocatorAddresses() inside the
// allocator's arming path, and ANY caller that ran before that point would
// otherwise freeze a null into this static for the life of the process and take
// the whole heap down with it. Same contract as RtResolveCached (rt_port.h):
// cache a hit, retry a miss. A hit costs one load, exactly as before.
void* MemAlloc(uint64_t bytes, int align) {
    using Fn = void* (*)(uint64_t, int);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x1800c7a50));
    return fn(bytes, align);
}

// `if (p) { Enter; Free(_singleton, p); Leave; }` — the null test is INSIDE
// 0x1800c7ad0 (`TEST RCX,RCX; JZ ret` is its first instruction), which is why
// the callers below hand it a possibly-null pointer exactly as retail does.
void MemFree(void* p) {
    using Fn = void (*)(void*);
    static Fn fn = nullptr;   // retry a miss — see MemAlloc above
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x1800c7ad0));
    fn(p);
}

// ============================================================================
// J1 — Collection::Array<T>::~Array, trivially-destructible T
// ============================================================================
//
// Retail, read at 0x18001c750 and confirmed identical at the other ten:
//
//     PUSH RDI / SUB RSP,0x30 / MOV [RSP+0x20],-2      ; MSVC EH state, no
//     MOV [RSP+0x48],RBX / MOV [RSP+0x50],RSI          ; semantics
//     MOV RDI,RCX
//     MOV RSI,[RCX]                    ; p = this->_data
//     TEST RSI,RSI / JZ zero
//     MOV RBX,[0x1801fc718] / ADD RBX,0x640            ; &_singleton->cs
//     MOV RCX,RBX / CALL [EnterCriticalSection]
//     MOV RDX,RSI / MOV RCX,[0x1801fc718] / CALL 0x1800c6c00   ; Free
//     MOV RCX,RBX / CALL [LeaveCriticalSection]
//  zero:
//     XOR EAX,EAX
//     MOV [RDI],RAX                    ; _data     = 0
//     MOV [RDI+8],RAX                  ; _size AND _capacity = 0, as one qword
//     ... epilogue ... RET             ; RAX is 0 at RET
//
// Three things worth spelling out because a "cleanup" would break them:
//
//  * `[RDI+8]` is ONE eight-byte store covering _size and _capacity together.
//    Capacity IS reset here — unlike SetSize, which never shrinks it.
//  * RAX is zero at the RET. No caller in the tree reads it, but returning
//    `void*`/nullptr costs nothing and means a future caller that does cannot
//    diverge. (A `void` return would leave GCC's choice of RAX in the register,
//    which is a difference for free.)
//  * The `MOV [RSP+0x20],-2` and the `MOV [RSP+0x40],RBX` spill are the MSVC
//    unwind funclet's state slot and its `this` for the critical section. They
//    have no effect on the object and are not reproduced.
//
// NOT PARAMETERIZED. The eleven bodies are the same instructions with the same
// operands; the element stride never appears, because freeing a buffer does not
// need to know what is in it. This is why one C++ function serves all eleven.
void* ArrayDtorImpl(ArrayHdr* a) {
    MemFree(a->_data);      // retail's `if (p)` lives inside 0x1800c7ad0
    a->_data     = nullptr;
    a->_size     = 0;
    a->_capacity = 0;
    return nullptr;         // retail leaves RAX == 0
}

// ============================================================================
// J3 — Collection::Array<T>::SetSize
// ============================================================================
//
// Retail, read at 0x18007a0d0 / 0x180023830 / 0x18002fea0 and cross-checked
// against the other thirteen. In C, with the parameters named:
//
//   void SetSize(Array* a, int n, const T* fill) {
//       int64_t oldSize = (int32_t)a->_size;            // MOVSXD  <-- (1)
//       if (n == a->_size) return;                      // exact-equal no-op
//       T* old = a->_data;
//       a->_size = n;
//       if (n > a->_capacity) {                         // SIGNED compare
//           a->_capacity = n;
//           if (n == (int32_t)oldSize + 1 && oldSize != 0)
//               a->_capacity = n * 3;                   // 32-bit LEA
//           a->_data = MemAlloc((int64_t)a->_capacity * stride, align); // (2)
//       }
//       int64_t i = (old == a->_data) ? oldSize : 0;
//       for (; i < n; ++i) {
//           T* dst = a->_data + i;
//           if (dst == 0) continue;                     // yes, really
//           if (i < oldSize) ASSIGN(dst, old + i);
//           else             DEFAULT(dst) /* or ASSIGN(dst, fill) */;
//       }
//       if (old != a->_data) { DESTROY(old, oldSize); MemFree(old); }
//       else if (oldSize > n) DESTROY(old + n, oldSize - n);
//   }
//
// (1) and (2) are the two MOVSXDs Ghidra's C prints as zero extensions. They
// only matter for negative values, and a negative `_size` or `_capacity` is
// already a corrupt container — but they are written as sign extensions here
// because the point of a faithful port is that it fails the same way.
//
// Details that are easy to lose and are all load-bearing:
//
//  * The no-op test is `n == _size`, not `n <= _capacity`. SetSize(_size) does
//    nothing at all, including no fill.
//  * Capacity NEVER shrinks: the shrink path skips the whole `if` and the copy
//    loop (start = oldSize >= n), so the buffer is kept.
//  * The `* 3` treble is gated on `oldSize != 0`, so the FIRST append to an
//    empty array allocates capacity exactly 1. Appending one at a time from
//    empty therefore walks 1, 6, 21, 66, 201 — and a bulk SetSize(n) allocates
//    exactly n, because n != oldSize + 1.
//  * `if (dst == 0)` is a per-iteration test of the COMPUTED element address.
//    It can only fire when `_data` is null and i == 0, i.e. when MemAlloc
//    failed. Retail checks it every iteration and so does this.
//  * The copy source cursor advances by `stride` every iteration whether or not
//    the copy branch is taken, which matters only if you write the loop with a
//    separate pointer; it is written as `old + i * stride` here, which is the
//    same thing.
//
// ---------------------------------------------------------------------------
// The parameterization
// ---------------------------------------------------------------------------
// `stride` and `align` are what the MemAlloc call site bakes in
// (`SHL RCX,3` + `MOV EDX,8`, or `IMUL RCX,RCX,0x1c` + `MOV EDX,4`, ...).
// `assign` / `init` / `destroy` are the element operations. `init` is used ONLY
// by the two-argument form; a null `init` means retail's `else` arm is EMPTY
// and the new tail is left holding whatever the allocation contained.
// ============================================================================

// `ElemAssign` / `ElemInit` / `ElemDestroy` and `ArraySpec` are declared in
// array_internal.h so the probe drives the identical rows.
//
// ElemDestroy's first parameter is `ignoredThis`: retail spills RCX to the
// shadow space and never reads it in either destroy helper. Verified by
// disassembling 0x1800059e0 and 0x180005930 — both start
// `TEST R8D,R8D / JLE ret / MOV [RSP+8],RCX` — and the only caller of
// 0x1800059e0 (inside 0x180004e30) does not set RCX at all before the call.

// ---- trivially-copyable element operations ---------------------------------

void Assign1 (uint8_t* d, const uint8_t* s) { *d = *s; }
void Assign4 (uint8_t* d, const uint8_t* s) {
    *reinterpret_cast<uint32_t*>(d) = *reinterpret_cast<const uint32_t*>(s);
}
void Assign8 (uint8_t* d, const uint8_t* s) {
    *reinterpret_cast<uint64_t*>(d) = *reinterpret_cast<const uint64_t*>(s);
}
// 0x180008d60 / 0x1800764c0 / 0x180076900 / 0x18007a0d0 store a ZERO first and
// then the value, into the same eight bytes. That is the smart-pointer element
// type's `operator=` inlined (null the slot, then take the new referent), and
// it is reproduced rather than folded: the final state is identical, but a
// hardware write watchpoint or a concurrent reader would see the difference,
// and BANISHED_REDZONE=watch is one of this lane's instruments.
void Assign8Z(uint8_t* d, const uint8_t* s) {
    *reinterpret_cast<uint64_t*>(d) = 0;
    *reinterpret_cast<uint64_t*>(d) = *reinterpret_cast<const uint64_t*>(s);
}
void Assign16(uint8_t* d, const uint8_t* s) {
    reinterpret_cast<uint64_t*>(d)[0] = reinterpret_cast<const uint64_t*>(s)[0];
    reinterpret_cast<uint64_t*>(d)[1] = reinterpret_cast<const uint64_t*>(s)[1];
}
// 0x180059140, Math::Transform: seven dwords at +0,+4,...,+0x18.
void Assign1c(uint8_t* d, const uint8_t* s) {
    for (int k = 0; k < 7; ++k)
        reinterpret_cast<uint32_t*>(d)[k] = reinterpret_cast<const uint32_t*>(s)[k];
}

// WAVE 9 LANE PKG2 rung 4b-ii — 0x180024c10, Array<CompressedStream::Window>.
// The copy is three plain qword moves at +0x00, +0x08 and +0x10.
void Assign18(uint8_t* d, const uint8_t* s) {
    for (int k = 0; k < 3; ++k)
        reinterpret_cast<uint64_t*>(d)[k] = reinterpret_cast<const uint64_t*>(s)[k];
}

// WAVE 9 LANE UIF — 0x180078520 / 0x1800783d0, Array<UI::SpriteDraw>, stride
// 0x28. The copy at 0x1800785c2..0x1800785e4 is FIVE plain qword moves at
// +0x00, +0x08, +0x10, +0x18 and +0x20: no zero-first store, no refcount, so
// the assign arm is a pure byte copy covering the whole 0x28-byte element.
void Assign28(uint8_t* d, const uint8_t* s) {
    for (int k = 0; k < 5; ++k)
        reinterpret_cast<uint64_t*>(d)[k] = reinterpret_cast<const uint64_t*>(s)[k];
}

void InitZero8(uint8_t* d) { *reinterpret_cast<uint64_t*>(d) = 0; }
// 0x18007b960's `else` arm is `MOV dword ptr [RDX],0xffffffff`.
void InitNeg1 (uint8_t* d) { *reinterpret_cast<int32_t*>(d) = -1; }
// WAVE 9 LANE UIF — the default (no-fill) arm of Array<UI::SpriteDraw>, read
// out of 0x1800783d0 at 0x18007847b..0x180078494. SIX stores, and what they
// DO NOT cover is the load-bearing part:
//     MOV [RAX],RBP        +0x00 qword = 0   sheet
//     MOV [RAX+0x8],RBP    +0x08 qword = 0   spriteIndex, x1
//     MOV [RAX+0x10],RBP   +0x10 qword = 0   y1, x2
//     MOV [RAX+0x18],EBP   +0x18 dword = 0   y2
//     MOV [RAX+0x20],EBP   +0x20 dword = 0   color
//     MOV [RAX+0x24],BPL   +0x24 byte  = 0   isLine
// so +0x1c..+0x1f (`rotation`) and +0x24+1..+0x27 (the tail padding) are
// NEVER WRITTEN by this arm. That is the retail default-constructed
// UI::SpriteDraw: every member zeroed except a float that has no initialiser.
void InitSpriteDraw(uint8_t* d) {
    reinterpret_cast<uint64_t*>(d)[0] = 0;   // +0x00
    reinterpret_cast<uint64_t*>(d)[1] = 0;   // +0x08
    reinterpret_cast<uint64_t*>(d)[2] = 0;   // +0x10
    *reinterpret_cast<uint32_t*>(d + 0x18) = 0;
    *reinterpret_cast<uint32_t*>(d + 0x20) = 0;
    d[0x24] = 0;
}
// 0x180059140's `else` arm, the identity Math::Transform:
//   [d+0x00] = 0 (qword)  [d+0x08] = 0 (dword)
//   [d+0x0c] = 0x3f800000 (qword: 1.0f at +0x0c, 0 at +0x10)
//   [d+0x14] = 0 (qword)
// All 0x1c bytes are written, so there is no unwritten hole in this one.
// 0x180024c10's `else` arm, an empty CompressedStream::Window. Three stores,
// and TWO of them are worth spelling out:
//   MOV dword ptr [RCX],R11D        -> [d+0x00] = 0   (4 bytes)
//   MOV qword ptr [RCX+0x4],-0x1    -> [d+0x04] = -1  (8 bytes, UNALIGNED,
//                                      covering +0x04..+0x0b)
//   MOV qword ptr [RCX+0x10],R11    -> [d+0x10] = 0   (8 bytes)
// So **+0x0c..+0x0f is NEVER WRITTEN** by the tail init, even though the
// element stride is 0x18. Any oracle over a grown Array<Window> must exclude
// those four bytes or it compares memory neither side wrote
// (reference_oracle_must_not_compare_unwritten_bytes). The unaligned 8-byte -1
// is retail's, not a transcription convenience — the same shape as
// InitIdentityTransform's qword at +0x0c below.
void InitWindow(uint8_t* d) {
    *reinterpret_cast<uint32_t*>(d + 0x00) = 0;
    *reinterpret_cast<int64_t*> (d + 0x04) = -1;
    *reinterpret_cast<uint64_t*>(d + 0x10) = 0;
}

void InitIdentityTransform(uint8_t* d) {
    *reinterpret_cast<uint64_t*>(d + 0x00) = 0;
    *reinterpret_cast<uint32_t*>(d + 0x08) = 0;
    *reinterpret_cast<uint64_t*>(d + 0x0c) = 0x3f800000ull;
    *reinterpret_cast<uint64_t*>(d + 0x14) = 0;
}

// ============================================================================
// The three NON-TRIVIAL element types (0x180004e30, 0x180051390, 0x180061480)
// ============================================================================
//
// These are the ones lane D's "one algorithm parameterized by stride,
// alignment and element-assign" did not cover: their element operations are
// CALLS to unexported helpers, and they carry a shrink-path destroy branch the
// other thirteen bodies do not have at all.
//
// Porting them the faithful way — calling the helpers at their own addresses,
// exactly as ClearAll / KeyMap::RegisterInput / Audio::Scene::Serialize already
// call Collection internals by address — CREATES FIVE NEW LANE-D ROWS. They are
// named here so the next census does not have to rediscover them:
//
//   0x180004220   element copy-assign for the 0x38-byte SpriteSheet element
//   0x1800059e0   destroy-range for the same (releases two String refs each)
//   0x180051650   element copy-assign for Audio's 0x40-byte EffectSlot
//   0x180052110   destroy-range for the same
//   0x180005930   destroy-range for the 0x10-byte {String*, ExternalPtr}
//
// *** CORRECTED 2026-09-07 BY WAVE 9 LANE D2. *** THREE OF THOSE FIVE ROWS NO
// LONGER EXIST: 0x1800059e0, 0x180052110 and 0x180005930 are ported and
// registered by runtime/src/collection/array_destroy.cpp, which found that all
// eleven of this cluster's per-element DESTROY helpers are three bodies
// parameterized by (stride, offset-list). The three `ResolveAddr` calls below
// therefore reach OUR bodies now, not the DLL's — the registry is consulted
// first — and this file's `destroy` fields are satisfied without leaving the
// port.
//
// *** CORRECTED AGAIN 2026-09-08 BY WAVE 9 LANE COLL. ONE OF THE TWO REMAINING
// ROWS IS GONE AND THE REASON GIVEN FOR HOLDING IT WAS WRONG. *** 0x180051650
// is PORTED (runtime/src/collection/introsort.cpp) and so is the callee this
// comment described as an Audio holdback: **0x180050d30 is not Audio-specific
// at all**, it is a plain `Collection::Array<16-byte POD>::operator=` — the
// same body lane UIW ported at 0x18001cdf0, with stride 0x10, ALIGN 4 (not 8,
// and not 0x10; that immediate is the whole difference and this family has now
// produced several near-misses that turned on one) and dtor 0x180051d80, which
// this file already registers at line ~1343. Its only other callee is MemAlloc.
// It was ALSO a separate unregistered address door in its own right, at
// runtime/src/audio/audio.cpp:1114, and that row closes too.
//
// 0x180004220 (the 0x38-byte SpriteSheet element copy-assign) IS still the
// DLL's and is the last row of this cluster.
//
// The two ResolveAddr sites stay written exactly as they are: they resolve
// lazily, on first USE rather than at init, so they pick up lane D2's
// registrations with no ordering requirement between the two inits, and
// spelling the address as a literal is what keeps tools/runtime_census.py able
// to see the edge.
//
// This tier has its own kill switch, `BANISHED_RT_ARRAY_NONTRIVIAL=0`, so an
// integration run can A/B the thirteen safe instantiations against these three
// without disarming the whole lane.
// ============================================================================

// EVERY ONE OF THE FIVE HELPER CALLS BELOW SPELLS ITS ADDRESS AS A LITERAL
// INSIDE ResolveAddr(Module::Runtime, ...), and must keep doing so. A one-line
// address-wrapper reads better and is INVISIBLE TO tools/runtime_census.py,
// which scans for that literal at the call site. This lane's whole point is
// that the census can see what it retired; hiding the five rows it CREATED
// behind a helper would be the mirror image of
// reference_census_cannot_see_nonliteral_registrations.md, where twelve live
// ports went uncounted for a full wave. (Written after the first draft of this
// file did exactly that: the census reported 27 retired and 0 created.)

// 0x180004e30's copy: `MOV RCX,dst; MOV RDX,src; CALL 0x180004220`.
void Assign38Call(uint8_t* d, const uint8_t* s) {
    using Fn = void (*)(uint8_t*, const uint8_t*);
    static Fn fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x180004220));
    fn(d, s);
}
void Destroy38(void* ignoredThis, uint8_t* base, int count) {
    using Fn = void (*)(void*, uint8_t*, int);
    static Fn fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x1800059e0));
    fn(ignoredThis, base, count);
}

// 0x180051390's copy: `CALL 0x180051650` with RCX=dst, RDX=src.
void Assign40Call(uint8_t* d, const uint8_t* s) {
    using Fn = void (*)(uint8_t*, const uint8_t*);
    static Fn fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x180051650));
    fn(d, s);
}
void Destroy40(void* ignoredThis, uint8_t* base, int count) {
    using Fn = void (*)(void*, uint8_t*, int);
    static Fn fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x180052110));
    fn(ignoredThis, base, count);
}
// 0x180051390's `else` arm, inlined in retail (Audio::EffectSlot's default):
//   [+0x00]=0q [+0x08]=0q [+0x10]=0q [+0x18]=0q [+0x20]=0q
//   [+0x28]=0x3f800000 (DWORD)      [+0x30]=0q [+0x38]=0q
//
// *** BYTES +0x2c .. +0x2f ARE NEVER WRITTEN. *** The element is 0x40 bytes and
// this covers 0x00-0x2b and 0x30-0x3f. Any oracle that memcmps a whole new
// EffectSlot will fail on four bytes of allocator fill and read like a
// catastrophic port defect — that is exactly the Wave 6 failure recorded in
// reference_oracle_must_not_compare_unwritten_bytes.md, four times over. The
// probe excludes that window explicitly; see array_probe.cpp.
void InitEffectSlot(uint8_t* d) {
    *reinterpret_cast<uint64_t*>(d + 0x00) = 0;
    *reinterpret_cast<uint64_t*>(d + 0x08) = 0;
    *reinterpret_cast<uint64_t*>(d + 0x10) = 0;
    *reinterpret_cast<uint64_t*>(d + 0x18) = 0;
    *reinterpret_cast<uint64_t*>(d + 0x20) = 0;
    *reinterpret_cast<uint32_t*>(d + 0x28) = 0x3f800000u;   // 1.0f
    /* +0x2c .. +0x2f deliberately untouched — retail does not write them */
    *reinterpret_cast<uint64_t*>(d + 0x30) = 0;
    *reinterpret_cast<uint64_t*>(d + 0x38) = 0;
}

// 0x180061480's copy, INLINED in retail rather than called. The element is
// {System::String* str; ExternalPtr ext} and the assignment is:
//     [dst]   = 0
//     RAX     = *(void**)src
//     if (RAX) { [dst] = RAX; ++*(int32_t*)((char*)RAX - 4); }   // REFCOUNT
//     [dst+8] = *(void**)(src+8)
// The `INC dword ptr [RAX-4]` is System::String's reference count at `str-4`
// (runtime/src/system/string.cpp names the same convention). Getting this wrong
// leaks or double-frees strings far from here, so it is written out literally
// rather than as a 16-byte copy.
void AssignStringExt(uint8_t* d, const uint8_t* s) {
    void** dp = reinterpret_cast<void**>(d);
    void* const* sp = reinterpret_cast<void* const*>(s);
    dp[0] = nullptr;
    void* str = sp[0];
    if (str) {
        dp[0] = str;
        ++*(reinterpret_cast<int32_t*>(static_cast<uint8_t*>(str) - 4));
    }
    dp[1] = sp[1];
}
void DestroyStringExt(void* ignoredThis, uint8_t* base, int count) {
    using Fn = void (*)(void*, uint8_t*, int);
    static Fn fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x180005930));
    fn(ignoredThis, base, count);
}

// ============================================================================
// WAVE 9 LANE UIW — two more element types, both read out of their own bodies
// ============================================================================
//
// These exist because four cutover residue rows in UI (`ComboElement::AddItem`,
// `ComboElement::RemoveAllItems`, `GraphElement::AddGraph`,
// `RadioElement::FindCheckBoxes`) reach seven `Collection::Array` instantiations
// that no lane owned. Three of the seven turned out to be exact clones of specs
// this file already had; the two below are new, and the seventh is `~Array`.
//
// ---------------------------------------------------------------------------
// 1. Array<System::String>, 0x180034860
// ---------------------------------------------------------------------------
// The assign, read at 0x1800348fc..0x180034916 (NOT from decompiled C — the
// null-source arm is a branch Ghidra's C folds away):
//
//     MOV [R8],R15          ; dst = 0, unconditionally, FIRST
//     MOV RCX,[RAX]         ; src value
//     TEST RCX,RCX / JZ done
//     MOV [R8],RCX
//     INC dword ptr [RCX-4] ; System::String's refcount at str-4
//
// The unconditional `dst = 0` before the test is why a null source leaves the
// destination NULL rather than untouched: the whole 8-byte stride is written on
// both arms, so there is no unwritten window.
void AssignStringRef(uint8_t* d, const uint8_t* s) {
    void** dp = reinterpret_cast<void**>(d);
    void* const* sp = reinterpret_cast<void* const*>(s);
    dp[0] = nullptr;
    void* str = sp[0];
    if (str) {
        dp[0] = str;
        ++*(reinterpret_cast<int32_t*>(static_cast<uint8_t*>(str) - 4));
    }
}

// The destroy-range 0x180034860 calls on both its realloc arm (0x18003492e) and
// its SHRINK arm (0x18003497d) — the second is why this element type needs a
// `destroy` at all, since kSpec_Fill8's instantiations have no shrink branch.
//
// *** IT IS ALREADY PORTED, AND THIS LANE'S FIRST DRAFT DID NOT KNOW THAT. ***
// 0x180011010 is `kD_Str_08` in runtime/src/collection/array_destroy.cpp — one
// of the eight helpers that file ports but deliberately leaves UNREGISTERED,
// with the note "Register it the day a caller appears". This lane is that
// caller (here, and again from 0x180029a80 whenever `TextElement::
// SetParameterCount` lands), so the B_ENTRY line and the registration were
// added there rather than a second body being written here.
//
// The first draft of this file DID write that second body, and also registered
// 0x180010ab0 — which array_destroy.cpp already registers as a J2 entry point.
// That is a SHADOWED ADDRESS: two registrations of the same address, one of them
// dead code, the failure `tools/dispatch_slot_map.py --check` reports for the
// exe side and nothing reports for this one. It surfaced only because a diff run
// printed `Collection::Array::~Array(+elem) @0x180010ab0` — a site name this
// lane had not written. **Before porting a Collection helper, grep the address:
// array.cpp and array_destroy.cpp are two files and one namespace.**
//
// Called by ADDRESS for the same reason DestroyStringExt above is: the
// registered body is in another translation unit and the registry is the door
// every other caller uses.
void DestroyStringRange(void* ignoredThis, uint8_t* base, int count) {
    using Fn = void (*)(void*, uint8_t*, int);
    static Fn fn = nullptr;               // cache a hit, retry a miss
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x180011010));
    fn(ignoredThis, base, count);
}

// ---------------------------------------------------------------------------
// 2. Array<GraphSeries>, 0x1800417d0  (stride 0x18)
// ---------------------------------------------------------------------------
// The stride is `LEA RAX,[RAX+RAX*2]; SHL RCX,3` = capacity * 24, align 8. A
// GraphSeries is {uint32 color; uint32 hole; Collection::Array<int32> points},
// with the nested Array at +0x08 — the same +0x08 `GraphElement::AddGraphPoint`
// (widgets_layout.cpp) already appends into, which is independent confirmation
// of the layout from a body this lane did not read.
//
// The assign, read at 0x180041890..0x1800418b1:
//     MOV EAX,[RDX] / MOV [RCX],EAX     ; the colour dword, +0x00..+0x03
//     ADD RCX,8
//     MOV [RCX],R8                      ; +0x08 = 0   (R8 is zero here)
//     MOV [RCX+8],0                     ; +0x10 = 0
//     ADD RDX,8
//     CALL 0x18001cdf0                  ; Array<int32>::operator=(dst+8, src+8)
//
// **+0x04..+0x07 IS NEVER WRITTEN BY EITHER ARM.** The colour is one dword, not
// a qword, and the nested Array starts at +0x08. That hole is declared in the
// spec's assign window below, because an oracle that compares bytes neither
// side writes fails 100% of calls and reads like a catastrophic port defect
// (reference_oracle_must_not_compare_unwritten_bytes.md, four such gaps in
// Wave 6).
//
// The destination's nested Array header is ZEROED BEFORE the operator= runs,
// and that is load-bearing rather than tidiness: on the realloc arm the
// destination is raw MemAlloc memory, and `operator=` starts by destroying the
// destination (it calls ~Array first). Handing it garbage would free a garbage
// pointer.
// `Collection::Array<int32_t>::operator=`, retail 0x18001cdf0. Unexported, so
// it is registered by ADDRESS below. It is the assign arm of the GraphSeries
// spec and has no other caller in our tree.
//
// Retail, 0x18001cdf0..0x18001ce7b:
//     if (dst == src) return dst;                 // CMP RCX,RDX / JZ
//     ~Array(dst);                                // CALL 0x18001c750  (OURS)
//     n   = src->_size;
//     old = dst->_size;                           // ZERO — the dtor just ran
//     dst->_size = n;
//     if (n > dst->_capacity) {
//         dst->_capacity = n;
//         if (n == old + 1 && old != 0) dst->_capacity = n * 3;
//         dst->_data = MemAlloc((int64)dst->_capacity * 4, 4);
//     }
//     for (i = 0; i < dst->_size; ++i) {          // bound RE-READ every pass
//         p = dst->_data + i*4;                   // base RE-READ every pass
//         if (p == 0) continue;
//         *p = *(int32_t*)(src->_data + i*4);     // src base RE-READ too
//     }
//     return dst;                                 // RAX = dst
//
// THE `old` TREBLE BRANCH IS DEAD IN RETAIL AND IS KEPT ANYWAY. `old` is read
// AFTER the destructor has zeroed `_size`, so it is always 0 and the `* 3`
// growth arm can never be taken here. Deleting it would be a correct-looking
// simplification of a body whose whole value is being the same instructions —
// and this project has been burned by exactly that shape
// (docs/runtime_port/WAVE9.md's `dirAlias` finding: a local that looks dead and
// is not). It is transcribed, with this note, rather than folded away.
//
// The `~Array` goes through ResolveAddr rather than calling ArrayDtorImpl
// directly, so the 0x18001c750 site's own `calls=` counts it and so the door is
// the same one every other by-address caller of that address uses.
void* RtColl_I32ArrayAssign(uint8_t* dstRaw, const uint8_t* srcRaw) {
    ArrayHdr* dst = reinterpret_cast<ArrayHdr*>(dstRaw);
    const ArrayHdr* src = reinterpret_cast<const ArrayHdr*>(srcRaw);
    if (dst == src) return dst;

    using DtorFn = void* (*)(void*);
    static DtorFn dtor = nullptr;            // cache a hit, retry a miss
    if (!dtor) dtor = reinterpret_cast<DtorFn>(ResolveAddr(Module::Runtime, 0x18001c750));
    dtor(dst);

    const int32_t n   = src->_size;
    const int32_t old = dst->_size;          // always 0 here — see above
    dst->_size = n;
    if (n > dst->_capacity) {
        dst->_capacity = n;
        if (n == static_cast<int32_t>(old + 1) && old != 0)
            dst->_capacity = static_cast<int32_t>(n * 3);
        dst->_data = static_cast<uint8_t*>(
            MemAlloc(static_cast<uint64_t>(static_cast<int64_t>(dst->_capacity)) * 4, 4));
    }
    for (int32_t i = 0; i < dst->_size; ++i) {
        uint8_t* p = dst->_data + static_cast<int64_t>(i) * 4;
        if (p == nullptr) continue;
        *reinterpret_cast<int32_t*>(p) =
            *reinterpret_cast<const int32_t*>(src->_data + static_cast<int64_t>(i) * 4);
    }
    return dst;
}

void AssignGraphSeries(uint8_t* d, const uint8_t* s) {
    *reinterpret_cast<uint32_t*>(d) = *reinterpret_cast<const uint32_t*>(s);
    /* +0x04 .. +0x07 deliberately untouched — retail does not write them */
    *reinterpret_cast<uint64_t*>(d + 0x08) = 0;
    *reinterpret_cast<uint64_t*>(d + 0x10) = 0;
    // BY ADDRESS, not as a direct C++ call to the body two functions up — the
    // same convention `Assign38Call` and `DestroyStringExt` above already
    // follow for an element operation that is a CALL in retail. It is not
    // pedantry: the FIRST version of this line called the inner body directly,
    // which is the same instructions but skips the registered entry point, so
    // `0x18001cdf0`'s diff site never fired and the run reported no coverage
    // for a whole ported body. Retail's 0x1800418b1 is a CALL to that address;
    // going through the registry is both faithful and instrumented.
    using Fn = void* (*)(void*, const void*);
    static Fn fn = nullptr;                 // cache a hit, retry a miss
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x18001cdf0));
    fn(d + 8, s + 8);
}

// 0x1800419b0, the destroy-range for the same element — and, exactly like
// 0x180011010 above, ALREADY PORTED in array_destroy.cpp as `kD_InnerArr_18`
// ("Array @+0x008, stride 0x018"), and left unregistered there for want of a
// caller. This lane supplies the caller; the A_ENTRY line and the registration
// were added in that file. Retail iterates the records at stride 0x18 and, for
// the nested Array at record+8, runs the same free-and-zero `~Array` runs.
//
// Reached by address so the registry is the door, as above.
void DestroyGraphSeriesRange(void* ignoredThis, uint8_t* base, int count) {
    using Fn = void (*)(void*, uint8_t*, int);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x1800419b0));
    fn(ignoredThis, base, count);
}

// ============================================================================
// The per-address specs.
//
// Every row was read out of that address's own disassembly. `stride` is the
// SHL/IMUL at the MemAlloc call, `align` is the immediate in EDX, and the
// element ops are the loop body.
// ============================================================================

const ArraySpec kSpec_Bool = {                    // 0x180008b90  Array<bool>
    "Array<bool>", 1, 1, &Assign1,
    nullptr,   // NEW TAIL LEFT UNINITIALISED — retail's else arm is empty
    nullptr, true, 0, 0, 0, 0, 0, 0 };

const ArraySpec kSpec_I32 = {                     // 0x18002fea0  Array<int32>
    "Array<int32>", 4, 4, &Assign4,
    nullptr,   // NEW TAIL LEFT UNINITIALISED
    nullptr, true, 0, 0, 0, 0, 0, 0 };

const ArraySpec kSpec_U64Zero = {                 // 0x180008ff0  Array<Time>
    "Array<Time>", 8, 8, &Assign8, &InitZero8, nullptr, true, 0, 0, 0, 0, 0, 0 };

// WAVE 9 LANE PKG2 rung 4b-ii — 0x180024d70, Array<CompressedStream::Header>.
// A NEW SPEC, and the reason is one field. Byte-similarity puts it closest to
// kSpec_U64Zero (first difference at 88 of 352), and U64Zero agrees on element
// size 8, on Assign8 and on InitZero8 — it disagrees ONLY on the ALIGNMENT
// handed to MemAlloc: 0x180008ff0 is `MOV EDX,0x8`, 0x180024d70 is
// `MOV EDX,0x4`, checked in the disassembly of both. Reusing U64Zero because it
// "looks like" this one would have passed a wrong alignment to the allocator on
// every packfile block directory. Similarity is corroboration, not evidence.
const ArraySpec kSpec_U64ZeroA4 = {               // 0x180024d70  Array<Header>
    "Array<CompressedStream::Header>", 8, 4, &Assign8, &InitZero8, nullptr, true,
    0, 0, 0, 0, 0, 0 };

// 0x180024c10 — Array<CompressedStream::Window>, the window table
// CompressedStream::SetWindowCount grows. Element stride 0x18 (`LEA
// RCX,[RAX+RAX*2]; SHL RCX,3`), align 8 (`MOV EDX,0x8`), both read from the
// body. Distinct from every spec above on BOTH the element size and the tail
// init, so unlike kSpec_U64ZeroA4 there was no lookalike to be tempted by.
// WAVE 9 LANE PKG2 rung 4c-ii - 0x1800298f0, the packfile ENTRY array.
// Element is 16 bytes: a System::Filename at +0x00, then two int32 at +0x08
// (offset) and +0x0c (size) -- the PackageFileEntry already in
// runtime/types/io/FileSystem.h, whose stride and field offsets this body
// independently confirms (SHL RCX,0x4, MOV EDX,0x8, and the two dword copies at
// 1800299c3/1800299c9).
//
// *** THIS ALMOST REUSED kSpec_StringExt, AND THAT WOULD HAVE BEEN A DEFECT. ***
// AssignStringExt is byte-for-byte what this element needs for its first eight
// bytes -- null the slot, adopt, increment the refcount -- and then copies the
// remaining eight verbatim, which is exactly the two dwords. Every visible
// property matches. But the element assign here is a CALL to 0x1800067d0,
// ??0Filename@System@@QEAA@AEBV01@@Z, and that function does one thing
// AssignStringExt does not: it ends with TWO calls to
// Filename::AdjustDelimiters (0x18000682a and 0x180006832). Reusing the
// lookalike would have left every packfile entry name with its separators
// unnormalised -- forward slashes never rewritten to backslashes -- which
// breaks lookup for any entry that uses them and breaks NOTHING otherwise, so
// it would have passed a green run.
//
// Third time this family has offered a wrong answer by resemblance, and the
// first where the difference was not an immediate but a TRAILING CALL. The
// assign below therefore delegates to the ported copy ctor rather than
// reimplementing its String half.
void AssignPkgEntry(uint8_t* d, const uint8_t* s) {
    using Fn = void* (*)(void*, const void*);
    static Fn ctor = nullptr;            // cache a hit, retry a miss
    if (!ctor) ctor = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x1800067d0));
    ctor(d, s);                                                    // 1800299bd
    *reinterpret_cast<uint32_t*>(d + 0x8) =
        *reinterpret_cast<const uint32_t*>(s + 0x8);               // 1800299c3
    *reinterpret_cast<uint32_t*>(d + 0xc) =
        *reinterpret_cast<const uint32_t*>(s + 0xc);               // 1800299c9
}

// The range destructor 0x1800298f0 calls on BOTH arms -- after a realloc
// (180029a0e, over the whole old array) and on a SHRINK with no realloc
// (180029a68, over the tail only).
//
// *** WAVE 9 LANE DESTROY, 2026-09-08: 0x180026b50 IS NOW PORTED, and the
// sentence that used to stand here was half right in a way that mattered. ***
// It said this was "a different function from DestroyStringExt's 0x180005930
// and DestroyStringRange's 0x180011010". That is true of the ADDRESS -- three
// distinct entry points -- and it is FALSE of the BODY: 0x180026b50 and
// 0x180005930 differ only in an alignment NOP and a duplicated store into this
// function's own SEH scope slot, neither of which any caller can observe. Both
// listings are tabulated side by side above the B_ENTRY block in
// array_destroy.cpp, which is where the claim is made from the disassembly
// rather than from resemblance, and where the probe row that tests it against
// the SHIPPED 0x180026b50 is named.
//
// So the element DOES still need its own ArraySpec -- because of the ASSIGN
// (0x1800067d0's two trailing Filename::AdjustDelimiters calls, the third
// near-miss this family produced) -- but it does NOT need its own DestroySpec.
// The two halves of "why kSpec_PkgEntry exists" have different answers and were
// being carried as one.
//
// This still forwards by ADDRESS, deliberately: ResolveAddr consults the port
// registry first, so it now reaches array_destroy.cpp's
// RtColl_DestroyStrings_180026b50 with the DLL present or absent, and the
// address stays spelled as a literal here so tools/addr_door_audit.py and
// tools/runtime_census.py can both still see the edge.
void DestroyPkgEntryRange(void* ignoredThis, uint8_t* base, int count) {
    using Fn = void (*)(void*, uint8_t*, int);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x180026b50));
    fn(ignoredThis, base, count);
}

// NO SEED ZEROS, and that is read out of the callee rather than copied from the
// sibling spec: 0x1800067d0's FIRST action is `MOV qword ptr [RBX],0`
// (1800067f0), and the read at 1800067f3 reads back the zero it just wrote. It
// never reads uninitialised memory, so there is nothing for a seed to protect.
const ArraySpec kSpec_PkgEntry = {                // 0x1800298f0  Array<Entry>
    "Array<IO::PackageFile::Entry>", 0x10, 8, &AssignPkgEntry, nullptr,
    &DestroyPkgEntryRange, true,
    /*assign unwritten*/ 0, 0,
    /*init   unwritten*/ 0, 0,
    /*seed zero       */ 0, 0 };

const ArraySpec kSpec_Window = {                  // 0x180024c10  Array<Window>
    "Array<CompressedStream::Window>", 0x18, 8, &Assign18, &InitWindow, nullptr,
    true, 0, 0, 0, 0, 0, 0 };

// WAVE 9 LANE GFX3, 2026-09-08 — 0x18005fb00, Array<Graphics::Scene::Node>.
// The node array `Graphics::Scene`'s grid appends to. Read out of its own body
// rather than inferred from the element type, and it lands one field away from
// kSpec_Window, which is a lookalike this lane had to rule out rather than
// adopt (fourth time this family has offered a wrong answer by resemblance;
// the earlier three differed in an alignment immediate, a shift, and a trailing
// call):
//   * stride 0x18 — `LEA RCX,[RAX+RAX*2]; SHL RCX,3` at 18005fb53/fb57. Same
//     as kSpec_Window.
//   * align 8 — `MOV EDX,0x8` at 18005fb5b. Same as kSpec_Window.
//   * assign = three plain qword moves at +0x00/+0x08/+0x10 (18005fba2..fbb4),
//     with no zero-first store. Same body as Assign18.
//   * *** init = NONE. *** This is the difference. 0x180024c10 has an `else`
//     arm that builds an empty Window; 0x18005fb00's loop has NO else arm at
//     all — `CMP R8,RSI; CMOVL RCX,R10` picks the old element or the FILL and
//     there is no third case, because every caller of this address passes a
//     fill. Reusing kSpec_Window would have run InitWindow over the new tail on
//     any two-argument call, writing three qwords retail never writes.
// So it is a distinct row, and it is the three-argument form only.
//
// The element's +0x14 is a DON'T-CARE the caller supplies as an uninitialised
// stack dword (see runtime/src/graphics/scene_grid.cpp's GridAdd banner and
// runtime/types/graphics/Scene.h). The assign COPIES it, faithfully; nothing
// here has to know that, and no oracle may compare it.
const ArraySpec kSpec_Node18 = {                  // 0x18005fb00  Array<Scene::Node>
    "Array<Graphics::Scene::Node>", 0x18, 8, &Assign18,
    nullptr,   // NO else arm in retail's loop — the three-argument form only
    nullptr, true, 0, 0, 0, 0, 0, 0 };

const ArraySpec kSpec_PtrZero = {                 // 0x180008d60, 0x1800764c0
    "Array<InternalPtr>", 8, 8, &Assign8Z, &InitZero8, nullptr, true, 0, 0, 0, 0, 0, 0 };

const ArraySpec kSpec_Transform = {               // 0x180059140  Array<Transform>
    "Array<Transform>", 0x1c, 4, &Assign1c, &InitIdentityTransform, nullptr, true, 0, 0, 0, 0, 0, 0 };

const ArraySpec kSpec_I32Neg1 = {                 // 0x18007b960  Array<int>, fill -1
    "Array<int32>(-1)", 4, 4, &Assign4, &InitNeg1, nullptr, true, 0, 0, 0, 0, 0, 0 };

const ArraySpec kSpec_Fill8 = {                   // 0x18001f990  Array<T*>
    "Array<ptr>", 8, 8, &Assign8, nullptr, nullptr, true, 0, 0, 0, 0, 0, 0 };

const ArraySpec kSpec_Fill8Z = {                  // 0x180076900, 0x18007a0d0
    "Array<InternalPtr>(fill)", 8, 8, &Assign8Z, nullptr, nullptr, true, 0, 0, 0, 0, 0, 0 };

const ArraySpec kSpec_Fill4 = {                   // 0x180023830  Array<uint32>
    "Array<uint32>", 4, 4, &Assign4, nullptr, nullptr, true, 0, 0, 0, 0, 0, 0 };

const ArraySpec kSpec_Fill16 = {                  // 0x18004f790, 0x18004f8e0
    "Array<callback16>", 0x10, 8, &Assign16, nullptr, nullptr, true, 0, 0, 0, 0, 0, 0 };

// WAVE 9 LANE UIF — Array<UI::SpriteDraw>, the sprite batch behind
// SpriteContext::AddSpriteDraw (append, 0x180078520) and
// SpriteContext::FlushDeferred (clear, 0x1800783d0). stride 0x28, align 8
// (`MOV EDX,0x8` at 0x18007857b and 0x18007841b in the two bodies).
//
// ASSIGN arm covers the whole element (five qwords) -> no unwritten window.
// INIT arm leaves TWO holes, +0x1c..+0x1f and +0x25..+0x27 (see
// InitSpriteDraw). `ArraySpec` carries ONE window per arm, so only the first
// is expressible here. That is NOT a live oracle risk for either registration
// below and this is why, stated rather than assumed: the init arm runs only
// for elements in [oldSize, newSize) when the caller passed NO fill, and
//   * 0x180078520 is the three-argument form — every grown element takes the
//     ASSIGN arm, so the init arm is unreachable through it;
//   * 0x1800783d0 has newSize folded to the constant 0, so it never grows at
//     all and the init arm is unreachable through it too.
// The +0x25..+0x27 hole is recorded here for whoever registers a two-argument
// Array<SpriteDraw>::SetSize, which would be the first caller that can reach
// it (reference_never_written_needs_by_whom.md: the extent belongs to the ARM,
// not to the type). The same one-window limit is already filed against
// kSpec_Sprite38 above.
const ArraySpec kSpec_SpriteDraw = {              // 0x180078520, 0x1800783d0
    "Array<UI::SpriteDraw>", 0x28, 8, &Assign28, &InitSpriteDraw, nullptr, true,
    0, 0, 0x1c, 0x20, 0, 0 };

// --- the non-trivial three --------------------------------------------------
// *** OPEN, filed by Wave 9 lane D2 2026-09-07: this row's unwritten window is
// WRONG, and it is wrong in the direction that fails 100% of comparisons. ***
// Lane D2 disassembled 0x180004220 (the `Assign38Call` target) and found it
// writes +0x00..+0x33 and stops: the element is 0x38 bytes, so **+0x34..+0x37
// is never written by the ASSIGN**. This row says `unwrittenLo = unwrittenHi =
// 0`, i.e. "no such window". It has not bitten because this lane's probe seeds
// all-zero elements, so those four bytes agree by accident.
//
// NOT changed by lane D2, because `unwritten*` is documented in
// array_internal.h as the window retail's DEFAULT (init) arm leaves alone and
// this window is on the ASSIGN arm — widening that meaning from another lane,
// with no way to run this probe, is how an oracle quietly stops comparing bytes
// it should compare. It is lane Q's call whether to widen the field or add a
// second window. See docs/runtime_port/WAVE9_LANE_D2.md.
// ASSIGN-ARM WINDOW +0x34..+0x37, read out of 0x180004220 for this correction:
// it refcount-assigns the two String pointers at +0x00 and +0x08, copies NINE
// dwords at +0x10,+0x14,+0x18,+0x1c,+0x20,+0x24,+0x28,+0x2c,+0x30 -- and STOPS.
// The element is 0x38 bytes, so +0x34..+0x37 is never written. That is exactly
// runtime/types/ui/SpriteSheet.h's `_pad34`: retail's own copy-assign does not
// copy the padding, which independently confirms the field IS padding.
//
// This spec has NO init arm (it is the three-argument form, so the new tail
// goes through `assign` with `fill` too), which is why the init window is 0/0
// and why the assign window must be excluded for EVERY element the loop
// touched, not just the tail. Found by lane D2 while porting 0x1800059e0 and
// filed as latent because the probe was seeding zeros -- see WAVE9_LANE_Q.md
// 3.4b.
//
// SEED ZEROS +0x00..+0x0f: both String slots are dereferenced (`INC [RAX-4]`
// in the assign, a refcount release in 0x1800059e0). Everything from +0x10 on
// is plain scalars and IS seeded non-zero, which is what makes the +0x34 window
// testable at all.
const ArraySpec kSpec_Sprite38 = {                // 0x180004e30
    "Array<SpriteEntry:0x38>", 0x38, 8, &Assign38Call, nullptr, &Destroy38, false,
    /*assign unwritten*/ 0x34, 0x38,
    /*init   unwritten*/ 0,    0,
    /*seed zero       */ 0x00, 0x10 };

// BOTH ARMS SKIP +0x2c..+0x2f, for two independent reasons -- which is a fact,
// not an assumption. The init arm is InitEffectSlot above. The assign arm,
// 0x180051650, was disassembled for this correction: dwords at +0x00,+0x04,
// +0x08,+0x0c, qwords at +0x10,+0x18, dwords at +0x20,+0x24,+0x28 -- and then
// it jumps straight to +0x30. +0x2c is skipped there too. It finishes by
// zeroing +0x30 and +0x38 and calling 0x180050d30 on the sub-object at +0x30,
// so the tail IS fully initialised before that call and has no window of its
// own.
//
// SEED ZEROS +0x30..+0x3f: that sub-object goes to 0x180050d30 (lane D's J10,
// unread by this lane) on the copy path and to the destroy-range 0x180052110
// on the free path, and a garbage pointer there would be freed. +0x00..+0x2f is
// plain scalars and IS seeded non-zero.
const ArraySpec kSpec_EffectSlot = {              // 0x180051390
    "Array<EffectSlot:0x40>", 0x40, 8, &Assign40Call, &InitEffectSlot, &Destroy40,
    false,
    /*assign unwritten*/ 0x2c, 0x30,
    /*init   unwritten*/ 0x2c, 0x30,
    /*seed zero       */ 0x30, 0x40 };

// NO WINDOW EITHER ARM: AssignStringExt writes both qwords of the 0x10-byte
// element (+0x00 via the refcounted path, +0x08 verbatim), so the assign covers
// the whole stride, and there is no init arm. Checked against 0x180061480's
// inlined loop rather than assumed, because the two specs above both turned out
// to have one.
//
// SEED ZEROS +0x00..+0x07: the String slot is dereferenced by the assign's
// `INC [RAX-4]` and by 0x180005930's release. +0x08, the ExternalPtr, is copied
// verbatim and never dereferenced by either, so it IS seeded non-zero.
const ArraySpec kSpec_StringExt = {               // 0x180061480
    "Array<{String*,ExternalPtr}>", 0x10, 8, &AssignStringExt, nullptr,
    &DestroyStringExt, true,
    /*assign unwritten*/ 0,    0,
    /*init   unwritten*/ 0,    0,
    /*seed zero       */ 0x00, 0x08 };

// --- WAVE 9 LANE UIW's two ------------------------------------------------
//
// NO WINDOW EITHER ARM. AssignStringRef's unconditional `dst = 0` before the
// null test means both arms write all 8 bytes; there is no init arm.
// SEED ZEROS the whole stride: the assign dereferences the source pointer
// (`INC [RAX-4]`) and the destroy dereferences and frees the destination's.
const ArraySpec kSpec_StringRef = {               // 0x180034860
    "Array<System::String>", 8, 8, &AssignStringRef, nullptr,
    &DestroyStringRange, false,
    /*assign unwritten*/ 0,    0,
    /*init   unwritten*/ 0,    0,
    /*seed zero       */ 0x00, 0x08 };

// --- WAVE 10 LANE UIORACLE -------------------------------------------------
//
// 0x180029a80, the TWO-ARGUMENT sibling of 0x180034860 -- the one
// array_destroy.cpp's Wave 9 note predicted would arrive: "It will get a
// SECOND caller the day 0x180029a80 (the two-argument sibling,
// `TextElement::SetParameterCount`'s) lands." That day is this commit.
//
// IT IS *NOT* kSpec_StringRef, AND THE DIFFERENCE IS THE ONE FIELD THE
// TWO-ARGUMENT FORM EVEN HAS. kSpec_StringRef's `init` is null, which this
// file's ArraySpec comment defines as "retail leaves the new tail untouched"
// -- true of 0x180034860, whose only caller passes a fill so the tail always
// takes the ASSIGN arm and there is no else arm to describe. 0x180029a80 has
// one, and it is not empty:
//
//     180029b2a  MOV qword ptr [RDX],R10      ; R10 == 0 -- the `else` arm
//
// so the tail is ZEROED. Reusing kSpec_StringRef here -- which was the first
// draft, on the "same element type, same destroy, same assign" argument --
// would have left every newly grown System::String slot holding whatever the
// fresh MemAlloc block contained, and the next SetParameterCount's destroy arm
// would have decremented a refcount through that garbage. Similarity is
// corroboration, not evidence (kSpec_U64ZeroA4's note, same trap, different
// field).
//
// Everything else is read from the same disassembly and agrees with
// kSpec_StringRef: stride 8 (`SHL RCX,0x3` at 180029ad3), align 8 (`MOV EDX,0x8`
// at 180029ad7), assign = zero-then-copy-with-addref (180029b11..180029b20),
// destroy = 0x180011010 (kD_Str_08, already ported and registered in
// array_destroy.cpp) on BOTH the realloc arm (180029b40) and the shrink arm
// (180029b8f).
const ArraySpec kSpec_StringRefZ = {              // 0x180029a80
    "Array<System::String>(zero tail)", 8, 8, &AssignStringRef, &InitZero8,
    &DestroyStringRange, false,
    /*assign unwritten*/ 0,    0,
    /*init   unwritten*/ 0,    0,
    /*seed zero       */ 0x00, 0x08 };

// ASSIGN WINDOW +0x04..+0x07: the colour is ONE dword and the nested Array
// starts at +0x08, so four bytes of every element are never written by anybody.
// There is no init arm (the only caller, GraphElement::AddGraph, passes a fill).
// SEED ZEROS +0x08..+0x17: the nested Array header is destroyed (its buffer
// freed) by the assign's `operator=` before being overwritten.
const ArraySpec kSpec_GraphSeries = {             // 0x1800417d0
    "Array<GraphSeries>", 0x18, 8, &AssignGraphSeries, nullptr,
    &DestroyGraphSeriesRange, false,
    /*assign unwritten*/ 0x04, 0x08,
    /*init   unwritten*/ 0,    0,
    /*seed zero       */ 0x08, 0x18 };

// ============================================================================
// Live instrumentation.
//
// One RtDiffSite PER ADDRESS, not per body. That distinction is the whole risk
// of this lane: eleven addresses share one J1 body, so a run that exercises the
// body proves nothing about the other ten REGISTRATIONS, and a mistyped address
// would leave a call site quietly on the DLL (harmless today, a missing symbol
// at cutover). Per-address `calls=` in the RtDiff summary is how a run says
// which of the 27 it actually reached.
//
// These sites are a COVERAGE COUNTER AND A CHEAP INVARIANT, NOT AN INDEPENDENT
// ORACLE. The header check below re-derives retail's own growth rule, so a port
// that got the rule wrong would agree with it. The independent oracle is the
// synthetic-subject replay in array_probe.cpp, which compares against the
// SHIPPED body. Do not read a green summary here as a verified port; read
// `[RtArrayProbe]` for that. The one genuinely independent thing checked here
// is the copy prefix (see below), which compares the new buffer against the old
// one before the old one is freed.
// ============================================================================

// *** A Site MUST HAVE STATIC STORAGE DURATION. ***
// RtDiffRegister stores the POINTER in rt_diff.cpp's `g_sites[]` and reads it
// back at RtDiffSummary() time, so a Site on the stack is a dangling pointer by
// the time the summary runs. It does not merely print rubbish: the summary's
// plausibility check reports it as "sites were CORRUPTED -- a stray write hit
// the harness's own statics", which is this project's most alarming diagnostic
// and names its worst failure family. The first draft of the probe passed a
// LOCAL Site from inside its per-case loop, and 16 targets x 9 cases produced
// 144 registrations of one stack address and one such line. A harness that
// cries wild-write for a benign cause is worse than one that says nothing.
//
// Every Site in this file is therefore a file-scope object, one per registered
// address, and SetSizeCore/ArrayDtorSited accept a NULL site for callers that
// want the body without the instrumentation.
//
// Two related facts about rt_diff.cpp, worth knowing before adding sites in
// bulk: registration is an UNSYNCHRONISED `g_sites[g_siteCount++]`, and it is
// bounds-checked against kMaxSites = 1024 by silently dropping anything past
// the cap. 27 static sites from this lane is not near it; 144 dynamic ones per
// run would have been on the way there.
//
// `struct Site { RtDiffSite d; uintptr_t addr; }` is declared in
// array_internal.h. The RtDiff macros want a `rt_diff_site__` in lexical scope
// and these sites are chosen at runtime, so the bodies below call
// RtDiffRegister/RtDiffReport directly rather than through RT_DIFF_*.

}  // namespace rtcoll

// ============================================================================
// The two bodies, with the per-address plumbing around them.
// ============================================================================
namespace rtcoll {

// Armed at init by RtCollectionPortInit; see array_internal.h for why the probe
// cannot run there. `g_probeRunning` breaks the recursion, because the probe
// drives SetSizeCore itself.
bool g_probePending = false;
bool g_probeRunning = false;

inline void ProbeGate() {
    if (!g_probePending || g_probeRunning) return;
    g_probeRunning = true;
    RtCollectionProbeRun();
    g_probePending = false;
    g_probeRunning = false;
}

// `site` may be NULL, and the probe passes NULL. See the "A Site must have
// static storage duration" note above: a null site means "do the work, touch
// nothing in the diff registry".
void ArrayDtorSited(ArrayHdr* a, Site* site) {
    ProbeGate();
    if (!site || !RtDiffEnabled()) { ArrayDtorImpl(a); return; }

    RtDiffRegister(&site->d);
    // Ours writes, and the post-state is the whole observable effect on the
    // object: retail's last two stores are `_data = 0` and one qword covering
    // _size and _capacity. A wrong pointer handed to Free is not visible here
    // and is caught instead by BANISHED_REDZONE=watch, which is why the
    // recommended J1 run pairs the two.
    ArrayDtorImpl(a);
    const bool ok = (a->_data == nullptr && a->_size == 0 && a->_capacity == 0);
    char ours[96], theirs[96];
    std::snprintf(ours, sizeof(ours), "data=%p size=%d cap=%d",
                  static_cast<void*>(a->_data), a->_size, a->_capacity);
    std::snprintf(theirs, sizeof(theirs), "data=0000000000000000 size=0 cap=0");
    RtDiffReport(&site->d, ok, ours, theirs);
}

// ---------------------------------------------------------------------------
// WAVE 9 LANE DESTROY — arm coverage for kSpec_PkgEntry's destroy step
// ---------------------------------------------------------------------------
// 0x180026b50 is called from TWO different branches of 0x1800298f0 and a run
// that only ever took one of them has verified one of them. Nothing OUTSIDE
// this function can tell the two apart -- the callee is handed (base, count) in
// both cases, and `base == old` vs `base == old + newSize*stride` is not
// distinguishable at the callee when newSize is 0. So the count is taken HERE,
// at the branch itself (WAVE9.md "count branches taken INSIDE the function
// under test, not proxies observed from outside it").
//
// One Site per ARM, so the RtDiff summary's per-site `calls=` IS the arm count
// -- the same idiom array_destroy.cpp uses to get per-ADDRESS counts out of one
// shared body. Both Sites carry the entry-point address they lead to so a
// summary line names something greppable.
//
// Scoped to kSpec_PkgEntry on purpose. A generic version would add a Site pair
// per spec or, worse, sum every spec's arms into one pair and report a number
// that is true of nothing. This lane's subject is one spec; when another spec
// needs arm coverage it gets its own pair, not a widened predicate here.
Site g_pkgArmRealloc = { { "kSpec_PkgEntry destroy arm: REALLOC (0x180029a0e -> 0x180026b50)",
                           0, 0, false, false }, 0x180026b50 };
Site g_pkgArmShrink  = { { "kSpec_PkgEntry destroy arm: SHRINK-TAIL (0x180029a68 -> 0x180026b50)",
                           0, 0, false, false }, 0x180026b50 };

static void NotePkgEntryDestroyArm(const ArraySpec* spec, bool reallocArm, int count) {
    if (spec != &kSpec_PkgEntry || !RtDiffEnabled()) return;
    Site* s = reallocArm ? &g_pkgArmRealloc : &g_pkgArmShrink;
    RtDiffRegister(&s->d);
    // Not a comparison and it does not pretend to be one: the predicate is the
    // one thing the branch itself guarantees (retail never calls the destroy
    // with count <= 0 from either arm -- the realloc arm passes oldSize, which
    // is 0 only when there was nothing to destroy, and the shrink arm passes
    // oldSize-newSize under `oldSize > newSize`). The VALUE of this Site is
    // `calls=`, and it is labelled so nobody reads it as an oracle.
    char ours[96];
    std::snprintf(ours, sizeof(ours), "arm=%s n=%d",
                  reallocArm ? "realloc" : "shrink-tail", count);
    RtDiffReport(&s->d, count >= 0, ours, "coverage only (n >= 0)");
}

// The core. `fill` non-null selects the three-argument form; otherwise the
// spec's `init` (which may itself be null) supplies the new tail.
void SetSizeCore(ArrayHdr* a, int newSize, const uint8_t* fill,
                 const ArraySpec* spec, Site* site) {
    ProbeGate();
    // NULL `site` = no diff-registry traffic at all. The probe passes NULL, both
    // because its own comparator is strictly stronger than the live checks below
    // and because a probe-owned site would have to be a static per case.
    const bool diff = (site != nullptr) && RtDiffEnabled();
    const uint32_t stride = spec->stride;

    const int32_t oldSizeI = a->_size;
    const int64_t oldSize  = static_cast<int64_t>(oldSizeI);   // MOVSXD
    if (newSize == oldSizeI) {
        // Retail's exact-equal early return. Counted so the probe and the live
        // summary can tell "entered" from "did work" — a shadow placed after an
        // early return makes calls= mean the wrong thing
        // (reference_probe_needs_an_effectiveness_control.md).
        if (diff) {
            RtDiffRegister(&site->d);
            RtDiffReport(&site->d, true, "noop(n==size)", "noop(n==size)");
        }
        return;
    }

    uint8_t* const old    = a->_data;
    const int32_t  oldCap = a->_capacity;
    a->_size = newSize;

    if (newSize > a->_capacity) {                 // signed 32-bit
        a->_capacity = newSize;
        // 32-bit LEA arithmetic, wrap included.
        if (newSize == static_cast<int32_t>(oldSizeI + 1) && oldSizeI != 0)
            a->_capacity = static_cast<int32_t>(newSize * 3);
        a->_data = static_cast<uint8_t*>(
            MemAlloc(static_cast<uint64_t>(static_cast<int64_t>(a->_capacity)) * stride,
                     spec->align));               // MOVSXD then SHL/IMUL
    }

    const bool realloced = (old != a->_data);
    for (int64_t i = realloced ? 0 : oldSize; i < static_cast<int64_t>(newSize); ++i) {
        uint8_t* dst = a->_data + i * stride;
        if (dst == nullptr) continue;             // retail tests the COMPUTED address
        if (i < oldSize)      spec->assign(dst, old + i * stride);
        else if (fill)        spec->assign(dst, fill);
        else if (spec->init)  spec->init(dst);
        // else: retail's else arm is empty and the new tail stays uninitialised
    }

    // The one independent check available at a live call site: on a realloc the
    // copied prefix must equal what is still sitting in the old buffer, and the
    // old buffer is not freed yet. This does NOT re-derive our own rule — it
    // compares against the source data — but it only means anything when the
    // element assignment is a pure byte copy, so `byteCopy` gates it.
    if (diff && realloced && old && spec->byteCopy) {
        RtDiffRegister(&site->d);
        const int64_t n = (oldSize < newSize ? oldSize : newSize) * stride;
        const bool ok = (n <= 0) || std::memcmp(a->_data, old, static_cast<size_t>(n)) == 0;
        char ours[128], theirs[128];
        std::snprintf(ours,   sizeof(ours),   "%s copied %lld bytes", spec->name, (long long)n);
        std::snprintf(theirs, sizeof(theirs), "%s source  %lld bytes", spec->name, (long long)n);
        RtDiffReport(&site->d, ok, ours, theirs);
    }

    if (realloced) {
        if (spec->destroy) {
            NotePkgEntryDestroyArm(spec, /*reallocArm=*/true, oldSizeI);
            spec->destroy(nullptr, old, oldSizeI);
        }
        MemFree(old);                             // null-guarded inside 0x1800c7ad0
    } else if (spec->destroy && oldSizeI > newSize) {
        NotePkgEntryDestroyArm(spec, /*reallocArm=*/false, oldSizeI - newSize);
        spec->destroy(nullptr, old + static_cast<int64_t>(newSize) * stride,
                      oldSizeI - newSize);
    }

    // Header algebra. WEAK BY CONSTRUCTION (it restates the rule this function
    // implements) and kept for its coverage value: a non-zero calls= on this
    // site is what says the address was reached at all.
    if (diff) {
        RtDiffRegister(&site->d);
        int32_t wantCap = oldCap;
        if (newSize > oldCap) {
            wantCap = newSize;
            if (newSize == static_cast<int32_t>(oldSizeI + 1) && oldSizeI != 0)
                wantCap = static_cast<int32_t>(newSize * 3);
        }
        const bool ok = (a->_size == newSize && a->_capacity == wantCap);
        char ours[128], theirs[128];
        std::snprintf(ours,   sizeof(ours),   "size=%d cap=%d", a->_size, a->_capacity);
        std::snprintf(theirs, sizeof(theirs), "size=%d cap=%d", newSize, wantCap);
        RtDiffReport(&site->d, ok, ours, theirs);
    }
}

}  // namespace rtcoll

// ============================================================================
// The 27 registered entry points.
//
// One free function per ADDRESS. They are thin — a tail call in a normal run,
// because RtDiffEnabled() is false and every diff block compiles to a
// predictable branch — and they exist so that each address owns an RtDiffSite
// and so a run can report per-address `calls=`.
//
// The naming is `RtColl_<Job>_<address>` on purpose: when the diff summary or a
// crash log names one of these, the address is right there.
// ============================================================================

namespace rtcoll {

#define J1_ENTRY(ADDR)                                                        \
    Site g_j1_##ADDR = { { "Collection::Array::~Array @0x" #ADDR, 0, 0, false, false }, \
                         0x##ADDR };                                          \
    void* RtColl_ArrayDtor_##ADDR(void* a) {                                  \
        ArrayDtorSited(static_cast<ArrayHdr*>(a), &g_j1_##ADDR);              \
        return nullptr;                                                       \
    }

J1_ENTRY(180004890)   // Achievement: Array<ExternalPtr<Goal>>
J1_ENTRY(18001bf50)   // Graphics:    Array<Geometry*>, ~Geometry's tail call
J1_ENTRY(18001c750)   // Graphics:    Array<int32>, the Scene dtor's four middle calls
J1_ENTRY(18001c9e0)   // Graphics:    Renderer::_drawables
J1_ENTRY(180033960)   // Engine:      MainLoop::_postDraw   (+0x80)
J1_ENTRY(180033a40)   // Engine:      MainLoop::_postUpdate (+0x90)
J1_ENTRY(180051d80)   // Audio:       Scene's channel array
J1_ENTRY(18005fc20)   // Graphics:    Scene's Array<Node>
J1_ENTRY(1800737f0)   // Picking:     Scene+0x08, Array<int>
J1_ENTRY(1800739e0)   // Picking:     Scene+0x18, Array<Cell>  (also the AddMesh scratch)
J1_ENTRY(180073bb0)   // Picking:     Scene+0x30, Array<Link>
// WAVE 9 LANE UIF, 2026-09-08 — THE TWELFTH ADDRESS. Lane D's survey found
// eleven; there are twelve. 0x180037240 is `UI::Element::_children`'s
// `~Array<Element*>`, the call at 0x1801161f7 in `??1Element@UI@@UEAA@XZ` on
// `this+0xa0`. It was invisible to the survey because nothing in our tree
// called it until this lane ported that destructor.
//
// It is the SAME BODY, and that is a byte-level claim, not an eyeball one:
// 0x180037240 and 0x180033960 were compared over all 115 bytes and differ at
// exactly eleven offsets (39,40, 60,61, 71,72, 76,77,78, 86,87) — every one of
// them inside a RIP-relative displacement or a CALL rel32, and every one
// resolving to the same four absolute operands the J1 note above pins
// (0x1801fc718, 0x18014b390, 0x1800c6c00, 0x18014b388). Same instructions,
// same immediates, same targets, so the "no parameters at all, not even a
// stride" conclusion covers it unchanged and one body still serves them all.
J1_ENTRY(180037240)   // UI:          Element::_children (+0xa0), Array<Element*>
// WAVE 9 LANE UIW, 2026-09-08 — THE THIRTEENTH AND FOURTEENTH ADDRESSES, both
// reached by `?RemoveAllItems@ComboElement@UI@@` (0x180075120): its second and
// third calls, on `_sprites` (+0xf8) and `_userData` (+0x108).
//
// Same byte-level claim as the twelfth, made the same way and not by eye. Both
// were read with `read_memory` over all 115 bytes and compared against
// 0x180033960. 0x180037310 differs at exactly eleven offsets and 0x180024aa0 at
// exactly fifteen; EVERY differing byte in both lies inside one of the five
// relocated operands (the disp32 of `MOV RBX,[rip]`, `CALL [rip]`,
// `MOV RCX,[rip]`, the `CALL rel32`, and the second `CALL [rip]`), and all five
// resolve in BOTH bodies to the same absolute targets the J1 note above pins:
// 0x1801fc718, 0x18014b390, 0x1801fc718, 0x1800c6c00, 0x18014b388. Outside
// those five windows the three bodies are byte-for-byte equal.
J1_ENTRY(180037310)   // UI:          ComboElement::_sprites  (+0xf8)
J1_ENTRY(180024aa0)   // UI:          ComboElement::_userData (+0x108)

// WAVE 9 LANE UIC, 2026-09-08 — THE THIRTEENTH AND FOURTEENTH. Lane UIF's note
// above says "there are twelve"; there are at least fourteen, and its own
// mechanism explains why: a J1 address is invisible to the survey until
// something in our tree calls it. These two surfaced from the OTHER direction —
// from reading `?AddController@DialogInstance@UI@@` (0x180113470), which the
// integrator's unregistered-address-door census flagged, and where they appear
// as two of the five "unexported internals" WAVE9_LANE_UIF.md row 10 declined
// `?CreateInstance@ElementDescription@` on. They are not opaque internals. They
// are this.
//
// SAME BODY, established the same way and with the same numbers:
// tools/uic_bytediff.py compares all 115 bytes against 0x180033960 and resolves
// every differing byte back to the absolute operand it encodes.
//   0x180009460 — 10 differences, at offsets 40,41 61,62 72,73 77,78 87,88
//   0x180039440 — 11 differences, at offsets 39,40 60,61 71,72 76,77,78 86,87
//                 (byte-for-byte the same diff-set lane UIF reports for
//                  0x180037240, which is corroboration that the reference and
//                  the method are both unchanged)
// ZERO differences outside a displacement in either, and all five relocatable
// operands resolve to the same four absolutes (0x1801fc718 twice, 0x18014b390,
// 0x1800c6c00, 0x18014b388).
//
// A NOTE ON THE MEASUREMENT ITSELF, because it nearly went the other way. The
// first version of uic_bytediff.py carried a HARDCODED offset table for those
// five fields, decoded four bytes one position off for one of them, and printed
// "*** MISMATCH *** -> 0x1917d29b4" — a confident, specific, wrong refutation,
// for the REFERENCE body, which is registered and known good. Locating the
// fields by instruction pattern instead is what produced the numbers above.
// (reference_recheck_must_match_its_own_proof.md: a check whose own decode is
// wrong reads as a finding about the subject.)
J1_ENTRY(180009460)   // UI:          reached from ?AddController@DialogInstance@ 0x18011363f
J1_ENTRY(180039440)   // UI:          reached from ?AddController@DialogInstance@ 0x180113649
// WAVE 9 LANE PKG2 rung 4b-iii — the two array dtors ?~CompressedStream@IO@@
// (0x1800aa650) calls on its own members, at +0x40 and +0x30. NOT byte-identical
// to the already-ported 0x180024aa0 it calls at +0x50 (first difference at byte
// 39 of 256 for both), so they get their own entries rather than a shared
// registration — the same discipline the SetSize family forced twice this rung.
// ~Array for a trivially-destructible T frees _data and zeroes the header, so
// the element size that differs between them does not reach the body.
J1_ENTRY(1800249c0)   // IO:          CompressedStream::_headers (+0x40), Array<Header>
J1_ENTRY(1800248e0)   // IO:          CompressedStream::_windows (+0x30), Array<Window>

#undef J1_ENTRY

// 0x18001cdf0 — `Array<int32>::operator=`. Body and evidence above.
Site g_uiw_18001cdf0 = { { "Collection::Array<int32>::operator= @0x18001cdf0",
                           0, 0, false, false }, 0x18001cdf0 };
void* RtColl_I32ArrayAssign_18001cdf0(void* dst, const void* src) {
    ProbeGate();
    void* r = RtColl_I32ArrayAssign(static_cast<uint8_t*>(dst),
                                    static_cast<const uint8_t*>(src));
    if (RtDiffEnabled()) {
        RtDiffRegister(&g_uiw_18001cdf0.d);
        // INDEPENDENT of the rule this body implements: it compares the copied
        // buffer against the SOURCE, which this function did not write. It is
        // silent when dst == src (retail's early return) and when the size is
        // zero, so the `worked` flag distinguishes "entered" from "did work"
        // (reference_probe_needs_an_effectiveness_control.md).
        const ArrayHdr* d = static_cast<const ArrayHdr*>(dst);
        const ArrayHdr* s = static_cast<const ArrayHdr*>(src);
        if (dst != src && d->_size > 0) {
            const size_t n = static_cast<size_t>(d->_size) * 4;
            const bool ok = (d->_size == s->_size) &&
                            std::memcmp(d->_data, s->_data, n) == 0;
            char ours[96], theirs[96];
            std::snprintf(ours,   sizeof(ours),   "copied %d int32", d->_size);
            std::snprintf(theirs, sizeof(theirs), "source %d int32", s->_size);
            RtDiffReport(&g_uiw_18001cdf0.d, ok, ours, theirs);
        }
    }
    return r;
}


// --- J3: the two-argument form, `SetSize(this, int n)` ----------------------
#define J3_ENTRY2(ADDR, SPEC)                                                 \
    Site g_j3_##ADDR = { { "Collection::Array::SetSize @0x" #ADDR, 0, 0, false, false }, \
                         0x##ADDR };                                          \
    void RtColl_ArraySetSize_##ADDR(void* a, int n) {                         \
        SetSizeCore(static_cast<ArrayHdr*>(a), n, nullptr, &SPEC, &g_j3_##ADDR); \
    }

J3_ENTRY2(180008b90, kSpec_Bool)        // Achievement: Array<bool>, tail UNINITIALISED
J3_ENTRY2(180008d60, kSpec_PtrZero)     // Achievement: Array<InternalPtr>
J3_ENTRY2(180008ff0, kSpec_U64Zero)     // Achievement: Array<Time>
J3_ENTRY2(18002fea0, kSpec_I32)         // Object/UI:   Array<int32>, tail UNINITIALISED
J3_ENTRY2(180059140, kSpec_Transform)   // Picking:     Array<Transform>, 4x unrolled in retail
J3_ENTRY2(1800764c0, kSpec_PtrZero)     // UI:          Array<InternalPtr<Element>>
J3_ENTRY2(180051390, kSpec_EffectSlot)  // Audio:       Array<EffectSlot>  [NON-TRIVIAL]
// WAVE 10 LANE UIORACLE — TextElement::_parameters (+0xc8) [NON-TRIVIAL]
J3_ENTRY2(180029a80, kSpec_StringRefZ)  // UI:          Array<System::String>, tail ZEROED
// WAVE 9 LANE PKG2 rung 4b, 2026-09-08 — two more two-argument instantiations,
// both reached from IO::ReadPackfile (the packfile entry directory). The
// integrator's guess that these would be a REGISTRATION rather than a port was
// half right: they belong to this family, but neither is byte-identical to any
// member already here, so each needs its own entry. The spec is read out of the
// body, not inferred from the caller's element type:
//   0x180029510  SHL RCX,0x2 / MOV EDX,0x4 / `MOV EAX,[RSI+RCX*4]; MOV [RDX],EAX`
//                and the copy loop JGEs past the tail, so the tail is left
//                UNINITIALISED: kSpec_I32, same as 0x18002fea0.
//   0x180029630  NO shift at all (element size 1) / MOV EDX,0x1 / `MOVZX EAX,
//                byte [RCX+RDI]; MOV [RDX],AL`, tail likewise UNINITIALISED:
//                kSpec_Bool, same as 0x180008b90.
// Byte-similarity agrees with both readings (first difference at 93 and 89
// respectively, i.e. a shared ~90-byte prologue then the element op) — but the
// similarity is the CORROBORATION, not the evidence.
J3_ENTRY2(180029510, kSpec_I32)         // IO:          Array<int32>, tail UNINITIALISED
J3_ENTRY2(180029630, kSpec_Bool)        // IO:          Array<bool>,  tail UNINITIALISED
// Rung 4b-ii: the packfile block directory. 8-byte elements, tail ZEROED
// (`MOV [RDX],R10` with R10 == 0), but align 4 — see kSpec_U64ZeroA4 above.
J3_ENTRY2(180024d70, kSpec_U64ZeroA4)   // IO:          Array<CompressedStream::Header>
J3_ENTRY2(180024c10, kSpec_Window)      // IO:          Array<CompressedStream::Window>

#undef J3_ENTRY2

// --- J3: the three-argument form, `SetSize(this, int n, const T* fill)` -----
#define J3_ENTRY3(ADDR, SPEC)                                                 \
    Site g_j3_##ADDR = { { "Collection::Array::SetSize @0x" #ADDR, 0, 0, false, false }, \
                         0x##ADDR };                                          \
    void RtColl_ArraySetSizeFill_##ADDR(void* a, int n, const void* fill) {   \
        SetSizeCore(static_cast<ArrayHdr*>(a), n,                             \
                    static_cast<const uint8_t*>(fill), &SPEC, &g_j3_##ADDR);  \
    }

J3_ENTRY3(180004e30, kSpec_Sprite38)    // UI:       SpriteSheet's 0x38 element [NON-TRIVIAL]
J3_ENTRY3(18001f990, kSpec_Fill8)       // Graphics: Array<Geometry*>
J3_ENTRY3(180023830, kSpec_Fill4)       // Graphics: Array<uint32>
J3_ENTRY3(18004f790, kSpec_Fill16)      // Engine:   MainLoop::_postDraw push
J3_ENTRY3(18004f8e0, kSpec_Fill16)      // Engine:   MainLoop::_postUpdate push
J3_ENTRY3(180061480, kSpec_StringExt)   // Graphics: Material's {String*,ExternalPtr} [NON-TRIVIAL]
J3_ENTRY3(180076900, kSpec_Fill8Z)      // UI:       Element::AddChild's child array
J3_ENTRY3(18007a0d0, kSpec_Fill8Z)      // Engine:   the dialog pending-list push
// WAVE 9 LANE UIW, 2026-09-08 — five more three-argument instantiations, all
// reached from the UI cutover residue. THREE OF THE FIVE ARE EXACT CLONES of
// specs already above, and that is a claim read out of each body, not assumed
// from the element type:
//   0x180024fe0  ComboElement::_userData  — SHL RCX,3 / MOV EDX,8 / plain
//                `MOV RAX,[src]; MOV [dst],RAX`. No zero-first store, so
//                Assign8, not Assign8Z: kSpec_Fill8, same as 0x18001f990.
//   0x18004bfc0  ComboElement::_sprites   — SHL RCX,4 / MOV EDX,8 / two qword
//                copies. kSpec_Fill16, same as MainLoop's two push sites.
//   0x180048fe0  RadioElement::_checkBoxes — SHL RCX,3 / MOV EDX,8 / `MOV
//                [dst],R11(=0)` THEN `MOV RAX,[src]; MOV [dst],RAX`. The
//                zero-first store is the smart-pointer element's operator=,
//                so kSpec_Fill8Z, same as Element::AddChild's own array.
// The other two carry the new specs above.
J3_ENTRY3(180024fe0, kSpec_Fill8)       // UI:       ComboElement::_userData  (+0x108)
J3_ENTRY3(18004bfc0, kSpec_Fill16)      // UI:       ComboElement::_sprites   (+0xf8)
J3_ENTRY3(180048fe0, kSpec_Fill8Z)      // UI:       RadioElement::_checkBoxes(+0xc0)
J3_ENTRY3(1800298f0, kSpec_PkgEntry)    // IO:       PackageFile's entry array [NON-TRIVIAL]
J3_ENTRY3(180034860, kSpec_StringRef)   // UI:       ComboElement::_strings   (+0xe8) [NON-TRIVIAL]
J3_ENTRY3(1800417d0, kSpec_GraphSeries) // UI:       GraphElement::_graphs    (+0xf0) [NON-TRIVIAL]
// WAVE 9 LANE GFX3 — the Graphics::Scene node array. Registered HERE and not in
// runtime/src/graphics/scene_grid.cpp on purpose: it is a member of this
// family, and a private second copy of SetSizeCore in the graphics tree is
// exactly the drift this parameterisation exists to prevent. Its oracle is the
// graphics grid probe, whose append arm drives it against the shipped body.
J3_ENTRY3(18005fb00, kSpec_Node18)      // Graphics: Array<Scene::Node>
// WAVE 9 LANE UIF — the UI sprite batch. Registered HERE for the same reason
// lane GFX3 gave for 0x18005fb00: it is a member of this family, and a private
// second copy of SetSizeCore in the UI tree is the drift this parameterisation
// exists to prevent. Its only caller is SpriteContext::AddSpriteDraw
// (0x18011c890, ported in runtime/src/ui/spritecontext.cpp), which appends one
// element by passing `size + 1` and a pointer to the record.
J3_ENTRY3(180078520, kSpec_SpriteDraw)  // UI:          Array<SpriteDraw> append

#undef J3_ENTRY3

// --- J3: the constant-folded one-argument form ------------------------------
// 0x18007b960 is `Array<int>::SetSize(10000)` with the fill value -1, and BOTH
// are immediates in the shipped body: `CMP ESI,0x2710`, `MOV [RCX+8],0x2710`,
// `MOV [RCX+0xc],0x7530` (the folded 3 * 10000) and `MOV [RDX],0xffffffff`.
// It takes ONE argument. Registering it behind the two- or three-argument
// signature would read a register the caller never set.
Site g_j3_18007b960 = { { "Collection::Array::SetSize(10000,-1) @0x18007b960", 0, 0, false, false },
                        0x18007b960 };
void RtColl_ArraySetSize_18007b960(void* a) {
    SetSizeCore(static_cast<ArrayHdr*>(a), 10000, nullptr, &kSpec_I32Neg1,
                &g_j3_18007b960);
}

// WAVE 9 LANE UIF — 0x1800783d0 is `Array<UI::SpriteDraw>::SetSize(0)`, the
// SAME one-argument shape as 0x18007b960 above with the new size folded to the
// constant 0. It is NOT a member of the J1 `~Array` family, and reading it as
// one would be a live defect rather than a harmless mislabel — so the
// distinction is spelled out here rather than left to the name:
//
//   ~Array (J1) FREES the buffer unconditionally. This body does not free at
//   all. Its epilogue is `CMP RSI,[RBX] / JZ return` at 0x1800784a1, where RSI
//   is the data pointer captured on entry and [RBX] the current one. Nothing
//   between them can reallocate when the new size is 0 (the grow branch is
//   `CMP [RCX+0xc],0 / JGE skip`, and capacity is never negative), so the two
//   are always equal and the MemFree at 0x1800784d2 is UNREACHABLE. The
//   element loop at 0x180078441 is unreachable for the same reason — its guard
//   is `TEST EAX,EAX / JNS`, and EAX is the old size, which is > 0 on every
//   path that gets there.
//
//   So the whole observable effect is `_size = 0`, capacity and buffer kept.
//   That is exactly what SpriteContext::FlushDeferred (0x18011c6a0) wants: it
//   drains the deferred batch every scissor change and would thrash the heap
//   if this freed. Registering the J1 body here instead would free a buffer
//   retail keeps and hand the next AddSpriteDraw a dangling pointer.
//
// SetSizeCore is still the right implementation, not a hand-written `_size=0`:
// it reproduces the same unreachable branches for the same reasons, so if the
// reasoning above is ever wrong the two disagree under the diff oracle instead
// of agreeing by construction.
Site g_j3_1800783d0 = { { "Collection::Array<SpriteDraw>::SetSize(0) @0x1800783d0", 0, 0, false, false },
                        0x1800783d0 };
void RtColl_ArraySetSize_1800783d0(void* a) {
    SetSizeCore(static_cast<ArrayHdr*>(a), 0, nullptr, &kSpec_SpriteDraw,
                &g_j3_1800783d0);
}

}  // namespace rtcoll

// ============================================================================
// Registration
// ============================================================================
//
// `RegisterRuntimeAddr` and not `RegisterRuntimeExport` for all 27: these
// instantiations have no exported name in either build, so there is nothing to
// register under. tools/runtime_census.py counts address registrations from the
// literal in the call, so every address below is spelled as a literal here even
// though the entry-point name already contains it
// (reference_census_cannot_see_nonliteral_registrations.md: twelve live ports
// were invisible to the census for a full wave because the name was a named
// constant).
//
// *** THIS REGISTRATION CHANGES BEHAVIOUR AT ~20 EXISTING CALL SITES. ***
// `ResolveAddr(Module::Runtime, …)` consults the port registry FIRST
// (reference_resolveaddr_consults_port_registry_first.md), so from the moment
// this runs, runtime/src/{graphics,picking,ui,audio,achievement,engine}/… and
// engine/funcs/engine_funcs.cpp stop reaching the DLL's Array bodies and reach
// these. Several of those files carried comments asserting the opposite; they
// are corrected in the same commit. If this lane is reverted, revert those too.
void RtCollectionPortInit() {
    if (!RtWaveEnabled("BANISHED_RT_COLLECTION")) return;

    using namespace rtcoll;

    // ---- J1: Array<T>::~Array, one body, eleven addresses ------------------
    RegisterRuntimeAddr(0x180004890, (void*)&RtColl_ArrayDtor_180004890);
    RegisterRuntimeAddr(0x18001bf50, (void*)&RtColl_ArrayDtor_18001bf50);
    RegisterRuntimeAddr(0x18001c750, (void*)&RtColl_ArrayDtor_18001c750);
    RegisterRuntimeAddr(0x18001c9e0, (void*)&RtColl_ArrayDtor_18001c9e0);
    RegisterRuntimeAddr(0x180033960, (void*)&RtColl_ArrayDtor_180033960);
    RegisterRuntimeAddr(0x180033a40, (void*)&RtColl_ArrayDtor_180033a40);
    RegisterRuntimeAddr(0x180051d80, (void*)&RtColl_ArrayDtor_180051d80);
    RegisterRuntimeAddr(0x18005fc20, (void*)&RtColl_ArrayDtor_18005fc20);
    RegisterRuntimeAddr(0x1800737f0, (void*)&RtColl_ArrayDtor_1800737f0);
    RegisterRuntimeAddr(0x1800739e0, (void*)&RtColl_ArrayDtor_1800739e0);
    RegisterRuntimeAddr(0x180073bb0, (void*)&RtColl_ArrayDtor_180073bb0);
    // Wave 9 lane UIF: the twelfth J1 address, reached by ??1Element@UI@@.
    RegisterRuntimeAddr(0x180037240, (void*)&RtColl_ArrayDtor_180037240);
    // ---- THIRTEENTH THROUGH SIXTEENTH, and the number is STILL a lower bound.
    //
    // Lanes UIC and UIW each found "the thirteenth and fourteenth" J1 address,
    // working in parallel and without contact. They are FOUR DIFFERENT
    // ADDRESSES. The merge is the evidence for the rule lane UIC stated from
    // its own half of it:
    //
    //   *** A family found by a CALLER-DRIVEN SURVEY is bounded BELOW, not
    //       above. *** Lane D found eleven and said eleven. Lane UIF found the
    //       twelfth and wrote "there are twelve". Two more lanes then found two
    //       each. A J1 address is invisible to a caller-driven survey until
    //       something in OUR tree calls it, so every count this family has ever
    //       carried was a lower bound wearing a count's clothes.
    //
    // So: sixteen registered, and the next lane that ports a UI container
    // should expect a seventeenth rather than be surprised by it.
    //
    // Lane UIW's pair is reached by ?RemoveAllItems@ComboElement@UI@@ and is
    // exercised (calls=6, matching its three teardown helpers exactly).
    RegisterRuntimeAddr(0x180037310, (void*)&RtColl_ArrayDtor_180037310);
    RegisterRuntimeAddr(0x180024aa0, (void*)&RtColl_ArrayDtor_180024aa0);
    RegisterRuntimeAddr(0x1800249c0, (void*)&RtColl_ArrayDtor_1800249c0);  // PKG2 4b-iii
    RegisterRuntimeAddr(0x1800248e0, (void*)&RtColl_ArrayDtor_1800248e0);  // PKG2 4b-iii
    // Lane UIC's pair is NOT reached from our tree yet -- registered ahead of
    // its caller (?AddController@DialogInstance@) so whoever ports that does
    // not have to rediscover them. An address with no caller reports calls=0
    // and is NOT verified by that
    // (reference_uncalled_port_is_not_verified.md); the evidence here is the
    // byte identity to the canonical J1 body, and it is stated as such.
    RegisterRuntimeAddr(0x180009460, (void*)&RtColl_ArrayDtor_180009460);
    RegisterRuntimeAddr(0x180039440, (void*)&RtColl_ArrayDtor_180039440);

    // ---- J3: SetSize, trivially-copyable elements --------------------------
    RegisterRuntimeAddr(0x180008b90, (void*)&RtColl_ArraySetSize_180008b90);
    RegisterRuntimeAddr(0x180008d60, (void*)&RtColl_ArraySetSize_180008d60);
    RegisterRuntimeAddr(0x180008ff0, (void*)&RtColl_ArraySetSize_180008ff0);
    RegisterRuntimeAddr(0x18002fea0, (void*)&RtColl_ArraySetSize_18002fea0);
    RegisterRuntimeAddr(0x180059140, (void*)&RtColl_ArraySetSize_180059140);
    RegisterRuntimeAddr(0x1800764c0, (void*)&RtColl_ArraySetSize_1800764c0);
    RegisterRuntimeAddr(0x18007b960, (void*)&RtColl_ArraySetSize_18007b960);
    RegisterRuntimeAddr(0x180029510, (void*)&RtColl_ArraySetSize_180029510);  // PKG2 4b
    RegisterRuntimeAddr(0x180029630, (void*)&RtColl_ArraySetSize_180029630);  // PKG2 4b
    RegisterRuntimeAddr(0x180024d70, (void*)&RtColl_ArraySetSize_180024d70);  // PKG2 4b-ii
    RegisterRuntimeAddr(0x180024c10, (void*)&RtColl_ArraySetSize_180024c10);  // PKG2 4b-ii
    RegisterRuntimeAddr(0x1800298f0, (void*)&RtColl_ArraySetSizeFill_1800298f0);  // PKG2 4c-ii
    RegisterRuntimeAddr(0x18001f990, (void*)&RtColl_ArraySetSizeFill_18001f990);
    RegisterRuntimeAddr(0x180023830, (void*)&RtColl_ArraySetSizeFill_180023830);
    RegisterRuntimeAddr(0x18004f790, (void*)&RtColl_ArraySetSizeFill_18004f790);
    RegisterRuntimeAddr(0x18004f8e0, (void*)&RtColl_ArraySetSizeFill_18004f8e0);
    RegisterRuntimeAddr(0x180076900, (void*)&RtColl_ArraySetSizeFill_180076900);
    RegisterRuntimeAddr(0x18007a0d0, (void*)&RtColl_ArraySetSizeFill_18007a0d0);
    // ---- Wave 9 lane GFX3: the Graphics::Scene node array ------------------
    RegisterRuntimeAddr(0x18005fb00, (void*)&RtColl_ArraySetSizeFill_18005fb00);
    // ---- Wave 9 lane UIW: the five UI instantiations -----------------------
    RegisterRuntimeAddr(0x180024fe0, (void*)&RtColl_ArraySetSizeFill_180024fe0);
    RegisterRuntimeAddr(0x18004bfc0, (void*)&RtColl_ArraySetSizeFill_18004bfc0);
    RegisterRuntimeAddr(0x180048fe0, (void*)&RtColl_ArraySetSizeFill_180048fe0);
    // The two NON-TRIVIAL ones are registered HERE, unconditionally, and NOT
    // behind BANISHED_RT_ARRAY_NONTRIVIAL. That switch exists because its three
    // members call unexported per-element helpers BY ADDRESS INTO THE DLL, so
    // turning them off returns three whole element types to retail. These two
    // do not: every element operation they use — AssignStringRef,
    // DestroyStringRange, AssignGraphSeries, DestroyGraphSeriesRange and the
    // `Array<int32>::operator=` underneath — is ours (the two destroy-ranges
    // live in array_destroy.cpp and are reached through the registry). Putting
    // them behind that switch would mean flipping it left
    // `ComboElement::AddItem`, `RemoveAllItems` and `GraphElement::AddGraph`
    // calling a null address after cutover, which is a worse failure than the
    // one the switch exists to make cheap to bisect.
    // WAVE 10 LANE UIORACLE — the two-argument Array<System::String>, the one
    // caller of which is ?SetParameterCount@TextElement@UI@@ (ported this wave).
    RegisterRuntimeAddr(0x180029a80, (void*)&RtColl_ArraySetSize_180029a80);
    RegisterRuntimeAddr(0x180034860, (void*)&RtColl_ArraySetSizeFill_180034860);
    RegisterRuntimeAddr(0x1800417d0, (void*)&RtColl_ArraySetSizeFill_1800417d0);

    // ---- Wave 9 lane UIW: the one helper that is neither SetSize nor ~Array
    RegisterRuntimeAddr(0x18001cdf0, (void*)&RtColl_I32ArrayAssign_18001cdf0);
    // WAVE 9 LANE UIF — Array<UI::SpriteDraw>: append (3-arg) and clear (0-arg).
    RegisterRuntimeAddr(0x180078520, (void*)&RtColl_ArraySetSizeFill_180078520);
    RegisterRuntimeAddr(0x1800783d0, (void*)&RtColl_ArraySetSize_1800783d0);
    // 0x180011010, 0x180010ab0 and 0x1800419b0 are NOT registered here: all
    // three belong to runtime/src/collection/array_destroy.cpp, which already
    // registered 0x180010ab0 and, in this same commit, registers the other two
    // now that a caller exists. Registering them here as well would SHADOW that
    // file's bodies with a second copy — see the note above DestroyStringRange.

    // ---- J3: SetSize, NON-TRIVIAL elements ---------------------------------
    // Separately switchable because these three are the only ones that call
    // unexported per-element helpers by address (five new lane-D rows) and the
    // only ones with a shrink-path destroy. An integration run that sees a new
    // failure can halve the search space with one env var.
    if (RtWaveEnabled("BANISHED_RT_ARRAY_NONTRIVIAL")) {
        RegisterRuntimeAddr(0x180004e30, (void*)&RtColl_ArraySetSizeFill_180004e30);
        RegisterRuntimeAddr(0x180051390, (void*)&RtColl_ArraySetSize_180051390);
        RegisterRuntimeAddr(0x180061480, (void*)&RtColl_ArraySetSizeFill_180061480);
    }

    // ---- Wave 9 lane UIC: Collection::LinkedList's three helpers -----------
    // In their own file with their own kill switch (BANISHED_RT_LINKEDLIST) —
    // see runtime/src/collection/linkedlist.cpp for why that separation is
    // deliberate rather than tidiness. Called from here because this is the
    // Collection namespace's one init hook.
    extern int RtCollLinkedListRegister();
    RtCollLinkedListRegister();

    // Wave 9 lane VID2: the four unexported Collection::Set AVL bodies
    // (retrace, both rotations, erase). Same reasoning as the linked list —
    // this is the Collection namespace's one init hook. They are the recorded
    // blocker for Video::Device::PlatformStartup; see
    // runtime/src/collection/set_avl.cpp and docs/runtime_port/WAVE9_LANE_VID2.md.
    if (RtWaveEnabled("BANISHED_RT_SETAVL")) {
        extern int RtCollectionSetAvlRegister();
        const int setAvl = RtCollectionSetAvlRegister();
        // Armed, not run — the probe allocates. It self-schedules on the first
        // call into Application::RegisterMessageCallback.
        const char* p = std::getenv("BANISHED_RT_SETAVL_PROBE");
        RtCollectionSetAvlProbeArm(p && *p && std::strcmp(p, "0") != 0);
        std::printf("[RtPort]   Collection::Set AVL: %d addresses registered\n", setAvl);
    }

    // Wave 9 lane SAVE2: Collection::Set<Pair<String,String>>::Clear and its
    // two helpers (0x18000f7d0 / 0x18000f890 / 0x18000fa90). Same reasoning
    // again — this is the Collection namespace's one init hook. They are the
    // boot-path door under the ported IO::Persistence::ReadSettings; see
    // runtime/src/collection/set_clear.cpp and docs/runtime_port/WAVE9_LANE_SAVE2.md.
    if (RtWaveEnabled("BANISHED_RT_SETCLEAR")) {
        extern int RtCollectionSetClearRegister();
        const int setClear = RtCollectionSetClearRegister();
        // Armed, not run — the probe allocates, and a probe that allocates
        // cannot run from a *PortInit (lane PKG2's null+0x640). It
        // self-schedules on the first real call into Clear.
        const char* pc = std::getenv("BANISHED_RT_SETCLEAR_PROBE");
        RtCollectionSetClearProbeArm(pc && *pc && std::strcmp(pc, "0") != 0);
        std::printf("[RtPort]   Collection::Set Clear: %d addresses registered\n", setClear);
    }

    // Wave 9 lane COLL: the keyed Collection::Set/Map INSERT side (the three
    // trees our tree reaches by address), the recursive subtree free, the
    // packfile introsort and the last two element copy-assigns. Called from
    // here because this is the Collection namespace's one init hook, the same
    // reasoning as the linked list and lane VID2's AVL primitives above.
    //
    // It registers SIX addresses to lane VID2's bodies (the sibling retraces
    // and rotations, byte-verified equal against the shipped DLL); those are
    // distinct addresses, not a duplicate registration of one.
    if (RtWaveEnabled("BANISHED_RT_AVLTREES")) {
        extern int RtCollectionAvlTreesRegister();
        extern int RtCollectionSortRegister();
        const int avl  = RtCollectionAvlTreesRegister();
        const int sort = RtCollectionSortRegister();
        // The CRT `_stricmp` / `__ascii_stricmp` the packfile descent compares
        // with. Registered from here rather than a new init because it has no
        // other owner and four live callers already reach it by address; see
        // runtime/src/crt/msvc_stricmp.cpp for why it is not std::strcasecmp.
        extern int  RtCrtStrICmpRegister();
        extern void RtCrtStrICmpCheckLocaleFlag();
        const int icmp = RtCrtStrICmpRegister();
        RtCrtStrICmpCheckLocaleFlag();
        const char* p = std::getenv("BANISHED_RT_AVLTREES_PROBE");
        RtCollectionAvlProbeArm(p && *p && std::strcmp(p, "0") != 0);
        std::printf("[RtPort]   Collection keyed Set/Map + sort (lane COLL): "
                    "%d + %d + %d addresses registered\n", avl, sort, icmp);
    }

    // ARM the probe rather than running it. It runs on the first call into any
    // entry point above, because at THIS moment the DLL's MemoryAllocator is
    // constructed but not started — see array_internal.h's "Probe scheduling".
    rtcoll::g_probePending = RtCollectionProbeArmed();
}
