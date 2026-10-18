// src/game/Game/serialize_scope.h -- GameUpdateLoop::SerializeAll's state across its two halves.
//
// NOT RETAIL as a type (ext.background_save, docs/extensions/handins/HITCH_SAVESNAP.md). Retail's
// SerializeAll (0x140095a90) is one straight-line body whose version-3 IO::Serializer is a
// block-scoped local. The tree cuts that body after PlayerController::Serialize
// (SerializeAllHead / SerializeAllBody, gameupdateloop_serialize.cpp); this holds what crosses the
// cut: the Serializer in raw storage (constructed by the head, ended by the body where the
// block-scoped local ended), the reference-id memo flag and the print-only direction label.
#pragma once

#include "Game/GameUpdateLoop.h"
#include "io/Serializer.h"

namespace IO { class CompressedMemoryStream; }
namespace Save { class SavedState; }

struct GameUpdateLoop::SerializeScope {
    alignas(IO::Serializer) unsigned char serStorage[sizeof(IO::Serializer)];
    IO::Serializer* ser       = nullptr;   // the object placement-built in serStorage
    bool            refIdMemo = false;
    const char*     serDir    = "";
    IO::Serializer& Ser() { return *ser; }
};

// ext.background_save's worker half of GameUpdateLoop::SaveRestore's save arm
// (gameupdateloop_serialize.cpp): SerializeAllBody, the B / C manifest tail, Close and Store --
// the statements that follow SerializeAll there, in the same order.
void GameUpdateLoop_SaveRestoreBody(GameUpdateLoop* self, IO::CompressedMemoryStream* cms,
                                    Save::SavedState* savedState,
                                    GameUpdateLoop::SerializeScope& scope);
