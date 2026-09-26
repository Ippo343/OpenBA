#pragma once
// engine/types/Component.h — ENDGAME B.3.8 pilot: the POD is gone.
// `Component` is a real class now (code/Game/Component/Component.h). The POD's
// field names were wrong at +0x08 (`next` — it is Object::Internal's
// _factoryIndex/_generation) and its slot list was wrong (B38_SLOTS.md §5.3);
// the class carries the retail layout and the retail slot order. This
// forwarder keeps the old include spelling working until the closing pass.
#include "Component/Component.h"

namespace ComponentAddr {
    constexpr uintptr_t Ctor           = 0x1401188b0;
    constexpr uintptr_t Vtable         = 0x140162920; // the Component TABLE (not a dtor)
    constexpr uintptr_t Draw_NoOp      = 0x140118820; // slot 6 base no-op
    constexpr uintptr_t Update_NoOp    = 0x140118a80; // slot 9 base no-op
    constexpr uintptr_t FlagNotify     = 0x1400c3470; // called when flags change & entity != null
}
