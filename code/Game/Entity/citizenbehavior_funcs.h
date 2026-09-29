#pragma once
// CitizenBehavior's C entry point left for its caller outside block WORK-a's
// files. The bodies are CitizenBehavior / PathGraph members (CitizenBehavior.h);
// the line below is a one-line forwarder defined in citizenbehavior.cpp (B4
// W1_BRIEF "Callers outside your files").
//
// CitizenBehavior is the population manager (GameUpdateLoop+0xF0).
// Profile source path: Entity\CitizenBehavior.cpp

#include "engine/types/CitizenBehavior.h"   // the forwarder: also carries CitizenBehaviorRaw
#include <cstdint>

// B4 FORWARD: 1 outside caller(s) (code/Game/Game/gameupdateloop.cpp)
void CitizenBehavior_Update(void* self, int64_t* deltaTime);   // CitizenBehavior::Update 0x14008c2b0
