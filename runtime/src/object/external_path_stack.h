#pragma once
// ============================================================================
// runtime/src/object/external_path_stack.h — Wave 11 lane OBJEXT
//   `Object::ExternalFactory::PushPath` / `PopPath` and the four
//   `Collection::Array<PathStackEntry>` bodies they need.
// ============================================================================
//
// Shared between external_path_stack.cpp and its probe, for the reason
// array_internal.h gives: the probe must drive the SAME bodies the registered
// entry points drive, or it is testing a copy of the port.
// ============================================================================

#include <cstdint>

#include "../../types/object/External.h"
#include "../../types/system/Filename.h"

namespace RtObject {

using RtSystem::Filename;

// ?PushPath@ExternalFactory@Object@@QEAAXAEBVFilename@System@@@Z  0x1800bd0e0
void RtObject_ExternalFactory_PushPath(ExternalFactory* self, const Filename* path);

// ?PopPath@ExternalFactory@Object@@QEAAXXZ                        0x1800bd040
void RtObject_ExternalFactory_PopPath(ExternalFactory* self);

// The effectiveness control. Every counter is a branch taken INSIDE a ported
// body, not a call to one: a large `calls` with an untaken branch is the
// `worked=0` trap one level down (the diff oracle).
struct PathStackCounters {
    uint64_t pushCalls;
    uint64_t popCalls;
    uint64_t setSizeCalls;
    uint64_t removeAtCalls;
    uint64_t removeAtInRange;     // the shift-down loop was entered
    uint64_t removeAtPastEnd;     // index >= size: only the --size runs
    uint64_t removeLastCalls;
    uint64_t elemsShifted;        // elements moved down by RemoveAt
    uint64_t elemsDestroyed;      // elements released by the destroy-range
    int32_t  depthAfterPush;
    int32_t  depthAfterPop;
};
extern PathStackCounters g_psc;

}  // namespace RtObject

extern "C" {
// The four container entry points, spelled with their retail addresses so a
// reader can grep either way.
void  RtObject_PathStackSetSize_18002e220(void* arr, int n, const void* fill);
void  RtObject_PathStackDestroyRange_18002e8d0(void* ignoredThis, uint8_t* base, int count);
void* RtObject_PathStackRemoveLast_18002ccb0(void* arr, void* out);
void  RtObject_PathStackRemoveAt_18002de30(void* arr, int index);
}

// external_path_stack_probe.cpp
// `live` is the LIVE ExternalFactory singleton, or null. It is NOT optional
// decoration: part 2 of the probe drives PushPath/PopPath against retail's own
// object rather than a fake, because every row in this lane is a non-static
// member on that singleton and an oracle that builds its own factory measures
// an object the game never uses.

// runtime/src/object/rt_object_port.cpp calls this.
namespace RtObject { int RtObjectExternalPathStackRegister(); }
