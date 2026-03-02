#pragma once
// Resolved function pointers for Runtime-x64.dll internal calls.
// These are for functions called BY reimplemented Runtime DLL code (internal calls).
// Function pointers called BY exe code (like ImageBuffer methods the UI panels call)
// stay in engine/funcs/engine_funcs.h per existing convention.
//
// Resolved by RuntimeFuncsInit() after ResolveInit() succeeds.

#include <cstdint>

// Resolve all Runtime DLL internal function pointers.
// Call after ResolveInit(). Returns true on success.
bool RuntimeFuncsInit();
