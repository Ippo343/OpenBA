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
#include "../../runtime/types/io/Modification.h"
#include "../../runtime/types/save/SaveLoad.h"
#include "../../runtime/types/graphics/Scene.h"
#include "../../runtime/types/graphics/Animation.h"

#include <cstdio>
#include <cstdlib>

namespace RtObject {
RtSystem::Filename* RtSystem_Filename_GetPathExtension(const RtSystem::Filename* self,
                                                       RtSystem::Filename* ret) {
    (void)self;
    ret->data = nullptr;
    return ret;
}
}  // namespace RtObject

// ============================================================================
// B3R RT-Core: the element-op bodies runtime/src/collection/array.cpp and
// array_destroy.cpp now call BY NAME, whose translation units (io/, save/,
// graphics/) a host test does not link. Nothing in `make unit` drives the six
// Array specs that reach them (Array<Array<uint8>>, Array<Save::MetaData>,
// Array<Graphics::DrawGroup>, Array<Graphics::Animation>); before B3R the same
// calls went through the stub ResolveAddr, which answered null. Each stub
// aborts loudly rather than faking a body, so a future test that does drive
// one of those specs has to link the real translation unit.
// ============================================================================
namespace {
[[noreturn]] void UnlinkedBody(const char* what) {
    std::fprintf(stderr, "[unit] %s is not linked into make unit; see link_stubs.cpp.\n", what);
    std::abort();
}
}  // namespace

namespace RtIO {
void* RtColl_ArrayU8Assign_180011220(void*, const void*) { UnlinkedBody("RtColl_ArrayU8Assign_180011220"); }
}  // namespace RtIO
extern "C" void* RtSave_MetaData_CopyCtor(void*, const void*) { UnlinkedBody("RtSave_MetaData_CopyCtor"); }
extern "C" void* RtGraphics_DrawGroup_CopyCtor(void*, const void*) { UnlinkedBody("RtGraphics_DrawGroup_CopyCtor"); }
extern "C" void* RtGraphics_Animation_CopyCtor(void*, const void*) { UnlinkedBody("RtGraphics_Animation_CopyCtor"); }
extern "C" void  RtGraphics_Animation_Dtor(void*) { UnlinkedBody("RtGraphics_Animation_Dtor"); }
Save::SaveLoad::SaveInfo::~SaveInfo() { UnlinkedBody("Save::SaveLoad::SaveInfo::~SaveInfo"); }
