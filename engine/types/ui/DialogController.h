#pragma once
// UI::DialogController, engine-side name. ONE definition of the layout lives
// in runtime/types/ui/DialogInstance.h (RULE B6-4.3, ENDGAME B.3.6 block B).
//
// This aliases the runtime's `RtUI::DialogController` POD, NOT the abstract
// `UI::DialogController` class (runtime/types/ui/DialogController.h): the 38
// exe panel structs in Panels.h embed `DialogController base;` BY VALUE, and a
// member of abstract type is ill-formed. POD and class are cross-asserted
// field for field (sizeof 0x30).
#include "runtime/types/ui/DialogInstance.h"
using DialogController = RtUI::DialogController;
