# Decompilation Status & Type Inventory

Status key: `-` = not started, `~` = partially decompiled, `x` = fully decompiled & documented, `R` = fully reimplemented & hooked in-game

## Binary Summary

| Binary | Functions | Classes | Role | Status File |
|--------|-----------|---------|------|-------------|
| Application-x64.exe | 10,476 | ~67 ECS components + 61 UI classes + game logic | Game logic, UI controllers, ECS | [application.md](application.md) |
| Runtime-x64.dll | 15,290 | 100+ across 18 namespaces | Full engine runtime | [runtime.md](runtime.md) |
| VideoDX11-x64.dll | 576 (~86 rendering) | ~25 Video::*Data classes | DX11 renderer plugin | [videodx11.md](videodx11.md) |

---

## Reimplementation Dependencies & Blockers

All engine core loop classes (MainLoop, UpdateLoop, LoaderLoop, DialogLoaderLoopT, DialogLoopT) are now fully reimplemented (R). Resolved infrastructure: ExternalFactory (FindOrLoad, Clean, IsDerivedFrom), String lifecycle (ctor/dtor/copy), Filename::AdjustDelimiters.

All engine core classes promoted to R. Configuration fully reimplemented (8/10 methods, remaining 2 are lifecycle passthroughs). Modification at 5/8 full reimpl, remaining 3 passthroughs need deeper I/O subsystem support.

Resolved dependencies (used by Configuration and Modification reimplementations):
- `Map::Find` (0x18002ace0) — AVL tree lookup for command-line map
- `Map::Insert` (0x18002a090) — AVL tree insert for command-line map
- `ParseCommand` (0x1800c4a00) — extract "/command" token from CLI string
- `ParseValue` (0x1800c4930) — extract value token after command
- `Persistence::_singleton` (0x1801fc6b8) — key/value settings store
- `Persistence::SetBool` (0x1800b8380) — store bool via "prefix + suffix" key format
- `Persistence::SetInt` (0x1800b80c0) — store int via "prefix + suffix" key format
- `Filename::GetPathFilename` (0x1800c6740) — extract base filename, strips path/extension
- `ModArrayCopy` (0x180010110) — deep copy Mod array (0x80-byte elements)

Remaining passthrough wrappers (blocked on subsystem infrastructure):

| Passthrough Function | Dependency | What's Needed |
|---------------------|------------|---------------|
| `Modification::RefreshList` | `FileSystem`, `PackageFile`, `Persistence` | ~3646 bytes, scans filesystem for .pkm packages, rebuilds mod list |
| `Modification::SetupFileSystem` | `FileSystem`, `PackageFile`, `ExternalFactory` | ~2617 bytes, mounts mod packages into engine VFS |
| `Modification::EnableSavingSettings` | Calls `RefreshList` | Thin wrapper, blocked by RefreshList |
| `Configuration::Ctor` | Map init, field zeroing | 62 bytes, lifecycle — low priority |
| `Configuration::Dtor` | Map teardown, list cleanup | 49 bytes, lifecycle — low priority |

### Hook Infrastructure Notes

- **Total hooks**: 57 (all verified in-game)
- **Trampoline safety**: Some hooks have RIP-relative instructions in their prologues. Trampolines for these are broken (wrong displacement after relocation). Currently safe because no reimplementations call through trampolines. If A/B testing via trampoline is needed later, `InstallHook` must detect and relocate RIP-relative instructions.
- **Too-small functions**: Functions under 12 bytes cannot receive the inline jump patch. These are documented as "R (too small to hook)" with full reimplementation in source comments.
