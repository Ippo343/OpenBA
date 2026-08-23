// standalone/reg_fire.cpp
// Address-registry wave for the FIRE DISASTER lane.
//
// Everything here is ported in:
//   code/Game/Component/firecomponent.cpp          Update (slot 9), Serialize (slot 3),
//                                                  the live vtable + factory
//   code/Game/Component/firecomponent_ignite.cpp   Ignite / Spread / Extinguish /
//                                                  slot 15 / danger rect / audio /
//                                                  particle fade-out / FireWork ctor
//   code/Game/Component/firecomponent_smstate.cpp  the two SM force helpers + the
//                                                  demolition gate
//
// ── TWO TYPE-ID GLOBALS ARE THE WHOLE STORY ──────────────────────────────────
// 0x1401c4218  FireDescription BaseTypeInfo. Every FireComponent body validates
//              its +0x18 description against it. Unregistered, ResolveAddr returns
//              0, IsDerivedFrom never matches, and the ignition roll was silently
//              unreachable.
// 0x1401c41e0  FireComponent type id. This is the MESSAGE ID the bucket-brigade
//              action queue carries (work_act_perform.cpp:303). Unregistered, that
//              read fell back to 0, SendComponentMessage::AbstractImpl matched no
//              component, and every bucket of water went nowhere — the same failure
//              mode action_pf_ctors.cpp:354 already documents for the eat path.
// Both are filled AFTER Engine::Startup (Object::Factory::Startup only assigns the
// real CRC ids then) by FillTypeIdGlobals_fire(), called from game_main.cpp.
//
// ── SHARED WITH THE TORNADO LANE ─────────────────────────────────────────────
// 0x14011e690 / 0x14011e7b0 / 0x14011ea10 are registered HERE and only here.
// tornadocomponent.cpp:261-263 resolves all three through ResolveAddr and
// reg_tornado.cpp:17 explicitly declines to register them ("the FIRE lane owns
// these three"), so both lanes dispatch one body. No function is ported twice.
//
// Also registered: 0x1401722c0, the 0x80000000 sign-flip mask. (0x140172234 =
// 300.0f and 0x140172114 = 0.5f are already registered with their real values by
// reg_citizen_needsupdate.cpp — both verified against the image bytes.)

#include "resolve.h"
#include "Object/factory_registration.h"
#include <cstdint>

// ---- ported bodies ----
extern "C" void  FireComponent_Ignite_610(void* self);                          // 0x140049610
extern "C" void  FireComponent_Spread_130(void* self);                          // 0x140049130
extern "C" void  FireComponent_Extinguish_4c0(void* self);                      // 0x1400494c0
extern "C" void  FireComponent_WaterApplied_090(void* self, int msg);           // 0x140049090
extern "C" void  FireComp_MarkDanger_99c0(void* self, short delta);             // 0x1400499c0
extern "C" void  FireComp_StampDangerRect_9fa80(int*, uint64_t, int, int, int, int, short);
extern "C" void  FireComp_StartAudioEmitter_42c60(void* ambientComp, int idx);  // 0x140042c60
extern "C" void* FireWork_CtorFull_e9bd0(void* mem, uint64_t* pEntityIBP, uint32_t cap);
extern "C" bool  FireComponent_IsBeingDemolished_11e690(void* destroyComp);     // 0x14011e690
extern "C" bool  FireComponent_BeginBurning_11e7b0(void* destroyComp, const int64_t* t);
extern "C" void  FireComponent_StopBurning_11ea10(void* destroyComp);           // 0x14011ea10
extern "C" void* Entity_FindComponent_Fire_5840(uintptr_t entity, void* retIBP);    // 0x140005840
extern "C" void* Entity_FindComponent_Citizen_9790(uintptr_t entity, void* retIBP); // 0x140009790

// ---- type-id global storage (values filled by FillTypeIdGlobals_fire) ----
extern uint32_t g_tid_FireDescription;   // 0x1401c4218
extern uint32_t g_tid_FireComponent;     // 0x1401c41e0

// DAT_1401722c0 — the float sign-flip mask (two qwords of 0x80000000 in the image;
// only the first dword is read).
static const uint32_t g_DAT_1401722c0 = 0x80000000u;

void RegisterFireBatch() {
    // --- FireComponent leaf bodies ---
    RegisterExeAddr(0x140049610, reinterpret_cast<uintptr_t>(&FireComponent_Ignite_610));
    RegisterExeAddr(0x140049130, reinterpret_cast<uintptr_t>(&FireComponent_Spread_130));
    RegisterExeAddr(0x1400494c0, reinterpret_cast<uintptr_t>(&FireComponent_Extinguish_4c0));
    RegisterExeAddr(0x140049090, reinterpret_cast<uintptr_t>(&FireComponent_WaterApplied_090));
    RegisterExeAddr(0x1400499c0, reinterpret_cast<uintptr_t>(&FireComp_MarkDanger_99c0));
    RegisterExeAddr(0x14009fa80, reinterpret_cast<uintptr_t>(&FireComp_StampDangerRect_9fa80));

    // --- audio arm ---
    // 0x140053720 and 0x140042960 are deliberately NOT here: the TORNADO lane
    // registers both (reg_tornado.cpp:80-81) and this lane resolves them. One body
    // per retail function.
    RegisterExeAddr(0x140042c60, reinterpret_cast<uintptr_t>(&FireComp_StartAudioEmitter_42c60));

    // --- the FireWork producer (Ignite is its only caller) ---
    RegisterExeAddr(0x1400e9bd0, reinterpret_cast<uintptr_t>(&FireWork_CtorFull_e9bd0));

    // --- shared with the TORNADO lane; registered once, here ---
    RegisterExeAddr(0x14011e690, reinterpret_cast<uintptr_t>(&FireComponent_IsBeingDemolished_11e690));
    RegisterExeAddr(0x14011e7b0, reinterpret_cast<uintptr_t>(&FireComponent_BeginBurning_11e7b0));
    RegisterExeAddr(0x14011ea10, reinterpret_cast<uintptr_t>(&FireComponent_StopBurning_11ea10));

    // --- THE TWO TYPE-ID GLOBALS (breaks #3 and #4) ---
    RegisterExeAddr(0x1401c4218, reinterpret_cast<uintptr_t>(&g_tid_FireDescription));
    RegisterExeAddr(0x1401c41e0, reinterpret_cast<uintptr_t>(&g_tid_FireComponent));

    // --- THE TWO SHARED FINDERS (code/Game/Component/entity_finders_disaster.cpp) ---
    // 0x140005840 FindComponent<FireComponent> was registered NOWHERE until
    // 2026-08-23. TornadoComponent's building arm is gated on `buildComp && fireComp`
    // and resolves this address for the second half, so with it null the arm was
    // unreachable — a tornado could cross a town and ignite nothing, reporting
    // `building=0` exactly like a tornado that crossed empty forest. 0x140009790
    // FindComponent<CitizenComponent> killed the tornado's citizen arm the same way.
    // Both type-id globals were already registered and filled; only the finders were
    // missing. Registered here rather than in reg_tornado.cpp because the fire lane
    // owns the FireComponent half and one file should own both.
    RegisterExeAddr(0x140005840, reinterpret_cast<uintptr_t>(&Entity_FindComponent_Fire_5840));
    RegisterExeAddr(0x140009790, reinterpret_cast<uintptr_t>(&Entity_FindComponent_Citizen_9790));

    // --- .rdata const ---
    RegisterExeAddr(0x1401722c0, reinterpret_cast<uintptr_t>(&g_DAT_1401722c0));
}
