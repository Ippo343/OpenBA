// ============================================================================
// runtime/src/volume/rectangle.cpp — Volume::Rectangle
// ============================================================================
//
// Wave 1, Lane 3 of the Runtime-x64.dll port (docs/runtime_port/PLAN.md §4.2/§6).
// See runtime/types/volume/Volume.h for the layout and ABI note, and
// runtime/src/volume/box.cpp's header comment for why Rectangle (like Box)
// has no dedicated original source file in the recovered tree.
// ============================================================================

#include "../../types/volume/Volume.h"
#include "../../funcs/rt_port.h"

// ??0Rectangle@Volume@@QEAA@XZ   retail 0x18005ddd0
// Default ctor: empty rectangle (min = +1.0e8f, max = -1.0e8f), the same
// empty-sentinel convention as Volume::Box.
Rectangle* RtVolume_Rectangle_Ctor(Rectangle* self) {
    RT_DIFF_BEGIN("Volume::Rectangle::Rectangle (ctor)");
    self->min = { RtMathConst::kBigPos, RtMathConst::kBigPos };
    self->max = { RtMathConst::kBigNeg, RtMathConst::kBigNeg };
    if (rt_diff_on__) {
        Rectangle theirs{};
        RtOriginal<Rectangle*(*)(Rectangle*)>("??0Rectangle@Volume@@QEAA@XZ")(&theirs);
        RT_DIFF_BYTES(self, &theirs, sizeof(Rectangle));
    }
    return self;
}
