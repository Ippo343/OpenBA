#pragma once
// ============================================================================
// code/Game/Game/EventLog.h — ENDGAME B.4 GAME-a
//
// The 0x28-byte in-game event log at GameUpdateLoop+0x138 (m_eventNotifier):
// the two string tables its messages are formatted from and the message
// array the event-log dialog and the notification pop-ups read.
//
// CLASS NAME UNCONFIRMED: retail gives it no reflected name or vtable;
// "EventLog" is descriptive (the tree's eventlog.cpp).
// No vtable: a plain struct.
//
// RETAIL (Application-x64.exe):
//   Load 0x140096b00: MemAlloc(0x28, 4); qwords +0x00 / +0x08 / +0x18 / +0x20
//     = 0; dword +0x10 = 0x01010101, word +0x14 = 0x0101, then byte +0x10 = 0;
//     later +0x00 = Find("Dialog/StringTable.rsc:events"), +0x08 =
//     Find("Dialog/StringTable.rsc:seasons")
//   Serialize 0x1400952c0 (version 1): Serialize(6, bool*) at +0x10; the last
//     200 entries: uint color, String text, five ints
//   AddEvent 0x140095580 appends through the grow 0x140019b70; Shutdown frees
//     the array with 0x14000a8a0
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "system/String.h"   // System::String (the entry's text)

namespace UI { class StringTable; }
namespace IO { class Stream; }
namespace Object { class InternalBasePtr; }

// One message (stride 0x28): Serialize 0x1400952c0's field order.
struct EventLogEntry {
    uint32_t       color;        // +0x00 ARGB (Serialize uint)
    uint8_t        _pad04[4];    // +0x04 never written by the grow's copy
    System::String text;         // +0x08 formatted message (String::Serialize)
    int32_t        eventType;    // +0x10
    int32_t        severity;     // +0x14
    int32_t        tileIndex;    // +0x18 map cell to jump to, -1 = none (0x140095030)
    int32_t        iconIndex;    // +0x1C (0x1400950d0)
    int32_t        popup;        // +0x20 >= 0 shows the pop-up
    uint8_t        _pad24[4];    // +0x24
};

static_assert(sizeof(EventLogEntry) == 0x28, "EventLogEntry stride 0x28 (0x140019b70)");
static_assert(offsetof(EventLogEntry, color) == 0x00, "EventLogEntry::color @ +0x00");
static_assert(offsetof(EventLogEntry, _pad04) == 0x04, "EventLogEntry +0x04");
static_assert(offsetof(EventLogEntry, text) == 0x08, "EventLogEntry::text @ +0x08");
static_assert(offsetof(EventLogEntry, eventType) == 0x10, "EventLogEntry::eventType @ +0x10");
static_assert(offsetof(EventLogEntry, severity) == 0x14, "EventLogEntry::severity @ +0x14");
static_assert(offsetof(EventLogEntry, tileIndex) == 0x18, "EventLogEntry::tileIndex @ +0x18");
static_assert(offsetof(EventLogEntry, iconIndex) == 0x1C, "EventLogEntry::iconIndex @ +0x1C");
static_assert(offsetof(EventLogEntry, popup) == 0x20, "EventLogEntry::popup @ +0x20");
static_assert(offsetof(EventLogEntry, _pad24) == 0x24, "EventLogEntry +0x24");

class EventLog {
public:
    UI::StringTable* events;      // +0x00 "Dialog/StringTable.rsc:events" (ExternalPtr)
    UI::StringTable* seasons;     // +0x08 "Dialog/StringTable.rsc:seasons" (ExternalPtr)
    bool           field_0x10[6]; // +0x10 Serialize(6, bool*); ctor {0, 1, 1, 1, 1, 1}
    uint8_t        _pad16[2];     // +0x16
    EventLogEntry* entries;       // +0x18 Array<Entry> data
    int32_t        entryCount;    // +0x20
    int32_t        entryCapacity; // +0x24

    // ---- members (B4 GAME-a; bodies in gameupdateloop_serialize.cpp) ----
    void Serialize(IO::Stream& stream);   // 0x1400952c0

    // ---- members (B4 GAME-a; bodies in eventlog.cpp) ----
    // entityIBP: the entity the message jumps to (null / empty: no location).
    void AddEvent(int eventType, int severity, const Object::InternalBasePtr* entityIBP);   // 0x140095580
    void AddEventWithName(int eventType, int severity, const Object::InternalBasePtr* entityIBP,
                          const System::String* name);                                    // 0x140095750
    void AddCitizenEvent(int eventType, int severity, const Object::InternalBasePtr* entityIBP,
                         const System::String* name, const System::String* title,
                         const System::String* extra);                                    // 0x140094e00
    // The three writers' one shared body (retail inlines it in each).
    void AddEventCore(int eventType, int severity, const Object::InternalBasePtr* entityIBP,
                      const char* const* extra, int nExtra);
};

static_assert(sizeof(EventLog) == 0x28, "EventLog is 0x28 (Load MemAlloc(0x28, 4))");
static_assert(offsetof(EventLog, events) == 0x00, "EventLog::events @ +0x00");
static_assert(offsetof(EventLog, seasons) == 0x08, "EventLog::seasons @ +0x08");
static_assert(offsetof(EventLog, field_0x10) == 0x10, "EventLog +0x10");
static_assert(offsetof(EventLog, _pad16) == 0x16, "EventLog +0x16");
static_assert(offsetof(EventLog, entries) == 0x18, "EventLog::entries @ +0x18");
static_assert(offsetof(EventLog, entryCount) == 0x20, "EventLog::entryCount @ +0x20");
static_assert(offsetof(EventLog, entryCapacity) == 0x24, "EventLog::entryCapacity @ +0x24");
