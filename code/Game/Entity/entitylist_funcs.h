#pragma once
// EntityList's C entry points left for callers outside block WORK-a's files.
// The bodies are EntityList members (EntityList.h); each line below is a
// one-line forwarder defined in entitylist.cpp (B4 W1_BRIEF "Callers outside
// your files"). The closing wave rewires the callers and deletes these.

#include "EntityList.h"
#include <cstdint>

// B4 FORWARD: 1 outside caller(s) (code/Game/Game/gameupdateloop.cpp)
void EntityList_Update(EntityList* self, void* time);        // EntityList::Update 0x1400c4090

// B4 FORWARD: 1 outside caller(s) (code/Game/Game/gameupdateloop.cpp)
void EntityList_PostUpdate(EntityList* self);                // EntityList::PostUpdate 0x1400c43d0

// B4 FORWARD: 2 outside caller(s) (code/Game/Game/gameupdateloop_teardown.cpp)
void EntityList_RemoveEntity(EntityList* self, void* entityIBP);  // EntityList::RemoveEntity 0x1400c38e0

// B4 FORWARD: 1 outside caller(s) (code/Game/Game/gameupdateloop_serialize.cpp)
void EntityList_Serialize(EntityList* self, void* stream);   // EntityList::Serialize 0x1400c44a0
