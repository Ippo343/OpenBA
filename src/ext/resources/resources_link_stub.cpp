// ============================================================================
// src/ext/resources/resources_link_stub.cpp -- TEMPORARY, block RSCFONT only
// ============================================================================
// NOT RETAIL. Stand-in definitions of Resources.h's four entry points so the
// RSCFONT branch links before block RSCTEXT's resources.cpp (the strong
// definitions) lands. The coordinator DELETES this file at merge (MinGW
// resolves a weak definition to "undefined" in a static link, so these are strong).
// Register answers false (no table), Provide null, Publish returns its
// argument; SetPath is real (the object's own _path, what the factory keys).
// ============================================================================
#include "ext/resources/Resources.h"

#include "object/External.h"
#include "system/Filename.h"

namespace Ext {
namespace Resources {

bool Register(const char*, ProviderFn, void*) { return false; }
Object::External* Provide(const char*) { return nullptr; }
void SetPath(Object::External* obj, const char* path) {
    if (obj != nullptr) obj->_path = System::Filename(path);
}
Object::External* Publish(Object::External* obj) { return obj; }

}  // namespace Resources
}  // namespace Ext
