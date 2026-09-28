// runtime/src/system/debug.cpp — System::Debug (B3R W0-SM M2)
//
// The class's one defined member. See runtime/types/system/Debug.h for the
// exports (all declared, the rest unported) and the evidence for the cell.
#include "../../types/system/Debug.h"

// ?_disableHandler@Debug@System@@0_NA (0x1801fc708): BSS in retail, zero at
// load. Was the mint's storage row until B3R W0-SM M2.
bool System::Debug::_disableHandler = false;
