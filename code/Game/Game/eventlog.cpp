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
// Reimplements (from Application-x64.exe):
//   AddEvent          0x140095580  — format text, build entry, append
//   AddCitizenEvent   0x140094e00  — citizen life-event variant: extra format args
//                                    {2}=citizen name {3}=title {4}=param string, plus
//                                    drop-to-1x-speed tail on critical events
//   EventArray_Grow   0x140019b70  — vector<Event>::resize/append primitive
//   YearString        0x1400951d0  — String::Format("%i", year)
//   SeasonString      0x140095230  — seasons table lookup by month index
//   IconIndex         0x1400950d0  — eventType -> icon index switch
//   TileIndex         0x140095030  — entity -> map tile index (null entity => -1)

#include "engine_funcs.h"
#include "Game/GameUpdateLoop.h"   // caller batch CB6: the world's named fields
#include "resolve.h"
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

// The element: {uint32 color @+0x00, System::String text @+0x08,
//               int32 eventType @+0x10, severity @+0x14, tileIdx @+0x18,
//               iconIndex @+0x1c, popup @+0x20}, stride 0x28, align 8.
// +0x04..+0x07 is padding retail's assign never writes. This is a fill-only
// instantiation (init == nullptr): retail 0x140019b70 has no default arm.
void AssignEventEntry(uint8_t* d, const uint8_t* s) {
    // B.3.2 bridge: the 0x28-byte event entry is still untyped bytes.
    *reinterpret_cast<uint32_t*>(d + 0x00) = *reinterpret_cast<const uint32_t*>(s + 0x00);
    new (d + 0x08) System::String(*reinterpret_cast<const System::String*>(s + 0x08));
    *reinterpret_cast<uint32_t*>(d + 0x10) = *reinterpret_cast<const uint32_t*>(s + 0x10);
    *reinterpret_cast<uint32_t*>(d + 0x14) = *reinterpret_cast<const uint32_t*>(s + 0x14);
    *reinterpret_cast<uint32_t*>(d + 0x18) = *reinterpret_cast<const uint32_t*>(s + 0x18);
    *reinterpret_cast<uint32_t*>(d + 0x1c) = *reinterpret_cast<const uint32_t*>(s + 0x1c);
    *reinterpret_cast<uint32_t*>(d + 0x20) = *reinterpret_cast<const uint32_t*>(s + 0x20);
}

void DestroyEventEntry(void* /*ignoredThis*/, uint8_t* base, int count) {
    for (int i = 0; i < count; ++i)
        reinterpret_cast<System::String*>(base + static_cast<int64_t>(i) * 0x28 + 0x08)->~String();
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

static void EventArray_Grow(void* arrBase, int newCount, const void* newEntry) {
    // B.3.2 bridge: the event-log manager (GameUpdateLoop+0x138 -> +0x18) is
    // still a raw 0x10-byte array header in an unconverted exe struct.
    reinterpret_cast<EventArray*>(arrBase)->SetSizeFill(newCount, newEntry);
}

// The matching destructor, FUN_14000a8a0 (retail's EventNotifier cleanup): ~String
// on each live entry's +0x08, MemFree the buffer, zero the header. Exported for
// game_leaf_helpers.cpp's Game_EventNotifierCleanup so the element row lives in
// exactly one place.
extern "C" void EventLog_EventArray_Destroy(void* arrBase) {
    reinterpret_cast<EventArray*>(arrBase)->~Array();   // B.3.2 bridge, as above
}

// ============================================================
// Helpers
// ============================================================

// YearString (0x1400951d0): String::Format("%i", *(int*)clock), clock = *(gw+0x128).
static System::String EventLog_YearString(void* gameWorld) {
    void* clock = reinterpret_cast<GameUpdateLoop*>(gameWorld)->m_dayCounter;   // B.3.8 bridge
    int year = clock ? *(int*)clock : 0;
    return System::String::Format("%i", year);
}

// SeasonString (0x140095230): monthIdx = (int)(AsSeconds(clock+8) / 3600 * 12);
// if monthIdx >= 12 -> 0. seasons table = *(eventlog+0x08). GetText(table, ret, idx).
static System::String EventLog_SeasonString(void* gameWorld, void* eventLog) {
    void* clock = reinterpret_cast<GameUpdateLoop*>(gameWorld)->m_dayCounter;   // B.3.8 bridge
    int idx = 0;
    if (clock) {
        float secs = reinterpret_cast<const System::Time*>((char*)clock + 8)->AsSeconds();   // B.3.4b bridge
        idx = (int)(secs / 3600.0f * 12.0f);
        if (idx >= 12) idx = 0;
    }
    System::String retStr;
    void* seasonsTable = *(void**)((uintptr_t)eventLog + 0x08);
    if (seasonsTable) {
        ::new (&retStr) System::String(
            reinterpret_cast<const UI::StringTable*>(seasonsTable)->GetText(idx));  // B.3.6 bridge
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
// footprint center: (y + h/2) * mapWidth + (x + w/2). mapWidth = first int of
// Game* (gw+0xF8). Null/stale entity or missing MapComponent => -1.
static int EventLog_TileIndex(void* entityIBP) {
    if (!entityIBP || *(int64_t*)entityIBP == 0) return -1;
    void* gameWorld = g_pGameWorldSingleton ? *g_pGameWorldSingleton : nullptr;
    if (!gameWorld) return -1;
    int* game = static_cast<int*>(reinterpret_cast<GameUpdateLoop*>(gameWorld)->m_game);   // B.3.8 bridge
    if (!game) return -1;

    typedef void* (*fn_FindCompMember_t)(int64_t self, void* outIBP);
    static fn_FindCompMember_t s_findMap = nullptr;
    if (!s_findMap)
        s_findMap = reinterpret_cast<fn_FindCompMember_t>(ResolveAddr(Module::Exe, 0x140004680));
    if (!s_findMap) return -1;

    int64_t mapIBP = 0;
    s_findMap(*(int64_t*)entityIBP, &mapIBP);
    if (mapIBP == 0) return -1;

    const uint8_t* mc = (const uint8_t*)mapIBP;
    int x = *(const int16_t*)(mc + 0x28), y = *(const int16_t*)(mc + 0x2a);
    int w = *(const int16_t*)(mc + 0x2c), h = *(const int16_t*)(mc + 0x2e);
    return ((h >> 1) + y) * (*game) + (w >> 1) + x;
}

// ABI shim for the address registry: the original 0x140095030 is
// (unused RCX, entityIBP RDX) — the first param is dead (stack-reuse artifact).
extern "C" int EventLog_TileIndex_Reg(void* /*unused*/, void* entityIBP) {
    return EventLog_TileIndex(entityIBP);
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
// eventLog = *(gameWorld+0x138). entityIBP is an optional InternalBasePtr* for
// the map-jump tile (null => tileIdx -1, no location). `extra` holds the String
// DATA pointers (char*) for the {2}.. args, exactly as the original passes them.
static void EventLog_AddEventCore(void* eventLog, int eventType, int severity,
                                  void* entityIBP,
                                  const char* const* extra, int nExtra,
                                  const char* /*traceTag*/) {
    if (!eventLog) return;
    void* gameWorld = g_pGameWorldSingleton ? *g_pGameWorldSingleton : nullptr;
    if (!gameWorld) return;

    // --- Build the display text ---
    System::String templateStr, formatted;
    System::String yearStr   = EventLog_YearString(gameWorld);
    System::String seasonStr = EventLog_SeasonString(gameWorld, eventLog);

    void* eventsTable = *(void**)eventLog;  // +0x00
    if (eventsTable) {
        ::new (&templateStr) System::String(
            reinterpret_cast<const UI::StringTable*>(eventsTable)->GetText(eventType));  // B.3.6 bridge
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
    uint8_t entry[0x28];
    *(uint32_t*)(entry + 0x00) = color;
    new (entry + 0x08) System::String(formatted);
    *(int32_t*)(entry + 0x10) = eventType;
    *(int32_t*)(entry + 0x14) = severity;
    *(int32_t*)(entry + 0x18) = tileIdx;
    *(int32_t*)(entry + 0x1c) = iconIndex;
    *(int32_t*)(entry + 0x20) = popup;

    int count = *(int*)((uintptr_t)eventLog + 0x20);
    EventArray_Grow((void*)((uintptr_t)eventLog + 0x18), count + 1, entry);

    // Cleanup the entry's String (the grow copied it); yearStr/seasonStr/
    // templateStr/formatted are released by their own scope.
    reinterpret_cast<System::String*>(entry + 0x08)->~String();

    // --- Tail: critical events (0xb/0xc/0x1e/0x1f) drop the game to 1x speed
    // when the settings byte +0x17 ("pause on events") is set. Speed lives in
    // the EntityUI obj (gw+0x120)+0x34. Present in ALL THREE retail writers.
    if (eventType > 0xa && (eventType < 0xd || (unsigned)(eventType - 0x1e) < 2u)) {
        if (g_pGameSettingsPtr && *g_pGameSettingsPtr &&
            *((const char*)*g_pGameSettingsPtr + 0x17) != 0) {
            void* speedObj = reinterpret_cast<GameUpdateLoop*>(gameWorld)->m_entityUI;   // B.3.8 bridge
            if (speedObj) *(int32_t*)((uintptr_t)speedObj + 0x34) = 1;
        }
    }
}

// ============================================================
// AddEvent (0x140095580)
// ============================================================
// The plain overload: "{0}=season {1}=year".
extern "C" void EventLog_AddEvent(void* eventLog, int eventType, int severity, void* entityIBP) {
    EventLog_AddEventCore(eventLog, eventType, severity, entityIBP,
                          nullptr, 0, "AddEvent");
}

// ============================================================
// AddEventWithName (0x140095750)
// ============================================================
// The named overload. Sole retail caller is GameUpdateLoop::OnScenarioComplete
// (0x140096830) — event types 0x1a/0x1b (scenario failed/won) with the parent
// Achievement's title as {2}. nameStr is a System::String* (data ptr at +0x00).
extern "C" void EventLog_AddEventWithName(void* eventLog, int eventType, int severity,
                                          void* entityIBP, void* nameStr) {
    const char* extra[1] = { nameStr ? *(const char**)nameStr : "" };
    EventLog_AddEventCore(eventLog, eventType, severity, entityIBP,
                          extra, 1, "AddEventWithName");
}

// ============================================================
// AddCitizenEvent (0x140094e00)
// ============================================================
// Citizen life-event variant (only exe caller: CitizenComponent_PostLifeEvent
// 0x140113af0, the former "AssignState" misnomer). Three extra format args —
// {2}=citizen name {3}=title (Child/Student/profession) {4}=extra param string.
// nameStr/titleStr/extraStr are System::String* (data ptr at +0x00).
extern "C" void EventLog_AddCitizenEvent(void* eventLog, int eventType, int severity,
                                         void* entityIBP, void* nameStr, void* titleStr,
                                         void* extraStr) {
    const char* extra[3] = {
        nameStr  ? *(const char**)nameStr  : "",
        titleStr ? *(const char**)titleStr : "",
        extraStr ? *(const char**)extraStr : "",
    };
    EventLog_AddEventCore(eventLog, eventType, severity, entityIBP,
                          extra, 3, "AddCitizenEvent");
}

// Convenience: add a location-less event to the live game's event log.
extern "C" void EventLog_AddSimple(int eventType, int severity) {
    void* gameWorld = g_pGameWorldSingleton ? *g_pGameWorldSingleton : nullptr;
    if (!gameWorld) return;
    void* eventLog = reinterpret_cast<GameUpdateLoop*>(gameWorld)->m_eventNotifier;   // B.3.8 bridge
    EventLog_AddEvent(eventLog, eventType, severity, nullptr);
}

// ============================================================
// EventLog_CheckResourceShortages (port of FUN_14008c100)
// ============================================================
// Per-tick resource-shortage warning scanner. townMgr = *(gameWorld+0xF0).
// Two shortage tables hang off the town manager:
//   townMgr+0x170: 21 entries, stride 0x20 — the first 6 are tracked resources
//                  (logs/stone/iron/firewood/tools/food). Per entry:
//                  cur +0x00, hi +0x04, produced +0x08, cooldown(Time) +0x18.
//                  Firewood (index 3) folds in a secondary source at
//                  table+0x120/+0x124/+0x128.
//   townMgr+0x180: 2 entries, stride 0x10 — bulk/produced storage pools.
//                  cur +0x00, hi +0x04, cooldown(Time) +0x08.
// A low-reserve warning fires (sev 3, null entity) when current stock falls
// below produced/20 (and produced/20 <= hi); a near-capacity warning fires when
// a storage pool exceeds 95% ((hi/20)*19 < cur). After firing, the entry's
// cooldown resets to 120s (DAT_140172210) so the warning can't repeat for two
// game-minutes. Event types: 0x11-0x16 (low reserve), 0x17-0x18 (storage full).
//
// Self-gating: if produced==0 (resource untracked in this build) the ratio test
// `cur < 0` is false, so no spurious warning — safe to tick unconditionally.
extern "C" void EventLog_CheckResourceShortages(void* townMgr, void* timeDelta) {
    if (!townMgr || !timeDelta) return;
    void* gameWorld = g_pGameWorldSingleton ? *g_pGameWorldSingleton : nullptr;
    if (!gameWorld) return;
    void* eventLog = reinterpret_cast<GameUpdateLoop*>(gameWorld)->m_eventNotifier;   // B.3.8 bridge
    if (!eventLog) return;
    int64_t tick = *(int64_t*)timeDelta;

    uint8_t cooldownTime[8];
    fn_Time_Ctor_Float(cooldownTime, 120.0f);  // DAT_140172210
    int64_t cooldownReset = *(int64_t*)cooldownTime;

    // --- Table 1: low-reserve warnings (townMgr+0x170), 21 entries, 6 tracked ---
    uintptr_t table1 = *(uintptr_t*)((uintptr_t)townMgr + 0x170);
    if (table1) {
        for (int i = 0; i < 21; i++) {
            uintptr_t entry = table1 + (uintptr_t)i * 0x20;
            int64_t* cooldown = (int64_t*)(entry + 0x18);
            *cooldown -= tick;
            if (*cooldown < 1) {
                *cooldown = 0;
                if (i < 6) {
                    int cur      = *(int*)(entry + 0x00);
                    int hi       = *(int*)(entry + 0x04);
                    int produced = *(int*)(entry + 0x08);
                    if (i == 3) {  // firewood folds in a secondary source
                        cur      += *(int*)(table1 + 0x120);
                        hi       += *(int*)(table1 + 0x124);
                        produced += *(int*)(table1 + 0x128);
                    }
                    if (cur < produced / 20 && produced / 20 <= hi) {
                        EventLog_AddEvent(eventLog, i + 0x11, 3, nullptr);
                        *cooldown = cooldownReset;
                    }
                }
            }
        }
    }

    // --- Table 2: storage-near-capacity warnings (townMgr+0x180), 2 entries ---
    uintptr_t table2 = *(uintptr_t*)((uintptr_t)townMgr + 0x180);
    if (table2) {
        for (int i = 0; i < 2; i++) {
            uintptr_t entry = table2 + (uintptr_t)i * 0x10;
            int64_t* cooldown = (int64_t*)(entry + 0x08);
            *cooldown -= tick;
            if (*cooldown < 1) {
                *cooldown = 0;
                int cur = *(int*)(entry + 0x00);
                int hi  = *(int*)(entry + 0x04);
                if ((hi / 20) * 19 < cur) {
                    EventLog_AddEvent(eventLog, i + 0x17, 3, nullptr);
                    *cooldown = cooldownReset;
                }
            }
        }
    }
}

// ============================================================
// Shared-leaf exports (for the address registry / other event writers)
// ============================================================
// These three are retail leaves used by more than one event writer. The crop
// infestation writer (DiseaseComponent::Infect, 0x1400483a0) inlines a THIRD
// entry-append variant — 3 format args, fixed colour 0xFFFF6060, icon 2,
// popup -1 — so it needs the primitives rather than AddEvent/AddCitizenEvent.
// Exported here instead of re-ported there: porting one retail function twice
// lets the two copies diverge (see reference_one_retail_fn_ported_twice_diverges).

extern "C" void EventLog_ArrayGrow_Reg(void* arrBase, int newCount, const void* newEntry) {
    EventArray_Grow(arrBase, newCount, newEntry);
}

extern "C" void EventLog_YearString_Reg(void* gameWorld, void* retStr) {
    // Retail constructs the result String in the caller's return slot.
    new (retStr) System::String(EventLog_YearString(gameWorld));
}

extern "C" void EventLog_SeasonString_Reg(void* gameWorld, void* eventLog, void* retStr) {
    new (retStr) System::String(EventLog_SeasonString(gameWorld, eventLog));
}
