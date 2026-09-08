#pragma once
// UI::GraphElement -- offsets used by Wave 7 lane E.
//
// Evidence (Runtime-x64.dll addresses):
//   RemoveGraphs()      0x180040610  tail-calls FUN_1800416d0(this+0xf0):
//                                    destroys every element of the Array at
//                                    this+0xf0, frees its buffer under the
//                                    allocator lock, zeroes {data,size,cap}.
//                                    NOT an exported name -- called by address
//                                    (ResolveAddr), same idiom as
//                                    runtime/src/graphics/scene.cpp's
//                                    RtArrayPtrSetSize / RtFreeArrayPtr.
//   AddGraph(color)     0x180040620  PORTED by Wave 9 lane UIW. Grows the
//                                    graph-series Array at this+0xf0 (element
//                                    stride 0x18) by one via 0x1800417d0 -- now
//                                    ours too (runtime/src/collection/array.cpp,
//                                    `kSpec_GraphSeries`) -- then writes `color`
//                                    into the new element's first 4 bytes.
//                                    Wave 7 lane E and Wave 9 lane UIF both held
//                                    it back because 0x1800417d0 was unported;
//                                    that is what changed, not the reading.
//   AddGraphPoint(g,v)  0x180040690  PORTED by Wave 9 lane UIF (the hold-back
//                                    below is the ORIGINAL Wave 7 note, kept
//                                    because its mechanism was right; what
//                                    changed is only that 0x180023830 became
//                                    ours in lane Q, so the row now closes).
//                                    Appends to a per-series
//                                    Array<int32_t> living inside graph series
//                                    `g` at (seriesArray[g] + 0x08), via the
//                                    exported-by-address 0x180023830
//                                    (Array<uint32_t>::SetSize, the SAME
//                                    instantiation runtime/src/graphics/
//                                    scene.cpp's RtArrayU32SetSize already
//                                    calls for Graphics::Scene -- shared
//                                    template instantiation, independent
//                                    confirmation of the address).

#include <cstdint>

namespace UI { namespace GraphElementOffsets {

constexpr int kGraphsData  = 0xF0;  // Collection::Array<GraphSeries> .data
constexpr int kGraphsCount = 0xF8;  // Collection::Array<GraphSeries> .size (int32_t)

// ---------------------------------------------------------------------------
// GraphSeries -- REVERSED by Wave 9 lane UIW (this header previously said
// "stride 0x18, NOT reversed -- only touched via the retail helper", which was
// true until AddGraph's element-assign had to be written out).
//
// Evidence: the assign arm of 0x1800417d0 at 0x180041890..0x1800418b1 writes
// exactly the colour dword and the nested Array header, and the destroy arm
// 0x1800419b0 destroys an Array at record+0x08 with stride 0x18.
//
//   +0x00  colour    uint32   AddGraph's argument; the ONLY 4 bytes AddGraph
//                             itself writes after the append.
//   +0x04  (hole)    4 bytes  NEVER WRITTEN BY ANY ARM. Not padding this lane
//                             chose -- padding retail leaves. It is declared as
//                             kSpec_GraphSeries's assign-unwritten window so no
//                             oracle compares it
//                             (reference_oracle_must_not_compare_unwritten_bytes.md).
//   +0x08  points    Collection::Array<int32_t>  data +0x08 / size +0x10 /
//                             capacity +0x14. This is the array
//                             GraphElement::AddGraphPoint appends into
//                             (widgets_layout.cpp reads it as "record + 8"),
//                             reached there by the SAME arithmetic -- two
//                             independent bodies agreeing on the offset.
// ---------------------------------------------------------------------------
constexpr int kSeriesStride      = 0x18;
constexpr int kSeriesColour      = 0x00;
constexpr int kSeriesPointsArray = 0x08;

} } // namespace UI::GraphElementOffsets
