// code/Game/Dialog/modbrowseui.cpp
// Clean reimplementation of ModBrowseUI methods.
//
// ModBrowseUI (0x30 bytes): Mod browser panel.
// Extends DialogController (0x30 base). No subclass fields.
//
// Only non-type-system unhookable slot is Slot3 (Shutdown no-op).
// All other slots are either the Dtor (reimplemented in engine/reimpl/)
// or base class stubs.

#include "Dialog/ui_panels.h"
#include <cstdint>

// ============================================================
