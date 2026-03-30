### Files/Code Areas Left:
1. action: Complete 100%
2. component: Main remaining work (65 files, 42 still passthroughs)
3. dialog: Complete 100%
4. entity: Complete 100%
5. game: Near-complete (7/8 GameUpdateLoop slots reimpl, Load passthrough, SaveRestore unhookable, standalone Game functions deferred)
6. goal: Complete 100%
7. tutorial: Complete 100%
8. work: Complete 100%
9. player: Complete (needs entity classes)

### Remaining GameUpdateLoop Passthroughs:
- Load (0x140096B00): 759 instructions, creates all subsystems. Passthrough.
- SaveRestore (0x1400959C0): Unhookable (Bug Class 3). Needs fresh reimpl for standalone exe.

### Component Module (largest remaining area):
  - 65 files, only 5 have clean code, 42 still passthroughs
  - Recommend tackling by sub-priority:
    a. Components with Update overrides (CitizenAI already done — do the rest: Herd, Character, Animal, etc.)
    b. Economy components (RawMaterial, Storage, Trade, WorkComponent — largest passthroughs)
    c. Passive/display components (StorageDisplay, Tombstone, etc.)

### Game standalone functions (deferred to component phase):
- SpawnTornado, InfectRandomCitizen, StartRandomFire, DestroyAllRawMaterials
- UpdateTimedEvents
- All decompiled, depend on EntityList internals + component sub-functions
