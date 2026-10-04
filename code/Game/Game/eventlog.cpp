// code/Game/Game/eventlog.cpp
// Clean reimplementation of the in-game event log writer.
//
// The event-log manager is the 0x28-byte struct at GameUpdateLoop+0x138 (field
// [0x27]); g_pGameWorldSingleton IS the GameUpdateLoop. Layout:
//   +0x00 events StringTable   +0x08 seasons StringTable
//   +0x10 5 category-filter bytes
//   +0x18 event array (0x28-byte entries)   +0x20 count   +0x24 capacity
//
// Event entry (0x28 bytes):
//   +0x00 uint32 ARGB color   +0x08 System::String text
//   +0x10 int eventType   +0x14 int severity   +0x18 int tileIdx
//   +0x1c int iconIndex   +0x20 int popup (>=0 shows popup)
//
// Reimplements (from Application-x64.exe), as EventLog members where retail's
// receiver is the log:
//   AddEvent          0x140095580  — format text, build entry, append
//   AddEventWithName  0x140095750  — {2}=name
//   AddCitizenEvent   0x140094e00  — citizen life-event variant: extra format args
//                                    {2}=citizen name {3}=title {4}=param string, plus
//                                    drop-to-1x-speed tail on critical events
//   EventArray_Grow   0x140019b70  — vector<Event>::resize/append primitive
//   YearString        0x1400951d0  — String::Format("%i", year)
//   SeasonString      0x140095230  — seasons table lookup by month index
//   IconIndex         0x1400950d0  — eventType -> icon index switch
//   TileIndex         0x140095030  — entity -> map tile index (null entity => -1)

#include "ui/UserInterface.h"
#include "Game/exe_bodies.h"   // B3R STORAGE: g_gameSettings
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "Game/GameUpdateLoop.h"   // caller batch CB6: the world's named fields
#include "Game/Calendar.h"   // m_dayCounter's class (B.4 GAME-a)
#include "Game/Map.h"   // m_game's class (B.4 GAME-a)
#include "Game/EventLog.h"   // the class these bodies are members of (B.4 GAME-a)
#include "Game/PlayerToolManager.h"   // m_toolManager (the speed tail)
#include "Component/MapComponent.h"   // the tile index's footprint
#include "object/Internal.h"
#include "Component/exe_bodies.h"   // FloorComponent_FindResData
#include "system/String.h"
#include "collection/Collection.h"
#include <new>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "system/Value.h"      // ENDGAME B.3.4b System caller batch (Random/Time/Color)
#include "ui/StringTable.h"    // ENDGAME B.3.6 UI callers (K4)
#include "Game/exe_bodies.h"

// ============================================================
// EventArray_Grow (0x140019b70) — grow-and-append primitive
// ============================================================
// arr points to {data* @+0, count @+8, capacity @+0xc}. 0x28-byte stride, a
// System::String at each entry +0x08. Faithful port: triple-capacity heuristic on
// a single-element grow, copy preserved entries (String copy-ctor), append the new
// entry, then destruct/free the old buffer's Strings.
//
// ENDGAME B.3.2 step 2: the body below WAS a hand-written copy of
// `rtcoll::SetSizeCore` in its three-argument (fill) form. It is now one
// `Collection::Array<>::SetSizeFill` driven by this instantiation's element ops,
// spelled as an `ArraySpec` row. Compared arm for arm against retail 0x140019b70
// (re-decompiled 2026-09-13) and against SetSizeCore before the swap:
//   * exact-equal early return; `_size` stored before the capacity test;
//   * capacity = newSize, trebled when newSize == oldSize+1 && oldSize != 0;
//   * MemAlloc(capacity * 0x28, align 8);
//   * loop from 0 (moved) or oldSize (in place) to newSize, skipping a null
//     computed element address; i < oldSize copies old[i], else copies `fill`
//     (retail's else arm is UNCONDITIONALLY the fill — there is no init arm);
//   * moved: ~String on every old element, then MemFree(old) — retail calls
//     MemFree without an `if (old)` (the null test is inside 0x1800c7ad0); the
//     previous port's `else if (oldData)` was equivalent, not extra;
//   * in place and shrinking: ~String on the truncated tail.
// Two deliberate differences, both out-of-memory-only: the allocation goes
// through the LOCKING static `?MemAlloc@MemoryAllocator@System@@` instead of
// the unlocked instance `Allocate` this body reached through `fn_MemAlloc`
// (same heap), and a null `fill` no longer faults (retail would; SetSizeCore
// falls through to the empty init arm). Every caller in the tree passes a
// non-null entry (AddEventCore below, eventlog_achievement.cpp,
// Component/diseasecomponent.cpp).
namespace {

// The element: EventLogEntry (EventLog.h), stride 0x28, align 8.
// +0x04..+0x07 is padding retail's assign never writes. This is a fill-only
// instantiation (init == nullptr): retail 0x140019b70 has no default arm.
// The element ops are written on EventLogEntry and reach the erased core
// through Collection.h's element adapters (B4 P2 GAME-1).
void CopyEventEntry(EventLogEntry* dst, const EventLogEntry* src) {
    dst->color = src->color;
    new (&dst->text) System::String(src->text);
    dst->eventType = src->eventType;
    dst->severity = src->severity;
    dst->tileIndex = src->tileIndex;
    dst->iconIndex = src->iconIndex;
    dst->popup = src->popup;
}

void DestroyEventEntries(EventLogEntry* entries, int count) {
    for (int i = 0; i < count; ++i)
        entries[i].text.~String();
}
void AssignEventEntry(uint8_t* d, const uint8_t* s) {
    rtcoll::AssignAs<EventLogEntry, &CopyEventEntry>(d, s);
}
void DestroyEventEntry(void* ignoredThis, uint8_t* base, int count) {
    rtcoll::DestroyAs<EventLogEntry, &DestroyEventEntries>(ignoredThis, base, count);
}

// stays explicit: byteCopy=false with a String copy-ctor `assign`, a real
// `destroy`, and an `assign unwritten` extent ([0x04,0x08) padding) — a
// combination no C++ element type can express.
const rtcoll::ArraySpec kSpec_EventEntry = {            // exe 0x140019b70 (grow),
    "Array<EventLog::Entry>", 0x28, 8,                  //     0x14000a8a0 (~Array)
    &AssignEventEntry, nullptr, &DestroyEventEntry, false,
    /*assign unwritten*/ 0x04, 0x08,
    /*init   unwritten*/ 0,    0,
    /*seed zero       */ 0,    0 };

using EventArray = Collection::Array<uint8_t, Collection::DefaultAllocator,
                                     Collection::ExplicitSpec<&kSpec_EventEntry>>;

}  // namespace

// `arrBase` is the log's {entries, entryCount, entryCapacity} header.
static void EventArray_Grow(EventLogEntry** arrBase, int newCount, const EventLogEntry* newEntry) {
    // B.3.2 bridge: EventLog +0x18 is declared as the Array's three fields.
    reinterpret_cast<EventArray*>(arrBase)->SetSizeFill(newCount, newEntry);   // LEFT: EventLog.h H5: entries is the Array's three raw fields (gameupdateloop_load.cpp asserts EventLog trivially constructible)
}

// The matching destructor, FUN_14000a8a0 (retail's EventNotifier cleanup): ~String
// on each live entry's +0x08, MemFree the buffer, zero the header. Exported for
// game_leaf_helpers.cpp's Game_EventNotifierCleanup so the element row lives in
// exactly one place.
extern "C" void EventLog_EventArray_Destroy(void* arrBase) {
    reinterpret_cast<EventArray*>(arrBase)->~Array();   // LEFT: SIG EventLog_EventArray_Destroy (Game_EventNotifierCleanup's void* from gameupdateloop.cpp); EventLog.h H5 as above
}

// ============================================================
// Helpers
// ============================================================

// YearString (0x1400951d0): String::Format("%i", *(int*)clock), clock = *(gw+0x128).
static System::String EventLog_YearString(GameUpdateLoop* gameWorld) {
    Calendar* clock = gameWorld->m_dayCounter;
    int year = clock ? clock->year : 0;
    return System::String::Format("%i", year);
}

// SeasonString (0x140095230): monthIdx = (int)(AsSeconds(clock+8) / 3600 * 12);
// if monthIdx >= 12 -> 0. seasons table = eventLog->seasons (+0x08).
static System::String EventLog_SeasonString(GameUpdateLoop* gameWorld, const EventLog* eventLog) {
    Calendar* clock = gameWorld->m_dayCounter;
    int idx = 0;
    if (clock) {
        float secs = clock->yearTime.AsSeconds();
        idx = (int)(secs / 3600.0f * 12.0f);
        if (idx >= 12) idx = 0;
    }
    System::String retStr;
    const UI::StringTable* seasonsTable = eventLog->seasons;
    if (seasonsTable) {
        ::new (&retStr) System::String(seasonsTable->GetText(idx));  // B.3.6 bridge
    } else {
        retStr = System::String("");
    }
    return retStr;
}

// SeverityColor: severity -> ARGB text color (shared by both AddEvent variants).
static uint32_t EventLog_SeverityColor(int severity) {
    switch (severity) {
        case 1:  return 0xFFFFFF00u;  // yellow
        case 2:  return 0xFF8080FFu;  // pink
        case 3:  return 0xFFFF6060u;  // red
        case 4:  return 0xFFEA7517u;  // orange
        default: return 0xFFFFFFFFu;  // white
    }
}

// PopupIndex: eventType -> popup icon index (0x1a/0x1c disaster start, 0x1b/0x1d
// disaster end; -1 = no modal popup). EventNotifyDialog raises EventPopupDialog
// when the entry's +0x20 field is >= 0.
static int EventLog_PopupIndex(int eventType) {
    if (eventType == 0x1a || eventType == 0x1c) return 0xc;
    if (eventType == 0x1b || eventType == 0x1d) return 0xd;
    return -1;
}

// TileIndex (0x140095030): entity IBP -> map tile index for the message's
// jump-to-location, from the entity's MapComponent (FUN_140004680, registered)
// footprint center: (y + h/2) * mapWidth + (x + w/2). mapWidth = the Map's size
// (gw+0xF8). Null/stale entity or missing MapComponent => -1.
static int EventLog_TileIndex(const ::Object::InternalBasePtr* entityIBP) {
    if (!entityIBP || entityIBP->Get() == nullptr) return -1;
    GameUpdateLoop* gameWorld = GameUpdateLoop::_singleton;
    if (!gameWorld) return -1;
    Map* game = gameWorld->m_game;
    if (!game) return -1;

    MapComponent* mc = nullptr;   // the InternalPtr<MapComponent> FindResData writes
    FloorComponent_FindResData(entityIBP->Get(), &mc);   // 0x140004680
    if (mc == nullptr) return -1;

    // The footprint words, read signed (the tree's reads; retail MOVSX).
    int x = static_cast<int16_t>(mc->tileX0), y = static_cast<int16_t>(mc->tileY0);
    int w = static_cast<int16_t>(mc->tileX1), h = static_cast<int16_t>(mc->tileY1);
    return ((h >> 1) + y) * game->size + (w >> 1) + x;
}

// B4 FORWARD: 1 outside caller file (Component/diseasecomponent.cpp).
// The original 0x140095030 is (unused RCX, entityIBP RDX) — the first param is
// dead (stack-reuse artifact).
extern "C" int EventLog_TileIndex_Reg(void* /*unused*/, void* entityIBP) {
    return EventLog_TileIndex(reinterpret_cast<const ::Object::InternalBasePtr*>(entityIBP));
}

// IconIndex (0x1400950d0): eventType -> icon index (feeds "EventIcon%i").
static int EventLog_IconIndex(int eventType) {
    switch (eventType) {
        case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 7: return 0;
        case 0xb: return 1;
        case 0xc: return 2;
        case 0xd: return 5;
        case 0xe: return 8;
        case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x16: return 6;
        case 0x17: case 0x18: return 7;
        case 0x19: return 9;
        case 0x1a: case 0x1c: return 10;
        case 0x1b: case 0x1d: return 0xb;
        case 0x1e: return 4;
        case 0x1f: return 3;
        default: return -1;
    }
}

// ============================================================
// AddEvent family core (0x140095580 / 0x140095750 / 0x140094e00)
// ============================================================
// Retail ships THREE event-log writers that are byte-identical apart from how
// many extra args they hand System::String::FormatIndexed:
//
//   0x140095580 AddEvent          {0}=season {1}=year
//   0x140095750 AddEventWithName  ... {2}=name          (scenario win/loss)
//   0x140094e00 AddCitizenEvent   ... {2}=name {3}=title {4}=extra
//
// Everything else — the severity->colour table, the popup/icon switches, the
// entity->tile lookup, the 0x28-byte entry append, and the "drop to 1x speed on
// a critical event" tail — is the same instruction sequence in all three, so
// they share one core here. Porting the same body three times is how the copies
// drift apart (see reference_one_retail_fn_ported_twice_diverges); that already
// happened here — the previous AddEvent port was missing the speed tail that
// retail 0x140095580 does have.
//
// entityIBP is an optional InternalBasePtr* for the map-jump tile (null =>
// tileIdx -1, no location). `extra` holds the String DATA pointers (char*) for
// the {2}.. args, exactly as the original passes them.
void EventLog::AddEventCore(int eventType, int severity, const ::Object::InternalBasePtr* entityIBP,
                            const char* const* extra, int nExtra) {
    GameUpdateLoop* gameWorld = GameUpdateLoop::_singleton;
    if (!gameWorld) return;

    // --- Build the display text ---
    System::String templateStr, formatted;
    System::String yearStr   = EventLog_YearString(gameWorld);
    System::String seasonStr = EventLog_SeasonString(gameWorld, this);

    const UI::StringTable* eventsTable = events;  // +0x00
    if (eventsTable) {
        ::new (&templateStr) System::String(eventsTable->GetText(eventType));  // B.3.6 bridge
    } else {
        templateStr = System::String("Event");
    }

    // FormatIndexed(fmt=template, %0=season, %1=year, %2..=extra). Args are the
    // String DATA pointers (char*). Fall back to the raw template if the export
    // is unresolved.
    const char* tmplData = templateStr._data;
    const char* seaData  = seasonStr._data;
    const char* yrData   = yearStr._data;
    switch (nExtra) {
        case 0:
            formatted = System::String::FormatIndexed(tmplData, seaData, yrData);
            break;
        case 1:
            formatted = System::String::FormatIndexed(tmplData, seaData, yrData,
                                                      extra[0]);
            break;
        default:  // 3 — the citizen variant
            formatted = System::String::FormatIndexed(tmplData, seaData, yrData,
                                                      extra[0], extra[1], extra[2]);
            break;
    }

    uint32_t color = EventLog_SeverityColor(severity);
    int popup      = EventLog_PopupIndex(eventType);
    int iconIndex  = EventLog_IconIndex(eventType);
    int tileIdx    = EventLog_TileIndex(entityIBP);  // null entity -> -1

    // --- Build the 0x28-byte entry and append ---
    // Raw storage: only the text is constructed (and destroyed) below, as retail's
    // stack entry is.
    alignas(8) uint8_t entryBuf[sizeof(EventLogEntry)];
    // LEFT: the entry is raw stack storage (H5: EventLogEntry holds a String).
    EventLogEntry* entry = reinterpret_cast<EventLogEntry*>(entryBuf);   // LEFT: H5 raw stack entry (a real EventLogEntry local would run String's ctor / dtor)

    entry->color = color;
    new (&entry->text) System::String(formatted);
    entry->eventType = eventType;
    entry->severity = severity;
    entry->tileIndex = tileIdx;
    entry->iconIndex = iconIndex;
    entry->popup = popup;

    int count = entryCount;
    EventArray_Grow(&entries, count + 1, entry);

    // Cleanup the entry's String (the grow copied it); yearStr/seasonStr/
    // templateStr/formatted are released by their own scope.
    entry->text.~String();

    // --- Tail: critical events (0xb/0xc/0x1e/0x1f) drop the game to 1x speed
    // when the settings byte +0x17 ("pause on events") is set. Speed lives in
    // the tool manager (gw+0x120)+0x34. Present in ALL THREE retail writers.
    if (eventType > 0xa && (eventType < 0xd || (unsigned)(eventType - 0x1e) < 2u)) {
        if (g_gameSettings != nullptr &&
            g_gameSettings->autoSlow != 0) {
            PlayerToolManager* speedObj = gameWorld->m_toolManager;
            if (speedObj) speedObj->field_0x34 = 1;
        }
    }
}

// ============================================================
// AddEvent (0x140095580)
// ============================================================
// The plain overload: "{0}=season {1}=year".
void EventLog::AddEvent(int eventType, int severity, const ::Object::InternalBasePtr* entityIBP) {
    AddEventCore(eventType, severity, entityIBP, nullptr, 0);
}

// The forwarders below convert their untyped arguments with reinterpret_cast
// (each is the pointer the old signature carried).
// B4 FORWARD: 5 outside caller files (Component/firecomponent_ignite.cpp,
// immigrationcomponent_states.cpp, tornadocomponent.cpp, tradecomponent_states.cpp,
// Entity/citizenbehavior.cpp; citizen_updatehealth / citizen_lifeevent reach the
// citizen variant below).
extern "C" void EventLog_AddEvent(void* eventLog, int eventType, int severity, void* entityIBP) {
    if (!eventLog) return;
    reinterpret_cast<EventLog*>(eventLog)->AddEvent(eventType, severity,
        reinterpret_cast<const ::Object::InternalBasePtr*>(entityIBP));
}

// ============================================================
// AddEventWithName (0x140095750)
// ============================================================
// The named overload. Retail callers: GameUpdateLoop::OnScenarioComplete
// (0x140096830) — event types 0x1a/0x1b (scenario failed/won) with the parent
// Achievement's title as {2} — and the award toast (eventlog_achievement.cpp).
void EventLog::AddEventWithName(int eventType, int severity, const ::Object::InternalBasePtr* entityIBP,
                                const System::String* name) {
    const char* extra[1] = { name ? name->_data : "" };
    AddEventCore(eventType, severity, entityIBP, extra, 1);
}

// ============================================================
// AddCitizenEvent (0x140094e00)
// ============================================================
// Citizen life-event variant (only exe caller: CitizenComponent_PostLifeEvent
// 0x140113af0, the former "AssignState" misnomer). Three extra format args —
// {2}=citizen name {3}=title (Child/Student/profession) {4}=extra param string.
void EventLog::AddCitizenEvent(int eventType, int severity, const ::Object::InternalBasePtr* entityIBP,
                               const System::String* name, const System::String* title,
                               const System::String* extraStr) {
    const char* extra[3] = {
        name     ? name->_data     : "",
        title    ? title->_data    : "",
        extraStr ? extraStr->_data : "",
    };
    AddEventCore(eventType, severity, entityIBP, extra, 3);
}

// B4 FORWARD: 2 outside caller files (Component/citizen_lifeevent.cpp,
// Component/citizen_updatehealth.cpp).
extern "C" void EventLog_AddCitizenEvent(void* eventLog, int eventType, int severity,
                                         void* entityIBP, void* nameStr, void* titleStr,
                                         void* extraStr) {
    if (!eventLog) return;
    reinterpret_cast<EventLog*>(eventLog)->AddCitizenEvent(eventType, severity,
        reinterpret_cast<const ::Object::InternalBasePtr*>(entityIBP),
        reinterpret_cast<const System::String*>(nameStr), reinterpret_cast<const System::String*>(titleStr),
        reinterpret_cast<const System::String*>(extraStr));
}

// ============================================================
// Shared-leaf exports (for the other event writers)
// ============================================================
// These three are retail leaves used by more than one event writer. The crop
// infestation writer (DiseaseComponent::Infect, 0x1400483a0) inlines a THIRD
// entry-append variant — 3 format args, fixed colour 0xFFFF6060, icon 2,
// popup -1 — so it needs the primitives rather than AddEvent/AddCitizenEvent.
// Exported here instead of re-ported there: porting one retail function twice
// lets the two copies diverge (see reference_one_retail_fn_ported_twice_diverges).
// B4 FORWARD: the three below have 1 outside caller file (Component/diseasecomponent.cpp).

extern "C" void EventLog_ArrayGrow_Reg(void* arrBase, int newCount, const void* newEntry) {
    EventArray_Grow(reinterpret_cast<EventLogEntry**>(arrBase), newCount,
                    reinterpret_cast<const EventLogEntry*>(newEntry));
}

extern "C" void EventLog_YearString_Reg(void* gameWorld, void* retStr) {
    // Retail constructs the result String in the caller's return slot.
    new (retStr) System::String(EventLog_YearString(reinterpret_cast<GameUpdateLoop*>(gameWorld)));
}

extern "C" void EventLog_SeasonString_Reg(void* gameWorld, void* eventLog, void* retStr) {
    new (retStr) System::String(EventLog_SeasonString(reinterpret_cast<GameUpdateLoop*>(gameWorld),
                                                      reinterpret_cast<const EventLog*>(eventLog)));
}
