// standalone/citizen_vitals.h — see citizen_vitals.cpp.
// One aggregate line every 1800 frames: citizen count, age/hunger/thirst spread.
// BANISHED_VITALS=0 disables.
#pragma once

void CitizenVitals_Tick(void* gameWorld);
