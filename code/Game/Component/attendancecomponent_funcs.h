#pragma once
// Function declarations for deeply reimplemented AttendanceComponent methods.
// Called from engine/reimpl/attendancecomponent.cpp wrappers.
// No dependency on hooks.h -- portable to standalone exe.

#include "AttendanceComponent.h"
#include <cstdint>

// Update (vtable slot [9], 0x140044370): Checks attendance queue count,
// gets building component, processes queue if building is idle.
void AttendanceComponent_Update(AttendanceComponent* self);

// Init: resolve all internal function pointers (called once at startup)
void AttendanceComponent_InitFuncs();
