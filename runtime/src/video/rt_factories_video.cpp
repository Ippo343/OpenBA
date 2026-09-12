// ============================================================================
// runtime/src/video/rt_factories_video.cpp -- WAVE 28 LANE FACTREST
//
// 7 classes. ShaderProgram is a retail null stub in both slots; the other
// six delegate to an out-of-line ??0 constructor.
//
// The bodies are the shared interpreter in runtime/src/object/rt_factories_core.cpp;
// everything class-specific is the transcribed step table in the .inc beside
// this file. Registration is BOTH doors (name and address) per slot, default
// ON, from RtFactoriesPortInit() which runs before RtDataMintInit().
//
// ATTENDANCE 0 IS THE EXPECTED READING HERE while the DLL is present, and it is
// NOT a verification of any body. See rt_factories.h.
// ============================================================================

#include <cstdint>

#include "../../funcs/rt_port.h"
#include "../object/rt_factories.h"

#include "rt_factories_video.inc"

int RtFactoriesVideoRegister() {
    return RtFactoryRegisterAll(kRegs, sizeof(kRegs) / sizeof(kRegs[0]));
}
