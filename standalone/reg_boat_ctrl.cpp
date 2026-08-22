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

void RegisterBoatCtrlAddrs() {
    RegisterExeAddr(0x1400453f0, reinterpret_cast<uintptr_t>(&BoatCtrl_State0_Enter));
    RegisterExeAddr(0x140045190, reinterpret_cast<uintptr_t>(&BoatCtrl_State0_Update));
    RegisterExeAddr(0x140045250, reinterpret_cast<uintptr_t>(&BoatCtrl_State0_Tick));
    RegisterExeAddr(0x140045150, reinterpret_cast<uintptr_t>(&BoatCtrl_State1_Enter));
    RegisterExeAddr(0x140044ff0, reinterpret_cast<uintptr_t>(&BoatCtrl_State1_Tick));
    RegisterExeAddr(0x140044f70, reinterpret_cast<uintptr_t>(&BoatCtrl_State2_Enter));
    RegisterExeAddr(0x140044f00, reinterpret_cast<uintptr_t>(&BoatCtrl_State2_Event0));
    RegisterExeAddr(0x140045670, reinterpret_cast<uintptr_t>(&BoatCtrl_Event1_Abort));
    RegisterExeAddr(0x140044ec0, reinterpret_cast<uintptr_t>(&BoatCtrl_State3_Enter));
    RegisterExeAddr(0x140044d10, reinterpret_cast<uintptr_t>(&BoatCtrl_State3_Tick));
    RegisterExeAddr(0x140044cd0, reinterpret_cast<uintptr_t>(&BoatCtrl_State4_Enter));
    RegisterExeAddr(0x140044c40, reinterpret_cast<uintptr_t>(&BoatCtrl_State4_Tick));
    RegisterExeAddr(0x1400a70c0, reinterpret_cast<uintptr_t>(&BoatCtrl_NextRoutePoint));
    RegisterExeAddr(0x14010a0e0, reinterpret_cast<uintptr_t>(&BoatCtrl_DriveToward));
}
