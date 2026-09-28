// ============================================================================
// runtime/src/ui/rt_ui_port.cpp — RtUiPortInit()  Wave 7 (UI)
// ============================================================================
// Calls the UI register functions that still register a row somebody reads.
// B3R RT-UI: the other ninety register functions registered only rows with no
// reader by name or by address anywhere in the tree; they were deleted with
// those rows.
//
// See docs/runtime_port/WAVE7.md.
// ============================================================================

#include <cstdio>
#include "../../funcs/rt_port.h"

extern int RtUiElementRegister();          // element.cpp
extern int RtUiManagerRegister();          // userinterface.cpp
extern int RtUiDialogControllerRegister(); // dialog_controller_base.cpp
extern int RtUiStandAloneRegister();       // userinterface_standalone.cpp
extern int RtUiSpriteSheetNewRegister();   // spritesheet_new.cpp

void RtUiPortInit() {
    int n = 0;
    n += RtUiElementRegister();
    n += RtUiManagerRegister();
    n += RtUiDialogControllerRegister();
    n += RtUiStandAloneRegister();
    n += RtUiSpriteSheetNewRegister();
    printf("[RtPort] UI: %d exports registered\n", n);
}
