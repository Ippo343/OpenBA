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

void InitZero8(uint8_t* d) { *reinterpret_cast<uint64_t*>(d) = 0; }
// 0x18007b960's `else` arm is `MOV dword ptr [RDX],0xffffffff`.
void InitNeg1 (uint8_t* d) { *reinterpret_cast<int32_t*>(d) = -1; }
// 0x180059140's `else` arm, the identity Math::Transform:
//   [d+0x00] = 0 (qword)  [d+0x08] = 0 (dword)
//   [d+0x0c] = 0x3f800000 (qword: 1.0f at +0x0c, 0 at +0x10)
//   [d+0x14] = 0 (qword)
// All 0x1c bytes are written, so there is no unwritten hole in this one.
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
// port. The two COPY-ASSIGN rows (0x180004220 and 0x180051650) are unchanged
// and are still the DLL's; 0x180051650 calls Audio's 0x180050d30 and is gated
// with the Wave 8 Audio holdbacks.
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
        if (spec->destroy) spec->destroy(nullptr, old, oldSizeI);
        MemFree(old);                             // null-guarded inside 0x1800c7ad0
    } else if (spec->destroy && oldSizeI > newSize) {
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

#undef J1_ENTRY

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

    // ---- J3: SetSize, trivially-copyable elements --------------------------
    RegisterRuntimeAddr(0x180008b90, (void*)&RtColl_ArraySetSize_180008b90);
    RegisterRuntimeAddr(0x180008d60, (void*)&RtColl_ArraySetSize_180008d60);
    RegisterRuntimeAddr(0x180008ff0, (void*)&RtColl_ArraySetSize_180008ff0);
    RegisterRuntimeAddr(0x18002fea0, (void*)&RtColl_ArraySetSize_18002fea0);
    RegisterRuntimeAddr(0x180059140, (void*)&RtColl_ArraySetSize_180059140);
    RegisterRuntimeAddr(0x1800764c0, (void*)&RtColl_ArraySetSize_1800764c0);
    RegisterRuntimeAddr(0x18007b960, (void*)&RtColl_ArraySetSize_18007b960);
    RegisterRuntimeAddr(0x18001f990, (void*)&RtColl_ArraySetSizeFill_18001f990);
    RegisterRuntimeAddr(0x180023830, (void*)&RtColl_ArraySetSizeFill_180023830);
    RegisterRuntimeAddr(0x18004f790, (void*)&RtColl_ArraySetSizeFill_18004f790);
    RegisterRuntimeAddr(0x18004f8e0, (void*)&RtColl_ArraySetSizeFill_18004f8e0);
    RegisterRuntimeAddr(0x180076900, (void*)&RtColl_ArraySetSizeFill_180076900);
    RegisterRuntimeAddr(0x18007a0d0, (void*)&RtColl_ArraySetSizeFill_18007a0d0);

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

    // ARM the probe rather than running it. It runs on the first call into any
    // entry point above, because at THIS moment the DLL's MemoryAllocator is
    // constructed but not started — see array_internal.h's "Probe scheduling".
    rtcoll::g_probePending = RtCollectionProbeArmed();
}
