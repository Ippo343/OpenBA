// code/Game/Dialog/ui_dialog_stub_factories.cpp
//
// Dormant-safe standalone factories for the UI dialog family — the last family
// of the factory cascade (notes/FACTORY_CASCADE_145_BRIEF.md).
//
// These 58 dialog/controller/config types are all DORMANT in the current
// standalone path (menu + worldgen never instantiate them; verified 0
// NullFactory hits with BANISHED_GAME_PRELOAD). Each was a REGISTER_NULL_FACTORY
// stub in factory_registrations_bulk.cpp — a latent IsDerivedFrom(NULL) landmine
// that crashes only if the type is ever loaded/instantiated by name.
//
// Strategy (same dormant-safe policy as code/Game/Game/tornado.cpp and
// code/Game/Component/frustumoutlineelement.cpp): replace the NullFactory with a
// real factory that
//   - MemAlloc's the object at its TRUE exe size (decompiled from each
//     Factory_<Type> / <Type>_Factory in Application-x64.exe),
//   - zeroes it,
//   - installs a minimal per-type standalone vtable (slot0 ScalarDtor,
//     slot2 GetTypeId returning the registered type id, all other slots a nop).
// No base ctor (DialogInstance / DialogController / DialogControllerConfig /
// External) is run — every base field is left zero, which is correct because the
// object is never dispatched while dormant. The non-null return removes the
// landmine; GetTypeId reports the right id for any IsDerivedFrom query.
//
// NOTE: the exe subclass vtable (vtable_<Type>, full of exe->Runtime thunks that
// don't resolve in standalone) is intentionally NOT installed, so no
// RegisterExeAddr / address_registry edit is needed here. When the HUD bring-up
// (task #10) makes a specific dialog reachable, promote it out of this file into
// a full per-class standalone block wiring its reimplemented slot methods (see
// code/Game/Dialog/messagedialog.cpp for that pattern).
//
// The method bodies for most of these dialogs already exist in their own
// code/Game/Dialog/<type>.cpp (used by DLL-hook mode); this file only supplies
// the standalone construction/registration the exe ctors used to provide.

#ifdef STANDALONE_EXE

#include "Object/factory_registration.h"
#include "engine_funcs.h"
#include <cstdint>
#include <cstddef>

namespace {

// Generic no-op for unused vtable slots. Dormant dialogs are never dispatched,
// so any slot beyond Dtor (+0x00) / GetTypeId (+0x10) is unreached. Variadic so
// a stray call with any argument list lands here harmlessly.
void StubNop(void*, ...) {}

// The widest dialog vtable observed is well under this (DialogInstance's
// MessageDialog used 14 slots); 48 is safely generous. Out-of-range slots are
// never read for dormant types.
constexpr int kStubVtableSlots = 48;

// Fills a vtable array with StubNop, then overrides the two slots that matter.
struct StubVtableInit {
    StubVtableInit(void** vt, void* dtor, void* getTypeId) {
        for (int i = 0; i < kStubVtableSlots; ++i)
            vt[i] = reinterpret_cast<void*>(&StubNop);
        vt[0] = dtor;       // +0x00 ScalarDtor
        vt[2] = getTypeId;  // +0x10 GetTypeId
    }
};

}  // namespace

// One dormant-safe factory + per-type vtable per UI dialog.
#define UI_DIALOG_STUB(Name, Parent, Size)                                       \
    namespace uistub_##Name {                                                    \
        static void* g_vt[kStubVtableSlots];                                     \
        uint32_t GetTypeId(void*) {                                              \
            static uint32_t c = 0;                                               \
            if (!c) c = ::Object::GetTypeIdByName(#Name);                        \
            return c;                                                            \
        }                                                                        \
        void* Dtor(void* self, uint32_t flags) {                                 \
            if (!self) return self;                                              \
            *reinterpret_cast<void**>(self) = g_vt;                              \
            if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);         \
            return self;                                                         \
        }                                                                        \
        StubVtableInit g_init(g_vt, reinterpret_cast<void*>(&Dtor),              \
                              reinterpret_cast<void*>(&GetTypeId));              \
        void Zero(void* p) {                                                     \
            for (size_t o = 0; o < static_cast<size_t>(Size); o += 8)            \
                *reinterpret_cast<uint64_t*>(static_cast<char*>(p) + o) = 0;     \
        }                                                                        \
        void* Factory() {                                                        \
            if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;           \
            void* p = fn_MemAlloc(g_pMemAllocSingleton,                          \
                                  static_cast<uint64_t>(Size), 8);               \
            if (!p) return nullptr;                                              \
            Zero(p);                                                             \
            *reinterpret_cast<void**>(p) = g_vt;                                 \
            return p;                                                            \
        }                                                                        \
        void* InPlace(void* m) {                                                 \
            if (!m) return nullptr;                                              \
            Zero(m);                                                            \
            *reinterpret_cast<void**>(m) = g_vt;                                 \
            return m;                                                            \
        }                                                                        \
    }                                                                            \
    REGISTER_TYPE(Name, Parent, &uistub_##Name::Factory, &uistub_##Name::InPlace)

// ---- DialogController family ------------------------------------------------
UI_DIALOG_STUB(AttendanceUI,        "UI::DialogController", 0x38);
UI_DIALOG_STUB(BuildRequirementUI,  "UI::DialogController", 0x60);
UI_DIALOG_STUB(CemeteryUI,          "UI::DialogController", 0x38);
UI_DIALOG_STUB(ConsumeProduceUI,    "UI::DialogController", 0x40);
UI_DIALOG_STUB(ControlUI,           "UI::DialogController", 0x40);
UI_DIALOG_STUB(DebugUI,             "DialogController",     0x130);
UI_DIALOG_STUB(CitizenUI,           "UI::DialogController", 0xE8);
UI_DIALOG_STUB(DestroyUI,           "UI::DialogController", 0x78);
UI_DIALOG_STUB(EntityUI,            "UI::DialogController", 0x48);
UI_DIALOG_STUB(EventLogUI,          "UI::DialogController", 0x80);
UI_DIALOG_STUB(HelpUI,              "UI::DialogController", 0x78);
UI_DIALOG_STUB(HotKeyToolTipUI,     "UI::DialogController", 0x30);
UI_DIALOG_STUB(ImmigrationUI,       "UI::DialogController", 0x50);
UI_DIALOG_STUB(MapUI,               "UI::DialogController", 0x50);
UI_DIALOG_STUB(ModBrowseUI,         "UI::DialogController", 0x30);
UI_DIALOG_STUB(NoiseUI,             "UI::DialogController", 0x70);
UI_DIALOG_STUB(PageToggleUI,        "UI::DialogController", 0x48);
UI_DIALOG_STUB(ProductionUI,        "UI::DialogController", 0x50);
UI_DIALOG_STUB(ProfessionUI,        "UI::DialogController", 0x78);
UI_DIALOG_STUB(RadiusUI,            "UI::DialogController", 0x38);
UI_DIALOG_STUB(ResourceLimitUI,     "UI::DialogController", 0x40);
UI_DIALOG_STUB(SpriteCaptureUI,     "UI::DialogController", 0x70);
UI_DIALOG_STUB(StatusBarUI,         "UI::DialogController", 0x1D0);
UI_DIALOG_STUB(StatusIconUI,        "UI::DialogController", 0x40);
UI_DIALOG_STUB(StorageCapacityUI,   "UI::DialogController", 0x38);
UI_DIALOG_STUB(TitleDescToolTipUI,  "UI::DialogController", 0x30);
UI_DIALOG_STUB(ToolUI,              "UI::DialogController", 0x58);
UI_DIALOG_STUB(TownHallUI,          "UI::DialogController", 0x1E8);
UI_DIALOG_STUB(TradeUI,             "UI::DialogController", 0x148);
UI_DIALOG_STUB(UpgradeUI,           "UI::DialogController", 0x38);
UI_DIALOG_STUB(WorkPlaceUI,         "UI::DialogController", 0x60);
UI_DIALOG_STUB(BuildUI,             "UI::DialogController", 0x50);
UI_DIALOG_STUB(ResidenceUI,         "UI::DialogController", 0x40);
UI_DIALOG_STUB(StorageUI,           "UI::DialogController", 0x40);
UI_DIALOG_STUB(TutorialUI,          "UI::DialogController", 0x60);

// ---- SelectResourceUI subclasses (parent is the abstract SelectResourceUI) --
UI_DIALOG_STUB(OrchardUI,           "SelectResourceUI",     0x48);
UI_DIALOG_STUB(CropUI,              "SelectResourceUI",     0x60);
UI_DIALOG_STUB(PastureUI,           "SelectResourceUI",     0x40);

// ---- DialogInstance family --------------------------------------------------
UI_DIALOG_STUB(AchievementDialog,   "UI::DialogInstance",   0xD0);
UI_DIALOG_STUB(AutoSaveDialog,      "UI::DialogInstance",   0xD0);
UI_DIALOG_STUB(EventNotifyDialog,   "UI::DialogInstance",   0xF0);
UI_DIALOG_STUB(EventPopupDialog,    "UI::DialogInstance",   0xC0);
UI_DIALOG_STUB(FollowDialog,        "UI::DialogInstance",   0xD8);
UI_DIALOG_STUB(GoalDialog,          "UI::DialogInstance",   0xC8);
UI_DIALOG_STUB(MapParamsDialog,     "UI::DialogInstance",   0xF8);
UI_DIALOG_STUB(ModDetailsDialog,    "UI::DialogInstance",   0xC8);
UI_DIALOG_STUB(ModDialog,           "UI::DialogInstance",   0xD8);
UI_DIALOG_STUB(ModUploadDialog,     "UI::DialogInstance",   0xB8);
UI_DIALOG_STUB(ScenarioDialog,      "UI::DialogInstance",   0xE8);
UI_DIALOG_STUB(SaveLoadDialog,      "UI::DialogInstance",   0x160);
UI_DIALOG_STUB(ToolbarDialog,       "UI::DialogInstance",   0x108);
UI_DIALOG_STUB(TutorialHighlightDialog, "UI::DialogInstance", 0xF0);

// ---- DialogControllerConfig family (all 0x28) -------------------------------
UI_DIALOG_STUB(ControlUIConfig,       "UI::DialogControllerConfig", 0x28);
UI_DIALOG_STUB(ResourceLimitUIConfig, "UI::DialogControllerConfig", 0x28);
UI_DIALOG_STUB(StatusIconUIConfig,    "UI::DialogControllerConfig", 0x28);
UI_DIALOG_STUB(UpgradeUIConfig,       "UI::DialogControllerConfig", 0x28);

// ---- External-based tutorial dialogs (parents are abstract External bases) --
UI_DIALOG_STUB(TutorialGoalDialog,  "TutorialGoal",         0x40);
UI_DIALOG_STUB(TutorialSetupDialog, "TutorialSetup",        0x78);

#undef UI_DIALOG_STUB

#endif  // STANDALONE_EXE
