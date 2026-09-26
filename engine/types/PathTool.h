#pragma once
// engine/types/PathTool.h — ENDGAME B.3.8 TOOL: the POD is gone.
// `PathTool` is a real class now (code/Game/Player/PathTool.h, compiler
// vtable). This forwarder keeps the old include spelling working until the
// closing pass deletes it (B8-4).
#include "Player/PathTool.h"
