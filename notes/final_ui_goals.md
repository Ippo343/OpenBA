# Final UI/Dialog Deep Reimplementation Goals

Remaining work to eliminate ALL trampoline calls from dialog/UI reimpl files.
Once complete, every dialog/UI hook will run pure clean C++ with zero exe dependencies.

---

## Step 0 — DONE (2026-03-07) — Cleanup stale trampoline comments

Removed all stale "trampoline broken" / "no trampoline call" comments from 10 reimpl files.

---

## Step 1 — DONE (2026-03-07) — UIComponent (2 trampoline calls)

File: `engine/reimpl/uicomponent.cpp`

- [x] **UIComponent::Shutdown** — decompiled, clean C++
- [x] **UIComponent::Serialize** — decompiled, clean C++

---

## Step 2 — DONE (2026-03-07) — Description classes (52 trampoline calls across 14 files)

All description classes follow the same vtable pattern: Dtor, Serialize, Cleanup,
CreateElement, and/or CreateInPlace.

### CaptureBox/FrustumOutline Descriptions (10 calls, 2 files)

- [x] **CaptureBoxDescription** — Dtor, Serialize, Cleanup, CreateElement, CreateInPlace (5)
- [x] **FrustumOutlineDescription** — Dtor, Serialize, Cleanup, CreateElement, CreateInPlace (5)

### Create* Descriptions (24 calls, 7 files)

- [x] **CreateBridgeDescription** — Dtor, Serialize, Cleanup (3)
- [x] **CreateDragDescription** — Dtor, Serialize, Cleanup (3)
- [x] **CreateFencedDescription** — Dtor, Serialize, Cleanup (3)
- [x] **CreatePathDescription** — Dtor, Serialize, Cleanup, CreateInPlace (4)
- [x] **CreatePlacedDescription** — Dtor, Serialize, Cleanup (3)
- [x] **CreateRemovalDescription** — Dtor, Serialize, Cleanup, CreateInPlace (4)
- [x] **CreateRoadDescription** — Dtor, Serialize, Cleanup, CreateInPlace (4)

### Other Descriptions (18 calls, 5 files)

- [x] **EnvironmentDescription** — Dtor, Serialize, Cleanup (3)
- [x] **RadiusDescription** — Dtor, Serialize, Cleanup, CreateInPlace (4)
- [x] **SelectDescription** — Dtor, Serialize, Cleanup, CreateInPlace (4)
- [x] **ToolbarDescription** — Dtor, Serialize, Cleanup (3)
- [x] **ValueDescription** — Dtor, Serialize, Cleanup, CreateInPlace (4)

---

## Step 3 — DONE (2026-03-07) — Element classes (5 trampoline calls across 2 files)

- [x] **CaptureBoxElement** — Dtor, ComputeLayout, Draw (3)
- [x] **FrustumOutlineElement** — Dtor, ComputeLayout (2)

---

## Step 4 — Audit and eliminate ResolveAddr exe calls

Scan all dialog/UI code for remaining `ResolveAddr(Module::Exe, ...)` calls.
These are direct exe dependencies that bypass the engine_funcs layer. Each one
should be either:
- Moved to a proper function pointer in `engine/funcs/engine_funcs.h`
- Or reimplemented in clean C++ if the target function is simple enough

Files to audit:
- `code/Game/Dialog/*.cpp`
- `engine/reimpl/*.cpp` (dialog/UI files only)

---

## Step 5 — Replace exe-resolved engine_funcs with clean C++

Some `fn_*` function pointers in `engine/funcs/engine_funcs.h` resolve to **exe**
functions at runtime. These need to be decompiled and reimplemented as clean C++
so dialog/UI code has zero exe dependencies.

Runtime DLL (`fn_*` pointers resolved via `ResolveExport`/`ResolveOrdinal`) are fine
to keep — the DLL is a separate binary and will eventually be consumed via
`#include <dllheader.h>` style imports.

Audit `engine/funcs/engine_funcs.h` for `fn_*` pointers that resolve from
`Module::Exe` and are used by dialog/UI code. Prioritize by usage count.

---

## Summary

| Category | Files | Trampoline calls | Status |
|----------|-------|-----------------|--------|
| Step 0: Comment cleanup | 10 | 0 | DONE |
| Step 1: UIComponent | 1 | 2 | DONE |
| Step 2: Descriptions | 14 | 52 | DONE |
| Step 3: Elements | 2 | 5 | DONE |
| Step 4: ResolveAddr audit | ~30 | — | TODO |
| Step 5: Exe engine_funcs | TBD | — | TODO |
| **Steps 0-3 Total** | **27** | **59/59 eliminated** | **COMPLETE** |
