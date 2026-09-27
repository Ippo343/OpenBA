// ============================================================================
// runtime/src/io/compressedstream.cpp — Wave 4 lane B: IO::CompressedMemoryStream
// ============================================================================
//
// The six IO stream-family exports our tree calls, all of them on
// IO::CompressedMemoryStream and all of them from ONE caller,
// code/Game/Game/gameupdateloop_serialize.cpp (GameUpdateLoop::SaveRestore,
// exe 0x1400959c0):
//
//   retail      profile     ordinal  export
//   0x1800a9520 0x18005c240  106     ??0CompressedMemoryStream@IO@@QEAA@XZ
//   0x1800a9560 0x18005c280  551     ??1CompressedMemoryStream@IO@@UEAA@XZ
//   0x1800a9600  --          1874    ?Create@CompressedMemoryStream@IO@@QEAA_NXZ
//   0x1800a9500  --          3302    ?Open@CompressedMemoryStream@IO@@QEAA_NXZ
//   0x1800a9620  --          1719    ?Close@CompressedMemoryStream@IO@@UEAAXXZ
//   0x180024350  --          2784    ?GetMemoryStream@CompressedMemoryStream@IO@@QEAAAEAVMemoryStream@2@XZ
//
// All six manglings are present in the shipped DLL's export directory
// (rt_io_port.cpp warning 2). Authoritative check:
// docs/runtime_port/wave4_export_names.txt — the wave-wide pefile sweep, 39 IO
// symbols, 39 ok / 0 MISSING; the addresses and ordinals above are its rows.
// The three vtable symbols and the seven call-through targets below are NOT in
// that sweep and were checked separately by this lane; all present, see
// WAVE4_LANE_B.md §2. Nothing here is a phantom name.
//
// ?GetMemoryStream sits far from the other five, in the 0x18002xxxx COMDAT pool
// of inline-defined and compiler-generated one-liners. It is NOT identical-
// COMDAT-folded with anything: grouping the whole export directory by RVA gives
// exactly one name at 0x180024350 (and at each of the other eight addresses
// this file registers), so the RegisterRuntimeAddr below binds one function,
// not a folded group.
//
// ORIGINAL SOURCE FILE. The recovered tree (docs/runtime_port/
// original_source_tree.txt) has no Stream.cpp and no
// CompressedMemoryStream.cpp: this class lives in IO\CompressedStream.cpp and
// IO::Stream / IO::MemoryStream in IO\MemoryStream.cpp. This file was
// scaffolded as "stream.cpp" and RENAMED to compressedstream.cpp by the
// orchestrator on lane B's recommendation, because the recovered tree has a
// SEPARATE IO\Stream.cpp for the IO::Stream base -- keeping the old name would
// have collided with it when that base is ported. The old name was the
// lane's file name, not the engine's.
//
// WHAT IS *NOT* PORTED, AND WHY THE CODEC IS SAFE
// -----------------------------------------------
// Every one of these six is a thin shim over IO::CompressedStream, and
// CompressedStream is where the actual compression lives (Commit,
// CompressWindow, DecompressFromSource, SetWindow, and through them
// System::Compression::Compress / Decompress). NONE of that is reimplemented
// here. Each shim calls the same exported CompressedStream / MemoryStream entry
// point retail calls, with the same arguments:
//
//   ??0CompressedStream@IO@@QEAA@XZ                     0x1800aa3d0
//   ??1CompressedStream@IO@@UEAA@XZ                     0x1800aa650
//   ?Commit@CompressedStream@IO@@QEAAXXZ                0x1800a9ee0
//   ?Create@CompressedStream@IO@@QEAA_NAEAVStream@2@H@Z 0x1800aa4b0
//   ?Open@CompressedStream@IO@@QEAA_NAEAVStream@2@H@Z   0x1800aa560
//   ??0MemoryStream@IO@@QEAA@XZ                         0x1800ae710
//   ??1MemoryStream@IO@@UEAA@XZ                         0x1800ae890
//
// so the compressed byte format is bit-identical by construction. Porting
// CompressedStream itself is a later wave's job.
//
// VTABLE OWNERSHIP — the argument for THIS class
// ----------------------------------------------
// ??1CompressedMemoryStream and ?Close are both virtual (UEAA), so PLAN §4.3
// risk 3 applies and WAVE3.md finding 4 says in capitals that Wave 3's
// exported-vtable argument is PER CLASS. Re-made here from scratch:
//
//  (a) ??_7CompressedMemoryStream@IO@@6B@ IS an exported data symbol
//      (0x180155560, ordinal 1409), as are ??_7MemoryStream@IO@@6B@
//      (0x1801555b0, 1465) and ??_7Stream@IO@@6B@ (0x1801549a0, 1506). Verified
//      with pefile against C:\GOG Games\Banished\Runtime-x64.dll. The ported
//      ctor and dtor therefore stamp the DLL'S OWN vtable, resolved by name.
//      We never build a private copy, so there is exactly ONE vtable and ONE
//      RTTI record for this class in the process. That is the whole of the
//      "two vtables in play" hazard, and it cannot arise.
//
//  (b) Consequence: slot 0 (the deleting-dtor thunk 0x1800244a0) and slot 7
//      (Close, 0x1800a9620) still point at the DLL's bodies. Every VIRTUAL
//      dispatch keeps landing in the DLL; the ports below are reached only
//      through our own direct, resolved-pointer calls. Both bodies are
//      byte-faithful reimplementations of each other over an identical layout,
//      so which copy runs is unobservable — but it does mean the diff oracle
//      only ever sees OUR callers, and the census will overstate coverage if
//      read as "the DLL now uses ours".
//
//  (c) No object crosses the boundary. Our only caller builds its
//      CompressedMemoryStream in a stack buffer (`alignas(16) uint8_t
//      cms[0x100]`) and calls ctor / Create / Open / Close / GetMemoryStream /
//      dtor through resolved function pointers — never `new`, never `delete`,
//      never a virtual call. The DLL's own uses are self-contained: the only
//      internal code xrefs to any of the six are to the dtor, from
//      ReadPackfile (0x1800b6054, IO::PackageFile), from the deleting-dtor
//      thunk, and from four SEH unwind funclets — every one of them on an
//      object the DLL both built and destroys itself.
//
//  (d) Therefore the class is NOT ported "half-way" in the sense PLAN §4.1
//      decision 5 warns about. The unported members (the copy ctor,
//      operator=, Open(void*,int), PreCommit, and the whole CompressedStream
//      base) continue to operate on the same single layout and the same single
//      vtable. Nothing observable depends on which copy of a body runs.
//
// This argument turns entirely on (a). If ??_7CompressedMemoryStream@IO@@6B@
// were not exported the honest move would be to hold the two virtuals back, and
// the lane doc says so.
//
// DIFF ORACLES — see docs/runtime_port/WAVE4_LANE_B.md §5 for the full
// can/cannot table. Two of the six get a real oracle (the ctor, by constructing
// the original into a scratch buffer and comparing all 0x98 bytes; and
// GetMemoryStream, which is pure). The other four allocate or mutate, so
// running the original alongside would construct twice or double-commit; they
// get shadow post-state assertions and are labelled PARTIAL, not proved.
//
// PORTABILITY: no <windows.h> (PLAN.md §4.1 decision 1).
// ============================================================================

#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <new>

#include "../../funcs/rt_port.h"
#include "../../types/io/Stream.h"

using RtIo::CompressedMemoryStream;
using RtIo::CompressedStream;
using RtIo::MemoryStream;
using RtIo::Stream;

namespace {

// ---- The entry points these shims forward to -------------------------------
// Every one is looked up ONCE per call site. the raw export door/ResolveExport is
// GetProcAddress / a registry probe (rt_port.h): a bare the raw DLL door in a body
// cost Wave 2 five points of sim time. These are cold paths — a save happens
// once every few minutes — but the rule is the rule.
//
// WAVE 9 LANE V2, 2026-09-07: the ORACLE sites (the scratch-object ctor/dtor
// at 235/263, both inside `if (the diff oracle)`) stay on the raw DLL door — that
// is the whole point of them. The sites the LIVE bodies execute — the base and
// member constructors, the two base destructors, and Commit — are REAL
// DEPENDENCIES and moved to RT_EXPORT_ONCE. None of them names the symbol its
// own enclosing port registers (CompressedMemoryStream's ctor/dtor call
// CompressedStream's and MemoryStream's), so there is no self-recursion.
// The Create / Open pair below were NOT among rawdoor_audit's findings and are
// converted anyway: the resolve and its argument list are on separate SOURCE
// LINES, and the checker's immediate-call regex is per-line. They are the same
// live-body real dependency as the rest, found by reading the file rather than
// by the checker — a clean run is a floor, not a ceiling.
// Raw here would be a NULL call the instant Runtime-x64.dll is absent; through
// the registry it is instead a named unresolved import if the neighbour is
// still unported at cutover, which is a work-list row rather than a crash.

using VoidThisFn  = void (*)(void*);
using StreamVFn   = void (*)(void*);          // vtable slot 7, Close()

// ENDGAME B.3.9 IO (W2): the by-name vtable reads (CompressedMemoryStream,
// CompressedStream, Stream) went with the ??0 doors that stamped them.

// The embedded MemoryStream's Close(), dispatched through its own vtable at
// +0x38 exactly as retail does (`MOV RAX,[RDI]; CALL [RAX+0x38]`). Deliberately
// NOT a direct call to ?Close@MemoryStream: retail dispatches virtually and a
// subclass could in principle sit there.
void CallStreamCloseVirtual(MemoryStream* ms) {
    const void* const* vtbl = *reinterpret_cast<const void* const* const*>(ms);
    reinterpret_cast<StreamVFn>(
        const_cast<void*>(vtbl[RtIo::kStreamVtblCloseSlot]))(ms);
}

}  // namespace

// ============================================================================
// IO::CompressedMemoryStream -- ENDGAME B.3.9 IO (W2)
// ============================================================================
// The six doors that stood here (??0 / ??1 / ?Create / ?Open / ?Close /
// ?GetMemoryStream) are the class's own members now: the constructor and
// GetMemoryStream are inline in Stream.h, Close is below, and every builder
// (Save::SaveLoad::SaveMetaData / LoadMetaData, PackageFile::ReadPackfile,
// the exe's GameUpdateLoop::SaveRestoreOut and its roundtrip check) spells
// `::new (p) IO::CompressedMemoryStream()` and ends the object with a
// non-virtual `DeletingDtor(0)` at the position the door call had.
namespace RtIo {
extern "C" bool RtIo_CompressedStream_Open(CompressedStream*, Stream*, int32_t);    // 0x1800aa560
extern "C" bool RtIo_CompressedStream_Create(CompressedStream*, Stream*, int32_t);  // 0x1800aa4b0
extern "C" void RtIo_CompressedStream_Commit(CompressedStream*);                   // 0x1800a9ee0
}  // namespace RtIo

namespace IO {

// ?Open@CompressedMemoryStream@IO@@QEAA_NXZ      retail 0x1800a9500
// Whole body: `return CompressedStream::Open(_memoryStream, 1);` -- 0x1800aa560
// sets _direction = 0 (reading), stores _source, SetWindowCount(1), then parses
// the window header table out of the source; false when the totals disagree.
bool CompressedMemoryStream::Open() {
    return RtIo::RtIo_CompressedStream_Open(
        reinterpret_cast<RtIo::CompressedStream*>(this),
        reinterpret_cast<RtIo::Stream*>(&_memoryStream), 1);
}

// ?Create@CompressedMemoryStream@IO@@QEAA_NXZ    retail 0x1800a9600
// Whole body: `return CompressedStream::Create(_memoryStream, 2);` -- 0x1800aa4b0
// sets _direction = 1 (writing), stores _source, news a System::Compression and
// SetWindowCount(2). Writing uses two windows, reading one: both literals are
// read off the two call sites.
bool CompressedMemoryStream::Create() {
    return RtIo::RtIo_CompressedStream_Create(
        reinterpret_cast<RtIo::CompressedStream*>(this),
        reinterpret_cast<RtIo::Stream*>(&_memoryStream), 2);
}

// ??1CompressedMemoryStream@IO@@UEAA@XZ          retail 0x1800a9560
// The profile build (0x18005c280) gives the source shape:
//     CompressedStream::Close();          // inlined in retail: guard + Commit
//     _memoryStream.Close();              // virtual, vtable +0x38
//     _memoryStream.~MemoryStream();      // the compiler's member dtor
//     ~CompressedStream();                // the compiler's base dtor
// The member's Close is dispatched through ITS OWN vptr (a different object at
// +0x68), exactly as retail's `CALL [RAX+0x38]`. No guard added.
CompressedMemoryStream::~CompressedMemoryStream() {
    if (_headerSize > 0 && _direction == 1)
        RtIo::RtIo_CompressedStream_Commit(reinterpret_cast<RtIo::CompressedStream*>(this));
    ::CallStreamCloseVirtual(reinterpret_cast<RtIo::MemoryStream*>(&_memoryStream));
}

}  // namespace IO

// ============================================================================
// Registration — called from RtIoPortInit() under the old DLL-era switch.
// ============================================================================
// ============================================================================
// Wave 9 lane W — IO::MemoryStream::Open(int) and IO::MemoryStream::~MemoryStream
//
//   0x1800ae670  ?Open@MemoryStream@IO@@QEAA_NH@Z   ord 3305
//   0x1800ae890  ??1MemoryStream@IO@@UEAA@XZ        ord 631
//
// Both are Wave 9 lane V rows: REAL DEPENDENCY (MIGRATED). Lane V2 moved their
// call sites (saveload.cpp:832, compressedstream.cpp:330) from the raw door to
// RT_EXPORT_ONCE, which is correct and NOT sufficient — with no port and no
// mint entry `ResolveExport` misses the registry and falls through to
// `the raw export door`, so the request only changed which residue list prints
// it. These two close the row for real.
//
// THE THIRD MEMBER OF THE ROW, ??0MemoryStream, IS **NOT** PORTED HERE AND THE
// REASON IS A FACT ABOUT THE BINARY, NOT AN ESTIMATE. Every concrete Stream
// constructor in this family allocates an 8-byte IO::Encoding and stores ONE
// pointer into it: the shared encoding vtable at **0x1801554a8** (see
// RtIo_CompressedMemoryStream_Ctor's oracle, which already special-cases that
// address). That address is **not an exported symbol** — checked against the
// DLL's export directory BY ADDRESS, not by name pattern; 0x1801555b0
// (??_7MemoryStream@IO@@6B@) is exported and 0x1801554a8 has no export at all,
// and it is not merely the interior of a bigger table either: 0x180155488..
// 0x1801554a0 are four _purecall slots belonging to the abstract base, so
// 0x1801554a8 is where the CONCRETE encoding's table begins. A ported ctor must
// obtain that pointer, and at cutover there is no name for `ResolveExport` to
// take and no row the mint can carry, because the mint is keyed by exported
// symbol. So ??0MemoryStream and ??0CompressedStream are blocked on EXTENDING
// THE MINT TO UNEXPORTED VTABLES, or on identifying the concrete Encoding class
// and rebuilding its table slot by slot — lane C2's shape of work, not a port.
// Named rather than defaulted; see WAVE9_LANE_W.md.
//
// Neither function below touches the encoding's CONTENTS, only its address, so
// neither inherits that blocker.
// ============================================================================

// The four fields ~MemoryStream and Open need, plus this class's own vtable.
// ??_7MemoryStream@IO@@6B@ (0x1801555b0) is exported, so it goes through the
// PORT-AWARE door for the reason mesh.cpp's RtVtable() records: the raw door is
// GetProcAddress, which returns null at cutover for a symbol the mint carries
// correctly just as much as for one it has never heard of.
// ----------------------------------------------------------------------------
// ResolveAddr / Module, forward-declared rather than pulled in from
// standalone/resolve.h -- that header drags in <windows.h>, whose <wingdi.h>
// declares a function called `Rectangle` and whose <windef.h> #defines `near`
// and `far` away, both of which collide with engine type and member names
// (PLAN 4.1 decision 1). Must stay byte-identical to resolve.h's; the linker
// checks the two against each other.
//
// Needed by Wave 9 lane W round 2: the shared IO::Encoding vtable at
// 0x1801554a8 is UNEXPORTED, so the only port-aware door to it is the ADDRESS
// one -- which is exactly the door lane C2's address-keyed mint registers.
// ----------------------------------------------------------------------------

// ?Open@MemoryStream@IO@@QEAA_NH@Z — 0x1800ae670.
//
// Retail, in order: MemAlloc(size,4) -> +0x18; size -> +0x28; 0 -> +0x24
// (_cursor); 0 -> +0x08 (_direction, i.e. READING); 1 -> +0x20 (_ownsData);
// return true UNCONDITIONALLY — including when the allocation failed. That last
// part is retail's and it is reproduced: a caller that trusts the `true` and
// writes into a null buffer is retail's behaviour, and a guard here would be
// the one CLAUDE.md warns against rather than a repair.
//
// The lock/Allocate/unlock trio the decompile shows inlined at this call site is
// instruction-for-instruction `?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z`
// (0x1800c7a50: EnterCriticalSection(_singleton+0x640); Allocate(_singleton, n,
// align); LeaveCriticalSection) — compared, not assumed — so calling MemAlloc
// through the port-aware door is both the faithful spelling and the one that
// follows lane E's allocator when it arms.
//
// ORACLE: FULL, by replay into a scratch object. Open ALLOCATES, so running
// theirs over the live object would leak the buffer ours just made and leave
// the object pointing at a second one. Instead a scratch MemoryStream is built
// with the DLL's own constructor, THEIR Open is run on it with the same size,
// and every field the function writes except the buffer pointer is compared —
// _size, _cursor, _direction, _ownsData, and the return value: all four
// non-pointer stores. Two independent allocations cannot share an address, so
// comparing _data would fail 100% of calls for a reason that is not a defect
// (reference_oracle_must_not_compare_unwritten_bytes.md, from the other side).
// The scratch is destroyed with the DLL's own destructor, so nothing leaks and
// nothing live is touched. Every call reaching the guard does the comparison,
// so `calls=` counts work rather than entry.
// ENDGAME B.3.9 IO (W2): the member; the ?Open door that stood here is gone
// with its by-name callers (SavedState::Retrieve, LoadMetaData). The allocation
// is the same ?MemAlloc@MemoryAllocator (RtSystem::RtMemAlloc), the size
// sign-extended exactly as `MOVSXD` does.
bool IO::MemoryStream::Open(int size) {
    _data = RtSystem::RtMemAlloc(static_cast<uint64_t>(size), 4);
    _size = static_cast<uint32_t>(size);
    _cursor = 0;
    _direction = 0;
    _ownsData = 1;
    return true;
}

// ?Create@MemoryStream@IO@@QEAA_NH@Z — 0x1800ae7f0.   WAVE 23 lane IOPORT.
//
// THE WRITE-SIDE TWIN OF Open ABOVE, and the ONLY difference is the direction.
// This file's own probe comment (compressed_stream_probe.cpp:143) already said
// so — "the same body except `MOV dword ptr [RBP+0x8],1` at 0x1800ae860" — and
// that claim was RE-DERIVED from the shipped bytes this lane rather than
// inherited. The full retail disassembly (Ghidra MCP, retail Runtime-x64.dll,
// not the profile build) is 26 instructions and the five stores are:
//
//     1800ae852  MOV [RBP+0x18],RBX     _data    = MemAlloc(size, 4)
//     1800ae856  MOV [RBP+0x28],ESI     _size    = size
//     1800ae859  MOV [RBP+0x24],0       _cursor  = 0
//     1800ae860  MOV [RBP+0x8],1        _direction = 1   <-- WRITING (Open: 0)
//     1800ae867  MOV [RBP+0x20],1       _ownsData  = 1
//     1800ae86b  MOV AL,1               return true, UNCONDITIONALLY
//
// STORE ORDER IS RETAIL'S, not tidied: _size before _cursor before _direction
// before _ownsData. Nothing here reads what it wrote, so the order is not
// load-bearing, but reordering would make a future byte-level diff lie.
//
// `MOVSXD RSI,EDX` at 1800ae80e sign-extends the int before it reaches
// Allocate, so a negative size reaches the allocator as a huge unsigned value
// rather than being clamped. `static_cast<uint64_t>(size)` reproduces exactly
// that bit pattern (a negative int converts to uint64_t as 2^64+v), which is
// why it is spelled this way and not with an intermediate size_t.
//
// THE UNCONDITIONAL `true` IS RETAIL'S AND IS REPRODUCED. Allocation failure is
// not reported; a caller that trusts the return and writes into a null buffer
// is behaving exactly as it does against the shipped DLL. Per CLAUDE.md this is
// the deref NOT to guard — a guard here would skip retail's behaviour, not
// restore it.
//
// WHY THIS ROW MATTERS BEYOND ITS SIZE. `?Open@MemoryStream@IO@@QEAA_NH@Z` is
// the WRONG overload for a writer — it sets _direction = 0 — and the failure
// mode when the two are confused is a `REP MOVSB` with RDI = 0 inside the DLL's
// memcpy, two calls away from the mistake. That is documented in
// compressed_stream_probe.cpp as having cost two runs to find. With both
// overloads now ported under their own mangled names, the distinction is
// visible in our source instead of only in the DLL.
//
// ORACLE: FULL, by replay into a scratch object — the SAME instrument the Open
// port above uses, for the same reason. Create ALLOCATES, so running theirs
// over the live object would leak the buffer ours just made and leave the
// object pointing at a second one. Instead a scratch MemoryStream is built with
// the DLL's own constructor, THEIR Create is run on it with the same size, and
// every field the function writes EXCEPT the buffer pointer is compared —
// _size, _cursor, _direction, _ownsData and the return value, i.e. all four
// non-pointer stores. _data is deliberately NOT compared: two independent
// allocations cannot share an address, so comparing it would fail 100% of calls
// for a reason that is not a defect
// (reference_oracle_must_not_compare_unwritten_bytes.md).
//
// THE CONTROL THIS ORACLE ACTUALLY RUNS. _direction is the one field that
// distinguishes Create from Open, and it IS in the compared set — so if this
// body were mistakenly written as a copy of Open (direction 0), the shadow
// would fire on every call. That is not a hypothetical: it is the exact defect
// the probe comment warns about, and this oracle is positioned to catch it.
// What the oracle does NOT cover: the allocator call itself (size and alignment
// reaching MemAlloc) is compared only indirectly, through _size.
// ENDGAME B.3.9 IO (W2): the member; the ?Create door that stood here is gone
// with its by-name caller (CompressedMemoryStream::PreCommit).
bool IO::MemoryStream::Create(int size) {
    _data = RtSystem::RtMemAlloc(static_cast<uint64_t>(size), 4);   // 1800ae852
    _size = static_cast<uint32_t>(size);                               // 1800ae856
    _cursor = 0;                                                       // 1800ae859
    _direction = 1;          // 1800ae860 — WRITING. Open stores 0 here.
    _ownsData = 1;                                                     // 1800ae867
    return true;
}

// ??1MemoryStream@IO@@UEAA@XZ — 0x1800ae890.
//
// Retail: vptr = MemoryStream's; if (_ownsData && _data) MemFree(_data);
// vptr = Stream's; if (_encoding) MemFree(_encoding). _data and _encoding are
// left holding freed pointers.
//
// ENDGAME B.3.9 IO: that is `IO::MemoryStream::~MemoryStream` followed by
// `~Stream` (Stream.h). The door STAYS for its by-name callers
// (CompressedMemoryStream's dtor above, Object::Internal::Clone, the dialog
// settings load) and is one non-virtual slot-0 call with flags 0: the
// destructor chain, no free (B9-6).
// ENDGAME B.3.9 IO (W2): the ??1MemoryStream door that stood here is gone --
// Object::Internal::Clone and the dialog-settings load end their stack
// streams with the class's non-virtual DeletingDtor(0).

// ============================================================================
// Wave 9 lane W, round 2 — ??0MemoryStream and ??0CompressedStream
//
//   0x1800ae710  ??0MemoryStream@IO@@QEAA@XZ        ord 273
//   0x1800aa3d0  ??0CompressedStream@IO@@QEAA@XZ    ord 108
//
// THE BLOCKER THAT HELD THESE BACK IS GONE, AND HALF OF WHY I RECORDED IT WAS
// WRONG. §4 of WAVE9_LANE_W.md said these two "cannot be closed by porting: the
// shared encoding vtable at 0x1801554a8 has no exported name and no row the mint
// can carry." The first clause is true — checked against the export directory by
// address. **The second was not.** Lane C2 (`c77b26f`) mints it KEYED BY ADDRESS
// (`symbol == nullptr`, registered through `RegisterRuntimeAddr`, reached with
// `ResolveAddr(Module::Runtime, ...)`), with the count carrying the STRONGER
// adjacency proof: the table ends exactly where `??_7BinaryFileStream@IO@@6B@`
// begins. "Unexported" was never "unmintable", and the mint being keyed by name
// was a property of the table, not of the mechanism.
//
// WHAT THAT DOES AND DOES NOT DO — the distinction has to survive into this
// file, because a partial close that reads as a full one is worse than an open
// row. Lane C2's own words: **minting is ENABLING, NOT SUFFICIENT.** All five
// slots of that vtable (0x1800a81e0, 0x1800a81d0, 0x1800a81c0, 0x1800a8120,
// 0x1800a8080) are unexported `.text` internals, none is ported, so all five
// mint as NAMED TRAPS. So:
//
//   * Porting these two ctors removes the CALL THROUGH NULL at construction —
//     the failure this lane measured live at `calls=1` on both the save and the
//     load cell (WAVE9_LANE_W.md §9 finding 3).
//   * It does NOT make an `IO::Encoding` functional. The object these ctors
//     build carries a vtable whose every slot is a trap, so the first VIRTUAL
//     CALL through `stream->_encoding` after cutover still stops — at a trap
//     that names the vtable and the slot, instead of an EXEC-of-0 several frames
//     from the cause. Better failure, not no failure. See §11 for which of the
//     five stampers that leaves and what still has to happen.
//
//   UPDATE, Wave 21 lane ENC: the "all five mint as NAMED TRAPS" sentence above
//   is now STALE. Slots 0/1/2 (0x1800a81e0, 0x1800a81d0, 0x1800a81c0) are
//   ported in runtime/src/io/encoding.cpp as RtIo_Encoding_{IsBinary,IsText,
//   GetKind} alongside the two factories already there. All FIVE slots of
//   0x1801554a8 are our bodies now, so an `IO::Encoding` built on this table
//   IS functional after cutover, not merely constructed.
//
// TRANSCRIBED FROM THE LISTINGS (0x1800ae710..0x1800ae7c7 and
// 0x1800aa3d0..0x1800aa4a5), which are the same function with a different field
// set. Details the decompiled C would have lost:
//
//   * `MOV byte ptr [RCX+0x20],BPL` — `_ownsData` is a BYTE store, not the
//     `undefined8` Ghidra prints around it.
//   * `MOV qword ptr [RCX+0x24],RBP` — ONE 8-byte store spanning `_cursor`
//     (+0x24) and `_size` (+0x28), and deliberately UNALIGNED. Writing the two
//     u32 fields separately is byte-equivalent and is what is done here.
//   * CompressedStream writes `+0x1c` and `+0x20` as DWORDs (`+0x20` = -1) and
//     leaves `+0x18` and `+0x24` untouched — the two gaps this file's existing
//     CompressedMemoryStream oracle already excludes.
//   * `MOV RAX,RDI` before the return: MSVC constructors return `this`. The
//     `CtorFn` typedef this file already uses is `void*(*)(void*)`, which is why
//     the existing call sites work and why these return `self`.
//   * The allocation is `CALL 0x1800c6e60` with RCX=`_singleton`, EDX=8, R8D=4
//     between an `EnterCriticalSection(_singleton+0x640)` / `LeaveCriticalSection`
//     pair. 0x1800c6e60 is `?Allocate@MemoryAllocator@System@@QEAAPEAX_KH@Z`, so
//     that trio is `?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z` (0x1800c7a50)
//     inlined — compared instruction by instruction, not assumed, exactly as for
//     `MemoryStream::Open` above.
//   * The `if (_encoding) MemFree(_encoding)` before the final store is retail's
//     and is reproduced even though `_encoding` was set to 0 eleven instructions
//     earlier and the branch is therefore dead in every reachable call. It is one
//     `TEST`/`JZ` in the shipped bytes; removing it would be a divergence for no
//     gain, and `FUN_180006a10` — the un-exported second copy of MemFree retail
//     calls there — is the same code as the exported name used here.
//   * An allocation FAILURE leaves `_encoding` NULL and the constructor returns
//     normally. Faithful, and not guarded: retail's callers rely on it never
//     failing, and a guard would hide the upstream problem rather than fix it.
//
// ORACLE for both: FULL, by constructing THEIRS into a private scratch buffer
// and comparing every byte the constructor writes. Nothing live is touched and
// the scratch is destroyed with the DLL's own destructor, so nothing leaks —
// the identical shape `RtIo_CompressedMemoryStream_Ctor` above already uses,
// and its gap mask is the cross-check: that function's verified
// {0x0c, 0x18, 0x24, 0x74, 0x89, 0x94} decomposes EXACTLY into
// CompressedStream's {0x0c, 0x18, 0x24} plus MemoryStream's
// {0x0c, 0x21, 0x2c} shifted by the embedded sub-object's 0x68. Two independent
// derivations of the same masks agreeing is worth more than either alone.
//
// The `_encoding` pointer at +0x10 is a fresh heap allocation on both sides and
// CANNOT match, so in both comparison copies it is replaced by the first qword
// of the object it points at — the encoding vtable. That still catches the
// failure that matters (an encoding that is null, or that is not the shared
// 0x1801554a8 table) while ignoring the address.
// ============================================================================

// ??0MemoryStream@IO@@QEAA@XZ (0x1800ae710). ENDGAME B.3.9 IO: the body is
// `IO::MemoryStream`'s real constructor (Stream.h). The door STAYS: it is
// reached by name on raw storage from CompressedMemoryStream's ctor (above),
// Save::SaveLoad::SaveMetaData, Object::Internal::Clone, the dialog-settings
// load and PackageFile::ReadPackfile -- one placement new (B9-6).
// ENDGAME B.3.9 IO (W2): the ??0MemoryStream door that stood here is gone --
// every builder spells `::new (p) IO::MemoryStream()`.

// ENDGAME B.3.9 IO (W2): the ??0CompressedStream door (0x1800aa3d0) that stood
// here is gone with its last caller; the body is IO::CompressedStream's real
// protected ctor (Stream.h), run by every CompressedMemoryStream /
// CompressedFileStream construction.

// ============================================================================
// `IO::CompressedMemoryStream::Close` — B.3.4 block 4 (STREAMS)
// ============================================================================
namespace IO {

// ?Close@CompressedMemoryStream@IO@@UEAAXXZ                  retail 0x1800a9620
//     if (headerSize > 0 && direction == 1) Commit(this);    // CompressedStream::Close
//     _memoryStream.Close();                                 // virtual, +0x38
// Retail tail-JMPs the virtual call, which is why Ghidra reports "could not
// recover jumptable" at 0x1800a9647; it is an ordinary tail call, not a switch.
//
// This is the function that turns the written windows into the compressed byte
// image sitting in _memoryStream, so SaveRestore must call it before Store.
//
// THE MEMBER'S `Close` IS DISPATCHED VIRTUALLY, DELIBERATELY, and that is not
// the recursion hazard the qualified-call rule is about. `_memoryStream` is a
// DIFFERENT OBJECT at +0x68 carrying its OWN vptr — retail does
// `MOV RAX,[RDI]; CALL [RAX+0x38]` on it — so the dispatch lands on
// MemoryStream's table, not on this class's. Qualifying it would change what
// retail calls, which is the opposite of what the rule asks for.
void CompressedMemoryStream::Close() {

    const bool committed = _headerSize > 0 && _direction == 1;
    if (committed)
        RT_EXPORT_ONCE(::VoidThisFn, "?Commit@CompressedStream@IO@@QEAAXXZ")(
            static_cast<CompressedStream*>(this));

    ::CallStreamCloseVirtual(
        reinterpret_cast<RtIo::MemoryStream*>(&_memoryStream));

    // ---- PARTIAL (presence only) -------------------------------------------
    // Be honest about this one: it proves nothing about equivalence. Calling the
    // original afterwards would Commit a second time and corrupt the save, and
    // the Commit decision reads the same two fields in both implementations, so
    // asserting it here is circular. The site exists so the summary's `calls=`
    // column shows the save path was actually taken and reports which branch it
    // took; the equivalence argument for this body rests on the decompile.
}

}  // namespace IO

// ENDGAME B.3.9 IO (W2): RtIoCompressedStreamRegister is gone -- every door it
// registered (the six CompressedMemoryStream doors, ??0CompressedStream and the
// four MemoryStream doors) is a class member now, reached directly.
