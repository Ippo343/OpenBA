#pragma once
// standalone/runtime_init.h -- the ported Runtime's one-time boot initialisation.
//
// Both are called from standalone/main.cpp's Init(), in this order, before
// Engine::Startup.

// Runtime-x64.dll must never be mapped in this process (ENDGAME A.1). Prints
// "[Standalone] Runtime-x64.dll: never loaded (ENDGAME A.1)" and returns true;
// if the DLL IS mapped, prints a FATAL line and returns false.
bool RuntimeDllNotMapped();

// Installs the libm seam, arms the allocator, installs the [ExtLoad] / [ExtAdd] /
// [ExtSaveText] exit hooks, prints the run-identity line and the [SLOTS29]
// attendance line -- in that order. Runs after main.cpp has handed the
// allocator its cutover armer (RtSystemAllocatorSetCutoverArmer).
void RuntimeInit();
