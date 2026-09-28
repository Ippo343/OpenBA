// ============================================================================
// runtime/src/ui/dialog_vtable.cpp — WAVE 10C LANE DLG8
//   the seven remaining code slots of ??_7Dialog@UI@@6B@
// ============================================================================
//
// THE ROW, AND WHAT WAS ACTUALLY BLOCKING IT
// ---------------------------------------------------------------------------
// `tools/runtime_census.py --unported --namespace UI` lists
// `??_7Dialog@UI@@6B@` (DLL 0x1801562a0, 9 slots). Wave 10B lane MINT minted
// the vtable DATA; what was missing was its CONTENTS. At this lane's baseline
// the readiness screen read
//
//     BLOCKED  ??_7Dialog@UI@@6B@   slots= 9 ported= 1 TRAP= 8 purecall= 0
//
// — one ported slot (4, `?Compile@External@Object@@`, runtime/src/object/
// external.cpp) and eight named abort traps. This file closes SEVEN of the
// eight. The eighth, slot 0, is the unexported VECTOR deleting destructor at
// 0x180038b20 and belongs to the 37-row family in
// runtime/src/object/deleting_dtor.cpp; this lane adds it there as a 38th row
// rather than writing a 38th copy of that body here. See §"SLOT 0" below.
//
// GREP-BEFORE-WRITE (CLAUDE.md, reference_grep_for_the_port_before_writing_it):
// all eight mangled names and all nine slot addresses were grepped across
// `code/`, `runtime/`, `engine/` and `standalone/` before a line was written.
// `runtime/src/ui/dialog.cpp`, `dialog_ctor_dtor.cpp`, `dialog_create.cpp` and
// `dialog_settings.cpp` all mention `??_7Dialog@UI@@6B@`, but every one of them
// mentions it as a value they STAMP, never as a slot body they define. No
// collision, and nothing here is a duplicate registration.
//
// ---------------------------------------------------------------------------
// THE SpriteSheet ANALOGY WAS CHECKED, NOT ASSUMED — AND IT BREAKS IN FOUR
// PLACES
// ---------------------------------------------------------------------------
// The brief pointed at `??_7SpriteSheet@UI@@6B@` (lane SS,
// runtime/src/ui/spritesheet_vtable.cpp) as the same slot SHAPE, and it is: 9
// slots, same order, same `Object::External` family idiom. But lane UI3's array
// dtor recipe was WRONG last wave in exactly this way — a recipe from a good
// lane, followed literally, would have swapped a body that destroys every
// element for one that only frees the block
// (reference_a_written_recipe_can_be_wrong.md). So every Dialog body below was
// disassembled, and so was its SpriteSheet analogue, before anything was
// reused. Four of the seven DIFFER, and three of the four differences would
// have been silent:
//
//   1. `Construct` / `Reconstruct` write a 0x88-byte object, not a 0x51-byte
//      one, and TWO of the stores are not zero-fills:
//          MOV dword [X+0x30],0xffffffff      <- NOT zero
//          MOV dword [X+0x34],0
//      Copying SpriteSheet's all-zeros `ConstructInPlace` would have left
//      `_backgroundColor` at 0 (transparent black) instead of 0xFFFFFFFF, on
//      every Dialog our ctor builds. Nothing would crash.
//   2. `+0x40` is stored as a DWORD and `+0x44` is never written by either the
//      ctor or Construct. A qword zero-fill there writes four bytes retail
//      leaves alone — the same four bytes lane DLG2's ctor oracle already
//      excludes.
//   3. SpriteSheet's `Construct` ends with a single BYTE store (`self[0x50]=0`)
//      and 0x51..0x57 is never touched; Dialog's ends with a QWORD at +0x80 and
//      the written extent is the whole class. The oracle windows are therefore
//      different, and an oracle that compares bytes NEITHER side writes fails
//      100% of calls (reference_oracle_must_not_compare_unwritten_bytes.md).
//   4. `Serialize` is not remotely the same body. SpriteSheet's is two strings,
//      three scalars, one array and one ExternalPtr. Dialog's is FOUR arrays,
//      three of them walked element-by-element with a different per-element
//      shape each, plus five scalars, three ExternalPtrs and a String — and it
//      reaches NINE unexported internals by address where SpriteSheet's reaches
//      three. It is transcribed from scratch below, instruction by instruction.
//
// The three that genuinely ARE the same shape (`TypeName`, `Type`,
// `GetFileVersion`) say so at the body, and `Destruct` is instruction-identical
// to SpriteSheet's three-instruction tail jump. Read out of the DISASSEMBLY,
// never Ghidra's decompiled C (reference_ghidra_c_output_vs_disassembly.md).
//
// ---------------------------------------------------------------------------
// SLOT 0 — WHY IT IS NOT IN THIS FILE
// ---------------------------------------------------------------------------
// 0x180038b20 disassembles as the family body verbatim:
//
//     TEST DL,0x2 / JZ scalar
//     MOV R8D,[RCX-0x8]          the array cookie
//     LEA R9,[0x180112e40]       ~Dialog
//     MOV EDX,0x88               sizeof(UI::Dialog)
//     CALL 0x18007ec98           `eh vector destructor iterator`
//     TEST SIL,0x1 / LEA RCX,[RBX-0x8] / CALL 0x180006a10   operator delete
//     LEA RAX,[RBX-0x8] / RET                       ... and returns the BLOCK
//   scalar:
//     CALL 0x180112e40 / TEST SIL,0x1 / MOV RCX,RBX / CALL 0x1800384d0
//     MOV RAX,RBX / RET
//
// i.e. it is a K1 ("call the dtor") row with `dtorVa=0x180112e40`,
// `elemSize=0x88`. Both constants are now re-derived from the shipped bytes by
// `tools/rt_deleting_dtor_verify.py`, which reads the table out of
// deleting_dtor.cpp and checks it against the DLL — the row this lane added is
// checked by the same instrument as the other 37, which is the whole reason to
// put it there rather than here.
//
// TWO FACTS THAT FELL OUT OF IT, both new:
//
//   * `sizeof(UI::Dialog) == 0x88`, MEASURED. runtime/types/ui/Dialog.h has
//     carried an explicit refusal to assert a size, on the correct ground that
//     a constructor's written extent is a LOWER BOUND and not a measurement
//     (reference_never_written_needs_by_whom.md — a written extent belongs to a
//     WRITER). The array branch's `MOV EDX,0x88` is a different witness with a
//     different relationship to the fact: it is the STRIDE the compiler walks a
//     `Dialog[]` with, which is `sizeof` by definition. The assert is now in
//     that header, pinned to this instruction.
//   * `0x1800384d0` — the scalar branch's `operator delete` — is EXPORTED, as
//     `??3Dialog@UI@@SAXPEAX@Z`, a CLASS-SCOPED operator delete. The family's
//     banner lists ten `operator delete` addresses and calls them all
//     "per-translation-unit copies"; this one is a per-CLASS copy with a name
//     in the export directory, and it is byte-identical to the other ten
//     (`if (p) { Enter(cs); Free(_singleton, p); Leave(cs); }`, disassembled).
//     It changes nothing about the port — the family calls one canonical
//     MemFree — but "unexported internal" was the wrong description of it, and
//     that is the exact class of mistake
//     reference_grep_cannot_tell_internal_from_unported_export.md is about.
//
// ---------------------------------------------------------------------------
// WHAT THIS FILE PROVES ABOUT UI::Dialog's LAYOUT, AND WHAT IT RENAMES
// ---------------------------------------------------------------------------
// `?Serialize@` reads or writes fourteen distinct fields, which is more of this
// class than the whole rest of the tree touches. Four offsets that Dialog.h
// carried as `_unknownNN` are now pinned, and two of them get real names off
// their own exported accessors rather than off a guess:
//
//   +0x30  uint32  _backgroundColor  `?GetBackgroundColor@Dialog@UI@@QEBAIXZ`
//                                    (0x1800388a0) is `MOV EAX,[RCX+0x30]; RET`
//                                    and Serialize records it through
//                                    rw_vtbl::kScalarUInt. The ctor seeds it
//                                    0xFFFFFFFF, not 0.
//   +0x34  int32   _drawLevel        `?GetDrawLevel@Dialog@UI@@QEBAHXZ`
//                                    (0x180038910) is `MOV EAX,[RCX+0x34]; RET`
//                                    and Serialize records it through
//                                    kScalarInt — LAST of all fourteen.
//   +0x70  int32   _templateCount    Serialize's third array bound.
//   +0x80  int32   _referenceCount   Serialize's fourth array bound. Dialog.h
//                                    said of +0x80 "ctor zeroes it; never
//                                    read". It is read here, and it is the size
//                                    of the `_references` array at +0x78 —
//                                    which is what makes `?GetReference@`'s
//                                    unchecked index an unchecked index into a
//                                    KNOWN-length array rather than into an
//                                    anonymous pointer.
//
// ---------------------------------------------------------------------------
// NINE ADDRESS DOORS, AND WHAT THEY COST
// ---------------------------------------------------------------------------
// `?Serialize@` calls eleven functions. TWO are exported and already ported
// (`?Serialize@String@System@@` 0x1800c9930, and the ReaderWriter virtuals,
// which are dispatched not called). The other NINE are unexported internals
// reached through `ResolveAddr(Module::Runtime, …)`. That door consults the
// port registry FIRST (reference_resolveaddr_consults_port_registry_first.md),
// so each becomes our body the moment somebody registers it and is the DLL's
// until then — but it also GROWS the cutover surface by nine
// (reference_porting_can_grow_the_cutover_surface.md), which is stated here and
// in the lane doc rather than left for a later census to find.
//
// EVERY ONE WAS CHECKED AGAINST THE EXPORT TABLE, not just grepped — a source
// grep cannot tell an unexported internal from an export nobody ported yet
// (reference_grep_cannot_tell_internal_from_unported_export.md). All nine are
// genuinely absent from Runtime-x64.dll's export directory:
//
//   0x180038f90  ExternalPtr<UI::ElementDescription>::Serialize. Named by lane
//                EXTPTR (WAVE10_LANE_EXTPTR.md:81) — one of the 25
//                instantiations that lane tabulated, of which it registered
//                four and deliberately left the rest. Called from THREE places
//                in this body (+0x38, each named template, each config entry).
//   0x180004590  ExternalPtr<UI::SpriteSheet>::Serialize (EXTPTR:75). +0x20.
//                NOTE the collision hazard: 0x180004590 in **VideoDX11-x64.dll**
//                is `FillSwapChainDesc`, and dx11/ names that address in four
//                places. Different module, different image; the Module::Runtime
//                argument is what keeps them apart, and it is spelled at every
//                call.
//   0x1800756d0  ExternalPtr<UI::DialogControllerConfig>::Serialize (EXTPTR:96).
//   0x180075d20  Array<DialogNamedTemplate>::SetSize, stride 0x10.
//   0x180075b20  Array<DialogControllerConfigEntry>::SetSize, stride 0x20.
//   0x1800492d0  Array<ExternalPtr<ElementDescription>>::SetSize, stride 0x08.
//   0x18002f070  Array<ExternalPtr<Object::External>>::SetSize, stride 0x08.
//                All four SetSizes call MemAlloc/MemFree, so they are squarely
//                inside the class-B allocator family and are NOT this lane's
//                rows — the same call lane SS made for 0x180078950.
//   0x1800493d0  the `_templates` element walker (count, ExternalPtr[]).
//   0x18002f170  the `_references` element walker (count, ExternalPtr[]).
//
// ---------------------------------------------------------------------------
// ATTENDANCE — read this before believing any A/B in the lane doc
// ---------------------------------------------------------------------------
// Six of these seven bodies read a constant or restamp an object; an A/B boot
// with and without `??_7Dialog@UI@@6B@` promoted can be byte-identical whether
// they ran a million times or never once. Lane DLGV hit exactly that: a GREEN
// readiness screen and a promotion it correctly refused, because its ATTENDANCE
// counter read zero in both halves of both pairs. So this file counts ENTRIES,
// unconditionally, on every build, and prints them at exit — the idiom from
// dialog_controller_base.cpp and dialog_settings.cpp.
//
// ============================================================================

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>

#include "../../funcs/rt_port.h"
#include "../../types/io/Serializer.h"
#include "../../types/ui/Dialog.h"
#include "../collection/array_internal.h" // ENDGAME B.3.2 step 2 — kSpec_ExtStr10at08 etc.

// Forward-declared rather than pulled in from standalone/resolve.h, for the
// reason rt_port.h gives: that header drags in <windows.h> and runtime/src/
// must stay platform-neutral. Byte-identical to resolve.h's declaration; the
// same pair appears in spritesheet_vtable.cpp and elementdescription_serialize.cpp.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace RtUiDlgV {

namespace {

// (ENDGAME B.3.9 TI-UI: slots 1/2 and TypeId -- 0x180038440 / 0x180038420 /
// 0x180038430 -- are DECLARE_TYPE's, over the class's own record.)

}  // namespace
}  // namespace RtUiDlgV

// ============================================================================
// ENDGAME B.3.9 block UI-D: the slot doors that stood here (Type, TypeName,
// GetFileVersion, Destruct, Construct, Reconstruct, and Serialize below) and
// their `[DlgVtbl] ATTENDANCE` counters are RETIRED with the minted
// `??_7Dialog@UI@@6B@`, their only reader. Construct is `::new (this)
// Dialog()` — the same stores the in-place copy at 0x180038750 inlines (its
// null test sat in the door; a virtual call always has a `this`). ONE door
// stays: `?TypeId@Dialog@UI@@SAIXZ` (0x180038430) is a static export, not a
// slot.
// ============================================================================
extern "C" uint32_t RtUiDialog_TypeId() {
    return UI::Dialog::TypeId();
}

// ============================================================================
// slot 5  ?Serialize@Dialog@UI@@UEAA_NAEAVStream@IO@@@Z   0x1801130c0
// ============================================================================
//
// TRANSCRIPTION, instruction by instruction (retail 0x1801130c0..0x180113314).
// RSI is `this`; the `IO::Serializer` local is at [RBP-0x18] (.stream) and
// [RBP-0x10] (.readerWriter); the shared int temporary is at [RBP+0x10].
//
//   1801130e3  MOV [RBP-0x18],RDX          ser.stream = &stream
//   1801130e7  MOV [RBP-0x10],0                .readerWriter = null
//   1801130ef  MOV RCX,[RDX+0x10]          encoding = stream->encoding
//   1801130f9  CMP [RDX+0x8],R8D           direction == 0 (reading) ?
//   180113103  CALL [RAX+0x18] / +0x20     CreateReader / CreateWriter(enc,&ser,0)
//   18011310b  MOV [RBP-0x10],RAX
//     ^ the IO::Serializer CONSTRUCTOR, inlined, version = 0. Identical in
//       shape to the one spritesheet_vtable.cpp and
//       elementdescription_serialize.cpp each spell out; constructing it MOVES
//       THE STREAM CURSOR by 12 bytes, which is why the oracle below uses
//       synthetic sinks and never the real stream.
//
//   18011311d  [RAX+0x98](rw, this+0x40, 0)      uint   _defaultFlags
//   18011312b  CALL 0x180038f90 (this+0x38,&ser) ExternalPtr<ElementDescription>
//   180113138  CALL 0x180004590 (this+0x20,&ser) ExternalPtr<SpriteSheet>
//   180113145  CALL 0x1800c9930 (this+0x28,&ser) String  _spriteName
//   180113158  [RAX+0x98](rw, this+0x30, 0)      uint   _backgroundColor
//     ^ +0x98 is rw_vtbl::kScalarUInt and +0xa0 is kScalarInt (RETAIL offsets;
//       the profile build's differ — runtime/types/io/Serializer.h carries the
//       standing warning). RCX is reloaded from [RBP-0x10] for every one of
//       these: the ReaderWriter, NOT the Encoding.
//
//   -- array 1: _namedTemplates, stride 0x10 ------------------------------
//   18011315e  count = [RSI+0x60]  -> the [RBP+0x10] temporary
//   180113172  [RAX+0xa0](rw,&count,0)           int
//   18011317c  if (ser.stream->direction == 0)   READ PATH ONLY
//   180113189      CALL 0x180075d20 (&this[0x58], count)   Array::SetSize
//   18011318e  RBX = [RSI+0x58]                  RE-READ after SetSize
//   1801131a7  loop count:  0x180038f90(RBX,   &ser)   entry+0x00 ExternalPtr
//   1801131b4               0x1800c9930(RBX+8, &ser)   entry+0x08 String
//              RBX += 0x10
//     ^ the data pointer is loaded AFTER SetSize, which reallocates. Hoisting
//       it above the branch would hand the loop a freed block on the read path.
//
//   -- array 2: _templates, stride 0x08 -----------------------------------
//   1801131c2  count = [RSI+0x70]        <- +0x70 IS the count. New fact.
//   1801131d6  [RAX+0xa0](rw,&count,0)
//   1801131e9  if reading: CALL 0x1800492d0 (&this[0x68], count)
//   1801131fd  CALL 0x1800493d0 (&ser, count, [RSI+0x68])   element walker
//
//   -- array 3: _controllerConfigs, stride 0x20 ---------------------------
//   180113202  count = [RSI+0x50]
//   180113216  [RAX+0xa0](rw,&count,0)
//   18011322d  if reading: CALL 0x180075b20 (&this[0x48], count)
//   180113232  RBX = [RSI+0x48]; RBX += 0x10       <- cursor starts at entry+0x10
//   18011325e  loop count: [RAX+0x98](rw, RBX-0x10, 0)  entry+0x00 uint _typeId
//   18011326c              0x180038f90(RBX-0x8, &ser)   entry+0x08 _elementDesc
//   180113278              0x1800756d0(RBX,     &ser)   entry+0x10 _config
//   180113285              0x1800c9930(RBX+8,   &ser)   entry+0x18 _name
//              RBX += 0x20
//     ^ INDEPENDENT CONFIRMATION of the +0x08/+0x10 order Dialog.h pins to the
//       DialogInstance ctor and warns was NAMED BACKWARDS until lane DLG2. The
//       ExternalPtr<ElementDescription> serializer (0x180038f90) is applied to
//       entry+0x08 and the ExternalPtr<DialogControllerConfig> one
//       (0x1800756d0) to entry+0x10 — two different instantiations, so this
//       body could not have them the other way round and still be type-correct.
//
//   -- array 4: _references, stride 0x08 ----------------------------------
//   180113293  count = [RSI+0x80]        <- +0x80 IS the count. New fact.
//   1801132aa  [RAX+0xa0](rw,&count,0)
//   1801132c1  if reading: CALL 0x18002f070 (&this[0x78], count)
//   1801132d1  CALL 0x18002f170 (&ser, count, [RSI+0x78])   element walker
//
//   1801132e4  [RAX+0xa0](rw, this+0x34, 0)     int  _drawLevel   <- LAST
//     ^ and this one is passed the FIELD ITSELF, not a stack copy: the four
//       counts above are each serialised through the [RBP+0x10] temporary
//       (because on the read path SetSize, not the record, is what updates the
//       stored size), but +0x34 is recorded in place.
//
//   1801132eb  if (rw) rw->vtable[0](rw, 1)      inlined Serializer dtor
//   1801132fe  MOV AL,1                          unconditional `true`
//
// ---------------------------------------------------------------------------
// ORACLE
// ---------------------------------------------------------------------------
// the emission oracle in the WRITING direction against two synthetic sinks —
// on the write path a Serialize is a pure read of the object, so ours and
// theirs can both run provided they emit into DIFFERENT sinks
// (reference_write_path_serialize_is_replayable.md).
//
// NO `stopAfter`, and that is a claim about THIS body rather than an omission:
// every one of the four stores into the object is a `SetSize` call, and every
// one of the four is guarded by retail's own `direction == 0` test, so on the
// write path this body writes nothing anywhere. The element walkers and the
// three ExternalPtr serializers touch only their own locals on the write side.
//
// Read the report's `calls=` column and not only `divergences=`: a body with no
// caller reports clean (reference_uncalled_port_is_not_verified.md), and
// `calls=0` has four causes that invert the conclusion. The ATTENDANCE counter
// at the top of this file is the instrument that tells them apart.
// ============================================================================

namespace RtUiDlgV {
namespace {

using rt::io::ReaderWriter;
using rt::io::Serializer;
using rt::io::Stream;

using SerMemberFn = void (*)(void*, Serializer*);

SerMemberFn ByAddr(uintptr_t addr) {
    return reinterpret_cast<SerMemberFn>(ResolveAddr(Module::Runtime, addr));
}

// ?Serialize@String@System@@ — EXPORTED and already ported
// (runtime/src/system/string.cpp registers both doors). Reached by address here
// because that is what the shipped instruction is, and it lands on our port.
SerMemberFn StringSerialize()      { static SerMemberFn f = ByAddr(0x1800c9930); return f; }
// The three ExternalPtr<T>::Serialize instantiations. All three are in lane
// EXTPTR's table of 25; that lane registered four and deliberately left these.
// Registering them HERE would be the duplicate-registration trap it refused.
SerMemberFn ElementDescPtrSerialize() { static SerMemberFn f = ByAddr(0x180038f90); return f; }
SerMemberFn SpriteSheetPtrSerialize() { static SerMemberFn f = ByAddr(0x180004590); return f; }
SerMemberFn ConfigPtrSerialize()      { static SerMemberFn f = ByAddr(0x1800756d0); return f; }

// ENDGAME B.3.2 step 2 — the four by-address SetSize forwarders that used to
// sit here are GONE. All four target fields are the {pointer, count,
// capacity} triples `Dialog.h` already lays out contiguously (and asserts) at
// the SAME offsets `Collection::Array<T>` would use — the header just spells
// them as three named members instead of one `Array<T>` member, the same
// situation `dialog_ctor_dtor.cpp`'s dtor conversion bridges. None of the four
// rows is trait-equivalent, so each call site below bridges through its own
// hand-derived row with `rtcoll::AsArray<&SPEC>`:
//
//   0x180075d20  kSpec_ExtStr10at08  {0x10,8,assign,init,destroy,byteCopy=false}
//                NON-TRIVIAL — Array<DialogNamedTemplate>
//   0x1800492d0  kSpec_U64Zero       {8,8,Assign8,InitZero8,null,true} — the
//                INIT is non-null (new tail slots are ZEROED), which a
//                trait-derived `Array<void*>` would NOT do (a pointer is
//                trivially default-constructible, so the trait leaves the
//                tail UNINITIALISED) — Array<ExternalPtr<ElementDescription>>
//   0x180075b20  kSpec_CfgEntry20    {0x20,8,assign,init,destroy,byteCopy=false}
//                NON-TRIVIAL — Array<DialogControllerConfigEntry>
//   0x18002f070  kSpec_U64Zero       same reasoning as 0x1800492d0 —
//                Array<ExternalPtr<Object::External>>
//
// Every one of the four is the TWO-argument `SetSize(int)` member (no fill).

// (serializer, count, entries)
using EntriesSerializeFn = void (*)(Serializer*, uint32_t, void*);
EntriesSerializeFn TemplatesWalker() {
    static EntriesSerializeFn f = reinterpret_cast<EntriesSerializeFn>(
        ResolveAddr(Module::Runtime, 0x1800493d0)); return f;
}
EntriesSerializeFn ReferencesWalker() {
    static EntriesSerializeFn f = reinterpret_cast<EntriesSerializeFn>(
        ResolveAddr(Module::Runtime, 0x18002f170)); return f;
}

// The inlined ctor at 0x1801130e3..0x18011310b and the inlined dtor at
// 0x1801132eb. Spelled out here rather than shared with the two other files
// that carry an identical pair: they are separate INLINED copies in the DLL,
// and keeping them separate means a correction to one cannot silently change
// the other's meaning.
ReaderWriter* MakeCodec(Stream* stream, Serializer* ser) {
    ser->stream = stream;
    ser->readerWriter = nullptr;

    rt::io::Encoding* enc = stream->encoding;
    const int slot = (stream->direction == rt::io::kDirectionReading)
                         ? rt::io::encoding_vtbl::kCreateReader
                         : rt::io::encoding_vtbl::kCreateWriter;
    using FactoryFn = ReaderWriter* (*)(rt::io::Encoding*, Serializer*, int);
    ReaderWriter* rw =
        reinterpret_cast<FactoryFn>(enc->vtable[slot / 8])(enc, ser, 0);
    ser->readerWriter = rw;
    return rw;
}

void DestroyCodec(ReaderWriter* rw) {
    if (!rw) return;
    using DtorFn = void (*)(ReaderWriter*, int);
    reinterpret_cast<DtorFn>(rw->vtable[rt::io::rw_vtbl::kDtor / 8])(rw, 1);
}

void RecUInt(ReaderWriter* rw, uint32_t* v) {
    using Fn = void (*)(ReaderWriter*, uint32_t*, const char*);
    reinterpret_cast<Fn>(rw->vtable[rt::io::rw_vtbl::kScalarUInt / 8])(rw, v, nullptr);
}

void RecInt(ReaderWriter* rw, int32_t* v) {
    using Fn = void (*)(ReaderWriter*, int32_t*, const char*);
    reinterpret_cast<Fn>(rw->vtable[rt::io::rw_vtbl::kScalarInt / 8])(rw, v, nullptr);
}

const char* const kSerializeMangled =
    "?Serialize@Dialog@UI@@UEAA_NAEAVStream@IO@@@Z";

}  // namespace
}  // namespace RtUiDlgV


// ############################################################################
// ENDGAME B.3.6 block B — the eight slot bodies are UI::Dialog members now
// (runtime/types/ui/Dialog.h). Unchanged except `self->x` became `x` (the
// Serialize offsets are the named fields they always were) and the
// hand-inlined slot-0 call `(*(void***)self)[0](self, 0)` became the virtual
// `DeletingDtor(0)`: it dispatches through the OBJECT's own table exactly as
// the tail jump did, so a derived Dialog still reaches its own slot 0.
// ############################################################################
namespace UI {

int32_t Dialog::GetFileVersion() const { return 6; }           // [3] 0x1801130b0

void Dialog::Destruct() {                                      // [7] 0x1800387f0
    DeletingDtor(0);          // `JMP qword ptr [RAX]` with EDX = 0 — VIRTUAL
}

Object::External* Dialog::Construct() {                        // [8] 0x180038750
    return ::new (this) Dialog();   // the inlined ctor, 180038766..1800387cf
}

Object::External* Dialog::Reconstruct() {                      // [6] 0x1800386b0
    DeletingDtor(0);          // NO null test on this path — retail's
    return ::new (this) Dialog();
}

bool Dialog::Serialize(void* streamv) {                        // [5] 0x1801130c0
    using namespace RtUiDlgV;
    Stream* const stream = static_cast<Stream*>(streamv);

    Serializer ser;
    ReaderWriter* rw = MakeCodec(stream, &ser);

    RecUInt(rw, &_defaultFlags);                               // +0x40
    ElementDescPtrSerialize()(&_rootDescription, &ser);        // +0x38
    SpriteSheetPtrSerialize()(&_spriteSheet, &ser);            // +0x20
    StringSerialize()(&_spriteName, &ser);                     // +0x28
    RecUInt(rw, &_backgroundColor);                            // +0x30

    const bool reading = (ser.stream->direction == rt::io::kDirectionReading);

    // -- array 1: _namedTemplates (+0x58), count +0x60, stride 0x10 ---------
    {
        int32_t count = _namedTemplateCount;   // a STACK COPY, per 0x18011315e
        RecInt(rw, &count);
        if (reading)
            rtcoll::AsArray<&rtcoll::kSpec_ExtStr10at08>(&_namedTemplates)
                ->SetSize(count);          // retail 0x180075d20
        // RE-READ after SetSize: it reallocates.
        uint8_t* e = reinterpret_cast<uint8_t*>(_namedTemplates);
        for (int32_t i = 0; i < count; ++i, e += 0x10) {
            ElementDescPtrSerialize()(e, &ser);             // entry+0x00
            StringSerialize()(e + 8, &ser);                 // entry+0x08
        }
    }

    // -- array 2: _templates (+0x68), count +0x70, stride 0x08 --------------
    {
        int32_t count = _templateCount;
        RecInt(rw, &count);
        if (reading)
            rtcoll::AsArray<&rtcoll::kSpec_U64Zero>(&_templates)
                ->SetSize(count);          // retail 0x1800492d0
        TemplatesWalker()(&ser, static_cast<uint32_t>(count), _templates);
    }

    // -- array 3: _controllerConfigs (+0x48), count +0x50, stride 0x20 ------
    {
        int32_t count = _controllerCount;
        RecInt(rw, &count);
        if (reading)
            rtcoll::AsArray<&rtcoll::kSpec_CfgEntry20>(&_controllerConfigs)
                ->SetSize(count);          // retail 0x180075b20
        uint8_t* e = reinterpret_cast<uint8_t*>(_controllerConfigs);
        for (int32_t i = 0; i < count; ++i, e += 0x20) {
            RecUInt(rw, reinterpret_cast<uint32_t*>(e));    // entry+0x00 _typeId
            ElementDescPtrSerialize()(e + 0x08, &ser);      // entry+0x08 _elementDesc
            ConfigPtrSerialize()(e + 0x10, &ser);           // entry+0x10 _config
            StringSerialize()(e + 0x18, &ser);              // entry+0x18 _name
        }
    }

    // -- array 4: _references (+0x78), count +0x80, stride 0x08 -------------
    {
        int32_t count = _referenceCount;
        RecInt(rw, &count);
        if (reading)
            rtcoll::AsArray<&rtcoll::kSpec_U64Zero>(&_references)
                ->SetSize(count);          // retail 0x18002f070
        ReferencesWalker()(&ser, static_cast<uint32_t>(count), _references);
    }

    RecInt(rw, &_drawLevel);    // +0x34 — the FIELD, not a copy. LAST.

    DestroyCodec(rw);
    return true;
}

// ??1Dialog@UI@@UEAA@XZ 0x180112e40 is in dialog_ctor_dtor.cpp.

}  // namespace UI

// ============================================================================
// Registration — BOTH doors on every row, for the reason the mint itself gives:
// a minted vtable slot is filled by NAME, but 30-odd sites in this tree reach
// exported code by ADDRESS, and a name-only registration leaves those walking
// past the port into the DLL.
//
// Slot 0 (0x180038b20) is NOT registered here — it is row 37 of the family in
// runtime/src/object/deleting_dtor.cpp.
// ============================================================================
int RtUiDialogVtableRegister() {
    int n = 0;
    RegisterRuntimeExport("?TypeId@Dialog@UI@@SAIXZ",
                          (void*)&RtUiDialog_TypeId); ++n;
    RegisterRuntimeAddr(0x180038430, (void*)&RtUiDialog_TypeId);
    return n;
}

// ============================================================================
// ENDGAME B.3.9 TI-UI (B39 D6): UI::Dialog's own type record, built at CRT init
// (head-insert into Object::BaseTypeInfo::_firstTypeInfo). Literals copied from
// the retired minted row ?_typeInfo@Dialog@UI@@ (0x1801faa88): name "Dialog", parent
// "Object::External" (retail ?_ParentTypeName@Dialog@UI@@), no file extension; factories:
// the class's own.
// ============================================================================
DEFINE_TYPE(UI::Dialog, "Object::External", nullptr)
