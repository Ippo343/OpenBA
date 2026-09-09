#pragma once
// ---------------------------------------------------------------------------
// standalone/mouse_inject.h — the hover / tooltip / drag-drop route
// ---------------------------------------------------------------------------
// Wave 10D lane MOUSE. See docs/runtime_port/WAVE10D_LANE_MOUSE.md.
//
// OFF unless BANISHED_MOUSE_INJECT is set. Prints nothing when unset.
//
// Two seams, both in the caller of UI::UserInterface::Update, because the
// injector must write the input bytes AFTER the frame's input poll and BEFORE
// the UI reads them, and must read the element flags back AFTER the UI has run:
//
//     MouseInject::PreUpdate(ui);     // place the cursor
//     fn_UI_Update(ui, time);         // the game hovers, for real
//     MouseInject::PostUpdate(ui);    // did it agree?
//
// Both are no-ops — a single cached-getenv test — when the switch is unset.
namespace MouseInject {

// True once BANISHED_MOUSE_INJECT names a script. Cheap; safe to call per frame.
bool Enabled();

void PreUpdate(void* ui);
void PostUpdate(void* ui);

}  // namespace MouseInject
