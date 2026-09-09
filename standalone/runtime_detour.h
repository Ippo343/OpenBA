#pragma once
// ============================================================================
// standalone/runtime_detour.h — inline detours into the loaded Runtime-x64.dll
// ============================================================================
//
// WHY THIS EXISTS (docs/runtime_port/PLAN.md §4.1 decision 3)
//
// The exe port had it easy: every call the exe makes into itself goes through
// ResolveAddr(Module::Exe, ...), so registering a reimplementation redirected
// ALL callers at once. The DLL is not like that. Runtime-x64.dll's internal
// call graph never goes through its export table — an internal
// `String::Append` call is a direct rel32 CALL inside the DLL. So
// RegisterRuntimeExport() redirects OUR ~1,180 call sites and nothing else;
// the DLL keeps running its own copy for its own callers.
//
// That is fine for a leaf forwarder, and NOT fine for anything with state
// (allocator, factory/handle table, filesystem, UI manager) — two copies of
// one subsystem's state is the swap-remove UAF family all over again. For
// those, and for A/B-proving a port against every caller, we patch the DLL
// function itself to JMP to ours.
//
// This is the `standalone/allocator_lock.cpp` pattern generalised: that file
// hand-rolls two fixed-size prologue patches with hardcoded expected bytes.
// Here the prologue is decoded with HDE64 and PC-relative instructions are
// relocated, so an arbitrary function can be detoured without hand-checking
// its first twelve bytes. The code is revived from the proxy-DLL era
// (`fcd3fb6^:patches/src/hooks.cpp`), where it carried the whole hooking mode
// for months; the bug-class comments in the .cpp are that history and are
// worth reading before changing anything.
//
// RULES
//   * Resolve the target through ResolveRuntimeRaw / ResolveExportRaw, never
//     ResolveAddr / ResolveExport — otherwise, once the port registers, the
//     installer patches OUR function and builds a trampoline to itself.
//     InstallRuntimeDetour* below already do this for you.
//   * A detour is permanent for the process unless you Remove it. Removing
//     while another thread is inside the function is a race; do it at
//     shutdown or not at all.
//   * Keep C++ exceptions out of detoured code. The DLL has its own CRT and
//     `__C_specific_handler`; unwinding across the boundary is undefined
//     (PLAN.md §4.3 risk 5).
//   * Every detour gets an env kill-switch at its call site, exactly like
//     kDefaultOnFlags in the exe port. Nothing goes default-on unproven.
// ============================================================================

#include <windows.h>
#include <cstdint>
#include <cstddef>

// Trampoline buffer: relocated instructions can expand, plus the 13-byte
// jump-back (MOV R10, imm64 + JMP R10) and an absolute-address data area that
// grows down from the end.
static constexpr size_t RT_TRAMPOLINE_BUFFER_SIZE = 256;

struct RtDetour {
    void*       trampoline;   // call this to invoke the ORIGINAL function
    void*       target;       // the address we patched
    uint8_t     original[32]; // saved original bytes
    size_t      patchSize;    // bytes saved/restored (>= 12; 0 = vtable detour)
    const char* label;        // for logs and crash diagnostics
};

// ---- The original byte-copy installer (predates the runtime port) ---------
//
// This is the helper that `filename_runtime_hook.cpp`, `persistobject_probe.cpp`
// and `video_interface.cpp` already use. It is deliberately dumb: the caller
// states the prologue length and the exact bytes it expects to find, and the
// patch is refused on any mismatch. That is the right tool when you have read
// the prologue in Ghidra and want the strictest possible guard.
//
// Use InstallDetour below instead when the prologue contains PC-relative
// instructions or an RSP capture, which this one cannot relocate.
//
// Interpose a function inside Runtime-x64.dll.
//
// TAKES A GHIDRA ADDRESS (base 0x180000000) AND RESOLVES IT WITH
// `ResolveRuntimeRaw` ITSELF. That is not a convenience, it is the fix for a
// defect this signature had for its whole life (Wave 9 lane FNHOOK): it used to
// take an ALREADY-RESOLVED address, and all ten call sites in the tree resolved
// it with `ResolveAddr(Module::Runtime, …)` — the door that consults the port
// registry FIRST (reference_resolveaddr_consults_port_registry_first.md). Once a
// port registered at a detour target, the installer aimed at OUR OWN GCC
// prologue, the byte guard below refused it, and the instrument went silently
// dead. Five of the ten targets were in that state when this was found. The
// rule was already written down thirty lines up; the parameter type is what let
// every caller break it, so the resolve now lives on this side of the boundary
// where no caller can get it wrong.
//
// Overwrites `patchLen` bytes at the resolved address with an absolute jump to
// `wrapper` and returns an executable trampoline holding the displaced bytes
// followed by a jump back to `addr + patchLen` — i.e. the original body,
// callable from the wrapper. Returns nullptr when the DLL is absent, which is
// the cutover value and is why every caller must tolerate a null trampoline.
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
void* RuntimeDetourInstall(uintptr_t ghidraAddr, void* wrapper, int patchLen,
                           const uint8_t* expected, int expectedLen,
                           const char* name);

// ---- Low-level installers -------------------------------------------------

// Overwrite `targetAddr` with a 12-byte absolute jump to `detourFunc` and
// build a trampoline holding the displaced prologue.
//
// `prologueSize` is how many bytes to displace: at least 12 (the jump) and it
// MUST end on an instruction boundary. The installer auto-detects one hazard:
//   * PC-relative instructions (CALL/JMP rel, Jcc, RIP-relative ModR/M) —
//     re-encoded against the trampoline's address via HDE64.
//
// It does NOT touch an RSP capture (MOV RAX,RSP / MOV R11,RSP), and must not:
// a trampoline entered by CALL sees the same frame SHAPE as retail's own entry
// (return address at [RSP], the caller's 32-byte home space at [RSP+8]), so the
// captured value needs no adjustment. This installer used to add 8 and that was
// a live defect — see runtime_detour.cpp's Bug Class 3 note and
// docs/runtime_port/WAVE10_LANE_FREEPRO.md.
//
// WHICH INSTALLER TO USE (the rule, so it does not get re-derived):
//   * `InstallDetour` / `InstallRuntimeDetour*` — the default. Relocates
//     PC-relative bytes, so it is the ONLY option when the displaced prologue
//     contains a CALL/JMP/Jcc rel or a RIP-relative operand.
//   * `RuntimeDetourInstall` (the byte-copy installer) — when you have read the
//     prologue and want the strictest guard: it copies verbatim, inserts
//     nothing, and refuses on any byte mismatch. Requires that no displaced
//     byte is PC-relative.
//   * `InstallDetourRSPSafe` — only when your detour tail-JMPs into the
//     trampoline instead of CALLing it.
//   * `InstallDetourVTableAt` — when the prologue cannot be displaced at all
//     (SEH-cookie functions, Bug Class 8).
// An RSP capture in the prologue is NOT by itself a reason to prefer one over
// another; all of them now reproduce it correctly.
bool InstallDetour(uintptr_t targetAddr, void* detourFunc, RtDetour* info,
                   size_t prologueSize = 12);

// For prologues that capture RSP and whose detour tail-JMPs (never CALLs) into
// the trampoline, so no return address is pushed and RSP is caller-level.
bool InstallDetourRSPSafe(uintptr_t targetAddr, void* detourFunc, RtDetour* info,
                          size_t prologueSize);

// Restore the original bytes and release the trampoline.
void RemoveDetour(RtDetour* info);

// Swap one vtable slot instead of patching code. Use for functions whose
// prologue cannot be overwritten without breaking .pdata/.xdata unwind (the
// SEH-cookie family). `vtableAddr` is an already-resolved runtime address.
bool InstallDetourVTableAt(uintptr_t vtableAddr, int slotIndex, void* detourFunc,
                           RtDetour* info);
void RemoveDetourVTable(RtDetour* info);

// ---- Runtime-DLL front doors (use these) ----------------------------------
//
// Both resolve the ORIGINAL DLL code (Raw), install, register the detour for
// diagnostics, and log one line. They return false and log why on any failure;
// a failed install leaves the DLL untouched.

bool InstallRuntimeDetour(const char* label, uintptr_t ghidraAddr, void* ourFn,
                          RtDetour* info, size_t prologueSize = 12);
bool InstallRuntimeDetourExport(const char* label, const char* mangledName, void* ourFn,
                                RtDetour* info, size_t prologueSize = 12);

// Optional safety belt: refuse to install unless the first `count` bytes at the
// target match `expected`. The allocator lock does this by hand; pass the bytes
// here and any DLL version drift is caught at install time instead of at the
// first call. Pass nullptr/0 to skip.
bool InstallRuntimeDetourChecked(const char* label, uintptr_t ghidraAddr, void* ourFn,
                                 RtDetour* info, size_t prologueSize,
                                 const uint8_t* expected, size_t count);

// ---- Diagnostics ----------------------------------------------------------

// Name of the detour whose target or trampoline contains `addr`, or nullptr.
// The VEH crash handler uses this to say "you died inside a detoured Runtime
// function" instead of printing a bare address.
const char* FindRuntimeDetourByAddress(uintptr_t addr);

// Count of currently installed detours, and a stderr listing.
size_t RuntimeDetourCount();
void   DumpRuntimeDetours();
