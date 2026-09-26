#pragma once
// AutoPickupWork / Work — ENDGAME B.3.8 block WRK: a FORWARDER (B8-4).
//
// The two POD structs that lived here (`Work`, 0x48, and `AutoPickupWork : Work`)
// are now the real classes code/Game/Work/Work.h and code/Game/Work/AutoPickupWork.h
// (one definition per name). The address constants moved with them
// (`WorkAddr`, `AutoPickupWorkAddr`).

#include "Work/AutoPickupWork.h"
