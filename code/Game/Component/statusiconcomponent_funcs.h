#pragma once
// Function declarations for deeply reimplemented StatusIconComponent methods.
// Called from engine/reimpl/statusiconcomponent.cpp wrappers.

#include "StatusIconComponent.h"
#include <cstdint>

// ============================================================
// StatusIconComponent — floating status icon display component
// Vtable: 0x140164620
// Size: 0x108 bytes
// ============================================================

// Every body is a member (StatusIconComponent.h): Startup 0x14012bed0 (the tree's
// StatusIconComponent_Serialize), ShowAfterLoad (the body of slot 11 0x14012bf40, the
// tree's StatusIconComponent_OnUpdate2), SetFlag 0x14012c3f0, StatusIconGeometry::Setup
// 0x1400aa440.

// Init: resolve all internal function pointers (called once at startup)
