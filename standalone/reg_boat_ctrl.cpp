// standalone/reg_boat_ctrl.cpp
// Address registration for BoatComponent's state machine (layer 4 of the trade wave).
//
// These bodies are reached through the state TABLE, not by address — the dispatcher in
// controller_state_table.h calls them directly out of BoatDispatch::table, exactly as
// retail's 0x1401c9150 entries do. Registering them anyway is what makes the addresses
// resolvable for anything that later binds by address, and it is how the tree records
// "this retail function now has a body" — a bare table entry leaves ResolveAddr
// answering NULL and the function reading as unported.
//
// NOT registered, deliberately:
//   0x1400468a0  the retail table BUILD. It appends into the exe's own .data array at
//                0x1401c9150/0x1401ca0e0, which standalone does not map;
//                BoatCtrl_PopulateTable() fills our Dispatcher<BoatCtrlTag>::table
//                instead. Same three primitives (0x1400039d0/3ae0/3b90), different
//                storage — registering the builder would hand callers a function that
//                writes into nothing.
//   0x1400039d0 / 0x140003ae0 / 0x140003b90  those primitives, for the same reason.

#include "resolve.h"
#include <cstdint>

extern "C" {
char BoatCtrl_State0_Enter(void*);                        // 0x1400453f0
char BoatCtrl_State0_Update(void*);                       // 0x140045190
char BoatCtrl_State0_Tick(void*, void*, void*);           // 0x140045250
char BoatCtrl_State1_Enter(void*);                        // 0x140045150
char BoatCtrl_State1_Tick(void*, void*, void*);           // 0x140044ff0
char BoatCtrl_State2_Enter(void*);                        // 0x140044f70
char BoatCtrl_State2_Event0(void*, void*);                // 0x140044f00
char BoatCtrl_Event1_Abort(void*, void*);                 // 0x140045670
char BoatCtrl_State3_Enter(void*);                        // 0x140044ec0
char BoatCtrl_State3_Tick(void*, void*, void*);           // 0x140044d10
char BoatCtrl_State4_Enter(void*);                        // 0x140044cd0
char BoatCtrl_State4_Tick(void*, void*, void*);           // 0x140044c40
int  BoatCtrl_NextRoutePoint(void*, const float*, float); // 0x1400a70c0
void BoatCtrl_DriveToward(void*, const float*, float, float);  // 0x14010a0e0
}

// ── .rdata float constants the boat state machine reads ──────────────────────────
// MEASURED 2026-08-22: without these the machine runs, steers and sails the whole
// route — and never docks. Every one of them sits behind an `if (s_fX && ...)` guard,
// so an unresolved constant does not crash or log, it silently DELETES the test:
//   0x1401721f4 null => the state 0 -> 1 arrival test never fires. Boat 1 passed within
//                       6.32 units of the dock (gate is 8.0), kept going through the
//                       out-leg, ran out of route at index 983 and scrapped itself.
//   0x14017219c null => next-waypoint spacing 0, so 0x1400a70c0 returns cur+1 every
//                       time and the boat crawls all 987 points one at a time instead
//                       of skipping to the next point >= 6 units away.
//   0x1401720fc null => the state 1 -> 2 dock test never fires either.
//   0x14017210c null => the state-1 speed clamp loses its lower bound.
// See [[reference-registered-but-filled-with-zero]] — the same shape, one level down:
// here it is the ADDRESS that never resolved, so the null-guard ate the branch.
//
// 0x140172180 (3.0) and 0x14017214c (1.0) are already registered elsewhere
// (reg_citizenai_handlers.cpp / reg_citizenai_actionctors.cpp), and 0x1401720ec (0.125)
// in reg_component_gap_A.cpp — which is exactly why this was hard to see: the waypoint
// advance worked, so the boat visibly sailed and looked healthy.
static const float k_rdata_17219c = 6.0f;    // 0x40C00000  next-waypoint spacing
static const float k_rdata_1721f4 = 64.0f;   // 0x42800000  state 0->1 gate, SQUARED (8.0)
static const float k_rdata_1720fc = 0.2f;    // 0x3E4CCCCD  state 1->2 dock range
static const float k_rdata_17210c = 0.33f;   // 0x3EA8F5C3  state-1 speed clamp, low

void RegisterBoatCtrlAddrs() {
    // 0x14017219c (6.0f) is registered by gameupdateloop_load.cpp (s_fAudioTimeScale),
    // which runs later. Same value; duplicate removed 2026-08-30.
    RegisterExeAddr(0x1401721f4, reinterpret_cast<uintptr_t>(&k_rdata_1721f4));
    RegisterExeAddr(0x1401720fc, reinterpret_cast<uintptr_t>(&k_rdata_1720fc));
    RegisterExeAddr(0x14017210c, reinterpret_cast<uintptr_t>(&k_rdata_17210c));

}
