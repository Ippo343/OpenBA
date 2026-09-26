#pragma once
// engine/types/DragPlacementTool.h — ENDGAME B.3.8 TOOL: the POD is gone.
// `DragPlacementTool` is a real class now (code/Game/Player/
// DragPlacementTool.h, compiler vtable). This forwarder keeps the old include
// spelling working until the closing pass deletes it (B8-4).
#include "Player/DragPlacementTool.h"
