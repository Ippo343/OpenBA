// code/Game/Component/citizen_lifeevent.cpp
// CitizenComponent_PostLifeEvent (0x140113af0) — clean reimplementation.
//
// Formerly misnamed "CitizenComponent_AssignState" in Ghidra: it assigns nothing.
// It posts a citizen life-event message to the event log (GameUpdateLoop+0x138)
// via EventLog_AddCitizenEvent (0x140094e00, code/Game/Game/eventlog.cpp).
// eventType (param 2) is the message-table index — 8/9/10 = became-adult
// variants from Update, plus death/birth types from HandleLifeEvent, OnRemove,
// helper_1b90 (repro) and helper_17a0.
//
// Title-string selection (feeds format slot {3}):
//   child   (age < adultAge, not in school)  -> ProfessionList(+0xB0)->+0x40 entry
//   student (in school, any age)             -> ProfessionList(+0xB0)->+0x48 entry
//   adult   (not in school)                  -> own profession at this+0x1F0
// Each source is a resource object whose UI::String name pair sits at
// {+0x20 string table, +0x30 name String}; converted UI::String -> System::String.
//
// Retail callers in Application-x64.exe (all now resolve 0x140113af0 through the
// address registry): CitizenComponent_Update @140112f9e, HandleLifeEvent
// @140116fdb, helper_17a0 @14011181e, helper_1b90 @140111e73, OnRemove @140113f94.

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

// code/Game/Game/eventlog.cpp
extern "C" void EventLog_AddCitizenEvent(void* eventLog, int eventType, int severity,
                                         void* entityIBP, void* nameStr, void* titleStr,
                                         void* extraStr);

extern "C" void CitizenComponent_PostLifeEvent(void* self, int eventType, int severity,
                                               const char* strParam) {
    uint8_t* p = reinterpret_cast<uint8_t*>(self);
    void* gw = g_pGameWorldSingleton ? *g_pGameWorldSingleton : nullptr;
    if (!gw) return;
    // [STANDALONE GUARD] partial worlds can lack the event log; retail derefs unguarded.
    void* eventLog = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(gw) + 0x138);
    if (!eventLog) return;

    // --- Pick the title source object ---
    int64_t desc = *reinterpret_cast<int64_t*>(p + 0x288);
    float age = *reinterpret_cast<float*>(p + 0x3C);
    float adultAge = desc ? *reinterpret_cast<float*>(desc + 0x44) : 0.0f;
    bool inSchool = *reinterpret_cast<int64_t*>(p + 0x208) != 0;
    uintptr_t profList = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(gw) + 0xB0);

    uintptr_t titleSrc = 0;
    if (age < adultAge && !inSchool) {
        titleSrc = profList ? *reinterpret_cast<uintptr_t*>(profList + 0x40) : 0;  // child
    } else if (!inSchool) {
        // [STANDALONE GUARD] +0x1F0 profession can be unstamped (same gap the
        // unemployed-flag path in Update guards); retail always has it set.
        titleSrc = *reinterpret_cast<uintptr_t*>(p + 0x1F0);                       // profession
    } else {
        titleSrc = profList ? *reinterpret_cast<uintptr_t*>(profList + 0x48) : 0;  // student
    }

    // --- Build the title System::String from the source's UI::String name pair ---
    uint8_t titleStr[8] = {};
    if (titleSrc && fn_UIString_Ctor && fn_UIString_ToSystemString) {
        uint8_t uiStr[16];
        fn_UIString_Ctor(uiStr, *reinterpret_cast<uint64_t*>(titleSrc + 0x20),
                         reinterpret_cast<void*>(titleSrc + 0x30));
        fn_UIString_ToSystemString(uiStr, titleStr);
    } else {
        fn_String_Ctor_CStr(titleStr, "");
    }

    // --- Extra param string ({4}); retail passes null for most events ---
    uint8_t paramStr[8] = {};
    fn_String_Ctor_CStr(paramStr, strParam ? strParam : "");

    // --- Post (null entity IBP: citizen life events carry no map location) ---
    int64_t nullIBP = 0;
    fn_InternalBasePtr_Ctor(&nullIBP, nullptr);
    EventLog_AddCitizenEvent(eventLog, eventType, severity, &nullIBP,
                             p + 0x28 /* citizen name String */, titleStr, paramStr);

    fn_String_Dtor(paramStr);
    fn_String_Dtor(titleStr);
}
