#pragma once
// Address registry for standalone exe build.
// Maps Ghidra exe addresses to our reimplemented functions, vtables, and globals.
//
// Three categories of exe addresses:
//   1. Code functions (0x140001xxx - 0x14014Fxxx): exe-internal functions
//      -> Registered as pointers to our C++ reimplementations
//   2. IAT dispatch slots (0x140151xxx - 0x140152xxx): .rdata holding Runtime DLL func ptrs
//      -> Standalone provides its own .rdata slots pointing to Runtime DLL functions
//   3. Vtables + type IDs (0x14015Exxx - 0x1401Cxxxx): class vtables and type globals
//      -> Standalone provides its own static vtable arrays and type ID storage
//
// Usage: Call AddressRegistryInit() from standalone main before any _InitFuncs().


#include <cstdint>

// Initialize the address registry with all known reimplemented addresses.
// Must be called after ResolveInit() (so Runtime DLL functions are available)
// and before any code/Game/ _InitFuncs() functions.
void AddressRegistryInit();

// Print progress stats to stdout.
void AddressRegistryPrintStats();

