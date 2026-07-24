// Phase 1 — VideoDX11 DeviceData slots 4, 5, 6, 7.
//
// The four one-liner stubs on the DeviceData vtable. These are pure virtuals on
// the Runtime-side Video::DeviceData that the DX11 backend declines to
// implement, so their *names* are unknown, but their bodies are three bytes
// each and fully decoded:
//
//   slot 4  0x180001190   c2 00 00        ret            -> void
//   slot 5  0x180005430   0f 57 c0 c3     xorps xmm0,xmm0; ret -> 0.0f
//   slot 6  0x180005440   32 c0 c3        xor al,al; ret -> false
//   slot 7  0x180001060   b0 01 c3        mov al,1; ret  -> true
//
// Ported first on purpose: they are the smallest possible change that exercises
// the whole seam — static registration, install-time patch, live dispatch —
// with a blast radius of nothing. If the game is identical with these four
// installed, the mechanism the other five lanes depend on is proven.
//
// Argument lists are unknown and deliberately not guessed. Under the Microsoft
// x64 ABI a callee may simply ignore arguments it was passed: they arrive in
// RCX/RDX/R8/R9 or on caller-allocated stack, and the caller cleans up. So a
// one-parameter signature is safe regardless of what the real one is. (Note the
// original slot 4 is `ret 0`, not `ret N` — it pops nothing, so there is no
// stack adjustment to reproduce either.)
//
// Return registers match the originals exactly: `bool` returns in AL like
// `mov al,1` / `xor al,al`, and `float` returns in XMM0 like `xorps xmm0,xmm0`.
//
// Slots 4 and 7 are shared code in the original — 0x180001190 is also
// VideoProgramData slot 2, and 0x180001060 is also RenderBufferData slot 1 and
// VideoProgramData slot 1. Patching a *vtable slot* rather than a *function*
// means those other classes are untouched by this file. Lane L2 owns them.

#include "dx11_seam.h"
#include "VideoDX11Data.h"

namespace VideoDX11 {

// 0x180001190 — `ret`
static void DeviceData_Stub4_Void(void* /*self*/) {
}

// 0x180005430 — `xorps xmm0,xmm0; ret`
static float DeviceData_Stub5_ZeroFloat(void* /*self*/) {
    return 0.0f;
}

// 0x180005440 — `xor al,al; ret`
static bool DeviceData_Stub6_False(void* /*self*/) {
    return false;
}

// 0x180001060 — `mov al,1; ret`
static bool DeviceData_Stub7_True(void* /*self*/) {
    return true;
}

}  // namespace VideoDX11

// GRADUATED default-on 2026-07-24: verified in-game with all four installed
// ("4 slot(s) ours, 0 failed"), autostart reached the world build, process
// responding, empty crash log. Per project policy (kDefaultOnFlags in
// component_factory_base.cpp) the env var is now a kill switch only —
// BANISHED_DX11_DEVICE_STUBS=0 puts all four back to the original code.
#define DX11_STUBS_FLAG "BANISHED_DX11_DEVICE_STUBS"

DX11_REGISTER_SLOT(DeviceData, 4, &::VideoDX11::DeviceData_Stub4_Void,
                   DX11_STUBS_FLAG, true, "DeviceData::Stub4 (void)");
DX11_REGISTER_SLOT(DeviceData, 5, &::VideoDX11::DeviceData_Stub5_ZeroFloat,
                   DX11_STUBS_FLAG, true, "DeviceData::Stub5 (0.0f)");
DX11_REGISTER_SLOT(DeviceData, 6, &::VideoDX11::DeviceData_Stub6_False,
                   DX11_STUBS_FLAG, true, "DeviceData::Stub6 (false)");
DX11_REGISTER_SLOT(DeviceData, 7, &::VideoDX11::DeviceData_Stub7_True,
                   DX11_STUBS_FLAG, true, "DeviceData::Stub7 (true)");
