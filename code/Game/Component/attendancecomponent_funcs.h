#pragma once
// Function declarations for deeply reimplemented AttendanceComponent methods.
// Called from engine/reimpl/attendancecomponent.cpp wrappers.

#include "AttendanceComponent.h"
#include <cstdint>

// Init: resolve all internal function pointers (called once at startup)
void AttendanceComponent_InitFuncs();
