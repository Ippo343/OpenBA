// ============================================================================
// src/ext/eye_camera/eye_camera.cpp -- ext.eye_camera: the engine glue
// ============================================================================
// NOT RETAIL. eye_camera.h has the contract and the seams; eye_math.h the
// conventions and the constants. Nothing here writes a serialized field: the
// only engine state touched is the Graphics::Camera member of PlayerController
// (+0x34, not serialized) through SetCameraToWorld / UpdateCamera, the same two
// writes retail's own seams make. Retail's CalcTransform still runs at step 6
// for its +0x00 store.
// ============================================================================

#include "ext/eye_camera/eye_camera.h"
#include "ext/eye_camera/eye_math.h"

#include <chrono>
#include <cstdint>

#include "Component/CharacterComponent.h"
#include "Component/CharacterDescription.h"
#include "Component/CitizenComponent.h"
#include "Component/ModelComponent.h"
#include "Dialog/FollowDialog.h"
#include "Entity/ComponentEntity.h"
#include "Game/GameUpdateLoop.h"
#include "Game/Map.h"
#include "Game/PlayerController.h"
#include "Game/PlayerToolManager.h"
#include "graphics/InterpolatedState.h"
#include "math/Math.h"
#include "object/Internal.h"
#include "ui/DialogInstance.h"
#include "ui/UserInterface.h"

namespace EyeCamera {

namespace {

// ---------------------------------------------------------------------------
// A weak reference to an Object::Internal (R1's lifetime rules: the follow
// dialog keeps only a raw CitizenComponent*, and our dialogs hold no strong
// data-source reference). Alive when the factory's slot `index` still holds
// this very object with the generation it had when the reference was taken:
// ~Internal swap-removes the slot, so a destroyed object is never found there,
// and the pointer is dereferenced only after the slot names it.
// ---------------------------------------------------------------------------
template <class T>
struct Weak {
    T*       ptr        = nullptr;
    int32_t  index      = -1;
    uint32_t generation = 0;
};

template <class T>
Weak<T> MakeWeak(T* p) {
    Weak<T> w;
    if (p != nullptr) {
        w.ptr = p;
        w.index = p->_factoryIndex;
        w.generation = p->_generation;
    }
    return w;
}

template <class T>
T* Resolve(const Weak<T>& w) {
    if (w.ptr == nullptr) return nullptr;
    const ::Object::InternalFactory* factory = ::Object::InternalFactory::_singleton;
    const Collection::Array<::Object::Internal*>& objects = factory->_objects;
    if (w.index < 0 || w.index >= objects._size) return nullptr;
    const ::Object::Internal* slot = objects._data[w.index];
    if (slot != w.ptr) return nullptr;
    if (w.ptr->_generation != w.generation) return nullptr;
    return w.ptr;
}

// ---------------------------------------------------------------------------
// The state. None of it is serialized (class A).
// ---------------------------------------------------------------------------
Weak<FollowDialog>    g_dialog;          // the follow we latched
Weak<ComponentEntity> g_entity;          // its citizen's entity, latched with it
Smooth                g_smooth;          // the smoothed feet / heading / distance
float                 g_distance = kDefaultDistance;   // the wheel's target (kept across follows)
float                 g_scale = 1.0f;    // the followed model's age scale (last sample)
Pose                  g_pose;            // the last eye transform built
bool                  g_havePose = false;
std::chrono::steady_clock::time_point g_lastFrame;
bool                  g_haveLastFrame = false;

void Disengage() {
    g_dialog = Weak<FollowDialog>();
    g_entity = Weak<ComponentEntity>();
    g_smooth.valid = false;
    g_havePose = false;
    g_haveLastFrame = false;
}

bool IsRemoved(const UI::UserInterface* ui, const UI::DialogInstance* dialog) {
    const Collection::Array<UI::DialogInstancePtr>& removed = ui->_removedDialogs;
    for (int32_t i = 0; i < removed._size; ++i)
        if (removed._data[i] == dialog) return true;
    return false;
}

// The frontmost open FollowDialog. The UI updates the list back to front, so
// when two citizen panels both follow, the frontmost one's OnUpdate writes
// retail's target last: follow the same citizen retail's camera follows.
FollowDialog* FindFollowDialog(const UI::UserInterface* ui) {
    for (auto* node = ui->_dialogs._tail; node != nullptr; node = node->prev) {
        UI::DialogInstance* dialog = node->element;
        if (dialog == nullptr || !dialog->IsA(FollowDialog::TypeId())) continue;
        if (IsRemoved(ui, dialog)) continue;
        return static_cast<FollowDialog*>(dialog);
    }
    return nullptr;
}

// The citizen's feet and heading from its model, interpolated at `alpha` the
// way the renderer interpolates it (Geometry::_transform, prev -> current),
// and the model's age scale. False when the model is out of the scene
// (hidden: inside a building).
bool SampleCitizen(const ComponentEntity* entity, float alpha, Vec3& feet, float& heading,
                   float& scale) {
    const ModelComponent* model = entity->FindComponent<ModelComponent>();
    if (model == nullptr || model->modelId < 0) return false;
    const Graphics::InterpolatedState<Math::Transform>& xf = model->meshInstance._transform;
    const Math::Transform t = xf._previous.Lerp(xf._current, Math::Scalar(alpha));
    feet = Vec3{t.translation.x, t.translation.y, t.translation.z};
    // The age scale SetupYoung / SetupAdult / SetupElder wrote (eye_math.h,
    // CONVENTIONS): uniform, so the z component is the height factor.
    scale = model->meshInstance._color[2];
    // Model yaw = heading + CharacterDescription::_f24 (the move tick's offset).
    float offset = 0.0f;
    if (const CharacterComponent* ch = entity->FindComponent<CharacterComponent>())
        if (ch->characterDescription != nullptr) offset = ch->characterDescription->_f24;
    heading = WrapDegrees(ModelYawFromQuaternion(t.rotation.z, t.rotation.w) - offset);
    return true;
}

// Keep the eye above the terrain when it backs off over a slope.
void ClampToTerrain(Pose& pose) {
    Map* map = GameUpdateLoop::_singleton->m_game;
    const float in[2] = {pose.eye.x, pose.eye.y};
    float out[3] = {0.0f, 0.0f, 0.0f};
    map->GetTerrainPos(out, in, 1, nullptr);   // flag 1: the heightmap only (clamped to the map)
    const float floorZ = out[2] + kMinClearance;
    if (pose.eye.z < floorZ) pose.eye.z = floorZ;
}

void BuildPose() {
    g_pose = ComputePose(g_smooth.feet, g_smooth.heading, g_scale, g_smooth.distance);
    ClampToTerrain(g_pose);
    g_havePose = true;
}

void PutOnCamera(PlayerController& pc) {
    const Math::Vector3 eye(g_pose.eye.x, g_pose.eye.y, g_pose.eye.z);
    const Math::Vector3 look(g_pose.look.x, g_pose.look.y, g_pose.look.z);
    const Math::Transform t = Math::Transform::FromLookAt(eye, look);
    pc.camera.SetCameraToWorld(t.AsTransform3());
}

}  // namespace

bool Active() {
    GameUpdateLoop* loop = GameUpdateLoop::_singleton;
    // Retail's follow flag: FollowDialog::OnOpened sets it, OnClosed clears it.
    if (!loop->m_toolManager->field_0x38) {
        Disengage();
        return false;
    }
    FollowDialog* dialog = FindFollowDialog(loop->m_userInterface);
    if (dialog == nullptr) {
        Disengage();
        return false;
    }
    if (Resolve(g_dialog) != dialog) {
        // A new follow: latch the dialog and its citizen's entity now, while the
        // dialog is fresh (its raw citizen pointer is read only here).
        Disengage();
        g_dialog = MakeWeak(dialog);
        const CitizenComponent* citizen = dialog->citizen;
        if (citizen != nullptr) g_entity = MakeWeak(citizen->_entity.Get());
    }
    const ComponentEntity* entity = Resolve(g_entity);
    // Removed, or marked for removal (entity +0x40 bit 10): retail's follow.
    if (entity == nullptr || ((entity->_entityFlags >> 10) & 1) != 0) {
        g_smooth.valid = false;
        g_havePose = false;
        return false;
    }
    if (!g_havePose) {
        // First step of this follow: start exactly at the citizen. Hidden from
        // the start (no transform to take): retail's follow until it shows.
        Vec3 feet;
        float heading = 0.0f;
        if (!SampleCitizen(entity, 1.0f, feet, heading, g_scale)) return false;
        Snap(g_smooth, feet, heading, g_distance);
        BuildPose();
    }
    return true;
}

void Apply(PlayerController& pc) {
    pc.CalcTransform();   // 0x1400983f0: retail's +0x00 store, the camera write skipped
    PutOnCamera(pc);
}

void ApplyFrame(PlayerController& pc, float alpha, float fovY, float farZ) {
    const auto now = std::chrono::steady_clock::now();
    float dt = 0.0f;
    if (g_haveLastFrame) dt = std::chrono::duration<float>(now - g_lastFrame).count();
    g_lastFrame = now;
    g_haveLastFrame = true;
    if (dt > kMaxFrameGap) g_smooth.valid = false;   // back after a gap (the toggle was off): snap
    if (dt < 0.0f) dt = 0.0f;

    Vec3 feet;
    float heading = 0.0f;
    if (const ComponentEntity* entity = Resolve(g_entity)) {
        if (SampleCitizen(entity, alpha, feet, heading, g_scale)) {
            Step(g_smooth, feet, heading, g_distance, dt);
            BuildPose();
        }
        // Hidden (inside a building): the last eye transform stays.
    }
    PutOnCamera(pc);
    pc.UpdateCamera(fovY, kNearPlane, farZ);   // 0x140098660 with the eye's near plane
}

int Zoom(int zoomDir) {
    if (zoomDir != 0) g_distance = ZoomDistance(g_distance, zoomDir);
    return 0;
}

}  // namespace EyeCamera
