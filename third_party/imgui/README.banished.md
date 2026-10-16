# third_party/imgui -- Dear ImGui, vendored for the debug tweak overlay

Dear ImGui (Omar Cornut and contributors, MIT, `LICENSE.txt` beside this file) drives the
debug-only tweak overlay `src/diag/tweak_overlay.{h,cpp}` (block TWEAKUI,
`docs/reference/tweak_overlay.md`). It is compiled into the **debug flavour only** (CMake option
`BANISHED_TWEAK_OVERLAY`, ON for `debug` on Windows, OFF for og / perf / release and Linux); no
other flavour sees a byte of it.

## Source

| | |
|---|---|
| repository | https://github.com/ocornut/imgui |
| tag | **`v1.92.9b`** (the latest release on 2026-10-16; a lightweight tag) |
| commit | `f1cc2ae15e53a861a874c3034aae6798fde194ab` (2026-07-31, "Version 1.92.9b") |
| fetched | `git clone --depth 1 --branch v1.92.9b https://github.com/ocornut/imgui.git` into a fresh empty scratch directory (`git rev-parse HEAD` = the commit above), then the files below copied in |

## What is here (and nothing else)

The core plus two backends, **unmodified** (sha256 of each, equal to the upstream tag's file):

```
f20418b409e53c8c9f4e90917ff395554a60320d4dfbf833da89b339cad8628a  LICENSE.txt
58ca417d54cc576fb89e69b24880b9d2ad4b380b2acdb149386c417ff36d7cdc  imconfig.h
01cd8afb847fe33f6d3386eca9fa057bb87783364bf3bd13b0fc582d1f3fb5ff  imgui.cpp
9e172dc5b03d5b6a7795ddd24faae0912641f0a92c1c8b1d5423f35fad1b927a  imgui.h
ce39607b566fac1123cd8089ce34f4a4cbe8c95fa2f78509aff9a7335333c438  imgui_draw.cpp
59e92be94d1a1443c68ad65d6c70ae713d234da97926564b45585d01ff40b314  imgui_internal.h
a0e7e3e874abb06da252cb8bf2b7ac87941aa176eeec51d9d81335d937851e0c  imgui_tables.cpp
1938176e90bb32ed4e3a59ef269239a95a191615bc0251f48ca6e5534e3c9642  imgui_widgets.cpp
7f9bffb8ac5e79d20787fdc20d29d29f7474dca673f64d4a9a8a9f9373e40972  imstb_rectpack.h
24a8db00354af8f4057417841635a1b6dfd8986f1608336fe6f10d6fd9769aaa  imstb_textedit.h
a4131c7a2e324c0625250830af6976adb5732b453f0fc6e8bc5ad70065bc2b79  imstb_truetype.h
76c9f8afc82b9118e6433261a9e923304a075dfb95f2b1c8a33e4dcc17aa6bb5  backends/imgui_impl_sdl3.cpp
1a7fbbc41af9c093da895ee2f48c5fcee0350303c18e4ab6ce7d3f265ef42b29  backends/imgui_impl_sdl3.h
4e946441228a562aebc2e502d35fcb2a17457ac30ba10535f402f65bfbc39add  backends/imgui_impl_dx11.cpp
7702b66d3549af3b890e0fb4990b7c446d9a32778bd2970e81fc66838bc25e2a  backends/imgui_impl_dx11.h
```

Not copied: `imgui_demo.cpp`, `misc/`, `examples/`, `docs/`, every other backend. Nothing of ours is
edited inside this directory: the integration choices (no `imgui.ini`, no cursor changes, no IME /
clipboard calls off the main thread, our own frame setup instead of `ImGui_ImplSDL3_NewFrame`) are made
from `src/diag/tweak_overlay.cpp` through ImGui's public API. Warnings in these files are silenced per
file in `CMakeLists.txt`, as for the ASSAO copy.

The DX11 backend compiles its two tiny shaders at run time with `D3DCompile`, so the debug exe imports
`d3dcompiler_47.dll` (part of Windows 10+). No other flavour links it.

## Moving the pin

Clone the new tag into an empty directory, check `git rev-parse HEAD` against the tag on GitHub, copy the
same fifteen files over these, update the table and the hashes, rebuild debug and check the overlay
(`docs/reference/tweak_overlay.md`). The backends' API moved in 1.92 (textures); a pin move across a
minor version may need `tweak_overlay.cpp` touched.
