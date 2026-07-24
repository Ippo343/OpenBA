#pragma once
// The VideoDX11 replacement seam.
//
// The twelve *Data vtables live in this exe. `VideoDX11-x64.dll` is not loaded,
// not patched, and does not need to be present: each ported slot registers its
// reimplementation here, and Dx11SeamInstall() assembles the registrations into
// twelve arrays that our factories store as vptrs.
//
// History, because it explains the shape of this file
// ---------------------------------------------------
// The port grew up patching the shipped DLL's .rdata vtables in place, with the
// original still reachable so a single slot could be flipped between ours and
// the shipped implementation ("AB mode"). That was how all six porting lanes
// were verified. It was removed once per-draw pipeline-state equality was
// proven against the shipped DLL end to end — see dx11/README.md §"Per-draw
// pipeline-state equality". With nothing left to compare against, the .rdata
// patching, the export detours, the 84-slot address map and the DEP tripwire
// that watched the shipped .text all became dead weight.
//
// To A/B a future change against the original again, restore AB mode from git
// (it was removed in the commit that added dx11/README.md's per-draw section).
// The per-draw comparison tooling in tools/dx11_state_diff/ is still current,
// and notes/dx11_state/shipped*.jsonl are recorded dumps of the shipped
// renderer's per-draw state that can be diffed without re-running it.
//
// Why the seam was ever the vtables rather than a wrapper
// ------------------------------------------------------
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
// Owning the vtables outright has none of those problems, and the same argument
// is why Runtime is stopped from loading the DLL at all rather than being left
// to load it and have it ignored — see standalone/video_interface.cpp.
//
// Install point: standalone/main.cpp, right after Dx11FuncsInit(), which is
// before Engine::Startup and therefore before any *Data object exists.

#include <cstdint>

namespace VideoDX11 {

// The twelve vtables. Order matches Dx11SeamInstall()'s report output.
// Addresses are the shipped DLL's, kept as documentation and as the key the
// reference decompilation in decomp/videodx11.md is indexed by; nothing reads
// the DLL any more. The full slot-by-slot map is the status board in
// dx11/README.md.
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

// The vtable a *Data object's vptr must point at. Every ported destructor
// re-seats `this` on it before running the Runtime base destructor, and every
// factory stores it into the object it just allocated.
//
// Null before Dx11SeamInstall(). There is deliberately no fallback to
// ResolveAddr(Module::VideoDX11, ...): the DLL is not mapped, so a caller that
// reached past this function would silently produce a vptr into nothing.
void** Dx11LiveVtable(Vt v);

// Always returns null — there is no shipped code in the process to fall back to.
//
// EVERY caller of this is a "a Runtime import failed to resolve, run the
// original for this slot instead" branch, inherited from when an original
// existed. None is supposed to fire. Each call is counted and the first per slot
// is reported on stderr, so Dx11ForwardCount() == 0 stays positive evidence
// rather than an absence of evidence. Most call sites invoke the result without
// a null check, so a fallback that does fire becomes a null-call crash preceded
// by the log line — which is the intended outcome, the alternative being to
// carry on with a resource that was never bound.
void* Dx11OriginalSlot(Vt v, int slot);

// Number of times a ported slot has hit that dead fallback path. Must be 0.
unsigned Dx11ForwardCount();

// Register a reimplementation for one vtable slot. Call before
// Dx11SeamInstall() — registrations afterwards are rejected.
//
//   label   shown in the seam report, e.g. "IndexBufferData::Bind".
//
// There is no per-slot enable flag. Each slot used to carry a BANISHED_DX11_*
// name and a defaultOn bool, because while the shipped DLL was mapped a slot
// could be switched back to its implementation. Once that went (see "AB mode,
// and why it is gone" in dx11/README.md) the flags could not do anything: there
// is no original to fall back to, so every slot is unconditionally ours and a
// registration that could be declined would just be a null vtable entry.
//
// Returns false on a bad (Vt, slot) pair or a duplicate registration.
bool Dx11RegisterSlot(Vt v, int slot, void* impl, const char* label);

// Register from a porting lane's own .cpp with NO edit to any shared file —
// this is how the six lanes stayed collision-free. The static initializer runs
// before main(), therefore before Dx11SeamInstall(). Registration order between
// translation units is unspecified and deliberately irrelevant: the seam's slot
// table is zero-initialized POD in .bss.
//
//   DX11_REGISTER_SLOT(IndexBufferData, 5, &IndexBufferData_Bind,
//                      "IndexBufferData::Bind");
//
// `slot` must be an integer literal (it is pasted into the guard variable name).
#define DX11_REGISTER_SLOT(vtName, slot, impl, label)                       \
    namespace {                                                             \
    const bool kDx11Reg_##vtName##_##slot = ::VideoDX11::Dx11RegisterSlot(   \
        ::VideoDX11::Vt::vtName, (slot), reinterpret_cast<void*>(impl),     \
        (label));                                                          \
    }

// Build the twelve vtables from the registrations.
//
// The check that matters here is completeness: every slot of every vtable must
// have a registered implementation, because an unregistered slot is a null
// function pointer that Runtime will eventually dispatch through. A hole is
// refused, not zero-filled.
//
// Returns false (and installs nothing) if any slot is unimplemented.
bool Dx11SeamInstall();

// True once Dx11SeamInstall() has succeeded.
bool Dx11SeamReady();

// Log the full 84-slot table and the fallback count.
void Dx11SeamReport();

}  // namespace VideoDX11
