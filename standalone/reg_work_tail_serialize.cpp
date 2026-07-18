// standalone/reg_work_tail_serialize.cpp
// Phase 2 Keystone B tail — registration for the closure-safe Work "Serialize"
// (StateMachine Slot4 action-evaluation) functions.
//
// Despite the Ghidra name "*_Serialize", 0x1400f2370 is PastureWork::Slot4 — the
// per-tick action-queue builder for pasture work (NOT IO stream serialization). Its
// full clean-C++ reimplementation already lives in code/Game/Work/pasturework.cpp
// (PastureWork_Slot4_Impl, ~360 lines, faithful offset-by-offset port). This batch
// only REGISTERS the exe address so standalone dispatch reaches our code, plus fills
// the three type-id .rdata globals the body reads through IsDerivedFrom.
//
// CLOSURE: every exe sub-callee actually invoked by PastureWork_Slot4_Impl is already
// registered (verified against the 0x1400f2370 CALL list):
//   e3db0 003050 004fe0 12e5c0 002f90 12c3f0 005500 0012d0 09ccf0 0a00f0 09f3e0
//   bbea0 002ed0 123070 122e20 0223c0 0f8620 004680 0bb0a0 021a30 128350 014c30 c3470
// The standard infra slots (CheckPtr 0x140152ab0, IsDerivedFrom 0x140152a98, Vec3 ctor
// 0x140152a30, MemAlloc/Free static) resolve through the IAT slot mechanism as in every
// other landed Work/Action file.
//
// NOTE: PastureWork_ResolveFuncs() also resolves 0x140115e60 (CitizenAI_State_LongTermPlan)
// into s_fn_CitizenAI_LongTermPlan and gates it in its `ok` check, BUT 0x1400f2370 NEVER
// calls it (not in the CALL list — it is a copy-paste leftover from a sibling Work file).
// It is therefore left null/unregistered: harmless, never dereferenced on this path. The
// `ok` gate is a DLL-mode install guard only; PastureWork_Slot4_Impl does not consult it.
//
// Type-id globals (BaseTypeInfo written by *_Register; first DWORD = the CRC type-id read
// by IsDerivedFrom). Filled via GetTypeIdByName AFTER Engine::Startup (the real CRC id is
// assigned only then). CLASS NAMES VERIFIED from the BaseTypeInfo ctor TypeName arg in
// Ghidra (the inline comments in pasturework.cpp were mislabeled):
//   0x1401c7bd8 = "PastureDescription"  (PastureDescription_Register @14014f635)
//   0x1401c6fd0 = "AnimalDescription"   (AnimalDescription_Register  @14014e575)
//   0x1401c7ba0 = "PastureComponent"    (PastureComponent_RegisterType @14014f5f5)
//
// 0x1400f2f00 (Pickup::Slot4/Execute) is intentionally NOT here — it is reimplemented
// as Pickup_Slot4_Impl and dispatched via the static Pickup vtable in
// code/Game/Work/pickup.cpp, so no address registration is needed.
#include "resolve.h"
#include "Object/factory_registration.h"   // Object::GetTypeIdByName
#include "Work/pasturework_funcs.h"        // PastureWork_Slot4_Impl
#include <cstdint>

// Type-id global storage (values filled later by FillTypeIdGlobals_worktail_serialize).
static uint32_t g_tid_PastureDescription = 0;  // 0x1401c7bd8
static uint32_t g_tid_AnimalDescription  = 0;  // 0x1401c6fd0
static uint32_t g_tid_PastureComponent   = 0;  // 0x1401c7ba0

extern "C" void FillTypeIdGlobals_worktail_serialize() {
    g_tid_PastureDescription = ::Object::GetTypeIdByName("PastureDescription");
    g_tid_AnimalDescription  = ::Object::GetTypeIdByName("AnimalDescription");
    g_tid_PastureComponent   = ::Object::GetTypeIdByName("PastureComponent");
}

void RegisterWorkTailSerializeBatch() {
    // PastureWork::Slot4 (0x1400f2370) -> clean reimpl in code/Game/Work/pasturework.cpp
    RegisterExeAddr(0x1400f2370, reinterpret_cast<uintptr_t>(&PastureWork_Slot4_Impl));

    // Type-id global storage (filled by FillTypeIdGlobals_worktail_serialize after Startup).
    RegisterExeAddr(0x1401c7bd8, reinterpret_cast<uintptr_t>(&g_tid_PastureDescription));
    RegisterExeAddr(0x1401c6fd0, reinterpret_cast<uintptr_t>(&g_tid_AnimalDescription));
    RegisterExeAddr(0x1401c7ba0, reinterpret_cast<uintptr_t>(&g_tid_PastureComponent));
}
