#pragma once
// code/Game/Goal/exe_bodies.h -- declarations of the exe bodies DEFINED in
// code/Game/Goal/ (ENDGAME B.3R, block W0-P0c, B3R_RULES R5).
//
// Every body the registry reaches by retail address, declared once, copied
// from its DEFINITION (linkage included), grouped by defining file; the
// retail address(es) it is registered under are the trailing comment. The
// defining .cpp includes this header so the compiler checks each pair.
// Readers call these by name (W1); nothing here changes what runs. Making
// them members is B.4's work.
//
// Declared in another header already (not redeclared here):
//   StatGoal_ResolveExternalPtr            0x1400265b0  (code/Game/Goal/goal_exe_helpers.h)

#include <cstdint>
