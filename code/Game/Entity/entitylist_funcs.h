#pragma once
// EntityList's C entry points left for callers outside block WORK-a's files.
// The bodies are EntityList members (EntityList.h); each line below is a
// one-line forwarder defined in entitylist.cpp (B4 W1_BRIEF "Callers outside
// your files"). The closing wave rewires the callers and deletes these.

#include "EntityList.h"
#include <cstdint>

// (EntityList_RemoveEntity, the last forwarder here, had no caller left: deleted, B4
// wave X. The header stays for its includers' EntityList.h.)
