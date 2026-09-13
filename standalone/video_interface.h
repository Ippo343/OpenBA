#pragma once
// Hand Runtime our renderer instead of letting it load VideoDX11-x64.dll.
// See standalone/video_interface.cpp for the plugin protocol this exploits.

// True once our function pointers are actually on the Device singleton.
bool VideoInterfacePrefilled();

// Wave 9 lane ENGIO. Stamp our DX11 CreateDeviceData/GetDeviceName into the
// Device's interface slot directly, without the detour.
//
// This is the ONLY way the slot is ever filled. Our ported
// Video::Device::InitializeInterfaces (0x1800F79F0,
// runtime/src/video/video_interfaces_win.cpp) calls it as its first statement;
// the old byte-patch detour on the DLL's body is gone with the DLL.
void VideoInterfaceFillDx11Slot(void* device);

// Cutover only: the DX9 placeholder in slot 0 (see video_interface.cpp).
void VideoInterfaceFillDx9Placeholder(void* device);
