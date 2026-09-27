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
// one. `Achievement::Goal::GoalState` is `int32_t`-backed, per Goal.h.
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
// KILL SWITCH: RT_ACHIEVEMENT=0 (whole namespace) or the per-class
// RT_GOAL / RT_ACHMGR, exactly as the rest of this directory.
// Registration is split across the two so the class-atomicity rule (PLAN §4.1
// decision 5) still holds: Goal/GoalInstance slots flip with goal.cpp,
// Achievement/AchievementInstance slots flip with achievement_manager.cpp.
// ============================================================================

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/achievement/AchievementManager.h"

// ENDGAME B.3.7 block B3: the eleven bodies are MEMBERS of the four real
// classes now; each registered door is a one-line QUALIFIED delegation.

namespace {

// BaseTypeInfo's first two fields, and the ONLY two any body here touches:
//     +0x00  uint32_t     typeId
//     +0x08  const char*  name
// Read with memcpy so no alignment or aliasing assumption is made about
// storage that may be ours or the mint's.
constexpr size_t kTypeInfoTypeIdOffset = 0x00;
constexpr size_t kTypeInfoNameOffset   = 0x08;

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

namespace Achievement {

// ---- ??_7Achievement@0@6B@  slots 1 and 2 ----------------------------------
uint32_t    Achievement::TypeId()         { return ReadTypeId(AchievementTypeInfo()); }    // 0x180001030
const char* Achievement::TypeName() const { return ReadTypeName(AchievementTypeInfo()); }  // 0x180001040
uint32_t    Achievement::Type() const     { return TypeId(); }                             // 0x180001020

// ---- ??_7AchievementInstance@Achievement@@6B@  slots 1 and 2 ---------------
uint32_t    AchievementInstance::TypeId()         { return ReadTypeId(AchievementInstanceTypeInfo()); }    // 0x1800017b0
const char* AchievementInstance::TypeName() const { return ReadTypeName(AchievementInstanceTypeInfo()); }  // 0x1800017c0
uint32_t    AchievementInstance::Type() const     { return TypeId(); }                                     // 0x1800017a0

// ---- ??_7Goal@Achievement@@6B@  slots 1, 2 and 3 ---------------------------
// (ENDGAME B.3.9 ENG: TypeId() is retail's static accessor 0x180001b50, the
// same load; the minted record stays until the D6 fold.)
uint32_t    Goal::TypeId()         { return ReadTypeId(GoalTypeInfo()); }     // 0x180001b50
const char* Goal::TypeName() const { return ReadTypeName(GoalTypeInfo()); }   // 0x180001b60
uint32_t    Goal::Type() const     { return TypeId(); }                       // 0x180001b40

// `MOV EAX,0x10000 ; RET`. A literal: the on-disk format version of a Goal,
// 65536 — NOT derived from anything in our tree.
int32_t Goal::GetFileVersion() const { return 0x10000; }                      // 0x18009ee10

// ---- ??_7GoalInstance@Achievement@@6B@  slots 1, 2, 4 and 6 ----------------
uint32_t    GoalInstance::TypeId()         { return ReadTypeId(GoalInstanceTypeInfo()); }    // 0x180001e50
const char* GoalInstance::TypeName() const { return ReadTypeName(GoalInstanceTypeInfo()); }  // 0x180001e60
uint32_t    GoalInstance::Type() const     { return TypeId(); }                              // 0x180001e40

// `RET`. One byte; the exe subclasses override it.
void GoalInstance::Startup() {}                                               // 0x18009f000

// `MOV RCX,[RCX+0x18] ; MOV RAX,[RCX] ; JMP [RAX+0x40]` — Goal slot 8 on
// `_goal`, a VIRTUAL call (the callee is always an exe override: slot 8 is
// `_purecall` in `??_7Goal@Achievement@@6B@`). The deref of `_goal` is
// UNGUARDED in retail and stays so.
Goal::GoalState GoalInstance::CheckGoalState() const {                        // 0x18009f060
    return _goal->CheckGoalState();
}

}  // namespace Achievement

// ============================================================================
// ENDGAME B.3.9 ENG: all eleven slot doors of this file and both register
// functions are gone with the four minted Achievement `??_7` rows -- the
// tables were their only reader. The bodies above are the classes' members,
// reached through the compiler's tables (and TypeId() by the other files).
// ============================================================================
