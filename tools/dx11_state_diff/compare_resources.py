#!/usr/bin/env python3
"""Compare the live D3D11 resource sets of two captures.

The frame-level A/B pass could never settle whether the buffer count meant
anything: it moved between 94 and 99 across nine captures on both sides, because
RenderDoc lists only the resources a frame happened to *reference*. Capturing
with `--opt-ref-all-resources` removes that dependence — the list becomes every
live resource, i.e. everything the renderer actually created and still holds.

Both captures must have been taken with that option or the comparison is
meaningless; capture.sh always passes it.

What is compared:
  resource types   histogram over ResourceType (Buffer / Texture / Shader /
                   state objects / views), i.e. how many D3D11 objects of each
                   kind the renderer holds
  textures         multiset of creation descs, with a content hash for textures
                   the GPU does not write (render targets are identified by
                   desc + role, since their contents are the frame itself)
  buffers          multiset of byte lengths — this is the constant-buffer
                   "ladder" the port allocates, and the thing the old buffer
                   count was a noisy proxy for

Usage:
    python compare_resources.py <a.rdc> <b.rdc> [--label-a A --label-b B]
"""

from __future__ import annotations

import argparse
import hashlib
import os
import sys
from collections import Counter

_DEFAULT_MODULE = r"C:\code\GIT\untitled_rev_eng_project\tools\renderdoc_mcp\runtime"


def load_renderdoc():
    path = os.environ.get("RENDERDOC_MODULE_PATH", _DEFAULT_MODULE)
    if path and path not in sys.path:
        sys.path.insert(0, path)
    if sys.platform == "win32" and hasattr(os, "add_dll_directory") and os.path.isdir(path):
        os.add_dll_directory(path)
    import renderdoc  # noqa: E402

    return renderdoc


rd = load_renderdoc()


def fmt_str(f):
    try:
        return str(f.Name())
    except Exception:
        return str(f)


def enum(v) -> str:
    s = str(v)
    return s.split(".")[-1] if "." in s else s


def survey(path: str) -> dict:
    cap = rd.OpenCaptureFile()
    if cap.OpenFile(path, "", None) != rd.ResultCode.Succeeded:
        raise SystemExit(f"cannot open {path}")
    result, ctrl = cap.OpenCapture(rd.ReplayOptions(), None)
    if result != rd.ResultCode.Succeeded:
        raise SystemExit(f"cannot replay {path}: {result}")

    target_flags = 0
    for flag in ("ColorTarget", "DepthTarget"):
        f = getattr(rd.TextureCategory, flag, None)
        if f is not None:
            target_flags |= int(f)

    types = Counter(enum(r.type) for r in ctrl.GetResources())

    textures = Counter()
    for t in ctrl.GetTextures():
        desc = (f"{enum(t.type)}:{t.width}x{t.height}x{t.depth}:m{t.mips}"
                f":a{t.arraysize}:{fmt_str(t.format)}:ms{t.msSamp}x{t.msQual}")
        if int(t.creationFlags) & target_flags:
            textures[f"{desc}:rendertarget"] += 1
        else:
            try:
                data = ctrl.GetTextureData(t.resourceId, rd.Subresource(0, 0, 0))
                h = hashlib.sha1(bytes(data)).hexdigest()[:16]
            except Exception:
                h = "nohash"
            textures[f"{desc}:{h}"] += 1

    buffers = Counter(b.length for b in ctrl.GetBuffers())

    out = {
        "path": path,
        "types": types,
        "textures": textures,
        "buffers": buffers,
        "nTextures": len(ctrl.GetTextures()),
        "nBuffers": len(ctrl.GetBuffers()),
        "nResources": len(ctrl.GetResources()),
        "texBytes": sum(t.byteSize for t in ctrl.GetTextures()),
        "bufBytes": sum(b.length for b in ctrl.GetBuffers()),
    }
    ctrl.Shutdown()
    cap.Shutdown()
    return out


def diff_counter(a: Counter, b: Counter, la: str, lb: str, title: str, limit: int = 30):
    print(f"\n-- {title} --")
    keys = sorted(set(a) | set(b), key=lambda k: str(k))
    only_a = [k for k in keys if k not in b]
    only_b = [k for k in keys if k not in a]
    differing = [k for k in keys if k in a and k in b and a[k] != b[k]]
    same = len(keys) - len(only_a) - len(only_b) - len(differing)

    print(f"{len(keys)} distinct keys: {same} identical, {len(differing)} count-differs, "
          f"only-{la} {len(only_a)}, only-{lb} {len(only_b)}")
    shown = 0
    for k in only_a:
        print(f"  ONLY {la} (x{a[k]}): {k}")
        shown += 1
        if shown >= limit:
            print("  ...")
            return
    for k in only_b:
        print(f"  ONLY {lb} (x{b[k]}): {k}")
        shown += 1
        if shown >= limit:
            print("  ...")
            return
    for k in differing:
        print(f"  COUNT {k}: {la}={a[k]} {lb}={b[k]}")
        shown += 1
        if shown >= limit:
            print("  ...")
            return
    if not (only_a or only_b or differing):
        print(f"  IDENTICAL")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("a")
    ap.add_argument("b")
    ap.add_argument("--label-a", default=None)
    ap.add_argument("--label-b", default=None)
    args = ap.parse_args()

    la = args.label_a or os.path.basename(args.a)
    lb = args.label_b or os.path.basename(args.b)

    rd.InitialiseReplay(rd.GlobalEnvironment(), [])
    A = survey(args.a)
    B = survey(args.b)

    print(f"=== resources: {la}  vs  {lb} ===")
    print(f"{'':<14}{la:>22}{lb:>22}")
    for key, label in (("nResources", "resources"), ("nTextures", "textures"),
                       ("nBuffers", "buffers")):
        print(f"{label:<14}{A[key]:>22}{B[key]:>22}")
    print(f"{'texture bytes':<14}{A['texBytes']:>22,}{B['texBytes']:>22,}")
    print(f"{'buffer bytes':<14}{A['bufBytes']:>22,}{B['bufBytes']:>22,}")

    diff_counter(A["types"], B["types"], la, lb, "resource types")
    diff_counter(A["textures"], B["textures"], la, lb, "textures (desc + content hash)")
    diff_counter(A["buffers"], B["buffers"], la, lb, "buffers (byte length)")

    rd.ShutdownReplay()
    return 0


if __name__ == "__main__":
    sys.exit(main())
