#pragma once
// Interpose a function inside Runtime-x64.dll.
//
// Overwrites `patchLen` bytes at `addr` with an absolute jump to `wrapper` and
// returns an executable trampoline holding the displaced bytes followed by a
// jump back to `addr + patchLen` — i.e. the original body, callable from the
// wrapper.
//
// `patchLen` must be >= 12 (the size of the absolute jump) AND must land on an
// instruction boundary; the helper NOPs the remainder. Getting that boundary
// wrong leaves half an instruction behind and the process dies somewhere else
// entirely, so read the prologue in Ghidra and count.
//
// `expected` / `expectedLen` are the bytes that must currently be at `addr`.
// The patch is refused on a mismatch — which catches a different build of
// Runtime, a wrong ASLR delta, and another detour already on the same address.
// Take the bytes from memory, not from a mnemonic listing: this compiler emits
// redundant REX prefixes, so `PUSH RBP` is `40 55` and not `55`.
//
// Returns nullptr on failure. Implemented in standalone/filename_runtime_hook.cpp,
// which is where the first six of these were written.

#include <cstdint>

void* RuntimeDetourInstall(uintptr_t addr, void* wrapper, int patchLen,
                           const uint8_t* expected, int expectedLen,
                           const char* name);
