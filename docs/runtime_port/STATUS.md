# Runtime port — status

Keep this to one screen. Update the closure block after every batch; never copy it into CLAUDE.md.

## Closure (2026-09-12 close, tools/cutover_closure, roots = census + every minted vtable slot)

| | |
|---|---|
| reachable functions from our call surface | 2,871 |
| unported | 128, all C runtime (our CRT covers them) |
| **engine functions to port (static lower bound)** | **0** (206 → 856 → 145 → 0 in one day) |
| minted vtables complete | 152 of 152 (the two unexported IO::Encoding tables joined 2026-09-12 evening) |
| registry doors | name and address are ONE door (`standalone/rt_export_addrs.inc`) |

Worklist: `docs/runtime_port/closure/WORKLIST.md` (empty). Every function the DLL's call graph
reaches from our call surface OR from any minted vtable slot is ported, and every port is
reachable both by mangled name and by retail address. From here the closure cannot help:
what the gate finds next is state, not code — objects handed to our bodies by a route that
built them wrong, and callbacks stored in data.

## Cutover gate

`make cutover` (deploys the debug exe, moves the DLL aside, boots with `BANISHED_RT_NO_DLL=1`,
which implies `RT_MINT` + `RT_ALLOC`, dumps the live registry, restores the DLL; output in
`docs/runtime_port/cutover/latest/`). Run it under `tools/game_dir_lock.sh`. Symbolize frames
with `addr2line -f -C -e standalone/build/reimpl-x64.exe 0x14xxxxxxx`.

Walls passed 2026-09-12: the Video shader family → the UI Description/Element family (53
classes) → `Font::Serialize`'s two by-address callees → (evening walk, one session, serial)
the two unminted `IO::Encoding` tables (Visitor 0x1801559f8, Text 0x1801559d0; both minted
by address, ten slot bodies in `io/encoding_visitor_text.cpp`) → the two unminted
`GraphicsMesh::Serialize` statics (0x1801fc69c/0x1801fc6a0, `EXTRA_STORAGE_BY_ADDR`) → a HANG,
not a crash: `AnimationGroup::Serialize` emitted the +0x30 float BEFORE the tag array (retail:
after), so the tag count came out of the float's bytes and `CompressedStream::Read` spun on a
zero-length chunk past EOF (found with `gdb -p`, not the crash log) → **the gate then booted to
the in-game world** (24 citizens, 21k entities at frame 7200) → the quit path: eleven scalar
destructors the `RT_DELDTOR` table still resolved to NULL (`object/deleting_dtor_leaves.cpp`:
SoundEffect, MaterialInstance, AnimationGroup, ToolTipDialog, ReaderWriter, Light, SunLight,
AchievementInstance, Base, Stream, ParticleInstance) → one `RT_ORIGINAL_ONCE` on a live path
(`video/buffers.cpp` MemFree) → **`make cutover` with `BANISHED_PAUSEMENU=600
BANISHED_PAUSEMENU_QUIT=120` exits 0 with an empty crash log and the DLL absent.**

**The gate's "done" condition is met: boot to the world, quit cleanly, DLL absent.** Then the
render walk (same evening): the user saw a black window. RenderDoc on a DLL-absent frame 150
showed 6 draws (DLL-present: 453); `Renderer::AddDrawItem` dropped every item on its
"VideoProgram+0x20 null" arm because every `MeshLayout\*.rsc` load failed External::Load's
version check (`[ExtLoad] badVer=65`, 0 DLL-present): `VertexLayout::GetFileVersion` returned
1, retail returns 3. Fixed; the DLL-absent frame is now 460 actions like DLL-present, and the
menu, title and terrain draw. **Still wrong DLL-absent, from the two captures side by side**
(`tools/trace_oracle/capture_arm.sh` with the DLL moved aside; images were in the session
scratchpad, regenerate rather than look for them):
- no building or tree meshes in the menu background town (terrain only) — the MeshInstance /
  GraphicsMesh draw path or its buffers, first place to look is what those 453 draws bind;
- text draws only the FIRST glyph of every label ("n" for New, "l" for Load); the Font glyph
  Array helper was this morning's by-address wall, and `Font::Serialize` is suspect;
- a large translucent grey rectangle covers the right half of the screen (a dialog panel or
  tooltip with a wrong size).
Method that worked twice tonight and should be the default for render walks: capture under
RenderDoc DLL-absent, look at the frame overview and one draw's state, then put a
count-limited `fprintf` on the silent-drop arms of the body the numbers point at, one gate
run, remove the trace. Also audited: the 19 `GetFileVersion` ports written as a constant all
match the DLL bytes now; ports that return through a variable were not covered.
What is NOT yet triaged: how many of the 651 `RT_ORIGINAL_ONCE`/`RtOriginal<>` sites sit on
live (non-oracle) paths — the gate found one (`video/buffers.cpp` MemFree).

## DLL-present build

`make sa-run` boots to the in-game world and pause-menu Quit works (re-verified 2026-09-12
at close, 975 UI exports registered, 135 deleting-dtor rows: `BANISHED_AUTOSTART=1 BANISHED_RT_DIFF=1 BANISHED_PAUSEMENU=600
BANISHED_PAUSEMENU_QUIT=120`, exit 0, empty crash log). Twelve divergent sites: the ten
pre-existing negative controls and vtable-identity artefacts, plus two that appeared the
moment the registry doors were unified and two name-only ports went live by address:
`UI::SpriteContext::Flush` (SHADOW class, "context DIRTY MASK differs", 52,927 of 57,110
calls) and `Video::VideoProgram::SetupShaders` (ORACLE class, already flagged by the harness
as "released pointer replayed", a counter at +0x4). The game renders and quits normally.
Whether these are oracle artefacts of bodies that now run, or port defects the bypass was
hiding, is unresolved — the first DLL-present question for the next session.
Known broken route: the unattended Mods menu example (`BANISHED_AUTOMENU_BUTTON=7
BANISHED_AUTOMENU_CLICKS=1001,5001,d0,4001,101,m6`) dies with an ACCESS_VIOLATION at
`ModDialog_ToggleMod_Impl+0x43` reading 0xd0. Upstream null, not yet found.

## Held back, with reasons

- `System::MemoryAllocator` (7 bodies) armed only under `BANISHED_RT_ALLOC`, which
  `BANISHED_RT_NO_DLL` now implies. Needs a DLL-present A/B before it goes default-on there.
- Perf gate breached +7.7% in Wave 4; the user deferred it to after cutover.
- `VertexLayout::Add` / `GetStreamCount` / `GetElement*` are non-virtual and unported.
- Ports reached only through the DLL's own vtables (every `UI::*Description` / `*Element`
  body, the ShaderProgram family, Audio::Stream::Run) show no oracle attendance DLL-present;
  only the gate exercises them. The DLL-present run proves link/register/boot, nothing more.
- `deleting_dtor.cpp` row indices are table positions (135 rows now). Every parallel batch
  collided on them; append after the last row and re-count before registering.
- `tools/rt_deleting_dtor_verify.py` flags the two `Object::*Compiler` rows BAD (their dtors
  restamp two vtables; the reader knows one) and eight rows UNRECOGNISED (bare tail-jump
  dtors). None is a port defect; each is explained at its row.
- Two mesh-code lines reached the DLL through `RT_ORIGINAL_ONCE`, the raw door that skips
  the registry, and silently bypassed two ported bodies until 2026-09-12. A bypassed port
  never runs, so the oracle cannot see it. Grep for that macro before trusting attendance.
