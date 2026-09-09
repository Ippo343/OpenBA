#pragma once
// Wave 10D lane MOUSE. Opt-in detours that put the three ported hover bodies
// (EndDragDrop, EndHover, BeginToolTip) on the DLL's OWN Element::Update path.
// See standalone/ui_hover_detour.cpp for why registration alone cannot do it.
// Off, silent and non-patching unless BANISHED_RT_UIHOVER_DETOUR is set.
extern "C" void RtUiHoverInstallDetours();
