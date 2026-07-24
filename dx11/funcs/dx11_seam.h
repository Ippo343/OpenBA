#pragma once
// Phase 0 — the VideoDX11 replacement seam.
//
// The seam is the twelve vtables in the shipped VideoDX11-x64.dll's .rdata.
// We VirtualProtect them writable, snapshot all 84 original slot targets, and
// then overwrite individual slots with our reimplementations. Everything we
// have not ported keeps pointing at the original code.
//
// Why patch the vtables instead of wrapping DeviceData
// ----------------------------------------------------
// The scaffold originally proposed a "delegating DeviceData": call the real
// CreateDeviceData, keep it as a delegate, and hand Runtime our own object with
// our own vtable. That does not survive contact with the DLL:
//
//   * There would be two DeviceData objects. Runtime stores the one it was
//     given at Video::Device+0x100, and EVERY resource Bind in the DLL reads
//     g_Device->[0x100]->[0x28] to find the D3D11 context. Un-ported original
//     code would therefore read OUR object's fields while ported code held the
//     delegate — or vice versa. The two would silently diverge.
//   * DeviceData+0x60 is a live registry of ImageBuffer*s (appended by
//     0x180002300, swap-removed by ~ImageBufferData). Split across two objects
//     it stops being a registry.
//   * There is no interception point anyway. Video::Device::InitializeInterfaces
//     (which fills Device+0xd0/+0xd8) and CreateVideoInterface (which calls
//     through them) both run inside one Engine::Startup call, so the exe never
//     gets control between them.
//
// Patching the vtable slots has none of those problems: there is exactly one
// DeviceData, it is the real one with the real fields, and our slot receives it
// as `this`. Ported and un-ported slots operate on identical state by
// construction. It also works for the eleven resource classes without touching
// their factories, and it can be installed at any time after the DLL is loaded
// — no ordering constraint at all.
//
// Install point: standalone/main.cpp, right after Dx11FuncsInit(), which is
// before Engine::Startup and therefore before any *Data object exists.
//
// Granularity and A/B testing
// ---------------------------
// One slot is one unit of work, exactly like an entry in
// standalone/address_registry.cpp. Dx11SetSlotEnabled() flips a single slot
// between our version and the original at runtime, so a RenderDoc capture with
// the slot on and one with it off differ by exactly that function.
//
// Per project policy (kDefaultOnFlags in component_factory_base.cpp): a new
// slot registers with defaultOn=false and an opt-in BANISHED_DX11_* env var.
// Once it is verified in-game the registration flips to defaultOn=true and the
// env var becomes a =0 kill switch.

#include <cstdint>

namespace VideoDX11 {

// The twelve vtables. Order matches Dx11SeamInstall()'s report output.
enum class Vt : int {
    DeviceData = 0,       // 0x180018bb0, 20 slots
    ContextData,          // 0x1800186f0, 20 slots
    ImageBufferData,      // 0x180018790,  7 slots
    ConstantBufferData,   // 0x1800185d0,  4 slots
    IndexBufferData,      // 0x1800187c8,  7 slots
    VertexBufferData,     // 0x180018830,  7 slots
    VertexLayoutData,     // 0x180018878,  3 slots
    VertexProgramData,    // 0x180018890,  3 slots
    PixelProgramData,     // 0x180018800,  3 slots
    RenderStateData,      // 0x180018818,  3 slots
    VideoProgramData,     // 0x1800185a8,  3 slots
    RenderBufferData,     // 0x180018588,  4 slots
    kVtCount
};

constexpr int kMaxSlots = 20;   // DeviceData and ContextData both have 20

const char* VtName(Vt v);
int         VtSlotCount(Vt v);
uintptr_t   VtGhidraAddr(Vt v);

// The original slot target, captured before any override was installed.
// Valid after Dx11SeamInstall(). This is how a ported slot calls through to
// the original for the parts it has not taken over yet.
//
// NOTE: five targets are shared between vtables (e.g. 0x180001060 is slot 1 of
// RenderBufferData AND VideoProgramData AND slot 7 of DeviceData), so identical
// return values from different (Vt, slot) pairs are expected, not a bug.
void* Dx11OriginalSlot(Vt v, int slot);

// Register a reimplementation for one vtable slot. Call before
// Dx11SeamInstall() — registrations afterwards are rejected.
//
//   envFlag   BANISHED_DX11_* name, or nullptr for unconditional.
//   defaultOn false while unproven (env var opts in), true once graduated
//             (env var then only kills it with =0).
//   label     shown in the seam report, e.g. "IndexBufferData::Bind".
//
// Returns false on a bad (Vt, slot) pair or a duplicate registration.
bool Dx11RegisterSlot(Vt v, int slot, void* impl,
                      const char* envFlag, bool defaultOn, const char* label);

// Snapshot every original slot, verify it against the address recorded in
// dx11/README.md, and apply each enabled override.
//
// The verification is the point: it re-proves the whole vtable walk against the
// live DLL on every boot. A single mismatch means the walk, the ASLR delta, or
// the shipped DLL is not what we think, and the seam refuses to patch anything
// rather than corrupt a vtable.
//
// Returns false (and patches nothing) if the DLL is absent or verification
// fails. Non-fatal: the game then runs entirely on the original plugin.
bool Dx11SeamInstall();

// True once Dx11SeamInstall() has succeeded.
bool Dx11SeamReady();

// Runtime A/B flip of a single slot. Returns false if the slot has no
// registered reimplementation.
bool Dx11SetSlotEnabled(Vt v, int slot, bool useOurs);

// Log the full 84-slot table: original target, our target, and live state.
void Dx11SeamReport();

}  // namespace VideoDX11
