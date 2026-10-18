// ============================================================================
// src/ext/resources/resources_weak_stub.cpp -- TEMPORARY, block RSCFONT only
// ============================================================================
// NOT RETAIL. Weak definitions of Resources.h's four entry points so the
// RSCFONT branch links before block RSCTEXT's resources.cpp (the strong
// definitions) lands. The coordinator DELETES this file at merge; GCC's
// weak symbols make the strong ones win if both are ever in one link.
// Register answers false (no table), Provide null, Publish returns its
// argument; SetPath is real (the object's own _path, what the factory keys).
// ============================================================================
#include "ext/resources/Resources.h"

#include "object/External.h"
#include "system/Filename.h"

namespace Ext {
namespace Resources {

__attribute__((weak)) bool Register(const char*, ProviderFn, void*) { return false; }
__attribute__((weak)) Object::External* Provide(const char*) { return nullptr; }
__attribute__((weak)) void SetPath(Object::External* obj, const char* path) {
    if (obj != nullptr) obj->_path = System::Filename(path);
}
__attribute__((weak)) Object::External* Publish(Object::External* obj) { return obj; }

}  // namespace Resources
}  // namespace Ext
