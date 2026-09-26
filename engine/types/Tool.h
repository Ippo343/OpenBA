#pragma once
// engine/types/Tool.h — ENDGAME B.3.8 TOOL: the two PODs are gone.
// `Tool` (code/Game/Player/Tool.h) and `DragTool` (code/Game/Player/dragtool.h)
// are real classes with compiler vtables. This forwarder keeps the old include
// spelling working until the closing pass deletes it (B8-4).
#include "Player/Tool.h"
#include "Player/dragtool.h"
