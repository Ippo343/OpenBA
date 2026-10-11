# ANIMAUDIO hand-in (Phase D.4, branch `d4-animaudio`, base `e9f7a978f`)

One retail function: `AnimAudioEvent::Construct(const Math::Transform&)`, Application-x64.exe
0x140087e40, vtable slot [8] of AnimAudioEvent (the event's action; the Runtime header's
`Construct` name is the misnomer AnimAudioEvent.h documents). It was the one PORT GAP left in
`src/game/Component/animaudioevent.cpp`: an empty body, so every animation-triggered sound in the
game was silent (row 1287).

Static gate only. The game, `sa-run` and `sa-deploy` were never run. Main did not move during the
block (`e9f7a978f` is still its tip).

## Commits

| commit | title | gate |
|---|---|---|
| `8d28e285c` | behaviour: AnimAudioEvent::Construct (0x140087e40), the animation-triggered sound | debug / perf / unit EXIT 0; unit 1000 / 0; 0 new warnings (debug, perf) against `runs/warn_baseline_{debug,perf}.txt`; castmetric src 2; abi_audit OK; acceptance 0 off target |
| (docs) | known_port_issues row 1287, this hand-in | n/a |

## What was ported (retail facts)

```
0x140087e4a  MOV RAX,[0x1401c3dd8]           g_pGameWorldSingleton (GameUpdateLoop::_singleton)
0x140087e57  MOV RCX,[RAX+0xd8]              the world's System::Random (m_random)
0x140087e5e  CALL System::Random::RandFloat  [0,1)
0x140087e64  MOVSS XMM1,[RDI+0x28]           chance
0x140087e69  COMISS XMM1,XMM0 ; JC skip      play when !(chance < roll)   (CF: chance < roll or NaN)
0x140087e76  CALL System::Time::Time(0.0f)
0x140087e7c  pos = transform +0x10/+0x14/+0x18 (Math::Transform::translation)
0x140087ea1  MOV RCX,[world+0x78]            Audio::Scene (m_audioScene)
0x140087e86  RDX = [this+0x20]               ExternalPtr<SoundEffect>, a bare pointer in a register
0x140087eaa  R8D = 4                         Audio::Scene::SoundType 4
0x140087e98  R9 = &time ; [RSP+0x20] = &pos
0x140087eb5  CALL Audio::Scene::PlayEffect3D the uint handle is dropped; no hidden return slot
```

The port (`animaudioevent.cpp`): `GameUpdateLoop::_singleton->m_random->RandFloat()`,
`if (_chance < roll) return;`, `System::Time{0}`, `m_audioScene->PlayEffect3D(ExternalPtr(_sound),
SoundType(4), timeZero, transform.translation)`. Both world reads are unguarded as retail's are. The
spelling follows `ambientemittercomponent.cpp`'s PlayEffect3D calls and `tool_base_drag.cpp`'s
PlayEffect calls.

## The dispatch path, verified in the tree

1. `ModelComponent::Tick` (slot [8], 0x140124960; `src/game/Component/modelcomponent.cpp`) calls
   `MeshInstance::Update(t)` every sim tick.
2. `Graphics::MeshInstance::Update` (Runtime 0x180100800; `src/engine/graphics/wave29_lane_slots.cpp`)
   calls `AnimationState::Update(..., rootOnly = _rootOnlyUpdates, applyEvents = true)`: the literal
   1 at 0x1801008d1, note 4 of that file's banner.
3. `AnimationState::Update` (`wave27_lane_gfxbig.cpp`) on layer 0, when the clip has events, calls
   `Animation::ApplyEvents(root, L->_time, to)` with the layer's OLD time and the new one.
4. `Animation::ApplyEvents` (`wave28_lane_gfxbody.cpp`) fires `ev->_event->Construct(base)` for every
   event whose tick falls in the window (ordinary or loop-wrap). Every fire bumps
   `g_applyEventsFired`, printed at exit as `[GFXBODY28] ... ApplyEvents=N (fired=M)`.
5. `Construct` is the virtual above: this body.

No arm on that path is a PORT GAP or guarded off. The two other `AnimationState::Update` callers
(`AnimationManager::Update`, `AnimationManager::GetInstanceId`) pass applyEvents = false, as retail's
literal 0 does, so the instanced pool never double-fires.

Perf-build note: PerfX ANIM_BATCH (non-retail, default-on in the perf exe) skips a batched model's
intermediate speed-iterations and gives the last one the skipped ticks in one call. ApplyEvents's
window is `[from, to)` so each event still fires exactly once per pass, only at the end of the frame
instead of mid-frame. PerfX ANIM_CULL's root-only update does not touch the applyEvents arm.

## Unit test

None added. `Audio::Scene`'s bodies and `GameUpdateLoop` do not link into the unit host
(`tests/unit/audio_abi_test.cpp` defines the Scene members as markers), so a probability 0 / 1 test
would need a stubbed world and scene: scaffolding the brief ruled out.

## Owed to the coordinator: the by-hand LISTEN

- A pasture with cows, sheep and chickens: the animal calls should now play, 3D-positioned at the
  animal, at the clip's random chance (`+0x28`, 1.0 by default; the resource sets it).
- Citizens at work: chopping, hammering, digging and the other animation-timed work sounds.
- `standalone_debug.log` at exit: `[GFXBODY28] ... ApplyEvents=N (fired=M)` with M > 0 on a route
  that had animated entities in view; M was already > 0 before this block (the dispatch ran into the
  empty body), so the line proves the path, the ear proves the sound.
- SoundType 4's channel group: if its volume slider in the audio options is "effects" the sounds
  follow it; if anything plays at full volume regardless, that is the first thing to read.
