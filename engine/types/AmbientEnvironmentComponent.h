#pragma once
// AmbientEnvironmentComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x48.
//
// Decompiled from: AmbientEnvironmentComponent_Ctor
// Parent: Component (direct)
//
// NOTE: Fields +0x28..+0x34 are NOT initialized by ctor (left as Component base state).
// Constructor only initializes:
//   vtable = 0x140158b70
//   +0x38 = 0 (qword), +0x40 = 0 (dword)
//   flags |= 0x88

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace AmbientEnvironmentComponentAddr {
    constexpr uintptr_t Vtable = 0x140158b70;
}

// ENDGAME B.3.8 CIT: the POD is gone — `AmbientEnvironmentComponent` is a real class now
// (code/Game/Component/AmbientEnvironmentComponent.h, compiler vtable). This forwarder keeps the
// old include spelling working until the closing pass deletes it (B8-4).
#include "Component/AmbientEnvironmentComponent.h"

