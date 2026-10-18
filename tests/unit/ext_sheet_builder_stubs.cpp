// tests/unit/ext_sheet_builder_stubs.cpp -- what ext_sheet_builder_test.cpp's
// link needs and the host cannot run (README.md, "Tree sources"). Delete a stub
// here when its real file joins sources.cmake.
#include <cstdio>

#include "graphics/Material.h"
#include "object/External.h"
#include "system/String.h"
#include "video/ImageBuffer.h"

// sheet_builder.cpp's FillMaterialInstance reaches these three (renderer.cpp and
// wave27_lane_gfxbig.cpp, which stand on the renderer). The test never calls
// FillMaterialInstance; the bodies only have to link.
namespace Graphics {
void MaterialInstance::SetMaterial(Object::ExternalPtr<Material> material) { _material = material; }
void MaterialInstance::AddTexture(const System::String&, Object::ExternalPtr<Video::ImageBuffer>) {
    std::fprintf(stderr, "stub reached: MaterialInstance::AddTexture\n");
}
void MaterialInstance::BuildParameters() {}
}  // namespace Graphics
