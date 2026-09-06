#!/usr/bin/env python3
"""Regenerate the runtime/src/<dir>/README.md skeleton for the Runtime port.

Run from the repo root. Idempotent: it recreates each directory README from
docs/runtime_port/original_source_tree.txt (the source tree recovered from
Runtime-x64-profile.dll's debug strings) and re-lists whatever is ported there
now, so a README never drifts from the files beside it.

    tools/ghidra_mcp/.venv/Scripts/python.exe tools/runtime_src_skeleton.py
"""
import os, collections

SEP = chr(92)  # backslash, kept out of the literal so shell heredocs can't eat it

lines = [l.strip() for l in open('docs/runtime_port/original_source_tree.txt', encoding='utf-8') if l.strip()]
byd = collections.OrderedDict()
for l in lines:
    d, f = l.split(SEP, 1)
    # Two entries in the recovered tree are lowercase include paths ("io PackageFile.h",
    # "video VideoDevice_win.h"); fold them into their real directory.
    key = d.lower()
    byd.setdefault(key, {"name": d if d[0].isupper() else d.upper(), "files": []})
    if d[0].isupper():
        byd[key]["name"] = d
    byd[key]["files"].append(f)

# Namespaces with functions in the DLL but no recovered source path: their code
# carries no assert text or log string for `strings` to pick up. Real directories
# all the same (PLAN.md §3.1 counts Math at 228 functions, Debug at 8).
for extra in ("Math", "Debug"):
    byd.setdefault(extra.lower(), {"name": extra, "files": []})

made = []
for _key, entry in byd.items():
    d, files = entry["name"], entry["files"]
    low = d.lower()
    path = os.path.join('runtime', 'src', low)
    os.makedirs(path, exist_ok=True)
    cpps = [f for f in files if f.endswith('.cpp')]
    hs = [f for f in files if f.endswith('.h')]
    existing = sorted(x for x in os.listdir(path) if x.endswith(('.cpp', '.h')))
    with open(os.path.join(path, 'README.md'), 'w', encoding='utf-8') as fh:
        fh.write("# runtime/src/%s — original `code%sRuntime%s%s%s`\n\n" % (low, SEP, SEP, d, SEP))
        fh.write("Port target for the Runtime-x64.dll `%s` directory. The file list below is the\n"
                 "ORIGINAL source tree recovered from `Runtime-x64-profile.dll` debug strings\n"
                 "(`docs/runtime_port/original_source_tree.txt`); mirror these names here, lowercased,\n"
                 "as `runtime/src/%s/<file>.cpp`, with binary-compatible types in\n"
                 "`runtime/types/%s/<Class>.h` (`static_assert` on every field offset).\n\n"
                 % (d, low, low))
        fh.write("A `_win` file is a PLATFORM SEAM (PLAN.md §4.1 decision 1): keep `<windows.h>` out of\n"
                 "every other file in this directory so a `_posix`/`_sdl` twin can be written later.\n"
                 "A `_compile` file is the offline asset compiler and is likely absent from retail —\n"
                 "confirm against the retail DLL before porting one.\n\n")
        fh.write("## Original files (%d cpp, %d h)\n\n" % (len(cpps), len(hs)))
        for f in files:
            fh.write("- `%s`\n" % f)
        fh.write("\n## Ported here\n\n")
        if existing:
            for f in existing:
                fh.write("- `%s`\n" % f)
        else:
            fh.write("_nothing yet_\n")
    made.append((low, len(cpps), len(hs), len(existing)))

print("%d directories" % len(made))
for m in made:
    print("  %-14s original %2d cpp / %2d h   ported %d" % m)
