#pragma once
// engine/types/PlacementTool.h — ENDGAME B.3.8 TOOL: the POD is gone.
// `PlacementTool` is a real class now (code/Game/Player/PlacementTool.h,
// compiler vtable). This forwarder keeps the old include spelling working
// until the closing pass deletes it (B8-4).
#include "Player/PlacementTool.h"
