#pragma once
// Proof that no shipped VideoDX11-x64.dll renderer code executes.
//
// The seam gives us "84/84 vtable slots are ours" and the two export detours
// give us "the plugin ABI is ours". Neither is the same claim as "no shipped
// code runs" — that is an assertion about every path into the DLL, including
// paths we have not thought of (an un-mapped callback registration, an MSVC EH
// funclet, a helper the vtable map does not name). Enumerating those by reading
// is exactly the kind of argument that quietly turns out to be wrong.
//
// So this measures it instead. When armed, VideoDX11's .text section is left
// readable and writable but NOT executable. Any attempt to execute a byte of it
// raises a DEP access violation, which a vectored handler catches, records
// (faulting address, plus the value at [RSP] as a best-effort caller) and then
// lets through by restoring execute permission.
//
// Semantics, stated precisely, because "we saw nothing" is only useful with the
// limits attached:
//
//   * A hit reopens the 4 KiB PAGE it faulted on, not the whole section, and
//     ContextData::BeginCommands re-closes every reopened page once per frame.
//     So one noisy function costs coverage of its page until the next frame,
//     and nothing else. Counts are therefore lower bounds; zero findings is
//     still zero entries.
//   * Detour trampolines live at DLL addresses but are our bytes. They are
//     recognised (Dx11IsDetourStub) and reported separately, not as findings.
//   * Loader/CRT plumbing (_DllMainCRTStartup on every thread attach, _freefls
//     on every thread exit) is listed by name in kExpected and reported as
//     "expected", not as a finding. It is not renderer code and it cannot be
//     removed while the DLL is loaded — which it must be, since Runtime
//     LoadLibrary's it and the seam reads its vtables out of .rdata.
//   * Another thread executing DLL code during the moment the range is
//     executable would be missed. Nothing in VideoDX11 creates threads, but
//     that is an argument, not a measurement.
//   * DllMain/CRT init at LoadLibrary time and CRT teardown at process detach
//     run outside the armed window and are irreducible: Runtime is what
//     LoadLibrary's the DLL, and the seam needs it loaded.
//
// Off unless BANISHED_DX11_TRIPWIRE is set — it costs two VirtualProtect calls
// per frame and deliberately makes a normally-harmless mistake fatal-looking.

#include <cstdint>

namespace VideoDX11 {

// Read BANISHED_DX11_TRIPWIRE. Cheap and cached.
bool Dx11TripwireEnabled();

// Locate VideoDX11's .text, install the vectored handler, and arm.
// No-op when disabled. Call after Dx11SeamInstall().
bool Dx11TripwireInstall();

// Re-arm after a hit disarmed it. Called once per frame from
// ContextData::BeginCommands. Cheap no-op when already armed or disabled.
void Dx11TripwireRearm();

// Restore execute permission and stop reporting.
void Dx11TripwireDisarm();

// Log every distinct faulting address recorded, or state that there were none.
void Dx11TripwireReport();

// Number of distinct entries into shipped (non-detour) DLL code.
int Dx11TripwireHitCount();

}  // namespace VideoDX11
