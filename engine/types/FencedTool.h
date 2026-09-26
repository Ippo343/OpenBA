#pragma once
// engine/types/FencedTool.h — ENDGAME B.3.8 TOOL: the POD is gone.
// `FencedTool` is a real class now (code/Game/Player/FencedTool.h, compiler
// vtable). This forwarder keeps the old include spelling working until the
// closing pass deletes it (B8-4).
#include "Player/FencedTool.h"
