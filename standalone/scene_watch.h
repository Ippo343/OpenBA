// standalone/scene_watch.h — Graphics::Scene node-array integrity probe.
//
// Enabled with BANISHED_SCENE_WATCH=1 (off = both calls return immediately).
//
// Why: Graphics::Scene::Clean (Runtime 0x1801023f0) walks the scene's node array
// and dispatches vtable +0x40 on every live node's Geometry. Every drawable
// component in this port embeds its Geometry INSIDE the component allocation
// (ModelComponent +0x28, StatusIconComponent +0x28, ...), so a component freed
// while its scene id is still registered leaves a node pointing into recycled
// heap — Clean then calls through a garbage vtable ("EXEC of <heap address>").
//
// SceneWatch_Check finds that node the frame it appears instead of whenever Clean
// happens to reach it; SceneWatch_Note records which of our AddObject sites last
// registered each scene id, so the report names the owner.
#pragma once
#include <cstdint>

void SceneWatch_Note(uint32_t sceneId, void* geometry, const char* who);
void SceneWatch_Check(void* scene);

// BANISHED_SCENE_CENSUS=1 — per-frame membership + position churn of the scene.
//
// "Models keep popping in and out while I play" has two candidate mechanisms and
// this separates them in one run:
//   MEMBERSHIP  — geometries leaving and re-entering the scene node array. Shows up
//                 as non-zero added/removed on a settled world.
//   POSITION    — geometries staying in the scene but having their transform
//                 REASSIGNED, so the model jumps somewhere else. Shows up as a
//                 non-zero moved/jumped count.
// Graphics::Geometry keeps its live transform at +0x84..+0x9F (quat at +0x84,
// position at +0x94/+0x98/+0x9C) and the PREVIOUS one at +0xA0..+0xBB, which the
// renderer interpolates between; Graphics::Geometry::SetTransform (Runtime
// 0x1800155a0) is the only writer and it copies current -> previous first. So a
// stale or wild previous is itself a visible streak/jump, and is counted too.
//
// Citizens and animals move legitimately every frame, so a raw "moved" count is
// meaningless on its own — what matters is `jumped` (a discontinuous hop no walker
// makes in one frame) and the membership deltas.
void SceneWatch_Census(void* scene);
