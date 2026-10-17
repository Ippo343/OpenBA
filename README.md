# OpenBA — Banished, reimplemented

<img src="images/ba_icon.png" width="64" align="right" alt="OpenBA icon">

OpenBA rebuilds **Banished** (Shining Rock Software, 2014) as readable, buildable C++ that
plays the retail game byte for byte, in the spirit of OpenRCT2, re3 and devilutionX. The
result is one standalone executable, `OpenBA.exe`, that loads **no retail binary**: the
game logic, the engine runtime and the Direct3D 11 renderer are all ours. It reads the game's
data from **your own installed copy** at run time and ships none of it.

On top of the faithful game sit a handful of **fixes** for retail bugs (on by default,
retail's behaviour kept one toggle away) and a set of opt-in **extensions**: a render thread,
parallel culling and pathfinding, ambient occlusion, better trees, crisp UI at any scale,
seamless ambient loops, quick loading screens and more. Every one of them is off in the
"faithful" profile, which is what every regression gate runs.

> **You need your own copy of Banished.** OpenBA contains no game assets, shaders, sounds,
> models or resource files, and will not run without the retail `WinData/*.pkg` packages.
> Buy the game (GOG, Steam or Humble; the GOG build is the one the port was made against).

## Status

- **Faithful:** the exe boots, builds a world, loads and saves retail saves, simulates and
  renders through the whole game, including the tutorial and the disaster routes. Every
  function reachable from the engine's call surface is ported; there is no address table, no
  hooking and no retail code path left in the build.
- **Verified** by a regression gate on every merge: a scripted new game, the tutorial, a
  save round-trip that must be byte-identical to a golden file, a world hash against a
  pinned seed, a stress town, the unit suite (1,151 tests) and a static "no doors" check.
- **Platforms:** Windows 10/11 x64 is the primary target. The same CMake tree builds and runs
  on Linux (tested under WSL2 with DXVK-native over lavapipe); see
  [docs/reference/environment.md](docs/reference/environment.md).
- **Known port issues** are tracked in [docs/known_port_issues.md](docs/known_port_issues.md);
  the retail bugs we found and how each one is fixed are in [docs/retail_bugs.md](docs/retail_bugs.md).

## Fixes and extensions

Everything below is a row in the in-game **Extensions** dialog (main menu) and the
**Enhanced** tab of Options. Fixes default **on**; extensions default **off**.

| Kind | Row | What it does |
|---|---|---|
| Fix | Status icons appear in place | New status icons no longer slide in from the map origin. |
| Fix | House "go to" button | Centres on the resident instead of reading past a storage component. |
| Fix | Tavern statistics | Counts the good actually consumed. |
| Fix | Grounded starting buildings | The starter barn sits on the flattened ground. |
| Fix | Steady dialogs on UI rescale | Open dialogs no longer flash to a corner. |
| Ext A | Eye-level camera | Follow a citizen at eye level. |
| Ext A | Sharp UI | Crisp text and sprites at any UI scale. |
| Ext A | Skip unseen work | Off-screen citizens, icons and effects pause until seen. |
| Ext A | Faster scene gathering | Skips out-of-view objects without touching them. |
| Ext A | Batched walking | Walking steps take a shortcut above 1x speed; same positions. |
| Ext A | Render thread | Renders on a second thread. |
| Ext A | Parallel culling | The scene gather split over threads (5+ cores). |
| Ext A | Quick loading screens | Loading and save screens close as soon as the work is done. |
| Ext A | Ambient occlusion | Screen-space AO (Intel ASSAO) on the opaque scene. |
| Ext A | Better trees | MSAA leaf edges, stable tree shading, leaf-scale shadow dapple. |
| Ext A | Max shadow resolution | A 4096 shadow map at the High setting. |
| Ext A | Seamless ambient loops | Removes the dropout at every ambient loop point. |
| Ext B | Faster pathfinding | Same-length routes for far less work; ties can differ. |
| Ext B | Parallel pathfinding | Paths found on several threads. |
| Ext B | Batched animation | Animations step once per frame above 1x speed. |
| Ext B | Batched walking, faster | Walkers seen a frame late. |

**Class A** extensions never change the simulation: a save made with them on loads anywhere.
**Class B** ones can make play diverge from the original, so a save records them and warns
when loaded without them, exactly as the game does for mods. The design, the classes and the
rules every change follows are in [docs/extensions/DESIGN.md](docs/extensions/DESIGN.md).

## Building

(The executable was formerly named `reimpl-x64.exe`; the Linux target `reimpl-x64`.)

Prerequisites on Windows: MSYS2 UCRT64 GCC 13, CMake 3.20+ and Ninja (the pip packages
work), Python 3, and the game installed. The Banished Modding ToolKit (free from Shining
Rock) is needed once, to compile the extension dialog and font resources.

```bash
git clone --recurse-submodules https://github.com/fran-cap/untitled_rev_eng_project
cd untitled_rev_eng_project
export BANISHED_DIR="C:/GOG Games/Banished"     # your install; this is the default
tools/build.sh unit          # build + run the unit tests (needs the install for a few)
tools/build.sh run           # build, deploy into BANISHED_DIR and launch a new game
```

`tools/build.sh` is the one entry point (`build | unit | deploy | run | run-menu | stress |
clean`, flavours `debug | og | perf | release`). The deploy step copies `OpenBA.exe` and
the compiled extension resources into the game directory next to the retail exe; it never
modifies a retail file. The full fresh-clone checklist, the Linux route and the debugger
setup are in [docs/reference/environment.md](docs/reference/environment.md).

## Layout

| Path | What |
|---|---|
| `src/game/` | the game logic, laid out as the original source tree (components, dialogs, AI, tools) |
| `src/engine/` | the engine runtime (objects, I/O, UI, audio, maths), with its Win32 and POSIX seams in `src/platform/` |
| `src/render/dx11/` | the Direct3D 11 renderer, plus the load-time shader rewrites the extensions use |
| `src/ext/` | fixes and extensions, each in its own directory behind the registry |
| `src/app/`, `src/diag/` | the launcher and the diagnostics / harness code |
| `tests/unit/` | the unit suite; `tools/` the build, gate, capture and analysis scripts |
| `docs/` | design, status and reference; `docs/history/` the full working record of the port |
| `third_party/` | SDL3 (submodule), Dear ImGui, FFmpeg's WMA v2 decoder (LGPL), the ASSAO sources (MIT) |

## How it was made

The retail binaries were decompiled with Ghidra and rewritten function by function into
C++ that mirrors the original structure, with binary-compatible class layouts so saves and
the game's own data files load unchanged. The whole process, every decision and every
measurement, is recorded under `docs/`: [docs/runtime_port/STATUS.md](docs/runtime_port/STATUS.md)
is the log of closes, [docs/runtime_port/NEXT_SESSION.md](docs/runtime_port/NEXT_SESSION.md)
the open items. Most of the code was written with AI agents (Claude) under human direction
and review; the commit history carries the attribution.

## Legal

OpenBA is an independent project with no affiliation to or endorsement by Shining Rock
Software. Banished is their work and their trademark. This repository holds only our own
source code and documentation: no game content, and no copies of the retail binaries. The
reimplementation was done for interoperability with the user's own copy of the game. The
Modding ToolKit and the game data stay where they are, in your install. If you are the
rights holder and have a concern, please open an issue.

**License:** OpenBA is free software under the **GNU General Public License v3.0**
([LICENSE](LICENSE)). The vendored third-party code keeps its own licence: SDL3 (zlib),
Dear ImGui (MIT), ASSAO (MIT) and the FFmpeg WMA v2 decoder (LGPL 2.1+), all GPL-compatible.
