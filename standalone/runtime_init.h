#pragma once
// standalone/runtime_init.h -- the ported Runtime's one-time boot initialisation.
//
// Called from standalone/main.cpp's Init(), after the allocator's cutover armer
// is handed in (RtSystemAllocatorSetCutoverArmer) and before Engine::Startup.

// Installs the libm seam, arms the allocator, installs the [ExtLoad] / [ExtAdd] /
// [ExtSaveText] exit hooks, prints the run-identity line and the [SLOTS29]
// attendance line -- in that order.
void RuntimeInit();
