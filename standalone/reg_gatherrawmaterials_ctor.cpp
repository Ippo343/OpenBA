// standalone/reg_gatherrawmaterials_ctor.cpp
// Registers the GatherRawMaterials Work constructor (GatherRawMaterials_Ctor
// @ 0x1400ec710) to its clean port in
// code/Game/Work/gatherrawmaterials_ctor.cpp.
//
// The lead wires RegisterGatherRawMaterialsCtor() into RegisterCodeFunctions()
// in address_registry.cpp.
//
// NOTE: the ctor calls Work_Ctor25 @ 0x1400f72e0 (base Work ctor), which is NOT
// yet registered for standalone — see the FLAG in the ctor source. It should be
// registered too so the Work base fields get initialised.
#include "resolve.h"
#include <cstdint>

extern "C" void* GatherRawMaterials_Ctor_Impl(void* mem, void* buildEntityIBP);  // 0x1400ec710

extern "C" void RegisterGatherRawMaterialsCtor() {
    RegisterExeAddr(0x1400ec710, reinterpret_cast<uintptr_t>(&GatherRawMaterials_Ctor_Impl));
}
