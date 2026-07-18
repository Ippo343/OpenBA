// standalone/reg_buildcomponent_sub8630.cpp
// Registers the "all required construction materials delivered?" gate
// (BuildDescription_helper_sub_8630 @ 0x140108630) to its clean port in
// code/Game/Component/buildcomponent_sub8630.cpp.
//
// The lead wires RegisterBuildComponentSub8630() into RegisterCodeFunctions()
// in address_registry.cpp.
#include "resolve.h"
#include <cstdint>

extern "C" bool BuildComponent_HaveAllMaterials(void* buildComp);   // 0x140108630

extern "C" void RegisterBuildComponentSub8630() {
    RegisterExeAddr(0x140108630, reinterpret_cast<uintptr_t>(&BuildComponent_HaveAllMaterials));
}
