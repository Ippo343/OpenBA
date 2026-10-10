# Phase D.2 block ENGTYPES: engine/types dissolved

Branch `d0-engtypes`, base 2c1840ad4. A pure restructure: `engine/` is gone and
`-Iengine/types` has left SA_INCLUDES. Every perf object is byte-identical to the base build
once debug info is stripped, except one `__DATE__`/`__TIME__` stamp. Nothing was STOPPED: no
header pair declared one class with two layouts.

## Who actually reached engine/types (measured, not guessed)

Taken from the `.d` dependency files of the perf and unit builds before the change:

| header | TUs that pulled it in | via |
|---|---|---|
| common.h | 843 perf, 80 unit | runtime/types/math/Math.h and 8 other runtime headers (relative `../../../engine/types/common.h`), plus bare `"common.h"` in two code/Game/Game stubs |
| ui/Panels.h | 40 | `Dialog/ui_panels.h`, plus direct `"ui/Panels.h"` in spritecaptureui.cpp, townhallui.cpp |
| ui/Dialogs.h | 13 | `Dialog/ui_dialogs.h`, plus direct includes in moddialog.cpp, saveloaddialog.cpp, standalone/modal_driver.cpp |
| Component.h | 9 perf, 1 unit | only through the Fire / Disease / Tornado forwarders' bare `"Component.h"` |
| FireComponent.h | 8 | `engine/types/FireComponent.h` in firecomponent_funcs.h, firecomponent_ignite.cpp |
| DiseaseComponent.h | 3 perf, 1 unit | diseasecomponent_funcs.h; bare `"DiseaseComponent.h"` in tests/unit/k1_abi_test.cpp |
| TornadoComponent.h | 2 | tornadocomponent_funcs.h |
| RawMaterialDescription.h | 1 unit | bare include in tests/unit/k1_abi_test.cpp |
| UpdateLoop / LoaderLoop / DialogLoaderLoopT / MainLoop / GameUpdateLoop.h | 1 unit | tests/unit/engine_loop_abi_test.cpp |
| CitizenBehavior.h, EntityList.h | 0 | none |

The brief's suspicion holds for two TUs only: the bare `"common.h"` in
code/Game/Game/menudialogloopt_stubs.cpp and splash_loop_stubs.cpp resolved to
engine/types/common.h. With `-Iengine/types` gone it would have silently resolved to the
different runtime/types/common.h, so both now name the moved file. Every other bare include of
a twin's basename resolves in its includer's own directory (checked file by file).

## Per header

- **common.h: moved, not into code/Game.** `git mv` to `runtime/types/global_types.h`.
  It declares the global `struct Time`, the Vector2/3/4 aliases of Math::Vector*,
  PostCallback and opaque forward declarations: runtime namespaces, and its includers are
  runtime/types headers with Math.h first. Putting it in code/Game would make
  runtime/types include code/Game. The name runtime/types/common.h is taken by a different
  forward-declaration header, hence the new name. Its own include of Math.h became `"math/Math.h"`.
  The ten relative includes and the two bare stub includes were repointed.
- **CitizenBehavior.h, EntityList.h: deleted.** Pure forwarders to Entity/; no includer.
- **Component.h: deleted.** Forwarder to Component/Component.h; reached only through the
  three forwarders below.
- **DiseaseComponent.h: deleted.** It forwarded to Component/DiseaseComponent.h and
  Component/DiseaseDescription.h; its vtable notes are already in the twin. Includers name
  both twins.
- **FireComponent.h: deleted.** Forwarder plus a `FireDescriptionField` offset namespace
  with no reader. FireDescription.h is a real class whose fields carry the same offsets and
  static_asserts, so nothing is lost. Its vtable and int32 notes are already in the twin.
- **TornadoComponent.h: deleted.** Forwarder plus a `TornadoDescriptionFields` offset
  struct with no reader. TornadoDescription.h carries the fields.
- **RawMaterialDescription.h: deleted, its flag table moved.** The `RawMaterialFlags` bit
  namespace exists nowhere else. It was moved verbatim into
  code/Game/Component/RawMaterialDescription.h above the class, with no reader. No layout
  or static_assert changed.
- **UpdateLoop.h, LoaderLoop.h, DialogLoaderLoopT.h, MainLoop.h: deleted.** Each held a
  global alias of the Engine:: class. The only user of the four aliases was
  engine_loop_abi_test, which asserted that they exist. Those four CHECKs were dropped
  with the aliases. The test keeps its GameUpdateLoop base-class check, now including
  Game/GameUpdateLoop.h directly. The test count is unchanged.
- **GameUpdateLoop.h: deleted.** Forwarder to Game/GameUpdateLoop.h.
- **ui/Dialogs.h, ui/Panels.h: moved.** `git mv` to code/Game/Dialog/Dialogs.h and
  Panels.h, next to the classes they gather. No name collision. Seven includers now say
  `"Dialog/Dialogs.h"` / `"Dialog/Panels.h"`.

## Other edits

- Makefile: `-Iengine/types` removed from SA_INCLUDES. The unit build reuses SA_INCLUDES,
  and there is no tests/Makefile.
- castmetric.py: `engine` dropped from INDEX_ROOTS and the docstring. leftfree_c.py:
  dropped from DIRS. No b9_session/*.sh names `engine`.
- .claude/rules/type-headers.md: the `engine/types/**` path glob and its plain-struct rule
  line went, since the directory no longer exists.
- CLAUDE.md and STATUS.md name engine/types only in history lines, so neither changed.
- Left alone: comments in code that mention engine/types historically, and old session
  scripts (b34 warn_ab2.sh, b39 filemap.py, b3r E_engine_funcs.py, b4 acceptance.sh /
  B_runtime_bodies.py / C_offsets.py). They tolerate a missing directory or are history.

## Verification

| check | result |
|---|---|
| `standalone-debug` -j2 | exit 0 |
| `standalone-perf` -j2 | exit 0 |
| `unit` | exit 0, 920 tests, 0 failures |
| new warnings vs b9 baselines | 0 debug, 0 perf |
| castmetric total | 2 |

**B11.** The b9_session `b11base` snapshot predates B9's own changes. Against it,
tool_harness shows B9's QwordArraySetSize to rtcoll::SetSizeCore swap. So the base was
rebuilt: a detached worktree at 2c1840ad4, `standalone-perf` -j2, then b11cmp over every
object.

```
# TOTAL IDENTICAL 16944
```

That is 0 SAME, 0 DIFF, 0 ONLY-BASE, 0 ONLY-TIP over all 935 objects. Three objects define
no function: tool_base_edit2, system/debug, ui_operator_delete. After
`objcopy --strip-debug`, 934 of 935 objects are byte-identical. The exception is
standalone__stress_harness.o, whose only difference is its `__DATE__ __TIME__` banner string.
The raw bytes differ only through the worktree's absolute path in the debug info.

Nothing was run against the game directory.
