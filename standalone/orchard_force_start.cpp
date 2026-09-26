// standalone/orchard_force_start.cpp
// One-shot harness that forces a demo orchard's master switch on and gives it a
// real tree type, so the state-2 -> state-9 transition (see
// code/Game/Component/orchardcomponent*.cpp — NOT touched by this file) has
// something to prove. Env-gated, default OFF, byte-unchanged when unset.
//
// Background (from the live-porting pass): OrchardComponent's state-2 per-frame
// handler only requests state 9 when comp+0x48 (the "master switch" byte) is
// non-zero. The ctor zeroes it. The only two retail paths that set it both go
// through CanCommand, which ANDs on hasType = (*(comp+0x60) != 0) — so an
// untyped orchard (which is exactly what BANISHED_BUILD_DEMO_ORCHARD spawns) can
// never start, by design. This harness force-sets both fields so the transition
// can be observed without waiting on a full panel-click UI flow.
//
// Env:
//   BANISHED_ORCHARD_FORCE_START=1   enable; fires once ~180 ticks after start
//
// Do NOT touch code/Game/Component/orchardcomponent*.cpp — actively owned by
// the porting pass. This file only pokes already-published fields/paths.

#include "resolve.h"
#include "engine_funcs.h"
#include "Object/factory_registration.h"   // ::Object::GetTypeIdByName
#include "options.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "ui/ComboElement.h"   // ENDGAME B.3.6 UI callers (K4)

// EntityUI::SetEditMode (code/Game/Dialog/entityui.cpp) — C++ linkage, matches the
// declaration tool_harness.cpp already uses for the same function.
void EntityUI_SetEditMode_Impl(void* playerController, uint64_t mode);

// EditTool select-by-entity harness hook (standalone/tool_harness.cpp) — opens the
// entity's info dialog exactly like a real click, returns 1 if a dialog opened.
extern "C" int EditTool_HarnessSelectEntity(void* editTool, void* entity);
extern "C" void* EditTool_HarnessAcquire(void* entityUI);
struct EditTool;
void EditTool_CloseInfoDialog(EditTool* self, uintptr_t dialogPtr);   // code/Game/Player/edittool.cpp

namespace {

bool s_initialized = false;
bool s_enabled     = false;
bool s_fired       = false;
constexpr int kSettleFrame = 180;   // a few frames after StartTown's demo spawn

void Init() {
    if (s_initialized) return;
    s_initialized = true;

    const char* env = Opt().orchardForceStart;
    if (!env || env[0] == '\0' ||
        std::strcmp(env, "0") == 0 || std::strcmp(env, "false") == 0) {
        s_enabled = false;
        fprintf(stderr, "[OrchardForceStart] disabled (BANISHED_ORCHARD_FORCE_START unset)\n");
        return;
    }
    s_enabled = true;
    fprintf(stderr, "[OrchardForceStart] ENABLED (BANISHED_ORCHARD_FORCE_START=\"%s\") — will fire "
                    "once at frame %d\n", env, kSettleFrame);
    std::fflush(stderr);
}

// Attempts to give the orchard a real tree type by driving the SAME combo-change
// event path BANISHED_ORCHARDPANEL_SELECT uses (tool_harness.cpp's FireOrchardPanel):
// open the entity's info dialog via EditTool, find its OrchardUI controller, pick
// combo item 0, and fire the real Slot9 (SelectResourceUI change handler) — which
// is what actually writes comp+0x60 in retail. Deliberately does NOT synthesize a
// pointer: if any step in this real path fails, comp+0x60 is left alone and the
// failure is logged, not papered over.
bool TryAssignRealTreeType(void* entityUI, void* orchEnt, void* orchComp) {
    if (!entityUI) {
        fprintf(stderr, "[OrchardForceStart]     cannot assign tree type: entityUI null\n");
        return false;
    }

    void* tool = EditTool_HarnessAcquire(entityUI);   // rejects a non-EditTool
    if (!tool) {
        fprintf(stderr, "[OrchardForceStart]     cannot assign tree type: EditTool activation "
                        "FAILED (no tool at entityUI+0x28) — is BANISHED_LIVE_TOOLS=1?\n");
        return false;
    }

    int opened = EditTool_HarnessSelectEntity(tool, orchEnt);
    void* dialog = *reinterpret_cast<void**>(static_cast<uint8_t*>(tool) + 0x28);
    fprintf(stderr, "[OrchardForceStart]     select orchard ent=%p -> opened=%d dialog=%p\n",
            orchEnt, opened, dialog);
    if (!dialog) {
        fprintf(stderr, "[OrchardForceStart]     cannot assign tree type: no info dialog opened "
                        "for this orchard\n");
        return false;
    }

    const uint32_t tidOUI = ::Object::GetTypeIdByName("OrchardUI");
    void** ctrlArr = *reinterpret_cast<void***>(reinterpret_cast<uintptr_t>(dialog) + 0x78);
    int cnt = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(dialog) + 0x80);
    void* oui = nullptr;
    for (int i = 0; ctrlArr && i < cnt; i++) {
        void* c = ctrlArr[i];
        if (!c) continue;
        void** vt = *reinterpret_cast<void***>(c);
        typedef uint32_t (*GetTypeId_t)(void*);
        if (vt && vt[2] && reinterpret_cast<GetTypeId_t>(vt[2])(c) == tidOUI) { oui = c; break; }
    }
    if (!oui) {
        fprintf(stderr, "[OrchardForceStart]     cannot assign tree type: no OrchardUI controller "
                        "among %d dialog controllers (tid=%u)\n", cnt, tidOUI);
        return false;
    }

    void* combo = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(oui) + 0x30);
    int items = combo ? reinterpret_cast<const UI::ComboElement*>(combo)->GetItemCount() : -1;  // B.3.6 bridge
    fprintf(stderr, "[OrchardForceStart]     OrchardUI=%p combo(+0x30)=%p items=%d\n", oui, combo, items);
    if (!combo || items <= 0) {
        fprintf(stderr, "[OrchardForceStart]     cannot assign tree type: combo unusable "
                        "(combo=%p items=%d) — refusing to synthesize a pointer\n", combo, items);
        return false;
    }
    // VARIETY, not just "a tree": walk the combo instead of taking item 0 for every
    // orchard. Food Variety wants 8 different orchard trees harvested in one year and
    // Farmer wants 8 orchard seed types owned; item 0 for all of them makes both
    // unreachable however many orchards are built. Still a REAL registered template
    // resolved through the same combo path a player click uses -- only the index moves.
    static int s_treeTurn = 0;
    const int wantItem = s_treeTurn % items;
    ++s_treeTurn;
    reinterpret_cast<UI::ComboElement*>(combo)->SetCurrentItem(wantItem);  // B.3.6 bridge
    void** vt = *reinterpret_cast<void***>(oui);
    typedef void (*Slot9_t)(void*, void*, int, int);
    reinterpret_cast<Slot9_t>(vt[9])(oui, combo, 0x14, 500);   // real combo-change event, msg 0x14 elem 500

    uintptr_t comp = reinterpret_cast<uintptr_t>(orchComp);
    uintptr_t treeTypeAfter = *reinterpret_cast<uintptr_t*>(comp + 0x60);
    fprintf(stderr, "[OrchardForceStart]     drove combo item=%d via OrchardUI::Slot9 -> "
                    "comp+0x60=%p (%s)\n", wantItem, reinterpret_cast<void*>(treeTypeAfter),
            treeTypeAfter ? "SET from real template" : "STILL NULL");
    return treeTypeAfter != 0;
}

void FireForceStart(void* entityUI, int frameNum) {
    void* gw = (g_pGameWorldSingleton && *g_pGameWorldSingleton) ? *g_pGameWorldSingleton : nullptr;
    if (!gw) {
        fprintf(stderr, "[OrchardForceStart] frame=%d: gameWorld singleton null — cannot run\n", frameNum);
        return;
    }
    uintptr_t entityList = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(gw) + 0xe8);
    if (!entityList) {
        fprintf(stderr, "[OrchardForceStart] frame=%d: EntityList (gw+0xe8) null — cannot run\n", frameNum);
        return;
    }
    uintptr_t arr = *reinterpret_cast<uintptr_t*>(entityList + 0x08);
    int       n   = *reinterpret_cast<int32_t*>(entityList + 0x10);
    const uint32_t tidOrchardComp = ::Object::GetTypeIdByName("OrchardComponent");
    if (!arr || n <= 0 || !tidOrchardComp) {
        fprintf(stderr, "[OrchardForceStart] frame=%d: cannot scan (arr=%p n=%d "
                        "tidOrchardComponent=%u)\n", frameNum, reinterpret_cast<void*>(arr), n, tidOrchardComp);
        return;
    }

    int found = 0, switched = 0, typed = 0;
    for (int i = 0; i < n; ++i) {
        void** slot = reinterpret_cast<void**>(arr + static_cast<uintptr_t>(i) * 8);
        void* ent = *slot;
        if (!ent) continue;

        int       cN = *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(ent) + 0x38);
        uintptr_t cA = *reinterpret_cast<uintptr_t*>(static_cast<uint8_t*>(ent) + 0x30);
        void* orchComp = nullptr;
        for (int c = 0; c < cN && cA; ++c) {
            void** cs = reinterpret_cast<void**>(cA + static_cast<uintptr_t>(c) * 8);
            void* comp = *cs;
            if (!comp) continue;
            void** vt = *reinterpret_cast<void***>(comp);
            typedef uint32_t (*GetTypeId_t)(void*);
            if (vt && vt[2] && reinterpret_cast<GetTypeId_t>(vt[2])(comp) == tidOrchardComp) {
                orchComp = comp;
                break;
            }
        }
        if (!orchComp) continue;
        ++found;

        uintptr_t comp = reinterpret_cast<uintptr_t>(orchComp);
        uint8_t   switchBefore = *reinterpret_cast<uint8_t*>(comp + 0x48);
        uintptr_t typeBefore   = *reinterpret_cast<uintptr_t*>(comp + 0x60);

        fprintf(stderr, "[OrchardForceStart] entity=%p comp=%p BEFORE: switch(+0x48)=%u "
                        "treeType(+0x60)=%s\n", ent, orchComp, switchBefore,
                typeBefore ? "SET" : "UNTYPED");

        // 1. Fill the tree type via the real combo-select path if it's still null.
        //
        // ⚠ ORDER MATTERS, and it did not used to. OrchardUI::Slot11 (retail 0x140075380)
        // calls component vtable[14](comp, 2) — Command's TOGGLE — whenever the orchard
        // goes from untyped to typed. That is retail's real "picking a tree type switches
        // the orchard on" path. Until the command slot was wired (2026-08-10) it was an
        // inert stub, so this harness compensated by writing +0x48 = 1 itself, FIRST.
        // Now that the toggle works, doing both flips the switch on and then straight back
        // off — measured: BEFORE switch=1, then `Command(2) toggle -> enabled=0`, and the
        // orchard sat in state 2 planting nothing. So: let the real path run first, and
        // only force the switch if it is still off afterwards.
        bool hasType = (typeBefore != 0);
        if (!hasType) {
            hasType = TryAssignRealTreeType(entityUI, ent, orchComp);
        } else {
            fprintf(stderr, "[OrchardForceStart]     already typed — leaving comp+0x60 alone\n");
        }
        if (hasType) ++typed;

        // 2. Only now, and only if the real path did not already do it.
        if (*reinterpret_cast<uint8_t*>(comp + 0x48) == 0) {
            *reinterpret_cast<uint8_t*>(comp + 0x48) = 1;
            ++switched;
            fprintf(stderr, "[OrchardForceStart]     switch still off after the type-select "
                            "path — forcing +0x48 = 1\n");
        } else {
            fprintf(stderr, "[OrchardForceStart]     switch already on via the REAL "
                            "Command(2) path — no force needed\n");
        }

        uint8_t   switchAfter = *reinterpret_cast<uint8_t*>(comp + 0x48);
        uintptr_t typeAfter   = *reinterpret_cast<uintptr_t*>(comp + 0x60);
        fprintf(stderr, "[OrchardForceStart] entity=%p comp=%p AFTER:  switch(+0x48)=%u "
                        "treeType(+0x60)=%s\n", ent, orchComp, switchAfter,
                typeAfter ? "SET" : "STILL UNTYPED");
    }

    if (found == 0) {
        fprintf(stderr, "[OrchardForceStart] frame=%d: scanned %d entities, found ZERO with an "
                        "OrchardComponent — nothing to force-start (set "
                        "BANISHED_BUILD_DEMO_ORCHARD=1?)\n", frameNum, n);
    } else {
        fprintf(stderr, "[OrchardForceStart] frame=%d: %d orchard(s) found, %d switch(es) forced on, "
                        "%d ended up with a real tree type\n", frameNum, found, switched, typed);
        if (typed < found) {
            fprintf(stderr, "[OrchardForceStart] frame=%d: %d orchard(s) STILL UNTYPED after the "
                            "attempt — see the per-entity lines above for why\n",
                    frameNum, found - typed);
        }
        // Every TryAssignRealTreeType above selected an orchard through the EditTool,
        // which opens its info panel; the LAST one stays on screen. An open panel costs
        // ~0.9 ms of UI update per frame (measured in the BANISHED_STRESS PostSim phase:
        // 0.95 ms vs 0.06 ms once it closed on its own thousands of frames later), which
        // is harness noise in a perf baseline. Close it the way a right-click does.
        if (void* tool = EditTool_HarnessAcquire(entityUI)) {
            uintptr_t dlg = *reinterpret_cast<uintptr_t*>(static_cast<uint8_t*>(tool) + 0x28);
            if (dlg) {
                EditTool_CloseInfoDialog(reinterpret_cast<EditTool*>(tool), dlg);
                fprintf(stderr, "[OrchardForceStart] frame=%d: closed the last orchard panel (%p)\n",
                        frameNum, reinterpret_cast<void*>(dlg));
            }
        }
    }
    std::fflush(stderr);
}

}  // namespace

extern "C" void OrchardForceStart_Tick(void* entityUI, int frameNum) {
    Init();
    if (!s_enabled) return;
    if (s_fired) return;
    if (frameNum < kSettleFrame) return;
    s_fired = true;
    fprintf(stderr, "[OrchardForceStart] firing at frame %d\n", frameNum);
    FireForceStart(entityUI, frameNum);
}
