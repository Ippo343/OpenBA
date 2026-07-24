# dx11_state_diff — per-draw state comparison for the VideoDX11 port

Compares what our reimplemented DX11 renderer does, draw by draw, against the
shipped `VideoDX11-x64.dll`. Written for the equality pass documented in
[`dx11/README.md`](../../dx11/README.md) §"Per-draw pipeline-state equality".

## The problem these tools work around

Banished's simulation is **not frame-deterministic even with the world seed
pinned**. Two runs of the *same* renderer draw different numbers of things, so
draw-N-vs-draw-N comparison is meaningless and so is any raw count.

Two consequences shape everything here:

1. **Nothing is judged against intuition.** Every comparison is run twice — once
   between two runs of the same renderer (the control) and once ours-vs-shipped
   (the test). Only fields the control proves stable count as evidence. A metric
   that flags the control is measuring content, not correctness.
2. **Resource handles are useless across processes.** RenderDoc `ResourceId`s are
   allocation-order counters. Every resource reference is replaced by a
   content-derived key: shaders by sha1 of their DXBC, textures by creation desc
   plus sha1 of mip 0, buffers by byte length.

## Scripts

| | |
|---|---|
| `capture.sh <tag> shipped\|standalone [frame]` | Capture one in-game frame under RenderDoc. Verifies from the log which renderer actually ran — `Video::Device` answers a failed DX11 bring-up by silently rendering on DX9, which otherwise looks like success. Always passes `--opt-ref-all-resources`. |
| `dump_state.py <capture.rdc> -o <out.jsonl>` | Walk every draw, record full D3D11 pipeline state with resource identity canonicalised. One JSON object per draw. |
| `compare_state.py <a.jsonl> <b.jsonl>` | Pipeline-configuration sets, alignment-free pipeline signatures, texture-derived invariants, and an aligned per-field diff. |
| `compare_resources.py <a.rdc> <b.rdc>` | Live resource sets: type histogram, textures by desc + content hash, buffers by byte length. Both captures must have used `--opt-ref-all-resources`. |

`capture.sh` does **not** verify resolution. Check the viewport before trusting a
capture — the intermittent window-size bug produced one run at 2560×1377 while
every other got 1280×720, and comparing it would have compared two resolutions.

## Usage

```bash
bash tools/dx11_state_diff/capture.sh shipped1 shipped 150
bash tools/dx11_state_diff/capture.sh sa1      standalone 150

PY=tools/renderdoc_mcp/.venv/Scripts/python.exe
$PY tools/dx11_state_diff/dump_state.py "C:/GOG Games/Banished/rdoc_ab/sa1.rdc" \
    -o notes/dx11_state/sa1.jsonl
$PY tools/dx11_state_diff/compare_state.py notes/dx11_state/sa1.jsonl \
    notes/dx11_state/shipped1.jsonl --label-a sa1 --label-b shipped1
```

The `renderdoc` Python module comes from `tools/renderdoc_mcp/runtime`
(`renderdoc.pyd`, Python 3.10); override with `RENDERDOC_MODULE_PATH`. Use the
`tools/renderdoc_mcp/.venv` interpreter — the module is built for 3.10.

Capturing is slow (minutes per run): the standalone build logs every window
message with an `fflush`, so reaching in-game frame 150 takes a while. Raise
`CAPTURE_WAIT_ITERS` if a run times out.

## Reading the output

**Pipeline signatures** are the primary result — they involve no alignment. The
signature deliberately excludes everything D3D11 derives from the bound texture
(`sampler.maxLOD`, `srv.numMips`, `srv.numSlices`); those are checked separately
against the bound texture's actual description, which claims more than set
equality would. Leaving any of them in manufactures findings — it flags the
control as well as the test.

A signature present on one side only is classified, not assumed: if that shader
pair never drew on the other side it is content; anything else prints as
`UNEXPLAINED` and is a real finding.

**The aligned per-field table** is secondary, because alignment is fuzzy. Read it
as two columns: fields at 0.0% in the control are the ones whose 0.0% in the test
means something. `cbContents` is 100% noise — per-object transforms genuinely
differ between runs — so constant-buffer *contents* are not verified by any of
this, only their bindings.
