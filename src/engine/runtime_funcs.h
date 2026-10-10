#pragma once
// Declarations of Runtime bodies that other Runtime modules call by name and
// whose defining module's header could not carry them (B3R, R5). (B3R W2-RES:
// RuntimeFuncsInit(), which resolved nothing, is deleted with its call.)

#include <cstdint>

// ==== B3R RT-IO: Runtime bodies reached by name from src/engine/save ====
// Declared here (B3R_RULES R5: the defining modules are outside RT-IO's file
// set). Copied from their definitions.
// ---- graphics/wave21_lane_gfxbody.cpp
extern "C" void* RtGraphics_FrameCopyImageBuffer_OperatorNew(uint64_t bytes);  // ??2FrameCopyImageBuffer@Graphics@@SAPEAX_K@Z
extern "C" void* RtGraphics_MaterialInstance_OperatorNew(uint64_t bytes);      // ??2MaterialInstance@Graphics@@SAPEAX_K@Z
// ---- ui/spritesheet_new.cpp
extern "C" void* RtUi_SpriteSheet_OperatorNew(uint64_t bytes);                 // 0x180002e60 ??2SpriteSheet@UI@@SAPEAX_K@Z
// ==== end B3R RT-IO ====
