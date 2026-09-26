#pragma once
// engine/types/EditTool.h — ENDGAME B.3.8 TOOL: the POD is gone.
// `EditTool` is a real class now (code/Game/Player/EditTool.h, compiler
// vtable). This forwarder keeps the old include spelling working until the
// closing pass deletes it (B8-4).
#include "Player/EditTool.h"
