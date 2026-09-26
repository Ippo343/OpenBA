#pragma once
// ModelComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x170.
//
// Decompiled from: ModelComponent_Ctor
// Parent: Component (direct)
//
// Constructor sets:
//   vtable = 0x140163ba0
//   +0x28: Graphics::MeshInstance sub-object (initialized by MeshInstance ctor)
//   +0x148: Math::Transform sub-object (initialized by Transform ctor)
//   +0x164 = 0 (qword), +0x16C = -1 (int32)
//   flags |= 0x83
//
// Contains large embedded sub-objects — MeshInstance spans +0x28..+0x147
// (0x120 bytes) and Transform spans +0x148..+0x163 (0x1C bytes).

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace ModelComponentAddr {
    constexpr uintptr_t Vtable = 0x140163ba0;
}

// ENDGAME B.3.8 VIS: the POD is gone — `ModelComponent` is a real class now
// (code/Game/Component/ModelComponent.h, compiler vtable). This forwarder keeps the
// old include spelling working until the closing pass deletes it (B8-4).
#include "Component/ModelComponent.h"
