// ============================================================================
// tests/unit/link_stubs.cpp — the one symbol the classes under test reference
// but do not own (ENDGAME B.3.1 step 1)
// ============================================================================
// `System::Filename::GetPathExtension` forwards to the C body that still lives
// in runtime/src/object/external_path_win.cpp, wrapped in that lane's path
// counters. Linking that file into a host test would drag in ExternalFactory,
// the package system and Win32; the member is therefore NOT covered by
// filename_test.cpp, and this stub only satisfies the linker.
//
// It goes away with the forward, when Object::ExternalFactory is converted and
// the body moves onto the class.
// ============================================================================
#include "../../runtime/types/system/Filename.h"

namespace RtObject {
RtSystem::Filename* RtSystem_Filename_GetPathExtension(const RtSystem::Filename* self,
                                                       RtSystem::Filename* ret) {
    (void)self;
    ret->data = nullptr;
    return ret;
}
}  // namespace RtObject
