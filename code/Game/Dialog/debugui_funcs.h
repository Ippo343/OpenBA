#pragma once
// DebugUI C bodies (defined in debugui.cpp). Split out of ui_panels.h (B.4 R0-EXE).

#include "Dialog/DebugUI.h"
#include <cstdint>

// B4 FORWARD: 1 outside caller(s) (standalone/disaster_harness.cpp) -- the body is
// DebugUI::OnElementEvent (0x140060bc0).
void DebugUI_Slot9(DebugUI* self, Object::InternalPtr<UI::Element>* element, int param3, int param4);  // B8: stays free -- harness / port-only: its one caller, standalone/disaster_harness.cpp:436, passes a NULL DebugUI (case 329 reads no DebugUI field); a member call on a literal null is UB and a new -Wnonnull warning
