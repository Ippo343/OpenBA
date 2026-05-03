#pragma once
// code/Game/Game/menudialogloopt_funcs.h
// Function declarations for the MenuDialogLoopT (Engine::DialogLoopT subclass
// that drives the main menu) deep reimplementation.
//
// Shared between standalone and DLL modes. Currently only used by standalone
// (engine/reimpl/ has no MenuDialogLoopT wrapper yet — Phase A is standalone-only).

#include <cstdint>

// MenuDialogLoopT_LoadScene_PhaseA — mirror the 3D-scene block of the original
// MenuDLT::Load (exe 0x14009A930), up to and including the three Scene::AddGlobalObject
// calls and the helper_a3a0/+0xb7c/+0xb80 transform copy. Skips:
//   - Audio/Music/Menu.rsc load (audio music ext stored at self+0xb88).
//   - TradeWork (gameSettings[0x16] gated DLC).
//   - Static-mesh array iteration (Phase B).
//   - Particle-system iteration (Phase C).
//   - The exe-side vtable overrides (PTR_FUN_14015e290, _vslot10_14015e240).
//
// Caller must already have invoked Engine::DialogLoopT::Load (which populates
// self+0x30 with the dialog External). On entry self+0xb90 is expected to be
// nullptr; on success it holds the loaded MenuScene External.
//
// Returns true on success (scene allocated and added). Returns false if the
// MenuScene resource fails to load (no scene background — UI-only menu).
bool MenuDialogLoopT_LoadScene_PhaseA(void* self);

// MenuDialogLoopT_LoadStaticMeshes_PhaseB — mirror the static-mesh loop of
// MenuDLT::Load (exe 0x14009A930). For each element of MenuScene+0x30 (count =
// MenuScene+0x38), allocate a Graphics::MeshInstance, set its mesh + transform
// + material list, and add it to the global scene via Scene::AddObject. Result
// is the props/buildings/foliage of the menu background.
//
// menuScene is the loaded MenuScene External (self+0xb90).
// scene     is the Graphics::Scene the meshes get added to (self+0xba0).
// No-op if either pointer is null or required fn pointers are missing.
void MenuDialogLoopT_LoadStaticMeshes_PhaseB(void* self, void* menuScene, void* scene);

// MenuDialogLoopT_LoadParticles_PhaseC — mirror the particle loop of
// MenuDLT::Load (exe 0x14009A930). For each element of MenuScene+0x50 (count =
// MenuScene+0x58), allocate a Graphics::ParticleSystemInstance, configure it
// via the equivalent of MenuScene_helper_3ca0 (vtable+0x48 SetParticleSystem,
// SetTransform from element yaw+offset, Update with t=0), and add it to both
// the all-instance array (+0xbe0) and the particle-only array (+0xbd0). Per-
// frame Update iterates +0xbd0 — that's what makes smokestack chimneys emit
// smoke.
//
// menuScene is the loaded MenuScene External (self+0xb90).
// scene     is the Graphics::Scene the particles get added to (self+0xba0).
// No-op if either pointer is null or required fn pointers are missing.
void MenuDialogLoopT_LoadParticles_PhaseC(void* self, void* menuScene, void* scene);

// MenuDialogLoopT_TickParticles — per-frame Update on each entry of the
// particle-only array at MenuDLT+0xbd0. Calls vtable+0x08 (Update(Time*)) on
// each particle so emitters spawn/age sprites. Caller is the per-frame
// Stub_Update path after the camera derivation step.
void MenuDialogLoopT_TickParticles(void* self, void* time);

// MenuDialogLoopT_DrawScene_PhaseA — mirror the 3D `if (self+0xb90)` block of
// MenuDLT::Draw (exe 0x14009b440). Caller invokes this BEFORE UI::Draw.
//
// alpha is the frame-blend value passed to Renderer::Draw.
// context is the live Video::Context (engine_singleton+0x10).
//
// No-op if self+0xb90 (the MenuScene External) is null.
void MenuDialogLoopT_DrawScene_PhaseA(void* self, float alpha, void* context);

// MenuDialogLoopT_UpdateCamera_PhaseA — mirror the camera-animation block of
// MenuDLT::Update (exe 0x14009b6e0). Animates the orbit yaw, derives the
// camera eye from the deserialized lookAtTarget/yaw/pitch/distance fields,
// rebuilds the Math::Transform via FromLookAt, converts to Transform3 and
// applies to the embedded Camera via SetCameraToWorld.
//
// Without this, the Camera's view matrix stays at identity (set by the
// Camera ctor) regardless of what was deserialized — so the renderer projects
// terrain through an identity camera at world origin and the menu is black.
//
// Caller invokes this each tick (Engine::DialogLoopT::Update path), passing
// the frame's deltaSeconds (time-as-seconds — see System::Time::AsSeconds).
//
// No-op if self+0xb90 (MenuScene) or self+0xbb8 (Transform) are null.
void MenuDialogLoopT_UpdateCamera_PhaseA(void* self, float deltaSeconds);
