// src/ext/fixes/tavern_counter.h -- fix.tavern_counter (docs/retail_bugs.md section 3).
//
// HappinessComponent::OnMessage (retail 0x14004a780) spends one stored unit with
// StorageComponent::SubtractItem (0x14012d150, at 0x14004a8da) and then reads the
// material back through the SAME row address (`MOV RDX,[R14]` at 0x14004a8e6) for the
// category it counts as consumed. SubtractItem compacts the array when the row reaches
// zero, so the row then holds the next item (or the stale tail) and the wrong category
// is counted. The lever (retail_bugs.md): "load the material before the call".
//
// Header-only and templated over the row and the subtract so the unit test
// (tests/unit/fix_tavern_counter_test.cpp) runs it without the world.
#pragma once

namespace Ext { namespace Fixes {

// Spend one unit of `row`'s material through `subtract(material)` and return the
// material whose category the caller counts. `fixed` is Fix().tavern_counter, read once
// by the caller.
template <class Row, class Subtract>
inline auto TavernSpendOne(Row* row, bool fixed, Subtract subtract) -> decltype(row->_resource) {
    if (fixed) {
        auto spent = row->_resource;   // loaded BEFORE the subtract
        subtract(spent);
        return spent;
    } else {
        subtract(row->_resource);      // retail: subtract, then re-read the (maybe compacted) row
        return row->_resource;
    }
}

} }  // namespace Ext::Fixes
