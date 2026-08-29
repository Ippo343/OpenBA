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
