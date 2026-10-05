# B5 — the dx11 seam: rule sheet (written 2026-10-05, start of the step)

ENDGAME B's last structural step. `dx11/` (our VideoDX11-x64.dll replacement, ~8.9k lines, 12 classes)
stops talking to the runtime's `Video::` classes through reinterpretation: the twelve `VideoDX11::XData`
PODs become real C++ SUBCLASSES of the twelve abstract `Video::XData` bases, the compiler builds every
vtable, and the hand-table machinery (`dx11/funcs/dx11_seam.{h,cpp}`, `DX11_REGISTER_SLOT`,
`Dx11SeamInstall`, `Dx11LiveVtable`, `Dx11VtableAddress`, `Dx11OriginalSlot` / `Orig<>`) leaves the tree.
Session kit: `b5_session/` (gate scripts copied from `b4_session/`, `runs/baseline` tracked). Inventories:
`b5_session/INVENTORY_A_DX11.md` (dx11 by class), `INVENTORY_B_OUTSIDE.md` (everything outside dx11/),
`DEVLOST_FINDING.md` (the 20 GB device-lost finding, read-only).

Starting numbers (`castmetric.py code runtime standalone dx11 engine` at `d14517b71`): PROD 336 = dx11 240
(reinterpret 135, static_void 87, pun 1, accessor 17; 51 bridge markers) + 96 outside (63 retail-difference,
23 named prerequisite, **7 dx11 seam**, 3 ruled). Acceptance (`b4_session/acceptance.sh`): 5 lines off —
04 (5 accessor defs), 05 (17 calls), 06 (1 pun), 07 (57 bridge lines), 10 (21, accepted miscount).
FLOOR category `dx11_seam` = 20 marked lines (CompanionData.h 12 cookie casts, Context.h 2 forwarders,
video_interface.cpp 6).

## 0. Scope (the user's, unchanged from B.4)

The DANGEROUS casts go; only safe casts stay. Differences from retail found on the way are RECORDED in
`docs/known_port_issues.md` (rows continue from 890), not fixed — except what the seam work itself
touches, in a `behaviour:` commit against `VideoDX11-x64.dll`. Device-lost stays DEFERRED (§6).

## 1. Decisions (taken in NEXT_SESSION §2 — do not re-litigate; refined here)

D1. **The twelve `Video::XData` bases flip to `virtual ~XData()`.** `DeletingDtor(unsigned)` and its
    vector arm (the twelve `CAST-OK: dx11_seam` cookie casts) are deleted; the twelve
    `static_assert(!has_virtual_destructor)` become `static_assert(has_virtual_destructor)`. GCC spends
    two slots: retail slot N >= 1 is compiler slot N + 1 — irrelevant here because NOTHING reads these
    tables by index once the hand tables are gone (B3X already converted every reader to by-name calls;
    `tests/unit/slot_check.h` derives the shift from `has_virtual_destructor`). The other virtuals keep
    retail order and their `detail::CompanionPureCall()` bodies (retail's `_purecall` in every slot: the
    base stays instantiable, which the DX9 placeholder of D5 and the unit tests rely on). The
    `~XData()` bodies in `companion_data.cpp` stay (empty); the vptr stamp retail's dtor did is now the
    compiler's.
D2. **Each root owns its allocation.** Retail's factories do `MemAlloc(size, 4)` + the base ctor; retail's
    slot 0 frees with `MemFree` on `flags & 1`. Each of the twelve bases is its OWN root
    (`reference_virtual_dtor_cutover_rules`: one class-scope `operator delete` per root), so each declares
    `static void* operator new(size_t)` = `System::MemoryAllocator::MemAlloc(n, 4)` and
    `static void operator delete(void*)` = `MemFree`, defined in ONE new file
    `runtime/src/video/companion_operators.cpp` (host arm `std::malloc/free` under `BANISHED_UNIT_TEST`,
    the `root_operator_delete.cpp` split). The factories become `return new VideoDX11::XData(owner);` —
    the `if (!p) return nullptr` arm is dropped (a null from MemAlloc is a crash either way; note it in
    the comment, do not add a nothrow overload).
D3. **`class VideoDX11::XData final : public Video::XData`** in `dx11/types/VideoDX11Data.h`, same
    private tail in the same order, every retail `static_assert(offsetof/sizeof)` kept (`sizeof` stays
    exact: the base is `{vptr, _owner}` = 0x10 and the tail follows at +0x10 — assert it). Every slot
    body becomes a MEMBER declared `override` with the base's EXACT signature (INVENTORY_A §1 lists the
    mismatches: the SLOT decides, B35_RULES B5-14 — a dx11 body that ignores a parameter still declares
    it). Bodies stay in their current .cpp files as `R XData::Member(...)`. The `self->field` spelling
    becomes `field`. Shared bodies (Index/VertexBufferData `Lock`/`Unlock`, the constant stubs) become
    one static helper called from both members, or one member each — never a cross-class `reinterpret_cast`.
D4. **COM pointers are typed.** `void* d3dDevice` -> `ID3D11Device* d3dDevice`, etc., with FORWARD
    declarations (`struct ID3D11Device;`) in `VideoDX11Data.h` so the header stays free of `<d3d11.h>`;
    the `static_cast<ID3D11X*>(field)` and `reinterpret_cast<void**>(&field)` casts die (FLOOR §5: 36 by
    themselves). `IID`/`QueryInterface`/`CreateDXGIFactory1(..., void**)` out-params stay as the
    `com_iid` floor category (CAST-OK: com_iid), Win32 handles as `win32`.
D5. **The DX9 placeholder is a real subclass**, `class VideoDX11::Dx9PlaceholderDeviceData final : public
    Video::DeviceData` (file `dx11/src/dx9_placeholder.cpp`), overriding `EnumerateInterfaces` (the shared
    DXGI walk under the "DirectX 9" name), `CreateDevice` (false) and `Shutdown` (nothing); the other 16
    slots INHERIT the base's `_purecall` bodies instead of dangling past a 4-entry hand table. The
    behaviour (a second GraphicsInterface entry so the Options dialog's `GetInterface(cfgIndex)` keeps
    `array index == slot index`, a standalone deviation retail never meets) is unchanged and its banner
    moves with it. `g_dx9PlaceholderVtable` and the `Dx9Placeholder` POD are what "leaves the tree".
D6. **`Video::Device::Adapter` and `GraphicsInterface` get their retail ctor / dtor as C++ members** and
    `System::String name` (ctor `??0Adapter@Device@Video@@` 0x1800f7100, dtor 0x18000cc10;
    `??0GraphicsInterface@Device@Video@@` 0x18000cd90, dtor 0x18000cdc0 — the bodies are the
    `RtVideo_*_Ctor/Dtor` doors in `device_enum_types.cpp`, moved onto the members). EVERY caller of the
    four doors converts in the SAME commit (INVENTORY_B §5: `devicedata.cpp`'s `gi` / `rec` locals,
    `AdapterArraySetLength` / `GfxInterfaceArraySetLength`, runtime's own users), or a name is released
    twice. That commit removes `optionsdialog.cpp:492 / :520`. It is a `behaviour:` commit only if a
    copy / growth site changes what runs; otherwise `B5:`.
D7. **`DeviceData` exposes its swap chain by name.** `standalone/main.cpp:879 / :951` read `+0x18` of the
    active DeviceData: they call `VideoDX11::Dx11SwapChain()` (a function in `dx11_backend_access.h`
    returning `IDXGISwapChain*` from the typed subclass through `static_cast` of the active
    `Video::DeviceData*` — the one downcast dx11 may make, because dx11 built the object).
    `boot_window.cpp:49` takes a `Video::Configuration*` once `devicedata.cpp` hands it typed.
D8. **The `Field<T>` / `FieldPtr` / `IbI32` / `IbU32` accessors and their offset tables go**: each site
    reads the NAMED member of `Video::Context` / `Video::Device` / `Video::ImageBuffer` (INVENTORY_A §5
    names them). `Context.h`'s two `CAST-OK: dx11_seam` forwarders (`SetRenderBuffer(const void*)`,
    `SetVertexBuffer(const void*, ...)`) are deleted; dx11 calls the typed members.
D9. **`Dx11OriginalSlot` / `Orig<>` arms are DELETED, not converted.** Every one is "the import failed,
    run the original" from the DLL-present era; the original does not exist and the arm returns null
    into a call. The two live ones (`DeviceData_CreateDevice` / `_EnumerateInterfaces` when
    `d3d11.dll` / `dxgi.dll` exports are missing) become `return false` with the comment saying why.
D10. **Ownership of the companion from the runtime side is unchanged**: `Video::X::_platformData` /
    `_data` stay `Video::XData*` (typed by B.4 R-VIDEO), created through `Device::CreateXData` (virtual
    on `Video::DeviceData`, now the subclass's override) and destroyed with `delete` where retail's
    slot-0 call with `flags = 1` was (`->DeletingDtor(1)` sites in INVENTORY_B §3 become `delete p`).
D11. **`_owner` becomes typed per class** (`Video::Device*`, `Video::Context*`, ...): the base ctor takes
    the typed owner (the eleven `Create*Data` forwarders already spell the types). The
    `static_cast<const Video::X*>(self->owner)` casts die.

## 2. Per-class recipe (what a block does for one class X)

1. `runtime/types/video/CompanionData.h`, class `Video::XData` ONLY: replace `DeletingDtor` with
   `virtual ~XData();`, type `_owner`, add the two operators (D2), flip the exemption assert. Keep every
   retail address comment. (Each block edits its own classes' sections of this one header — disjoint
   hunks; the coordinator resolves by hunk.)
2. `dx11/types/VideoDX11Data.h`, class `VideoDX11::XData` ONLY: the subclass (D3, D4), every slot as an
   `override` declaration carrying the retail address comment, the constructor
   `explicit XData(Video::X* owner)` doing exactly the factory's initial stores (the non-zero ones:
   `ConstantBufferData::activeIndex = -1`, `ContextData::topology = 0`; the byte-vs-dword stores the
   factory comment records), the destructor declared `~XData() override`.
3. The class's .cpp: free functions -> members; `self->` -> nothing; the slot-0 body becomes the
   destructor: COM releases in RETAIL ORDER (the comment says which), NO vptr re-seat, NO qualified
   base-dtor call, NO `MemFree` arm. Delete the `DX11_REGISTER_SLOT` lines. The factory in
   `devicedata.cpp` for X: `return new XData(owner);` (until the DEV block lands, the factory is still
   a free function registered in DeviceData's hand table — that is fine: it returns a `Video::XData*`
   that now wears a COMPILER table; see §3 W0 for why the seam install tolerates the gap).
4. Build (`-j2`) debug + perf + unit; the unit tests that pin X's slots / size (INVENTORY_B §2) are
   rewritten in the same commit (retail slot numbers stay; `slot_check.h` shifts). A size assert on
   the subclass is `sizeof(VideoDX11::XData) == <retail MemAlloc size>`.
5. Static gate only (no game): 0 new warnings against `b5_session/runs/warn_baseline_*.txt` (the gate's
   `norm` rule), unit 0 failures, `castmetric.py dx11` down by the class's rows, no `self->vptr` left for X.
6. Hand in: branch name, the commit, the per-class cast count before / after, the signature mismatches
   you resolved (which side was right and the retail evidence), anything you could NOT convert and why.

## 3. Shape and order

- **W0 (coordinator, serial, gated):** (a) `Dx11SeamInstall` tolerates a vtable with ZERO registrations
  (a flipped class) so blocks can land one class at a time while `dx11_seam.cpp` still exists; (b) the
  base operators file (D2) and the typed-owner ctor signatures for all twelve bases (so blocks do not
  all edit the same twelve lines); (c) the eleven factory bodies move out of `devicedata.cpp` into the
  owning class's file with external linkage (`VideoDX11::Factory_XData`), declared in
  `VideoDX11Data.h`, so no two blocks edit the factory region. Gate: full (`gate.sh`, resave 5).
- **PILOT (one opus agent):** VideoProgramData + RenderBufferData (`smallclasses.cpp`, 262 lines, no
  private state). Its hand-in is the model the other blocks copy; the coordinator gates it ALONE.
- **Blocks (opus, worktrees, disjoint files; at most four building at once, `-j2`):**
  | block | classes | files owned |
  |---|---|---|
  | BUF | ConstantBufferData, IndexBufferData, VertexBufferData | `dx11/src/buffers.cpp` |
  | RES | ImageBufferData, RenderStateData, VertexLayoutData, VertexProgramData, PixelProgramData | `imagebufferdata.cpp`, `renderstatedata.cpp`, `vertexlayoutdata.cpp`, `shaderprograms.cpp` |
  | CTX | ContextData (+ D8 for Context / Device fields, D9 for its `Orig<>`) | `contextdata.cpp`, `Context.h`'s two forwarders |
  | DEV | DeviceData (+ D5, D6, D7, D9; `devicedata_stubs.cpp`, `exports.cpp`) | `devicedata.cpp`, `devicedata_stubs.cpp`, `exports.cpp`, new `dx9_placeholder.cpp`, `Device.h` Adapter / GraphicsInterface, `device_enum_types.cpp`, `video_interface.cpp`, `main.cpp:879/:951`, `boot_window.cpp:49`, `optionsdialog.cpp:492/:520` |
  Shared by region: `VideoDX11Data.h`, `CompanionData.h`, `dx11_backend_access.h` (helpers:
  `Dx11ImageBackend` is RES's, `Dx11DeviceData` / `Dx11Device` / `Dx11Context` / `Dx11SwapChain` DEV's).
  DEV is the largest and runs from the start; BUF and RES are the mechanical ones.
- **CLOSE (coordinator):** delete `dx11/funcs/dx11_seam.{h,cpp}`, `Dx11VtableAddress`, the `Vt` enum,
  `kSlots`; `Dx11SeamInstall` call and `Dx11SeamReport` atexit in `standalone/main.cpp`; the floor
  category `dx11_seam` from `FLOOR.md` and `castmetric.py` (and its 20 lines); acceptance line 01 back
  to the PROD-total form (the S4 comment in `acceptance.sh` says where); `leftfree_c.py`'s "dx11 seam"
  group (7 -> 0); the exemption paragraph in `reference_virtual_dtor_cutover_rules`; docs
  (`dx11/README.md`, `dx11/src/README.md`, `engine_architecture.md`); then the independent audit,
  `stepclose.sh`, `devlost.sh` (bounded, §6), `worldcmp.sh` against `oldbase`.

## 4. The gate (every merge, coordinator only)

`b5_session/mergegate.sh <branch> <label> "<title>"`: the merged tree builds BEFORE the merge commit
exists; `gate.sh` (debug / release / perf / unit builds, 0 new warnings, new game `5683/0`, tutorial
`5773/12`, loadgame, the -O2 new-game route, typetable == baseline, resave 5/5 == golden). PLUS, for
this step, `devlost.sh` on EVERY merge that touches a video object (it rebuilds every companion:
`DLMAX=5`, `killed=YES` expected until §6 is settled) and a trace-oracle comparison
(`tools/trace_oracle/`, skill `camera-pin`) on the PILOT, CTX and DEV merges — never a pixel diff.
Invariants: unit 914 / 0 at the start; the B11 object comparison is NOT required for a vtable-shape
change (the codegen changes by construction) — instead the reviewer reads every slot body diff
against the previous free function: same statements, same order, same releases.

## 5. Working rules (the ones that cost a re-run before)

- Agents never run the game; builds `-j2` in the foreground (600000 ms timeout, re-invoke); at most four
  building at once; the coordinator alone gates. No `git stash`, no amend / rebase / push, no sub-agents,
  no `taskkill /IM`. A fresh worktree is reset to the NAMED base commit first (`git log -1` before
  editing). Merge main before the final build; resolve a shared-header conflict by hunk.
- No null guard added or dropped; a retail unguarded deref stays unguarded. A class local with a zeroing
  ctor adds stores: the subclass ctor writes exactly the factory's stores, nothing else.
- Bodies keep their retail address comments and their quirk notes ("do not fix" lists).
- Write files that contain backslashes with the Write / Edit tool, never a heredoc. Never `git add` the
  session directory wholesale (`runs/` is untracked except `baseline`).
- A marker proposal is not a marker: `castmetric.py --floor` before raising a ceiling (no ceiling rises
  in this step; `dx11_seam` goes to 0 and is deleted; `com_iid` / `win32` / `gpu_map` ceilings stay).

## 6. Device-lost (read `b5_session/DEVLOST_FINDING.md` before touching Finish / Resize)

The forced route loops a ~100 MB round trip per frame after one injection (known_port_issues 890).
Device-lost is DEFERRED by the user; the rule for this step: the CTX / DEV blocks reproduce the current
bodies' behaviour exactly (same hr handling, same releases) and RECORD what the finding says about them.
If the finding shows the HARNESS (the injection flag) is what loops, the harness fix is a `B5:` commit
on `standalone/` and `devlost.sh` loses its `DLMAX` cap; if it shows a real leak in a body a block
rewrites anyway, that is a `behaviour:` commit against VideoDX11-x64.dll with the retail evidence, gated
alone. Nothing else in the device-lost area is touched.

## 7. Acceptance at the close

`b4_session/acceptance.sh` lines 01-12 at target with line 01 in the PROD-total form, i.e. the PROD
total is 63 retail-difference + 23 named prerequisite + 3 ruled = 89 and NOTHING is filed under "dx11
seam"; lines 04 / 05 / 06 / 07 at 0; `castmetric.py dx11` = the `com_iid` + `win32` + `gpu_map` floor
lines only; `grep -rn "DeletingDtor\|DX11_REGISTER_SLOT\|Dx11VtableAddress\|dx11_seam" code runtime
standalone dx11 engine tests` = 0; unit 0 failures; `stepclose.sh` clean; `worldcmp.sh` identical.

## 8. Corrections (dated, appended as blocks find them)

(none yet)
