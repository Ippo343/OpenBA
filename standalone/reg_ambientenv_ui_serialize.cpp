// Save/load wave — slot-3 stream Serialize registrations for
// AmbientEnvironmentComponent and UIComponent.
//
// Registers the retail addresses of both classes' Serialize bodies (Ghidra
// mislabels each "*_Shutdown"; both actually call Component_Serialize first,
// confirming they are Serialize) so ResolveAddr(Module::Exe, ...) finds them
// once the address is looked up (not currently exercised — see the header
// notes in ambientenvironmentcomponent_serialize.cpp / uicomponent_serialize.cpp:
// standalone has no save/load driver yet).
//
// Hook: address_registry.cpp calls RegisterAmbientEnvAndUISerialize().
#include "resolve.h"
#include <cstdint>

// ambientenvironmentcomponent_serialize.cpp
extern "C" void AmbientEnvironmentComponent_Serialize(void* self, void* stream);
// uicomponent_serialize.cpp
extern "C" void UIComponent_Serialize(void* self, void* stream);

void RegisterAmbientEnvAndUISerialize() {
    RegisterExeAddr(0x1400437e0, reinterpret_cast<uintptr_t>(&AmbientEnvironmentComponent_Serialize));
    RegisterExeAddr(0x14012dd00, reinterpret_cast<uintptr_t>(&UIComponent_Serialize));
}
