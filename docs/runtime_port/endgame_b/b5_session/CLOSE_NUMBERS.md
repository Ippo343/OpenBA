# B5 (the dx11 seam) — close numbers (2026-10-05)

Start `d14517b71` (B4 CLOSED) -> close `020a4e3ae` (Merge b5-CLOSE) + the close documents.
41 commits, 8 gated merges (GATE_TABLE.md rows 1-8), 0 problems; 96 files, +7,100 / -4,047 lines.

## 1. What the step did

- The twelve `Video::*Data` abstract bases: `virtual void* DeletingDtor(unsigned)` -> `virtual ~XData()`;
  typed `Owner* _owner` (`Owner` typedef); class-scope `operator new` (`MemAlloc(n, 4)`) / `operator delete`
  (`MemFree`) in `runtime/src/video/companion_operators.cpp`; the twelve dead `Video_XData_Ctor` doors
  deleted. NO class in the tree declares `DeletingDtor` any more (the one grep hit is a comment).
- The twelve `VideoDX11::*Data` PODs -> `class XData final : public Video::XData`, every slot an
  `override` with the retail address on its declaration, COM pointers typed (`ID3D11*` forward
  declarations), constructors = the factory's stores, destructors = the COM releases in retail order.
  `DeviceData`'s eleven factories are members: `return new XData(owner);`.
- Deleted: `dx11/funcs/dx11_seam.{h,cpp}` (84-slot hand tables, `DX11_REGISTER_SLOT`, `Dx11SeamInstall`,
  `Dx11LiveVtable`, `Dx11OriginalSlot` / `Orig<>`), `Dx11VtableAddress`, the slot enums and `kSlots`,
  `AllocData`, `Field<> / FieldPtr<> / IbI32 / IbU32` and their offset tables (`Ctx::`, `Dev::`, `Cfg::`,
  `Ib*`), `Context.h`'s two `const void*` forwarders, the `VideoRenderState` mirror, `ReleaseCompanion<>`,
  the `RtVideo_Adapter / GraphicsInterface` ctor / copy / dtor doors, `g_dx9PlaceholderVtable`,
  `WideStringStorage`, `ShaderProgram.h`'s `programdata_vtbl`, `-Idx11/funcs`.
- The DX9 placeholder is `Dx9PlaceholderDeviceData final : Video::DeviceData` (`dx11/src/dx9_placeholder.cpp`).
- `Video::Device::Adapter` / `GraphicsInterface`: retail ctor / copy / dtor as members, `System::String name`,
  `operator=` deleted (0x1800f7100 / 0x18000cc70 / 0x18000cc10; 0x18000cd90 / 0x18000ce10 / 0x18000cdc0).
- Named: `Video::Device::onVideoError / onVideoRestart` (+0x50 / +0x58, was `gap50`); `Context`'s
  +0x2100..+0x212f as offset-named int32 halves + `void*` (the Lock byte offset / vertex count / index
  count / Lock result — CTX hand-in, GATE_TABLE row 4); `RenderState.h`'s stencil fields renamed from the
  one reading code (offsets unchanged).
- Base signatures corrected from the callers / retail mangling: `Index/VertexBufferData::Serialize(IO::Serializer*)`,
  `ImageBufferData::Create(const ImageBuffer::LevelArray*)`, `RenderBufferData::Resolve(void*, RenderBuffer*, bool)`,
  `DeviceData::SetImageBuffer(ImageBuffer*, int, int)`, the eleven `Create*Data(X*)`.
- 18 runtime `p->DeletingDtor(1)` sites -> `delete p` (same guards).
- The 7 "dx11 seam" casts outside dx11/ gone: `main.cpp:879 / :951` call `VideoDX11::Dx11SwapChain()`,
  `boot_window.cpp`'s `ArmDeferredShow` takes `Video::Configuration*`, `optionsdialog.cpp:492 / :520` read `.name`.
- One diagnostic added (B5_RULES §6): `ContextData::Finish` prints `GetDeviceRemovedReason()` on the lost path.

## 2. The metric

| | start `d14517b71` | close |
|---|---|---|
| castmetric PROD total | 336 | **89** (= 63 retail-difference + 23 named prerequisite + 3 ruled + 0 work) |
| dx11/ | 240 (reinterpret 135, static_void 87, pun 1, accessor 17; 51 bridge markers) | **0** (11 CAST-OK: gpu_map 5, win32 4, com_iid 1, byte_io 1; 7 tier_i) |
| code / runtime / standalone / engine | 74 / 15 / 5 / 0 (+2 seam in code, 5 in standalone) | 74 / 15 / 0 / 0 |
| floor categories | `dx11_seam` 20 / 20 | DELETED; `gpu_map` 0 -> 5, `com_iid` 2 -> 3, `win32` 85 -> 88, `byte_io` 41 -> 42 (all measured) |
| cast_ok / tier_i / bridge | 745 / 447 / 57 | 731 / 449 / 0 |
| acceptance (`b4_session/acceptance.sh`) | 5 lines off (01 closing form, 04 = 5, 05 = 17, 06 = 1, 07 = 57, 10 = 21) | **1 line off: 10 = 21** (the accepted template-field miscount); 01 in the PROD-total form (89 of target 89) |
| unit tests | 914 / 0 | **920 / 0** |
| dx11 lines | 8,936 | 6,827 |

## 3. Gate rows

Eight merges (W0, PILOT, BUF, CTX, RES, DEVA, DEVB, CLOSE): every one 0 problems, new game `5683/0`,
tutorial `5773/12`, typetable `b983f144`, resave 5/5, 0 new warnings; `devlost.sh` on seven of them
(INJECT=1, COMPLETE=5, crash 0, killed=YES at the cap). GATE_TABLE.md.

## 4. Close checks (on `020a4e3ae`)

- `worldcmp.sh` against `oldbase` (`b2a5434c`): identical, 118 lines, seed 12345.
- `stepclose.sh`: see STEPCLOSE below (filled at the end).
- Independent audit: `b5_session/AUDIT.md`.

## 5. Found on the way

- The device-lost loop is NOT the harness (`DEVLOST_FINDING.md`; known_port_issues row 890 updated): one
  injection, then real repeated removals of a genuinely recreated device, `GetDeviceRemovedReason` =
  0x887a0005 with no sub-cause; leading hypothesis the exe-side PORT GAP `GameUpdateLoop::OnVideoError /
  OnVideoRestart`. DEFERRED area; nothing in it was changed.
- dx11 read its own hand table by INDEX in one place (`Finish`'s Shutdown / CreateDevice): fixed by name
  BEFORE any flip (W0b) — an index would have named `Slot4` after the flip.
- The DX9 placeholder was destroyed by the runtime as a `Video::DeviceData` while being a 16-byte POD with
  a 4-entry hand table (INVENTORY_B surprise 1): it is a real subclass now.
- `mergegate.sh` refuses a dirty tree: commit the gate table BEFORE the merge (row 5 note). A stopped
  background task does not stop the script it launched: a long close runs with `nohup`, never in a task.
- The first commit of CTX's branch does not build alone (the merge does).
- No retail difference was found by any block; no new known_port_issues row besides 890's update.

## 6. STEPCLOSE (`runs/stepclose/summary.txt`, on `020a4e3ae`, 21:22-21:36)

Mods route exit 0; front-end Quit / Options / Achievements exit 0, crash 0; GoalDialog from a tutorial
pause menu opens (1 `OpenGoal` line, killed at its time limit as designed); placement / road / removal tool
routes exit 0 (PlacementTool CREATE -> 1); in-game save `SAVE OPERATION COMPLETE`, crash log 0;
`sa-stress` exit 0, clean quit (800 citizens x3, speed 10, 6000 frames, 139 fps; recorded, not chased);
true release compile from a clean obj dir exit 0 (940 files, 6.4 MB exe), `make clean`, clean rebuild of
debug / perf / unit exit 0, then the full gate on the clean-built exe: 0 problems, unit 920 / 0, new game
5683 / 0, tutorial 5773 / 12, loadgame, -O2 new game, resave 3/3; `NOT IMPLEMENTED` stubs 0, stub set 0
new / 0 gone; tutorial census citizen=16 herd=16. Audit (`AUDIT.md`): 88 OK, 13 FINDING (comments and one
dead struct, applied in the close commit), 2 QUESTION (ruled: B5_RULES §8 C7-C9). Nothing owed.
