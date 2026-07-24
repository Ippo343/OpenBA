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

// ============================================================
// Two ways to make our 84 slots the live ones
// ============================================================
//
// Standalone (default) — the vtables are OURS, twelve arrays in the exe's
//   .data built from the same DX11_REGISTER_SLOT entries. Nothing is patched
//   and VideoDX11-x64.dll is never loaded: Runtime is stopped from
//   LoadLibrary'ing it by pre-filling Video::Device+0xc8/+0xd0/+0xd8 (see
//   standalone/video_interface.cpp). This is the real replacement.
//
// AB (BANISHED_DX11_AB=1) — the original arrangement: load the shipped DLL,
//   verify all 84 live slot pointers against the map below, and patch its
//   .rdata in place. Kept solely because it is the only way to A/B a single
//   slot against the shipped implementation (Dx11SetSlotEnabled), which is how
//   every lane in this port was verified and how per-draw state equality will
//   have to be verified. Not used in a normal run.
//
// The difference is confined to Dx11LiveVtable() and to what Dx11SeamInstall()
// does; no ported slot knows which mode it is running under.
enum class Mode {
    Standalone = 0,
    AB         = 1,
};

Mode        Dx11ActiveMode();
const char* Dx11ModeName();

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

// The vtable a *Data object's vptr must point at: our array in Standalone
// mode, the shipped DLL's .rdata array in AB mode. Every ported destructor
// re-seats `this` on it before running the Runtime base destructor, and every
// factory stores it into the object it just allocated.
//
// Null before Dx11SeamInstall(). There is deliberately no fallback to
// ResolveAddr(Module::VideoDX11, ...): in Standalone mode the DLL is not
// mapped, so a caller that reached past this function would silently produce a
// vptr into nothing.
void** Dx11LiveVtable(Vt v);

// The original slot target, captured before any override was installed.
// Valid after Dx11SeamInstall(). This is how a ported slot calls through to
// the original for the parts it has not taken over yet.
//
// NOTE: five targets are shared between vtables (e.g. 0x180001060 is slot 1 of
// RenderBufferData AND VideoProgramData AND slot 7 of DeviceData), so identical
// return values from different (Vt, slot) pairs are expected, not a bug.
//
// EVERY caller of this is a fallback path — "a Runtime import failed to
// resolve, so run the shipped code for this slot instead". None is supposed to
// fire. Each call is therefore counted and the first one per slot is reported
// on stderr, so that Dx11ForwardCount() == 0 is positive evidence that no
// shipped renderer code ran, rather than an absence of evidence.
//
// **In Standalone mode this always returns null**, because there is no shipped
// code to fall back to. Most call sites invoke the result without a null check,
// so a fallback that fires becomes a null-call crash preceded by the log line
// above. That is the intended outcome: the alternative would be to silently run
// the shipped implementation, which is the thing this branch exists to stop.
void* Dx11OriginalSlot(Vt v, int slot);

// Number of times a ported slot has fallen back to the shipped implementation.
// Must be 0 for the "the original DLL is unreachable" claim to hold.
unsigned Dx11ForwardCount();

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

// Register from a porting lane's own .cpp with NO edit to any shared file —
// this is how the six lanes stay collision-free. The static initializer runs
// before main(), therefore before Dx11SeamInstall(). Registration order between
// translation units is unspecified and deliberately irrelevant: the seam's slot
// table is zero-initialized POD in .bss.
//
//   DX11_REGISTER_SLOT(IndexBufferData, 5, &IndexBufferData_Bind,
//                      "BANISHED_DX11_IB_BIND", false, "IndexBufferData::Bind");
//
// `slot` must be an integer literal (it is pasted into the guard variable name).
#define DX11_REGISTER_SLOT(vtName, slot, impl, envFlag, defaultOn, label)  \
    namespace {                                                            \
    const bool kDx11Reg_##vtName##_##slot = ::VideoDX11::Dx11RegisterSlot(  \
        ::VideoDX11::Vt::vtName, (slot), reinterpret_cast<void*>(impl),    \
        (envFlag), (defaultOn), (label));                                  \
    }

// ============================================================
// Detours — for the code the vtables cannot reach
// ============================================================
//
// The twelve vtables cover everything the DLL does *after* it has an object.
// They do not cover the two exports, `CreateDeviceData` (0x1800011a0) and
// `GetDeviceName` (0x180001230), which Runtime calls through
// Video::Device+0xd8 / +0xd0. Those fields are filled by
// Video::Device::InitializeInterfaces and consumed by CreateVideoInterface,
// both inside one Engine::Startup call, so the exe never gets a window in which
// to overwrite them — the same reason the seam is the vtables and not
// Device+0xd0/+0xd8 in the first place.
//
// What we *can* do is replace the functions those fields point at. A detour
// overwrites the first 12 bytes of the export with
//
//     48 B8 <imm64>   MOV RAX, our_impl
//     FF E0           JMP RAX
//
// which is position-independent (a rel32 JMP cannot span exe->DLL) and clobbers
// only RAX, dead at function entry and the return register for both exports.
//
// Because this rewrites *code*, it is verified harder than a vtable slot: the
// original bytes must match `expectedBytes` exactly, or nothing is written.
// That catches a different build of the DLL, a wrong ASLR delta, and — unlike
// the vtable check — another detour already sitting on the same address.
//
// kDetourSize bytes are overwritten, so the target must be at least that long
// including its 0xCC alignment padding. GetDeviceName is 8 bytes of code
// followed by 8 bytes of padding; CreateDeviceData's prologue alone is 14.
constexpr int kDetourSize = 12;

// Register a detour. Same envFlag/defaultOn semantics as Dx11RegisterSlot.
// Call before Dx11SeamInstall().
bool Dx11RegisterDetour(uintptr_t ghidraAddr, void* impl,
                        const uint8_t* expectedBytes, int expectedLen,
                        const char* envFlag, bool defaultOn, const char* label);

// Register from a lane's own .cpp, with no edit to any shared file.
#define DX11_REGISTER_DETOUR(tag, ghidraAddr, impl, expectedBytes, envFlag,   \
                             defaultOn, label)                               \
    namespace {                                                              \
    const bool kDx11Detour_##tag = ::VideoDX11::Dx11RegisterDetour(          \
        (ghidraAddr), reinterpret_cast<void*>(impl), (expectedBytes),        \
        static_cast<int>(sizeof(expectedBytes)), (envFlag), (defaultOn),     \
        (label));                                                            \
    }

// True if `addr` falls inside a live detour's 12-byte trampoline. Those bytes
// are OURS even though they live at a VideoDX11 address, so the tripwire
// (dx11_tripwire.h) must not report executing them as "shipped code ran".
bool Dx11IsDetourStub(uintptr_t runtimeAddr);

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
