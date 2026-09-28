// ============================================================================
// runtime/src/io/serializer.cpp — IO::Serializer, ported.   Wave 4, lane A.
// ============================================================================
//
// Byte-faithful reimplementation of the 22 exported IO::Serializer members our
// tree reaches, against RETAIL Runtime-x64.dll (image base 0x180000000). Layouts
// and the evidence for every offset are in runtime/types/io/Serializer.h; the
// oracle story and what each diff site can and cannot prove are in
// docs/runtime_port/WAVE4_LANE_A.md.
//
// ----------------------------------------------------------------------------
// HISTORY OF THIS FILE, AND WHAT CHANGED
// ----------------------------------------------------------------------------
// This file existed before the wave as a 2025-era debugging aid: the same
// forwarders, gated on BANISHED_REIMPL_SERIALIZER, default OFF, registered
// only into the exe's dispatch slots. PLAN.md §2.1 calls it "the one existing
// runtime reimpl". Wave 4 lane A re-derived every fact in it from the shipped
// DLL rather than trusting it, and three of its claims were wrong:
//
//   1. "the `version` ctor arg is IGNORED" — FALSE. It is passed through in
//      R8 to the encoding factory and ends up written into the stream. See
//      runtime/types/io/Serializer.h.
//   2. serializer+0x08 was typed `Encoding*`. It is an `IO::ReaderWriter*`;
//      `IO::Encoding` is a different class living at stream+0x10.
//   3. the fallback path resolved the original with ResolveExport(). Once the
//      port registers, ResolveExport returns THIS FUNCTION — the fallback
//      would have called itself. Fixed: the fallback uses the raw DLL door,
//      which goes through the raw export door and caches (bare the raw DLL door is
//      GetProcAddress, and cost Wave 2 five points of sim time).
//
// It also only covered 12 of the 22 members. The three scalar widths it was
// missing (signed char&, short&, unsigned short&), the five array widths
// (char*, signed char*, bool*, unsigned int*, float*) and the ctor/dtor are
// ported here.
//
// ----------------------------------------------------------------------------
// RETAIL ADDRESSES AND PINNED ReaderWriter VTABLE SLOTS
// ----------------------------------------------------------------------------
// Left column: the retail address and export ordinal, read from the shipped
// DLL's export directory with pefile on 2026-09-06 (all 22 names verified
// present — reference_resolveexport_phantom_names.md).
// Right column: the tail-jump displacement decoded from that function's own
// three instructions in the shipped binary.
//
// *** `VerifyForwarder()` NO LONGER EXISTS, AND THIS BANNER USED TO CLAIM IT
// DID. *** (Corrected in B.3.4 block 3.) It was a registration-time
// cross-check that re-decoded each retail forwarder's tail-jump displacement
// out of the loaded DLL and refused the whole family if any of them disagreed
// with the constant we had transcribed. It went when the DLL went: with no
// Runtime-x64.dll in the process there are no retail bytes left to read, so the
// check had nothing to compare against and was deleted rather than left
// returning a constant `true`. Nothing was left behind — the only traces were
// these two comment references, which is why a reader of block 3's brief was
// sent looking for a function that is not here. What replaces it is static:
// the slot constants are `rt::io::rw_vtbl::k*`, each pinned by a
// `static_assert` in runtime/types/io/Serializer.h and re-proved executably by
// tests/unit/io_vtable_abi_test.cpp.
//
//   0x1800b9960 ord 381   ctor(Stream&,int)          (see the ctor body)
//   0x1800b99b0 ord 675   ~Serializer()              rw vtable +0x00, arg 1
//   0x1800070b0 ord 3011  GetVersion() const         rw vtable +0x08
//   0x180006290 ord 2593  GetEncoding()              stream+0x10, no dispatch
//   0x1800061e0 ord 3742  Serialize(bool&)           rw vtable +0x78
//   0x18007c3b0 ord 3732  Serialize(signed char&)    rw vtable +0xC0
//   0x180055c80 ord 3734  Serialize(uchar&)          rw vtable +0xB8
//   0x18007c3c0 ord 3735  Serialize(short&)          rw vtable +0xB0
//   0x1800675a0 ord 3736  Serialize(ushort&)         rw vtable +0xA8
//   0x1800061d0 ord 3737  Serialize(int&)            rw vtable +0xA0
//   0x180035000       -   Serialize(int64&)          rw vtable +0x90
//                         ENDGAME B.3.4b SYSTEM (5), the 24th door.
//   0x1800061f0 ord 3738  Serialize(uint&)           rw vtable +0x98
//   0x180021660 ord 3739  Serialize(float&)          rw vtable +0x80
//   0x180052920 ord 3753  Serialize(n, bool*)        rw vtable +0x20
//   0x180054160 ord 3745  Serialize(n, uchar*)       rw vtable +0x60
//   0x18007c400 ord 3746  Serialize(n, short*)       rw vtable +0x58
//   0x180034fe0 ord 3747  Serialize(n, ushort*)      rw vtable +0x50
//   0x180077660 ord 3748  Serialize(n, int*)         rw vtable +0x48
//   0x180029e60 ord 3749  Serialize(n, uint*)        rw vtable +0x40
//   0x1800150d0 ord 3750  Serialize(n, float*)       rw vtable +0x28
//   0x180034fd0 ord 3744  Serialize(n, char*)        rw vtable +0x18
//   0x18007c3f0 ord 3743  Serialize(n, signed char*) rw vtable +0x68
//   0x1800346d0 ord 3804  SerializeBytes(size,void*) rw vtable +0xC8
//
// Every displacement above is independently confirmed by the contents of
// ??_7BinaryReader@IO@@6B@ (0x1801552e0) and ??_7BinaryWriter@IO@@6B@
// (0x180155210): the slot at that offset is the exported
// ?Serialize@Binary{Reader,Writer}@IO@@ whose mangled parameter type matches.
//
// NOT PORTED, because our tree does not call them (call_surface_20260906.csv
// has no row for any of them) and an uncalled port is an unverified port:
//   ?Skip@Serializer@IO@@              0x1800070a0  rw vtable +0x10
//   ?GetStream@Serializer@IO@@         0x18000a0a0  `MOV RAX,[RCX]; RET`
//   Serialize(char&) 0x18007c3e0 (rw +0x70), and the int64/uint64 scalar and
//   array forms.
//
// CORRECTED, ENDGAME B.3.4 callers batch H: `?GetDirection@Serializer@IO@@`
// (0x180006210) WAS on that list and is now PORTED — see the GetDirection door
// below. It was invisible to the call-surface census because the exe INLINES
// it, so its ~37 uses across code/Game are hand-written `MOV RAX,[ser];
// CMP dword [RAX+8]` transcriptions rather than calls.
//
// The two char-array forms ARE ported despite having no ResolveExport site,
// because two exe PE-import slots are imports of them and are read by
// retail-shaped exe code: 0x140152490 is Serialize(n,char*) and 0x140152478 is
// Serialize(n,signed char*) (tools/dispatch_slot_map.py). Both are today bound
// in standalone/address_registry.cpp to the WRONG wrapper -- see the defect
// list in docs/runtime_port/WAVE4_LANE_A.md; the correct wrappers now exist.
//
// PLATFORM: nothing here touches Win32. The only host dependency is
// the raw export door, declared in rt_port.h precisely so <windows.h> stays out
// of runtime/src/ (PLAN §4.1 decision 1).
// ============================================================================

#include "serializer.h"

#include "../../funcs/rt_port.h"

#include <cstdio>
#include <cstring>
#include <new>       // the ctor door placement-constructs into the caller's bytes

using rt::io::Encoding;
using rt::io::ReaderWriter;
using rt::io::Serializer;
using rt::io::Stream;

// ============================================================================
// ENDGAME B.3.4 block 3 — THIS FILE IS NOW DOORS ONLY
// ============================================================================
//
// The 22 bodies moved onto the real class `IO::Serializer`
// (runtime/types/io/Serializer.h, bodies in serializer_class.cpp, which is
// registry-free so `make unit` can drive the real class). What is left here is
// the `RtSerializer_*` forwarders the exe's Serializer import slots are bound
// to; B3R RT-IO deleted every registration row and `RtIoSerializerRegister()`.
//
// *** EVERY DOOR DELEGATES WITH A PLAIN, UNQUALIFIED CALL, AND THAT IS NOT AN
// OVERSIGHT. *** Blocks 1 and 2 had to write the delegation qualified —
// `reinterpret_cast<IO::BinaryReader*>(self)->IO::BinaryReader::SerializeInt(...)`
// — because a live `BinaryReader` wears the MINTED table 0x1801552e0 whose
// slots resolve back to these very doors, so a virtual call inside a door would
// dispatch through the minted table INTO ITSELF: infinite recursion, not
// delegation. `IO::Serializer` has NO VTABLE AT ALL (B34_RULES §1.2 rule 2 /
// §3.1), so there is nothing to dispatch through and `Ser(self)->Serialize(...)`
// is already a direct call. A reader arriving from blocks 1-2 should expect the
// `Class::` and not find it — this is why.
//
// B3R RT-IO: the registration rows of these doors are gone except the ctor's
// (one unconverted reader in runtime/src/ui). The door BODIES stay reachable by
// name: the exe dispatch slots at 0x1401524xx in standalone/address_registry.cpp
// are PE IMPORT slots filled directly with these function pointers.
//
// WHAT THE PER-CALL SITE PROVES, AND WHAT IT DOES NOT
//
// These functions MUTATE THE STREAM. There is no "call ours, call theirs,
// compare" for them: running the original as an oracle would write the record
// a second time and desync every record after it — precisely
// reference_slot3_wrapper_must_resolve_first.md. So no site here calls an
// original, and none ever did. What proves the bytes is the save/load pass: a
// wrong slot consumes or emits the wrong number of bytes and the damage shows
// up later, on an innocent-looking record. That is what the resave gate is.

// `self` arrives as the POD view every unconverted caller spells. The two
// views are cross-asserted field for field in runtime/types/io/Serializer.h,
// which is what makes this cast honest rather than hopeful.
static inline IO::Serializer* Ser(Serializer* self) {          // B.3.4 bridge
    return reinterpret_cast<IO::Serializer*>(self);
}

#define RT_SER_SCALAR(FN, T)                                                   \
    void FN(Serializer* self, T* v, const char* name) {                        \
        Ser(self)->Serialize(*v, name);                                        \
    }

#define RT_SER_ARRAY(FN, T)                                                    \
    void FN(Serializer* self, int n, T* d, const char* name) {                 \
        Ser(self)->Serialize(n, d, name);                                      \
    }

extern "C" {

// ---- scalars ---------------------------------------------------------------
RT_SER_SCALAR(RtSerializer_Serialize_Bool, bool)   // IO::Serializer::Serialize(bool&)
RT_SER_SCALAR(RtSerializer_Serialize_SChar, int8_t)   // IO::Serializer::Serialize(schar&)
RT_SER_SCALAR(RtSerializer_Serialize_UChar, uint8_t)   // IO::Serializer::Serialize(uchar&)
RT_SER_SCALAR(RtSerializer_Serialize_Short, int16_t)   // IO::Serializer::Serialize(short&)
RT_SER_SCALAR(RtSerializer_Serialize_UShort, uint16_t)   // IO::Serializer::Serialize(ushort&)
RT_SER_SCALAR(RtSerializer_Serialize_Int, int32_t)   // IO::Serializer::Serialize(int&)
// ENDGAME B.3.4b SYSTEM (5) — the 24th door. `_J` is MSVC's `__int64`, which is
// `int64_t` on this toolchain; `long` is FOUR bytes on Win64 and would bind
// neither overload, so there is no ambiguity to create here.
RT_SER_SCALAR(RtSerializer_Serialize_Int64, int64_t)   // IO::Serializer::Serialize(int64&)
RT_SER_SCALAR(RtSerializer_Serialize_UInt, uint32_t)   // IO::Serializer::Serialize(uint&)
RT_SER_SCALAR(RtSerializer_Serialize_Float, float)   // IO::Serializer::Serialize(float&)

// ---- arrays ----------------------------------------------------------------
RT_SER_ARRAY(RtSerializer_Serialize_CharArr, char)   // IO::Serializer::Serialize(n,char*)
RT_SER_ARRAY(RtSerializer_Serialize_SCharArr, int8_t)   // IO::Serializer::Serialize(n,schar*)
RT_SER_ARRAY(RtSerializer_Serialize_BoolArr, bool)   // IO::Serializer::Serialize(n,bool*)
RT_SER_ARRAY(RtSerializer_Serialize_UCharArr, uint8_t)   // IO::Serializer::Serialize(n,uchar*)
RT_SER_ARRAY(RtSerializer_Serialize_ShortArr, int16_t)   // IO::Serializer::Serialize(n,short*)
RT_SER_ARRAY(RtSerializer_Serialize_UShortArr, uint16_t)   // IO::Serializer::Serialize(n,ushort*)
RT_SER_ARRAY(RtSerializer_Serialize_IntArr, int32_t)   // IO::Serializer::Serialize(n,int*)
RT_SER_ARRAY(RtSerializer_Serialize_UIntArr, uint32_t)   // IO::Serializer::Serialize(n,uint*)
RT_SER_ARRAY(RtSerializer_Serialize_FloatArr, float)   // IO::Serializer::Serialize(n,float*)

// ============================================================================
// GetVersion — FULL ORACLE
// ============================================================================
// Retail 0x1800070b0 is the same three-instruction forwarder, tail-jumping
// rw vtable +0x08. Diffing calls the virtual twice, which is safe here and
// only here because both implementations of that slot are a single load:
//   ?GetVersion@BinaryReader@IO@@UEBAHXZ  0x180023930  MOV EAX,[RCX+0x28]; RET
//   ?GetVersion@BinaryWriter@IO@@UEBAHXZ  0x180023ab0  MOV EAX,[RCX+0x28]; RET
// If a third ReaderWriter subclass ever gives that slot a side effect this
// site becomes unsound.
int RtSerializer_GetVersion(Serializer* self) {
    return Ser(self)->GetVersion();
}

// ============================================================================
// GetEncoding — FULL ORACLE
// ============================================================================
// Retail 0x180006290 is `MOV RAX,[RCX]; MOV RAX,[RAX+0x10]; RET` — two loads,
// no dispatch, no side effect. It returns the Stream's IO::Encoding (the
// policy object), NOT the Serializer's ReaderWriter. Diffing it is free.
// The member returns a REFERENCE, because retail's mangling does
// (`AEBVEncoding@2@`).

// ============================================================================
// GetDirection — FULL ORACLE       ENDGAME B.3.4 callers batch H
// ============================================================================
// Retail 0x180006210 is `MOV RAX,[RCX]; MOV EAX,[RAX+0x8]; RET` — two loads,
// no dispatch, no side effect, so diffing it is free. It answers with the
// STREAM's direction; `IO::Stream`'s own 0x180006200 is the one-load form of
// the same field and is stated inline on the class (runtime/types/io/Stream.h).
//
// The banner at the top of this file listed GetDirection under "NOT PORTED,
// because our tree does not call them (call_surface_20260906.csv has no row
// for any of them) and an uncalled port is an unverified port". That was true
// when it was written. It stopped being true when the B.3.4 caller batches
// measured ~37 hand-written copies of exactly these two loads across
// code/Game — the exe INLINES the member at every one of them (e.g.
// Application-x64.exe 0x1400da960 and 0x1400ed3c5, both `MOV RAX,[RSP+ser];
// CMP dword ptr [RAX + 0x8],0x0`), which is why they never showed up as calls.
// The banner's line 91 is corrected in place above.

// ============================================================================
// ctor — SHADOW ORACLE (the original is NOT run; it would move the cursor)
// ============================================================================
// Retail 0x1800b9960, transcribed instruction for instruction:
//
//     MOV [RCX],RDX                 self->stream = stream
//     MOV qword [RCX+0x08],0        self->readerWriter = nullptr
//     CMP dword [RDX+0x08],0        stream->direction
//     MOV RCX,[RDX+0x10]            RCX = stream->encoding      (the factory)
//     MOV RAX,[RCX]                 RAX = factory vtable
//     MOV RDX,RBX                   RDX = this                  (arg 2)
//     JNZ writing
//     CALL [RAX+0x18] / [RAX+0x20]  arg 3 = R8D = version, never touched
//     MOV [RBX+0x08],RAX
//     MOV RAX,RBX ; RET             ctor returns `this`
//
// The `int version` argument is NOT ignored (the claim this file used to make):
// R8D is live all the way to the CALL, and the thing being called is
// ??0Binary{Reader,Writer}@IO@@QEAA@AEAVSerializer@1@H@Z — (Serializer&, int).
// BinaryWriter stores it and writes it to the stream. That pins the arity of
// both factory slots at 3, which the Wave 4 brief listed as open.
//
// WHY THERE IS NO RUNNABLE ORACLE: the ReaderWriter this creates reads (load)
// or writes (save) TWELVE BYTES of stream in its own constructor. Running the
// DLL ctor a second time on the same stream would consume or emit a second
// 12-byte header. The shadow below therefore checks the post-state instead,
// and every clause of it is a real claim about the port:
//
//   stream stored      -> self->stream == stream
//   factory ran        -> self->readerWriter != nullptr
//   ARG 2 WAS `this`   -> rw->serializer == self && rw->stream == stream
//                         (this is the arity/argument-order proof; if we had
//                          passed the factory or the stream in RDX these would
//                          be wrong)
//   RIGHT SLOT CHOSEN  -> rw->vtable == ??_7BinaryReader (direction == 0)
//                                    or ??_7BinaryWriter (otherwise)
//   ARG 3 REACHED IT   -> on the writing path only, rw+0x28 == version. On the
//                         reading path BinaryReader immediately overwrites
//                         +0x28 with a dword read from the stream, so there is
//                         nothing to compare.
//
// CAVEAT, stated so a divergence is read correctly: the vtable clause assumes
// the Stream carries the standard binary IO::Encoding. A stream with a custom
// encoding would produce some other ReaderWriter subclass and trip that clause
// without anything being wrong. The log line prints the vtable pointer so the
// two cases are distinguishable.
//
// B.3.4 block 3: the body is `IO::Serializer::Serializer(Stream&, int)`
// (serializer_class.cpp). B3R RT-IO deleted the door once its last reader
// (runtime/src/ui/dialog_settings.cpp) called the constructor directly.

// ============================================================================
// dtor — NO ORACLE
// ============================================================================
// Retail 0x1800b99b0:
//     MOV RCX,[RCX+0x08] ; TEST RCX,RCX ; JZ out
//     MOV RAX,[RCX] ; MOV EDX,1 ; JMP [RAX]     ; scalar-deleting dtor, flag 1
// Flag 1 means "also free the memory", so this DELETES the ReaderWriter, and
// BinaryReader::~BinaryReader (0x1800a82f0) reads a further 4 bytes from the
// stream on its way out. Running the original as an oracle would double-free
// and double-read. The body is `IO::Serializer::~Serializer`
// (serializer_class.cpp); B3R RT-IO deleted the door, which had no reader left.
//
// Note retail does NOT null self->readerWriter afterwards. Kept.

} // extern "C"
