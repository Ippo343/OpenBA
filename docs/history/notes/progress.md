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

### Known Bugs (2026-04-03):

**StorageDisplayComponent::Draw reimpl crash**
- Our reimpl crashes in TradeComponent_impl_f370 (array resize with 0x48-byte elements)
- Crash happens after MeshArrayResize succeeds, somewhere in the tile iteration loop
- Trampoline bypass works fine — original code has no issue
- Currently bypassed: `engine/reimpl/storagedisplaycomponent.cpp` Reimpl_Draw calls trampoline
- TODO: Compare reimpl loop logic against Ghidra decompilation of 0x140056730

**GameUpdateLoop::Startup null deref**
- Crash at Runtime-x64.dll + 0x1148BA (Ghidra 0x1801148BA)
- READ of address 0x40 — RBP=0, null pointer + field offset
- Last hook: GameUpdateLoop::Startup
- Happens after StorageDisplayComponent fix, during new game world startup
- EXE return on stack: 0x140049967 (EXE+0xB9967... check callers)
- TODO: Decompile RT function at 0x1801148BA, check GameUpdateLoop::Startup reimpl

### Standalone todos:
  Populate the 582 address registry entries in address_registry.cpp and implement the game's init path (Engine::Create → MainLoop::Run) in standalone/main.cpp.