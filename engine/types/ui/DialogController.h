#pragma once
// UI::DialogController, engine-side name. ONE definition of the layout lives
// in runtime/types/ui/DialogInstance.h (RULE B6-4.3, ENDGAME B.3.6 block B).
//
// This aliases the runtime's `RtUI::DialogController` POD, NOT the abstract
// `UI::DialogController` class (runtime/types/ui/DialogController.h). The 38
// exe panel structs that embedded it BY VALUE are gone (ENDGAME B.3.8 block PNL:
// every panel is a real subclass of the class); this alias and the POD retire at
// the END of B.3. POD and class are cross-asserted field for field (sizeof 0x30).
#include "runtime/types/ui/DialogInstance.h"
using DialogController = RtUI::DialogController;
