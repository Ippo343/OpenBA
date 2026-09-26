#pragma once
// engine/types/BridgeTool.h — ENDGAME B.3.8 TOOL: the POD is gone.
// `BridgeTool` is a real class now (code/Game/Player/BridgeTool.h, compiler
// vtable). This forwarder keeps the old include spelling working until the
// closing pass deletes it (B8-4).
#include "Player/BridgeTool.h"
