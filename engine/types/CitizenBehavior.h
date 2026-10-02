#pragma once
// engine/types/CitizenBehavior.h — FORWARDER (ENDGAME B.4 W0-H1)
//
// `CitizenBehavior` is declared as a real class in code/Game/Entity/CitizenBehavior.h
// (every field, retail's types, asserted; with ProfessionResourceEntry,
// ResourceNotifyEntry, PopulationSummary and PathGraph). This header forwards to
// it. (B.4 R-CORE: the `CitizenBehaviorRaw` accessor mirror it used to carry had
// no user left in any code line and is deleted, B4_RULES H10.)

#include <cstdint>
#include <cstddef>
#include "Entity/CitizenBehavior.h"

