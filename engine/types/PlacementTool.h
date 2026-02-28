#pragma once
// PlacementTool — Building placement tool, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x48.
// NOTE: "PlacementTool_Ctor" in Ghidra actually decompiles as DragPlacementTool_Ctor.
// The PlacementTool_Factory allocates 0x48 bytes. Uses DragTool inheritance chain.
//
// Decompiled from: PlacementTool_Ctor (0x1400fe850), PlacementTool_Factory (0x14002dc40)
// Inherits: Object::Internal -> Tool -> DragTool (0x38) -> PlacementTool

#include <cstdint>
#include <cstddef>

namespace PlacementToolAddr {
    constexpr uintptr_t Ctor    = 0x1400fe850;
    constexpr uintptr_t Factory = 0x14002dc40;
}

// PlacementTool uses same layout as DragPlacementTool
// See DragPlacementTool.h for full struct definition (0x48 bytes)
