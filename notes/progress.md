### Files/Code Areas Left:
1. action: Complete
2. component:
3. dialog: Complete
4. entity:
5. game:
6. goal: Complete
7. tutorial: Complete
8. work: Complete


  1. Action (finish first — nearly done)

  - 16/17 already reimplemented, only MoveTo remains as passthrough
  - MoveTo is the largest (237 lines) and most critical action (pathfinding/movement)

  2. Game (small, high-impact)

  - Only 2 files: GameUpdateLoop and Weather
  - Clean code already exists in code/Game/Game/ — just need to wire the reimpl wrappers to call it instead of trampolines
  - GameUpdateLoop is the main tick loop — understanding it thoroughly helps with everything else

  3. Entity (tiny)

  - 1 file (entityui.cpp, already real impl in reimpl layer)
  - Just needs clean code moved to code/Game/Entity/ for the standalone exe goal

  4. Component (largest, save for last)

  - 65 files, only 3 have clean code, 42 still passthroughs
  - Recommend tackling by sub-priority:
    a. Components with Update overrides (CitizenAI already done — do the rest: Herd, Character, Animal, etc.)
    b. Economy components (RawMaterial, Storage, Trade, WorkComponent — largest passthroughs)
    c. Passive/display components (StorageDisplay, Tombstone, etc.)