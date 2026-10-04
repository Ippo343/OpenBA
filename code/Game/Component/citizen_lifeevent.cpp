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

#include "ui/UserInterface.h"
#include "Game/exe_bodies.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "system/String.h"
#include <cstdint>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "object/External.h"   // ENDGAME B.3.6 UI callers (K4)
#include "ui/String.h"         // ENDGAME B.3.6 UI callers (K4)
#include "ui/StringTable.h"    // ENDGAME B.3.6 UI callers (K4)
#include <new>
#include "Component/exe_bodies.h"
#include "Component/CitizenComponent.h"     // B.4 W1 COMP-b: PostLifeEvent is a member
#include "Component/CitizenDescription.h"
#include "Game/Profession.h"
#include "Game/ProfessionList.h"

// code/Game/Game/eventlog.cpp
extern "C" void EventLog_AddCitizenEvent(void* eventLog, int eventType, int severity,
                                         void* entityIBP, void* nameStr, void* titleStr,
                                         void* extraStr);

void CitizenComponent::PostLifeEvent(int eventType, int severity, const char* strParam) {
    GameUpdateLoop* gw = GameUpdateLoop::_singleton;
    if (!gw) return;
    // [STANDALONE GUARD] partial worlds can lack the event log; retail derefs unguarded.
    void* eventLog = gw->m_eventNotifier;
    if (!eventLog) return;

    // --- Pick the title source profession ---
    CitizenDescription* desc = description;
    float age = this->age;
    float adultAge = desc ? desc->adultAge : 0.0f;
    bool inSchool = school != nullptr;
    ProfessionList* profList = gw->m_professionList;

    Profession* titleSrc = nullptr;
    if (age < adultAge && !inSchool) {
        titleSrc = profList ? profList->childProfession : nullptr;     // child
    } else if (!inSchool) {
        // [STANDALONE GUARD] +0x1F0 profession can be unstamped (same gap the
        // unemployed-flag path in Update guards); retail always has it set.
        titleSrc = profession;                                          // profession
    } else {
        titleSrc = profList ? profList->studentProfession : nullptr;   // student
    }

    // --- Build the title System::String from the source's UI::String name pair ---
    System::String titleStr;
    if (titleSrc) {
        uint8_t uiStr[16];
        ::new (uiStr) UI::String(
            Object::ExternalPtr<UI::StringTable>(titleSrc->stringTable),
            titleSrc->stringName);   // +0x30
        ::new (&titleStr) System::String(
            reinterpret_cast<const UI::String*>(uiStr)->operator System::String());  // B.3.6 bridge  LEFT: H5 raw UI::String local (a typed local would run ~UI::String)
    } else {
        titleStr.Set("");   // == String("") (ctor is default + Set)
    }

    // --- Extra param string ({4}); retail passes null for most events ---
    System::String paramStr(strParam ? strParam : "");

    // --- Post (null entity IBP: citizen life events carry no map location) ---
    ComponentEntity* nullIBP = nullptr;
    EventLog_AddCitizenEvent(eventLog, eventType, severity, &nullIBP,
                             &name /* citizen name String (H5 raw) */, &titleStr, &paramStr);
}
