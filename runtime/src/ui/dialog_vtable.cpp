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
// They are deliberately NOT RtDiff `calls` numbers: an RtDiff site increments
// `calls` only from inside a comparison, so a body that ran and always took the
// uncompared branch prints `calls=0`, which reads as "never ran" and means the
// opposite (reference_calls_zero_can_mean_always_took_the_uncovered_branch.md).
//
// KILL SWITCH: BANISHED_RT_DLGVTABLE=0 leaves all seven on the DLL. Its own
// switch rather than lane B's BANISHED_RT_UIDIALOG, because promoting the
// vtable row is what makes these dispatch at all and the A/B behind that
// promotion has to be able to move exactly this group.
// ============================================================================

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/io/Serializer.h"
#include "../../types/ui/Dialog.h"
#include "../io/save_trace.h"

// Forward-declared rather than pulled in from standalone/resolve.h, for the
// reason rt_port.h gives: that header drags in <windows.h> and runtime/src/
// must stay platform-neutral. Byte-identical to resolve.h's declaration; the
// same pair appears in spritesheet_vtable.cpp and elementdescription_serialize.cpp.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace RtUiDlgV {

namespace {

// ---------------------------------------------------------------------------
// ATTENDANCE COUNTERS — the EFFECTIVENESS CONTROL for the promotion A/B, and
// NOT a verification of anything. See the banner.
// ---------------------------------------------------------------------------
struct DlgCounts {
    unsigned long long typeName = 0, type = 0, typeId = 0, fileVersion = 0,
                       serialize = 0, reconstruct = 0, destruct = 0,
                       construct = 0, constructNull = 0;
    ~DlgCounts() {
        const unsigned long long total = typeName + type + typeId + fileVersion +
                                         serialize + reconstruct + destruct +
                                         construct + constructNull;
        if (total == 0) {
            printf("[DlgVtbl] ATTENDANCE: none of the seven bodies was entered "
                   "in this run.\n");
            return;
        }
        printf("[DlgVtbl] ATTENDANCE (entries, not comparisons): total=%llu "
               "TypeName=%llu Type=%llu TypeId=%llu GetFileVersion=%llu "
               "Serialize=%llu Reconstruct=%llu Destruct=%llu Construct=%llu "
               "ConstructNull=%llu\n",
               total, typeName, type, typeId, fileVersion, serialize,
               reconstruct, destruct, construct, constructNull);
    }
};
DlgCounts g_counts;

// ---- the exported DATA these bodies read -----------------------------------
// `?_typeInfo@Dialog@UI@@0VBaseTypeInfo@Object@@A` is spelled `0V…A`, the
// PRIVATE static form — and this DLL exports its private members, so a name
// door is the correct door and no address door is needed
// (reference_aeaa_is_private_not_unexported.md).
const uint8_t* DialogTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@Dialog@UI@@0VBaseTypeInfo@Object@@A");
}
void* BaseVtable()     { return RT_DATA_SLOT(void*, "??_7Base@Object@@6B@");     }  // 0x180154788
void* ExternalVtable() { return RT_DATA_SLOT(void*, "??_7External@Object@@6B@"); }  // 0x1801547a0
void* DialogVtable()   { return RT_DATA_SLOT(void*, "??_7Dialog@UI@@6B@");       }  // 0x1801562a0

typedef uint32_t    (*U32ThisFn)(const void*);
typedef const char* (*CStrThisFn)(const void*);
typedef int         (*IntThisFn)(const void*);
typedef void*       (*PtrThisFn)(void*);

// `Object::BaseTypeInfo` (runtime/types/object/TypeInfo.h): +0x00 typeId
// (uint32), +0x08 `const char* name`. Spelled as constants for the reason
// dialog_controller_base.cpp spells them.
constexpr int kBaseTypeInfoTypeId = 0x00;
constexpr int kBaseTypeInfoName   = 0x08;

// ---------------------------------------------------------------------------
// The in-place construct, byte for byte as 0x180038750 spells it.
//
// It is instruction-for-instruction identical to `??0Dialog@UI@@QEAA@XZ`
// (0x180112db0), which lane DLG2 already ported as RtUiDialog_Ctor — and this
// is deliberately NOT a call to that port. They are two separate INLINED copies
// in the DLL, and keeping them two here means a future correction to one cannot
// silently change the other's meaning. That is lane SS's argument for the same
// pair one class over, and it applies unchanged.
//
// The two dead intermediate vtable stores at +0x00 (Base, then External, then
// Dialog) are kept: they ARE the body, and a debugger break between them would
// observe the intermediate value.
//
// TWO NON-ZERO / NON-QWORD STORES, both of which a copied SpriteSheet body
// would have got wrong — see banner difference (1) and (2):
//     +0x30  DWORD 0xFFFFFFFF     (not zero)
//     +0x34  DWORD 0
//     +0x40  DWORD 0              (+0x44..0x47 NEVER WRITTEN by retail)
// ---------------------------------------------------------------------------
void ConstructInPlace(uint8_t* self) {
    auto qword = [](uint8_t* p, size_t off) -> void*& {
        return *reinterpret_cast<void**>(p + off);
    };
    auto dword = [](uint8_t* p, size_t off) -> uint32_t& {
        return *reinterpret_cast<uint32_t*>(p + off);
    };
    qword(self, 0x00) = BaseVtable();       // 180038766
    qword(self, 0x00) = ExternalVtable();   // 180038770
    qword(self, 0x08) = nullptr;            // 180038785 (via LEA RAX,[RCX+8])
    qword(self, 0x10) = nullptr;
    qword(self, 0x18) = nullptr;
    qword(self, 0x00) = DialogVtable();     // 180038790
    qword(self, 0x20) = nullptr;            // _spriteSheet
    qword(self, 0x28) = nullptr;            // _spriteName
    dword(self, 0x30) = 0xFFFFFFFFu;        // 1800387a2  _backgroundColor
    dword(self, 0x34) = 0;                  // 1800387a9  _drawLevel
    qword(self, 0x38) = nullptr;            // _rootDescription
    dword(self, 0x40) = 0;                  // 1800387b0  DWORD only; +0x44 untouched
    qword(self, 0x48) = nullptr;
    qword(self, 0x50) = nullptr;
    qword(self, 0x58) = nullptr;
    qword(self, 0x60) = nullptr;
    qword(self, 0x68) = nullptr;
    qword(self, 0x70) = nullptr;
    qword(self, 0x78) = nullptr;
    qword(self, 0x80) = nullptr;            // 1800387cf  the last store
}

// The two windows `Construct`'s oracle may compare: everything retail writes,
// and nothing it does not. +0x44..+0x47 is the one four-byte hole
// (reference_oracle_must_not_compare_unwritten_bytes.md — this exact class of
// window cost the Wave 7 run a 100% false divergence).
constexpr size_t kWrittenLo   = 0x00;
constexpr size_t kWrittenLoEnd = 0x44;
constexpr size_t kWrittenHi   = 0x48;
constexpr size_t kWrittenHiEnd = 0x88;

}  // namespace
}  // namespace RtUiDlgV

// ---- slot 2  ?Type@Dialog@UI@@UEBAIXZ  0x180038420 -------------------------
//   MOV EAX,dword ptr [0x1801faa88] / RET     — RCX is never read.
// SAME SHAPE as SpriteSheet's slot 2 (0x180002e30): two instructions, the
// class's own `_typeInfo` at +0x00.
extern "C" uint32_t RtUiDialog_Type(const void* self) {
    ++RtUiDlgV::g_counts.type;
    RT_DIFF_BEGIN("UI::Dialog::Type");
    (void)self;
    uint32_t ours = 0;
    std::memcpy(&ours, RtUiDlgV::DialogTypeInfo() + RtUiDlgV::kBaseTypeInfoTypeId,
                sizeof ours);
    RT_DIFF_SCALAR(ours, RT_ORIGINAL_ONCE(RtUiDlgV::U32ThisFn,
        "?Type@Dialog@UI@@UEBAIXZ")(self));
    RT_DIFF_WORK(true);
    return ours;
}

// The STATIC spelling of the same two instructions, at its own address
// (0x180038430, `?TypeId@Dialog@UI@@SAIXZ`). NOT a vtable slot — it is a free
// symbol this lane closes because it is the same two instructions and because
// leaving it on the DLL while slot 2 is ours is a split nobody wants to reason
// about. Lane SS did the same for `?TypeId@SpriteSheet@UI@@SAIXZ`.
extern "C" uint32_t RtUiDialog_TypeId() {
    ++RtUiDlgV::g_counts.typeId;
    RT_DIFF_BEGIN("UI::Dialog::TypeId");
    uint32_t ours = 0;
    std::memcpy(&ours, RtUiDlgV::DialogTypeInfo() + RtUiDlgV::kBaseTypeInfoTypeId,
                sizeof ours);
    using Fn = uint32_t (*)();
    RT_DIFF_SCALAR(ours, RT_ORIGINAL_ONCE(Fn, "?TypeId@Dialog@UI@@SAIXZ")());
    RT_DIFF_WORK(true);
    return ours;
}

// ---- slot 1  ?TypeName@Dialog@UI@@UEBAPEBDXZ  0x180038440 ------------------
//   MOV RAX,qword ptr [0x1801faa90] / RET     — `_typeInfo + 8`, RCX unread.
// SAME SHAPE as SpriteSheet's slot 1 (0x180002e50).
extern "C" const char* RtUiDialog_TypeName(const void* self) {
    ++RtUiDlgV::g_counts.typeName;
    RT_DIFF_BEGIN("UI::Dialog::TypeName");
    (void)self;
    const char* ours = nullptr;
    std::memcpy(&ours, RtUiDlgV::DialogTypeInfo() + RtUiDlgV::kBaseTypeInfoName,
                sizeof ours);
    RT_DIFF_SCALAR(static_cast<const void*>(ours),
                   static_cast<const void*>(
                       RT_ORIGINAL_ONCE(RtUiDlgV::CStrThisFn,
                           "?TypeName@Dialog@UI@@UEBAPEBDXZ")(self)));
    RT_DIFF_WORK(true);
    return ours;
}

// ---- slot 3  ?GetFileVersion@Dialog@UI@@UEBAHXZ  0x1801130b0 ---------------
//   MOV EAX,6 / RET. The dialog resource's on-disk format version.
// SAME SHAPE as SpriteSheet's slot 3 (0x18011d710) — and, by coincidence, the
// same CONSTANT. That the two happen to agree is not a reason to share a body:
// they are two independent resource formats that both reached version 6.
extern "C" int RtUiDialog_GetFileVersion(const void* self) {
    ++RtUiDlgV::g_counts.fileVersion;
    RT_DIFF_BEGIN("UI::Dialog::GetFileVersion");
    (void)self;
    const int ours = 6;
    RT_DIFF_SCALAR(ours, RT_ORIGINAL_ONCE(RtUiDlgV::IntThisFn,
        "?GetFileVersion@Dialog@UI@@UEBAHXZ")(self));
    RT_DIFF_WORK(true);
    return ours;
}

// ---- slot 7  ?Destruct@Dialog@UI@@UEAAXXZ  0x1800387f0 ---------------------
//   MOV RAX,[RCX] / XOR EDX,EDX / JMP qword ptr [RAX]
// INSTRUCTION-IDENTICAL to SpriteSheet's slot 7 (0x180003180): a tail jump into
// SLOT 0 of the object's own vtable — the vector deleting destructor — with
// flag 0, i.e. "destroy, do not free". Reading the vtable from the OBJECT and
// not from a resolved constant is load-bearing: a derived Dialog would dispatch
// to its own slot 0, and hard-coding ours would break that silently.
//
// NOT REPLAYED. Its effect is to run a destructor on the live object, and
// calling the shipped body a second time would run it twice — a double free of
// the four arrays a Dialog owns.
extern "C" void RtUiDialog_Destruct(void* self) {
    ++RtUiDlgV::g_counts.destruct;
    RT_DIFF_BEGIN("UI::Dialog::Destruct");
    RT_DIFF_WORK(true);
    void** const vt = *reinterpret_cast<void***>(self);
    reinterpret_cast<void (*)(void*, uint32_t)>(vt[0])(self, 0);
}

// ---- slot 8  ?Construct@Dialog@UI@@UEAAPEAVExternal@Object@@XZ  0x180038750 -
//   if (this) { <the constructor, inlined>; return this; } else return nullptr;
// The null test at 0x180038761 is retail's own, not a guard added here.
extern "C" void* RtUiDialog_Construct(void* self) {
    if (!self) {
        ++RtUiDlgV::g_counts.constructNull;
        RT_DIFF_BEGIN("UI::Dialog::Construct");
        if (rt_diff_on__) {
            void* theirs = RT_ORIGINAL_ONCE(RtUiDlgV::PtrThisFn,
                "?Construct@Dialog@UI@@UEAAPEAVExternal@Object@@XZ")(nullptr);
            RT_DIFF_SCALAR(static_cast<void*>(nullptr), theirs);
        }
        RT_DIFF_WORK(false);          // the null arm does no work, and says so
        return nullptr;
    }
    ++RtUiDlgV::g_counts.construct;
    RT_DIFF_BEGIN("UI::Dialog::Construct");
    RtUiDlgV::ConstructInPlace(static_cast<uint8_t*>(self));

    if (rt_diff_on__) {
        // A PRIVATE scratch object, so the shipped body never writes the live
        // one. Poisoned first, and compared over the TWO written windows rather
        // than over sizeof — +0x44..+0x47 is written by neither side, and
        // comparing it reports allocator residue as a divergence.
        alignas(16) uint8_t theirs[0x88];
        std::memset(theirs, 0xCD, sizeof theirs);
        void* const ret = RT_ORIGINAL_ONCE(RtUiDlgV::PtrThisFn,
            "?Construct@Dialog@UI@@UEAAPEAVExternal@Object@@XZ")(theirs);
        RT_DIFF_BYTES(self, theirs, RtUiDlgV::kWrittenLoEnd - RtUiDlgV::kWrittenLo);
        RT_DIFF_BYTES(static_cast<uint8_t*>(self) + RtUiDlgV::kWrittenHi,
                      theirs + RtUiDlgV::kWrittenHi,
                      RtUiDlgV::kWrittenHiEnd - RtUiDlgV::kWrittenHi);
        RT_DIFF_SHADOW(ret == static_cast<void*>(theirs),
                       "returns this", "returns this");
        RT_DIFF_WORK(true);
    }
    return self;
}

// ---- slot 6  ?Reconstruct@Dialog@UI@@UEAAPEAVExternal@Object@@XZ  0x1800386b0
//   Destruct-then-Construct on the SAME storage. Note the difference from
//   Construct, which SpriteSheet's pair has too: there is NO null test on this
//   path (0x1800386c2 dereferences RCX immediately). Retail relies on the
//   caller never passing null, and CLAUDE.md's rule says leave the deref
//   faithful — do not "fix" a faithful crash with a null check.
//
// NOT REPLAYED, for Destruct's reason: the destructor half runs on the live
// object and cannot be run twice.
extern "C" void* RtUiDialog_Reconstruct(void* self) {
    ++RtUiDlgV::g_counts.reconstruct;
    RT_DIFF_BEGIN("UI::Dialog::Reconstruct");
    RT_DIFF_WORK(true);
    void** const vt = *reinterpret_cast<void***>(self);
    reinterpret_cast<void (*)(void*, uint32_t)>(vt[0])(self, 0);
    RtUiDlgV::ConstructInPlace(static_cast<uint8_t*>(self));
    return self;
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
// `RtSaveTrace::Compare` in the WRITING direction against two synthetic sinks —
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

// Array<T>::SetSize(array, newCount) — TWO arguments, not three. Lane Q's
// correction on the SetSize family (eight instantiations taking two arguments
// behind a three-argument typedef) was checked here per address: all four read
// RCX and EDX and nothing else.
using ArraySetSizeFn = void (*)(void*, uint32_t);
ArraySetSizeFn SetSizeNamedTemplates() {   // stride 0x10
    static ArraySetSizeFn f = reinterpret_cast<ArraySetSizeFn>(
        ResolveAddr(Module::Runtime, 0x180075d20)); return f;
}
ArraySetSizeFn SetSizeTemplates() {        // stride 0x08
    static ArraySetSizeFn f = reinterpret_cast<ArraySetSizeFn>(
        ResolveAddr(Module::Runtime, 0x1800492d0)); return f;
}
ArraySetSizeFn SetSizeConfigs() {          // stride 0x20
    static ArraySetSizeFn f = reinterpret_cast<ArraySetSizeFn>(
        ResolveAddr(Module::Runtime, 0x180075b20)); return f;
}
ArraySetSizeFn SetSizeReferences() {       // stride 0x08
    static ArraySetSizeFn f = reinterpret_cast<ArraySetSizeFn>(
        ResolveAddr(Module::Runtime, 0x18002f070)); return f;
}

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

// ---- probe scaffolding (same contract as spritesheet_vtable.cpp) -----------
Stream* StreamOf(void* serializer) {
    return static_cast<Serializer*>(serializer)->stream;
}

bool g_probing = false;
struct ProbeGuard {
    ProbeGuard()  { g_probing = true;  }
    ~ProbeGuard() { g_probing = false; }
};

const char* const kSerializeMangled =
    "?Serialize@Dialog@UI@@UEAA_NAEAVStream@IO@@@Z";

// `Compare` defends against `ours == theirs` by comparing function pointers,
// and the lambda adapters below defeat that check — so the equivalent check is
// made on the thing that matters: does the registry hand OUR body back? A probe
// whose ours-side reaches the body BY ADDRESS compares retail with ITSELF
// (reference_registering_an_address_converts_a_selfcheck_into_an_oracle.md).
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
using StreamMemberFn = void (*)(void*, Stream*);
StreamMemberFn RetailBody() {
    return RT_ORIGINAL_ONCE(StreamMemberFn, kSerializeMangled);
}

}  // namespace
}  // namespace RtUiDlgV

extern "C" bool RtUiDialog_Serialize(RtUI::Dialog* self, rt::io::Stream* stream) {
    using namespace RtUiDlgV;
    ++RtUiDlgV::g_counts.serialize;

    static int probes = 0;
    if (RtSaveTrace::Enabled() && !g_probing && probes < 16) {
        ++probes;
        ProbeGuard guard__;
        StreamMemberFn theirs = RetailBody();
        if (theirs && PortIsLive((const void*)&RtUiDialog_Serialize)) {
            RtSaveTrace::Compare(
                "UI::Dialog::Serialize", self,
                [](void* obj, void* ser) {
                    RtUiDialog_Serialize(static_cast<RtUI::Dialog*>(obj),
                                         StreamOf(ser));
                },
                [](void* obj, void* ser) { RetailBody()(obj, StreamOf(ser)); });
        }
    }

    Serializer ser;
    ReaderWriter* rw = MakeCodec(stream, &ser);
    uint8_t* const base = reinterpret_cast<uint8_t*>(self);
    auto u32 = [base](size_t off) { return reinterpret_cast<uint32_t*>(base + off); };
    auto i32 = [base](size_t off) { return reinterpret_cast<int32_t*>(base + off); };

    RecUInt(rw, u32(0x40));                                 // _defaultFlags
    ElementDescPtrSerialize()(base + 0x38, &ser);           // _rootDescription
    SpriteSheetPtrSerialize()(base + 0x20, &ser);           // _spriteSheet
    StringSerialize()(base + 0x28, &ser);                   // _spriteName
    RecUInt(rw, u32(0x30));                                 // _backgroundColor

    const bool reading = (ser.stream->direction == rt::io::kDirectionReading);

    // -- array 1: _namedTemplates (+0x58), count +0x60, stride 0x10 ---------
    {
        int32_t count = *i32(0x60);        // a STACK COPY, per 0x18011315e
        RecInt(rw, &count);
        if (reading) SetSizeNamedTemplates()(base + 0x58, static_cast<uint32_t>(count));
        // RE-READ after SetSize: it reallocates.
        uint8_t* e = *reinterpret_cast<uint8_t**>(base + 0x58);
        for (int32_t i = 0; i < count; ++i, e += 0x10) {
            ElementDescPtrSerialize()(e, &ser);             // entry+0x00
            StringSerialize()(e + 8, &ser);                 // entry+0x08
        }
    }

    // -- array 2: _templates (+0x68), count +0x70, stride 0x08 --------------
    {
        int32_t count = *i32(0x70);
        RecInt(rw, &count);
        if (reading) SetSizeTemplates()(base + 0x68, static_cast<uint32_t>(count));
        TemplatesWalker()(&ser, static_cast<uint32_t>(count),
                          *reinterpret_cast<void**>(base + 0x68));
    }

    // -- array 3: _controllerConfigs (+0x48), count +0x50, stride 0x20 ------
    {
        int32_t count = *i32(0x50);
        RecInt(rw, &count);
        if (reading) SetSizeConfigs()(base + 0x48, static_cast<uint32_t>(count));
        uint8_t* e = *reinterpret_cast<uint8_t**>(base + 0x48);
        for (int32_t i = 0; i < count; ++i, e += 0x20) {
            RecUInt(rw, reinterpret_cast<uint32_t*>(e));    // entry+0x00 _typeId
            ElementDescPtrSerialize()(e + 0x08, &ser);      // entry+0x08 _elementDesc
            ConfigPtrSerialize()(e + 0x10, &ser);           // entry+0x10 _config
            StringSerialize()(e + 0x18, &ser);              // entry+0x18 _name
        }
    }

    // -- array 4: _references (+0x78), count +0x80, stride 0x08 -------------
    {
        int32_t count = *i32(0x80);
        RecInt(rw, &count);
        if (reading) SetSizeReferences()(base + 0x78, static_cast<uint32_t>(count));
        ReferencesWalker()(&ser, static_cast<uint32_t>(count),
                           *reinterpret_cast<void**>(base + 0x78));
    }

    RecInt(rw, i32(0x34));      // _drawLevel — the FIELD, not a copy. LAST.

    DestroyCodec(rw);
    return true;
}

// ============================================================================
// Registration — BOTH doors on every row, for the reason the mint itself gives:
// a minted vtable slot is filled by NAME, but 30-odd sites in this tree reach
// exported code by ADDRESS, and a name-only registration leaves those walking
// past the port into the DLL.
//
// Slot 0 (0x180038b20) is NOT registered here — it is row 37 of the family in
// runtime/src/object/deleting_dtor.cpp, registered under BANISHED_RT_DELDTOR.
// THE COUPLING IS REAL AND IS STATED: `BANISHED_RT_DELDTOR=0` turns slot 0 back
// into a trap and makes this vtable unsafe to promote. That is not new with
// this lane — `??_7SpriteSheet@UI@@6B@`, promoted last wave, has exactly the
// same dependency on exactly the same switch.
// ============================================================================
int RtUiDialogVtableRegister() {
    int n = 0;
    RegisterRuntimeExport("?Type@Dialog@UI@@UEBAIXZ",
                          (void*)&RtUiDialog_Type); ++n;
    RegisterRuntimeAddr(0x180038420, (void*)&RtUiDialog_Type);
    RegisterRuntimeExport("?TypeId@Dialog@UI@@SAIXZ",
                          (void*)&RtUiDialog_TypeId); ++n;
    RegisterRuntimeAddr(0x180038430, (void*)&RtUiDialog_TypeId);
    RegisterRuntimeExport("?TypeName@Dialog@UI@@UEBAPEBDXZ",
                          (void*)&RtUiDialog_TypeName); ++n;
    RegisterRuntimeAddr(0x180038440, (void*)&RtUiDialog_TypeName);
    RegisterRuntimeExport("?GetFileVersion@Dialog@UI@@UEBAHXZ",
                          (void*)&RtUiDialog_GetFileVersion); ++n;
    RegisterRuntimeAddr(0x1801130b0, (void*)&RtUiDialog_GetFileVersion);
    RegisterRuntimeExport("?Destruct@Dialog@UI@@UEAAXXZ",
                          (void*)&RtUiDialog_Destruct); ++n;
    RegisterRuntimeAddr(0x1800387f0, (void*)&RtUiDialog_Destruct);
    RegisterRuntimeExport("?Construct@Dialog@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUiDialog_Construct); ++n;
    RegisterRuntimeAddr(0x180038750, (void*)&RtUiDialog_Construct);
    RegisterRuntimeExport("?Reconstruct@Dialog@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUiDialog_Reconstruct); ++n;
    RegisterRuntimeAddr(0x1800386b0, (void*)&RtUiDialog_Reconstruct);
    RegisterRuntimeExport("?Serialize@Dialog@UI@@UEAA_NAEAVStream@IO@@@Z",
                          (void*)&RtUiDialog_Serialize); ++n;
    RegisterRuntimeAddr(0x1801130c0, (void*)&RtUiDialog_Serialize);
    return n;
}
