#pragma once
// Hand Runtime our renderer instead of letting it load VideoDX11-x64.dll.
// See standalone/video_interface.cpp for the plugin protocol this exploits.

// Detour Video::Device::InitializeInterfaces so the DX11 interface slot is
// already populated with our CreateDeviceData / GetDeviceName when it runs,
// which makes it skip the LoadLibrary for that slot entirely.
//
// Call after Dx11SeamInstall() (the mode must be decided) and before
// Engine::Startup (InitializeInterfaces runs inside it). No-op in AB mode.
void InstallVideoInterfacePrefill();

// True once our function pointers are actually on the Device singleton.
bool VideoInterfacePrefilled();

// Wave 9 lane ENGIO. Stamp our DX11 CreateDeviceData/GetDeviceName into the
// Device's interface slot directly, without the detour.
//
// At cutover the detour cannot be installed (there is no Runtime body to patch)
// and this becomes the ONLY way the slot is ever filled: whoever ports
// Video::Device::InitializeInterfaces (0x1800F79F0) must call it. Until then it
// is unused and InstallVideoInterfacePrefill is the live path.
void VideoInterfaceFillDx11Slot(void* device);

// Cutover only: the DX9 placeholder in slot 0 (see video_interface.cpp).
void VideoInterfaceFillDx9Placeholder(void* device);
