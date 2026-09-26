#pragma once
// engine/types/RemovalTool.h — ENDGAME B.3.8 TOOL: the POD is gone.
// `RemovalTool` is a real class now (code/Game/Player/RemovalTool.h, compiler
// vtable). This forwarder keeps the old include spelling working until the
// closing pass deletes it (B8-4).
#include "Player/RemovalTool.h"
