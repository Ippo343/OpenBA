#pragma once
// ComponentEntity -- the ECS entity container. Application-x64.exe, build 170910.
//
// ENDGAME B.3.8 K1 (B8-4): the POD that lived here is now the REAL class in
// code/Game/Entity/ComponentEntity.h (fields at the same offsets, asserted
// there). This header forwards so there is never two definitions of one name.
// The address constants below are kept for the unconverted readers.

#include <cstdint>
#include "Entity/ComponentEntity.h"

namespace ComponentEntityAddr {
    constexpr uintptr_t Ctor_Simple  = 0x1400c3550;  // destructor body
    constexpr uintptr_t Ctor_Full    = 0x1400c3620;  // default ctor
    constexpr uintptr_t CopyCtor     = 0x1400c3720;  // ctor from a ComponentDescription
    constexpr uintptr_t Vtable       = 0x14015fa78;
    constexpr uintptr_t RegisterType = 0x14014cfd0;

    // Helper functions
    constexpr uintptr_t RefreshEntityFlags  = 0x1400c3470;
    constexpr uintptr_t ShutdownComponents  = 0x1400c3000;
    constexpr uintptr_t StartupComponents   = 0x1400c30a0;
    constexpr uintptr_t ResizeComponentArray = 0x1400032e0;

    // Type IDs
    constexpr uintptr_t TypeId_Component = 0x1401c72a8;  // Component base class type ID
    constexpr uintptr_t TypeInfo_CE      = 0x1401c5d78;  // ComponentEntity type info ptr
    constexpr uintptr_t TypeId_CE        = 0x1401c5d70;  // ComponentEntity type id
    constexpr uintptr_t TypeInfo_CE_Ext  = 0x1401c5db0;  // (Action's, the adjacent table)
    constexpr uintptr_t TypeId_CE_Ext    = 0x1401c5da8;  // (Action's, the adjacent table)
}
