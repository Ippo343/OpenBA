### Files/Code Areas Left:
1. action: Complete 100%
2. component: Complete 100%
3. dialog: Complete 100
4. entity: Complete 100%
5. game: Near-complete (7/8 GameUpdateLoop slots reimpl, Load passthrough, SaveRestore unhookable, standalone Game functions deferred)
6. goal: Complete 100%
7. tutorial: Complete 100%
8. work: Complete 100%
9. player: Complete (needs entity classes)

### Remaining GameUpdateLoop Passthroughs:
- Load (0x140096B00): 759 instructions, creates all subsystems. Passthrough.
- SaveRestore (0x1400959C0): Unhookable (Bug Class 3). Needs fresh reimpl for standalone exe.

### Component Module (COMPLETED 2026-03-30):
  - 67 .cpp + 66 _funcs.h files in code/Game/Component/
  - All 16 active Update components extracted to clean code
  - All 48 passive components extracted to clean code
  - Only 2 actual trampoline calls remain (ParticleComponent::OnRemove, TradeComponent::Shutdown)
  - ~10 complex functions delegate to original exe via ResolveAddr (pathfinding, graphics, audio deps)
  - ResourceProduceComponent has no reimpl file (parent class, called by children)

### Game standalone functions (deferred to component phase):
- SpawnTornado, InfectRandomCitizen, StartRandomFire, DestroyAllRawMaterials
- UpdateTimedEvents
- All decompiled, depend on EntityList internals + component sub-functions

### Standalone todos:
  Populate the 582 address registry entries in address_registry.cpp and implement the game's init path (Engine::Create → MainLoop::Run) in standalone/main.cpp.