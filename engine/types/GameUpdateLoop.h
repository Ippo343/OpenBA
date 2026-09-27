#pragma once
// engine/types/GameUpdateLoop.h — FORWARDER (ENDGAME B.3.8 block LOOP, rule B8-4)
//
// `GameUpdateLoop` is a real C++ class now: code/Game/Game/GameUpdateLoop.h
// (an `Engine::UpdateLoop` subclass with a compiler vtable; the field list,
// offsets and retail slot map live there). This header only forwards, so the
// includers that spell `"GameUpdateLoop.h"` keep compiling; there is one
// definition of the name. The UpdateLoop base's fields are Engine::UpdateLoop's
// (`_resourceList`, `_loadBit`, ...), no longer this POD's `m_` copies.
#include "Game/GameUpdateLoop.h"
