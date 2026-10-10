// ============================================================================
// src/engine/video/companion_operators.cpp — ENDGAME B5 (the dx11 seam), W0
// ============================================================================
//
// The class-scope `operator new` / `operator delete` of the twelve `Video::*Data`
// companion bases (src/engine/video/CompanionData.h), B5_RULES.md D2.
//
// WHY THEY EXIST. Each of the twelve is its OWN hierarchy root (no common base in
// retail: twelve separate `??_7XData@Video@@6B@` tables, twelve separate slot-0
// deleting destructors in the 0x180058590..0x180058b10 band). A root whose classes
// declare `virtual ~T()` needs a class-scope `operator delete`, or GCC's D0 would
// hand MemoryAllocator arena storage to the CRT `free()`
// (src/engine/object/root_operator_delete.cpp says the same for the five Object /
// Geometry / Stream / ReaderWriter / UpdateLoop roots). Retail's slot-0 bodies free
// with `?MemFree@MemoryAllocator@System@@SAXPEAX@Z` (0x1800c7ad0) on `flags & 1`.
//
// `operator new` is the other half of the same retail fact: VideoDX11's eleven
// factories (0x180005650..0x180005aa0) and the DeviceData export (0x1800011a0)
// each do `MemAlloc(sizeof, 4)` and then the base ctor, so `new VideoDX11::XData(owner)`
// through this operator allocates exactly what the shipped plugin allocated.
// Retail's `if (!p) return nullptr` after MemAlloc is not reproduced: a null here
// would be dereferenced by the next line either way (the arena never returns null
// in practice), and a nothrow overload would add a branch retail's callers do not
// take.
//
// Only the unsized forms are declared, so D0 calls exactly these functions.
// ============================================================================

#include <cstddef>
#include <cstdlib>

#include "src/engine/video/CompanionData.h"
#if !defined(BANISHED_UNIT_TEST)
#include "src/engine/system/MemoryAllocator.h"
#endif

namespace {

inline void* RootAlloc(size_t n) {
#if defined(BANISHED_UNIT_TEST)
    return std::malloc(n);
#else
    return System::MemoryAllocator::MemAlloc(n, 4);   // the factories' MemAlloc(size, 4)
#endif
}

inline void RootFree(void* p) {
#if defined(BANISHED_UNIT_TEST)
    std::free(p);
#else
    System::MemoryAllocator::MemFree(p);   // ?MemFree@MemoryAllocator@System@@SAXPEAX@Z
#endif
}

}  // namespace

#define RT_VIDEO_COMPANION_OPERATORS(CLASS)                                     \
    void* Video::CLASS::operator new(size_t n) { return RootAlloc(n); }          \
    void  Video::CLASS::operator delete(void* p) { RootFree(p); }

RT_VIDEO_COMPANION_OPERATORS(DeviceData)
RT_VIDEO_COMPANION_OPERATORS(ContextData)
RT_VIDEO_COMPANION_OPERATORS(ImageBufferData)
RT_VIDEO_COMPANION_OPERATORS(IndexBufferData)
RT_VIDEO_COMPANION_OPERATORS(VertexBufferData)
RT_VIDEO_COMPANION_OPERATORS(ConstantBufferData)
RT_VIDEO_COMPANION_OPERATORS(RenderBufferData)
RT_VIDEO_COMPANION_OPERATORS(VertexLayoutData)
RT_VIDEO_COMPANION_OPERATORS(VertexProgramData)
RT_VIDEO_COMPANION_OPERATORS(PixelProgramData)
RT_VIDEO_COMPANION_OPERATORS(RenderStateData)
RT_VIDEO_COMPANION_OPERATORS(VideoProgramData)

#undef RT_VIDEO_COMPANION_OPERATORS
