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
//   EventArray_Grow   0x140019b70  — vector<Event>::resize/append primitive
//   YearString        0x1400951d0  — String::Format("%i", year)
//   SeasonString      0x140095230  — seasons table lookup by month index
//   IconIndex         0x1400950d0  — eventType -> icon index switch
//   (TileIndex        0x140095030  — entity -> map tile; null entity => -1)

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

// ============================================================
// EventArray_Grow (0x140019b70) — grow-and-append primitive
// ============================================================
// arr points to {data* @+0, count @+8, capacity @+0xc}. 0x28-byte stride, a
// System::String at each entry +0x08. Faithful port: triple-capacity heuristic on
// a single-element grow, copy preserved entries (String copy-ctor), append the new
// entry, then destruct/free the old buffer's Strings.
static void EventArray_Grow(void* arrBase, int newCount, const void* newEntry) {
    uintptr_t* base = (uintptr_t*)arrBase;
    int oldCount = *(int*)((uintptr_t)base + 8);
    if (newCount == oldCount) return;

    uintptr_t oldData = *base;
    *(int*)((uintptr_t)base + 8) = newCount;
    int* capPtr = (int*)((uintptr_t)base + 0xC);
    if (*capPtr < newCount) {
        *capPtr = newCount;
        if (newCount == oldCount + 1 && oldCount != 0) *capPtr = newCount * 3;
        *base = (uintptr_t)fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)*capPtr * 0x28, 8);
    }
    uintptr_t newData = *base;

    int startIdx = (oldData == newData) ? oldCount : 0;
    for (int i = startIdx; i < newCount; i++) {
        uintptr_t dst = newData + (uintptr_t)i * 0x28;
        const void* src = (i < oldCount) ? (const void*)(oldData + (uintptr_t)i * 0x28) : newEntry;
        *(uint32_t*)dst = *(const uint32_t*)src;                        // +0x00 color
        fn_String_Ctor_Copy((void*)(dst + 0x08), (void*)((uintptr_t)src + 0x08));  // +0x08 String
        *(uint32_t*)(dst + 0x10) = *(const uint32_t*)((uintptr_t)src + 0x10);
        *(uint32_t*)(dst + 0x14) = *(const uint32_t*)((uintptr_t)src + 0x14);
        *(uint32_t*)(dst + 0x18) = *(const uint32_t*)((uintptr_t)src + 0x18);
        *(uint32_t*)(dst + 0x1c) = *(const uint32_t*)((uintptr_t)src + 0x1c);
        *(uint32_t*)(dst + 0x20) = *(const uint32_t*)((uintptr_t)src + 0x20);
    }

    if (oldData == newData) {
        // In-place shrink: dtor excess Strings (not hit on append, kept for fidelity)
        for (int i = newCount; i < oldCount; i++)
            fn_String_Dtor((void*)(oldData + (uintptr_t)i * 0x28 + 0x08));
    } else if (oldData) {
        for (int i = 0; i < oldCount; i++)
            fn_String_Dtor((void*)(oldData + (uintptr_t)i * 0x28 + 0x08));
        fn_MemFreeStatic((void*)oldData);
    }
}

// ============================================================
// Helpers
// ============================================================

// YearString (0x1400951d0): String::Format("%i", *(int*)clock), clock = *(gw+0x128).
static void EventLog_YearString(void* gameWorld, void* retStr) {
    void* clock = *(void**)((uintptr_t)gameWorld + 0x128);
    int year = clock ? *(int*)clock : 0;
    fn_String_Format(retStr, "%i", year);
}

// SeasonString (0x140095230): monthIdx = (int)(AsSeconds(clock+8) / 3600 * 12);
// if monthIdx >= 12 -> 0. seasons table = *(eventlog+0x08). GetText(table, ret, idx).
static void EventLog_SeasonString(void* gameWorld, void* eventLog, void* retStr) {
    void* clock = *(void**)((uintptr_t)gameWorld + 0x128);
    int idx = 0;
    if (clock && fn_Time_AsSeconds) {
        float secs = fn_Time_AsSeconds((char*)clock + 8);
        idx = (int)(secs / 3600.0f * 12.0f);
        if (idx >= 12) idx = 0;
    }
    void* seasonsTable = *(void**)((uintptr_t)eventLog + 0x08);
    if (seasonsTable && fn_StringTable_GetText) {
        fn_StringTable_GetText(seasonsTable, retStr, idx);
    } else {
        fn_String_Ctor_CStr(retStr, "");
    }
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
// AddEvent (0x140095580)
// ============================================================
// eventLog = *(gameWorld+0x138). Builds "FormatIndexed(events[eventType], season,
// year)" text, colours by severity, computes icon + popup flag, and appends the
// 0x28-byte entry. entityIBP is an optional InternalBasePtr* for the map-jump tile
// (null => tileIdx -1, no location). Faithful to the 4-arg exe overload.
extern "C" void EventLog_AddEvent(void* eventLog, int eventType, int severity, void* entityIBP) {
    if (!eventLog) return;
    void* gameWorld = g_pGameWorldSingleton ? *g_pGameWorldSingleton : nullptr;
    if (!gameWorld) return;

    // --- Build the display text ---
    uint8_t yearStr[8], seasonStr[8], templateStr[8], formatted[8];
    EventLog_YearString(gameWorld, yearStr);
    EventLog_SeasonString(gameWorld, eventLog, seasonStr);

    void* eventsTable = *(void**)eventLog;  // +0x00
    if (eventsTable && fn_StringTable_GetText) {
        fn_StringTable_GetText(eventsTable, templateStr, eventType);
    } else {
        fn_String_Ctor_CStr(templateStr, "Event");
    }

    // FormatIndexed(fmt=template, %0=season, %1=year). Args are the String DATA
    // pointers (char*). Fall back to the raw template if the export is unresolved.
    if (fn_String_FormatIndexed) {
        fn_String_FormatIndexed(formatted, *(const char**)templateStr,
                                *(const char**)seasonStr, *(const char**)yearStr);
    } else {
        fn_String_Ctor_Copy(formatted, templateStr);
    }

    // --- Colour by severity ---
    uint32_t color;
    switch (severity) {
        case 1:  color = 0xFFFFFF00u; break;  // yellow
        case 2:  color = 0xFF8080FFu; break;  // pink
        case 3:  color = 0xFFFF6060u; break;  // red
        case 4:  color = 0xFFEA7517u; break;  // orange
        default: color = 0xFFFFFFFFu; break;  // white
    }

    // --- Popup flag by eventType (0x1a/0x1c -> 0xc, 0x1b/0x1d -> 0xd, else -1) ---
    int popup;
    if (eventType == 0x1a || eventType == 0x1c)      popup = 0xc;
    else if (eventType == 0x1b || eventType == 0x1d) popup = 0xd;
    else                                             popup = -1;

    int iconIndex = EventLog_IconIndex(eventType);

    // tileIdx: null entity -> -1 (no map location). The entity->tile port
    // (FUN_140095030 via FUN_140004680) is not needed for location-less events.
    int tileIdx = -1;
    (void)entityIBP;

    // --- Build the 0x28-byte entry and append ---
    uint8_t entry[0x28];
    *(uint32_t*)(entry + 0x00) = color;
    fn_String_Ctor_Copy(entry + 0x08, formatted);
    *(int32_t*)(entry + 0x10) = eventType;
    *(int32_t*)(entry + 0x14) = severity;
    *(int32_t*)(entry + 0x18) = tileIdx;
    *(int32_t*)(entry + 0x1c) = iconIndex;
    *(int32_t*)(entry + 0x20) = popup;

    // Trace BEFORE destructing the temporaries (reading them after dtor shows null).
    if (std::getenv("BANISHED_EVENT_TRACE")) {
        const char* fmtData  = *(const char**)formatted;
        const char* tmplData = *(const char**)templateStr;
        const char* seaData  = *(const char**)seasonStr;
        const char* yrData   = *(const char**)yearStr;
        int existing = *(int*)((uintptr_t)eventLog + 0x20);
        fprintf(stderr, "[EventLog] AddEvent type=%d sev=%d icon=%d popup=%d (count now %d)\n"
                        "           text=\"%s\" template=\"%s\" season=\"%s\" year=\"%s\"\n",
                eventType, severity, iconIndex, popup, existing,
                fmtData ? fmtData : "(null)", tmplData ? tmplData : "(null)",
                seaData ? seaData : "(null)", yrData ? yrData : "(null)");
        fflush(stderr);
    }

    int count = *(int*)((uintptr_t)eventLog + 0x20);
    EventArray_Grow((void*)((uintptr_t)eventLog + 0x18), count + 1, entry);

    // Cleanup temporaries (the grow copied the entry's String).
    fn_String_Dtor(entry + 0x08);
    fn_String_Dtor(formatted);
    fn_String_Dtor(templateStr);
    fn_String_Dtor(seasonStr);
    fn_String_Dtor(yearStr);
}

// Convenience: add a location-less event to the live game's event log.
extern "C" void EventLog_AddSimple(int eventType, int severity) {
    void* gameWorld = g_pGameWorldSingleton ? *g_pGameWorldSingleton : nullptr;
    if (!gameWorld) return;
    void* eventLog = *(void**)((uintptr_t)gameWorld + 0x138);
    EventLog_AddEvent(eventLog, eventType, severity, nullptr);
}
