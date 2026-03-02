#pragma once
// Function declarations for deeply reimplemented UI panel methods.
// These are clean game logic functions called from engine/reimpl/ wrappers.
// No dependency on hooks.h or resolve.h — portable to standalone exe.

#include "ui/Panels.h"
#include "ui/DialogController.h"

// ============================================================
// TitleDescToolTipUI
// ============================================================

// Serialize (vtable +0x20): Finds "textTitle" and "textDesc" elements in the
// parent dialog, reads user data from the controller's data source (+0x28),
// and sets them as text parameters on the respective elements.
void TitleDescToolTipUI_Serialize(TitleDescToolTipUI* self);
