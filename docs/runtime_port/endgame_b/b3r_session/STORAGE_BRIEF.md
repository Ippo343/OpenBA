# STORAGE brief — the singleton cells (read AFTER `BLOCK_BRIEF.md`, `../B3R_RULES.md` R7 and §8.5, `W1_BRIEF.md`)

Wave 1 left every read of the `engine_funcs` STORAGE globals in place. They are pointers to CELLS:
a reader writes `*g_pGameWorldSingleton` to get the object. The cells are file-statics of
`standalone/address_registry.cpp`, registered at their retail address and resolved once by
`EngineFuncsInit()`. This block gives each cell its C++ name and converts every reader.

## Measured on main (code lines, all directories)
| global | cell | lines |
|---|---|---:|
| `g_pGameWorldSingleton` | `0x1401c3dd8` | 889 |
| `g_pGameSingleton` | `0x1401c3dd8` (the SAME cell) | 29 |
| `g_pGameSettingsPtr` (`int32_t**`) | `0x1401c3de0` | 47 |
| `g_pNewGameSettings` (`void**`) | `0x1401c3de0` (the SAME cell) | 6 |
| `g_pKeyMapSingleton` | `0x1401c3de8` ("container / manager object, NOT the KeyMap") | 50 |
| `g_pInputConfigSingletonExref` | import slot `0x140151738` = `?_singleton@Configuration@Input@@` through the two-link chain `g_StandaloneKeyMapMid` -> `g_StandaloneKeyMapPtr` | 39 |
| `g_pMemAllocSingleton` | a SNAPSHOT of `System::MemoryAllocator::_singleton`, null until `RunFactorySelfTest` refreshes it | 26 |

## Decisions
1. **Each cell becomes ONE named object, owned by the class retail's code stores into it.** Read
   retail (Ghidra, the exe program; `check_connection` first, `program=` on every call, read-only):
   the xrefs that WRITE `0x1401c3dd8`, `0x1401c3de0`, `0x1401c3de8` name the constructor, and the
   constructor names the class. The object is a class static of that class
   (`static <Class>* _singleton;`, defined in the class's `.cpp`), typed as the class. If the class
   the cell belongs to is not a real C++ class in the tree, the object is a namespace-scope pointer
   in the header of the module that writes it, named after retail's use, and the hand-in says so.
2. **Two globals on one cell are one object.** `g_pGameSingleton` and `g_pGameWorldSingleton`;
   `g_pNewGameSettings` and `g_pGameSettingsPtr`.
3. **A reader keeps its dereference count and its type.** `*g_pGameWorldSingleton` (a `void*`)
   becomes `<Class>::_singleton`; where the reader then does raw offset arithmetic the expression is
   `reinterpret_cast<uint8_t*>(<Class>::_singleton)  // B.3.x bridge`. Do NOT convert offset reads
   into member accesses: that is B.4.
4. **The pointer test on the CELL goes, the test on the OBJECT stays.**
   `g_pGameWorldSingleton && *g_pGameWorldSingleton` becomes `<Class>::_singleton != nullptr`.
   A test of the cell pointer ALONE (`if (!g_pGameWorldSingleton) return;`) was true only before
   `EngineFuncsInit()`: the object was necessarily null then too, so it becomes the test of the
   object — never a deleted guard. This keeps every path's outcome.
5. **The writers** (the constructors / boot code that store into the cell, through the global or a
   resolve) write the named object. The registry rows for the three cells stay registered until
   wave 2 and point at the named objects, so any remaining by-address reader sees the same storage:
   `standalone/address_registry.cpp`'s three `g_storage_*` statics are deleted and its three
   `RegisterExeAddr` lines take the address of the named object.
6. **The Input chain.** `g_pInputConfigSingletonExref` readers triple-deref to reach `Input::KeyMap`.
   They become a read of `Input::Configuration::_singleton` (the class static exists:
   `runtime/types/input/`) and its `_keyMap` member, IF `SetStandaloneKeyMap()`'s pointer and
   `Input::Configuration::_singleton->_keyMap` are provably the same object at every reader (read
   `RunGameMain` and `Engine::Startup`). If they can differ, the chain stays a named object and the
   readers are LEFT, listed with the reason.
7. **`g_pMemAllocSingleton`** — `behaviour:` commit. Every reader reads
   `System::MemoryAllocator::_singleton` LIVE; the global, its initialiser in `EngineFuncsInit` and
   the refresh in `standalone/main.cpp` (`RunFactorySelfTest`) and `code/Game/Game/game_main.cpp` are
   deleted. The self-test itself stays. State in the commit which readers could run BEFORE the old
   refresh and therefore saw null where they now see the allocator.
8. **The globals are deleted** from `engine/funcs/engine_funcs.{h,cpp}` with their initialisers and
   their dump lines, once their last reader is converted.

## Files
Every file that reads one of the seven globals (`code/Game/**`, `standalone/**`, `engine/funcs/**`),
the headers of the owning classes, `standalone/address_registry.cpp` (the three cells and the KeyMap
chain ONLY), `standalone/main.cpp` and `code/Game/Game/game_main.cpp` (the allocator refresh ONLY).

## Method
Script it, one global at a time, one titled commit per global (`B3R STORAGE: …`), debug build after
each. The patterns are few (`*g_pX`, `g_pX && *g_pX`, `!g_pX || !*g_pX`, `g_pX ? *g_pX : nullptr`,
a local `void* gw = *g_pX;`), but READ what the script did in every file whose pattern count differs
from the median, and every site that matched no pattern.

## Hand-in (in addition to BLOCK_BRIEF.md's)
Per cell: the retail writer address, the owning class, the object's name and header; readers
converted; guards rewritten per decision 4; sites LEFT. The Input chain verdict. The allocator
commit's list of readers that could run before the refresh.
