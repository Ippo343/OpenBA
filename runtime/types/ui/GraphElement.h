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
//   AddGraph(color)     0x180040620  HELD BACK -- see WAVE7_LANE_E.md sec2.
//                                    Grows the graph-series Array at this+0xf0
//                                    (element stride 0x18) by one via the
//                                    non-exported FUN_1800417d0, then writes
//                                    `color` into the new element's first
//                                    4 bytes.
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

constexpr int kGraphsData  = 0xF0;  // Collection::Array<GraphSeries> .data (GraphSeries stride 0x18, NOT reversed -- only touched via the retail helper)
constexpr int kGraphsCount = 0xF8;  // Collection::Array<GraphSeries> .size (int32_t)

} } // namespace UI::GraphElementOffsets
