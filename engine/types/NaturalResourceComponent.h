#pragma once
// NaturalResourceComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x30.
//
// Created via: NaturalResourceComponent_Factory (0x140036550)
// Parent: Component (direct)
//
// Factory does: Component_Ctor + set vtable + init fields
//   vtable = 0x140163e10
//   +0x28 = 0x3f800000 (float 1.0)
//   +0x2c = 0x100 (uint16, two packed bytes: 0x00, 0x01)

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace NaturalResourceComponentAddr {
    constexpr uintptr_t Factory = 0x140036550;
    constexpr uintptr_t Vtable  = 0x140163e10;
}

// ENDGAME B.3.8 CIT: the POD is gone — `NaturalResourceComponent` is a real class now
// (code/Game/Component/NaturalResourceComponent.h, compiler vtable). This forwarder keeps the
// old include spelling working until the closing pass deletes it (B8-4).
#include "Component/NaturalResourceComponent.h"

