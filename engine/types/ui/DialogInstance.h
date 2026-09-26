#pragma once
// UI::DialogInstance, engine-side name. ONE definition of the layout lives in
// runtime/types/ui/DialogInstance.h (RULE B6-4.3, ENDGAME B.3.6 block B).
//
// This aliases the runtime's `RtUI::DialogInstance` POD, NOT the abstract
// `UI::DialogInstance` class: the 20 exe dialog structs in Dialogs.h embed
// `DialogInstance base;` BY VALUE, and a member of abstract type is
// ill-formed. The POD and the class are cross-asserted field for field in the
// runtime header (sizeof 0xb8; +0x30 is `_dataSource`, +0xb0 `_userFlags`).
#include "runtime/types/ui/DialogInstance.h"
using DialogInstance = RtUI::DialogInstance;
