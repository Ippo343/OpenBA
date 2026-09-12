// ============================================================================
// runtime/src/achievement/achievement_typeinfo_accessors.cpp — WAVE 27 ACHIEVE
// ============================================================================
//
// ELEVEN MINTED VTABLE SLOT BODIES, ACROSS ALL FOUR Achievement `??_7` ROWS.
//
// SUBJECT AND DENOMINATOR, STATED FIRST. Lane ACHIEVE owns the four Achievement
// vtable rows of the Wave 27 promotion backlog. Re-derived on this tip with
// `tools/runtime_census.py` and corroborated by `tools/mint_slot_sweep.py`
// (which reports the same unported-slot counts from a different scan):
//
//     ??_7Achievement@0@6B@                      4/9   -> 5 unported
//     ??_7Goal@Achievement@@6B@                  4/7   -> 3 unported
//     ??_7GoalInstance@Achievement@@6B@          3/7   -> 4 unported
//     ??_7AchievementInstance@Achievement@@6B@   1/4   -> 3 unported
//
// This file writes 11 of those 15. It does NOT write the other 4:
// `Construct`/`Destruct`/`Reconstruct@Achievement@1@` and
// `Persist@AchievementInstance@Achievement@@`, which are not accessors and are
// elsewhere (see WAVE27_LANE_ACHIEVE.md §3 for where each of the 15 landed).
//
// GREPPED BEFORE WRITTEN (kickoff §4 #3). Every one of the 11 mangled names,
// and every one of the 11 retail addresses, was grepped across code/ engine/
// runtime/ standalone/ dx11/ before a line of this file existed. All 11
// returned nothing outside standalone/rt_data_mint_tables.inc (the generated
// slot table, which is a description of the DLL, not a port).
//
// THE SHAPE IS NOT NEW. `runtime/src/object/object_typeinfo_accessors.cpp`
// (Wave 11 lane OBJDATA) and `runtime/src/graphics/graphics_typeinfo_accessors.cpp`
// (Wave 21 lane GFXVT) already ported the identical accessor family for the
// Object and Graphics rows. This file follows GFXVT deliberately — same door,
// same oracle idiom, same memcpy-out-of-the-node reading — so a reader who has
// read that file has read this one.
//
// ============================================================================
// THE BODIES, READ FROM THE SHIPPED Runtime-x64.dll BYTES
// ============================================================================
// Disassembled, not decompiled: Ghidra's C output drops dereferences and prints
// MOVSXD as a zero extension (reference_ghidra_c_output_vs_disassembly.md).
//
//   ?Type@Achievement@1@UEBAIXZ                       0x180001020
//       MOV EAX,dword ptr [0x1801f9898] ; RET          = node+0x00
//   ?TypeName@Achievement@1@UEBAPEBDXZ                0x180001040
//       MOV RAX,qword ptr [0x1801f98a0] ; RET          = node+0x08
//
//   ?Type@AchievementInstance@Achievement@@UEBAIXZ     0x1800017a0
//       MOV EAX,dword ptr [0x1801f98d0] ; RET          = node+0x00
//   ?TypeName@AchievementInstance@Achievement@@UEBAPEBDXZ 0x1800017c0
//       MOV RAX,qword ptr [0x1801f98d8] ; RET          = node+0x08
//
//   ?Type@Goal@Achievement@@UEBAIXZ                    0x180001b40
//       MOV EAX,dword ptr [0x1801f9908] ; RET          = node+0x00
//   ?TypeName@Goal@Achievement@@UEBAPEBDXZ             0x180001b60
//       MOV RAX,qword ptr [0x1801f9910] ; RET          = node+0x08
//
//   ?Type@GoalInstance@Achievement@@UEBAIXZ            0x180001e40
//       MOV EAX,dword ptr [0x1801f9940] ; RET          = node+0x00
//   ?TypeName@GoalInstance@Achievement@@UEBAPEBDXZ     0x180001e60
//       MOV RAX,qword ptr [0x1801f9948] ; RET          = node+0x08
//
//   ?GetFileVersion@Goal@Achievement@@UEBAHXZ          0x18009ee10
//       MOV EAX,0x10000 ; RET                          -> literal 65536
//
//   ?Startup@GoalInstance@Achievement@@UEAAXXZ         0x18009f000
//       RET                                            -> EMPTY. One byte.
//
//   ?CheckGoalState@GoalInstance@Achievement@@UEBA?AW4GoalState@Goal@2@XZ
//                                                      0x18009f060
//       MOV RCX,qword ptr [RCX + 0x18]                 ; self->_goal.ptr
//       MOV RAX,qword ptr [RCX]                        ; that Goal's vtable
//       JMP  qword ptr [RAX + 0x40]                    ; tail call, slot 8
//
// `this` is read by only TWO of the eleven — the eight node readers and the
// empty Startup ignore RCX entirely. That is not an inference from a decompile:
// the eight encodings above have NO RCX operand at all, and Startup is a single
// `RET`. It is what makes the node readers shareable across every instance of
// the class, and why `self` below is present-but-unused in those nine.
//
// THE +0x00 / +0x08 READING IS CROSS-CHECKED, not merely decoded. All four node
// addresses appear in standalone/rt_data_mint_tables.inc as the minted
// `?_typeInfo@…@Achievement@@0VBaseTypeInfo@Object@@A` rows at exactly those
// VAs — a second, independently generated instrument agreeing with this lane's
// byte decode:
//
//   ?_typeInfo@Achievement@1@              0x1801f9898   (.inc line 1853)
//   ?_typeInfo@AchievementInstance@…@      0x1801f98d0   (.inc line 1857)
//   ?_typeInfo@Goal@Achievement@@          0x1801f9908   (.inc line 2021)
//   ?_typeInfo@GoalInstance@Achievement@@  0x1801f9940   (.inc line 2025)
//
// THE DOOR IS A NAME DOOR, and `0V…A` is the PRIVATE static spelling, not an
// unexported one — this DLL exports its private members
// (reference_aeaa_is_private_not_unexported.md), so no address door is needed
// to reach a node. Reading through RT_DATA_SLOT rather than pinning the DLL's
// node is what makes these bodies FOLLOW a promotion of the `?_typeInfo@` row.
//
// ⚠ A POINTER DIVERGENCE ON THE FOUR `TypeName` BODIES IS EXPECTED, NOT A
//   REGRESSION, once any `?_typeInfo@` row is promoted: our node lives in our
//   image and the shipped export returns the DLL's — same characters, different
//   object. `UI::SpriteSheet::TypeName`, `Object::External::TypeName` and all
//   three Graphics TypeName bodies already report exactly this. The comparison
//   is deliberately left AS A POINTER rather than relaxed to a strcmp, because
//   it is the one screen that can still see identity move, and the identity
//   question is retired from both shipped binaries (zero consumers of any
//   node's +0x08 — reference_basetypeinfo_name_is_content_only.md).
//
// ============================================================================
// CheckGoalState — THE ONE BODY HERE THAT IS NOT AN ACCESSOR
// ============================================================================
// It is a three-instruction TAIL CALL and it is UNGUARDED. `MOV RCX,[RCX+0x18]`
// then `MOV RAX,[RCX]` faults if `_goal.ptr` is null; retail does not test it.
// That deref stays faithful (CLAUDE.md "Don't 'fix' a faithful crash with a
// null check"): a GoalInstance whose _goal is null is an upstream defect, and a
// guard here would skip the state evaluation rather than restore it.
//
// SLOT 8 (`[RAX+0x40]`) OF `??_7Goal@Achievement@@6B@` IS PURECALL IN RETAIL
// (standalone/rt_data_mint_tables.inc kVSlots29[8] == RT_MINT_PURECALL, address
// 0x18007ec28). That is not a decode error: `Achievement::Goal` declares
// CheckGoalState pure and the EXE's subclasses (StatGoal, TimeGoal,
// PopulationGoal, BuildingGoal, FinishTutorialGoal, … in code/Game/Goal/)
// override it. So the object this body dispatches on is never a bare
// `Achievement::Goal` at runtime, and the call lands in the exe.
//
// runtime/types/achievement/Goal.h already records +0x18 as
// `ExternalPtr<Achievement::Goal>` and names 0x18009f060 as one of the two
// bodies that pin it, so this port changes no type.
//
// NO `?AW4GoalState@Goal@2@` RETURN-SLOT HAZARD. The mangled return type is a
// `W4` enum, which the MS x64 ABI returns in EAX like an int — there is no
// hidden return slot to omit (reference_hidden_retslot_wild_write.md). The
// `JMP` rather than `CALL/RET` in retail is itself the proof: a body with a
// hidden return-slot argument cannot tail-jump to a function that does not take
// one. `RtAchievement::GoalState` is `int32_t`-backed, per Goal.h.
//
// ============================================================================
// Startup — AN EMPTY BODY IS STILL A PORT
// ============================================================================
// `?Startup@GoalInstance@Achievement@@UEAAXXZ` is one `RET`. Porting a one-byte
// function looks pointless until you remember WHY these rows are in the
// backlog: the row cannot be promoted while ANY non-purecall slot has no body,
// because the mint fills an unfillable slot with a NAMED TRAP that aborts at
// cutover. An empty retail body is a slot that must be filled with an empty
// OUR body, not one that can be skipped. It carries no diff probe because there
// is nothing to compare: no return value, no argument read, no side effect.
//
// ============================================================================
// WHAT THIS FILE DOES NOT DO
// ============================================================================
// It promotes no `??_7` row. Registering a slot body makes the row
// COMPLETE-able, and COMPLETE is the STATIC half of readiness only
// (reference_a_green_readiness_screen_is_not_a_route.md). On this file ALONE,
// `??_7Goal@Achievement@@6B@` and `??_7GoalInstance@Achievement@@6B@` reach
// 7/7; `??_7Achievement@0@6B@` reaches 6/9 and `??_7AchievementInstance@` 3/4.
// The promotability verdict for each row is in WAVE27_LANE_ACHIEVE.md §5.
//
// KILL SWITCH: BANISHED_RT_ACHIEVEMENT=0 (whole namespace) or the per-class
// BANISHED_RT_GOAL / BANISHED_RT_ACHMGR, exactly as the rest of this directory.
// Registration is split across the two so the class-atomicity rule (PLAN §4.1
// decision 5) still holds: Goal/GoalInstance slots flip with goal.cpp,
// Achievement/AchievementInstance slots flip with achievement_manager.cpp.
// ============================================================================

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/achievement/Goal.h"

extern "C" uint32_t    RtAch_Achievement_Type(const void*);
extern "C" const char* RtAch_Achievement_TypeName(const void*);
extern "C" uint32_t    RtAch_AchievementInstance_Type(const void*);
extern "C" const char* RtAch_AchievementInstance_TypeName(const void*);
extern "C" uint32_t    RtAch_Goal_Type(const void*);
extern "C" const char* RtAch_Goal_TypeName(const void*);
extern "C" int32_t     RtAch_Goal_GetFileVersion(const void*);
extern "C" uint32_t    RtAch_GoalInstance_Type(const void*);
extern "C" const char* RtAch_GoalInstance_TypeName(const void*);
extern "C" void        RtAch_GoalInstance_Startup(void*);
extern "C" int32_t     RtAch_GoalInstance_CheckGoalState(const void*);

namespace {

// BaseTypeInfo's first two fields, and the ONLY two any body here touches:
//     +0x00  uint32_t     typeId
//     +0x08  const char*  name
// Read with memcpy rather than a struct cast so no alignment or
// strict-aliasing assumption is made about storage that may be ours or the
// DLL's depending on whether the `?_typeInfo@` row is promoted.
constexpr size_t kTypeInfoTypeIdOffset = 0x00;
constexpr size_t kTypeInfoNameOffset   = 0x08;
static_assert(kTypeInfoTypeIdOffset == 0, "BaseTypeInfo::typeId is at +0x00");
static_assert(kTypeInfoNameOffset == 8,   "BaseTypeInfo::name is at +0x08");
static_assert(sizeof(uint32_t) == 4,      "typeId is a DWORD (MOV EAX, not RAX)");
static_assert(sizeof(const char*) == 8,   "name is a QWORD (MOV RAX)");

// `??_7Goal@Achievement@@6B@` slot 8. Read out of the RETAIL bytes at
// 0x18009f067 (`JMP qword ptr [RAX + 0x40]`) and NOT carried over from the
// profile build, whose vtable offsets are known to differ
// (reference_encoding_vtable_offsets_differ_between_builds.md). Goal.h's
// `GoalVtbl` namespace deliberately lists only the slots ITS lane dispatched
// through, so this one is pinned here, beside the instruction it came from.
constexpr int kGoalVtblCheckGoalState = 0x40;
static_assert(kGoalVtblCheckGoalState / 8 == 8,
              "Goal vtable +0x40 is slot 8 (purecall in retail; EXE subclasses override)");
static_assert(kGoalVtblCheckGoalState < RtAchievement::GoalVtbl::kGetInstanceType,
              "slot 8 precedes GetInstanceType at +0x48, which Goal.h already pinned");

const uint8_t* AchievementTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@Achievement@1@0VBaseTypeInfo@Object@@A");
}
const uint8_t* AchievementInstanceTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@AchievementInstance@Achievement@@0VBaseTypeInfo@Object@@A");
}
const uint8_t* GoalTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@Goal@Achievement@@0VBaseTypeInfo@Object@@A");
}
const uint8_t* GoalInstanceTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@GoalInstance@Achievement@@0VBaseTypeInfo@Object@@A");
}

typedef uint32_t    (*U32ThisFn)(const void*);
typedef const char* (*CStrThisFn)(const void*);
typedef int32_t     (*I32ThisFn)(const void*);

uint32_t ReadTypeId(const uint8_t* node) {
    uint32_t v = 0;
    std::memcpy(&v, node + kTypeInfoTypeIdOffset, sizeof v);
    return v;
}
const char* ReadTypeName(const uint8_t* node) {
    const char* v = nullptr;
    std::memcpy(&v, node + kTypeInfoNameOffset, sizeof v);
    return v;
}

}  // namespace

// ---- ??_7Achievement@0@6B@  slots 2 and 1 ----------------------------------

extern "C" uint32_t RtAch_Achievement_Type(const void* self) {
    RT_DIFF_BEGIN("Achievement::Achievement::Type");
    const uint32_t ours = ReadTypeId(AchievementTypeInfo());
    RT_DIFF_SCALAR(ours, RT_ORIGINAL_ONCE(U32ThisFn,
                       "?Type@Achievement@1@UEBAIXZ")(self));
    RT_DIFF_WORK(true);
    return ours;
}

extern "C" const char* RtAch_Achievement_TypeName(const void* self) {
    RT_DIFF_BEGIN("Achievement::Achievement::TypeName");
    const char* const ours = ReadTypeName(AchievementTypeInfo());
    RT_DIFF_SCALAR(static_cast<const void*>(ours),
                   static_cast<const void*>(
                       RT_ORIGINAL_ONCE(CStrThisFn,
                           "?TypeName@Achievement@1@UEBAPEBDXZ")(self)));
    RT_DIFF_WORK(true);
    return ours;
}

// ---- ??_7AchievementInstance@Achievement@@6B@  slots 2 and 1 ---------------

extern "C" uint32_t RtAch_AchievementInstance_Type(const void* self) {
    RT_DIFF_BEGIN("Achievement::AchievementInstance::Type");
    const uint32_t ours = ReadTypeId(AchievementInstanceTypeInfo());
    RT_DIFF_SCALAR(ours, RT_ORIGINAL_ONCE(U32ThisFn,
                       "?Type@AchievementInstance@Achievement@@UEBAIXZ")(self));
    RT_DIFF_WORK(true);
    return ours;
}

extern "C" const char* RtAch_AchievementInstance_TypeName(const void* self) {
    RT_DIFF_BEGIN("Achievement::AchievementInstance::TypeName");
    const char* const ours = ReadTypeName(AchievementInstanceTypeInfo());
    RT_DIFF_SCALAR(static_cast<const void*>(ours),
                   static_cast<const void*>(
                       RT_ORIGINAL_ONCE(CStrThisFn,
                           "?TypeName@AchievementInstance@Achievement@@UEBAPEBDXZ")(self)));
    RT_DIFF_WORK(true);
    return ours;
}

// ---- ??_7Goal@Achievement@@6B@  slots 2, 1 and 3 ---------------------------

extern "C" uint32_t RtAch_Goal_Type(const void* self) {
    RT_DIFF_BEGIN("Achievement::Goal::Type");
    const uint32_t ours = ReadTypeId(GoalTypeInfo());
    RT_DIFF_SCALAR(ours, RT_ORIGINAL_ONCE(U32ThisFn,
                       "?Type@Goal@Achievement@@UEBAIXZ")(self));
    RT_DIFF_WORK(true);
    return ours;
}

extern "C" const char* RtAch_Goal_TypeName(const void* self) {
    RT_DIFF_BEGIN("Achievement::Goal::TypeName");
    const char* const ours = ReadTypeName(GoalTypeInfo());
    RT_DIFF_SCALAR(static_cast<const void*>(ours),
                   static_cast<const void*>(
                       RT_ORIGINAL_ONCE(CStrThisFn,
                           "?TypeName@Goal@Achievement@@UEBAPEBDXZ")(self)));
    RT_DIFF_WORK(true);
    return ours;
}

// `MOV EAX,0x10000 ; RET`. A literal, not a read: the on-disk serialised format
// version of an Achievement::Goal, and it is 65536, NOT the 4 / 0x1000 that
// Graphics::MaterialInstance and Graphics::Particle return. Each of these
// constants was decoded separately, and this one is written as its own literal
// for that reason. Faithful means returning 0x10000, NOT deriving it from
// anything in our tree — a derived spelling would silently track OUR format if
// Goal::Serialize is ever revised, and retail's does not.
extern "C" int32_t RtAch_Goal_GetFileVersion(const void* self) {
    RT_DIFF_BEGIN("Achievement::Goal::GetFileVersion");
    const int32_t ours = 0x10000;
    RT_DIFF_SCALAR(ours, RT_ORIGINAL_ONCE(I32ThisFn,
                       "?GetFileVersion@Goal@Achievement@@UEBAHXZ")(self));
    RT_DIFF_WORK(true);
    return ours;
}

// ---- ??_7GoalInstance@Achievement@@6B@  slots 2, 1, 4 and 6 ----------------

extern "C" uint32_t RtAch_GoalInstance_Type(const void* self) {
    RT_DIFF_BEGIN("Achievement::GoalInstance::Type");
    const uint32_t ours = ReadTypeId(GoalInstanceTypeInfo());
    RT_DIFF_SCALAR(ours, RT_ORIGINAL_ONCE(U32ThisFn,
                       "?Type@GoalInstance@Achievement@@UEBAIXZ")(self));
    RT_DIFF_WORK(true);
    return ours;
}

extern "C" const char* RtAch_GoalInstance_TypeName(const void* self) {
    RT_DIFF_BEGIN("Achievement::GoalInstance::TypeName");
    const char* const ours = ReadTypeName(GoalInstanceTypeInfo());
    RT_DIFF_SCALAR(static_cast<const void*>(ours),
                   static_cast<const void*>(
                       RT_ORIGINAL_ONCE(CStrThisFn,
                           "?TypeName@GoalInstance@Achievement@@UEBAPEBDXZ")(self)));
    RT_DIFF_WORK(true);
    return ours;
}

// `RET`. Retail's body is one byte and does nothing at all — it does not read
// `this`, does not touch the flags word, does not call the Goal. The base
// class's Startup hook is a no-op that the EXE's GoalInstance subclasses
// override. No diff probe: there is no value, no argument read and no side
// effect for an oracle to compare.
extern "C" void RtAch_GoalInstance_Startup(void* /*self*/) {
}

// `MOV RCX,[RCX+0x18] ; MOV RAX,[RCX] ; JMP [RAX+0x40]`.
//
// Reads GoalInstance::_goal (+0x18, an ExternalPtr<Goal> whose sole member is
// the raw pointer) and tail-calls that object's vtable slot 8. Slot 8 is
// purecall on `??_7Goal@Achievement@@6B@` itself, so the callee is always an
// EXE subclass override — see this file's banner.
//
// The deref is UNGUARDED and stays that way: retail does not test _goal.
//
// No diff probe. This body has no computation of its own to compare — it is one
// dispatch — and calling the DLL's copy as an oracle would run the EXE
// subclass's CheckGoalState a SECOND time on the same object. Those overrides
// read live town state; re-running one is a behaviour change, not an
// observation.
extern "C" int32_t RtAch_GoalInstance_CheckGoalState(const void* self) {
    const RtAchievement::GoalInstance* gi =
        static_cast<const RtAchievement::GoalInstance*>(self);
    void* goal = gi->_goal.ptr;
    void* const* vtable = *static_cast<void* const* const*>(goal);
    using CheckFn = int32_t (*)(void*);
    return reinterpret_cast<CheckFn>(vtable[kGoalVtblCheckGoalState / 8])(goal);
}

// ============================================================================
// REGISTRATION — by NAME and by ADDRESS, both, for every body.
//
// A body PORTED but never REGISTERED is indistinguishable from an ABSENT one to
// the census (reference_half_registered_port_reads_as_absent.md), and the mint
// fills a slot by NAME for RT_MINT_NAMED and by ADDRESS otherwise. All eleven
// of these slots are RT_MINT_NAMED; the address door is registered anyway so a
// by-address call site (ResolveAddr consults the port registry FIRST —
// reference_resolveaddr_consults_port_registry_first.md) reaches the same body.
//
// TWO register functions, not one, because the two halves sit on DIFFERENT kill
// switches (class atomicity, PLAN §4.1 decision 5).
// ============================================================================

// Goal + GoalInstance — flips with BANISHED_RT_GOAL, beside goal.cpp.
int RtAchGoalTypeInfoAccessorsRegister() {
    int n = 0;

    RegisterRuntimeExport("?Type@Goal@Achievement@@UEBAIXZ",
                          (void*)&RtAch_Goal_Type); ++n;
    RegisterRuntimeAddr(0x180001b40ull, (void*)&RtAch_Goal_Type);

    RegisterRuntimeExport("?TypeName@Goal@Achievement@@UEBAPEBDXZ",
                          (void*)&RtAch_Goal_TypeName); ++n;
    RegisterRuntimeAddr(0x180001b60ull, (void*)&RtAch_Goal_TypeName);

    RegisterRuntimeExport("?GetFileVersion@Goal@Achievement@@UEBAHXZ",
                          (void*)&RtAch_Goal_GetFileVersion); ++n;
    RegisterRuntimeAddr(0x18009ee10ull, (void*)&RtAch_Goal_GetFileVersion);

    RegisterRuntimeExport("?Type@GoalInstance@Achievement@@UEBAIXZ",
                          (void*)&RtAch_GoalInstance_Type); ++n;
    RegisterRuntimeAddr(0x180001e40ull, (void*)&RtAch_GoalInstance_Type);

    RegisterRuntimeExport("?TypeName@GoalInstance@Achievement@@UEBAPEBDXZ",
                          (void*)&RtAch_GoalInstance_TypeName); ++n;
    RegisterRuntimeAddr(0x180001e60ull, (void*)&RtAch_GoalInstance_TypeName);

    RegisterRuntimeExport("?Startup@GoalInstance@Achievement@@UEAAXXZ",
                          (void*)&RtAch_GoalInstance_Startup); ++n;
    RegisterRuntimeAddr(0x18009f000ull, (void*)&RtAch_GoalInstance_Startup);

    RegisterRuntimeExport("?CheckGoalState@GoalInstance@Achievement@@UEBA?AW4GoalState@Goal@2@XZ",
                          (void*)&RtAch_GoalInstance_CheckGoalState); ++n;
    RegisterRuntimeAddr(0x18009f060ull, (void*)&RtAch_GoalInstance_CheckGoalState);

    return n;
}

// Achievement + AchievementInstance — flips with BANISHED_RT_ACHMGR, beside
// achievement_manager.cpp.
int RtAchManagerTypeInfoAccessorsRegister() {
    int n = 0;

    RegisterRuntimeExport("?Type@Achievement@1@UEBAIXZ",
                          (void*)&RtAch_Achievement_Type); ++n;
    RegisterRuntimeAddr(0x180001020ull, (void*)&RtAch_Achievement_Type);

    RegisterRuntimeExport("?TypeName@Achievement@1@UEBAPEBDXZ",
                          (void*)&RtAch_Achievement_TypeName); ++n;
    RegisterRuntimeAddr(0x180001040ull, (void*)&RtAch_Achievement_TypeName);

    RegisterRuntimeExport("?Type@AchievementInstance@Achievement@@UEBAIXZ",
                          (void*)&RtAch_AchievementInstance_Type); ++n;
    RegisterRuntimeAddr(0x1800017a0ull, (void*)&RtAch_AchievementInstance_Type);

    RegisterRuntimeExport("?TypeName@AchievementInstance@Achievement@@UEBAPEBDXZ",
                          (void*)&RtAch_AchievementInstance_TypeName); ++n;
    RegisterRuntimeAddr(0x1800017c0ull, (void*)&RtAch_AchievementInstance_TypeName);

    return n;
}
