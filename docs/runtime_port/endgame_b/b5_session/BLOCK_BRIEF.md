# B5 block brief (read with `../B5_RULES.md`; the rules there win on any conflict)

You convert one or more `VideoDX11::XData` classes from a POD + hand-built vtable into a real C++
subclass of the abstract `Video::XData` base, with the compiler's vtable. Your worktree is yours; the
coordinator merges and gates. Pattern model (after the pilot lands): `dx11/src/smallclasses.cpp` and the
`VideoProgramData` / `RenderBufferData` sections of `dx11/types/VideoDX11Data.h` and
`runtime/types/video/CompanionData.h` on main.

## Setup (first thing, in this order)
1. `git log -1` in your worktree. If it is not the NAMED base commit in your prompt:
   `git reset --hard <that commit>`. Then `git checkout -b <your branch name>`.
2. Read `docs/runtime_port/endgame_b/B5_RULES.md` §1 (D1-D11), §2 (the recipe), §5 (working rules),
   then `b5_session/INVENTORY_A_DX11.md` §1 (your classes' slot bodies vs base signatures — resolve
   every MISMATCH: the base slot's signature is the one the CALLER dispatches through, so the override
   carries it; where the base is wrong, the retail evidence is quoted in CompanionData.h's comments
   and in your .cpp's address comments — fix the base and say so in your hand-in), §3 (your
   destructors), §4 (your casts, by classification).

## What a converted class looks like
```cpp
// dx11/types/VideoDX11Data.h
struct ID3D11Buffer;                       // forward declarations, never <d3d11.h> here
namespace VideoDX11 {
class IndexBufferData final : public Video::IndexBufferData {
public:
    explicit IndexBufferData(Video::IndexBuffer* owner);   // the factory's stores, nothing more
    ~IndexBufferData() override;                           // retail 0x180003xxx: the COM releases
    bool  Create(uint32_t sizeBytes, uint8_t dynamic, const void* data) override;  // [1] 0x...
    ...                                                    // every slot, retail order, address comment
    ID3D11Buffer* buffer;        // +0x10   (the typed COM pointer; was void*)
    int32_t       writeCursor;   // +0x18
    uint8_t       locked;        // +0x1c
    uint8_t       pad1d[3];
};
static_assert(sizeof(IndexBufferData) == 0x20, "MemAlloc 0x20 @ 0x1800057a0");
static_assert(offsetof(IndexBufferData, buffer) == 0x10, ...);   // every field, as before
}
```
- The base's `_owner` is typed (W0): `Video::IndexBuffer* _owner`. Your old `void* indexBuffer` field
  at +0x08 IS the base's `_owner` — delete the duplicate, read `_owner`.
- Bodies in the .cpp become `R XData::Member(...) { ... }`; `self->x` -> `x`; `self` -> `this`.
  Keep every address comment, quirk note and "do not fix" list exactly.
- The destructor: the COM releases of the old slot-0 body in RETAIL ORDER (the comment says which);
  `Dx11Release(p)` is fine. Delete the vptr re-seat, the qualified base-dtor call and the
  `if (flags & 1) MemFree` arm — the compiler and the base's `operator delete` (W0) do those.
- The factory in `dx11/src/devicedata.cpp` for your class: `return new XData(owner);` (the base's
  `operator new` is `MemAlloc(n, 4)`). Its `DX11_REGISTER_SLOT` line stays (DeviceData is still a hand
  table until the DEV block lands) — only the BODY changes. The DEV block will turn it into an override.
- Delete your class's `DX11_REGISTER_SLOT` lines and its `Dx11VtableAddress` / `Orig<>` /
  `Dx11OriginalSlot` uses (D9: an `Orig<>` arm becomes the documented failure return, never a call).
- Typed COM pointers (D4): `static_cast<ID3D11X*>(field)` and `reinterpret_cast<void**>(&field)` die;
  a `QueryInterface` / `CreateDXGIFactory1` out-param stays `CAST-OK: com_iid`.
- Shared bodies (Lock/Unlock across Index/VertexBufferData; the `return true` / `ret` stubs): one
  `static` helper in the .cpp called from each member, or one tiny member each. Never a cast between
  two companion types.
- `CompanionData.h`, YOUR classes' sections only: `virtual void* DeletingDtor(unsigned flags) {...}`
  -> `virtual ~XData();` (the body stays in companion_data.cpp, empty); the
  `static_assert(!std::has_virtual_destructor<XData>::value, ...)` -> `static_assert(
  std::has_virtual_destructor<XData>::value, "B5: the dx11 subclass wears the compiler's table")`.
  Keep the `[0] 0x1800589xx` address comment on the destructor line. Do not touch other classes'
  sections (another block owns them; the coordinator merges by hunk).
- `tests/unit/b39_vid_data_test.cpp`: remove YOUR classes from `CheckData<>` (the `DeletingDtor`
  test) and add them to a `CheckDataB5<T>` that checks `has_virtual_destructor`, `is_destructible`,
  `!is_default_constructible`, `_owner` at +0x08 after construction, `sizeof(T) == 0x10`. Keep the
  PMF slot-order test. `tests/unit/video_data_abi_test.cpp` / `slot_check_test.cpp`: rewrite the pins
  for your classes (INVENTORY_B §2 lists them); retail slot numbers stay — `slot_check.h` shifts.

## Build and static gate (no game run, ever)
```
/c/msys64/ucrt64/bin/mingw32-make.exe -j2 standalone-debug > build_debug.log 2>&1; echo $?
/c/msys64/ucrt64/bin/mingw32-make.exe -j2 standalone-perf  > build_perf.log  2>&1; echo $?
/c/msys64/ucrt64/bin/mingw32-make.exe -j2 unit             > build_unit.log  2>&1; echo $?
```
All three exit 0, `914 tests, 0 failures` (plus whatever you added), and 0 NEW warnings: compare
`grep 'warning:' build_debug.log | sed -E 's/:[0-9]+:[0-9]+: warning:/: warning:/' | sort -u` against
`docs/runtime_port/endgame_b/b5_session/runs/warn_baseline_debug.txt` (same for perf). Then
`python docs/runtime_port/endgame_b/b4_session/castmetric.py dx11` — report your files' rows before and
after. `grep -n "self->vptr\|DX11_REGISTER_SLOT\|Dx11VtableAddress\|Orig<" <your files>` must be empty.
Foreground only: a 600000 ms Bash timeout and re-invoke; never a background monitor.

## Hard rules (each cost a re-run in an earlier step)
- Never run `reimpl-x64.exe`, `make sa-run`, `sa-deploy` or any game route. Never touch
  `C:/GOG Games/Banished`. Never `taskkill` by image name.
- No `git stash`, no amend / rebase / push, no sub-agents. Commit on your branch only; one commit per
  class is ideal, titled `B5 <BLOCK>: <Class> is a real subclass of Video::<Class> (...)`.
  End every commit message with the two attribution lines from your prompt.
- Builds `-j2`, in the foreground. Nothing in `scratchpad/` of the repo.
- No null guard added or dropped. A retail unguarded deref stays. The subclass constructor writes
  EXACTLY the factory's stores (the byte-vs-dword notes matter), no zero-initialising braces.
- Do not "fix" retail quirks ("do not fix" lists in each file). A difference from retail you find is
  a new row in `docs/known_port_issues.md` (continue the numbering), not a code change.
- Before your final build: `git merge main` (resolve a shared-header conflict by hunk, keeping both
  sides' classes), build again, then hand in.

## Hand-in (your final message)
Branch, commits (hashes), per-class: cast rows before/after, signature mismatches resolved (which side
was right, the evidence), the exact destructor release order you preserved, tests changed, anything
NOT converted and why, any retail difference recorded (row numbers). Keep it under 40 lines.
