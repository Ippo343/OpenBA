# src/ — code orientation

Read `../CLAUDE.md` and `../AGENTS.md` first. This file records invariants that are easy to break
and expensive to rediscover; `CLAUDE.md` remains the authority on the porting workflow.

- `src/engine` is the ported Runtime-x64.dll; `src/game` is Banished game logic; `src/render/dx11`
  replaces VideoDX11-x64.dll; `src/ext` is the **only** home of non-faithful behaviour and
  extensions; `src/app` is the launcher/boot; `src/diag` is harnesses and diagnostics.
- The platform build matrix is not symmetric: Windows compiles `platform/win` + `platform/common` +
  `platform/sdl` + `diag/win`; Linux compiles `platform/posix` + `common` + `sdl` + `diag/posix`.
  Never put a Win32 call in `common`, `game`, or `engine`.
- `src/ext/registry.{h,cpp}` is the extension registry (extension rows, locks, profiles) — not the
  Windows registry. `src/engine/io/persistence_settings.cpp` notes describe retail's old registry
  store; runtime settings are a UTF-8 file (`Documents/Banished/settings.cfg`) with a one-time
  read-only registry import. Do not reintroduce registry writes.
- `src/game/**/exe_bodies.h` (and the equivalents under `Action/`, `Work/`, `Player/`, `Tutorial/`)
  are declaration/retail-address maps for bodies reached by old address-registry rows. They are not
  stable public headers. `// PORT GAP` means the body is not ported, not that a null check is needed.
- `src/ext/ui/**/*.rsc` are project-authored text resources embedded into the exe at build time by
  `tools/ext_rsc_embed.py`; they are not retail assets. Never add retail `.rsc`, `.pkg`, textures,
  models, sounds, or shader copies.
- `src/render/dx11/ao/assao` and `../third_party/{imgui,wmadec}` are vendored upstream sources. Do
  not reformat them; their warning suppressions are per-file in `CMakeLists.txt`.
- Harness code under `src/diag` (top-level plus `diag/win` or `diag/posix`) is compiled into the
  shipping exe; only the ImGui tweak overlay is debug-flavour-only, and `src/diag/font_dump` is a
  separate debug-only target. Expect crash handlers, debug registers, thread snapshots, process-memory
  reads, and input injection to be present in the binary and to look AV-suspicious. They are local
  dev tools, not malware.

## Invariants

- ABI-pinned classes: fixed-width fields only (`uint32_t`, `int64_t`, pointers, float/double, bool,
  enums, other pinned classes). `static_assert` every `sizeof`/`offsetof`; GCC Itanium layout must
  reproduce retail MSVC offsets. No `long`, `wchar_t`, `size_t`, bit-fields, or native handles.
  `tools/abi_audit.py` enforces the rule.
- Behaviour fidelity: retail's unguarded dereferences are load-bearing. Find the upstream null
  rather than adding a guard (CLAUDE.md hard rule). Guard only where the standalone genuinely
  differs, and document why.
- All environment flags are `BANISHED_*`, inventoried in `src/app/options.h`, read once by
  `OptionsInit()` and consumed through `Opt()`. No new `getenv` calls.
- Perf/release force-include `src/app/log_strip.h`; qualified `std::printf` breaks those builds.
  Every TU also gets `src/app/one_write_stdio.h`, which macro-wraps the `fprintf` family on MinGW.
- No network, telemetry, persistence, or bundled binaries. Keep this baseline unless the user
  explicitly changes it.
