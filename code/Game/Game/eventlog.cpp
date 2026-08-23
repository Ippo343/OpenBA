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
    int* game = *(int**)((uintptr_t)gameWorld + 0xF8);
    if (!game) return -1;

    typedef void* (*fn_FindCompMember_t)(int64_t self, void* outIBP);
    static fn_FindCompMember_t s_findMap = nullptr;
    if (!s_findMap)
        s_findMap = reinterpret_cast<fn_FindCompMember_t>(ResolveAddr(Module::Exe, 0x140004680));
    if (!s_findMap || !fn_InternalBasePtr_CheckPtr) return -1;

    fn_InternalBasePtr_CheckPtr(entityIBP);
    int64_t mapIBP = 0;
    s_findMap(*(int64_t*)entityIBP, &mapIBP);
    if (mapIBP == 0) return -1;
    fn_InternalBasePtr_CheckPtr(&mapIBP);

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

    uint32_t color   = EventLog_SeverityColor(severity);
    int popup        = EventLog_PopupIndex(eventType);
    int iconIndex    = EventLog_IconIndex(eventType);
    int tileIdx      = EventLog_TileIndex(entityIBP);  // null entity -> -1

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

// ============================================================
// AddCitizenEvent (0x140094e00)
// ============================================================
// Citizen life-event variant (only exe caller: CitizenComponent_PostLifeEvent
// 0x140113af0, the former "AssignState" misnomer). Same shape as AddEvent but the
// format string gets three extra args — {0}=season {1}=year {2}=citizen name
// {3}=title (Child/Student/profession) {4}=extra param string — and a tail that
// drops the game to 1x speed on critical events (types 0xb/0xc/0x1e/0x1f) when
// the "pause on events" settings byte (+0x17) is set.
// nameStr/titleStr/extraStr are System::String* (data ptr at +0x00).
extern "C" void EventLog_AddCitizenEvent(void* eventLog, int eventType, int severity,
                                         void* entityIBP, void* nameStr, void* titleStr,
                                         void* extraStr) {
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

    // Args are the String DATA pointers, exactly as the original passes them.
    const char* nameData  = nameStr  ? *(const char**)nameStr  : "";
    const char* titleData = titleStr ? *(const char**)titleStr : "";
    const char* extraData = extraStr ? *(const char**)extraStr : "";
    if (fn_String_FormatIndexed) {
        fn_String_FormatIndexed(formatted, *(const char**)templateStr,
                                *(const char**)seasonStr, *(const char**)yearStr,
                                nameData, titleData, extraData);
    } else {
        fn_String_Ctor_Copy(formatted, templateStr);
    }

    uint32_t color = EventLog_SeverityColor(severity);
    int popup      = EventLog_PopupIndex(eventType);
    int iconIndex  = EventLog_IconIndex(eventType);
    int tileIdx    = EventLog_TileIndex(entityIBP);  // PostLifeEvent passes a null IBP -> -1

    // --- Build the 0x28-byte entry and append ---
    uint8_t entry[0x28];
    *(uint32_t*)(entry + 0x00) = color;
    fn_String_Ctor_Copy(entry + 0x08, formatted);
    *(int32_t*)(entry + 0x10) = eventType;
    *(int32_t*)(entry + 0x14) = severity;
    *(int32_t*)(entry + 0x18) = tileIdx;
    *(int32_t*)(entry + 0x1c) = iconIndex;
    *(int32_t*)(entry + 0x20) = popup;

    if (std::getenv("BANISHED_EVENT_TRACE")) {
        const char* fmtData = *(const char**)formatted;
        int existing = *(int*)((uintptr_t)eventLog + 0x20);
        fprintf(stderr, "[EventLog] AddCitizenEvent type=%d sev=%d icon=%d popup=%d "
                        "(count now %d)\n           text=\"%s\" name=\"%s\" title=\"%s\" "
                        "extra=\"%s\"\n",
                eventType, severity, iconIndex, popup, existing,
                fmtData ? fmtData : "(null)", nameData ? nameData : "(null)",
                titleData ? titleData : "(null)", extraData ? extraData : "(null)");
        fflush(stderr);
    }

    int count = *(int*)((uintptr_t)eventLog + 0x20);
    EventArray_Grow((void*)((uintptr_t)eventLog + 0x18), count + 1, entry);

    fn_String_Dtor(entry + 0x08);
    fn_String_Dtor(formatted);
    fn_String_Dtor(templateStr);
    fn_String_Dtor(seasonStr);
    fn_String_Dtor(yearStr);

    // --- Tail: critical events (0xb/0xc/0x1e/0x1f) drop the game to 1x speed when
    // the settings byte +0x17 is set. Speed lives in the EntityUI obj (gw+0x120)+0x34.
    if (eventType > 0xa && (eventType < 0xd || (unsigned)(eventType - 0x1e) < 2u)) {
        if (g_pGameSettingsPtr && *g_pGameSettingsPtr &&
            *((const char*)*g_pGameSettingsPtr + 0x17) != 0) {
            void* speedObj = *(void**)((uintptr_t)gameWorld + 0x120);
            if (speedObj) *(int32_t*)((uintptr_t)speedObj + 0x34) = 1;
        }
    }
}

// Convenience: add a location-less event to the live game's event log.
extern "C" void EventLog_AddSimple(int eventType, int severity) {
    void* gameWorld = g_pGameWorldSingleton ? *g_pGameWorldSingleton : nullptr;
    if (!gameWorld) return;
    void* eventLog = *(void**)((uintptr_t)gameWorld + 0x138);
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
    void* eventLog = *(void**)((uintptr_t)gameWorld + 0x138);
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
    EventLog_YearString(gameWorld, retStr);
}

extern "C" void EventLog_SeasonString_Reg(void* gameWorld, void* eventLog, void* retStr) {
    EventLog_SeasonString(gameWorld, eventLog, retStr);
}
