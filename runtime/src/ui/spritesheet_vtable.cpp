// ============================================================================
// runtime/src/ui/spritesheet_vtable.cpp — WAVE 10 LANE UI4
//   the SpriteSheet virtual table's slot bodies
// ============================================================================
//
// WHY THIS FILE EXISTS, and what the census row it answers actually meant
// ---------------------------------------------------------------------------
// The wave-10 census lists `??_7SpriteSheet@UI@@6B@` as an UNPORTED UI export
// referenced from runtime/src/ui/sprite.cpp. That row is REAL but it is not
// what it looks like, and the difference is the whole of this lane's scoping:
//
//   * The vtable DATA is already minted. Wave 9 lane C2's generator emitted it
//     as `kVSlots45` in standalone/rt_data_mint_tables.inc (9 slots, size proof
//     "tail-not-a-pointer"), and standalone/rt_data_mint.cpp registers it under
//     BOTH doors — `RegisterRuntimeExport("??_7SpriteSheet@UI@@6B@", …)` and
//     `RegisterRuntimeAddr(0x180154590, …)`. The census cannot see either,
//     because both spell the symbol through a STRUCT FIELD rather than a
//     literal (reference_census_cannot_see_nonliteral_registrations.md), and
//     the mint is additionally gated behind `BANISHED_RT_MINT=1` until cutover.
//     So the row is a census artefact twice over, not unwritten data.
//
//   * What IS unwritten is the table's CONTENTS. A minted slot is filled from
//     the port registry at RtDataMintInit() time and is a NAMED TRAP until the
//     body behind it is ported. Before this file, 8 of SpriteSheet's 9 slots
//     were traps — only slot 0 (the deleting destructor, wave 9 lane K) was
//     filled. That is exactly the "8 predicted vtable walls" that
//     docs/runtime_port/WAVE10_PARTITION.md assigns to lane UI4, and it is the
//     lane's real work.
//
// This file closes 7 of those 8. `?Serialize@` is held back, and its blocker
// is named at the bottom of this banner rather than left implicit.
//
// ---------------------------------------------------------------------------
// THE ORACLE
// ---------------------------------------------------------------------------
// Every body here is a pure read or an in-place construct on storage the
// caller owns, so the REPLAY oracle applies without qualification: call ours,
// call the shipped export on the same inputs (a private scratch object for the
// two that write), compare. Nothing on these paths allocates, takes a lock, or
// touches state outside the object — which is what makes replaying safe and is
// why sprite.cpp's argument for scratch replicas does not need to be repeated.
//
// ---------------------------------------------------------------------------
// SLOT 5 IS NOW PORTED — and the recorded blocker was WRONG IN TWO WAYS
// ---------------------------------------------------------------------------
// The paragraph that used to sit here said `?Serialize@SpriteSheet@UI@@` was
// held back because it "drives four calls through the ENCODING vtable at +0xa0
// and one at +0x78", offsets that could not be named from the profile build.
// Wave 10B lane SS re-derived that from the shipped disassembly rather than
// inheriting it, and BOTH halves of the claim are false:
//
//   1. THOSE ARE NOT ENCODING SLOTS. Read the `this` register at each call:
//      `MOV RCX,qword ptr [RSP + 0x30]` (0x18011d793, 0x18011d7a8, 0x18011d7bd,
//      0x18011d7d6) loads the local at RSP+0x30, which is the SERIALIZER's
//      +0x08 field — the `IO::ReaderWriter*` the encoding's factory slot just
//      returned. The Encoding itself is touched ONCE in this whole body, at
//      0x18011d755, to pick +0x18 or +0x20. +0x78 and +0xa0 are ReaderWriter
//      slots, not Encoding slots.
//
//   2. THE MAP ALREADY EXISTED. `rt::io::rw_vtbl` in
//      runtime/types/io/Serializer.h has carried all 26 RETAIL ReaderWriter
//      slot offsets since Wave 4, pinned twice against the shipped bytes
//      (each Serializer forwarder's own tail-jump displacement, AND the
//      contents of ??_7BinaryReader@IO@@6B@ / ??_7BinaryWriter@IO@@6B@). It
//      names +0x78 `kScalarBool` and +0xa0 `kScalarInt`, and its banner
//      already carries the standing warning against the profile build's
//      +0x90/+0xb8. Nothing had to be produced to expire this holdback.
//
// So the holdback was a MISATTRIBUTION (ReaderWriter slots called Encoding
// slots) on top of a search that never reached the file holding the answer.
// Recorded here rather than deleted, per PLAN §6 step 8, so the wrong claim is
// recognisable if it is met again — the same paragraph is repeated in
// WAVE10_LANE_UI4.md §6 and WAVE10_UI_CENSUS.md §6, which are another lane's
// documents and were not edited here.
//
// A THIRD independent confirmation of the same offsets fell out of the port:
// the per-entry helper at 0x180078ad0 drives each SpriteInfo through +0xa0 for
// its five ints and +0x80 for its four floats — and +0x80 is
// `rw_vtbl::kScalarFloat`, so a slot this body never uses itself still lands
// exactly where Wave 4's map puts it.
// ============================================================================

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/io/Serializer.h"
#include "../../types/ui/SpriteSheet.h"
#include "../io/save_trace.h"

// Forward-declared rather than pulled in from standalone/resolve.h, for the
// reason rt_port.h gives: that header drags in <windows.h> and runtime/src/
// must stay platform-neutral. Byte-identical to resolve.h's declaration; the
// same pair already appears in runtime/src/ui/elementdescription_serialize.cpp
// and runtime/src/achievement/goal_serialize.cpp.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace {

// ---- the exported DATA these bodies read -----------------------------------
// `?_typeInfo@SpriteSheet@UI@@0VBaseTypeInfo@Object@@A` is spelled `0V…A`, the
// PRIVATE static form — and this DLL exports its private members, so a name
// door is the correct door and no address door is needed
// (reference_aeaa_is_private_not_unexported.md). Read as a byte pointer at
// +0x00 / +0x08, the same idiom runtime/src/io/package_file.cpp uses.
const uint8_t* SheetTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@SpriteSheet@UI@@0VBaseTypeInfo@Object@@A");
}
void* BaseVtable()     { return RT_DATA_SLOT(void*, "??_7Base@Object@@6B@");     }  // 0x180154788
void* ExternalVtable() { return RT_DATA_SLOT(void*, "??_7External@Object@@6B@"); }  // 0x1801547a0
void* SheetVtable()    { return RT_DATA_SLOT(void*, "??_7SpriteSheet@UI@@6B@");  }  // 0x180154590

typedef uint32_t    (*U32ThisFn)(const void*);
typedef const char* (*CStrThisFn)(const void*);
typedef int         (*IntThisFn)(const void*);
typedef void*       (*PtrThisFn)(void*);

// The in-place construct, byte for byte as 0x180003100 spells it — and NOT a
// call to RtUi_SpriteSheet_Ctor, even though the two are the same instruction
// sequence. They are two separate inlined copies in the DLL; keeping them two
// here means a future correction to one cannot silently change the other's
// meaning, and the oracle checks each against its own export.
//
// The two dead intermediate vtable stores are kept for the reason
// sprite.cpp's constructor already states: they ARE the body, and a debugger
// break between them would observe the intermediate value.
void ConstructInPlace(uint8_t* self) {
    auto qword = [](uint8_t* p, size_t off) -> void*& {
        return *reinterpret_cast<void**>(p + off);
    };
    qword(self, 0x00) = BaseVtable();
    qword(self, 0x00) = ExternalVtable();
    qword(self, 0x08) = nullptr;
    qword(self, 0x10) = nullptr;
    qword(self, 0x18) = nullptr;
    qword(self, 0x00) = SheetVtable();
    qword(self, 0x20) = nullptr;
    qword(self, 0x28) = nullptr;   // m_spriteCount + m_spriteCapacity, one store
    qword(self, 0x30) = nullptr;
    qword(self, 0x38) = nullptr;
    qword(self, 0x40) = nullptr;
    qword(self, 0x48) = nullptr;   // m_field48 + m_field4C
    self[0x50] = 0;                // one byte; +0x51..+0x57 are never stored
}

}  // namespace

// ---- slot 2  ?Type@SpriteSheet@UI@@UEBAIXZ  0x180002e30 --------------------
//   MOV EAX,dword ptr [0x1801fadd0] / RET     — RCX is never read.
extern "C" uint32_t RtUi_SpriteSheet_Type(const void* self) {
    RT_DIFF_BEGIN("UI::SpriteSheet::Type");
    (void)self;
    uint32_t ours = 0;
    std::memcpy(&ours, SheetTypeInfo(), sizeof ours);
    RT_DIFF_SCALAR(ours, RT_ORIGINAL_ONCE(U32ThisFn, "?Type@SpriteSheet@UI@@UEBAIXZ")(self));
    RT_DIFF_WORK(true);
    return ours;
}

// The STATIC spelling of the same two instructions, at its own address
// (0x180002e40). Registered separately because it is a separate export with a
// separate call surface; sharing the body is what the DLL does too.
extern "C" uint32_t RtUi_SpriteSheet_TypeId() {
    RT_DIFF_BEGIN("UI::SpriteSheet::TypeId");
    uint32_t ours = 0;
    std::memcpy(&ours, SheetTypeInfo(), sizeof ours);
    using Fn = uint32_t (*)();
    RT_DIFF_SCALAR(ours, RT_ORIGINAL_ONCE(Fn, "?TypeId@SpriteSheet@UI@@SAIXZ")());
    RT_DIFF_WORK(true);
    return ours;
}

// ---- slot 1  ?TypeName@SpriteSheet@UI@@UEBAPEBDXZ  0x180002e50 -------------
//   MOV RAX,qword ptr [0x1801fadd8] / RET     — typeInfo + 8, RCX unread.
//
// ⚠ THIS ORACLE REPORTS ONE PERMANENT DIVERGENCE, BY DESIGN. READ THIS BEFORE
//   TREATING IT AS A REGRESSION.  (WAVE 10C lane TIDENT)
//
//     [RtDiff] UI::SpriteSheet::TypeName  calls=1 worked=1 divergences=1
//     [RtDiff] DIVERGENCE ... ours=00007ff60aa4612c theirs=00007ffcab5e4578
//
// RT_DIFF_SCALAR below compares the returned `const char*` AS A POINTER.
// `?_typeInfo@SpriteSheet@UI@@` is a PROMOTED minted row, so `SheetTypeInfo()`
// resolves to OUR node, whose `name` is a string literal in our image; the
// shipped export returns the DLL's. Same characters, different object. It is
// the string twin of the "Construct divergence is the vtable identity"
// paragraph in standalone/reg_mint_promoted.cpp, and it went unnamed for two
// waves because the only string screen anywhere was a strcmp.
//
// MEASURED, not argued (2026-09-09, debug, DLL present, under game_dir_lock):
//   promoted ON   UI::SpriteSheet::TypeName  calls=1 divergences=1
//   promoted OFF  UI::SpriteSheet::TypeName  calls=1 divergences=0
//   both halves   IO::PackageFile::TypeName  calls=4 divergences=0
// `calls=1` on BOTH sides, so what moved is the VALUE and not the coverage;
// PackageFile is the same code shape on an UNPROMOTED row and does not move,
// which is what rules out "the oracle is broken".
//
// AND IT IS HARMLESS, measured in the shipped binaries rather than assumed:
// nothing anywhere consumes a BaseTypeInfo::name BY POINTER. In Runtime-x64.dll
// the only static readers of any node's +0x08 are the 103 ?TypeName@ accessors,
// none of which is called anywhere in the DLL; Factory::Startup CRCs the string
// and Factory::GetTypeName has no caller in the DLL and is not imported by the
// exe. In Application-x64.exe all 525 references to the 42 imported
// ?_typeInfo@ slots read the dword at +0x00 and NOT ONE reads +0x08. The full
// census with addresses is docs/runtime_port/WAVE10C_LANE_TIDENT.md.
//
// The comparison is deliberately LEFT AS A POINTER. Relaxing it to strcmp would
// silence the one screen in the tree that can still see identity change.
extern "C" const char* RtUi_SpriteSheet_TypeName(const void* self) {
    RT_DIFF_BEGIN("UI::SpriteSheet::TypeName");
    (void)self;
    const char* ours = nullptr;
    std::memcpy(&ours, SheetTypeInfo() + 8, sizeof ours);
    RT_DIFF_SCALAR(static_cast<const void*>(ours),
                   static_cast<const void*>(
                       RT_ORIGINAL_ONCE(CStrThisFn,
                           "?TypeName@SpriteSheet@UI@@UEBAPEBDXZ")(self)));
    RT_DIFF_WORK(true);
    return ours;
}

// ---- slot 3  ?GetFileVersion@SpriteSheet@UI@@UEBAHXZ  0x18011d710 ----------
//   MOV EAX,6 / RET. The sheet resource's on-disk format version.
extern "C" int RtUi_SpriteSheet_GetFileVersion(const void* self) {
    RT_DIFF_BEGIN("UI::SpriteSheet::GetFileVersion");
    (void)self;
    const int ours = 6;
    RT_DIFF_SCALAR(ours, RT_ORIGINAL_ONCE(IntThisFn,
        "?GetFileVersion@SpriteSheet@UI@@UEBAHXZ")(self));
    RT_DIFF_WORK(true);
    return ours;
}

// ---- slot 4  ?Compile@SpriteSheet@UI@@UEAA…  0x18011d720 -------------------
//   MOV EAX,1 / RET. It IGNORES its Filename argument entirely — the return is
//   an Object::External::CompileResult and 1 is the constant answer. Not a
//   stub of ours: those two instructions are the whole shipped body.
extern "C" int RtUi_SpriteSheet_Compile(void* self, const void* filename) {
    RT_DIFF_BEGIN("UI::SpriteSheet::Compile");
    (void)self; (void)filename;
    const int ours = 1;
    using Fn = int (*)(void*, const void*);
    RT_DIFF_SCALAR(ours, RT_ORIGINAL_ONCE(Fn,
        "?Compile@SpriteSheet@UI@@UEAA?AW4CompileResult@External@Object@@AEBVFilename@System@@@Z")
        (self, filename));
    RT_DIFF_WORK(true);
    return ours;
}

// ---- slot 7  ?Destruct@SpriteSheet@UI@@UEAAXXZ  0x180003180 ----------------
//   MOV RAX,[RCX] / XOR EDX,EDX / JMP qword ptr [RAX]
// A tail jump into SLOT 0 of the object's own vtable — the vector deleting
// destructor — with flag 0, i.e. "destroy, do not free". Reading the vtable
// from the OBJECT and not from a resolved constant is load-bearing: a derived
// SpriteSheet would dispatch to its own slot 0, and hard-coding ours would
// break that silently.
//
// NOT REPLAYED. Its effect is to run a destructor on the live object, and
// calling the shipped body a second time would run it twice — a double free of
// whatever the sheet owns. This is the one body in the file where the replay
// argument fails, and it is stated rather than quietly skipped.
extern "C" void RtUi_SpriteSheet_Destruct(void* self) {
    RT_DIFF_BEGIN("UI::SpriteSheet::Destruct");
    RT_DIFF_WORK(true);
    void** const vt = *reinterpret_cast<void***>(self);
    reinterpret_cast<void (*)(void*, uint32_t)>(vt[0])(self, 0);
}

// ---- slot 8  ?Construct@SpriteSheet@UI@@UEAAPEAVExternal@Object@@XZ  0x180003100
//   if (this) { <the constructor, inlined>; return this; } else return nullptr;
// The null test is retail's own (0x180003114), not a guard added here.
extern "C" void* RtUi_SpriteSheet_Construct(void* self) {
    RT_DIFF_BEGIN("UI::SpriteSheet::Construct");
    if (!self) {
        if (rt_diff_on__) {
            void* theirs = RT_ORIGINAL_ONCE(PtrThisFn,
                "?Construct@SpriteSheet@UI@@UEAAPEAVExternal@Object@@XZ")(nullptr);
            RT_DIFF_SCALAR(static_cast<void*>(nullptr), theirs);
        }
        RT_DIFF_WORK(false);          // the null arm does no work, and says so
        return nullptr;
    }
    ConstructInPlace(static_cast<uint8_t*>(self));

    if (rt_diff_on__) {
        // A PRIVATE scratch object, so the shipped body never writes the live
        // one. Poisoned first, and compared over kSpriteSheetWrittenBytes
        // (0x51) rather than sizeof — +0x51..+0x57 is never stored by either
        // side and comparing it reports allocator residue
        // (reference_oracle_must_not_compare_unwritten_bytes.md; this exact
        // window cost the Wave 7 run a 100% false divergence).
        alignas(16) uint8_t theirs[sizeof(SpriteSheet)];
        std::memset(theirs, 0xCD, sizeof theirs);
        void* const ret = RT_ORIGINAL_ONCE(PtrThisFn,
            "?Construct@SpriteSheet@UI@@UEAAPEAVExternal@Object@@XZ")(theirs);
        RT_DIFF_BYTES(self, theirs, kSpriteSheetWrittenBytes);
        RT_DIFF_SHADOW(ret == static_cast<void*>(theirs),
                       "returns this", "returns this");
        RT_DIFF_WORK(true);
    }
    return self;
}

// ---- slot 6  ?Reconstruct@SpriteSheet@UI@@UEAAPEAVExternal@Object@@XZ  0x180003080
//   Destruct-then-Construct on the SAME storage, and note the difference from
//   Construct: there is NO null test on this path (0x180003092 dereferences
//   RCX immediately). Retail relies on the caller never passing null, and
//   CLAUDE.md's rule says leave the deref faithful.
//
// NOT REPLAYED, for Destruct's reason: the destructor half runs on the live
// object and cannot be run twice.
extern "C" void* RtUi_SpriteSheet_Reconstruct(void* self) {
    RT_DIFF_BEGIN("UI::SpriteSheet::Reconstruct");
    RT_DIFF_WORK(true);
    void** const vt = *reinterpret_cast<void***>(self);
    reinterpret_cast<void (*)(void*, uint32_t)>(vt[0])(self, 0);
    ConstructInPlace(static_cast<uint8_t*>(self));
    return self;
}

// ============================================================================
// slot 5  ?Serialize@SpriteSheet@UI@@UEAA_NAEAVStream@IO@@@Z   0x18011d730
// ============================================================================
//
// TRANSCRIPTION, instruction by instruction (retail 0x18011d730..0x18011d845):
//
//   18011d744  MOV RBX,RCX                  this
//   18011d747  MOV [RSP+0x28],RDX           IO::Serializer local: .stream  = &stream
//   18011d74c  MOV [RSP+0x30],0                                   .readerWriter = null
//   18011d755  MOV RCX,[RDX+0x10]           encoding = stream->encoding
//   18011d75f  CMP [RDX+0x8],R8D            direction == 0 ?
//   18011d76a  CALL [RAX+0x18]  / +0x20     CreateReader / CreateWriter (enc, &ser, 0)
//   18011d772  MOV [RSP+0x30],RAX           ser.readerWriter = rw
//     ^ this is the IO::Serializer CONSTRUCTOR, inlined, with version = 0.
//       It is NOT a call to ??0Serializer@IO@@ — MSVC inlined it here, and the
//       shape is identical to the one elementdescription_serialize.cpp calls
//       MakeCodec(). Constructing it MOVES THE STREAM CURSOR by 12 bytes
//       (runtime/types/io/Serializer.h), which is why this body is not
//       replayable against the real stream and why the oracle below uses
//       synthetic sinks instead.
//
//   18011d780  CALL 0x1800c9930  (RBX+0x40) String::Serialize  m_groupName    <- FIRST
//   18011d78e  CALL 0x1800c9930  (RBX+0x38) String::Serialize  m_textureName  <- SECOND
//     ^ the ORDER IS +0x40 THEN +0x38, i.e. the two strings come off the wire
//       in the opposite order to their addresses. Getting this backwards would
//       not crash: it would swap two String records of usually-different length
//       and desync every subsequent record. Transcribed from the LEA operands,
//       not from field order.
//
//   18011d7a2  CALL [RAX+0xa0]   (RBX+0x48, null) int   m_field48
//   18011d7b7  CALL [RAX+0xa0]   (RBX+0x4c, null) int   m_field4C
//   18011d7cc  CALL [RAX+0x78]   (RBX+0x50, null) bool  m_field50
//     ^ RCX for all three is reloaded from [RSP+0x30] each time — the
//       ReaderWriter, not the Encoding. See the banner.
//
//   18011d7cf  MOV EAX,[RBX+0x28] / MOV [RSP+0x50],EAX   count = m_spriteCount
//   18011d7e6  CALL [RAX+0xa0]   (&count, null)          int count
//     ^ a STACK COPY is serialised, never the field. On the read path the
//       count arrives here and m_spriteCount is updated by SetSize below, not
//       by this record.
//
//   18011d7ec  MOV RAX,[RSP+0x28] / CMP [RAX+0x8],0 / JNZ
//   18011d7ff  CALL 0x180078950  (&m_sprites, count)   Array<SpriteInfo>::SetSize
//     ^ READ PATH ONLY. Third argument is absent; retail passes two.
//
//   18011d804  MOV R8,[RBX+0x20]                        RE-READ after SetSize
//   18011d811  CALL 0x180078ad0  (&ser, count, data)    per-entry serialize
//     ^ the data pointer is loaded AFTER SetSize, which reallocates. Hoisting
//       that load above the branch would hand the helper a freed block on the
//       read path.
//
//   18011d81f  CALL 0x180014520  (RBX+0x30, &ser)       ExternalPtr<T>::Serialize
//   18011d825  if (rw) rw->vtable[0](rw, 1)             inlined Serializer dtor
//   18011d839  MOV AL,1                                 unconditional `true`
//
// ---------------------------------------------------------------------------
// THREE ADDRESS DOORS, AND WHAT EACH ONE COSTS
// ---------------------------------------------------------------------------
// Four callees; one is exported and ported, three are unexported internals
// reached BY ADDRESS through ResolveAddr(Module::Runtime, …). That door
// consults the port registry FIRST
// (reference_resolveaddr_consults_port_registry_first.md), so each becomes our
// body the moment somebody registers it, and is the DLL's until then. It also
// GROWS the cutover surface by three
// (reference_porting_can_grow_the_cutover_surface.md), which is stated in the
// lane doc rather than left for a later census to discover:
//
//   0x1800c9930  ?Serialize@String@System@@QEAAXAEAVSerializer@IO@@@Z
//                EXPORTED and already ported — runtime/src/system/string.cpp
//                registers both doors. Reached by address here because that is
//                what the shipped instruction is, and it lands on our port.
//
//   0x180078950  Array<UI::SpriteSheet::SpriteInfo>::SetSize, stride 0x38.
//                UNPORTED. It calls System::MemoryAllocator::MemAlloc/Free, so
//                it is squarely inside the class-B allocator family and is NOT
//                this lane's row. Same shape as the fourteen kSpec_* rows in
//                runtime/src/collection/array.cpp and as lane UI4's recorded
//                0x18004bde0 (stride 0x28) — a one-row follow-on for the
//                collection family.
//
//   0x180078ad0  the per-SpriteInfo record walker (name, group, x, y, width,
//                height, reserved, u0, v0, u1, v1). UNPORTED, pure, no
//                allocation of its own beyond String::Serialize's. Cheap to
//                port later; deliberately not taken here because it is a
//                second body and this lane's promotion gate needs one.
//
//   0x180014520  ExternalPtr<Graphics::MaterialInstance>::Serialize — one of
//                the twenty-five instantiations lane EXTPTR measured and
//                tabulated (WAVE10_LANE_EXTPTR.md:79). Lane EXTPTR registered
//                four of the twenty-five and deliberately left the Graphics
//                ones alone; registering it here would be exactly the
//                duplicate-registration trap that lane refused, so it stays
//                the DLL's. THIS ALSO NAMES SpriteSheet+0x30: it is not an
//                anonymous qword, it is the sheet's MaterialInstance handle.
//
// ---------------------------------------------------------------------------
// ORACLE
// ---------------------------------------------------------------------------
// `RtSaveTrace::Compare` in the WRITING direction against two synthetic sinks,
// the same instrument elementdescription_serialize.cpp uses and for the same
// reason: on the write path a Serialize is a pure read of the object, so ours
// and theirs can both run provided they emit into DIFFERENT sinks
// (reference_write_path_serialize_is_replayable.md). No `stopAfter`: nothing on
// this body's write path stores into the object — SetSize is guarded to the
// reading side by retail's own branch, and the ExternalPtr write arm touches
// only its own locals.
//
// Read the report's `calls=` column and not only `divergences=`: a body with no
// caller reports clean (reference_uncalled_port_is_not_verified.md), and until
// `??_7SpriteSheet@UI@@6B@` is promoted this port genuinely has no caller —
// which is the whole point of the promotion this lane exists to make.
// ============================================================================

namespace {

using rt::io::ReaderWriter;
using rt::io::Serializer;
using rt::io::Stream;

using SerMemberFn    = void (*)(void*, Serializer*);
using StreamMemberFn = void (*)(void*, Stream*);

SerMemberFn ByAddr(uintptr_t addr) {
    return reinterpret_cast<SerMemberFn>(ResolveAddr(Module::Runtime, addr));
}

// ?Serialize@String@System@@ — exported, ported, reached by address because the
// shipped instruction is `CALL 0x1800c9930`.
SerMemberFn StringSerialize() {
    static SerMemberFn f = ByAddr(0x1800c9930);
    return f;
}
// Array<SpriteInfo>::SetSize(array, newCount) — two arguments, not a member of
// the SerMemberFn family, so it gets its own type.
using ArraySetSizeFn = void (*)(void*, uint32_t);
ArraySetSizeFn ArraySetSize() {
    static ArraySetSizeFn f = reinterpret_cast<ArraySetSizeFn>(
        ResolveAddr(Module::Runtime, 0x180078950));
    return f;
}
// (serializer, count, entries)
using EntriesSerializeFn = void (*)(Serializer*, uint32_t, void*);
EntriesSerializeFn EntriesSerialize() {
    static EntriesSerializeFn f = reinterpret_cast<EntriesSerializeFn>(
        ResolveAddr(Module::Runtime, 0x180078ad0));
    return f;
}
SerMemberFn MaterialPtrSerialize() {
    static SerMemberFn f = ByAddr(0x180014520);
    return f;
}

// The inlined ctor at 0x18011d747..0x18011d772, and the inlined dtor at
// 0x18011d825. Deliberately spelled out here rather than shared with
// elementdescription_serialize.cpp's identical pair: they are two separate
// INLINED copies in the DLL, and keeping them two means a correction to one
// cannot silently change the other's meaning — the same argument this file's
// ConstructInPlace already makes.
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

void RecInt(ReaderWriter* rw, int32_t* v) {
    using Fn = void (*)(ReaderWriter*, int32_t*, const char*);
    reinterpret_cast<Fn>(rw->vtable[rt::io::rw_vtbl::kScalarInt / 8])(rw, v, nullptr);
}

void RecBool(ReaderWriter* rw, uint8_t* v) {
    using Fn = void (*)(ReaderWriter*, uint8_t*, const char*);
    reinterpret_cast<Fn>(rw->vtable[rt::io::rw_vtbl::kScalarBool / 8])(rw, v, nullptr);
}

// WAVE 29 LANE WALL — the `…Via` pair reload `ser->readerWriter` AND its
// vtable pointer on EVERY call, which is what 0x180078ad0 does and is NOT what
// `RecInt`/`RecBool` above do (those take an already-loaded `rw` because their
// caller holds one across a straight run of calls with nothing in between).
// The difference is deliberate and is argued in the banner on
// RtUi_SpriteSheet_SerializeEntries: in that loop `String::Serialize` runs
// between the reload and the scalar calls.
void RecIntVia(Serializer* ser, int32_t* v) {
    ReaderWriter* rw = ser->readerWriter;
    using Fn = void (*)(ReaderWriter*, int32_t*, const char*);
    reinterpret_cast<Fn>(rw->vtable[rt::io::rw_vtbl::kScalarInt / 8])(rw, v, nullptr);
}

void RecFloatVia(Serializer* ser, float* v) {
    ReaderWriter* rw = ser->readerWriter;
    using Fn = void (*)(ReaderWriter*, float*, const char*);
    reinterpret_cast<Fn>(rw->vtable[rt::io::rw_vtbl::kScalarFloat / 8])(rw, v, nullptr);
}

// ---- probe scaffolding (same contract as elementdescription_serialize.cpp) --
Stream* StreamOf(void* serializer) {
    return static_cast<Serializer*>(serializer)->stream;
}

bool g_probing = false;
struct ProbeGuard {
    ProbeGuard()  { g_probing = true;  }
    ~ProbeGuard() { g_probing = false; }
};

const char* const kSerializeMangled =
    "?Serialize@SpriteSheet@UI@@UEAA_NAEAVStream@IO@@@Z";

// `Compare` defends against `ours == theirs` by comparing function pointers,
// and the lambda adapters below defeat that check — so the equivalent check is
// made on the thing that matters: does the registry hand OUR body back?
bool PortIsLive(const void* ours) {
    void* viaRegistry = ResolveExport(kSerializeMangled);
    if (viaRegistry == ours) return true;
    std::fprintf(stderr,
                 "[RtSaveTrace] FAIL %s: the port is NOT registered (registry -> %p, "
                 "ours %p), so this comparison would prove nothing\n",
                 kSerializeMangled, viaRegistry, ours);
    return false;
}

// No detour is installed on this row, so the RAW export address IS retail's
// body. (If one is ever added, this must become a trampoline lookup or
// RT_ORIGINAL_ONCE will recurse into the patched prologue —
// reference_detour_machinery_exists_wave7_zero_is_stale.md.)
StreamMemberFn RetailBody() {
    return RT_ORIGINAL_ONCE(StreamMemberFn, kSerializeMangled);
}

}  // namespace

// ============================================================================
// WAVE 29 LANE WALL — `0x180078ad0`, the per-SpriteInfo record walker.
//
// THIS IS THE FUNCTION CUTOVER ATTEMPT 31 DIED ON, and it died here for a
// reason that had nothing to do with SpriteSheet. `EntriesSerialize()` above
// is an ADDRESS DOOR onto an unported, unexported DLL internal. With the DLL
// PRESENT `ResolveAddr(Module::Runtime, 0x180078ad0)` hands back retail's body
// and everything works; with the DLL ABSENT it hands back nothing and the
// `CALL RAX` that follows is an EXEC of 0
// ([[reference_by_address_door_dies_at_cutover]]).
//
// THE OFFSET IN THE CRASH LOG NAMES THE DOOR, AND NOTHING ELSE DOES. The
// attempt-31 return address was `RtUi_SpriteSheet_Serialize+0x1b7`, i.e. the
// instruction after `CALL *%RAX` at 0x1403d7ca8, whose RAX came from
// `EntriesSerialize()` at +0x198 — with RAX=0 in the register dump. There are
// FOUR address doors in this body and reading them in source order would have
// fingered `StringSerialize()` instead, which is registered and fine.
//
// ---------------------------------------------------------------------------
// EXTENT, DERIVED FROM THE INSTRUCTION STREAM (not `.pdata` — warning 16)
// ---------------------------------------------------------------------------
// 0x180078ad0 .. 0x180078bd8, i.e. through the `REPZ RET` at 0x180078bd7,
// followed by seven INT3 and then an unrelated `XOR EAX,EAX / MOV [RCX],RAX`
// body at 0x180078be0. 0x109 bytes, one loop, no SEH, and no allocation of
// its own beyond whatever `String::Serialize` does.
//
// ---------------------------------------------------------------------------
// SIGNATURE AND BODY — objdump of the shipped Runtime-x64.dll
// ---------------------------------------------------------------------------
//   (RCX = Serializer*, EDX = count, R8 = SpriteInfo* entries)
//
//   TEST EDX,EDX ; JLE  -> a count of zero or less does NOTHING, including no
//                          virtual call. `count` is read SIGNED even though
//                          the caller's typedef spells it `uint32_t`; the bits
//                          in EDX are the same either way.
//   RBX = entries + 0x10, and the loop is a DO-WHILE on `DEC RSI`, stride 0x38.
//
//   per entry:
//     CALL 0x1800c9930   (RBX-0x10, ser)   String::Serialize  name   +0x00
//     CALL 0x1800c9930   (RBX-0x08, ser)   String::Serialize  group  +0x08
//     vtable +0xa0 (kScalarInt)   RBX+0x00   -> x           +0x10
//     vtable +0xa0                RBX+0x04   -> y           +0x14
//     vtable +0xa0                RBX+0x0c   -> width       +0x1c
//     vtable +0xa0                RBX+0x10   -> height      +0x20
//     vtable +0xa0                RBX+0x08   -> _reserved18 +0x18   <-- LAST
//     vtable +0x80 (kScalarFloat) RBX+0x14   -> u0          +0x24
//     vtable +0x80                RBX+0x18   -> v0          +0x28
//     vtable +0x80                RBX+0x1c   -> u1          +0x2c
//     vtable +0x80                RBX+0x20   -> v1          +0x30
//
// THE FIELD AT +0x18 IS SERIALIZED FIFTH OF THE FIVE INTS, NOT THIRD. The
// record is walked in AddSpriteInfo's PARAMETER order — x, y, width, height —
// with the always-zero `_reserved18` appended at the end, which is why the
// stream order is not the declaration order. A loop written "in field order"
// emits the same eleven values in the wrong SEQUENCE and desynchronises every
// following record in the file: the failure is not a crash, it is a corrupt
// sprite atlas, and nothing in this tree would report it. The order here is
// transcribed from the displacement on each LEA, one at a time.
//
// It agrees with the field list this file's own door table at the top already
// recorded — "name, group, x, y, width, height, reserved, u0, v0, u1, v1" —
// an annotation that HELD when it was finally checked. Recorded because the
// standing expectation in this project is the opposite
// ([[reference_pdata_understates_by_24x_and_annotations_lie]]) and a warning
// that is always right is a warning nobody re-checks.
//
// `+0x34` IS NOT SERIALIZED. There is no eleventh scalar call. That agrees
// with SpriteSheet.h's Wave 7 correction, which proved from the element
// copy-construct helper that +0x34 is never written by either implementation.
//
// RETAIL RELOADS `ser->readerWriter` AND ITS VTABLE POINTER BEFORE EVERY ONE
// of the nine scalar calls (`MOV RCX,[RDI+8] ; MOV RAX,[RCX]`), rather than
// hoisting either out of the loop. Reproduced through `RecIntVia`/
// `RecFloatVia`, because `String::Serialize` runs between the reload and the
// first scalar call and a hoist would be a silent behavioural change if it
// ever replaced the codec.
// ---------------------------------------------------------------------------
extern "C" void RtUi_SpriteSheet_SerializeEntries(Serializer* ser,
                                                  int32_t     count,
                                                  SpriteInfo* entries) {
    if (count <= 0) return;                 // 0x180078ad0  TEST EDX,EDX ; JLE

    SpriteInfo* e = entries;
    do {
        StringSerialize()(&e->name,  ser);  // +0x00  CALL 0x1800c9930
        StringSerialize()(&e->group, ser);  // +0x08  CALL 0x1800c9930

        RecIntVia(ser, &e->x);              // +0x10
        RecIntVia(ser, &e->y);              // +0x14
        RecIntVia(ser, &e->width);          // +0x1c
        RecIntVia(ser, &e->height);         // +0x20
        RecIntVia(ser, &e->_reserved18);    // +0x18  FIFTH — see the banner

        RecFloatVia(ser, &e->u0);           // +0x24
        RecFloatVia(ser, &e->v0);           // +0x28
        RecFloatVia(ser, &e->u1);           // +0x2c
        RecFloatVia(ser, &e->v1);           // +0x30

        ++e;                                // ADD RBX,0x38
    } while (--count != 0);                 // DEC RSI ; JNE
}

extern "C" bool RtUi_SpriteSheet_Serialize(SpriteSheet* self, Stream* stream) {
    static int probes = 0;
    if (RtSaveTrace::Enabled() && !g_probing && probes < 16) {
        ++probes;
        ProbeGuard guard__;
        StreamMemberFn theirs = RetailBody();
        if (theirs && PortIsLive((const void*)&RtUi_SpriteSheet_Serialize)) {
            RtSaveTrace::Compare(
                "UI::SpriteSheet::Serialize", self,
                [](void* obj, void* ser) {
                    RtUi_SpriteSheet_Serialize(static_cast<SpriteSheet*>(obj),
                                               StreamOf(ser));
                },
                [](void* obj, void* ser) { RetailBody()(obj, StreamOf(ser)); });
        }
    }

    Serializer ser;
    ReaderWriter* rw = MakeCodec(stream, &ser);

    StringSerialize()(&self->m_groupName,   &ser);   // +0x40 FIRST — see banner
    StringSerialize()(&self->m_textureName, &ser);   // +0x38 second

    RecInt (rw, &self->m_field48);   // +0x48
    RecInt (rw, &self->m_field4C);   // +0x4c
    RecBool(rw, &self->m_field50);   // +0x50

    int32_t count = self->m_spriteCount;   // +0x28, into the RSP+0x50 temporary
    RecInt(rw, &count);

    if (ser.stream->direction == rt::io::kDirectionReading)
        ArraySetSize()(&self->m_sprites, static_cast<uint32_t>(count));

    // Re-read AFTER SetSize: it reallocates.
    EntriesSerialize()(&ser, static_cast<uint32_t>(count), self->m_sprites);

    MaterialPtrSerialize()(&self->m_field30, &ser);  // +0x30

    DestroyCodec(rw);
    return true;
}

// ============================================================================
// Registration — called from RtUiSpriteCaptureRegister(), under the same
// BANISHED_RT_UISPRITECAP switch, because the two halves of lane UI4 are
// verified by the same run and an A/B that could split them would be a third
// state nobody wants to reason about.
// ============================================================================
int RtUiSpriteSheetVtableRegister() {
    int n = 0;
    RegisterRuntimeExport("?Type@SpriteSheet@UI@@UEBAIXZ",
                          (void*)&RtUi_SpriteSheet_Type); ++n;
    RegisterRuntimeAddr(0x180002e30, (void*)&RtUi_SpriteSheet_Type);
    RegisterRuntimeExport("?TypeId@SpriteSheet@UI@@SAIXZ",
                          (void*)&RtUi_SpriteSheet_TypeId); ++n;
    RegisterRuntimeAddr(0x180002e40, (void*)&RtUi_SpriteSheet_TypeId);
    RegisterRuntimeExport("?TypeName@SpriteSheet@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_SpriteSheet_TypeName); ++n;
    RegisterRuntimeAddr(0x180002e50, (void*)&RtUi_SpriteSheet_TypeName);
    RegisterRuntimeExport("?GetFileVersion@SpriteSheet@UI@@UEBAHXZ",
                          (void*)&RtUi_SpriteSheet_GetFileVersion); ++n;
    RegisterRuntimeAddr(0x18011d710, (void*)&RtUi_SpriteSheet_GetFileVersion);
    RegisterRuntimeExport(
        "?Compile@SpriteSheet@UI@@UEAA?AW4CompileResult@External@Object@@AEBVFilename@System@@@Z",
        (void*)&RtUi_SpriteSheet_Compile); ++n;
    RegisterRuntimeAddr(0x18011d720, (void*)&RtUi_SpriteSheet_Compile);
    RegisterRuntimeExport("?Destruct@SpriteSheet@UI@@UEAAXXZ",
                          (void*)&RtUi_SpriteSheet_Destruct); ++n;
    RegisterRuntimeAddr(0x180003180, (void*)&RtUi_SpriteSheet_Destruct);
    RegisterRuntimeExport("?Construct@SpriteSheet@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_SpriteSheet_Construct); ++n;
    RegisterRuntimeAddr(0x180003100, (void*)&RtUi_SpriteSheet_Construct);
    RegisterRuntimeExport("?Reconstruct@SpriteSheet@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_SpriteSheet_Reconstruct); ++n;
    RegisterRuntimeAddr(0x180003080, (void*)&RtUi_SpriteSheet_Reconstruct);
    // WAVE 10B lane SS — the ninth slot, and the one that was keeping
    // ??_7SpriteSheet@UI@@6B@ out of standalone/reg_mint_promoted.cpp.
    RegisterRuntimeExport(kSerializeMangled, (void*)&RtUi_SpriteSheet_Serialize); ++n;
    RegisterRuntimeAddr(0x18011d730, (void*)&RtUi_SpriteSheet_Serialize);
    // BOTH doors on every row, for the reason the mint itself gives: a minted
    // vtable slot is filled by NAME, but 30-odd sites in this tree reach
    // exported code by ADDRESS, and a name-only registration leaves those
    // walking past the port into the DLL.

    // ---- WAVE 29 LANE WALL -------------------------------------------------
    //
    // 0x180078ad0 — the per-SpriteInfo record walker, and the EXEC-of-0 that
    // cutover attempt 31 died on. ADDRESS ONLY: it is an unexported internal,
    // so there is no name to register and the address door is the only door.
    //
    // GREPPED BEFORE WRITING, across runtime/, standalone/, code/ and engine/:
    // 0x180078ad0 appears at exactly four sites, three of them comments in
    // this file and the fourth the `EntriesSerialize()` door at :469. Nothing
    // registers it, so this shadows no existing binding
    // ([[reference_grep_for_the_port_before_writing_it]]).
    //
    // This registration is what turns the probe at the top of
    // `RtUi_SpriteSheet_Serialize` into an oracle for the new body: retail's
    // outer body reaches 0x180078ad0 by an in-DLL `rel32` CALL, which is not a
    // door and which we therefore do NOT intercept — so the `theirs` side
    // keeps running retail's copy while `ours` now runs this file
    // ([[reference_registering_an_address_converts_a_selfcheck_into_an_oracle]]).
    // Before this line the probe compared retail's inner body with itself.
    RegisterRuntimeAddr(0x180078ad0, (void*)&RtUi_SpriteSheet_SerializeEntries); ++n;
    return n;
}
