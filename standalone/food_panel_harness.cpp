// standalone/food_panel_harness.cpp
// BANISHED_FOOD_PANEL=<class> — dialog harness for the six food-production panels.
//
//   <class> = cropfield | orchard | pasture | fishing | hunting | gatherer | all
//
// The placement harnesses answer "does the building work"; this answers "does its
// PANEL work", which is a separate failure surface with its own well-documented traps
// in this codebase: a controller can be wired and still inert because a gated element
// type-id left ControlUI+0x30 null, and a component can be live while its command slots
// were never filled because FillTypedVtable erased a static-init write.
//
// For each class it reports, as OUTCOMES rather than intent:
//   * whether an entity of that class exists at all,
//   * whether selecting it actually opened a dialog,
//   * every UI controller on that dialog (count + type id + name where resolvable),
//   * whether the component's command slots 13/14 are filled, and
//   * the CanCommand verdict for command ids 0..7, which is what greys or enables each
//     panel button. "All zero" is the signature of a panel whose buttons are all dead.
//
// A class with no entity prints "NO ENTITY" rather than nothing, so a silent section can
// never be mistaken for a passing one.
//
// Runs once, at a settle frame (default 900, override with BANISHED_FOOD_PANEL_FRAME) so
// the world and the demo buildings exist. Test-only, opt-in, default off.

#include "resolve.h"
#include "engine_funcs.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// Both provided by the existing tool harness / dialog layer.
extern "C" int EditTool_HarnessSelectEntity(void* editTool, void* entity);
void EntityUI_SetEditMode_Impl(void* playerController, uint64_t mode);

namespace {

struct FoodClass {
    const char* key;        // BANISHED_FOOD_PANEL value
    const char* component;  // component type name
    const char* uiClass;    // the panel controller we hope to find (may be absent)
};

const FoodClass kClasses[] = {
    { "cropfield", "CropFieldComponent", "CropUI"     },
    { "orchard",   "OrchardComponent",   "OrchardUI"  },
    { "pasture",   "PastureComponent",   "PastureUI"  },
    { "fishing",   "FishingComponent",   nullptr      },
    { "hunting",   "HuntComponent",      nullptr      },
    { "gatherer",  "HerbalistComponent", nullptr      },
};
const int kClassCount = (int)(sizeof(kClasses) / sizeof(kClasses[0]));

bool  s_init = false;
char  s_want[32] = {0};
int   s_fireFrame = 900;
bool  s_fired = false;

void Init() {
    if (s_init) return;
    s_init = true;
    const char* e = std::getenv("BANISHED_FOOD_PANEL");
    if (!e || !e[0] || std::strcmp(e, "0") == 0) return;
    std::snprintf(s_want, sizeof(s_want), "%s", e);
    if (const char* f = std::getenv("BANISHED_FOOD_PANEL_FRAME")) {
        int v = std::atoi(f);
        if (v > 0) s_fireFrame = v;
    }
    std::fprintf(stderr, "[FoodPanel] ENABLED target=\"%s\" fireFrame=%d\n",
                 s_want, s_fireFrame);
    std::fflush(stderr);
}

uint32_t Tid(const char* n) { return n ? ::Object::GetTypeIdByName(n) : 0; }

void* FindCompOnEntity(void* ent, uint32_t tid) {
    if (!ent || !tid) return nullptr;
    const int n = *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(ent) + 0x38);
    uintptr_t a = *reinterpret_cast<uintptr_t*>(static_cast<uint8_t*>(ent) + 0x30);
    for (int i = 0; i < n && a; ++i) {
        void** cs = reinterpret_cast<void**>(a + static_cast<uintptr_t>(i) * 8);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(cs);
        void* c = *cs;
        if (!c) continue;
        void** vt = *reinterpret_cast<void***>(c);
        if (!vt || !vt[2]) continue;
        if (reinterpret_cast<uint32_t(*)(void*)>(vt[2])(c) == tid) return c;
    }
    return nullptr;
}

// Probe one class end to end.
void ProbeClass(void* entityUI, const FoodClass& fc) {
    const uint32_t tid = Tid(fc.component);
    if (!tid) {
        std::fprintf(stderr, "[FoodPanel] %-19s TYPE ID UNRESOLVED — cannot probe\n",
                     fc.component);
        return;
    }

    void* gw = (g_pGameWorldSingleton && *g_pGameWorldSingleton) ? *g_pGameWorldSingleton : nullptr;
    if (!gw) { std::fprintf(stderr, "[FoodPanel] gameWorld null\n"); return; }
    uintptr_t el = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(gw) + 0xe8);
    if (!el) { std::fprintf(stderr, "[FoodPanel] EntityList null\n"); return; }
    uintptr_t arr = *reinterpret_cast<uintptr_t*>(el + 0x08);
    const int n = *reinterpret_cast<int32_t*>(el + 0x10);

    void* ent = nullptr; void* comp = nullptr;
    for (int i = 0; i < n && arr && !ent; ++i) {
        void** slot = reinterpret_cast<void**>(arr + static_cast<uintptr_t>(i) * 8);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(slot);
        void* e = *slot;
        if (!e) continue;
        if (void* c = FindCompOnEntity(e, tid)) { ent = e; comp = c; }
    }
    if (!ent) {
        std::fprintf(stderr, "[FoodPanel] %-19s NO ENTITY in world (scanned %d) — set the "
                     "matching BANISHED_BUILD_DEMO_* flag\n", fc.component, n);
        return;
    }

    // ── the command slots, before opening anything ────────────────────────────────
    // Slots 13/14 are CanCommand/Command. A live component with nulls here has a panel
    // whose buttons cannot even be evaluated, let alone clicked.
    void** cvt = *reinterpret_cast<void***>(comp);
    void* canCmd = cvt ? cvt[13] : nullptr;
    void* doCmd  = cvt ? cvt[14] : nullptr;
    std::fprintf(stderr, "[FoodPanel] %-19s ent=%p comp=%p CanCommand(13)=%p Command(14)=%p\n",
                 fc.component, ent, comp, canCmd, doCmd);

    if (canCmd) {
        // Slot 13 does NOT have one signature across the family. Orchard/Pasture/CropField
        // implement CanCommand(self, id) and answer per command; the ResourceProduce base
        // (0x14012b520, shared by Fishing/Hunt/Herbalist) takes only `self` and returns a
        // BITMASK — bit 1 = pause offered, bit 0 = resume offered. Sweeping ids against a
        // mask yields the same number eight times, so detect that and decode instead of
        // reporting eight identical "verdicts" as if they were per-button answers.
        uint8_t r[8];
        int enabled = 0;
        bool uniform = true;
        for (int id = 0; id < 8; ++id) {
            r[id] = reinterpret_cast<uint8_t (*)(void*, int)>(canCmd)(comp, id);
            if (r[id]) ++enabled;
            if (r[id] != r[0]) uniform = false;
        }
        if (uniform) {
            std::fprintf(stderr, "[FoodPanel]   command MASK = 0x%02X  (pause=%s resume=%s)"
                         "  [identical for every id -> the ResourceProduce mask form]%s\n",
                         (unsigned)r[0],
                         (r[0] & 2) ? "yes" : "no", (r[0] & 1) ? "yes" : "no",
                         r[0] == 0 ? "  <-- no command offered" : "");
        } else {
            std::fprintf(stderr, "[FoodPanel]   CanCommand(id) =");
            for (int id = 0; id < 8; ++id)
                std::fprintf(stderr, " %d:%u", id, (unsigned)r[id]);
            std::fprintf(stderr, "   (%d of 8 enabled%s)\n", enabled,
                         enabled == 0 ? "  <-- every button dead" : "");
        }
    }

    // ── open the panel ────────────────────────────────────────────────────────────
    void* tool = *reinterpret_cast<void**>(static_cast<uint8_t*>(entityUI) + 0x28);
    if (!tool) {
        EntityUI_SetEditMode_Impl(entityUI, 0);
        tool = *reinterpret_cast<void**>(static_cast<uint8_t*>(entityUI) + 0x28);
    }
    if (!tool) {
        std::fprintf(stderr, "[FoodPanel]   EditTool activation FAILED — cannot open a panel\n");
        return;
    }
    const int opened = EditTool_HarnessSelectEntity(tool, ent);
    void* dialog = *reinterpret_cast<void**>(static_cast<uint8_t*>(tool) + 0x28);
    std::fprintf(stderr, "[FoodPanel]   select -> opened=%d dialog=%p\n", opened, dialog);
    if (!dialog) {
        std::fprintf(stderr, "[FoodPanel]   NO DIALOG: this building has no info-panel UI "
                     "binding, or SelectEntity refused it\n");
        return;
    }

    // ── enumerate the dialog's controllers ────────────────────────────────────────
    void** ctrlArr = *reinterpret_cast<void***>(reinterpret_cast<uintptr_t>(dialog) + 0x78);
    const int cnt = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(dialog) + 0x80);
    const uint32_t wantUi = Tid(fc.uiClass);
    bool sawWanted = false;
    std::fprintf(stderr, "[FoodPanel]   %d controller(s):", cnt);
    for (int i = 0; ctrlArr && i < cnt; ++i) {
        void* c = ctrlArr[i];
        if (!c) { std::fprintf(stderr, " [null]"); continue; }
        void** vt = *reinterpret_cast<void***>(c);
        const uint32_t t = (vt && vt[2])
            ? reinterpret_cast<uint32_t(*)(void*)>(vt[2])(c) : 0;
        std::fprintf(stderr, " %08X", t);
        if (wantUi && t == wantUi) sawWanted = true;
    }
    std::fprintf(stderr, "\n");
    if (fc.uiClass) {
        std::fprintf(stderr, "[FoodPanel]   %s (tid=%08X): %s\n", fc.uiClass, wantUi,
                     !wantUi ? "TYPE ID UNRESOLVED"
                             : (sawWanted ? "PRESENT on the dialog"
                                          : "ABSENT — the panel opened without its controller"));
    } else {
        std::fprintf(stderr, "[FoodPanel]   (no dedicated panel controller class identified "
                     "for this building yet — the type ids above are the evidence)\n");
    }
    std::fflush(stderr);
}

}  // namespace

extern "C" void FoodPanel_Tick(void* entityUI, int frameNum) {
    Init();
    if (!s_want[0] || s_fired) return;
    if (frameNum < s_fireFrame) return;
    s_fired = true;
    if (!entityUI) {
        std::fprintf(stderr, "[FoodPanel] entityUI null at frame %d — cannot probe\n", frameNum);
        std::fflush(stderr);
        return;
    }
    const bool all = (std::strcmp(s_want, "all") == 0);
    int probed = 0;
    for (int i = 0; i < kClassCount; ++i) {
        if (all || std::strcmp(s_want, kClasses[i].key) == 0) {
            ProbeClass(entityUI, kClasses[i]);
            ++probed;
        }
    }
    if (probed == 0)
        std::fprintf(stderr, "[FoodPanel] unknown target \"%s\" — expected one of "
                     "cropfield|orchard|pasture|fishing|hunting|gatherer|all\n", s_want);
    std::fflush(stderr);
}
