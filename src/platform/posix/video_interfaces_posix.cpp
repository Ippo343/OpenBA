// ============================================================================
// src/platform/posix/video_interfaces_posix.cpp -- the POSIX twin of
//   src/platform/win/video_interfaces_win.cpp (Phase D.4 LINUX-1, plans/RENDER.md C.5)
// ============================================================================
//   ?GetDLLForInterface@Device@Video@@QEBA?AVString@System@@H@Z   0x1800f7b20
//   ?InitializeInterfaces@Device@Video@@QEAAXXZ                   0x1800f79f0
//
// Read the Windows file for both bodies, their retail addresses and every oddity they
// keep. The twin is the same two bodies less the one Win32 call: retail's
// SetErrorMode(SEM_FAILCRITICALERRORS) before each LoadLibraryW suppressed the OS
// "cannot find DLL" box, and there is no such box off Windows (and, since ENDGAME A.1,
// no LoadLibraryW either: the loop loads nothing on any platform).
//
// GetDLLForInterface keeps its retail names ("VideoDX9-x64.dll", "VideoDX11-x64.dll",
// "VideoGL-x64.dll", and "-x64.dll" for an index outside 0..2): it is a pure function of
// the index that nothing loads by, and the names are what retail answers.
// ============================================================================

#include <cstdint>

#include "video/Device.h"
#include "system/String.h"
#include "video_interface.h"   // VideoInterfaceFillDx11Slot / VideoInterfaceFillDx9Placeholder

using System::String;
using InterfaceSlot = Video::Device::InterfaceSlot;

namespace {

// The plugin base names and suffixes retail reads from .rdata (0x1801588c0..0x1801588f0).
const char kNameDX9[]    = "VideoDX9";
const char kNameDX11[]   = "VideoDX11";
const char kNameGL[]     = "VideoGL";
const char kSuffixArch[] = "-x64";
const char kSuffixExt[]  = ".dll";

constexpr int kSlotCount = 3;

}  // namespace

// 0x1800f7b20: `this` is never read; an index outside 0..2 yields "-x64.dll".
System::String Video::Device::GetDLLForInterface(int index) const {
    String ret;

    const char* base = nullptr;
    if (index == 0)      base = kNameDX9;
    else if (index == 1) base = kNameDX11;
    else if (index == 2) base = kNameGL;

    if (base != nullptr) {
        String tmp;
        tmp.Set(base);
        ret = tmp;
    }

    ret += kSuffixArch;
    ret += kSuffixExt;
    return ret;
}

// 0x1800f79f0: slot 1 is filled with our renderer first, the loop resolves each empty
// slot's name and loads nothing (ENDGAME A.1), slot 0 gets the cutover placeholder.
void Video::Device::InitializeInterfaces() {
    VideoInterfaceFillDx11Slot(this);

    for (int i = 0; i < kSlotCount; ++i) {
        InterfaceSlot* const slot = &dllSlots[i];
        if (slot->module != nullptr) continue;
        // (retail: SetErrorMode(SEM_FAILCRITICALERRORS) here; Windows-only)
        String name = Video::Device::GetDLLForInterface(i);
    }

    if (dllSlots[0].module == nullptr) {
        ::VideoInterfaceFillDx9Placeholder(this);
    }
}
