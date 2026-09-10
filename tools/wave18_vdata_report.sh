#!/usr/bin/env bash
# tools/wave18_vdata_report.sh — pull lane VDATA's four screens out of a run pair.
#
# Reads the SNAPSHOTTED logs, never the game dir (a read after the release reads
# whichever lane ran next; that fooled a whole analysis on 2026-09-09).
set -u
OUT="${1:?usage: wave18_vdata_report.sh <outdir>}"

ROWS='ImageBuffer@Video|PixelProgram@Video|RenderState@Video|VertexLayout@Video|VertexProgram@Video|VideoProgram@Video|EdgeMesh@Graphics|GraphicsMesh@Graphics|ReflectionImageBuffer@Graphics|FrameCopyImageBuffer@Graphics'

for half in control armed; do
  for stream in error debug; do
    f="$OUT/${half}_standalone_${stream}.log"
    [ -f "$f" ] || continue
    echo "######################## $half / $stream ########################"

    echo "---- [W17Attend] control + the ten OnVideoError rows + the vtable"
    grep -a "W17Attend" "$f" | grep -aE "CONTROL|SUPPRESSED|$ROWS|\?\?_7ImageBuffer@Video|subject rows"

    echo "---- [RtMintShadow] the ImageBuffer vtable row and any armed typeinfo"
    grep -a "RtMintShadow" "$f" | grep -aE "ImageBuffer@Video@@6B@|$ROWS|clean:|problem|vtable rows|typeinfo rows"

    echo "---- [DeviceLost] did the forced device-lost event fire?"
    grep -a "DeviceLost" "$f"

    echo "---- [RtDiff] this lane's seven sites + OnVideoError"
    grep -a "RtDiff" "$f" | grep -aE "ImageBuffer::(Type|TypeName|GetFileVersion|Serialize|Reconstruct|Destruct|Construct)|OnVideoError|OnVideoRestart|===="

    echo "---- [RtSaveTrace]"
    grep -a "RtSaveTrace" "$f" | head -20

    echo "---- [RtMint] armed rows"
    grep -a "\[RtMint\]" "$f" | head -20

    echo "---- [RtResolve] misses"
    grep -a "RtResolve" "$f" | head -5
    echo
  done
done
