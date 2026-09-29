#pragma once
// The panel controller classes (ui/Panels.h) and their DialogController base.
//
// B.4 R0-EXE: the C-body declarations that stood here moved to one header per
// class: cropui_funcs.h, resourcelimitui_funcs.h, entityui_funcs.h, tradeui_funcs.h,
// noiseui_funcs.h, spritecaptureui_funcs.h, debugui_funcs.h. The 18 empty inline
// slot bodies went with B.4 R0-EXE's unreferenced-body deletion (each class
// declares its own empty override).

#include "ui/Panels.h"
#include "ui/DialogController.h"

// standalone/disaster_harness.cpp reaches DebugUI_Slot9 through this header.
#include "Dialog/debugui_funcs.h"
