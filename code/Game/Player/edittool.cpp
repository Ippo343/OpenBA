// code/Game/Player/edittool.cpp
// Clean reimplementation of EditTool vtable methods from Application-x64.exe.
//
// EditTool (0x38 bytes) extends Tool (0x28 bytes) directly.
// Handles entity selection and editing in-game.
//
// Vtable at 0x140161ca0 (13 slots). Four are reimplemented here:
//   Slot0   Dtor       (0x14002db00) — scalar deleting destructor
//   Slot6   OnKey      (0x1400fd030) — entity selection via ray picking
//   Slot7   Startup    (0x1400fcda0) — dialog positioning near entity
//   Slot10  Serialize  (0x1400fd290) — compare entity reference
//
// Decompiled from Application-x64.exe.

#include "Player/edittool_funcs.h"
#include "options.h"
#include "EditTool.h"
#include "Player/dragtool.h"   // Tool + DragTool (ENDGAME B.3.8 TOOL)
#include "Component/ModelComponent.h"
#include "Dialog/ToolUI.h"
#include "Dialog/EntityUI.h"
#include "Dialog/EntityUI.h"
#include "Dialog/ToolUI.h"
#include "engine_funcs.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "volume/Volume.h"
#include "Player/exe_bodies.h"
#include "Dialog/exe_bodies.h"
#include "object/TypeInfo.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.6 UI callers (K4)
#include "runtime/types/ui/DialogInstance.h"   // ENDGAME B.3.6 UI callers (K4)
#include "ui/Element.h"        // ENDGAME B.3.6 UI callers (K4)
#include "ui/UserInterface.h"  // ENDGAME B.3.6 UI callers (K4)
#include "picking/Scene.h"     // ENDGAME B.3.7 callers (K2)
#include "input/Input.h"       // ENDGAME B.3.7 callers (K3)
#include "video/Device.h"   // ENDGAME B.3.9 SS2: Video::Device::_singleton

// BANISHED_SELECT_DIAG=1 traces the live EditTool pick path (A/B-safe: off by default).
static int SelDiagOn() {
    static int s = -1;
    if (s < 0) { const char* e = Opt().selectDiag; s = (e && e[0] && e[0] != '0') ? 1 : 0; }
    return s;
}

// Forward decl (full header is included in the STANDALONE factory block at file end).
void EntityUI_SetEditMode_Impl(void* playerController, uint64_t mode);

// ============================================================
// EditTool Slot10 (vtable +0x50) — "is this the entity I have selected?"
// ============================================================
// Calls InternalBasePtr::CheckPtr on self->lastEntityRef (+0x30), then compares
// *entityRef with it. Returns true if the entity references match — i.e. whether a
// given entity is the one currently being edited/selected by this tool.
//
// NAMING (2026-07-31): this was `EditTool_Serialize_Impl`. It is NOT a serializer;
// the name came from an early guess at the slot's purpose and survived long enough to
// mislead a crash investigation — a frame reading "Serialize" in a citizen-death
// teardown looks like nonsense and delays the read of an otherwise clean stack.
//
// CONTRACT: `entityRef` is DEREFERENCED. Every caller must pass it. Retail's own
// caller (TradeWork_impl_3480, 0x140103480) seeds a local from *param_2 immediately
// before the call and passes its address; a one-arg call site in nomadcomponent.cpp
// left RDX holding stale garbage and faulted here on every citizen death.
//
// Original: 0x1400fd290
bool EditTool::IsSelected(uint64_t* ref) {
    // CheckPtr on self->lastEntityRef to resolve lazy/weak pointers

    // Compare: does *ref match self->lastEntityRef?
    return *ref == lastEntityRef;
}

// ============================================================
// Exe helpers for Startup and OnKey, called by name (Player/exe_bodies.h unless
// noted):
//   0x140100380 PlayerScreenRect_Entity_0380 -- entity screen bounding rectangle
//   0x140098210 PlayerScreenRect_WorldToScreen_098210 -- project a world Vector3
//               through the camera (citizen fallback, TryProjectEntityModelPos)
//   0x140066800 PlayerTrade_EditTool_66800 -- toggle page setup
//   0x1401005a0 PlayerRay_GetMouseRay_1005a0 -- camera ray from the mouse position
//   0x1400aa530 PlayerRay_TerrainMarch_aa530 -- map hit test (was a dropped-page
//               typo 0x14010a530 once; the CALL at 0x1400fd12a in EditTool_Draw)
//   0x1400a0280 PlayerEdit_EntityAtCell_a0280 -- entity at a world cell
//   0x1400fcb40 HighlightClear_Impl -- clear the selection highlight
//   0x1400fd3b0 EditTool_SelectEntity_Impl (Dialog/exe_bodies.h)
// Import slots: 0x1401528a8 Volume::Rectangle's ctor (RtVolume_Rectangle_Ctor),
// 0x140152a98 Object::Base::IsDerivedFrom (Object::IsDerivedFromNullSafe),
// 0x140152a90 Object::InternalBasePtr's ctor (placement new).
// The bodies' uint8_t returns are read as the bool the old door typedefs read.
//
// 0x1400fcc90 Entity_impl_cc90 (DESELECT: clears the selection outline + closes the
// info dialog) is deliberately NOT registered (standalone/reg_player_render.cpp):
// our 0380 screen-rect helper fails for citizens, so a live cc90 would deselect +
// clear the citizen's highlight EVERY FRAME. Its reader below is its null arm.
// ============================================================

// ============================================================
// EditTool::Startup (vtable +0x38, Slot7) — Dialog positioning near entity
// ============================================================
// Gets dialog controller via IBP at self+0x28, casts with IsDerivedFrom
// to ToolUI and PageToggleUI types. If ToolUI controller's flag at +0x40
// is false, gets entity screen rect and positions dialog near it.
//
// Original: 0x1400fcda0
// Per-frame selection-outline follow re-sync (tool_base_render.cpp). Keeps a
// moving selected entity's yellow outline glued to it as it walks. Self-gates on
// BANISHED_SELECTION_OUTLINE, so it is a no-op on the shipping default path.
extern "C" void HighlightFollow_Impl(void* self);

// Resolve the ModelComponent class type-id: prefer the raw .rdata DWORD; in the
// standalone exe that constant is null, so fall back to the live factory tree by
// name (the same reason 0x140003f60's registered reimpl finds nothing in SA).
static uint32_t ModelComponentTypeId() {
    return ModelComponent::TypeId();   // ENDGAME B.3.8 callers: the class's record
}

// Project a dynamic entity's (e.g. a citizen's) live world position to a screen point.
// Used as the reposition fallback for entities that Entity_impl_0380 can't bound (no
// static footprint / model box). Reads the origin (translation) of the entity's
// ModelComponent world Transform — ModelComponent+0x148 is a Math::Transform (0x1C),
// whose last 3 floats (+0x158/+0x15c/+0x160) are the world x/y/z — and projects it
// through the camera. Returns false (caller leaves the panel at its default position)
// if the component is missing or the point is behind the camera.
static bool TryProjectEntityModelPos(uintptr_t entityObj, float* outX, float* outY) {
    if (!entityObj) return false;

    // Scan the entity's component list (entity+0x30 = array of component IBPs,
    // entity+0x38 = count) for its ModelComponent. This mirrors 0x140003f60 but with a
    // type-id that actually resolves in standalone (0x140003f60's registered reimpl reads
    // the raw .rdata ModelComponent id, which is null in SA, so it never matches).
    const uint32_t modelTid = ModelComponentTypeId();
    if (!modelTid) return false;

    uintptr_t modelComp = 0;
    int count = *(int32_t*)(entityObj + 0x38);
    uintptr_t arr = *(uintptr_t*)(entityObj + 0x30);
    for (int i = 0; i < count; i++) {
        uint64_t* slot = (uint64_t*)(arr + (int64_t)i * 8);
        void* comp = *(void**)slot;
        if (!comp) continue;
        uint32_t tid = reinterpret_cast<Object::Base*>(comp)->Type();  // B.3.8 bridge
        if (tid == modelTid) {
            if (Object::IsDerivedFromNullSafe(comp, modelTid)) modelComp = (uintptr_t)comp;
            break;
        }
    }
    if (!modelComp) return false;

    // World position = ModelComponent transform translation (+0x148 + 0x10).
    float* pos = (float*)(modelComp + 0x158);
    float worldPos[3] = { pos[0], pos[1], pos[2] };

    uintptr_t gameWorld = (uintptr_t)GameUpdateLoop::_singleton;
    uintptr_t camHolder = *(uintptr_t*)(gameWorld + 0xe0);
    if (!camHolder) return false;

    float screen[2] = { 0.0f, 0.0f };
    if (!PlayerScreenRect_WorldToScreen_098210(camHolder, worldPos, screen)) return false;
    *outX = screen[0];
    *outY = screen[1];
    return true;
}

// ============================================================
// EditTool close-info-dialog — port of Entity_impl_cc90 (0x1400fcc90)
// ============================================================
// Retail sequence: (1) Entity_impl_cb40 clears the selection highlight (it reads
// the entity from tool+0x30, so it runs BEFORE the refs drop), (2) tool+0x28
// (dialog IBP) and tool+0x30 (entity IBP) are nulled, (3) the dialog's ToolUI
// controller pin flag (+0x40) is read and, when unpinned, the dialog is closed
// through UI::UserInterface::CloseDialog — which releases the DialogInstance and
// runs every controller's dtor slot, so the panel objects are actually destroyed
// (no stale entity context survives).
//
// DELIBERATELY NOT registered at 0x1400fcc90 — that address doubles as the
// EditTool per-frame deactivate vtable slot, which would wipe the selection
// outline every frame (see standalone/reg_player_render.cpp). Direct callers
// only: the per-frame tick's dead-entity branch in EditTool::Update.
void EditTool_CloseInfoDialog(EditTool* self, uintptr_t dialogPtr) {
    // (1) clear selection highlight (0x1400fcb40, registered unconditionally by
    // RegisterPlayerRenderBatch).
    HighlightClear_Impl(self);

    // (2) drop the tool's dialog + selected-entity refs so the tick early-outs and
    // the same footprint can be re-selected later.
    // Retail cc90 runs cb40 UNCONDITIONALLY (above), then gates everything below on
    // tool+0x28 being non-null. Returning here still clears the outline, which is
    // the whole point of the right-click path.
    if (!dialogPtr) return;

    self->entityRef = 0;
    self->lastEntityRef = 0;

    // PIN CHECK. Retail cc90 reads the dialog's ToolUI controller and closes only
    // when +0x40 (pinned) is clear; a pinned panel survives deselect. The old direct
    // caller had already done this check, so it was omitted here -- but slot 5 (tool
    // teardown) has NOT, and without it a right-click would close pinned panels too.
    {
        const uint32_t tidToolUI = ToolUI::TypeId();
        uintptr_t ctrl = 0;
        ::new (&ctrl) Object::InternalPtr<UI::DialogController>(
            reinterpret_cast<UI::DialogInstance*>(dialogPtr)->GetController(tidToolUI));  // B.3.6 bridge
        if (ctrl != 0) {
            if (Object::IsDerivedFromNullSafe((void*)ctrl, tidToolUI)) {
                if (*(char*)(ctrl + 0x40) != 0) return;   // pinned -> leave it open
            }
        }
    }

    // (3) close the dialog. The caller ran the pin check (retail cc90 re-checks it;
    // we are only reached from the unpinned branch). CloseDialog takes the
    // UserInterface (dialog+0x10) and an InternalPtr<DialogInstance>& — pass a
    // stack copy, matching the toolui.cpp close-button path.
    if (!dialogPtr) return;
    if (dialogPtr < 0x10000 || dialogPtr > 0x00007FFFFFFFFFFFULL) return;
    void* userInterface = *(void**)(dialogPtr + 0x10);
    if (!userInterface) return;
    uint64_t dialogRef = dialogPtr;
    reinterpret_cast<UI::UserInterface*>(userInterface)->CloseDialog(  // B.3.6 bridge
        reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(&dialogRef));
    // Fires on entity-death AND on right-click deselect (vtable slot 5), so this
    // must not claim a cause it cannot know.
    printf("[EditTool] deselect -> closed info dialog %p\n", (void*)dialogPtr);
    fflush(stdout);
}

// One-shot bounded reposition tracer (BANISHED_SELECT_DIAG). Startup runs every
// frame, so cap output to a handful of lines.
static bool RepoDiag() {
    if (!SelDiagOn()) return false;
    static int budget = 8;
    if (budget <= 0) return false;
    --budget;
    return true;
}
#define REPO_DIAG(...) do { if (RepoDiag()) { printf(__VA_ARGS__); fflush(stdout); } } while (0)

void EditTool::Update(void*, char) {
    // Re-sync the selection outline to the (possibly moving) selected entity every
    // frame BEFORE the dialog-positioning early-outs below — the follow reads the
    // selected-entity ref at +0x30 and is independent of the info dialog at +0x28.
    HighlightFollow_Impl(this);

    // Early out: if entity IBP at +0x28 is null, nothing to do
    if (entityRef == 0)
        return;

    void* entityIBP = (void*)&entityRef;  // &self+0x28

    // Resolve lazy/weak pointer

    // If the referenced info dialog was destroyed while this EditTool kept its
    // selection — in standalone the panel's close (ToolUI CloseDialog) does NOT tear
    // the tool down the way retail's EntityUI::Slot9 -> SetEditMode(0) does — CheckPtr
    // marks the IBP with the -1 "dead reference" sentinel (0xffffffffffffffff). The
    // code below dereferences it and calls GetController(-1,...), which faults deep in
    // the Runtime. Treat a dead / non-canonical ref as "nothing selected": clear it so
    // the per-frame tick becomes a safe no-op (line above already early-outs on 0).
    // (Retail destroys the tool on close, so the dialog is always live there and
    // this guard never triggers.)
    {
        uintptr_t resolved = *(uintptr_t*)entityIBP;
        if (resolved < 0x10000 || resolved > 0x00007FFFFFFFFFFFULL) {
            entityRef = 0;
            // Drop the selected-entity ref too: leaving +0x30 set blocks re-selecting
            // the same building (EditTool::OnKey's `lastEntityRef != foundEntity`
            // guard and d3b0's step-1 early-out both see it as still selected).
            lastEntityRef = 0;
            return;
        }
    }

    // Get ToolUI controller from dialog
    uintptr_t dialogPtr = *(uintptr_t*)entityIBP;  // deref IBP to get dialog
    uintptr_t controllerResult = 0;
    uintptr_t toolUIObj = 0;

    {
        const uint32_t tidToolUI = ToolUI::TypeId();
        ::new (&controllerResult) Object::InternalPtr<UI::DialogController>(
            reinterpret_cast<UI::DialogInstance*>(dialogPtr)->GetController(tidToolUI));  // B.3.6 bridge
        if (controllerResult != 0) {
            void* ctrObj = (void*)controllerResult;
            if (Object::IsDerivedFromNullSafe(ctrObj, tidToolUI)) {
                toolUIObj = controllerResult;
            }
        }
    }

    // Wrap in IBP (local_48 in decompilation)
    uintptr_t toolUIIBP = 0;
    new (&toolUIIBP) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(toolUIObj));   // B.3.3 bridge

    // Get PageToggleUI controller from the same dialog
    dialogPtr = *(uintptr_t*)entityIBP;
    controllerResult = 0;
    uintptr_t pageToggleObj = 0;

    {
        const uint32_t tidEntityUI = EntityUI::TypeId();
        ::new (&controllerResult) Object::InternalPtr<UI::DialogController>(
            reinterpret_cast<UI::DialogInstance*>(dialogPtr)->GetController(tidEntityUI));  // B.3.6 bridge
        if (controllerResult != 0) {
            void* ctrObj = (void*)controllerResult;
            if (Object::IsDerivedFromNullSafe(ctrObj, tidEntityUI)) {
                pageToggleObj = controllerResult;
            }
        }
    }

    // Wrap in IBP (local_50 in decompilation)
    uintptr_t pageToggleIBP = 0;
    new (&pageToggleIBP) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(pageToggleObj));   // B.3.3 bridge

    REPO_DIAG("[Repos] entityRef(dialog)=%p toolUIIBP=%p pageToggleIBP=%p\n",
              (void*)entityRef, (void*)toolUIIBP, (void*)pageToggleIBP);

    // Standalone safety: this reposition only runs once an entity's info dialog is open, a
    // path never exercised until GUI select landed. If either controller came back null (a
    // missing standalone controller, or an entity whose dialog lacks a ToolUI/EntityUI), we
    // cannot position — skip rather than deref a null IBP. The dialog still shows at its .rsc
    // default position. Retail always has both controllers.
    if (toolUIIBP == 0 || pageToggleIBP == 0) {
        REPO_DIAG("[Repos] BAIL: null controller (toolUI=%p pageToggle=%p)\n",
                  (void*)toolUIIBP, (void*)pageToggleIBP);
        return;
    }

    // Check ToolUI controller flag at +0x40
    if (*(char*)(toolUIIBP + 0x40) != 0) {
        // Flag is set — skip positioning
        REPO_DIAG("[Repos] BAIL: toolUI+0x40 pinned flag set\n");
        return;
    }

    // Retail close-on-entity-death (0x1400fcda0 -> Entity_impl_cc90): for an
    // UNPINNED info panel, when the entity can no longer produce a screen rect the
    // tool deselects and closes the dialog. In retail that fires because a dying
    // entity's model/footprint components disappear and Entity_impl_0380 fails. We
    // detect death directly — datasource weak-ref nulled or marked-for-removal
    // (entity+0x40 bit 0x400) via SA_DialogEntityAlive on the EntityUI controller —
    // BEFORE touching the entity's component array below. Without this, standalone
    // kept dead-entity panels open forever (the upgrade-path crash class the
    // SA_DialogEntityAlive wrapper guards papered over; those guards stay as the
    // backstop for PINNED panels, which retail also leaves open).
    if (!SA_DialogEntityAlive((void*)pageToggleIBP)) {
        EditTool_CloseInfoDialog(this, dialogPtr);
        return;
    }

    // Construct a Volume::Rectangle for entity bounding rect
    uint8_t rectangle[32];
    memset(rectangle, 0, sizeof(rectangle));
    RtVolume_Rectangle_Ctor(reinterpret_cast<struct Rectangle*>(rectangle));

    // Get entity reference from PageToggleUI controller at +0x30
    uintptr_t entityObj = *(uintptr_t*)(pageToggleIBP + 0x30);

    // Get entity's screen bounding rectangle
    uintptr_t entityParam = entityObj;
    bool gotRect = PlayerScreenRect_Entity_0380(entityObj, &entityParam,
                                                reinterpret_cast<uint64_t*>(rectangle)) != 0;

    REPO_DIAG("[Repos] entityObj=%p gotRect=%d rect(L,T,R,B)=[%.1f %.1f %.1f %.1f]\n",
              (void*)entityObj, (int)gotRect,
              *(float*)&rectangle[0], *(float*)&rectangle[4],
              *(float*)&rectangle[8], *(float*)&rectangle[12]);

    // Citizen / dynamic-entity fallback: 0380 only yields a rect for entities with a
    // static footprint or model bounding-box (buildings). Citizens have neither, so it
    // returns 0 and — without this — their info panel stays at the .rsc default corner.
    // Project the entity's ModelComponent world-transform origin to screen and synthesize
    // a zero-width rect centred on it, so the shared positioning math below drops the
    // panel just above the citizen. (Retail's 0380 also fails here; this is a standalone
    // QoL addition, guarded so it never runs when 0380 already produced a rect.)
    if (!gotRect) {
        float sx = 0.0f, sy = 0.0f;
        if (TryProjectEntityModelPos(entityObj, &sx, &sy)) {
            *(float*)&rectangle[0] = sx;   // left
            *(float*)&rectangle[4] = sy;   // top
            *(float*)&rectangle[8] = sx;   // right (== left -> midX = sx, centred)
            gotRect = true;
            REPO_DIAG("[Repos] citizen fallback: projected worldpos -> screen(%.1f,%.1f)\n", sx, sy);
        }
    }

    if (!gotRect) {
        // Retail deselects here (cc90) when the entity rect can't be computed.
        REPO_DIAG("[Repos] BAIL: no screen rect (0380 failed for this entity)\n");
        // PORT GAP 0x1400fcc90 Entity_impl_cc90: not registered (on purpose, see the
        // note at the top of this file); retail calls it here.
        return;
    }

    // Get dialog root element and its padded size
    dialogPtr = *(uintptr_t*)entityIBP;
    void* rootIBPPtr = nullptr;
    uintptr_t rootRetbuf = 0;  // InternalPtr<Element> return-by-value buffer
    ::new (&rootRetbuf) Object::InternalPtr<UI::Element>(
        reinterpret_cast<UI::DialogInstance*>(dialogPtr)->GetRoot());  // B.3.6 bridge
    rootIBPPtr = &rootRetbuf;


    int dialogWidth = 0, dialogHeight = 0;
    uintptr_t rootElement = rootIBPPtr ? *(uintptr_t*)rootIBPPtr : 0;
    if (rootElement) {
        reinterpret_cast<UI::Element*>(rootElement)->GetPaddedSize(dialogWidth, dialogHeight);  // B.3.6 bridge
    }

    // Calculate dialog position from entity rect.
    // The rect is a Volume::Rectangle (16 bytes) filled by fe960:
    //   +0x00 = minX (left), +0x04 = minY (top), +0x08 = maxX (right), +0x0c = maxY.
    // In the original disasm (EditTool::OnCreate @0x1400fcda0) the 16-byte rect spans two
    // stack slots: local_30 (bytes 0..7) and local_28 (bytes 8..15). It reads:
    //   local_res20     = local_30._0_4_  -> rect+0x00 (minX / left)
    //   local_res8._4_4_= local_30._4_4_  -> rect+0x04 (minY / top)
    //   (float)local_28 = rect+0x08       -> maxX / right   <-- NOT rect+0x10
    //   midX = (left + right) * 0.5
    // A prior port misread local_28 as rect+0x10 (past the 16-byte rect), which reads 0
    // and pulls every dialog toward the left edge. maxX lives at +0x08.
    float rectLeft  = *(float*)&rectangle[0];
    float rectTop   = *(float*)&rectangle[4];
    float rectRight = *(float*)&rectangle[8];

    float midX = (rectLeft + rectRight) * 0.5f;

    // Construct Vector2 for dialog position.
    // ??0Vector2@Math@@QEAA@AEBVScalar@1@0@Z  retail 0x180070db0 (was the
    // 0x1401522d8 dispatch slot)
    Math::Vector2 vec2{Math::Scalar(midX), Math::Scalar(rectTop)};

    // Position = (vec2.x - dialogWidth/2, vec2.y - dialogHeight) / uiScale
    int posX = (int)vec2.x - (dialogWidth >> 1);
    int posY = (int)vec2.y - dialogHeight;

    // Get UI scale factor from dialog's UI instance
    dialogPtr = *(uintptr_t*)entityIBP;
    // dialog+0x10 = UI instance, UI+0xdc = scale factor
    float uiScale = *(float*)(*(uintptr_t*)(dialogPtr + 0x10) + 0xdc);

    // Apply scale and set position
    int scaledX = (int)((float)posX / uiScale);
    int scaledY = (int)((float)posY / uiScale);

    // Keep the panel on-screen. The anchor projects to wherever the entity is, which can
    // be near (or past) a screen edge — most visibly for citizens, whose anchor is a
    // single projected point rather than a clamped footprint box. Clamp the upper-left to
    // [0, screen-dialog] in UI space so the whole panel stays visible. Positions already
    // inside the screen are unaffected, so building placement is unchanged.
    if (uiScale > 0.0f && ::Video::Device::_singleton) {
        uintptr_t disp = (uintptr_t)::Video::Device::_singleton;
        int screenW = *(int*)(disp + 4);
        int screenH = *(int*)(disp + 8);
        if (screenW > 0 && screenH > 0) {
            int maxX = (int)((float)screenW / uiScale) - dialogWidth;
            int maxY = (int)((float)screenH / uiScale) - dialogHeight;
            if (maxX < 0) maxX = 0;
            if (maxY < 0) maxY = 0;
            if (scaledX < 0) scaledX = 0; else if (scaledX > maxX) scaledX = maxX;
            if (scaledY < 0) scaledY = 0; else if (scaledY > maxY) scaledY = maxY;
        }
    }

    dialogPtr = *(uintptr_t*)entityIBP;
    REPO_DIAG("[Repos] SETPOS midX=%.1f rectTop=%.1f dlgW=%d dlgH=%d uiScale=%.3f "
              "-> scaled=(%d,%d)\n", midX, rectTop, dialogWidth,
              dialogHeight, uiScale, scaledX, scaledY);
    reinterpret_cast<UI::DialogInstance*>(dialogPtr)->SetUpperLeft(scaledX, scaledY, true);  // B.3.6 bridge

    // If game world flag bit 10 is set and pageToggle controller exists, call helper
    uintptr_t gameWorld = (uintptr_t)GameUpdateLoop::_singleton;
    uint32_t worldFlags = *(uint32_t*)(gameWorld + 0xd0);
    if (((worldFlags >> 10) & 1) != 0 && pageToggleIBP != 0) {
        PlayerTrade_EditTool_66800(static_cast<int64_t>(pageToggleIBP));
    }
}

// ============================================================
// EditTool::OnKey (vtable +0x30, Slot6) — Entity selection via ray picking
// ============================================================
// If param2 is true and tool is in a valid state, reads KeyMap,
// computes camera ray, does map hit test + picking scene intersection
// to find entities under cursor. Calls fn_EditTool_SelectEntity to
// select/deselect the found entity.
//
// Original: 0x1400fd030
char EditTool::OnKey(char param2) {
    if (SelDiagOn()) {
        static int calls = 0;
        if (calls < 3) { calls++;
            printf("[SelDiag] OnKey#%d param2=%d CamRay=%p MapHit=%p AtCell=%p "
                   "inputConfig=%p\n", calls, param2,
                   (void*)&PlayerRay_GetMouseRay_1005a0, (void*)&PlayerRay_TerrainMarch_aa530,
                   (void*)&PlayerEdit_EntityAtCell_a0280, (void*)Input::Configuration::_singleton);
            fflush(stdout);
        }
    }

    if (param2 == 0) return 1;

    // Check tool state: self+0x10 -> state struct
    uintptr_t statePtr = (uintptr_t)dialogPtr;
    bool flag80 = *(char*)(statePtr + 0x80) != 0;
    if (flag80 && *(uintptr_t*)(statePtr + 0x90) == 0) {
        return 1;
    }

    // The KeyMap: Input::Configuration::_singleton->_keyMap (retail's triple
    // deref of the import slot 0x140151738).
    if (!Input::Configuration::_singleton) return 1;
    uintptr_t keyMap = reinterpret_cast<uintptr_t>(Input::Configuration::_singleton->_keyMap);

    // Check KeyMap::Pressed(keyMap, 0, 5)
    bool pressed = reinterpret_cast<const Input::KeyMap*>(keyMap)->Pressed(0, 5);  // B.3.7 bridge
    if (SelDiagOn() && pressed)
        printf("[SelDiag] action-5 PRESSED (keyMap=%p) — running pick\n", (void*)keyMap);
    if (!pressed) return 1;

    // Get world data pointers
    uintptr_t gameWorld = (uintptr_t)GameUpdateLoop::_singleton;
    uintptr_t entityList = *(uintptr_t*)(gameWorld + 0xf8);

    // Compute camera ray from mouse position
    // rayOrigin = 12 bytes (Vector3), rayDir = 12 bytes (Vector3)
    float rayOrigin[3] = {0.0f, 0.0f, 0.0f};
    float rayDir[3] = {0.0f, 0.0f, 0.0f};
    // THE PICK RANGE CAP. Retail stamps this 400.0f BEFORE the map hit test
    // (0x1400fd0e5: MOV dword ptr [RBP + 0x6f],0x43c80000) and then hands the SAME stack
    // slot to Picking::Scene::Intersect as its R9 max-distance (0x1400fd1c6: LEA R9,
    // [RBP + 0x6f]) with NO re-initialization in between — the only writes in that window
    // are the hitPoint/hitNormal buffers (RBP+0x17/0x27) and foundEntity (RBP+0x7f).
    // So it is one in/out variable: the terrain hit shortens it, and the picking pass is
    // then capped at the terrain distance. That is what stops the ray selecting a mesh
    // BEHIND the ground.
    // This port had it backwards in both directions — it started at 0.0f and was then
    // reset to 400.0f just before the picking call, which threw the terrain distance away.
    float maxDist = 400.0f;   // 0x43c80000

    PlayerRay_GetMouseRay_1005a0(reinterpret_cast<void*>(keyMap), rayOrigin, rayDir);

    // Map hit test to find world position under cursor
    // MUST be 4 floats, matching retail EditTool::Draw's `float local_58[4]` (0x1400fd030).
    // MapHitTest's callee (retail 0x1400aaa20) writes a full Vector3 through this out-param
    // on EVERY call, hit or miss — its tail is
    // `MOVSS [RCX],XMM1 / [RCX+4],XMM0 / [RCX+8],XMM2` at 0x1400aad2a-33. On a MISS t == 0,
    // so it writes the RAY ORIGIN, i.e. the camera position.
    // This was declared `float hitPos[2]` with the slack put in a SEPARATE `hitDistBuf[2]`
    // that nothing read — so the optimiser deleted it and the third float landed on
    // `foundEntity` (frame: 0x38 = hitPos[0..1], 0x40 = foundEntity). The camera Z then
    // travelled into EditTool_SelectEntity as an entity pointer and crashed at
    // edittool_select.cpp:189 (observed 0x4167CF1B = 14.4877, and 0x4167EB1A on 2026-07-26 —
    // the same camera, slightly different zoom, which is why the two values are within 0.006).
    // Reaches SelectEntity only when the terrain ray MISSES (so the EntityAtCell branch that
    // would overwrite the slot is skipped) and the picking pass also misses.
    float hitPos[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    uintptr_t foundEntity = 0;

    uintptr_t mapComponent = *(uintptr_t*)(gameWorld + 0x110);
    int* mapSize = *(int**)(gameWorld + 0xf8);

    if (mapComponent) {
        float maxDistArr[2];
        maxDistArr[0] = 400.0f;  // 0x43c80000
        maxDistArr[1] = 0.0f;

        bool mapHit = PlayerRay_TerrainMarch_aa530(mapComponent, rayOrigin, rayDir,
                                                   maxDistArr, hitPos, &maxDist) != 0;

        if (mapHit) {
            int gridSize = *mapSize;
            int halfGrid = gridSize >> 1;

            // Convert world position to grid cell
            int cellX = (int)(hitPos[0] + (float)halfGrid);
            int cellY = (int)(hitPos[1] + (float)halfGrid);

            // Clamp to grid bounds
            if (cellX < 0) cellX = 0;
            if (cellX >= gridSize) cellX = gridSize - 1;
            if (cellY < 0) cellY = 0;
            if (cellY >= gridSize) cellY = gridSize - 1;

            int cellIndex = gridSize * cellY + cellX;

            if (cellIndex > 0) {
                uintptr_t retIBP = 0;
                void* result = PlayerEdit_EntityAtCell_a0280(entityList, &retIBP, cellIndex);
                if (result) foundEntity = *(uintptr_t*)result;
            }
            if (SelDiagOn())
                printf("[SelDiag] mapHit=1 hitPos=(%.2f,%.2f) cell=(%d,%d) cellEntity=%p\n",
                       hitPos[0], hitPos[1], cellX, cellY, (void*)foundEntity);
        } else if (SelDiagOn()) {
            printf("[SelDiag] mapHit=0 (ray missed terrain) ray o=(%.2f,%.2f,%.2f) d=(%.2f,%.2f,%.2f)\n",
                   rayOrigin[0], rayOrigin[1], rayOrigin[2], rayDir[0], rayDir[1], rayDir[2]);
        }
    } else if (SelDiagOn()) {
        printf("[SelDiag] MapHitTest skipped (MapHit=%p mapComp=%p)\n",
               (void*)&PlayerRay_TerrainMarch_aa530, (void*)mapComponent);
    }

    // Picking scene intersection (3D ray picking for precise entity selection)
    void* pickingScene = *(void**)(gameWorld + 0x70);
    float hitNormal[3] = {0.0f, 0.0f, 0.0f};
    float hitPoint[3] = {0.0f, 0.0f, 0.0f};

    if (pickingScene) {
        // NO reset here — retail deliberately carries the map hit test's shortened
        // distance into the pick (see the maxDist declaration). Re-stamping 400.0f is
        // what let the pick reach through terrain.
        int hitId = reinterpret_cast<const Picking::Scene*>(pickingScene)->Intersect(   // B.3.7 bridge
            *reinterpret_cast<const Vector3*>(rayOrigin), *reinterpret_cast<const Vector3*>(rayDir),
            *reinterpret_cast<const Math::Scalar*>(&maxDist),
            *reinterpret_cast<Vector3*>(hitPoint), *reinterpret_cast<Vector3*>(hitNormal));
        if (hitId >= 0) {
            // Get the entity associated with this picking hit
            void* userData = reinterpret_cast<const Picking::Scene*>(pickingScene)->GetUserData(static_cast<uint32_t>(hitId));   // B.3.7 bridge
            uintptr_t pickIBP = 0;
            new (&pickIBP) ::Object::InternalBasePtr(
                reinterpret_cast<RtObject::Internal*>(userData));   // B.3.3 bridge
            foundEntity = pickIBP;
        }
        if (SelDiagOn())
            printf("[SelDiag] Picking hitId=%d -> foundEntity=%p\n", hitId, (void*)foundEntity);
    } else if (SelDiagOn()) {
        printf("[SelDiag] Picking skipped (scene=%p)\n", pickingScene);
    }

    // Select the found entity if valid and different from current
    if (foundEntity != 0) {
        // If flag80 is set, verify entity matches expected type
        if (flag80) {
            // CORRECTED 2026-08-01. This used to blame "a mesh registered without
            // Picking::Scene::SetMeshUserData", citing foundEntity=0x4167eb1a from
            // 2026-07-26. That was WRONG and cost a second investigation: the float bit
            // pattern was the CAMERA POSITION, written past the end of the 8-byte `hitPos`
            // buffer by MapHitTest's 12-byte Vector3 store (see the hitPos declaration
            // above, now float[4] as retail has it). Picking was never at fault — the
            // exports on that path were re-verified against the Runtime DLL and all match.
            // Kept as defence in depth only. NOTE it did not fire on the 2026-08-01 repeat:
            // flag80 was 0, so this whole branch was skipped and the corrupt value went
            // straight through to SelectEntity.
            if (foundEntity < 0x10000 || (foundEntity & 0x7) != 0) {
                static int s_warned = 0;
                if (s_warned < 4) {
                    s_warned++;
                    fprintf(stderr, "[EditTool] WARN: picking user data %p is not a valid entity "
                            "pointer — skipping select. A mesh was registered without "
                            "Picking::Scene::SetMeshUserData.\n", (void*)foundEntity);
                    fflush(stderr);
                }
                return 1;
            }
            uintptr_t entityTypeRef = *(uintptr_t*)(foundEntity + 0x28);
            uintptr_t expectedType = *(uintptr_t*)((uintptr_t)dialogPtr + 0x90);
            if (entityTypeRef != expectedType) {
                return 1;  // type mismatch, skip
            }
        }

        // Check if entity is different from last selected
        if (lastEntityRef != foundEntity) {
            // Call EditTool_SelectEntity (0x1400fd3b0)
            uintptr_t entityParam = foundEntity;
            EditTool_SelectEntity_Impl(this, &entityParam);
            if (SelDiagOn())
                printf("[SelDiag] SELECT entity=%p -> dialog(self+0x28)=%p\n",
                       (void*)foundEntity, (void*)entityRef);
        }
    } else if (SelDiagOn()) {
        printf("[SelDiag] no entity under cursor — nothing selected\n");
    }

    return 1;
}

// ============================================================
// EditTool members (EditTool.h) — ENDGAME B.3.8 TOOL.
// The hand-built g_EditTool_Vtable[13], its slot stubs, the _SA factories and
// RegisterEditToolVtable are gone: the compiler builds the table, and nothing
// reads 0x140161ca0 back by address (the old dtor was the only reader).
// Inherited from Tool, as in retail: [11] no setting (0x140004950 `*ret = 0`:
// EditTool has no ToolSetting; SetEditMode dereferences the returned slot).
// ============================================================
#include "Object/factory_registration.h"

// [5] 0x1400fd2e0 is a JMP thunk straight to 0x1400fcc90 (deselect + close the
// info dialog). SetEditMode's teardown (the only slot-5 caller) invokes it, and
// that is HOW retail clears the selection outline AND closes the panel when the
// player right-clicks. NOT the same as registering 0x1400fcc90 by address: that
// address also backs a per-frame deactivate slot on a DIFFERENT table
// (0x1401ee710), and registering it made cc90 run every frame
// (reg_player_render.cpp records the measurement). Retail reads the dialog IBP
// from +0x28 itself.
void EditTool::Shutdown() { EditTool_CloseInfoDialog(this, static_cast<uintptr_t>(entityRef)); }

// [6] OnKey (0x1400fd030), [7] Update (0x1400fcda0) and [10] IsSelected
// (0x1400fd290: "is *ref the entity I have selected?" — callers MUST pass the
// second argument; a one-arg call site once left RDX stale and faulted inside it,
// nomadcomponent.cpp) are defined above.

// Cursor-free select harness (BANISHED_TOOL_HARNESS=select). The live pick needs the OS
// cursor over an entity, which the headless settle-frame lacks. This mirrors OnKey's
// cell-fallback: scan the region map around the start-town centre via EntityAtCell and
// drive fn_EditTool_SelectEntity on each occupant until one opens an info dialog
// (editTool+0x28 goes non-null). Entities without a UIDescription (trees/rocks) safely
// no-op, so the first building/citizen hit is what opens. Returns 1 if a dialog opened.
// Drive the select path on ONE specific entity (a definitive UIDescription-bearing building
// from the founding-house capture). Returns 1 if an info dialog opened (editTool+0x28 set).
// Is `tool` really an EditTool? EntityUI+0x28 holds whatever tool is CURRENTLY active
// — after any build/placement tool has been activated (SetEditMode with a template)
// that is a DragTool-derived object, and EditTool's fields alias its fields: EditTool
// +0x28/+0x30 (dialog IBP / selected-entity IBP) sit exactly on DragTool's dragState
// and endCell/lastEndCell int pair. Driving the select path on one of those reads the
// two cell ints as an entity pointer — measured 2026-08-19 with the pasture harness:
// FencedTool+0x30 = 0xffffffff00008263 (endCell 0x8263, lastEndCell -1), which
// Entity_impl_cb40 handed straight to the 0x140004740 component finder and faulted on
// entity+0x38. So the answer decides whether this pointer may be used as an EditTool.
extern "C" bool EditTool_IsEditTool(void* tool) {
    if (!tool) return false;
    const uint32_t tid = EditTool::TypeId();   // B38 D4: the class static, not a lazy cache
    if (tid == 0 || tid == 0xFFFFFFFFu) return false;
    return Object::IsDerivedFromNullSafe(tool, tid);
}

// Get an EditTool to drive a panel with, the way a player does: if the active tool is
// something else (a build tool the harness or the player left armed), switch back to
// edit mode first — SetEditMode(pc, 0) destroys that tool and installs an EditTool —
// rather than reinterpreting it. Returns null if no EditTool could be activated.
extern "C" void* EditTool_HarnessAcquire(void* entityUI) {
    if (!entityUI) return nullptr;
    void* tool = *(void**)((uintptr_t)entityUI + 0x28);
    if (EditTool_IsEditTool(tool)) return tool;
    if (tool) {
        static int warned = 0;
        if (warned < 4) {
            ++warned;
            printf("[EditSel] active tool %p is not an EditTool — switching to edit mode "
                   "(SetEditMode 0) before touching a panel\n", tool);
            fflush(stdout);
        }
    }
    EntityUI_SetEditMode_Impl(entityUI, 0);
    tool = *(void**)((uintptr_t)entityUI + 0x28);
    return EditTool_IsEditTool(tool) ? tool : nullptr;
}

extern "C" int EditTool_HarnessSelectEntity(void* editTool, void* entity) {
    if (!editTool || !entity) return 0;
    // Refuse a non-EditTool outright: d3b0 would write the dialog + entity refs into
    // whatever object this is, and Entity_impl_cb40 would deref its +0x30 as an entity.
    if (!EditTool_IsEditTool(editTool)) {
        static int warned = 0;
        if (warned < 4) {
            ++warned;
            printf("[EditSel] REFUSED: %p is not an EditTool (its +0x30 is not a selected "
                   "entity) — call EditTool_HarnessAcquire first\n", editTool);
            fflush(stdout);
        }
        return 0;
    }
    // Clear any prior selection ref so d3b0 doesn't early-out on a repeat.
    uintptr_t entityParam = (uintptr_t)entity;
    EditTool_SelectEntity_Impl(editTool, &entityParam);
    void* dlg = *(void**)((uintptr_t)editTool + 0x28);
    printf("[EditSel] direct select entity=%p -> dialog=%p (%s)\n",
           entity, dlg, dlg ? "OPENED" : "no dialog");
    return dlg != 0 ? 1 : 0;
}

// Dump a dialog's element tree with geometry + flags so we can see WHY injected
// content isn't drawing (zero size? off-screen? visibility flag?). Element layout:
//   +0x10 name (System::String) · +0x34 flags · +0x38..0x58 rect fields
//   +0xa0 children array · +0xa8 child count · vtable[2]=GetTypeId
static void DumpCitizenElemTree(void* elem, int depth, int* counter) {
    if (!elem || depth > 10 || *counter > 200) return;
    (*counter)++;
    uintptr_t e = (uintptr_t)elem;
    char* nameData = *(char**)(e + 0x10);
    int len = nameData ? *(int*)(nameData - 8) : 0;
    uint32_t tid = 0;
    tid = reinterpret_cast<Object::Base*>(elem)->Type();   // B.3.x bridge (slot 2)
    uint32_t flags = *(uint32_t*)(e + 0x34);
    // Element bounds: +0x68 x, +0x6c y, +0x70 w, +0x74 h (ints) — from
    // TabElement::UpdateContainers geometry math.
    int bx = *(int*)(e + 0x68), by = *(int*)(e + 0x6c);
    int bw = *(int*)(e + 0x70), bh = *(int*)(e + 0x74);
    printf("[CitTree] %*selem=%p tid=%u flags=0x%x name='%.*s' | rect=(%d,%d %dx%d)\n",
           depth * 2, "", elem, tid, flags, len > 0 ? len : 0, nameData ? nameData : "",
           bx, by, bw, bh);
    // These locals exist only for the trace line above; BANISHED_STRIP_LOGS
    // turns printf into ((void)0) in release, which otherwise makes every one
    // of them -Wunused (standalone/log_strip.h's own documented caveat).
    (void)tid; (void)flags; (void)len; (void)bx; (void)by; (void)bw; (void)bh;
    void** kids = *(void***)(e + 0xa0);
    int kc = *(int*)(e + 0xa8);
    for (int i = 0; kids && i < kc; i++) DumpCitizenElemTree(kids[i], depth + 1, counter);
}

// Drive the select path on a CITIZEN. Citizens aren't in the region-map cell array
// (they move) — they register in the Picking::Scene via PickingComponent OnAdded /
// the move-tick (see citizenai_executor.cpp RegisterCitizenPickMesh). Their pick
// userData is the entity backref we can recover with Picking::Scene::GetUserData.
// This bypasses the cursor ray (headless harness) but exercises the exact chain a
// real click uses: pick userData -> EditTool_SelectEntity -> CitizenUI dialog.
extern "C" int EditTool_HarnessSelectCitizen(void* editTool) {
    if (!editTool) { printf("[EditSel] no active EditTool\n"); return 0; }
    void* scene = *(void**)((uintptr_t)GameUpdateLoop::_singleton + 0x70);
    if (!scene) { printf("[EditSel] picking scene null\n"); return 0; }

    // Citizens register with low pick-ids (0..N). Probe the first several; the first
    // that yields a live userData entity gets selected.
    for (int id = 0; id < 16; id++) {
        void* userData = reinterpret_cast<const Picking::Scene*>(scene)->GetUserData(static_cast<uint32_t>(id));   // B.3.7 bridge
        if (!userData) continue;
        printf("[EditSel] citizen pick-id %d -> entity=%p; selecting...\n", id, userData);
        uintptr_t entityParam = (uintptr_t)userData;
        EditTool_SelectEntity_Impl(editTool, &entityParam);
        void* dlg = *(void**)((uintptr_t)editTool + 0x28);
        printf("[EditSel] citizen select entity=%p -> dialog=%p (%s)\n",
               userData, dlg, dlg ? "OPENED" : "no dialog");
        if (dlg) {
            return 1;
        }
    }
    printf("[EditSel] no citizen pick userData found in ids 0..15\n");
    return 0;
}

extern "C" int EditTool_HarnessSelectNearby(void* editTool) {
    if (!editTool) { printf("[EditSel] no active EditTool\n"); return 0; }

    uintptr_t gameWorld = (uintptr_t)GameUpdateLoop::_singleton;
    uintptr_t entityList = *(uintptr_t*)(gameWorld + 0xf8);
    int* mapSize = *(int**)(gameWorld + 0xf8);
    int gridSize = (mapSize && *mapSize > 0) ? *mapSize : 256;
    int tried = 0, found = 0;

    // Expanding Chebyshev rings around centre (124,124), same valley the placement/
    // removal harnesses target.
    for (int ring = 0; ring <= 16; ring++) {
        for (int dy = -ring; dy <= ring; dy++) {
            for (int dx = -ring; dx <= ring; dx++) {
                if (ring > 0 && dx > -ring && dx < ring && dy > -ring && dy < ring)
                    continue;  // ring perimeter only
                int cx = 124 + dx, cy = 124 + dy;
                if (cx < 0 || cx >= gridSize || cy < 0 || cy >= gridSize) continue;
                int cellIndex = gridSize * cy + cx;
                if (cellIndex <= 0) continue;

                uintptr_t retIBP = 0;
                void* result = PlayerEdit_EntityAtCell_a0280(entityList, &retIBP, cellIndex);
                uintptr_t entity = result ? *(uintptr_t*)result : 0;
                if (!entity) continue;
                found++;

                uintptr_t entityParam = entity;
                EditTool_SelectEntity_Impl(editTool, &entityParam);
                tried++;
                void* dlg = *(void**)((uintptr_t)editTool + 0x28);
                if (dlg != 0) {
                    printf("[EditSel] cell(%d,%d) entity=%p OPENED info dialog=%p "
                           "(tried=%d occupants=%d)\n", cx, cy, (void*)entity, dlg, tried, found);
                    return 1;
                }
            }
        }
    }
    printf("[EditSel] no dialog opened after scanning rings (occupants=%d tried=%d) — "
           "entities may lack a UIDescription\n", found, tried);
    return 0;
}
// create 0x14002daa0: MemAlloc(0x38, 8), then the ctor 0x1400fd340.
void* EditTool::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(EditTool), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) EditTool());
}
// createInPlace 0x14002dae0: `if (storage) ctor(storage)`.
void* EditTool::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) EditTool());
}

DEFINE_EXE_TYPE(EditTool, "Tool")
