// standalone/reg_eventlog_citizen.cpp
// Citizen life-event message chain — closes the long-standing "AssignState"
// Level-2 gap (0x140113af0, misnomer fixed in Ghidra 2026-07-19: it posts an
// event-log message, assigns nothing).
//
// addr          symbol                            one-liner
// -----------   -------------------------------   ---------------------------------
// 0x140113af0   CitizenComponent_PostLifeEvent    title pick + post to event log
// 0x140094e00   EventLog_AddCitizenEvent          5-arg FormatIndexed + entry append
// 0x140095030   EventLog_TileIndex                entity IBP -> map tile idx (or -1)
//
// Bodies: code/Game/Component/citizen_lifeevent.cpp, code/Game/Game/eventlog.cpp.
// Callers that now resolve instead of silently no-op: CitizenComponent_Update's
// became-adult branch (citizencomponent.cpp), the msg-4 state-9 assign
// (citizen_needsupdate.cpp), and the ported message handler's type-8 post
// (citizencomponent.cpp step-2 block). The event-log display side
// (EventNotifyDialog/EventPopupDialog) already reads gw+0x138, so posted
// messages render without further wiring.
#include "resolve.h"
#include <cstdint>

extern "C" void CitizenComponent_PostLifeEvent(void* self, int eventType, int severity,
                                               const char* strParam);          // 0x140113af0
extern "C" void EventLog_AddCitizenEvent(void* eventLog, int eventType, int severity,
                                         void* entityIBP, void* nameStr, void* titleStr,
                                         void* extraStr);                      // 0x140094e00
extern "C" int  EventLog_TileIndex_Reg(void* unused, void* entityIBP);         // 0x140095030 (RCX dead in original)

extern "C" void RegisterEventLogCitizen() {
    RegisterExeAddr(0x140113af0, reinterpret_cast<uintptr_t>(&CitizenComponent_PostLifeEvent));
    RegisterExeAddr(0x140094e00, reinterpret_cast<uintptr_t>(&EventLog_AddCitizenEvent));
    RegisterExeAddr(0x140095030, reinterpret_cast<uintptr_t>(&EventLog_TileIndex_Reg));
}
