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
// Ported first on purpose: they were the smallest possible change that
// exercised the whole seam — static registration, install-time patch, live
// dispatch — with a blast radius of nothing.
//
// B5 DEV-A (the dx11 seam): they are `VideoDX11::DeviceData` members overriding
// the base's `Slot4`..`Slot7` (runtime/types/video/CompanionData.h), so each
// carries its base slot's signature — the CALLER's dispatch shape — and ignores
// what it is passed, as the three-byte originals do. The four
// DX11_REGISTER_SLOT lines that installed them in the hand table are gone. (The
// original slot 4 is `ret 0`, not `ret N` — it pops nothing, so there is no
// stack adjustment to reproduce either.)
//
// Return registers match the originals exactly: `bool` returns in AL like
// `mov al,1` / `xor al,al`, and `float` returns in XMM0 like `xorps xmm0,xmm0`.
//
// Slots 4 and 7 are shared code in the original — 0x180001190 is also
// VideoProgramData slot 2, and 0x180001060 is also RenderBufferData slot 1 and
// VideoProgramData slot 1. A vtable holds a *slot*, not a *function*: those
// classes have their own members (smallclasses.cpp) and are untouched by this
// file.

#include "VideoDX11Data.h"

namespace VideoDX11 {

// 0x180001190 — `ret`. The Runtime's one caller (Engine::Loader::Run
// 0x1800dbfc0) passes a bool in DL (1 around the load, 0 after); the body
// ignores it (B3R owed 3).
void DeviceData::Slot4(bool /*loading*/) {
}

// 0x180005430 — `xorps xmm0,xmm0; ret`
float DeviceData::Slot5() {
    return 0.0f;
}

// 0x180005440 — `xor al,al; ret`
bool DeviceData::Slot6() {
    return false;
}

// 0x180001060 — `mov al,1; ret`
bool DeviceData::Slot7() {
    return true;
}

}  // namespace VideoDX11
