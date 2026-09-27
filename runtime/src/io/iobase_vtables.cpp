// ============================================================================
// runtime/src/io/iobase_vtables.cpp — Wave 19 lane IOBASE, tranche 1
// ============================================================================
//
// The bodies that stand between four minted `??_7…@IO@@6B@` rows and READY,
// plus the two `IO::VisitorStream` exports that ride with them. Every one was
// read as DISASSEMBLY out of the shipped `Runtime-x64.dll` (`objdump -d`), not
// from Ghidra's C (reference_ghidra_c_output_vs_disassembly.md).
//
//   0x18000c6a0  ?GetSource@Stream@IO@@UEBA?AVString@System@@XZ   slot 1 of SIX
//   0x1800242a0  MemoryStream vector deleting dtor      slot 0, UNEXPORTED
//   0x1800a82f0  ??1BinaryReader@IO@@UEAA@XZ
//   0x180023a10  BinaryReader vector deleting dtor      slot 0, UNEXPORTED
//   0x1800bac90  ??1Visitor@IO@@UEAA@XZ
//   0x18002b710  Visitor vector deleting dtor           slot 0, UNEXPORTED
//   0x1800baff0  ?GetSize@VisitorStream@IO@@UEBAIXZ
//   0x1800bb000  ?GetCurrentPos@VisitorStream@IO@@UEBAIXZ
//   0x1800bb010  ?SetCurrentPos@VisitorStream@IO@@UEAA_NH@Z
//   0x1800bb020  ?Read@VisitorStream@IO@@UEAA_NHPEAX@Z
//   0x1800bb030  ?Write@VisitorStream@IO@@UEAA_NHPEBX@Z
//   0x1800bb040  ??1VisitorStream@IO@@UEAA@XZ
//   0x1800bb090  ??0VisitorStream@IO@@QEAA@XZ
//   0x18002bf80  VisitorStream vector deleting dtor     slot 0, UNEXPORTED
//
// ----------------------------------------------------------------------------
// 1. `?GetSource@Stream@IO@@` — ONE BODY, SIX MINTED ROWS, AND ZERO IDENTITY
// ----------------------------------------------------------------------------
// Phase 0c names it the trap in FOUR rows (Stream, MemoryStream,
// CompressedStream, CompressedMemoryStream). That count PREDATES phase 0b's
// mint fix. On this tip it is `RT_MINT_NAMED` at slot 1 of **six** rows in
// standalone/rt_data_mint_tables.inc — the four above plus
// `??_7CompressedFileStream@` (:1223) and `??_7VisitorStream@` (:1569).
//
// **It moves the census identity by ZERO on its own** — and the reason is not
// the one this comment first gave. FIRST DRAFT, WRONG, kept because the
// correction is the useful part: I wrote that registering it "adds +1 to
// `called` AND +1 to `ported` in the same step". MEASURED after the fact, it
// adds **nothing to either column**. `runtime_census.py` builds its namespace
// table from symbols that have a CALL DOOR in our tree; a symbol that is
// registered but that nothing resolves is excluded from the table entirely and
// listed under "Ports with no caller in our tree" instead. Before this lane the
// symbol appeared NOWHERE in `--namespace IO` (its only tree occurrences were
// mint rows, which are not a call door); after it, it is still nowhere in the
// table and is one of that list's 234 exports, UNTRIAGED.
//
// The conclusion survives the correction and the arithmetic is unchanged, but
// "+1/+1" and "0/0" are different claims about the instrument and only one of
// them is true. Its whole value is that it is a PREREQUISITE: a `??_7…` row
// cannot be promoted while one of its slots is a trap, and it is the row's
// PROMOTION that adds +1 to `ported` against a `called` the row already had.
//
// The same correction applies to `??1BinaryReader@`, `??1Visitor@`,
// `??1VisitorStream@` and the five VisitorStream stream methods below: all are
// registered here and none has a resolve door in the tree, so all eight are
// caller-less ports that move no count. That also means the census's own
// warning applies to them — a port with no caller is NOT verified by a clean
// diff run (reference_uncalled_port_is_not_verified.md). What verifies them is
// that the mint slots they fill are dispatched; see WAVE19_LANE_IOBASE.md.
//
// The shipped body, in full (0x18000c6a0):
//
//     mov  [rsp+0x10],rdx        ; the HIDDEN RETURN SLOT
//     sub  rsp,0x18
//     movq [rsp+8],-2            ; SEH frame marker, no semantics
//     xor  eax,eax
//     mov  [rsp],eax             ; dtor-state local, dead
//     mov  [rdx],rax             ; *ret = nullptr    <-- the whole function
//     movl [rsp],1               ; dtor-state local, dead
//     mov  rax,rdx
//     ret
//
// i.e. `String GetSource() const { return String(); }` — a default-constructed
// `System::String`, whose entire representation is one null qword. RDX is the
// hidden return slot for the by-value `String`, so the typedef below spells it
// (reference_hidden_retslot_wild_write.md: an fn_* typedef that omits the
// hidden slot is a wild write that still returns the right answer).
//
// It reads NOTHING from `this`. That is not an inference from a decompile —
// RCX is never touched in the nine instructions above.
//
// ----------------------------------------------------------------------------
// 2. THE FOUR VECTOR DELETING DESTRUCTORS ARE ONE SHAPE, AND I READ EACH LOOP
// ----------------------------------------------------------------------------
// binary_writer.cpp:187-230 already carries the exact twin of these
// (`RtIo_BinaryWriter_DeletingDtor`, 0x180023b50) and its header block
// describes the shape. That is a written recipe from a good lane, which is
// precisely the thing reference_a_written_recipe_can_be_wrong.md is about — a
// prescribed pair of dtor ports would have swapped a body that destroys EVERY
// element for one that destroys only the first. So each of the four here was
// disassembled on its own, and so was the helper:
//
//   0x18007ec98 is `__ehvec_dtor(base, size, count, dtor)`. Its loop, read from
//   its own bytes, walks from the LAST element to the FIRST:
//       rbx = base + count*size ;  edi = count-1
//       while (edi >= 0) { rbx -= size; (*dtor)(rbx); --edi; }
//   It destroys every element. The `flags & 2` arms below therefore must too,
//   and each one is written as a loop, not as a single destructor call.
//
// The common shape (all four, byte for byte):
//
//     if (flags & 2) {                               // delete[] form
//         count = *(int32_t*)((char*)this - 8);      // the array cookie
//         __ehvec_dtor(this, sizeof(T), count, ~T);
//         if (flags & 1) MemFree((char*)this - 8);
//         return (char*)this - 8;
//     }
//     ~T(this);                                      // INLINED in retail
//     if (flags & 1) MemFree(this);
//     return this;
//
// with `sizeof(T)` = 0x30 (MemoryStream), 0x30 (BinaryReader), 0x28 (Visitor),
// 0x18 (VisitorStream), matching each type header's `static_assert`.
//
// **THE `flags & 2` ARM IS A NAMED ABORT, NOT A PORT — and that is a declared
// divergence.** ENC-2 took the same position for BinaryWriter and gave the
// reason for BinaryWriter only. Re-derived here per class rather than
// inherited:
//
//   * MemoryStream — every producer in the DLL and in this tree constructs one
//     by value on a stack frame or as a sub-object of CompressedMemoryStream
//     (+0x68). No array allocation exists.
//   * BinaryReader / Visitor — the only producers are IO::Encoding::CreateReader
//     (0x1800a8120) and the Visitor factory, both a bare `MemAlloc(0x30, 4)` /
//     `MemAlloc(0x28, 4)` that lays down NO array cookie.
//   * VisitorStream — its one caller, `?Add@ExternalFactory@Object@@`, stack-
//     constructs a single instance (runtime/src/object/external_add.cpp:348).
//
// Reaching the arm means a wrong-typed pointer arrived or a producer exists
// that this lane did not find, and in EITHER case the eight bytes at `this-8`
// are heap metadata rather than a count — so continuing would loop an
// arbitrary number of times over arbitrary memory. Aborting with a message
// that says what would have to have happened is the honest disposition; a
// silent single-element destroy would be exactly the swapped body the memory
// above warns about. This is called out in WAVE19_LANE_IOBASE.md as the one
// place in this file that is not byte-faithful.
//
// `0x180006a10`, the unexported free these bodies call, is byte-identical to
// the exported `?MemFree@MemoryAllocator@System@@SAXPEAX@Z` (0x1800c7ad0) —
// asserted by compressedstream.cpp, re-derived by binary_writer.cpp, and
// re-read here: both are `TEST RCX,RCX; JZ ret; …; Enter; Free(_singleton,p);
// Leave`, over the SAME allocator sub-object (`[0x1801fc718] + 0x640`). Using
// the exported name is exact, not merely equivalent.
//
// ----------------------------------------------------------------------------
// 3. ~BinaryReader CONSUMES FOUR BYTES, AND THAT IS THE MIRROR OF ~BinaryWriter
// ----------------------------------------------------------------------------
// 0x1800a82f0 in full:
//
//     mov  [rcx],??_7BinaryReader@IO@@6B@     ; restamp on dtor entry
//     mov  rcx,[rcx+0x18]                     ; the ReaderWriter's Stream*
//     mov  r8,[rsp+0x48]                      ; &a dead 4-byte stack local
//     mov  edx,4
//     call [rax+0x18]                         ; Stream::Read(4, &scratch)
//     mov  [rbx],??_7ReaderWriter@IO@@6B@     ; base-unwind on dtor exit
//
// Slot +0x18 is index 3 == `Read` on every table in the family (Stream.h:63).
// The four bytes are read and **discarded** — the destination is a stack slot
// nothing reads afterwards. That is the read-side mirror of ~BinaryWriter's
// `Write(0xBBBBBBBB)` trailer (binary_writer.cpp:~30): the writer emits the
// trailer magic, the reader swallows it. The scratch buffer is reproduced as a
// real local because the Read genuinely stores through it.
//
// Retail INLINES ~BinaryReader into the deleting dtor at 0x180023a10; the
// out-of-line 0x1800a82f0 is the same instructions. Calling one from the other
// here is the same collapse binary_writer.cpp makes and is stated rather than
// hidden.
//
// ~Visitor (0x1800bac90) is TWO STAMPS AND NOTHING ELSE — no member is read.
//
// ----------------------------------------------------------------------------
// 4. IO::VisitorStream IS A NULL SINK, AND ITS FIVE STREAM SLOTS ARE 2 BYTES
// ----------------------------------------------------------------------------
// FileStream.h:354-401 pins the class and says so; this file is the code half.
// Read out of .text, each body complete:
//
//     0x1800baff0  GetSize        xor eax,eax ; ret      -> 0
//     0x1800bb000  GetCurrentPos  xor eax,eax ; ret      -> 0
//     0x1800bb010  SetCurrentPos  mov al,1    ; ret      -> true
//     0x1800bb020  Read           mov al,1    ; ret      -> true
//     0x1800bb030  Write          mov al,1    ; ret      -> true
//
// `Read` returns true WITHOUT touching the caller's buffer. That is faithful
// and it is the point: `?Add@ExternalFactory@Object@@` runs an External's own
// `Serialize` against one so the object walks itself exactly as a save would
// and every byte is swallowed. The only thing that happens is the side effect
// in the Encoding the constructor installs at +0x10.
//
// THE CONSTRUCTOR (0x1800bb090) is the only one with substance:
//     Stream ctor inlined:  vptr = ??_7Stream@ ; direction = 0 ; encoding = 0
//     vptr = ??_7VisitorStream@ ; direction = 1              // 1 == WRITING
//     e = MemAlloc(8, 4)                                     // the Encoding
//     if (e) *(void**)e = 0x1801559f8                        // Visitor encoding
//     if (this->encoding) MemFree(this->encoding)            // the ctor's own 0
//     this->encoding = e
//     return this
//
// Two things in that order are worth writing down. First, `direction` is set
// to 0 by the inlined Stream ctor and then to **1** — the null sink presents
// itself as a WRITE stream, which is what makes an External serialize rather
// than deserialize into it. Second, the `if (this->encoding) MemFree(...)` is
// unreachable on this path (the store four instructions earlier made it null)
// and is reproduced anyway because it costs one branch and retail emits it.
//
// **0x1801559f8 IS NOT 0x1801554a8.** The shared BinaryEncoding table the rest
// of runtime/src/io stamps is 0x1801554a8; the Visitor encoding is a DIFFERENT,
// also-unexported table at 0x1801559f8, and external_add.cpp:58 already records
// that. Both are unexported, so the only port-aware door is the ADDRESS door
// (`ResolveAddr(Module::Runtime, …)`, which is registry-first —
// reference_resolveaddr_consults_port_registry_first.md), and stamping the
// wrong one would produce a stream that walks the object and records nothing.
//
// ----------------------------------------------------------------------------
// 5. ??1Modification@IO@@ CLEARS THE EXPORTED SINGLETON, AND THAT IS THE POINT
// ----------------------------------------------------------------------------
// 0x1800b0470, in full — five instructions of substance:
//
//     mov  [rip+0x14c21f],0        ; ?_singleton@Modification@IO@@2PEAV12@EA
//     lea  rcx,[rbx+0x18]  ; call 0x180010b80   ; ~Array<Mod>(savedMods)
//     lea  rcx,[rbx+0x08]  ; jmp  0x180010b80   ; ~Array<Mod>(mods)   TAIL
//
// The ORDER is savedMods FIRST, then mods, and it is reproduced rather than
// tidied — the two arrays alias each other's element storage nowhere, but the
// claim "order does not matter here" is one this lane did not verify and does
// not need to make.
//
// **The singleton store is a SIDE EFFECT THAT MUST BE PUBLISHED INTO THE
// EXPORTED SLOT, not into storage of our own** — a port that owns its own copy
// is invisible until something reads the slot
// (reference_port_must_reproduce_side_effects.md).
// `?_singleton@Modification@IO@@2PEAV12@EA` is tagged UNPROMOTABLE(HARMFUL) in
// reg_mint_promoted.cpp, i.e. deliberately left on the DLL, so `RT_DATA_SLOT`
// hands back the DLL's slot and the store lands where retail's does. That is
// the correct outcome and it is why the tag matters here rather than being
// merely bookkeeping.
//
// `0x180010b80` (`Collection::Array<Mod>::~Array`) is ALREADY PORTED —
// `RtColl_ArrayModDtor_180010b80`, modification.cpp:573, registered by address
// at :905 — so it is reached through the ADDRESS door, which is registry-first
// and therefore lands on our body, not the DLL's
// (reference_resolveaddr_consults_port_registry_first.md).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <new>          // placement new into the retail hidden return slot

#include "../../funcs/rt_port.h"
#include "../../types/io/Stream.h"
#include "../../types/io/FileStream.h"
#include "../../types/io/Serializer.h"
#include "../../types/io/Modification.h"
#include "../../types/system/String.h"

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module module, uintptr_t ghidraAddr);

namespace RtIo {

namespace {

using MemFreeFn  = void  (*)(void*);
using VoidThisFn   = void  (*)(void*);

inline void MemFree(void* p) {
    // Null-tested INSIDE the callee in retail; handing it a null is faithful.
    RT_EXPORT_ONCE(MemFreeFn, "?MemFree@MemoryAllocator@System@@SAXPEAX@Z")(p);
}

// The one place the `flags & 2` divergence is spelled, so the four call sites
// cannot drift apart and the message always names its own class.
// Collection::Array<Mod>::~Array, 0x180010b80. Already ported by lane MOD
// (modification.cpp:573) and registered BY ADDRESS at :905, so the address
// door — which is registry-first — reaches our body.
using ArrayDtorFn = void* (*)(void*);
inline void ArrayModDtor(void* arrayField) {
    static ArrayDtorFn fn = nullptr;
    if (!fn) fn = reinterpret_cast<ArrayDtorFn>(
                      ResolveAddr(Module::Runtime, 0x180010b80ull));
    fn(arrayField);
}

[[noreturn]] void ArrayArmUnreachable(const char* cls, const char* producer) {
    std::fprintf(stderr,
        "\n[RtIo] FATAL: %s deleting dtor reached with flags&2 (delete[]).\n"
        "[RtIo] No code path array-allocates a %s: %s\n"
        "[RtIo] writes no array cookie, so the 8 bytes at this-8 are heap\n"
        "[RtIo] metadata, not a count. Reaching here means a wrong-typed\n"
        "[RtIo] pointer arrived, or a producer exists that Wave 19 lane\n"
        "[RtIo] IOBASE did not find -- see WAVE19_LANE_IOBASE.md.\n\n",
        cls, cls, producer);
    std::fflush(stderr);
    std::abort();
}

}  // namespace

// ===========================================================================
// ?GetSource@Stream@IO@@UEBA?AVString@System@@XZ   0x18000c6a0
// ===========================================================================
// `ret` is the HIDDEN RETURN SLOT (RDX in the shipped ABI). `self` is never
// read — see header §1 for the nine instructions this replaces.
extern "C" void* RtIo_Stream_GetSource(const void* self, void** ret) {
    (void)self;
    // ENDGAME B.3.1 step 2: the signature stays void** -- it is a registered
    // entry point another file (compressed_stream_write_probe.cpp) declares
    // and calls -- but `ret` is the same raw, uninitialised hidden return
    // slot either way, so the default ctor is constructed into it rather than
    // hand-written as a null-qword store.
    new (ret) System::String();
    return ret;
}

// ===========================================================================
// ?GetDirection@Stream@IO@@QEBA?AW4Direction@12@XZ   0x180006200
// ===========================================================================
// ENDGAME B.3.4 callers batch H. The WHOLE shipped body is
// `MOV EAX,dword ptr [RCX + 0x8]; RET` — NON-virtual (there is no slot for it
// in Stream's 7-slot table), const, and it reads `_direction` with no null
// test, which the door reproduces. The member itself is stated inline on
// `IO::Stream` (runtime/types/io/Stream.h); this is only its registered entry
// point, put here beside `RtIo_Stream_GetSource` because this file is where
// `IO::Stream`'s doors live.
//
// The door returns `int32_t` rather than the enum for the same reason
// `RtSerializer_GetDirection` does: a by-address caller takes the raw dword,
// and the ABI is EAX either way.
extern "C" int32_t RtIo_Stream_GetDirection(const void* self) {
    return static_cast<int32_t>(
        reinterpret_cast<const IO::Stream*>(self)->GetDirection());
}

// ===========================================================================
// MemoryStream vector deleting destructor   0x1800242a0   — vtable slot 0
// ===========================================================================
// ~MemoryStream ITSELF WAS ALREADY IN THE TREE and this lane nearly rewrote it.
// `runtime/src/io/compressedstream.cpp:657` (Wave 9 lane W) holds
// `RtIo_MemoryStream_Dtor`, registered under BOTH doors at :963-965, and it
// carries a diff oracle. My duplicate got as far as the LINKER, which reported
// it as `multiple definition` — the grep did not catch it because I searched
// for the mangled name and the ADDRESS and found only comments; the body is
// spelled `RtIo_MemoryStream_Dtor` in a file whose name says CompressedStream.
// (reference_grep_for_the_port_before_writing_it.md, and the diff table
// catching it rather than review.)
//
// So this is the missing COMPLEMENT, not a rewrite — and lane W said so in
// advance at compressedstream.cpp:966-971: "??_7MemoryStream@IO@@6B@ slot 0 is
// the DELETING destructor thunk (0x1800242a0), NOT this body, so registering
// ??1 does not change what a virtual `delete` dispatches to." This body is what
// that note was waiting for. It reaches lane W's ~MemoryStream through the
// EXPORT door, exactly as compressedstream.cpp:330 does, so a future change to
// that body is picked up here and there is one implementation, not two.
extern "C" void* RtIo_MemoryStream_DeletingDtor(MemoryStream* self, unsigned int flags) {
    if (flags & 2u)
        ArrayArmUnreachable("MemoryStream",
                            "every producer stack-constructs one or embeds it "
                            "at CompressedMemoryStream+0x68, which");
    RT_EXPORT_ONCE(VoidThisFn, "??1MemoryStream@IO@@UEAA@XZ")(self);
    if (flags & 1u) MemFree(self);
    return self;
}

// ===========================================================================
// IO::VisitorStream — the null sink. Header §4.
// ===========================================================================
// ENDGAME B.3.9 IO: the class is built by its real constructor
// (runtime/types/io/FileStream.h -- `?Add@ExternalFactory@`'s stack instance)
// and wears the compiler's table. The ctor / dtor / slot-0 / five slot doors
// that stamped and filled the minted ??_7VisitorStream row are deleted with it;
// the five slot bodies are the members below.

// ===========================================================================
// ??1Modification@IO@@QEAA@XZ   0x1800b0470   — header §5
// ===========================================================================
extern "C" void RtIo_Modification_Dtor(RtIO::Modification* self) {
    // Publish into the DLL's exported slot; see header §5.
    void** singleton = RT_DATA_SLOT(void**, "?_singleton@Modification@IO@@2PEAV12@EA");
    if (singleton) *singleton = nullptr;                        // 1800b0486

    ArrayModDtor(&self->savedMods);                             // 1800b0495  +0x18
    ArrayModDtor(&self->mods);                                  // 1800b04a4  +0x08  TAIL
}

}  // namespace RtIo

// ============================================================================
// ENDGAME B.3.4 STREAMS (4/6) — the five `IO::VisitorStream` slot bodies.
// ============================================================================
// A member of `IO::X` cannot be defined inside `namespace RtIo` — `IO` and
// `RtIo` are siblings, and C++ requires a member definition to appear in a
// namespace ENCLOSING the class (B34_BRIEFS section A.2). So the bodies open
// their own `namespace IO` block after the doors close, exactly as
// memory_stream.cpp, file_stream.cpp and serialize_family.cpp do.
//
// These are the whole class: two instructions each, read out of .text, and
// header section 4 above enumerates them with their raw bytes. `this` is never
// touched by any of the five — retail's bodies do not even load RCX — so none
// of them names it, and that absence is the null sink's definition rather than
// an omission.
//
// `IO::VisitorStream::Write` is the class's KEY FUNCTION (the first non-inline,
// non-pure virtual it declares), so GCC emits `vtable for IO::VisitorStream`
// in THIS translation unit. That is why the host unit build needs the stub
// copies in tests/unit/io_stream_abi_stubs.cpp: this file stands on the port
// registry and cannot link into a host test.
namespace IO {

// ?Write@VisitorStream@IO@@UEAA_NHPEBX@Z   0x1800bb030   `mov al,1 ; ret`
// IT ACCEPTS EVERY BYTE AND STORES NONE. This is the sink half of the
// reference-visit pass; the Encoding at +0x10 is where the walk's only
// observable effect happens.
bool VisitorStream::Write(int, const void*) { return true; }

// ?Read@VisitorStream@IO@@UEAA_NHPEAX@Z   0x1800bb020   `mov al,1 ; ret`
// Returns true WITHOUT writing to `dst` — faithful, and the mechanism of the
// whole reference-visit pass. A port that filled the buffer would be inventing
// data retail never produces.
bool VisitorStream::Read(int, void*) { return true; }

// ?SetCurrentPos@VisitorStream@IO@@UEAA_NH@Z   0x1800bb010   `mov al,1 ; ret`
bool VisitorStream::SetCurrentPos(int) { return true; }

// ?GetCurrentPos@VisitorStream@IO@@UEBAIXZ   0x1800bb000   `xor eax,eax ; ret`
uint32_t VisitorStream::GetCurrentPos() const { return 0; }

// ?GetSize@VisitorStream@IO@@UEBAIXZ   0x1800baff0   `xor eax,eax ; ret`
uint32_t VisitorStream::GetSize() const { return 0; }

}  // namespace IO

// ============================================================================
// Registration
// ============================================================================
// EXPORTED bodies get BOTH doors (name and address). The four deleting
// destructors are UNEXPORTED and get the address door only — which is also the
// door their mint slots use (`RT_MINT_UNNAMED` on each VA), so registering the
// address is exactly what turns each row's slot 0 from a trap into this body.
//
// `AEAA`/`AEBA` would mean PRIVATE, not unexported; every name below was
// checked against the shipped DLL's export directory before it was written
// (reference_resolveexport_phantom_names.md, reference_aeaa_is_private_not_unexported.md).
int RtIoBaseVtablesRegister() {
    using namespace RtIo;
    int n = 0;

    // ---- the six-row prerequisite -----------------------------------------
    RegisterRuntimeExport("?GetSource@Stream@IO@@UEBA?AVString@System@@XZ",
                          (void*)&RtIo_Stream_GetSource); ++n;
    RegisterRuntimeAddr(0x18000c6a0, (void*)&RtIo_Stream_GetSource);

    // ENDGAME B.3.4 callers batch H — by NAME (what runtime_census.py reads)
    // and by ADDRESS, the same pair the GetSource row above uses.
    RegisterRuntimeExport("?GetDirection@Stream@IO@@QEBA?AW4Direction@12@XZ",
                          (void*)&RtIo_Stream_GetDirection); ++n;
    RegisterRuntimeAddr(0x180006200, (void*)&RtIo_Stream_GetDirection);

    // ---- MemoryStream ------------------------------------------------------
    // ??1MemoryStream is lane W's, registered in compressedstream.cpp:963.
    RegisterRuntimeAddr(0x1800242a0, (void*)&RtIo_MemoryStream_DeletingDtor); ++n;

    // ---- IO::Modification ---------------------------------------------------
    RegisterRuntimeExport("??1Modification@IO@@QEAA@XZ",
                          (void*)&RtIo_Modification_Dtor); ++n;
    RegisterRuntimeAddr(0x1800b0470, (void*)&RtIo_Modification_Dtor);

    return n;
}
