#pragma once
// ============================================================================
// code/Game/Game/Calendar.h — ENDGAME B.4 GAME-a
//
// The 0x18-byte year clock at GameUpdateLoop+0x128 (m_dayCounter).
//
// CLASS NAME UNCONFIRMED: retail gives this object no reflected name, vtable
// or serializer key. "Calendar" is descriptive (the tree called the member a
// day counter; retail counts YEARS: the clock wraps at DAT_14017225c, the
// seconds in a year that the month readers divide by).
// No vtable: a plain struct.
//
// RETAIL (Application-x64.exe):
//   Load 0x140096b00: MemAlloc(0x18, 8); +0x00 = 1; Time(+0x08, 0.0);
//     Time(+0x10, DAT_14017225c)
//   Update 0x140096380: yearTime += dt; yearTime >= yearLength -> ++year,
//     yearTime -= yearLength
//   readers: WeatherSimulation::Update 0x1400944e0 (month from
//     yearTime.AsSeconds() / yearLength), MusicPlayer::Update 0x1400a40e0,
//     the town hall's year / date text
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "system/Value.h"   // System::Time (8 bytes, trivial)

class Calendar {
public:
    int32_t      year;           // +0x00 ctor 1; +1 when yearTime reaches yearLength
    uint8_t      _pad04[4];      // +0x04 not written
    System::Time yearTime;       // +0x08 time into the current year (ctor 0.0)
    System::Time yearLength;     // +0x10 ctor Time(DAT_14017225c)
};

static_assert(sizeof(Calendar) == 0x18, "Calendar is 0x18 (Load MemAlloc(0x18, 8))");
static_assert(offsetof(Calendar, year) == 0x00, "Calendar::year @ +0x00");
static_assert(offsetof(Calendar, _pad04) == 0x04, "Calendar +0x04");
static_assert(offsetof(Calendar, yearTime) == 0x08, "Calendar::yearTime @ +0x08");
static_assert(offsetof(Calendar, yearLength) == 0x10, "Calendar::yearLength @ +0x10");
