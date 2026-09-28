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
// BANISHED_FOOD_PANEL_SEAT=1 additionally measures the FIRST-SEAT path: it dumps the
// workplace's profession key / accepting flag / roster / capacity / profession-BST
// bucket, then dispatches WorkPlaceUI vtable slot 9 with the workerCountUp element id
// (0x1450) and reports the roster delta. That click is the ONLY thing in retail or here
// that can seat worker #1 — see the long note at the probe itself.
//
// Runs once, at a settle frame (default 900, override with BANISHED_FOOD_PANEL_FRAME) so
// the world and the demo buildings exist. Test-only, opt-in, default off.

#include <windows.h>
#include <new>
#include "Game/exe_bodies.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "options.h"
#include "Object/factory_registration.h"
#include "Component/AttendanceComponent.h"
#include "Component/Component.h"
#include "Component/WorkComponent.h"
#include "Component/WorkPlaceComponent.h"
#include "Component/WorkPlaceDescription.h"
#include "Dialog/RadiusUI.h"
#include "Dialog/ToolUI.h"
#include "Dialog/WorkPlaceUI.h"
#include "Dialog/exe_bodies.h"      // EntityUI_SetEditMode_Impl
#include "Component/exe_bodies.h"   // CitizenComp_TryEnrolInSchool (0x140111840)
#include "object/TypeInfo.h"        // Object::IsDerivedFromNullSafe
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B

// Both provided by the existing tool harness / dialog layer.
extern "C" int EditTool_HarnessSelectEntity(void* editTool, void* entity);
extern "C" void* EditTool_HarnessAcquire(void* entityUI);
extern "C" bool  EditTool_IsEditTool(void* tool);
extern "C" void* StartTown_ResolveTemplateByPath(const char* path);
// EntityUI_SetEditMode_Impl: Dialog/exe_bodies.h.
// Retail close path (port of Entity_impl_cc90 0x1400fcc90) — destroys the
// DialogInstance and runs every controller's teardown. C++ linkage taking EditTool*,
// so the real declaration is needed for the mangled name to match.
// RadiusUI::Slot6 is dispatched through the controller's own vtable[6] instead of by
// name, which is also the honest test: it proves the SLOT is filled, not just that a
// body exists somewhere in the tree.
#include "Player/edittool_funcs.h"

namespace {

// Shared truthy test over an already-read RAW value (from Opt()): set and not
// "0"/"false" — the same test this file has always applied to the environment.
bool ValOn(const char* v) {
    return v && v[0] && std::strcmp(v, "0") != 0 && std::strcmp(v, "false") != 0;
}

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
    // The four non-food workplaces whose command slots 13/14 were wired 2026-08-15.
    // Their demo spawners are BANISHED_BUILD_DEMO_{TAVERN,MARKET,FORESTERLODGE,SCHOOLHOUSE}.
    { "consumeproduce", "ConsumeProduceComponent", nullptr },
    { "market",     "MarketComponent",    nullptr      },
    { "forester",   "ForesterComponent",  nullptr      },
    { "attendance", "AttendanceComponent", nullptr     },
};
const int kClassCount = (int)(sizeof(kClasses) / sizeof(kClasses[0]));

bool  s_init = false;
char  s_want[32] = {0};
int   s_fireFrame = 900;
bool  s_fired = false;

// Post-seat follow-up state (BANISHED_FOOD_PANEL_SEAT). The seat click proves the
// roster moved; it does not prove anything DOWNSTREAM of a staffed building ran. For a
// schoolhouse the downstream half is the student link, and its retail gate is exactly
// "this workplace has a worker" — CitizenComponent::Update @0x1401131fe dispatches the
// student finder 0x140111590 only for citizens with +0x68 set, and that finder's
// validity test (0x1400440f0) requires the school to be staffed. So a teacher seated at
// frame N should show students queuing some frames later, and if it does not, the defect
// is downstream of this harness rather than in the seat. Report the queue over time so
// the two are never conflated again.
void*    s_seatComp   = nullptr;   // the workplace component we seated into
void*    s_seatEntity = nullptr;
int      s_seatFrame  = 0;
int      s_followsLeft = 0;
int      s_nextFollow = 0;

void Init() {
    if (s_init) return;
    s_init = true;
    const char* e = Opt().foodPanel;
    if (!e || !e[0] || std::strcmp(e, "0") == 0) return;
    std::snprintf(s_want, sizeof(s_want), "%s", e);
    if (const char* f = Opt().foodPanelFrame) {
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
        void* c = *cs;
        if (!c) continue;
        if (reinterpret_cast<Object::Base*>(c)->Type() == tid) return c;   // B.3.8 bridge
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

    void* gw = (GameUpdateLoop::_singleton != nullptr) ? GameUpdateLoop::_singleton : nullptr;
    if (!gw) { std::fprintf(stderr, "[FoodPanel] gameWorld null\n"); return; }
    uintptr_t el = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(gw) + 0xe8);
    if (!el) { std::fprintf(stderr, "[FoodPanel] EntityList null\n"); return; }
    uintptr_t arr = *reinterpret_cast<uintptr_t*>(el + 0x08);
    const int n = *reinterpret_cast<int32_t*>(el + 0x10);

    void* ent = nullptr; void* comp = nullptr;
    for (int i = 0; i < n && arr && !ent; ++i) {
        void** slot = reinterpret_cast<void**>(arr + static_cast<uintptr_t>(i) * 8);
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
    // Slots 13/14 are CommandMask/Command, Component virtuals (real classes since
    // ENDGAME B.3.8, so neither slot can be null any more). `comp` is an element of the
    // entity's component array matched by Type().
    Component* ccomp = reinterpret_cast<Component*>(comp);   // B.3.8 bridge
    std::fprintf(stderr, "[FoodPanel] %-19s ent=%p comp=%p\n", fc.component, ent, comp);

    {
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
            r[id] = static_cast<uint8_t>(ccomp->CommandMask(id));
            if (r[id]) ++enabled;
            if (r[id] != r[0]) uniform = false;
        }
        if (uniform && r[0] != 0) {
            // ⚠ A uniform non-zero result does NOT identify the form: a per-command
            // CanCommand whose toggles all happen to be off answers 1 for every id via
            // its `else` arm, which is indistinguishable from the ResourceProduce mask.
            // Report the value and the bit decode, which are true either way; do not
            // assert the form.
            //
            // ⚠ The example this note used to cite — "ForesterComponent looks uniform
            // on a fresh lodge" — is EXPIRED as of the controller promotion later on
            // 2026-08-15. It was true only because our factory zero-filled the body;
            // ForesterComponent_Ctor actually writes +0x50 = 0x101, and now that the
            // hand-expanded factory reproduces that, a fresh lodge reports
            // "0:1 1:3 2:3 3:1 …" — non-uniform, and it lands in the per-id branch
            // below. The reasoning above still stands; the illustration no longer does.
            std::fprintf(stderr, "[FoodPanel]   command value = 0x%02X for every id "
                         "(bit1=%s bit0=%s; on the ResourceProduce base these mean "
                         "pause/resume offered). Form NOT determined — a per-command "
                         "CanCommand can be uniform when all its toggles are off.\n",
                         (unsigned)r[0],
                         (r[0] & 2) ? "set" : "clear", (r[0] & 1) ? "set" : "clear");
        } else if (uniform) {
            // All zero says nothing about the FORM — a per-command CanCommand that
            // legitimately refuses everything looks identical to a mask of 0. Do not
            // assert which one it is; a crop field with no slot array (unplanted)
            // greys all three buttons, and that is retail behaviour, not a defect.
            std::fprintf(stderr, "[FoodPanel]   every command returns 0 — all buttons "
                         "disabled. Form indeterminate from this alone; for a crop field "
                         "this is CORRECT while unplanted (+0x50 slot array still null).\n");
        } else {
            std::fprintf(stderr, "[FoodPanel]   CanCommand(id) =");
            for (int id = 0; id < 8; ++id)
                std::fprintf(stderr, " %d:%u", id, (unsigned)r[id]);
            std::fprintf(stderr, "   (%d of 8 enabled%s)\n", enabled,
                         enabled == 0 ? "  <-- every button dead" : "");
        }

        // ── actually CLICK it (BANISHED_FOOD_PANEL_CLICK=1) ───────────────────────
        // A filled slot 13 proves the checkbox can be DRAWN enabled; it does not prove
        // the click does anything — slot 14 is a separate body and was independently
        // dead here for months (the event-id 0-vs-3 mismatch, fixed in 330affb).
        // Dispatch slot 14 and re-read slot 13: a working pair must CHANGE the mask,
        // because pause and resume are mutually exclusive offers.
        //
        // Retail ignores the command id on the ResourceProduce base pair and on
        // Attendance, but the Forester override reads it, so send id 1 (its "plant"
        // toggle) rather than 0 — id 0 would be a no-op on the forester and would make
        // a working slot 14 look dead.
        if (ValOn(Opt().foodPanelClick)) {
            const uint8_t was = r[1];
            ccomp->Command(1);
            const uint8_t now = static_cast<uint8_t>(ccomp->CommandMask(1));
            std::fprintf(stderr, "[FoodPanel]   CLICK cmd=1 -> mask 0x%02X -> 0x%02X  %s\n",
                         (unsigned)was, (unsigned)now,
                         (now != was) ? "OK — the command changed state"
                                      : "NO CHANGE — slot 14 ran but nothing moved");
        }
    }

    // ── open the panel ────────────────────────────────────────────────────────────
    // Must be an EditTool, not merely "a tool" — see EditTool_HarnessAcquire.
    void* tool = EditTool_HarnessAcquire(entityUI);
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
        const uint32_t t = reinterpret_cast<Object::Base*>(c)->Type();   // B.3.8 bridge
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

    // ── FIRST-SEAT PROBE (BANISHED_FOOD_PANEL_SEAT=1) ─────────────────────────────
    // Retail has NO organic first-seat for a workplace, and neither do we — that is
    // faithful, not a gap. Proof, from Application-x64.exe:
    //   * WPC+0x60 is the LIVE ROSTER COUNT, not a capacity or a request: Detach
    //     (0x14012f7e0) removes from the +0x58 IBP array and then tests `+0x60 == 0`
    //     to mean "roster is now empty, hide the worker mesh".
    //   * The periodic employment allocator (0x1400d43b0) builds its Phase-2 need
    //     table under `if (*(int *)(wpc + 0x60) != 0)`, seeding remaining AND the
    //     denominator from that same roster count. It re-solves WHO fills the seats a
    //     building already has; it structurally cannot open seat #1 anywhere.
    //   * Build completion writes no roster at all. What it writes is the CAPACITY:
    //     f670 (0x14012f670, the state-2 "built" enter reached via edf0) does
    //     `WorkComponent+0x30 = CalcMaxWorkers(workComp)`. FindAvailableBuilding
    //     (0x14008d100) and the citizen finder ffc0 (0x14010ffc0) both lazily refill
    //     that same field when it reads 0, so a missed publish is self-healing.
    //   * Seating is then citizen-driven per tick: CitizenComponent::Update @
    //     0x1401135d1 — `if (citizen+0x248 == 0) { ffc0/0470; AssignHousing; }` — but
    //     it searches ONLY the bucket for the citizen's OWN profession key (+0x1f0).
    //     A laborer's default key has no bucket, so a laborer never self-seats.
    //
    // So the one thing that converts a laborer into this building's profession is the
    // panel's workerCountUp click (element id 0x1450 -> WorkPlaceUI vtable slot 9).
    // That single click is the entire first-seat mechanism, which makes it worth an
    // explicit end-to-end measurement rather than a code read: dispatch the slot and
    // report the roster DELTA. Everything downstream ("no teacher" -> "no students")
    // hangs off this one dispatch.
    if (ValOn(Opt().foodPanelSeat)) {
        void* wpc = FindCompOnEntity(ent, WorkPlaceComponent::TypeId());
        void* wc  = FindCompOnEntity(ent, WorkComponent::TypeId());
        if (!wpc) {
            std::fprintf(stderr, "[FoodPanel]   seat: no WorkPlaceComponent on this entity — "
                         "not an employer, nothing to seat\n");
        } else {
            const uintptr_t w = reinterpret_cast<uintptr_t>(wpc);
            const uint64_t key   = *reinterpret_cast<uint64_t*>(w + 0x48);
            const int      accept= *reinterpret_cast<uint8_t*>(w + 0x50);
            const int      roster= *reinterpret_cast<int32_t*>(w + 0x60);
            const int      cap   = wc ? *reinterpret_cast<int32_t*>(
                                        reinterpret_cast<uintptr_t>(wc) + 0x30) : -1;

            // Is the owner registered in the town profession BST under that key? An
            // unregistered workplace is invisible to BOTH finders, so a dead click
            // would otherwise be indistinguishable from a broken slot 9.
            int bucket = -1;
            void* gw2 = (GameUpdateLoop::_singleton != nullptr) ? GameUpdateLoop::_singleton : nullptr;
            uintptr_t townMgr = gw2 ? *reinterpret_cast<uintptr_t*>(
                                    reinterpret_cast<uintptr_t>(gw2) + 0xf0) : 0;
            if (townMgr && key) {
                uint64_t* node = *reinterpret_cast<uint64_t**>(townMgr + 0x100);
                while (node) {
                    if (key < node[0])      node = reinterpret_cast<uint64_t*>(node[4]);
                    else if (key > node[0]) node = reinterpret_cast<uint64_t*>(node[5]);
                    else { bucket = static_cast<int>(node[2]); break; }
                }
            }
            // WPC+0x48 is seated from `WorkPlaceDescription+0x20`, but only retail's f670
            // guards that read with IsDerivedFrom(WPC+0x18, WorkPlaceDescription) first —
            // StartTown_ActivateWorkplace reads +0x18 -> +0x20 RAW, and a start-town
            // building never runs f670. If +0x18 is some other description, +0x20 is the
            // wrong field of the wrong object and the key matches nothing. Check the type.
            {
                void* wpDesc = *reinterpret_cast<void**>(w + 0x18);
                const uint32_t tidWpd = WorkPlaceDescription::TypeId();
                uint32_t got = 0;
                if (wpDesc) got = reinterpret_cast<Object::Base*>(wpDesc)->Type();   // B.3.8 bridge
                std::fprintf(stderr, "[FoodPanel]   seat PRE: WPC+0x18 desc=%p typeId=%08X "
                             "(WorkPlaceDescription=%08X) desc+0x20=0x%llx  %s\n",
                             wpDesc, got, tidWpd,
                             (unsigned long long)(wpDesc ? *reinterpret_cast<uint64_t*>(
                                 reinterpret_cast<uintptr_t>(wpDesc) + 0x20) : 0),
                             !wpDesc              ? "NO DESCRIPTION"
                           : (got == tidWpd)      ? "type OK — +0x20 really is the work profession"
                                                  : "WRONG TYPE — +0x48 was seated from the wrong object");
            }
            std::fprintf(stderr, "[FoodPanel]   seat PRE: profKey(+0x48)=0x%llx accepting(+0x50)=%d "
                         "roster(+0x60)=%d capacity(WC+0x30)=%d bstBucket=%d%s\n",
                         (unsigned long long)key, accept, roster, cap, bucket,
                         key == 0        ? "   <-- key 0: panel hides the spinner, cannot seat"
                       : bucket < 0      ? "   <-- NOT REGISTERED in townMgr+0x100: both finders are blind to it"
                       : accept == 0     ? "   <-- not accepting: FindAvailableBuilding skips it"
                                         : "");

            // Find the WorkPlaceUI controller on the dialog and dispatch ITS slot 9 —
            // dispatching through the controller's own vtable is the honest test (it
            // proves the slot is filled, not merely that a body exists somewhere).
            const uint32_t tidWpUi = WorkPlaceUI::TypeId();
            WorkPlaceUI* wpUi = nullptr;
            for (int i = 0; ctrlArr && i < cnt; ++i) {
                void* c = ctrlArr[i];
                if (!c) continue;
                if (reinterpret_cast<Object::Base*>(c)->Type() == tidWpUi) {   // B.3.8 bridge
                    wpUi = reinterpret_cast<WorkPlaceUI*>(c); break;           // B.3.8 bridge
                }
            }
            if (!wpUi) {
                std::fprintf(stderr, "[FoodPanel]   seat: WorkPlaceUI (tid=%08X) ABSENT from this "
                             "dialog — the worker spinner does not exist here\n", tidWpUi);
            } else {
                {
                    // msg 0x12 = button command, id 0x1450 = workerCountUp. WorkPlaceUI is a
                    // real class (ENDGAME B.3.8): slot 9 is its OnElementEvent override.
                    wpUi->OnElementEvent(nullptr, 0x12, 0x1450, 0, 0, 0, nullptr);
                    const int rosterNow = *reinterpret_cast<int32_t*>(w + 0x60);
                    const int capNow = wc ? *reinterpret_cast<int32_t*>(
                                           reinterpret_cast<uintptr_t>(wc) + 0x30) : -1;
                    std::fprintf(stderr, "[FoodPanel]   seat CLICK workerCountUp(0x1450): "
                                 "roster %d -> %d, capacity %d -> %d   %s\n",
                                 roster, rosterNow, cap, capNow,
                                 rosterNow > roster
                                   ? "OK — the click seated a worker (first-seat path is LIVE)"
                                   : "NO CHANGE — slot 9 ran but nobody was seated");
                    if (rosterNow > roster) {
                        s_seatComp   = wpc;
                        s_seatEntity = ent;
                        s_followsLeft = 6;      // ~6 reports
                    }
                }
            }
        }
        std::fflush(stderr);
    }

    // ── TEARDOWN (BANISHED_FOOD_PANEL_CLOSE=1) ────────────────────────────────────
    // Closes the panel we just opened. Until this existed the harness left every panel
    // open, so RadiusUI::Slot6 (retail 0x140077600) and its `+0x30 = -1` reset were
    // CODE-REVIEWED ONLY — never once executed in a run. RadiusUI is the first
    // controller on the dock / hunter-lodge / gatherer-hut panels, so its teardown is
    // the one that leaks a RadiusDisplayManager slot if it is wrong.
    //
    // Two stages, deliberately in this order:
    //   (a) dispatch the controller's own idx6 and re-read +0x30 while the object is
    //       still ALIVE. Reading it after CloseDialog would be a use-after-free, and a
    //       freed-memory read that happens to contain -1 proves nothing.
    //   (b) then run the real retail close path (EditTool_CloseInfoDialog ->
    //       UI::UserInterface::CloseDialog), which destroys the DialogInstance and every
    //       controller. Slot6 gets dispatched a second time there, so this also proves
    //       the teardown is IDEMPOTENT — the second pass must see overlay=-1 and must
    //       not call RadiusDisplay_Remove again on a slot it no longer owns.
    //
    // Pair with BANISHED_DIALOG_TRACE=1 to see the `[RadiusUI] Slot6 ... overlay=N` lines.
    if (!ValOn(Opt().foodPanelClose)) return;

    const uint32_t tidRadius = RadiusUI::TypeId();
    RadiusUI* radius = nullptr;
    for (int i = 0; ctrlArr && i < cnt; ++i) {
        void* c = ctrlArr[i];
        if (!c) continue;
        if (reinterpret_cast<Object::Base*>(c)->Type() == tidRadius) {   // B.3.8 bridge
            radius = reinterpret_cast<RadiusUI*>(c); break;              // B.3.8 bridge
        }
    }
    if (!radius) {
        std::fprintf(stderr, "[FoodPanel]   teardown: no RadiusUI on this panel "
                     "(tid=%08X) — nothing to measure here\n", tidRadius);
    } else {
        const int before = *reinterpret_cast<int32_t*>(
            reinterpret_cast<uintptr_t>(radius) + 0x30);
        {
            radius->OnClosed();   // slot 6, RadiusUI's override (real class, ENDGAME B.3.8)
            const int after = *reinterpret_cast<int32_t*>(
                reinterpret_cast<uintptr_t>(radius) + 0x30);
            std::fprintf(stderr, "[FoodPanel]   teardown: RadiusUI=%p overlay %d -> %d  %s\n",
                         static_cast<void*>(radius), before, after,
                         (after == -1)
                             ? ((before >= 0) ? "OK (had a live overlay, released + reset)"
                                              : "OK but VACUOUS — no overlay was ever built, so "
                                                "the release path did not run; check Slot5")
                             : "FAIL — +0x30 was not reset to -1");
        }
    }

    // BANISHED_FOOD_PANEL_CLOSE=2 — close through the PLAYER'S path: the panel's
    // ToolUI close button (slot 9, msg 0x12 id 1), not EditTool_CloseInfoDialog.
    // That is the call site in the open HighlightClear crash report ("closing a
    // panel"), and it differs from the harness close in one way that matters: it
    // reaches Impl_cb40 through the ACTIVE tool it looks up itself, instead of the
    // tool the caller already holds.
    if (Opt().foodPanelClose && Opt().foodPanelClose[0] == '2') {
        const uint32_t tidToolUI = ToolUI::TypeId();
        ToolUI* toolUi = nullptr;
        for (int i = 0; ctrlArr && i < cnt; ++i) {
            void* c = ctrlArr[i];
            if (!c) continue;
            if (Object::IsDerivedFromNullSafe(c, tidToolUI)) {
                toolUi = reinterpret_cast<ToolUI*>(c); break;   // B.3.8 bridge
            }
        }
        // BANISHED_FOOD_PANEL_ARM_BUILDTOOL=1 — arm a BUILD tool while the panel is
        // still open, then click close. That is the player sequence the HighlightClear
        // crash was reported from: EntityUI+0x28 now holds a FencedTool, whose +0x30 is
        // the drag endCell pair rather than a selected entity, and the close handler
        // used to hand it to Entity_impl_cb40 regardless of type.
        if (ValOn(Opt().foodPanelArmBuildTool)) {
            void* tmpl = StartTown_ResolveTemplateByPath("Template\\Pasture.rsc");
            if (!tmpl) {
                std::fprintf(stderr, "[FoodPanel]   teardown: Pasture template unresolved — "
                             "build tool NOT armed\n");
            } else {
                EntityUI_SetEditMode_Impl(entityUI, reinterpret_cast<uint64_t>(tmpl));
                void* armed = *reinterpret_cast<void**>(static_cast<uint8_t*>(entityUI) + 0x28);
                std::fprintf(stderr, "[FoodPanel]   teardown: armed build tool %p at "
                             "EntityUI+0x28 before the close click\n", armed);
            }
            std::fflush(stderr);
        }
        if (!toolUi) {
            std::fprintf(stderr, "[FoodPanel]   teardown: no ToolUI-derived controller on "
                         "this panel — cannot drive the close button\n");
        } else {
            std::fprintf(stderr, "[FoodPanel]   teardown: close BUTTON via ToolUI=%p\n",
                         static_cast<void*>(toolUi));
            std::fflush(stderr);
            toolUi->OnElementEvent(nullptr, 0x12, 1, 0, 0, 0, nullptr);   // slot 9
            // Re-read the CURRENT active tool: with ARM_BUILDTOOL the EditTool we
            // opened with has been destroyed by SetEditMode, so the captured `tool`
            // pointer is dangling and must not be dereferenced.
            void* nowTool = *reinterpret_cast<void**>(static_cast<uint8_t*>(entityUI) + 0x28);
            void* afterBtn = (nowTool && EditTool_IsEditTool(nowTool))
                ? *reinterpret_cast<void**>(static_cast<uint8_t*>(nowTool) + 0x28) : nullptr;
            std::fprintf(stderr, "[FoodPanel]   teardown: close button -> activeTool=%p%s "
                         "dialogRef=%p %s\n", nowTool,
                         (nowTool && !EditTool_IsEditTool(nowTool)) ? " (not an EditTool)" : "",
                         afterBtn, afterBtn ? "STILL SET" : "(cleared)");
            std::fflush(stderr);
            return;
        }
    }

    // (b) the real close path.
    EditTool_CloseInfoDialog(reinterpret_cast<EditTool*>(tool), reinterpret_cast<uintptr_t>(dialog));
    void* after = *reinterpret_cast<void**>(static_cast<uint8_t*>(tool) + 0x28);
    std::fprintf(stderr, "[FoodPanel]   teardown: CloseInfoDialog -> tool dialog=%p %s\n",
                 after, after ? "STILL OPEN — close path did not take" : "(closed)");
    std::fflush(stderr);
}

}  // namespace

// Follow-up report on the workplace the seat probe staffed: does the roster HOLD, and
// does anything downstream (the school's student queue) move now that it is staffed?
void FollowUp(int frameNum) {
    if (!s_seatComp) return;
    const uintptr_t w = reinterpret_cast<uintptr_t>(s_seatComp);
    const int roster = *reinterpret_cast<int32_t*>(w + 0x60);

    // AttendanceComponent, when present: queue count at +0x30, capacity at desc+0x20.
    int queued = -1, capacity = -1;
    if (void* att = FindCompOnEntity(s_seatEntity, AttendanceComponent::TypeId())) {
        const uintptr_t a = reinterpret_cast<uintptr_t>(att);
        queued = *reinterpret_cast<int32_t*>(a + 0x30);
        if (void* d = *reinterpret_cast<void**>(a + 0x18))
            capacity = *reinterpret_cast<int32_t*>(reinterpret_cast<uintptr_t>(d) + 0x20);
    }
    std::fprintf(stderr, "[FoodPanel]   seat +%d frames: roster(+0x60)=%d%s",
                 frameNum - s_seatFrame, roster,
                 roster == 0 ? "  <-- the seat did NOT hold, something detached the worker" : "");
    if (queued >= 0)
        std::fprintf(stderr, "  students queued(+0x30)=%d of %d%s",
                     queued, capacity,
                     queued == 0 ? "  <-- staffed but no student has linked yet" : "");
    std::fprintf(stderr, "\n");

    // If the building is staffed and STILL has no attendees, the defect is on the
    // citizen side, so measure that side rather than guessing. Both attendance finders
    // (0x140111590 -> citizen+0x228, 0x1401116f0 -> citizen+0x230) resolve through
    // sub_1930 (0x140111930), which matches a candidate building by
    // `WorkPlaceComponent+0x48 == key`, where the key is read off the CITIZEN's own
    // record at +0x288 (+0x30 for the first slot, +0x28 for the second). So a citizen
    // can only ever attend this building if one of those two keys EQUALS this
    // building's profession key. Count that population directly — "N citizens carry the
    // key" vs "0 do" separates a broken finder from a town that simply has nobody
    // eligible, and those two have opposite fixes.
    if (queued == 0 && roster > 0) {
        void* gw3 = (GameUpdateLoop::_singleton != nullptr) ? GameUpdateLoop::_singleton : nullptr;
        uintptr_t tm = gw3 ? *reinterpret_cast<uintptr_t*>(
                            reinterpret_cast<uintptr_t>(gw3) + 0xf0) : 0;
        if (tm) {
            const uint64_t key = *reinterpret_cast<uint64_t*>(w + 0x48);
            uintptr_t czArr = *reinterpret_cast<uintptr_t*>(tm + 0xa0);
            const int czN = *reinterpret_cast<int32_t*>(tm + 0xa8);
            int total = 0, noRec = 0, k30 = 0, k28 = 0, linked228 = 0, linked230 = 0;
            for (int i = 0; i < czN && czArr; ++i) {
                void** cs = reinterpret_cast<void**>(czArr + static_cast<uintptr_t>(i) * 8);
                uintptr_t cz = reinterpret_cast<uintptr_t>(*cs);
                if (!cz) continue;
                ++total;
                uintptr_t rec = *reinterpret_cast<uintptr_t*>(cz + 0x288);
                if (!rec) { ++noRec; continue; }
                if (*reinterpret_cast<uint64_t*>(rec + 0x30) == key) ++k30;
                if (*reinterpret_cast<uint64_t*>(rec + 0x28) == key) ++k28;
                if (*reinterpret_cast<uint64_t*>(cz + 0x228)) ++linked228;
                if (*reinterpret_cast<uint64_t*>(cz + 0x230)) ++linked230;
                // "Nobody carries the key" has two very different causes: the town has
                // no eligible citizens (faithful), or the record's key fields were never
                // populated at all (a data-load gap). All-zero keys distinguish them, so
                // print the raw pair for the first few citizens rather than only the count.
                if (i < 1) {
                    // CitizenDescription carries THREE ExternalRef<Profession> (+0x20,
                    // +0x28, +0x30); only the last two feed the attendance finders, so
                    // print all three next to the two professions we can already name
                    // (profReg+0x30 default/laborer, +0x38 builder). That turns three
                    // anonymous pointers into an identified map instead of a guess.
                    uintptr_t pr = *reinterpret_cast<uintptr_t*>(
                                       reinterpret_cast<uintptr_t>(gw3) + 0xb0);
                    std::fprintf(stderr, "[FoodPanel]       citizen[%d]=%p rec(+0x288)=%p\n"
                                 "[FoodPanel]         rec+0x20=0x%llx  rec+0x28=0x%llx  rec+0x30=0x%llx\n"
                                 "[FoodPanel]         schoolWPC+0x48=0x%llx  laborer(profReg+0x30)=0x%llx  "
                                 "builder(profReg+0x38)=0x%llx\n",
                                 i, reinterpret_cast<void*>(cz), reinterpret_cast<void*>(rec),
                                 (unsigned long long)*reinterpret_cast<uint64_t*>(rec + 0x20),
                                 (unsigned long long)*reinterpret_cast<uint64_t*>(rec + 0x28),
                                 (unsigned long long)*reinterpret_cast<uint64_t*>(rec + 0x30),
                                 (unsigned long long)key,
                                 (unsigned long long)(pr ? *reinterpret_cast<uint64_t*>(pr + 0x30) : 0),
                                 (unsigned long long)(pr ? *reinterpret_cast<uint64_t*>(pr + 0x38) : 0));
                }
            }
            // Name the two keys the citizen record actually carries by finding the
            // building that requires each one. Citizens all share one record here, so
            // those two keys are the town-wide "where do I go when sick / when a
            // student" pair — if neither resolves to this building, the mismatch is the
            // whole story and the owning building tells us which key is which.
            {
                uintptr_t bArr = *reinterpret_cast<uintptr_t*>(tm + 0x60);
                const int bN = *reinterpret_cast<int32_t*>(tm + 0x68);
                uint64_t want[2] = {0, 0};
                if (czArr && czN > 0) {
                    void** cs0 = reinterpret_cast<void**>(czArr);
                    if (uintptr_t cz0 = reinterpret_cast<uintptr_t>(*cs0)) {
                        if (uintptr_t r0 = *reinterpret_cast<uintptr_t*>(cz0 + 0x288)) {
                            want[0] = *reinterpret_cast<uint64_t*>(r0 + 0x28);
                            want[1] = *reinterpret_cast<uint64_t*>(r0 + 0x30);
                        }
                    }
                }
                const uint32_t tidWpc = WorkPlaceComponent::TypeId();
                // Dump the registry itself. sub_1930 (0x140111930) scans townMgr+0x60
                // (count +0x68) — the ATTENDANCE component list, not the entity list:
                // FindStorage appends the citizen into `found+0x28` with count
                // `found+0x30`, which is AttendanceComponent's own queue. So a school
                // that is absent from this list is unreachable no matter what its key
                // is, and that is a different bug from a key mismatch. Print both.
                std::fprintf(stderr, "[FoodPanel]       townMgr+0x60 attendance registry: %d entr%s\n",
                             bN, bN == 1 ? "y" : "ies");
                bool selfListed = false;
                for (int i = 0; i < bN && bArr; ++i) {
                    void** bs = reinterpret_cast<void**>(bArr + static_cast<uintptr_t>(i) * 8);
                    uintptr_t b = reinterpret_cast<uintptr_t>(*bs);
                    if (!b) continue;
                    void* sub = *reinterpret_cast<void**>(b + 0x10);
                    void* c = FindCompOnEntity(sub, tidWpc);
                    const uint64_t k48 = c ? *reinterpret_cast<uint64_t*>(
                                            reinterpret_cast<uintptr_t>(c) + 0x48) : 0;
                    if (b == reinterpret_cast<uintptr_t>(s_seatComp) || sub == s_seatEntity)
                        selfListed = true;
                    if (i < 8)
                        std::fprintf(stderr, "[FoodPanel]         [%d] comp=%p entity=%p wpc=%p "
                                     "wpc+0x48=0x%llx queue(+0x30)=%d\n",
                                     i, reinterpret_cast<void*>(b), sub, c,
                                     (unsigned long long)k48,
                                     *reinterpret_cast<int32_t*>(b + 0x30));
                }
                std::fprintf(stderr, "[FoodPanel]       this schoolhouse listed in the registry: %s\n",
                             selfListed ? "YES"
                                        : "NO  <-- sub_1930 can never see it; the key is not the problem");
                for (int k = 0; k < 2; ++k) {
                    if (!want[k]) continue;
                    const char* owner = "NO BUILDING IN TOWN REQUIRES THIS KEY";
                    for (int i = 0; i < bN && bArr; ++i) {
                        void** bs = reinterpret_cast<void**>(bArr + static_cast<uintptr_t>(i) * 8);
                        uintptr_t b = reinterpret_cast<uintptr_t>(*bs);
                        if (!b) continue;
                        void* sub = *reinterpret_cast<void**>(b + 0x10);
                        void* c = FindCompOnEntity(sub, tidWpc);
                        if (!c) continue;
                        if (*reinterpret_cast<uint64_t*>(reinterpret_cast<uintptr_t>(c) + 0x48) != want[k])
                            continue;
                        owner = "a building exists with this key";
                        if (FindCompOnEntity(sub, AttendanceComponent::TypeId()))
                            owner = "an ATTENDANCE building (school/hospital) has this key";
                        break;
                    }
                    std::fprintf(stderr, "[FoodPanel]       record key +0x%02x = 0x%llx -> %s%s\n",
                                 k == 0 ? 0x28 : 0x30, (unsigned long long)want[k], owner,
                                 want[k] == key ? "  == THIS building" : "");
                }
                // The SCHOOL key is the third ref, +0x20, consumed by the misnamed
                // CitizenComp_TryEnrolInSchool (0x140111840) — enrollment, which runs ONLY
                // at the child->adult transition. So an empty queue here is expected
                // unless somebody actually grew up during the run.
                if (czArr && czN > 0) {
                    int minors = 0, students = 0;
                    for (int i = 0; i < czN; ++i) {
                        void** cs = reinterpret_cast<void**>(czArr + static_cast<uintptr_t>(i) * 8);
                        uintptr_t cz = reinterpret_cast<uintptr_t>(*cs);
                        if (!cz) continue;
                        uintptr_t rec = *reinterpret_cast<uintptr_t*>(cz + 0x288);
                        if (rec && *reinterpret_cast<float*>(cz + 0x3c)
                                     < *reinterpret_cast<float*>(rec + 0x44)) ++minors;
                        if (*reinterpret_cast<uint64_t*>(cz + 0x208)) ++students;
                    }
                    std::fprintf(stderr, "[FoodPanel]       enrolment population: %d below adult age "
                                 "(+0x3c < rec+0x44), %d already enrolled (+0x208)%s\n",
                                 minors, students,
                                 minors == 0
                                   ? "   <-- nobody can enrol: enrolment fires only on the "
                                     "child->adult edge, and every founder spawned adult"
                                   : "");
                    // Minors exist but none has crossed the adult edge during this short
                    // run, so waiting proves nothing either way. Drive the enroller
                    // (0x140111840) directly on one minor: that isolates "the finder /
                    // sub_40f0 gate refuses" from "the edge simply has not fired yet",
                    // which a soak alone cannot separate.
                    if (minors > 0 && students == 0) {
                        void* victim = nullptr;
                        for (int i = 0; i < czN && !victim; ++i) {
                            void** cs = reinterpret_cast<void**>(czArr + static_cast<uintptr_t>(i) * 8);
                            uintptr_t cz = reinterpret_cast<uintptr_t>(*cs);
                            if (!cz) continue;
                            uintptr_t rec = *reinterpret_cast<uintptr_t*>(cz + 0x288);
                            if (rec && *reinterpret_cast<float*>(cz + 0x3c)
                                         < *reinterpret_cast<float*>(rec + 0x44))
                                victim = reinterpret_cast<void*>(cz);
                        }
                        if (!victim) {
                            std::fprintf(stderr, "[FoodPanel]       enrol probe: %s\n",
                                         "no minor found");
                        } else {
                            // Read the queue off the ATTENDANCE component the enroller
                            // picked (citizen+0x208), NOT off `w` — `w` is the
                            // WorkPlaceComponent, whose +0x30 is an unrelated field, and
                            // printing it made a successful enrolment read as "queue 0 -> 0".
                            int64_t seed = 0;
                            new (&seed) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
                            // 0x140111840 (the enroller).
                            const char ok = CitizenComp_TryEnrolInSchool(
                                reinterpret_cast<CitizenComponent*>(victim), &seed);   // B.3.x bridge
                            uintptr_t att = *reinterpret_cast<uintptr_t*>(
                                                reinterpret_cast<uintptr_t>(victim) + 0x208);
                            std::fprintf(stderr, "[FoodPanel]       enrol probe on minor %p: "
                                         "returned %d, +0x208=%p, that component's queue(+0x30)=%d   %s\n",
                                         victim, (int)ok, reinterpret_cast<void*>(att),
                                         att ? *reinterpret_cast<int32_t*>(att + 0x30) : -1,
                                         ok ? "OK — enrolment works; the edge just had not fired"
                                            : "REFUSED — the finder or its sub_40f0 gate is the defect");
                        }
                    }
                }
            }
            std::fprintf(stderr, "[FoodPanel]     eligibility: %d citizens, %d with no +0x288 record; "
                         "carrying THIS key: +0x30(slot 228)=%d +0x28(slot 230)=%d; "
                         "already linked: +0x228=%d +0x230=%d%s\n",
                         total, noRec, k30, k28, linked228, linked230,
                         (k30 == 0 && k28 == 0)
                           ? "   <-- NOBODY is eligible: the finders can never match this building"
                           : "   <-- eligible citizens EXIST, so the finder or its sub_40f0 gate is refusing them");
        }
    }
    std::fflush(stderr);
}

extern "C" void FoodPanel_Tick(void* entityUI, int frameNum) {
    Init();
    if (!s_want[0]) return;
    if (s_fired) {
        if (s_followsLeft > 0 && frameNum >= s_nextFollow) {
            --s_followsLeft;
            s_nextFollow = frameNum + 600;
            FollowUp(frameNum);
        }
        return;
    }
    if (frameNum < s_fireFrame) return;
    s_fired = true;
    s_seatFrame = frameNum;
    s_nextFollow = frameNum + 600;
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
