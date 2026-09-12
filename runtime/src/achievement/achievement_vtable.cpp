// ============================================================================
// runtime/src/achievement/achievement_vtable.cpp — WAVE 27 lane ACHIEVE
// ============================================================================
//
// THE FOUR NON-ACCESSOR SLOTS of `??_7Achievement@0@6B@` and
// `??_7AchievementInstance@Achievement@@6B@`. The other eleven slots of the
// four Achievement vtable rows are in achievement_typeinfo_accessors.cpp; read
// that file's banner first, it carries the denominator for both.
//
//   ?Construct@Achievement@1@UEAAPEAVExternal@Object@@XZ    0x1800012f0  slot 8
//   ?Reconstruct@Achievement@1@UEAAPEAVExternal@Object@@XZ  0x180001270  slot 6
//   ?Destruct@Achievement@1@UEAAXXZ                         0x180001370  slot 7
//   ?Persist@AchievementInstance@Achievement@@UEAAXAEAVStream@IO@@@Z
//                                                           0x18009e4a0  slot 3
//
// GREPPED BEFORE WRITTEN: all four mangled names and all four addresses
// returned nothing across code/ engine/ runtime/ standalone/ dx11/ except the
// generated standalone/rt_data_mint_tables.inc.
//
// ============================================================================
// A GHIDRA MCP DEFECT MADE THE FIRST READING OF TWO OF THESE WRONG
// ============================================================================
// `mcp__ghidra-mcp__disassemble_function(address="0x18009f0e0")` returned a
// DIFFERENT function (entry 0x18009f090, a 0x100-byte allocation stamping
// vtable 0x1801a9310) while `get_function_by_address` on the SAME address
// correctly reported `Initialize at 18009f0e0, body 18009f0e0-18009f264`. The
// same happened for 0x180009d70 (returned entry 0x180009c20). It is the
// address-silently-ignored / name-fallback defect already recorded for
// `get_function_metrics`. NOT FIXED — kickoff §4 #1. Written up in
// WAVE27_LANE_ACHIEVE.md under INSTRUMENT DEFECT OBSERVED, NOT FIXED.
//
// Consequence for this file and its sibling goal_list.cpp: every decode below
// was re-taken from the SHIPPED BYTES via `pefile` + `objdump -b binary`, not
// from the MCP disassembler, and each listing is transcribed beside the body it
// produced.
//
// ============================================================================
// Construct / Reconstruct / Destruct — Object::External's three-hook protocol
// ============================================================================
// `Object::External` gives every resource class three virtual hooks the loader
// uses to recycle storage in place: build into raw memory, tear down without
// freeing, and do both. All three are inlined copies of one constructor.
//
//   ?Destruct@Achievement@1@  0x180001370, THREE instructions:
//       48 8b 01     MOV RAX,[RCX]        ; this->vtable
//       33 d2        XOR EDX,EDX          ; flags = 0  -> do NOT free
//       48 ff 20     JMP [RAX]            ; tail call slot 0
//     i.e. `this->vtable[0](this, 0)` — the scalar-deleting destructor with the
//     delete bit CLEAR. The call is VIRTUAL, so on an EXE subclass (Scenario)
//     it runs the subclass's destructor, which is the whole point of the hook.
//
//   ?Reconstruct@Achievement@1@  0x180001270:
//       MOV RAX,[RCX]; XOR EDX,EDX; CALL [RAX]     ; destruct in place
//       …then the THIRTEEN STORES of ??0Achievement@0@, inlined verbatim:
//       [this]=??_7Base@Object@@6B@ (0x180154788)
//       [this]=??_7External@Object@@6B@ (0x1801547a0)
//       [this+0x08]=[this+0x10]=[this+0x18]=0
//       [this]=??_7Achievement@0@6B@ (0x180154740)
//       [this+0x20 .. +0x50]=0
//       MOV RAX,RBX; RET                            ; returns `this`
//
//   ?Construct@Achievement@1@  0x1800012f0:
//       TEST RCX,RCX; JZ -> `XOR EDX,EDX; MOV EAX,EDX` (return nullptr)
//       …the SAME thirteen stores…
//       MOV RAX,RCX; RET
//     The null test is the ONLY difference from Reconstruct's tail, and it is
//     retail's, not a guard added here.
//
// THE THIRTEEN STORES ARE NOT RE-TRANSCRIBED. `RtAch_Achievement_Ctor`
// (achievement_manager.cpp:999, Wave 5 lane B) is already the byte-faithful
// port of that exact sequence, already carries the full-byte oracle against
// `??0Achievement@0@QEAA@XZ` into a poisoned scratch buffer, and is already
// registered. Writing the stores a second time here would be a second copy of
// a constant that must never drift — the precise shape of
// reference_grep_for_the_port_before_writing_it.md. These three bodies CALL it.
//
// THE DEAD VTABLE STORES SURVIVE THAT REUSE. Two of the thirteen (Base, then
// External) are overwritten before anything reads them; they are kept inside
// RtAch_Achievement_Ctor for the reason its own banner gives, and calling it
// preserves them.
//
// NO DIFF ORACLE ON ANY OF THE THREE, and the reason is the same for all:
// running retail's copy as a second opinion would run a SECOND destructor on a
// live object (Destruct, Reconstruct) or a second construction over storage we
// have already built (Construct). The oracle that matters already runs, one
// level down, inside RtAch_Achievement_Ctor — the full-byte comparison of the
// thirteen stores, which is the only part of these three bodies that is not a
// single dispatch. What is NOT covered: the virtual slot-0 call itself. That is
// one indirect call through a pointer this file does not compute.
//
// ============================================================================
// AchievementInstance::Persist — THE OUTLIER OF THE Serialize FAMILY
// ============================================================================
// It is the FIFTH member of the family goal_serialize.cpp and
// achievement_serialize.cpp ported, and it is the only one that is not the
// plain "one codec, N records, destroy codec" shape. From the bytes at
// 0x18009e4a0:
//
//   ser1 = { stream, null }
//   enc  = stream->encoding                                 ; [stream+0x10]
//   ser1.rw = stream->direction ? enc->CreateWriter(&ser1, 1)
//                               : enc->CreateReader(&ser1, 1)
//        *** THE VERSION ARGUMENT IS 1, NOT 0 ***   (MOV R8D,1 at 0x18009e4c9)
//        Every other member of this family passes 0. `MakeCodec` in the two
//        sibling files HARDCODES 0, which is why this file does not reuse it.
//   if (ser1.rw->GetVersion() == 0) { ser1.rw->Skip(); goto cleanup; }
//                                                    ; slots +0x08 and +0x10
//   ExternalPtr<Achievement>::Serialize(&this->achievement, &ser1)  ; +0x20
//   ser2 = { stream, null }
//   ser2.rw = … same factory, VERSION 0 …             (XOR R8D,R8D at 0x18009e522)
//   int count = this->goalList._size                  ; [this+0x18]
//   ser2.rw->ScalarInt(&count, null)                  ; slot +0xA0
//   if (stream->direction == reading)
//       Array<InternalPtr<GoalInstance>>::SetSize(&this->goalList, count)
//   SerializeInstancePtrs(&ser2, count, this->goalList._data)
//   if (ser2.rw) ser2.rw->vtable[0](ser2.rw, 1)
//   cleanup:
//   if (ser1.rw) ser1.rw->vtable[0](ser1.rw, 1)
//
// TWO CODECS, NESTED, WITH DIFFERENT VERSIONS. The outer one exists only to ask
// the encoding whether this optional record block is present at all; the inner
// one carries the goal list. Collapsing them into one, or passing 0 for the
// outer, changes the bytes on disk.
//
// THE VERSION GATE IS A REAL EARLY RETURN. `GetVersion()==0` skips the
// achievement pointer AND the whole goal list. A save written by an older
// build takes that path.
//
// ---- ORACLE, AND THE HONEST LIMIT OF IT ------------------------------------
// PREFIX, stopAfter = 1, through RtSaveTrace::Compare — the same mechanism
// goal_serialize.cpp and achievement_serialize.cpp use, for the same reason:
// the tail (`SerializeInstancePtrs`, 0x180009fa0) is a loop of
// `Object::InternalBasePtr::Serialize`, which save_trace.h names in as many
// words as a member that "mutates the pointer AND the global factory".
// Replaying it performs that mutation twice on the one live factory.
//
// stopAfter = 1 IS A FLOOR I CHOSE, NOT THE INDEX save_trace.h ASKS FOR. That
// header says to pick "the emission index of the last call before the first
// store", read off the disassembly. I could not derive it: the emissions before
// the mutating loop include however many `ExternalPtr<Achievement>::Serialize`
// (0x180006d10) makes, and that body BRANCHES on `Encoding::GetVersion()==3`
// into paths with different record counts. So this oracle covers the codec
// construction and AT MOST the first emission, and NOTHING after it. The
// version argument (1 vs 0), the two-codec nesting and the early return are
// therefore checked; the achievement pointer record, the count record and every
// instance record are NOT.
//
// A SECOND LIMIT, stated because a silent one is worse: if the synthetic sink's
// `GetVersion()` returns 0, BOTH sides take the Skip path and the comparison is
// vacuous — a pass that proves nothing
// (reference_probe_needs_an_effectiveness_control.md). The run's own
// [RtSaveTrace] emission counts are what distinguish that case; a compare that
// reports zero emissions on both sides is the vacuous one.
//
// ---- THREE NEW ADDRESS DOORS, NAMED RATHER THAN HIDDEN ---------------------
// Each is commented at its call site in the kickoff §4 #10 shape (SYMBOL,
// CONDITION, CONSEQUENCE). They grow the cutover surface — the known cost of a
// faithful port that calls DLL internals
// (reference_porting_can_grow_the_cutover_surface.md).
// ============================================================================

#include <cstdint>
#include <cstdio>

#include "../../funcs/rt_port.h"
#include "../../types/achievement/AchievementManager.h"
#include "../../types/io/Serializer.h"
#include "../io/save_trace.h"

using RtAchievement::AchievementInstance;
using rt::io::Encoding;
using rt::io::ReaderWriter;
using rt::io::Serializer;
using rt::io::Stream;

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

// achievement_manager.cpp, Wave 5 lane B — the byte-faithful port of
// ??0Achievement@0@QEAA@XZ (0x18009e240) and its thirteen stores.
extern void* RtAch_Achievement_Ctor(void* selfRaw);

extern "C" void* RtAch_Achievement_Construct(void*);
extern "C" void* RtAch_Achievement_Reconstruct(void*);
extern "C" void  RtAch_Achievement_Destruct(void*);

namespace {

// ---------------------------------------------------------------------------
// The codec prologue, WITH the version argument exposed.
// ---------------------------------------------------------------------------
// Deliberately NOT shared with goal_serialize.cpp / achievement_serialize.cpp:
// those two hardcode version 0 and are gated by a different env switch, and a
// shared helper would make one lane's kill switch silently affect the other's
// bodies. The duplication is the same one those two files already record.
ReaderWriter* MakeCodecVersioned(Stream* stream, Serializer* ser, int version) {
    ser->stream = stream;
    ser->readerWriter = nullptr;

    Encoding* enc = stream->encoding;
    void** encVt = enc->vtable;
    const int slot = (stream->direction == rt::io::kDirectionReading)
                         ? rt::io::encoding_vtbl::kCreateReader
                         : rt::io::encoding_vtbl::kCreateWriter;

    using FactoryFn = ReaderWriter* (*)(Encoding*, Serializer*, int);
    ReaderWriter* rw = reinterpret_cast<FactoryFn>(encVt[slot / 8])(enc, ser, version);
    ser->readerWriter = rw;
    return rw;
}

// `if (rw) rw->vtable[0](rw, 1)` — the scalar DELETING destructor. Retail runs
// it for BOTH codecs, inner first.
void DestroyCodec(ReaderWriter* rw) {
    if (!rw) return;
    using DtorFn = void (*)(ReaderWriter*, int);
    reinterpret_cast<DtorFn>(rw->vtable[rt::io::rw_vtbl::kDtor / 8])(rw, 1);
}

using PtrSerializeFn   = void (*)(void*, Serializer*);
using ArraySetSizeFn   = void (*)(void*, int);
using ArraySerializeFn = void (*)(Serializer*, int, void*);

// ---- ADDRESS DOOR -----------------------------------------------------------
// SYMBOL:      Object::ExternalPtr<Achievement::Achievement>::Serialize,
//              Runtime 0x180006d10. UNEXPORTED template instantiation — there
//              is no mangled name to ask for, which is why this is an address
//              door and not a name door (contrast §4 #4: `AEAA` private members
//              ARE exported; this one genuinely is not in the export table).
//              Identified from its own bytes: at 0x180006d68 it loads
//              `[0x1801f9898]`, which is `?_typeInfo@Achievement@1@` — the
//              class it is instantiated for.
// CONDITION:   reached only from AchievementInstance::Persist below, once per
//              call, on BOTH directions.
// CONSEQUENCE: at cutover this address resolves to nothing unless something
//              registers it, and Persist then calls null. It is CALLED, not
//              reimplemented; whoever runs the cutover attempt that first
//              reaches a save/load must either port it or accept a dead
//              Persist. Same standing cost as the four doors
//              achievement_serialize.cpp opened.
PtrSerializeFn ExternalPtrSerialize_Achievement() {
    static PtrSerializeFn f = nullptr;
    if (!f) f = reinterpret_cast<PtrSerializeFn>(ResolveAddr(Module::Runtime, 0x180006d10));
    return f;
}

// ---- ADDRESS DOOR -----------------------------------------------------------
// SYMBOL:      Collection::Array<Object::InternalPtr<Achievement::GoalInstance>>
//              ::SetSize, Runtime 0x180009d70. Unexported template
//              instantiation. It is a DIFFERENT instantiation from the
//              0x180008d60 that achievement_serialize.cpp uses (that one is
//              Array<InternalPtr<AchievementInstance>>, and it IS ported —
//              Wave 9 lane Q). Identified by its two callers: this Persist and
//              `?Initialize@GoalList@Achievement@@` (0x18009f10d), both of
//              which hand it a GoalList.
// CONDITION:   READ DIRECTION ONLY here (`stream->direction == reading`). On a
//              save it is never reached.
// CONSEQUENCE: unported, so at cutover every LOAD of a game with achievements
//              calls null. Named here and in goal_list.cpp, which reaches it
//              unconditionally.
ArraySetSizeFn ArraySetSizeGoalInstancePtrs() {
    static ArraySetSizeFn f = nullptr;
    if (!f) f = reinterpret_cast<ArraySetSizeFn>(ResolveAddr(Module::Runtime, 0x180009d70));
    return f;
}

// ---- ADDRESS DOOR -----------------------------------------------------------
// SYMBOL:      the N-element serializer for
//              Object::InternalPtr<Achievement::GoalInstance>,
//              Runtime 0x180009fa0. Signature (Serializer*, int count, T* data),
//              the same shape as the 0x180009a20 that achievement_serialize.cpp
//              opened for InternalPtr<AchievementInstance>.
// CONDITION:   both directions, once per Persist.
// CONSEQUENCE: this is the MUTATING tail — it ends in PersistObject, which
//              mutates the pointer and the global Object::Factory. It is why
//              the oracle above is a prefix, and it is unported, so at cutover
//              it resolves to nothing.
ArraySerializeFn SerializeGoalInstancePtrs() {
    static ArraySerializeFn f = nullptr;
    if (!f) f = reinterpret_cast<ArraySerializeFn>(ResolveAddr(Module::Runtime, 0x180009fa0));
    return f;
}

// The save_trace adapter and its re-entrancy guard, transcribed from
// goal_serialize.cpp. `Compare` hands each side a `Serializer*` because its
// first subject took one; this member takes an `IO::Stream&`, and
// `Serializer::stream` is at +0x00, so the sink's Stream is recovered from it.
using StreamMemberFn = void (*)(void*, Stream*);

Stream* StreamOf(void* serializer) {
    return static_cast<Serializer*>(serializer)->stream;
}

// `Compare` calls `ours`, which is this very body. Without the flag each nested
// entry would start ANOTHER comparison, and Compare is not reentrant.
bool g_probing = false;

struct ProbeGuard {
    ProbeGuard()  { g_probing = true; }
    ~ProbeGuard() { g_probing = false; }
};

// Defends against the vacuous `ours == theirs` pass
// (reference_uncalled_port_is_not_verified.md) on the one thing that decides
// it: whether the export registry hands OUR body back for this name.
bool PortIsLive(const char* mangled, const void* ours) {
    void* viaRegistry = ResolveExport(mangled);
    if (viaRegistry == ours) return true;
    std::fprintf(stderr,
                 "[RtSaveTrace] FAIL %s: the port is NOT registered (registry -> %p, "
                 "ours %p), so this comparison would prove nothing\n",
                 mangled, viaRegistry, ours);
    return false;
}

}  // namespace

// ============================================================================
// void Achievement::Achievement::Destruct()
// ?Destruct@Achievement@1@UEAAXXZ                          retail 0x180001370
//
// `this->vtable[0](this, 0)` and nothing else. Slot 0 is the scalar-deleting
// destructor; flags 0 means destroy in place and do NOT free the storage.
//
// The deref of `this->vtable` is unguarded in retail and stays unguarded here.
// ============================================================================
extern "C" void RtAch_Achievement_Destruct(void* self) {
    void* const* vtable = *static_cast<void* const* const*>(self);
    using DeletingDtorFn = void (*)(void*, int);
    reinterpret_cast<DeletingDtorFn>(vtable[0])(self, 0);
}

// ============================================================================
// Object::External* Achievement::Achievement::Reconstruct()
// ?Reconstruct@Achievement@1@UEAAPEAVExternal@Object@@XZ    retail 0x180001270
//
// Destruct-in-place through the VIRTUAL slot 0, then re-run the Achievement
// constructor over the same storage, then return `this`.
//
// The virtual call is what makes this different from "call the destructor":
// on an EXE subclass (Scenario) it destroys the SUBCLASS, and the
// reconstruction then rebuilds only the Achievement prefix — leaving a
// subclass object with a base-class vptr. That is what retail does and it is
// what the loader relies on, because the caller re-stamps afterwards.
// ============================================================================
extern "C" void* RtAch_Achievement_Reconstruct(void* self) {
    void* const* vtable = *static_cast<void* const* const*>(self);
    using DeletingDtorFn = void (*)(void*, int);
    reinterpret_cast<DeletingDtorFn>(vtable[0])(self, 0);

    RtAch_Achievement_Ctor(self);
    return self;
}

// ============================================================================
// Object::External* Achievement::Achievement::Construct()
// ?Construct@Achievement@1@UEAAPEAVExternal@Object@@XZ      retail 0x1800012f0
//
// `if (!this) return nullptr;` then the constructor's thirteen stores, then
// return `this`. The null test is RETAIL'S — `TEST RCX,RCX / JZ` at
// 0x180001301 — not a guard added by this port. It is the one place in this
// family where retail does check, and dropping it would change behaviour just
// as adding one elsewhere would.
// ============================================================================
extern "C" void* RtAch_Achievement_Construct(void* self) {
    if (!self) return nullptr;
    RtAch_Achievement_Ctor(self);
    return self;
}

// ============================================================================
// void Achievement::AchievementInstance::Persist(IO::Stream&)
// ?Persist@AchievementInstance@Achievement@@UEAAXAEAVStream@IO@@@Z
//                                                          retail 0x18009e4a0
//
// VIRTUAL (UEAA). RCX = this, RDX = the Stream. See this file's banner for the
// full transcription, the two-codec nesting, the version-1 argument and the
// stated limits of the prefix oracle.
//
// PORTING THIS COMPLETES ??_7AchievementInstance@Achievement@@6B@ at 4/4.
// ============================================================================
extern "C" void RtAch_AchievementInstance_Persist(AchievementInstance* self, Stream* stream) {
    static int probes = 0;
    if (RtSaveTrace::Enabled() && !g_probing && probes < 16) {
        ++probes;
        ProbeGuard guard__;
        static const char* const kMangled =
            "?Persist@AchievementInstance@Achievement@@UEAAXAEAVStream@IO@@@Z";
        auto theirs = reinterpret_cast<StreamMemberFn>(ResolveExportRaw(kMangled));
        if (theirs && PortIsLive(kMangled, (const void*)&RtAch_AchievementInstance_Persist)) {
            RtSaveTrace::Compare(
                "Achievement::AchievementInstance::Persist", self,
                [](void* obj, void* ser) {
                    RtAch_AchievementInstance_Persist(
                        static_cast<AchievementInstance*>(obj), StreamOf(ser));
                },
                [](void* obj, void* ser) {
                    reinterpret_cast<StreamMemberFn>(ResolveExportRaw(
                        "?Persist@AchievementInstance@Achievement@@UEAAXAEAVStream@IO@@@Z"))(
                            obj, StreamOf(ser));
                },
                /*stopAfter=*/1);
        }
    }

    // The OUTER codec, version 1. See the banner: every sibling in this family
    // passes 0 here and this one does not.
    Serializer outer;
    ReaderWriter* rwOuter = MakeCodecVersioned(stream, &outer, 1);

    using VersionFn = int  (*)(ReaderWriter*);
    using SkipFn    = void (*)(ReaderWriter*);
    const int version = reinterpret_cast<VersionFn>(
        rwOuter->vtable[rt::io::rw_vtbl::kGetVersion / 8])(rwOuter);

    if (version == 0) {
        // The optional block is absent in this stream's format. Retail skips it
        // and destroys only the outer codec — the inner one was never built.
        reinterpret_cast<SkipFn>(
            rwOuter->vtable[rt::io::rw_vtbl::kSkip / 8])(rwOuter);
        DestroyCodec(rwOuter);
        return;
    }

    ExternalPtrSerialize_Achievement()(&self->achievement, &outer);   // +0x20

    // The INNER codec, version 0, built over the SAME stream.
    Serializer inner;
    ReaderWriter* rwInner = MakeCodecVersioned(stream, &inner, 0);

    int count = self->goalList.goals._size;                          // +0x18
    using IntFn = void (*)(ReaderWriter*, int*, const char*);
    reinterpret_cast<IntFn>(rwInner->vtable[rt::io::rw_vtbl::kScalarInt / 8])(
        rwInner, &count, nullptr);

    if (stream->direction == rt::io::kDirectionReading) {
        ArraySetSizeGoalInstancePtrs()(&self->goalList, count);
    }
    SerializeGoalInstancePtrs()(&inner, count, self->goalList.goals._data);

    DestroyCodec(rwInner);
    DestroyCodec(rwOuter);
}

// ---- Registration ----------------------------------------------------------
// All four are gated by BANISHED_RT_ACHMGR with achievement_manager.cpp,
// achievement_state.cpp and achievement_serialize.cpp — class atomicity
// (PLAN §4.1 decision 5).
//
// Every one of the four is a slot of a MINTED, UNPROMOTED vtable, so
// registering them changes no dispatch today. They are registered anyway,
// because an unregistered body is indistinguishable from an absent one to the
// census (reference_half_registered_port_reads_as_absent.md) and because these
// slots are exactly what the COMPLETE-ness of the two rows was waiting on.
// By NAME and by ADDRESS both: the slots are RT_MINT_NAMED, and the address
// door makes a by-address call site land on the same body.
int RtAchVtableRegister() {
    int n = 0;

    RegisterRuntimeExport("?Construct@Achievement@1@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtAch_Achievement_Construct); ++n;
    RegisterRuntimeAddr(0x1800012f0ull, (void*)&RtAch_Achievement_Construct);

    RegisterRuntimeExport("?Reconstruct@Achievement@1@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtAch_Achievement_Reconstruct); ++n;
    RegisterRuntimeAddr(0x180001270ull, (void*)&RtAch_Achievement_Reconstruct);

    RegisterRuntimeExport("?Destruct@Achievement@1@UEAAXXZ",
                          (void*)&RtAch_Achievement_Destruct); ++n;
    RegisterRuntimeAddr(0x180001370ull, (void*)&RtAch_Achievement_Destruct);

    RegisterRuntimeExport("?Persist@AchievementInstance@Achievement@@UEAAXAEAVStream@IO@@@Z",
                          (void*)&RtAch_AchievementInstance_Persist); ++n;
    RegisterRuntimeAddr(0x18009e4a0ull, (void*)&RtAch_AchievementInstance_Persist);

    return n;
}
