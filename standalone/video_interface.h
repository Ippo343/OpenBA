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
