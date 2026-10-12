// ============================================================================
// src/ext/eye_camera/eye_camera.h -- ext.eye_camera (class A, default OFF)
// ============================================================================
// NOT RETAIL. "Eye-level camera": while the player follows a citizen through
// retail's own follow (the citizen panel's Follow button -> FollowDialog), the
// camera sits at the citizen's eyes, or a short way behind and above the head,
// looking along the citizen's heading. Position, heading and distance are
// smoothed; the mouse wheel / zoom keys move the eye between first person (0)
// and kMaxDistance. Ending the follow (the dialog's close button, Escape, the
// citizen panel closing) gives retail's camera back exactly where
// PlayerController's target / yaw / pitch / distance put it: this code never
// writes them (they are serialized, PlayerController::Serialize 0x1400988c0 and
// PlayerToolManager::Serialize 0x140102920).
//
// The three seams, each selected at the caller with the toggle read once:
//   GameUpdateLoop::Update step 6   if (Ext::Ext().eye_camera && EyeCamera::Active())
//                                       EyeCamera::Apply(*playerCtrl);
//                                   else retail's CalcTransform / AsTransform3 /
//                                       SetCameraToWorld
//   GameUpdateLoop::Draw step 4     ... EyeCamera::ApplyFrame(pc, alpha, fovY, far)
//                                   else retail's UpdateCamera(40, 2, 512)
//   PlayerToolManager::UpdateCamera ... zoomDir = EyeCamera::Zoom(zoomDir)
//
// Active() is the whole engagement test: retail's follow flag (PlayerToolManager
// +0x38, set by FollowDialog::OnOpened, cleared by OnClosed), an open
// FollowDialog in the UI's dialog list, and its citizen's entity still alive
// through a weak reference (Object::InternalFactory slot + generation) and not
// marked for removal. A removed citizen gives retail's follow back, and
// nothing else does while the dialog is open: an idle citizen keeps its last
// heading, a hidden one (model out of the scene: inside a building) keeps the
// last eye transform (or, engaged while inside, the door it went in by). The
// weak references follow InternalFactory's swap-remove (EYECAM.md §9).
// ============================================================================
#pragma once

class PlayerController;   // Game/PlayerController.h

namespace EyeCamera {

// True while the extension should own the camera (see the banner). Cheap: one
// flag, one walk of the open dialogs, one weak-reference test. Call it only
// when Ext::Ext().eye_camera is on.
bool Active();

// GameUpdateLoop::Update step 6 (once per update tick). Runs retail's
// PlayerController::CalcTransform for its +0x00 store (a serialized field,
// kept exactly retail's) and then puts the last eye transform on the camera,
// so the tick's later readers (picking, the audio listener) see the eye.
void Apply(PlayerController& pc);

// GameUpdateLoop::Draw step 4 (once per rendered frame): advance the
// smoothing by the real frame time, build the eye from the citizen model's
// transform interpolated at `alpha` (the renderer's own), set it on the
// camera, and rebuild the projection with the near plane kNearPlane.
void ApplyFrame(PlayerController& pc, float alpha, float fovY, float farZ);

// PlayerToolManager::UpdateCamera's zoom input: moves the eye's distance and
// returns 0, so retail's zoom target (PlayerToolManager +0x3C) is untouched.
int Zoom(int zoomDir);

}  // namespace EyeCamera
