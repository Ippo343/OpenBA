#pragma once
// engine/types/RoadTool.h — ENDGAME B.3.8 TOOL: the POD is gone.
// `RoadTool` is a real class now (code/Game/Player/RoadTool.h, compiler
// vtable). This forwarder keeps the old include spelling working until the
// closing pass deletes it (B8-4).
#include "Player/RoadTool.h"
