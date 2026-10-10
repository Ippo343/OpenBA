// src/diag/orchard_diag.h
// The orchard diagnostic's public API (src/diag/orchard_diag.cpp; BANISHED_ORCHARD_DIAG,
// read once at boot). Port-only instrumentation: the PlantOrchard / HarvestOrchard
// constructors and Execute bodies count themselves through these, the game loop ticks the
// periodic dump. Every entry is a no-op while the diagnostic is off.
#pragma once

extern "C" void OrchardDiag_Tick(int frameNum);
extern "C" void OrchardDiag_NotePlantConstructed();
extern "C" void OrchardDiag_NotePlantExecuted();
extern "C" void OrchardDiag_NoteHarvestConstructed();
extern "C" void OrchardDiag_NoteHarvestExecuted();
