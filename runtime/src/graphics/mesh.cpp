// ============================================================================
// runtime/src/graphics/mesh.cpp — Wave 6 lane E
//   Graphics::MeshInstance + GraphicsMesh + EdgeMesh + EdgeMeshInstance
// ============================================================================
//
// Ports the Graphics mesh-family exports our tree calls (docs/runtime_port/
// PLAN.md §4.2 wave 6, §6). Layouts and the evidence for every offset are in
// runtime/types/graphics/Mesh.h; the per-function write-up, the oracle
// arguments and everything this lane could NOT verify are in
// docs/runtime_port/WAVE6_LANE_E.md.
//
//   | Export                                  | Retail      | Ord  | Oracle          |
//   |-----------------------------------------|-------------|------|-----------------|
//   | ??0MeshInstance@…@XZ                    | 0x1800fffc0 |  275 | FULL (scratch)  |
//   | ??0MeshInstance@…@AEBV01@@Z             | 0x1800170c0 |  274 | post-state      |
//   | ??1MeshInstance@…@XZ                    | 0x180017060 |  632 | post-state      |
//   | ?GetMaterialList@GraphicsMesh@…H@Z      | 0x1800126c0 | 2768 | FULL            |
//   | ?GetNodeIndexFromName@GraphicsMesh@…    | 0x1800fee90 | 2812 | FULL            |
//   | ?GetNodeToWorld@MeshInstance@…          | 0x180016a40 | 2813 | FULL (scratch)  |
//   | ?IsAnimating@MeshInstance@…             | 0x1800ff7d0 | 3097 | FULL            |
//   | ?PlayAnimation@MeshInstance@…           | 0x1800ff840 | 3421 | post-state      |
//   | ?RootAnimationOffset@MeshInstance@…     | 0x180016ef0 | 3589 | FULL (scratch)  |
//   | ?SetMaterialInstance@MeshInstance@…     | 0x1800ffe80 | 3908 | FULL (replay)   |
//   | ?SetMesh@MeshInstance@…                 | 0x1800ffea0 | 3915 | FULL (replay)   |
//   | ?UpdateHierarchy@MeshInstance@…         | 0x1800ff910 | 4518 | FULL (replay)   |
//   | ??0EdgeMesh@Graphics@@QEAA@XZ           | 0x1800a2830 |  161 | post-state      |
//   | ??0TriangleVertex@EdgeMesh@…            | 0x1800a2f50 |  462 | FULL (scratch)  |
//   | ??0EdgeVertex@EdgeMesh@…                | 0x1800a2df0 |  164 | FULL (scratch)  |
//   | ?Set@EdgeMesh@Graphics@@…               | 0x1800a23e0 | 3813 | post-state      |
//   | ?SetMaterials@EdgeMesh@Graphics@@…      | 0x1800a2760 | 3910 | FULL (replay)   |
//   | ??0EdgeMeshInstance@…@XZ                | 0x1800a3ab0 |  163 | FULL (scratch)  |
//   | ??0EdgeMeshInstance@…@AEBV01@@Z         | 0x1800174f0 |  162 | post-state      |
//   | ?SetMesh@EdgeMeshInstance@…             | 0x1800a3a20 | 3914 | FULL (replay)   |
//
// All twenty-four names in this lane's scope (the twenty above plus the four
// held back at the bottom of this file) were checked against the shipped export
// directory of C:\GOG Games\Banished\Runtime-x64.dll with pefile BEFORE any body
// was written — reference_resolveexport_phantom_names.md: a name the DLL does
// not contain resolves to NULL in silence, the port still registers and the
// census still counts it. **Zero phantoms.** The addresses and ordinals above
// are that query's output, not Ghidra's and not an inference.
//
// REGISTRATION: by NAME for all twenty, and additionally by ADDRESS for
// GetNodeIndexFromName, whose one address-only call site is
// `code/Game/Action/showtool.cpp:169`
// (`ResolveAddr(Module::Runtime, 0x1800fee90)`, a fallback taken because the
// exe dispatch slot 0x140152970 is empty in standalone). Every other call site
// in this lane's scope reaches its function through `ResolveExport(<name>)` —
// including the ones that look address-shaped, because
// `standalone/address_registry.cpp` spells them
// `RegisterDispatchSlot(<exeSlot>, ResolveExport(<name>))`: the exe slot is
// storage, the resolution is by name. A name-only registration is therefore
// enough for those, and the census's orphan list is what will say if that read
// of the tree was wrong.
//
// ---------------------------------------------------------------------------
// WHAT STAYS IN THE DLL, AND WHY
// ---------------------------------------------------------------------------
// Everything this file calls out to falls into one of three groups.
//
// 1. Collection::Array<T> out-of-line instantiations (`Rt*Array*` below).
//    MSVC emits one copy/resize/free per element width, and each body takes the
//    allocator's own CRITICAL SECTION around MemoryAllocator::Free
//    (`EnterCriticalSection(_singleton + 0x640)`, plainly visible in the
//    decompile of every one of them). PLAN §4.1 decision 1 keeps runtime/src/
//    free of <windows.h> outside a `_win` twin, and PLAN §4.3 risk 1 warns that
//    a ported Set/Array body cannot serve two element widths. So these are
//    called at their retail addresses, exactly as retail's own code does — the
//    same shape Wave 5 lane D used for Picking's eight FUN_ internals and Wave 4
//    lane C used for Map_Find / Map_Insert.
//
// 2. Neighbouring EXPORTED engine functions retail reaches by a direct internal
//    CALL, not through the export table: Geometry's ctor/copy-ctor/dtor and
//    SetBounds, AnimationState's ResetAnimationState / PlayAnimation, and
//    Quaternion::operator* / Quaternion::Rotate. These are resolved ONCE with
//    RT_ORIGINAL_ONCE and then called through the cached pointer.
//
//    Why RT_ORIGINAL_ONCE (the DLL) and not ResolveExport (whatever is
//    registered)? Because retail's `MeshInstance::UpdateHierarchy` contains a
//    direct `CALL 0x18001e280`, so calling the DLL's body IS the byte-faithful
//    thing, and it removes a registration-ORDER dependency between lanes that
//    run concurrently in one tree. It also keeps this lane's diff oracle honest:
//    a divergence reported here is this lane's, not a neighbour's.
//
//    *** NO GetProcAddress SITS ON A HOT PATH IN THIS FILE. *** rt_port.h is
//    explicit that RtOriginal IS GetProcAddress — an export-table binary search
//    under the loader lock — and that two of them inside System::Time's body
//    cost 5.6 percentage points of sim time while hiding as ntdll self time
//    (reference_rtoriginal_is_getprocaddress.md, WAVE3_LANE_P.md). Every
//    resolution in this file is either inside an `if (rt_diff_on__)` guard (the
//    oracle, which no shipping run enters) or wrapped in RT_ORIGINAL_ONCE /
//    a function-local static, which resolves once per call site for the life of
//    the process. `MeshInstance::UpdateHierarchy` and `IsAnimating` in
//    particular execute nothing per call that retail did not.
//
// 3. Video::VertexBuffer / IndexBuffer::PlatformConstruct, which EdgeMesh's
//    ctor calls eight times. These are Wave 6 lane C's classes AND they are the
//    seam into `dx11/` — PlatformConstruct asks Device::_singleton's plugin for
//    a companion object and then links the buffer into the device's GLOBAL
//    intrusive buffer list. Lane E calls them; it does not port them, and the
//    global list is the reason `EdgeMesh::EdgeMesh` gets no scratch-buffer
//    oracle (see the oracle note above that function).
//
// ---------------------------------------------------------------------------
// IS THE CLASS SPLIT?  (PLAN §4.1 decision 5, §4.3 risk 3 — the real hazard.)
// ---------------------------------------------------------------------------
// MeshInstance and EdgeMeshInstance are VIRTUAL classes whose objects are built
// by these ctors and carry the DLL's own exported vtable
// (??_7MeshInstance@Graphics@@6B@ at 0x180154dc0, stamped by the ctor and
// resolved BY NAME here rather than cloned). So the DLL's `Renderer::Draw` and
// `Scene::Clean` keep dispatching MeshInstance's virtuals — Update, Draw,
// AddDrawItems, Clean, GetInstanceTransform — into the DLL, and this lane
// changes none of them. What flips is the NON-virtual surface our tree calls
// directly. Both implementations then operate on ONE object with ONE layout,
// which is the condition WAVE6.md sets for a Video/Graphics lane to be safe.
//
// The two exceptions are stated where they occur: `~MeshInstance` is virtual
// (UEAA) and IS ported, because the only caller that reaches it is ours through
// dispatch slot 0x1401514a8, which is a direct call and not a vtable dispatch;
// and `AddDrawItems` is virtual and is NOT ported, for a separate reason given
// at the bottom of this file.
//
// PORTABILITY: no <windows.h>, per PLAN §4.1 decision 1.
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/graphics/Mesh.h"

using RtGraphics::ExternalPtr;
using RtGraphics::Geometry;
using RtGraphics::GraphicsMeshView;
using RtGraphics::EdgeMesh;
using RtGraphics::EdgeMeshInstance;
using RtGraphics::EdgeVertex;
using RtGraphics::InterpolatedTransform;
using RtGraphics::MeshAnimationLayer;
using RtGraphics::MeshInstance;
using RtGraphics::MeshNode;
using RtGraphics::TriangleVertex;
using RtGraphics::VideoBufferHeader;
using RtGraphics::kEdgeMeshVertexScale;

// ----------------------------------------------------------------------------
// ResolveAddr, forward-declared rather than pulled in from standalone/resolve.h
// ----------------------------------------------------------------------------
// Same reasoning rt_port.h gives for declaring RegisterRuntimeExport locally:
// resolve.h drags in <windows.h>, and <wingdi.h>'s `Rectangle` function and
// <windef.h>'s `near` / `far` macros collide with the engine's own type and
// member names. This declaration must stay byte-identical to resolve.h's; the
// linker checks the two against each other.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace {

// ============================================================================
// The DLL internals this lane calls, resolved once each.
//
// The Array<T> helpers are NOT exported (they are template instantiations), so
// they are reached by retail ADDRESS through ResolveAddr, which is a base +
// ASLR-delta add and not a lookup. The cache is here so a hot body does not
// even pay that.
// ============================================================================

typedef void*  (*ArrayCopyFn)(void* dst, const void* src);   // Array<T>::operator=
typedef void   (*ArrayFreeFn)(void* self);                   // Array<T>::~Array
typedef void   (*ArrayResizeFn)(void* self, int n);          // Array<T>::SetSize

template <typename Fn>
Fn RtAddr(uintptr_t ghidra) {
    return reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, ghidra));
}

// Array<ExternalPtr<MaterialInstance>>, element 8 bytes.
//   0x180014c70  operator=   (frees, grows, then copies element by element)
//   0x180014d00  ~Array      (Free under the allocator critical section, then
//                             zeroes _data and the {_size,_capacity} qword)
ArrayCopyFn RtMatArrayCopy() { static ArrayCopyFn f = RtAddr<ArrayCopyFn>(0x180014c70); return f; }
ArrayFreeFn RtMatArrayFree() { static ArrayFreeFn f = RtAddr<ArrayFreeFn>(0x180014d00); return f; }

// Array<AnimationState layer>, element 0x38, alignment 8.
ArrayCopyFn RtLayerArrayCopy() { static ArrayCopyFn f = RtAddr<ArrayCopyFn>(0x18001b700); return f; }
ArrayFreeFn RtLayerArrayFree() { static ArrayFreeFn f = RtAddr<ArrayFreeFn>(0x18001c480); return f; }

// Array<InterpolatedState<Transform>>, element 0x38, alignment 4. Note that
// this is a DIFFERENT instantiation from the layer array above even though the
// stride matches, because the element type differs — MSVC emits one body per
// instantiation and the SetSize body below default-constructs new elements to
// an identity Transform pair, which the layer body does not.
ArrayCopyFn   RtNodeArrayCopy()   { static ArrayCopyFn   f = RtAddr<ArrayCopyFn>(0x18001b810);   return f; }
ArrayFreeFn   RtNodeArrayFree()   { static ArrayFreeFn   f = RtAddr<ArrayFreeFn>(0x18001c550);   return f; }
ArrayResizeFn RtNodeArrayResize() { static ArrayResizeFn f = RtAddr<ArrayResizeFn>(0x18005ac50); return f; }

// Array<GraphicsMesh::TriangleList>, element 0x18, alignment 4 — EdgeMesh::Set
// uses it for both its triangle list and its edge list.
ArrayResizeFn RtTriListArrayResize() { static ArrayResizeFn f = RtAddr<ArrayResizeFn>(0x1800136f0); return f; }

// The exported neighbours, by mangled name. Each is a direct internal CALL in
// the retail body it replaces, so going to the DLL is the byte-faithful choice
// as well as the one that removes a cross-lane registration-order dependency.
typedef void  (*GeometryCtorFn)(void* self);
typedef void  (*GeometryCopyCtorFn)(void* self, const void* other);
typedef void  (*GeometryDtorFn)(void* self);
typedef void  (*GeometrySetBoundsFn)(void* self, const void* box);
typedef void  (*AnimResetFn)(void* animState);
typedef void  (*AnimPlayFn)(void* animState, ExternalPtr group, int index,
                            const void* nodeArray, const void* startTime,
                            const void* blendTime, float weight, bool flag);
typedef void* (*QuatMulFn)(const void* self, void* ret, const void* other);
typedef void* (*QuatRotateFn)(const void* self, void* ret, const void* vec);

// RT_EXPORT_ONCE on the four below, not RT_ORIGINAL_ONCE (Wave 9 lane R,
// 2026-09-07). All four names are PORTED, and the raw door skips the port
// registry by design -- so MeshInstance::UpdateHierarchy was running the DLL's
// Quaternion::operator* on every node of every model, while Wave 1's port of it
// sat registered, bit-exact over millions of diffed calls, and unreached.
// Raw belongs in an oracle; a dependency belongs on the seam.
GeometryCtorFn RtGeometryCtor() {
    return RT_EXPORT_ONCE(GeometryCtorFn, "??0Geometry@Graphics@@QEAA@XZ");
}
GeometryCopyCtorFn RtGeometryCopyCtor() {
    return RT_ORIGINAL_ONCE(GeometryCopyCtorFn, "??0Geometry@Graphics@@QEAA@AEBV01@@Z");
}
GeometryDtorFn RtGeometryDtor() {
    return RT_EXPORT_ONCE(GeometryDtorFn, "??1Geometry@Graphics@@UEAA@XZ");
}
GeometrySetBoundsFn RtGeometrySetBounds() {
    return RT_ORIGINAL_ONCE(GeometrySetBoundsFn,
                            "?SetBounds@Geometry@Graphics@@QEAAXAEBVBox@Volume@@@Z");
}
AnimResetFn RtAnimReset() {
    return RT_ORIGINAL_ONCE(AnimResetFn, "?ResetAnimationState@AnimationState@Graphics@@QEAAXXZ");
}
AnimPlayFn RtAnimPlay() {
    return RT_ORIGINAL_ONCE(AnimPlayFn,
        "?PlayAnimation@AnimationState@Graphics@@QEAAXV?$ExternalPtr@VAnimationGroup@Graphics@@@Object@@HAEBV?$Array@UNode@GraphicsMesh@Graphics@@@Collection@@AEBVTime@System@@2M_N@Z");
}
QuatMulFn RtQuatMul() {
    return RT_EXPORT_ONCE(QuatMulFn, "??DQuaternion@Math@@QEBA?AV01@AEBV01@@Z");
}
QuatRotateFn RtQuatRotate() {
    return RT_EXPORT_ONCE(QuatRotateFn, "?Rotate@Quaternion@Math@@QEBA?AVVector3@2@AEBV32@@Z");
}

// Video buffer platform constructors, called eight times by EdgeMesh's ctor.
typedef void (*BufferPlatformCtorFn)(void* self);
BufferPlatformCtorFn RtVertexBufferPlatformCtor() {
    return RT_ORIGINAL_ONCE(BufferPlatformCtorFn, "?PlatformConstruct@VertexBuffer@Video@@QEAAXXZ");
}
BufferPlatformCtorFn RtIndexBufferPlatformCtor() {
    return RT_ORIGINAL_ONCE(BufferPlatformCtorFn, "?PlatformConstruct@IndexBuffer@Video@@QEAAXXZ");
}

// The vtables the ctors stamp. All four are REAL EXPORTED SYMBOLS in the
// shipped DLL — PLAN §2.2 hazard D was corrected once already ("the DLL exports
// 147 ??_7 vtable symbols") and this lane confirms four more of them by name.
// Stamping the DLL's own vtable, rather than cloning slots into an array we
// own, is what keeps the object dispatchable by the DLL's Renderer after our
// ctor built it (§4.3 risk 3). The per-class argument PLAN demands is easy
// here: these classes are not subclassed anywhere in our tree, our ctor is the
// ONLY writer of slot 0, and every virtual the DLL then dispatches is a DLL
// body this lane did not port.
void* RtVtable(const char* mangled) { return ResolveExportRaw(mangled); }

void* RtMeshInstanceVtable() {
    static void* v = RtVtable("??_7MeshInstance@Graphics@@6B@");        // 0x180154dc0
    return v;
}
void* RtEdgeMeshInstanceVtable() {
    static void* v = RtVtable("??_7EdgeMeshInstance@Graphics@@6B@");    // 0x180154d70
    return v;
}
void* RtEdgeMeshVtable() {
    static void* v = RtVtable("??_7EdgeMesh@Graphics@@6B@");            // 0x180154c70
    return v;
}
void* RtObjectBaseVtable() {
    static void* v = RtVtable("??_7Base@Object@@6B@");                  // 0x180154788
    return v;
}
void* RtObjectExternalVtable() {
    static void* v = RtVtable("??_7External@Object@@6B@");              // 0x1801547a0
    return v;
}

// Graphics::AnimationManager::_singleton, an exported DATA symbol
// (0x1801fe7b0). GetNodeToWorld and RootAnimationOffset index the instanced
// pose pool through it.
void* RtAnimationManager() {
    static void** slot = reinterpret_cast<void**>(
        ResolveExportRaw("?_singleton@AnimationManager@Graphics@@0PEAV12@EA"));
    return slot ? *slot : nullptr;
}

// Byte helpers. Every body below addresses its object through the typed struct
// where the struct pins the field, and through these where the retail code
// reaches past what this lane pinned (GraphicsMesh's detail levels, the
// AnimationManager pool).
inline uint8_t*       B(void* p)       { return reinterpret_cast<uint8_t*>(p); }
inline const uint8_t* B(const void* p) { return reinterpret_cast<const uint8_t*>(p); }

template <typename T> T  Rd(const void* p, size_t off) {
    T v; std::memcpy(&v, B(p) + off, sizeof(T)); return v;
}

}  // namespace

// ============================================================================
// Graphics::GraphicsMesh::GetMaterialList(int) const
//   ?GetMaterialList@GraphicsMesh@Graphics@@QEBAAEBV?$Array@V?$ExternalPtr@
//    VMaterialInstance@Graphics@@@Object@@@Collection@@H@Z
//   retail 0x1800126c0, ordinal 2768
//
// The whole body is `return *(mesh + 0x70) + index * 0x10` — the +0x70 field is
// a pointer to an ARRAY OF Array headers, one per detail level, and 0x10 is
// sizeof(Collection::Array<T>).
//
// ORACLE: FULL. Pure pointer arithmetic on a const object, no side effects, so
// both implementations can run on the live object and the two results compared
// directly.
// ============================================================================
static const void* RtGraphics_GraphicsMesh_GetMaterialList(const void* self, int index) {
    RT_DIFF_BEGIN("Graphics::GraphicsMesh::GetMaterialList");

    const GraphicsMeshView* mesh = reinterpret_cast<const GraphicsMeshView*>(self);
    const void* ours = reinterpret_cast<const uint8_t*>(mesh->_materialLists)
                     + static_cast<int64_t>(index) * 0x10;

    if (rt_diff_on__) {
        typedef const void* (*Fn)(const void*, int);
        const void* theirs = RtOriginal<Fn>(
            "?GetMaterialList@GraphicsMesh@Graphics@@QEBAAEBV?$Array@V?$ExternalPtr@VMaterialInstance@Graphics@@@Object@@@Collection@@H@Z")(self, index);
        RT_DIFF_SCALAR(ours, theirs);
    }
    return ours;
}

// ============================================================================
// Graphics::GraphicsMesh::GetNodeIndexFromName(const char*) const
//   ?GetNodeIndexFromName@GraphicsMesh@Graphics@@QEBAHPEBD@Z
//   retail 0x1800fee90, ordinal 2812
//
// A linear scan of the node array comparing each node's name to `name`, with
// retail's own inlined strcmp. Two details that are NOT ordinary strcmp and are
// reproduced exactly rather than replaced with std::strcmp:
//
//  * NULL is a legal name on both sides and compares EQUAL to NULL. If a node's
//    name is null the body returns that node's index when `name` is null too,
//    and otherwise SKIPS the node without comparing — it never dereferences a
//    null (`if (pcVar1 == 0) { if (param_1 == 0) return i; } else if (param_1
//    != 0) {...}`). A null `name` against a named node therefore falls through
//    to "not this one", not to a crash. This is not a guard retail lacks; it is
//    retail's own control flow, and CLAUDE.md's rule about not inventing null
//    checks cuts the other way here — inventing a std::strcmp would ADD a
//    crash retail does not have.
//  * The scan returns the FIRST match, and -1 when nothing matches.
//
// ORACLE: FULL. Const, no side effects.
// ============================================================================
static int RtGraphics_GraphicsMesh_GetNodeIndexFromName(const void* self, const char* name) {
    RT_DIFF_BEGIN("Graphics::GraphicsMesh::GetNodeIndexFromName");

    const GraphicsMeshView* mesh = reinterpret_cast<const GraphicsMeshView*>(self);
    const int count = mesh->_nodes._size;
    const MeshNode* nodes = mesh->_nodes._data;

    int ours = -1;
    for (int i = 0; i < count; ++i) {
        const char* nodeName = nodes[i]._name;
        if (nodeName == nullptr) {
            if (name == nullptr) { ours = i; break; }
            continue;                       // named-vs-null: skip, do not deref
        }
        if (name == nullptr) continue;      // ditto, the other way round

        // Retail's inlined strcmp: walk while the bytes agree and neither has
        // hit its terminator, then subtract the two bytes at the stopping
        // index. Reproduced literally so the returned "equal / not equal" is
        // decided on the same index retail decides it on.
        int64_t k = 0;
        if (nodeName[0] == name[0]) {
            while (nodeName[k] != '\0' && name[k] != '\0' && nodeName[k + 1] == name[k + 1])
                ++k;
        }
        if (static_cast<int>(nodeName[k]) - static_cast<int>(name[k]) == 0) { ours = i; break; }
    }

    if (rt_diff_on__) {
        typedef int (*Fn)(const void*, const char*);
        int theirs = RtOriginal<Fn>("?GetNodeIndexFromName@GraphicsMesh@Graphics@@QEBAHPEBD@Z")(self, name);
        RT_DIFF_SCALAR(ours, theirs);
    }
    return ours;
}

// ============================================================================
// Graphics::MeshInstance::IsAnimating() const
//   ?IsAnimating@MeshInstance@Graphics@@QEBA_NXZ
//   retail 0x1800ff7d0, ordinal 3097
//
// False when no animation layer is active. Otherwise it looks at layer[0]:
//   anim = *(layer->_group) + 0x20  ->  the group's Animation array
//   a    = anim + layer->_animIndex * 0x70
//   return a->_looping (+0x3c, a byte) || layer->_time (+0x18) < a->_length (+0x58)
//
// The three offsets inside Animation (+0x3c, +0x58, stride 0x70) belong to Wave
// 6 lane G and are read here, not pinned here.
//
// PERF: this is called per model per frame from
// code/Game/Component/naturalresourcecomponent.cpp and through dispatch slot
// 0x1401514b0. The body below is four loads and a compare — the same work
// retail does — and resolves nothing.
//
// ORACLE: FULL. Const, no side effects.
// ============================================================================
static bool RtGraphics_MeshInstance_IsAnimating(const void* self) {
    RT_DIFF_BEGIN("Graphics::MeshInstance::IsAnimating");

    const MeshInstance* mi = reinterpret_cast<const MeshInstance*>(self);
    bool ours;
    if (mi->_animation._layers._size == 0) {
        ours = false;
    } else {
        const uint8_t* layer = mi->_animation._layers._data;
        const void* group = Rd<void*>(layer, 0x00);
        const int32_t animIndex = Rd<int32_t>(layer, 0x08);
        const int64_t layerTime = Rd<int64_t>(layer, 0x18);

        const uint8_t* anim = B(Rd<void*>(group, 0x20)) + static_cast<int64_t>(animIndex) * 0x70;
        if (Rd<int8_t>(anim, 0x3c) != 0)
            ours = true;
        else
            ours = layerTime < Rd<int64_t>(anim, 0x58);
    }

    if (rt_diff_on__) {
        typedef bool (*Fn)(const void*);
        bool theirs = RtOriginal<Fn>("?IsAnimating@MeshInstance@Graphics@@QEBA_NXZ")(self);
        RT_DIFF_SCALAR(ours, theirs);
    }
    return ours;
}

namespace {

// The pool pose the two "instanced" branches share.
//
//   poolId = layers[count - 1]._poolId              (the LAST layer, not the first)
//   bank   = *(AnimationManager::_singleton + 0x20 + ((poolId >> 8) & 0xff) * 0x30)
//   slot   = bank + (poolId & 0xff) * 0x48
//
// Spelled in both decompiles as `*(int*)(count * 0x38 - 0x2c + buffer)`, which
// is `buffer + (count-1)*0x38 + 0x0c` — the last layer's pool id.
//
// *** THE TWO CALLERS DIVERGE AFTER THIS POINT AND THE DIFFERENCE IS ONE EXTRA
// DEREFERENCE. *** RootAnimationOffset reads three floats straight out of the
// slot at +0x18. GetNodeToWorld loads a POINTER out of the slot at +0x38 and
// indexes THAT by node * 0x38 — the slot holds the pool entry's per-node pose
// array, not the poses themselves. Sharing this helper past the slot would have
// made GetNodeToWorld read 0x38-byte strides out of the 0x48-byte pool entry
// itself. The helper therefore stops at the slot and each caller does its own
// last step.
//
// TRANSCRIBED FROM THE DISASSEMBLY, NOT THE DECOMPILE. Two of the three steps
// below were wrong when this was written from Ghidra's C output, and both were
// wrong in ways the C output actively invited (WAVE6_LANE_E.md §6.7, §6.8).
// The retail instructions, identical in both callers
// (0x180016f00-0x180016f39 and 0x180016acd-0x180016b0e):
//
//     MOVSXD RCX, dword ptr [R8 + RAX - 0x2c]   ; poolId, SIGN-extended to 64
//     MOV    RAX, RCX
//     SAR    RAX, 0x8                           ; ARITHMETIC shift
//     MOVZX  EAX, AL                            ; hi = that & 0xff
//     LEA    R8,  [RAX + RAX*2]                 ; hi*3 ...
//     MOV    RAX, qword ptr [0x1801fe7b0]       ; the _singleton VARIABLE's value
//     MOV    RDX, qword ptr [RAX]               ; *** SECOND dereference ***
//     MOVZX  EAX, CL                            ; lo = poolId & 0xff
//     ADD    R8,  R8                            ; ... *2  => hi*0x30
//     LEA    RCX, [RAX + RAX*8]                 ; lo*9, scaled *8 => lo*0x48
//     MOV    RAX, qword ptr [RDX + R8*8 + 0x20] ; bank
//
// Two traps, both recorded here because the decompile hides both:
//
//  1. Ghidra prints `uVar5 = (ulonglong)*(int *)(...)` and then
//     `(longlong)uVar5 >> 8`. The `(ulonglong)` reads as a ZERO extension, so
//     this was first written as a logical shift on a uint32. The instruction is
//     MOVSXD followed by SAR: a SIGN extension and an ARITHMETIC shift. For a
//     negative pool id the two pick DIFFERENT banks 0x30 bytes apart, and
//     0xffffffff is the one negative value where they happen to agree — so the
//     bug hides exactly on the id most likely to appear in a debug session.
//
//  2. `?_singleton@AnimationManager@Graphics@@0PEAV12@EA` is the address OF the
//     pointer variable, and retail dereferences it TWICE: once to get the
//     AnimationManager, and again to get the object the bank table lives in.
//     Ghidra spells that `*(longlong *)AnimationManager::_singleton`, where the
//     bare symbol already means "the contents of 0x1801fe7b0" — so the printed
//     single star is the SECOND dereference, not the first. Doing only one
//     lands `+ 0x20 + hi*0x30` inside the 0x9d8-byte AnimationManager itself,
//     which yields a plausible-looking garbage bank rather than a fault.
const uint8_t* RtInstancedPoolSlot(const MeshInstance* mi) {
    const uint8_t* layers = mi->_animation._layers._data;

    // MOVSXD: the stored pool id is a SIGNED int32 widened to 64 bits.
    const int64_t poolId = Rd<int32_t>(
        layers, static_cast<size_t>(mi->_animation._layers._size) * 0x38 - 0x2c);

    // SAR then MOVZX AL: arithmetic shift of the sign-extended value, low byte.
    const uint64_t hi = static_cast<uint64_t>(static_cast<uint8_t>(poolId >> 8));
    const uint64_t lo = static_cast<uint64_t>(static_cast<uint8_t>(poolId));

    const uint8_t* mgr  = B(RtAnimationManager());        // *(0x1801fe7b0)
    const uint8_t* base = B(Rd<void*>(mgr, 0));           // *mgr  <- the second deref
    const uint8_t* bank = B(Rd<void*>(base, 0x20 + hi * 0x30));
    return bank + lo * 0x48;
}

}  // namespace

// ============================================================================
// Graphics::MeshInstance::GetNodeToWorld(int) const -> Math::Transform
//   ?GetNodeToWorld@MeshInstance@Graphics@@QEBA?AVTransform@Math@@H@Z
//   retail 0x180016a40, ordinal 2813
//
// *** HIDDEN RETURN SLOT. *** The function returns a 0x1c-byte Math::Transform
// BY VALUE, so the MS x64 ABI is RCX = this, RDX = the caller's Transform
// buffer, R8D = the node index, and RAX = RDX on return. Ghidra shows this
// plainly and confusingly at once: it types the second parameter as the `int`
// and then reads the index out of `in_R8D`. Declaring this function without the
// return slot would be the recorded WILD WRITE — a scribble through a stale RCX
// that still returns the right answer, live on every scene load
// (reference_hidden_retslot_wild_write.md, Wave 1's
// fn_Math_Transform3_Identity). tools/abi_retslot_audit.py enforces the rule.
//
// Two branches:
//  * NOT instanced (_animation._instanced == 0): copy _nodes[node]._state[0]
//    (seven dwords) into the return slot. Note it reads the FIRST of the two
//    Transforms in the InterpolatedState.
//  * Instanced: read the pool slot, take the geometry's own transform, and
//    compose — translation = geomTranslation + Rotate(geomRotation, poolPos),
//    rotation = geomRotation * poolRotation. The return slot is pre-seeded with
//    an identity (`w = 1.0f` at +0x0c, zeros elsewhere) BEFORE the two
//    quaternion calls; that store is reproduced because it is observable if
//    either call were ever to fault.
//
// ORACLE: FULL, through a scratch buffer. Ours writes the caller's real slot,
// theirs writes a private 0x1c-byte buffer, and the two are compared byte for
// byte — rt_diff.h's "functions that write through an out-parameter" case.
// Never let the original write into live state.
// ============================================================================
static void* RtGraphics_MeshInstance_GetNodeToWorld(const void* self, void* ret, int node) {
    RT_DIFF_BEGIN("Graphics::MeshInstance::GetNodeToWorld");

    const MeshInstance* mi = reinterpret_cast<const MeshInstance*>(self);
    uint8_t* out = B(ret);

    if (mi->_animation._instanced == 0) {
        const uint8_t* src = B(mi->_nodes._data) + static_cast<int64_t>(node) * 0x38;
        std::memcpy(out, src, 0x1c);            // seven dwords, _state[0]
    } else {
        // The geometry transform, copied into a local first exactly as retail
        // does (it materialises all seven floats before either call).
        uint8_t geomXf[0x1c];
        std::memcpy(geomXf, B(self) + 0x84, 0x1c);

        // slot + 0x38 is a POINTER to the pool entry's per-node pose array; the
        // node index strides THAT, not the slot. See RtInstancedPoolSlot.
        const uint8_t* poses = B(Rd<void*>(RtInstancedPoolSlot(mi), 0x38));
        uint8_t poolXf[0x1c];
        std::memcpy(poolXf, poses + static_cast<int64_t>(node) * 0x38, 0x1c);

        // Retail seeds the return slot with the identity transform before the
        // two calls: qword at +0x0c = 0x3f800000 (so +0x0c = 1.0f, +0x10 = 0),
        // qword at +0x00 = 0, dword at +0x08 = 0, qword at +0x14 = 0.
        std::memset(out, 0, 0x1c);
        const float one = 1.0f;
        std::memcpy(out + 0x0c, &one, 4);

        // translation = geom.translation + Rotate(geom.rotation, pool.translation)
        uint8_t rotated[16];
        RtQuatRotate()(geomXf /* rotation at +0x00 */, rotated, poolXf + 0x10);
        for (int i = 0; i < 3; ++i) {
            const float t = Rd<float>(geomXf, 0x10 + i * 4) + Rd<float>(rotated, i * 4);
            std::memcpy(out + 0x10 + i * 4, &t, 4);
        }
        // rotation = geom.rotation * pool.rotation
        uint8_t q[16];
        RtQuatMul()(geomXf, q, poolXf);
        std::memcpy(out, q, 16);
    }

    if (rt_diff_on__) {
        uint8_t scratch[0x1c];
        std::memset(scratch, 0xcd, sizeof(scratch));
        typedef void* (*Fn)(const void*, void*, int);
        RtOriginal<Fn>("?GetNodeToWorld@MeshInstance@Graphics@@QEBA?AVTransform@Math@@H@Z")(
            self, scratch, node);
        RT_DIFF_BYTES(out, scratch, sizeof(scratch));
    }
    return ret;
}

// ============================================================================
// Graphics::MeshInstance::RootAnimationOffset() const -> Math::Vector3
//   ?RootAnimationOffset@MeshInstance@Graphics@@QEBA?AVVector3@Math@@XZ
//   retail 0x180016ef0, ordinal 3589
//
// *** HIDDEN RETURN SLOT *** again: RCX = this, RDX = a Vector3 buffer. Same
// rule as GetNodeToWorld above.
//
// Not instanced: the instance's own _rootAnimationOffset (+0x100). Instanced:
// three floats at poolSlot + 0x18. Note the instanced branch reads the pool
// slot ITSELF, with no per-node stride — the root offset is one Vector3 per
// pool entry.
//
// standalone/perf_experimental.cpp's ANIM_BATCH note ("the move tick's walk
// speed is |MeshInstance::RootAnimationOffset()| per tick", and "instanced
// anims read the AnimationManager pool pose") describes exactly these two
// branches, and this port does not change which branch is taken: it reads the
// same `_instanced` byte perf_experimental reasons about.
//
// ORACLE: FULL, through a scratch buffer, same shape as GetNodeToWorld.
// ============================================================================
static void RtGraphics_MeshInstance_RootAnimationOffset(const void* self, void* ret) {
    RT_DIFF_BEGIN("Graphics::MeshInstance::RootAnimationOffset");

    const MeshInstance* mi = reinterpret_cast<const MeshInstance*>(self);
    uint8_t* out = B(ret);

    if (mi->_animation._instanced == 0) {
        std::memcpy(out, B(self) + 0x100, 12);
    } else {
        std::memcpy(out, RtInstancedPoolSlot(mi) + 0x18, 12);
    }

    if (rt_diff_on__) {
        uint8_t scratch[12];
        std::memset(scratch, 0xcd, sizeof(scratch));
        typedef void (*Fn)(const void*, void*);
        RtOriginal<Fn>("?RootAnimationOffset@MeshInstance@Graphics@@QEBA?AVVector3@Math@@XZ")(
            self, scratch);
        RT_DIFF_BYTES(out, scratch, sizeof(scratch));
    }
}

// ============================================================================
// Graphics::MeshInstance::UpdateHierarchy()
//   ?UpdateHierarchy@MeshInstance@Graphics@@QEAAXXZ
//   retail 0x1800ff910, ordinal 4518
//
// Recomputes every node's world transform from the geometry transform and the
// mesh's node parent chain. Node 0 is seeded from Geometry::_transform; each
// later node composes its parent's already-computed world transform with its
// own local one. Both halves of each InterpolatedState are written to the same
// value.
//
// *** THIS IS THE HOTTEST FUNCTION IN THIS LANE AND THIS PROJECT HAS PAID FOR
// THIS PATH TWICE. *** `?_rootOnlyUpdates@MeshInstance@Graphics@@0_NA` being
// unregistered cost 38% of sim time
// (project_stress_perf_rootonly_and_libcalls.md), and
// standalone/perf_experimental.cpp's default-on ANIM_BATCH path reads
// MeshInstance state every frame. Three things this port does about that:
//   * It resolves NOTHING per call. `RtQuatMul()` is a function-local static
//     resolved once for the life of the process (RT_ORIGINAL_ONCE); calling
//     RtOriginal directly here would be the recorded 5.6-point defect.
//   * It does not touch `_rootOnlyUpdates`, which retail's UpdateHierarchy does
//     not read either. The flag is read by MeshInstance::Update, which stays in
//     the DLL, so perf_experimental's save/clear/restore of it keeps working
//     unchanged.
//   * The whole oracle, including its allocation, sits inside `if
//     (rt_diff_on__)`. A shipping run executes the loop and nothing else.
//
// THE FLOAT MATH IS TRANSCRIBED, NOT REDERIVED. The position half is retail's
// own INLINED quaternion rotate — including the two `* 0.0f` terms, which are
// the zero w of the vector-as-quaternion and which a compiler may not fold
// away without -ffast-math (x * 0.0 is not 0.0 for NaN or -0.0). The
// association of every sum is exactly as the decompiled expression writes it,
// because a reassociated float sum is a real divergence and Wave 1 found one
// (`Transform::FromDirection`). The rotation half is the DLL's own
// Quaternion::operator* — retail emits a direct `CALL 0x18001e280` here, so
// calling the DLL's body is the faithful thing.
//
// AND IT LEAVES A WAVE 1 PORT STILL UNEXERCISED, deliberately. RT_ORIGINAL_ONCE
// goes through ResolveExportRaw, which is GetProcAddress and therefore always
// the DLL — it does NOT consult the runtime registry. So Wave 1's port of
// `??DQuaternion@Math@@QEBA?AV01@AEBV01@@Z` keeps its zero-caller status (it is
// still on the census orphan list, and reference_uncalled_port_is_not_verified
// .md records a WRONG Quaternion::operator* surviving all of Wave 1 exactly
// that way). Switching this call to ResolveExport would flip it and finally
// exercise it — but it would also fold a neighbouring lane's correctness into
// this lane's diff site, so a divergence reported here would no longer be
// evidence about this lane. That trade is the wave lead's to make, not lane E's;
// it is written up as an opportunity in WAVE6_LANE_E.md.
//
// ORACLE: FULL, by REPLAY. UpdateHierarchy is a pure function of (Geometry::
// _transform, mesh nodes) into _nodes — running it twice yields the same array,
// so ours runs, the result is snapshotted, theirs runs on the same object, and
// the two are compared. Nothing is mutated that the second call does not
// rewrite identically. This is the same "FULL (replay)" oracle Wave 5 lane D
// used for Picking::Scene::SetTransform.
// ============================================================================
static void RtGraphics_MeshInstance_UpdateHierarchy(void* self) {
    RT_DIFF_BEGIN("Graphics::MeshInstance::UpdateHierarchy");

    MeshInstance* mi = reinterpret_cast<MeshInstance*>(self);
    const int count = mi->_nodes._size;
    if (count != 0) {
        uint8_t* xf = B(mi->_nodes._data);
        const uint8_t* mesh = B(mi->_mesh.p);

        // Node 0: the geometry's own transform, in both halves.
        std::memcpy(xf, B(self) + 0x84, 0x1c);
        std::memcpy(xf + 0x1c, xf, 0x1c);

        const uint8_t* nodes = B(Rd<void*>(mesh, 0x60));
        for (int i = 1; i < count; ++i) {
            const uint8_t* node = nodes + static_cast<int64_t>(i) * 0x48;
            const int32_t parent = Rd<int32_t>(node, 0x08);
            const uint8_t* p = xf + static_cast<int64_t>(parent) * 0x38;   // parent world xf

            const float qx = Rd<float>(p, 0x00);
            const float qy = Rd<float>(p, 0x04);
            const float qz = Rd<float>(p, 0x08);
            const float qw = Rd<float>(p, 0x0c);
            const float px = Rd<float>(p, 0x10);
            const float py = Rd<float>(p, 0x14);
            const float pz = Rd<float>(p, 0x18);

            // The node's LOCAL transform lives at node + 0x0c; the rotation is
            // the first 16 bytes and the translation the three floats at +0x10
            // from there (i.e. node + 0x1c).
            const uint8_t* localXf = node + 0x0c;
            const float vx = Rd<float>(localXf, 0x10);
            const float vy = Rd<float>(localXf, 0x14);
            const float vz = Rd<float>(localXf, 0x18);

            // Conjugate components, produced in retail by XORing the sign bit
            // (DAT_1801617c0 = 0x80000000, read out of the shipped DLL). The w
            // component is deliberately NOT negated.
            const float cx = -qx, cy = -qy, cz = -qz;

            // t = q * (v, 0)   -- association exactly as decompiled
            const float tx = (qw * vx + qx * 0.0f + qz * vy) - qy * vz;
            const float ty = (qy * 0.0f - qz * vx) + qw * vy + qx * vz;
            const float tz = ((qy * vx + qz * 0.0f) - qx * vy) + qw * vz;
            const float tw = ((qw * 0.0f - qx * vx) - qy * vy) - qz * vz;

            // world.translation = p + (t * conj(q)).xyz
            //
            // *** THE PARENT TRANSLATION IS ADDED LAST, AFTER THE ROTATION SUM
            // IS COMPLETE. *** Retail accumulates each component fully and only
            // then does one ADDSS with p:
            //     ffbf6 ADDSS XMM3,XMM1   / ffbfd SUBSS XMM3,XMM0  -> R_X
            //     ffc04 ADDSS XMM8,XMM3                            -> p.x + R_X
            //     ffc3d ADDSS XMM2,XMM1   / ffc41 ADDSS XMM2,XMM0  -> R_Y
            //     ffc4f ADDSS XMM7,XMM2                            -> p.y + R_Y
            //     ffc45 SUBSS XMM10,XMM11 / ffc4a ADDSS XMM10,XMM4 -> R_Z
            //     ffc53 ADDSS XMM6,XMM10                           -> p.z + R_Z
            // The first draft wrote `py + (a - b) + c + d`, which folds p.y into
            // the running sum after the FIRST term instead of after the last —
            // a reassociated float sum, the same defect Wave 1 found in
            // `Transform::FromDirection` (WAVE1.md). It changes only the last
            // bit, which is why it diverged on ~5% of nodes and not all of them,
            // and why the X component — where the whole rotation sum already sat
            // inside one pair of parentheses — was correct by accident.
            const float rx = (qw * tx + tw * cx + tz * cy) - ty * cz;
            const float ry = (qw * ty - tz * cx) + tw * cy + tx * cz;
            const float rz = ((ty * cx + qw * tz) - tx * cy) + tw * cz;
            const float wx = px + rx;
            const float wy = py + ry;
            const float wz = pz + rz;

            // world.rotation = parentRotation * nodeLocalRotation
            uint8_t q[16];
            RtQuatMul()(p, q, localXf);

            uint8_t* dst = xf + static_cast<int64_t>(i) * 0x38;
            std::memcpy(dst + 0x00, q, 16);
            std::memcpy(dst + 0x10, &wx, 4);
            std::memcpy(dst + 0x14, &wy, 4);
            std::memcpy(dst + 0x18, &wz, 4);
            std::memcpy(dst + 0x1c, dst, 0x1c);       // second half = first half
        }
    }

    if (rt_diff_on__) {
        const size_t n = static_cast<size_t>(count) * 0x38;
        uint8_t* ours = n ? static_cast<uint8_t*>(std::malloc(n)) : nullptr;
        if (ours) {
            std::memcpy(ours, mi->_nodes._data, n);
            typedef void (*Fn)(void*);
            RtOriginal<Fn>("?UpdateHierarchy@MeshInstance@Graphics@@QEAAXXZ")(self);
            RT_DIFF_BYTES(ours, mi->_nodes._data, n);
            std::free(ours);
        } else {
            RT_DIFF_SHADOW(true, "nodes=0", "nodes=0");
        }
    }
}

// ============================================================================
// Graphics::MeshInstance::SetMaterialInstance(int, ExternalPtr<MaterialInstance>)
//   ?SetMaterialInstance@MeshInstance@Graphics@@QEAAXHV?$ExternalPtr@
//    VMaterialInstance@Graphics@@@Object@@@Z
//   retail 0x1800ffe80, ordinal 3908
//
// One store: `materials[index] = value`. No bounds check — retail has none, and
// CLAUDE.md forbids adding one.
//
// ORACLE: FULL by replay. The store is idempotent, so running theirs after ours
// writes the same qword to the same slot and the comparison is real.
// ============================================================================
static void RtGraphics_MeshInstance_SetMaterialInstance(void* self, int index, ExternalPtr value) {
    RT_DIFF_BEGIN("Graphics::MeshInstance::SetMaterialInstance");

    MeshInstance* mi = reinterpret_cast<MeshInstance*>(self);
    mi->_materials._data[index] = value;

    if (rt_diff_on__) {
        void* ours = mi->_materials._data[index].p;
        typedef void (*Fn)(void*, int, ExternalPtr);
        RtOriginal<Fn>(
            "?SetMaterialInstance@MeshInstance@Graphics@@QEAAXHV?$ExternalPtr@VMaterialInstance@Graphics@@@Object@@@Z")(
            self, index, value);
        RT_DIFF_SCALAR(ours, mi->_materials._data[index].p);
    }
}

// ============================================================================
// Graphics::MeshInstance::SetMesh(ExternalPtr<GraphicsMesh>)
//   ?SetMesh@MeshInstance@Graphics@@QEAAXV?$ExternalPtr@VGraphicsMesh@
//    Graphics@@@Object@@@Z
//   retail 0x1800ffea0, ordinal 3915
//
// In order:
//   1. store the mesh handle at +0xc0;
//   2. copy the mesh's detail-level-0 material Array into _materials, through
//      the DLL's Array<ExternalPtr>::operator= (0x180014c70);
//   3. OR `1 << *(int*)(material + 0x20)` into Geometry::_materialTypeFlags for
//      every material now in _materials — note the loop reads the count from
//      +0xd0 BEFORE the loop and re-reads the buffer pointer once, exactly as
//      decompiled;
//   4. AnimationState::ResetAnimationState;
//   5. resize _nodes to the mesh's node count (mesh + 0x68);
//   6. UpdateHierarchy;
//   7. re-read the mesh handle from +0xc0 (retail re-loads it; it does NOT
//      reuse the argument) and, if any of the six AABB floats differs from
//      Geometry::_localBounds, copy all six and OR kDirtyBounds (2) into
//      Geometry::_dirtyFlags.
//
// Step 7's comparison is a float `!=` chain, so a NaN in either box makes the
// test "different" and forces the copy. Reproduced as written.
//
// ORACLE: FULL by replay. Every step is idempotent for a fixed argument: the
// array copy re-copies the same elements, the flag ORs re-OR the same bits,
// ResetAnimationState re-clears an already-cleared state, the resize to the
// same count is the "exact-equal is a no-op" path of Array<T>::SetSize, and
// UpdateHierarchy is pure. So ours runs, the whole 0x120-byte object plus both
// heap buffers are snapshotted, theirs runs, and the two are compared.
// ============================================================================
static void RtGraphics_MeshInstance_SetMesh(void* self, ExternalPtr mesh) {
    RT_DIFF_BEGIN("Graphics::MeshInstance::SetMesh");

    MeshInstance* mi = reinterpret_cast<MeshInstance*>(self);

    mi->_mesh = mesh;
    // *** LOAD the pointer at mesh+0x70; do NOT pass the address of the field. ***
    // Retail is `FUN_180014c70(this + 200, *(longlong **)(param_2 + 0x70))` — a
    // DEREFERENCE. GraphicsMesh+0x70 is a POINTER to an array of Array headers
    // (one per detail level), not an embedded Array, so the source of this copy
    // is detail level 0's header, which is exactly what
    // GetMaterialList(0) returns and what `_materialLists` names.
    //
    // Passing `B(mesh.p) + 0x70` instead crashed the first integration run, and
    // it is worth recording WHY it looked plausible: the very next Runtime call
    // in this function, and the one in PlayAnimation below, DO take the address
    // of an embedded Array (`&mi->_nodes`, `mesh + 0x60`). The two shapes sit
    // three lines apart. The tell is in the retail decompile every time — an
    // embedded Array is passed as `(longlong *)(x + off)`, a pointer field as
    // `*(longlong **)(x + off)` — so read the stars, not the offsets.
    RtMatArrayCopy()(&mi->_materials, reinterpret_cast<const GraphicsMeshView*>(mesh.p)->_materialLists);

    if (mi->_materials._size > 0) {
        uint32_t flags = mi->_base._materialTypeFlags;
        const ExternalPtr* mats = mi->_materials._data;
        for (int32_t n = mi->_materials._size; n != 0; --n) {
            const void* mat = mats->p;
            ++mats;
            flags |= 1u << (static_cast<uint8_t>(Rd<int32_t>(mat, 0x20)) & 0x1f);
            mi->_base._materialTypeFlags = flags;
        }
    }

    RtAnimReset()(&mi->_animation);
    RtNodeArrayResize()(&mi->_nodes, Rd<int32_t>(mesh.p, 0x68));
    RtGraphics_MeshInstance_UpdateHierarchy(self);

    const uint8_t* m = B(mi->_mesh.p);           // re-read, as retail does
    float* lb = reinterpret_cast<float*>(B(self) + 0x34);
    if (lb[0] != Rd<float>(m, 0x80) || lb[1] != Rd<float>(m, 0x84) ||
        lb[2] != Rd<float>(m, 0x88) || lb[3] != Rd<float>(m, 0x8c) ||
        lb[4] != Rd<float>(m, 0x90) || lb[5] != Rd<float>(m, 0x94)) {
        for (int i = 0; i < 6; ++i)
            lb[i] = Rd<float>(m, 0x80 + i * 4);
        mi->_base._dirtyFlags |= 2u;
    }

    if (rt_diff_on__) {
        const size_t nodeBytes = static_cast<size_t>(mi->_nodes._size) * 0x38;
        const size_t matBytes  = static_cast<size_t>(mi->_materials._size) * 8;
        uint8_t  obj[0x120];
        uint8_t* nodeCopy = nodeBytes ? static_cast<uint8_t*>(std::malloc(nodeBytes)) : nullptr;
        uint8_t* matCopy  = matBytes  ? static_cast<uint8_t*>(std::malloc(matBytes))  : nullptr;
        std::memcpy(obj, self, sizeof(obj));
        if (nodeCopy) std::memcpy(nodeCopy, mi->_nodes._data, nodeBytes);
        if (matCopy)  std::memcpy(matCopy,  mi->_materials._data, matBytes);

        typedef void (*Fn)(void*, ExternalPtr);
        RtOriginal<Fn>(
            "?SetMesh@MeshInstance@Graphics@@QEAAXV?$ExternalPtr@VGraphicsMesh@Graphics@@@Object@@@Z")(
            self, mesh);

        // The Array headers themselves may legitimately differ in _data if the
        // replay reallocated, so compare the object EXCEPT the two buffer
        // pointers, then compare the buffer CONTENTS separately.
        bool eq = std::memcmp(obj, self, 0xc8) == 0
               && std::memcmp(obj + 0xd0, B(self) + 0xd0, 0x20) == 0
               && std::memcmp(obj + 0xf8, B(self) + 0xf8, 0x120 - 0xf8) == 0
               && (!nodeCopy || std::memcmp(nodeCopy, mi->_nodes._data, nodeBytes) == 0)
               && (!matCopy  || std::memcmp(matCopy,  mi->_materials._data, matBytes) == 0);
        char a[96], b[96];
        // The verdict is a FIVE-term conjunction and the message names all five,
        // because a message that describes fewer terms than the predicate is how
        // Wave 5 spent a day on RemoveMesh
        // (reference_shadow_predicate_must_name_its_failing_term.md).
        std::snprintf(a, sizeof(a), "head=%d mid=%d tail=%d nodes=%d mats=%d",
                      std::memcmp(obj, self, 0xc8) == 0,
                      std::memcmp(obj + 0xd0, B(self) + 0xd0, 0x20) == 0,
                      std::memcmp(obj + 0xf8, B(self) + 0xf8, 0x120 - 0xf8) == 0,
                      !nodeCopy || std::memcmp(nodeCopy, mi->_nodes._data, nodeBytes) == 0,
                      !matCopy  || std::memcmp(matCopy,  mi->_materials._data, matBytes) == 0);
        std::snprintf(b, sizeof(b), "head=1 mid=1 tail=1 nodes=1 mats=1");
        RT_DIFF_SHADOW(eq, a, b);
        std::free(nodeCopy);
        std::free(matCopy);
    }
}

// ============================================================================
// Graphics::MeshInstance::PlayAnimation(ExternalPtr<AnimationGroup>, int,
//                                       const Time&, const Time&, float)
//   ?PlayAnimation@MeshInstance@Graphics@@QEAAXV?$ExternalPtr@
//    VAnimationGroup@Graphics@@@Object@@HAEBVTime@System@@1M@Z
//   retail 0x1800ff840, ordinal 3421
//
// A pure forward into AnimationState::PlayAnimation with two arguments the
// caller does not supply: the mesh's node array (`*(this+0xc0) + 0x60`) and a
// trailing `true`. The callee's mangled name spells its seven parameters
// exactly — (ExternalPtr, int, const Array<Node>&, const Time&, const Time&,
// float, bool) — and the retail call site fills them in that order.
//
// THE RISK HERE IS THE ABI, NOT THE LOGIC. `const Time&` is a POINTER argument
// and this project's recorded trap is that a pointer parameter typed `int` only
// goes wrong from the fifth argument onward, where the stack slot's high dword
// is garbage (reference_hidden_retval_and_int_pointer_args.md). `blendTime` is
// this function's fifth argument and `weight` its sixth, so both live on the
// stack and both are exactly where that trap bites. They are typed `const
// void*` and `float` here, counted against the mangled name, not guessed.
//
// ORACLE: POST-STATE, and it checks the thing that can actually be wrong. There
// is no replay: PlayAnimation starts or blends an animation, so calling the
// original a second time would apply the blend twice. Instead the port verifies
// that the layer AnimationState just wrote carries the group handle and the
// animation index we forwarded — which is what a mis-marshalled argument would
// corrupt. It does NOT verify the two Time references or the weight; those
// three are declared unverified in WAVE6_LANE_E.md rather than covered by a
// predicate that does not actually test them.
// ============================================================================
static void RtGraphics_MeshInstance_PlayAnimation(void* self, ExternalPtr group, int index,
                                                  const void* startTime, const void* blendTime,
                                                  float weight) {
    RT_DIFF_BEGIN("Graphics::MeshInstance::PlayAnimation");

    MeshInstance* mi = reinterpret_cast<MeshInstance*>(self);
    RtAnimPlay()(&mi->_animation, group, index, B(mi->_mesh.p) + 0x60,
                 startTime, blendTime, weight, true);

    if (rt_diff_on__) {
        // *** THE NEW LAYER IS layers[0], NOT layers[count-1]. ***
        // AnimationState::PlayAnimation (0x1800fdf50) grows the array by one and
        // then walks it DOWNWARD copying `layer[i] = layer[i-1]`, i.e. it
        // INSERTS AT THE FRONT. Reading the last layer read the layer being
        // pushed out, so this predicate reported the PREVIOUS animation's index
        // — 187 of 453 calls, with `ours anim=0 theirs anim=1` on a matching
        // group pointer. The PORT was correct; the ORACLE read the wrong end
        // (WAVE6_LANE_E.md §6.10).
        //
        // Corroboration that front-insertion is the convention and not a
        // one-off: `MeshInstance::IsAnimating` (0x1800ff7d0) decides whether the
        // instance is still animating from `layers[0]` and nothing else.
        //
        // Two retail paths this predicate now also covers, which the old one did
        // not model at all:
        //  * index < 0 CLEARS every layer (`FUN_18005c8a0(this+8, 0)` on the
        //    early-return branch) — so the correct expectation is an EMPTY
        //    array, and reading layers[0] there would have been out of bounds.
        //  * re-playing the same (group, index) on a LOOPING animation returns
        //    early after writing 1.0f to layers[0]+0x10, adding no layer.
        //    layers[0] still carries the right group and index, so the same
        //    check holds.
        char a[112], b[112];
        const int32_t n = mi->_animation._layers._size;
        bool eq;
        if (index < 0) {
            eq = (n == 0);
            std::snprintf(a, sizeof(a), "index<0 layers=%d", n);
            std::snprintf(b, sizeof(b), "index<0 layers=0");
        } else if (n > 0) {
            const MeshAnimationLayer* top =
                reinterpret_cast<const MeshAnimationLayer*>(mi->_animation._layers._data);
            eq = top->_group.p == group.p && top->_animIndex == index;
            std::snprintf(a, sizeof(a), "layers=%d group=%p anim=%d",
                          n, top->_group.p, top->_animIndex);
            std::snprintf(b, sizeof(b), "layers>0 group=%p anim=%d", group.p, index);
        } else {
            eq = false;
            std::snprintf(a, sizeof(a), "layers=0 (no layer written)");
            std::snprintf(b, sizeof(b), "layers>0 group=%p anim=%d", group.p, index);
        }
        RT_DIFF_SHADOW(eq, a, b);
    }
}

// ============================================================================
// Graphics::MeshInstance::MeshInstance()
//   ??0MeshInstance@Graphics@@QEAA@XZ
//   retail 0x1800fffc0, ordinal 275
//
// Geometry base ctor, then the vtable stamp, then this class's own fields. The
// disassembly is unambiguous about the widths and this port matches them: three
// qwords at +0xc0/+0xc8/+0xd0, a single BYTE at +0xd8 (`MOV byte ptr [RAX],CL`),
// qwords at +0xe0/+0xe8, qwords at +0xf0/+0xf8/+0x100, a DWORD at +0x108, four
// 1.0f dwords at +0x10c..+0x118, and a WORD at +0x11c. The last store being a
// word — not a dword — is why sizeof is 0x120 and why +0x11e is left alone.
//
// Note what is NOT written: +0x104 gets no store of its own; it is the second
// half of the +0x100 qword. And Geometry's own ctor leaves +0x14 and +0xbc
// uninitialised (lane D's header records that); this port does not fill them.
//
// ORACLE: FULL, through a scratch buffer. `Geometry::Geometry` is pure field
// initialisation — no allocation, no registration, no global list (checked at
// 0x1800a4270) — so running theirs into a private 0x120-byte buffer has no side
// effect at all, and the two objects can be compared byte for byte. The two
// uninitialised gaps are excluded from the comparison, because comparing
// uninitialised bytes is comparing whatever two different stacks happened to
// hold.
// ============================================================================
static void* RtGraphics_MeshInstance_Ctor(void* self) {
    RT_DIFF_BEGIN("Graphics::MeshInstance::MeshInstance()");

    MeshInstance* mi = reinterpret_cast<MeshInstance*>(self);
    RtGeometryCtor()(self);
    mi->_base._vtable = RtMeshInstanceVtable();

    mi->_mesh.p = nullptr;
    mi->_materials._data = nullptr;
    mi->_materials._size = 0;
    mi->_materials._capacity = 0;
    mi->_animation._instanced = 0;
    mi->_animation._layers._data = nullptr;
    mi->_animation._layers._size = 0;
    mi->_animation._layers._capacity = 0;
    mi->_nodes._data = nullptr;
    mi->_nodes._size = 0;
    mi->_nodes._capacity = 0;
    mi->_rootAnimationOffset.x = 0.0f;
    mi->_rootAnimationOffset.y = 0.0f;
    mi->_rootAnimationOffset.z = 0.0f;
    mi->_color[0] = 1.0f;
    mi->_color[1] = 1.0f;
    mi->_color[2] = 1.0f;
    mi->_color[3] = 1.0f;
    mi->_sortBias = 0;

    if (rt_diff_on__) {
        uint8_t scratch[0x120];
        std::memset(scratch, 0xcd, sizeof(scratch));
        typedef void* (*Fn)(void*);
        RtOriginal<Fn>("??0MeshInstance@Graphics@@QEAA@XZ")(scratch);
        // THREE uninitialised gaps are excluded, not two. Geometry leaves
        // +0x14..+0x17 and +0xbc..+0xbf (lane D's header records those), and
        // MeshInstance leaves +0xd9..+0xdf — the seven bytes of padding between
        // the AnimationState bool and its layer Array. The disassembly is
        // explicit: `LEA RAX,[RBX+0xd8]`, then `MOV byte ptr [RAX],CL`, then
        // `MOV qword ptr [RAX+0x8],RCX` — nothing writes RAX+1..RAX+7. Plus the
        // trailing pad at +0x11e..+0x11f.
        //
        // Comparing that gap is what made this site report a divergence on
        // 30130 of 30130 calls: `scratch` is memset to 0xcd and the live object
        // holds whatever the allocator left, so the two can never agree there.
        // The PORT was correct; the ORACLE was reading uninitialised memory
        // (WAVE6_LANE_E.md §6.9).
        bool head = std::memcmp(B(self) + 0x00, scratch + 0x00, 0x14) == 0;
        bool body = std::memcmp(B(self) + 0x18, scratch + 0x18, 0xbc - 0x18) == 0;
        bool mid  = std::memcmp(B(self) + 0xc0, scratch + 0xc0, 0xd9 - 0xc0) == 0;
        bool tail = std::memcmp(B(self) + 0xe0, scratch + 0xe0, 0x11e - 0xe0) == 0;
        char a[96], b[96];
        std::snprintf(a, sizeof(a), "head=%d body=%d mid=%d tail=%d", head, body, mid, tail);
        std::snprintf(b, sizeof(b), "head=1 body=1 mid=1 tail=1");
        RT_DIFF_SHADOW(head && body && mid && tail, a, b);
    }
    return self;
}

// ============================================================================
// Graphics::MeshInstance::MeshInstance(const MeshInstance&)
//   ??0MeshInstance@Graphics@@QEAA@AEBV01@@Z
//   retail 0x1800170c0, ordinal 274
//
// Geometry copy ctor, vtable stamp, then: copy +0xc0; ZERO the three Array
// headers before handing each to its DLL operator= (an operator= that does not
// first see a zeroed destination would free a garbage pointer); copy the
// _instanced byte; copy the eight scalars at +0x100..+0x11c, the last as a WORD.
//
// The zero-then-copy order matters and is the reason this is not a memcpy.
//
// ORACLE: POST-STATE (invariant), and deliberately NOT a scratch comparison.
// Running the original into a scratch buffer would allocate three arrays and
// leave them to be freed, and the only way to free them is ~MeshInstance, which
// runs `Geometry::~Geometry` — and Geometry's copy ctor copied the ATTACH ARRAY,
// so the scratch object's dtor would walk live sibling geometries and clear
// their parent link and their sceneId. That is corruption of live state to run
// an oracle, which rt_diff.h forbids in as many words ("Never let the original
// write into live state"). The check therefore verifies that every field this
// port itself writes equals the source's, plus that the three copied arrays
// have the source's element counts.
// ============================================================================
static void* RtGraphics_MeshInstance_CopyCtor(void* self, const void* other) {
    RT_DIFF_BEGIN("Graphics::MeshInstance::MeshInstance(const&)");

    MeshInstance* mi = reinterpret_cast<MeshInstance*>(self);
    const MeshInstance* src = reinterpret_cast<const MeshInstance*>(other);

    RtGeometryCopyCtor()(self, other);
    mi->_base._vtable = RtMeshInstanceVtable();

    mi->_mesh = src->_mesh;

    mi->_materials._data = nullptr;
    mi->_materials._size = 0;
    mi->_materials._capacity = 0;
    RtMatArrayCopy()(&mi->_materials, &src->_materials);

    mi->_animation._instanced = src->_animation._instanced;

    mi->_animation._layers._data = nullptr;
    mi->_animation._layers._size = 0;
    mi->_animation._layers._capacity = 0;
    RtLayerArrayCopy()(&mi->_animation._layers, &src->_animation._layers);

    mi->_nodes._data = nullptr;
    mi->_nodes._size = 0;
    mi->_nodes._capacity = 0;
    RtNodeArrayCopy()(&mi->_nodes, &src->_nodes);

    mi->_rootAnimationOffset = src->_rootAnimationOffset;
    mi->_color[0] = src->_color[0];
    mi->_color[1] = src->_color[1];
    mi->_color[2] = src->_color[2];
    mi->_color[3] = src->_color[3];
    mi->_sortBias = src->_sortBias;

    if (rt_diff_on__) {
        bool mesh   = mi->_mesh.p == src->_mesh.p;
        bool scal   = std::memcmp(B(self) + 0x100, B(other) + 0x100, 0x11e - 0x100) == 0;
        bool inst   = mi->_animation._instanced == src->_animation._instanced;
        bool counts = mi->_materials._size == src->_materials._size
                   && mi->_animation._layers._size == src->_animation._layers._size
                   && mi->_nodes._size == src->_nodes._size;
        bool vt     = mi->_base._vtable == RtMeshInstanceVtable();
        char a[128], b[128];
        std::snprintf(a, sizeof(a), "mesh=%d scalars=%d instanced=%d counts=%d vtable=%d",
                      mesh, scal, inst, counts, vt);
        std::snprintf(b, sizeof(b), "mesh=1 scalars=1 instanced=1 counts=1 vtable=1");
        RT_DIFF_SHADOW(mesh && scal && inst && counts && vt, a, b);
    }
    return self;
}

// ============================================================================
// Graphics::MeshInstance::~MeshInstance()
//   ??1MeshInstance@Graphics@@UEAA@XZ
//   retail 0x180017060, ordinal 632
//
// Free the node array, the animation layer array and the material array — in
// that order, each through its own DLL Array<T>::~Array — then run
// Geometry::~Geometry, which re-stamps Geometry's vtable, detaches every child,
// and frees the attach array.
//
// TWO MANGLING / LIFETIME NOTES, both recorded landmines:
//  * `??1` and `UEAA`. This dtor is VIRTUAL, so it mangles `UEAA`, not `QEAA`.
//    The non-virtual spelling is a name the DLL does not contain and would
//    resolve to NULL in silence behind whatever guard the caller has
//    (reference_phantom_dtor_mangling_qeaa_vs_ueaa.md). The name registered at
//    the bottom of this file was verified against the export directory.
//  * `MeshInstance` is NOT an `Object::Internal` and does NOT live in the
//    handle table, so the swap-remove that Wave 5 found missing from
//    `~Goal` — and that
//    reference_internalfactory_swapremove_wild_write.md describes — has no
//    analogue here. The class derives from Graphics::Geometry, whose dtor is
//    the only registry-like teardown, and it is called. (`EdgeMesh` DOES derive
//    from Object::External; its dtor is not in this lane's scope and is not
//    ported.)
//
// ORACLE: POST-STATE only. There is no way to run both: the original would
// double-free three buffers. The check asserts the three arrays are cleared,
// which is the observable the DLL's own ~Array guarantees and the thing a
// wrong-offset port would get wrong.
// ============================================================================
static void RtGraphics_MeshInstance_Dtor(void* self) {
    RT_DIFF_BEGIN("Graphics::MeshInstance::~MeshInstance");

    MeshInstance* mi = reinterpret_cast<MeshInstance*>(self);
    RtNodeArrayFree()(&mi->_nodes);
    RtLayerArrayFree()(&mi->_animation._layers);
    RtMatArrayFree()(&mi->_materials);
    RtGeometryDtor()(self);

    if (rt_diff_on__) {
        bool nodes = mi->_nodes._data == nullptr && mi->_nodes._size == 0;
        bool layer = mi->_animation._layers._data == nullptr && mi->_animation._layers._size == 0;
        bool mats  = mi->_materials._data == nullptr && mi->_materials._size == 0;
        bool attach = mi->_base._attached._data == nullptr && mi->_base._attached._size == 0;
        char a[112], b[112];
        std::snprintf(a, sizeof(a), "nodes=%d layers=%d materials=%d attached=%d",
                      nodes, layer, mats, attach);
        std::snprintf(b, sizeof(b), "nodes=1 layers=1 materials=1 attached=1");
        RT_DIFF_SHADOW(nodes && layer && mats && attach, a, b);
    }
}

// ============================================================================
// Graphics::EdgeMesh::TriangleVertex::TriangleVertex(const Math::Vector3&)
//   ??0TriangleVertex@EdgeMesh@Graphics@@QEAA@AEBVVector3@Math@@@Z
//   retail 0x1800a2f50, ordinal 462
//
// Three floats scaled by 512.0f and TRUNCATED toward zero to int16 (the retail
// instruction is CVTTSS2SI into a 32-bit register followed by a 16-bit store,
// which is C's float->int conversion followed by a narrowing store — not a
// round), then a zero int16 at +6. The 512.0f is DAT_180161728, read out of the
// shipped DLL as the bytes 00 00 00 44; it is not an inferred "probably 256".
//
// ORACLE: FULL, through a scratch buffer. No side effects.
// ============================================================================
static void* RtGraphics_EdgeMesh_TriangleVertex_Ctor(void* self, const void* vec3) {
    RT_DIFF_BEGIN("Graphics::EdgeMesh::TriangleVertex::TriangleVertex");

    TriangleVertex* tv = reinterpret_cast<TriangleVertex*>(self);
    tv->x = static_cast<int16_t>(static_cast<int>(Rd<float>(vec3, 0) * kEdgeMeshVertexScale));
    tv->y = static_cast<int16_t>(static_cast<int>(Rd<float>(vec3, 4) * kEdgeMeshVertexScale));
    tv->z = static_cast<int16_t>(static_cast<int>(Rd<float>(vec3, 8) * kEdgeMeshVertexScale));
    tv->w = 0;

    if (rt_diff_on__) {
        TriangleVertex scratch;
        std::memset(&scratch, 0xcd, sizeof(scratch));
        typedef void* (*Fn)(void*, const void*);
        RtOriginal<Fn>("??0TriangleVertex@EdgeMesh@Graphics@@QEAA@AEBVVector3@Math@@@Z")(&scratch, vec3);
        RT_DIFF_BYTES(tv, &scratch, sizeof(scratch));
    }
    return self;
}

// ============================================================================
// Graphics::EdgeMesh::EdgeVertex::EdgeVertex(const TriangleVertex&,
//                                            const TriangleVertex&, int)
//   ??0EdgeVertex@EdgeMesh@Graphics@@QEAA@AEBUTriangleVertex@12@0H@Z
//   retail 0x1800a2df0, ordinal 164
//
// The two vertices' xyz shorts side by side, the int argument truncated to
// int16 in the first half's w, and 0 in the second half's w. The `0` in the
// mangled name is a back-reference to the first TriangleVertex parameter, which
// is why the second argument is also a `const TriangleVertex&` and not the
// non-const reference an eye-parse of the decompile suggests.
//
// ORACLE: FULL, through a scratch buffer. No side effects.
// ============================================================================
static void* RtGraphics_EdgeMesh_EdgeVertex_Ctor(void* self, const void* a, const void* b, int flags) {
    RT_DIFF_BEGIN("Graphics::EdgeMesh::EdgeVertex::EdgeVertex");

    EdgeVertex* ev = reinterpret_cast<EdgeVertex*>(self);
    const TriangleVertex* ta = reinterpret_cast<const TriangleVertex*>(a);
    const TriangleVertex* tb = reinterpret_cast<const TriangleVertex*>(b);

    ev->a.x = ta->x;  ev->b.x = tb->x;
    ev->a.y = ta->y;  ev->b.y = tb->y;
    ev->a.z = ta->z;
    ev->a.w = static_cast<int16_t>(flags);
    ev->b.z = tb->z;
    ev->b.w = 0;

    if (rt_diff_on__) {
        EdgeVertex scratch;
        std::memset(&scratch, 0xcd, sizeof(scratch));
        typedef void* (*Fn)(void*, const void*, const void*, int);
        RtOriginal<Fn>("??0EdgeVertex@EdgeMesh@Graphics@@QEAA@AEBUTriangleVertex@12@0H@Z")(
            &scratch, a, b, flags);
        RT_DIFF_BYTES(ev, &scratch, sizeof(scratch));
    }
    return self;
}

// ============================================================================
// Graphics::EdgeMesh::SetMaterials(ExternalPtr x4)
//   ?SetMaterials@EdgeMesh@Graphics@@QEAAXV?$ExternalPtr@VMaterialInstance@
//    Graphics@@@Object@@000@Z
//   retail 0x1800a2760, ordinal 3910
//
// Four stores — and THE MIDDLE TWO ARE CROSSED. Retail writes arg1 to +0x28,
// arg2 to +0x38, arg3 to +0x30 and arg4 to +0x40. That is not a decompiler
// artefact (the four MOVs name their source registers explicitly) and it is not
// something to tidy up: EdgeMeshInstance::SetMesh reads the same four slots and
// would see a different assignment if this were "corrected". Reproduced as
// shipped; the note is repeated in runtime/types/graphics/Mesh.h so a reader of
// either file meets it.
//
// ORACLE: FULL by replay — four idempotent stores.
// ============================================================================
static void RtGraphics_EdgeMesh_SetMaterials(void* self, ExternalPtr m0, ExternalPtr m1,
                                             ExternalPtr m2, ExternalPtr m3) {
    RT_DIFF_BEGIN("Graphics::EdgeMesh::SetMaterials");

    EdgeMesh* em = reinterpret_cast<EdgeMesh*>(self);
    em->_material0 = m0;    // +0x28  <- arg 1
    em->_material2 = m1;    // +0x38  <- arg 2
    em->_material1 = m2;    // +0x30  <- arg 3   (crossed, see above)
    em->_material3 = m3;    // +0x40  <- arg 4

    if (rt_diff_on__) {
        uint8_t ours[0x20];
        std::memcpy(ours, B(self) + 0x28, sizeof(ours));
        typedef void (*Fn)(void*, ExternalPtr, ExternalPtr, ExternalPtr, ExternalPtr);
        RtOriginal<Fn>(
            "?SetMaterials@EdgeMesh@Graphics@@QEAAXV?$ExternalPtr@VMaterialInstance@Graphics@@@Object@@000@Z")(
            self, m0, m1, m2, m3);
        RT_DIFF_BYTES(ours, B(self) + 0x28, sizeof(ours));
    }
}

// ============================================================================
// Graphics::EdgeMesh::EdgeMesh()
//   ??0EdgeMesh@Graphics@@QEAA@XZ
//   retail 0x1800a2830, ordinal 161
//
// Object::External's ctor inlined (stamp Base's vtable, then External's, then
// zero +0x08/+0x10/+0x18), then EdgeMesh's own vtable, then zero +0x20..+0x40,
// then eight (buffer header, PlatformConstruct) pairs, then the two list Arrays
// and an EMPTY bounds box (min = +1e8, max = -1e8; the constants 0x4cbebc20 and
// 0xccbebc20 were read out of the DLL, not guessed).
//
// The three successive vtable stamps at +0x00 are the base-class chain MSVC
// emits and only the last one survives; they are reproduced because a partially
// constructed object is observable if any of the eight PlatformConstruct calls
// re-enters (it calls into dx11/ through Device::_singleton's plugin).
//
// ORACLE: POST-STATE only, and the reason is not laziness.
// `Video::VertexBuffer::PlatformConstruct` (0x1800f57e0) asks
// `Video::Device::_singleton`'s plugin for a companion object AND LINKS THE
// BUFFER INTO THE DEVICE'S GLOBAL INTRUSIVE LIST (`_firstBuffer`, under the
// device's critical section). Running the original into a scratch buffer would
// therefore register eight phantom buffers in a live global list and create
// eight orphan dx11 companions — real corruption of live state, which rt_diff.h
// forbids. The check verifies the fields THIS port writes, and states plainly
// that the eight PlatformConstruct calls are unchecked because they are the
// DLL's own, called unchanged.
// ============================================================================
static void* RtGraphics_EdgeMesh_Ctor(void* self) {
    RT_DIFF_BEGIN("Graphics::EdgeMesh::EdgeMesh()");

    EdgeMesh* em = reinterpret_cast<EdgeMesh*>(self);

    em->_vtable = RtObjectBaseVtable();
    em->_vtable = RtObjectExternalVtable();
    em->_externalA = nullptr;
    em->_externalB = nullptr;
    em->_externalC = nullptr;
    em->_vtable = RtEdgeMeshVtable();
    em->_reserved20 = nullptr;
    em->_material0.p = nullptr;
    em->_material1.p = nullptr;
    em->_material2.p = nullptr;
    em->_material3.p = nullptr;

    VideoBufferHeader* const vbufs[4] = { &em->_triVerts, &em->_edgeVerts,
                                          &em->_capVerts, &em->_extraVerts };
    VideoBufferHeader* const ibufs[4] = { &em->_triIndices, &em->_edgeIndices,
                                          &em->_capIndices, &em->_extraIndices };
    for (int i = 0; i < 4; ++i) {
        // Retail's order is strictly vertex, index, vertex, index, ... and each
        // pair is (zero the dword, zero the flag byte, PlatformConstruct).
        vbufs[i]->_size = 0;  vbufs[i]->_flags = 0;  RtVertexBufferPlatformCtor()(vbufs[i]);
        ibufs[i]->_size = 0;  ibufs[i]->_flags = 0;  RtIndexBufferPlatformCtor()(ibufs[i]);
    }

    em->_triangleLists._data = nullptr;
    em->_triangleLists._size = 0;
    em->_triangleLists._capacity = 0;
    em->_edgeLists._data = nullptr;
    em->_edgeLists._size = 0;
    em->_edgeLists._capacity = 0;

    float* bounds = reinterpret_cast<float*>(B(self) + 0x168);
    const uint32_t kPlus  = 0x4cbebc20u;   //  +1e8f
    const uint32_t kMinus = 0xccbebc20u;   //  -1e8f
    for (int i = 0; i < 3; ++i) std::memcpy(&bounds[i],     &kPlus,  4);
    for (int i = 0; i < 3; ++i) std::memcpy(&bounds[3 + i], &kMinus, 4);

    if (rt_diff_on__) {
        bool vt   = em->_vtable == RtEdgeMeshVtable();
        bool ext  = em->_externalA == nullptr && em->_externalB == nullptr
                 && em->_externalC == nullptr && em->_reserved20 == nullptr;
        bool mats = em->_material0.p == nullptr && em->_material1.p == nullptr
                 && em->_material2.p == nullptr && em->_material3.p == nullptr;
        bool lists = em->_triangleLists._data == nullptr && em->_triangleLists._size == 0
                  && em->_edgeLists._data == nullptr && em->_edgeLists._size == 0;
        uint8_t wantBounds[24];
        for (int i = 0; i < 3; ++i) std::memcpy(wantBounds + i * 4,        &kPlus,  4);
        for (int i = 0; i < 3; ++i) std::memcpy(wantBounds + 12 + i * 4,   &kMinus, 4);
        bool box = std::memcmp(bounds, wantBounds, sizeof(wantBounds)) == 0;
        char a[128], b[128];
        std::snprintf(a, sizeof(a), "vtable=%d external=%d materials=%d lists=%d bounds=%d",
                      vt, ext, mats, lists, box);
        std::snprintf(b, sizeof(b), "vtable=1 external=1 materials=1 lists=1 bounds=1");
        RT_DIFF_SHADOW(vt && ext && mats && lists && box, a, b);
    }
    return self;
}

// ============================================================================
// Graphics::EdgeMesh::Set(...)  — 25 parameters
//   ?Set@EdgeMesh@Graphics@@QEAA_NHPEBUTriangleList@GraphicsMesh@2@H0HPEAGHPEAX
//    HH1H2HH1H2HH1H2HAEBVBox@Volume@@@Z
//   retail 0x1800a23e0, ordinal 3813
//
// Copy the bounds box, resize and fill the two TriangleList arrays, then upload
// four (index, vertex) buffer pairs and return whether every upload succeeded.
// Each upload is two VIRTUAL calls on the embedded Video buffer object: slot 2
// at vt+0x08 (index 1) to size it and vt+0x10 (index 2) to fill it, and any false
// short-circuits the whole chain — the nesting in the decompile is a chain of
// `if (a && b) { if (c && d) { ... } }`, so a failure at pair 2 leaves pairs 3
// and 4 untouched. That early-out is behaviour, not an optimisation, and is
// reproduced.
//
// The four pairs are, in retail's order:
//   indices @ +0x70 (count*2), vertices @ +0x50 (stride*count)
//   indices @ +0xb0,           vertices @ +0x90
//   indices @ +0xf0,           vertices @ +0xd0
//   indices @ +0x130,          vertices @ +0x110
// Note the INDEX buffer of each pair is uploaded FIRST and that the buffer
// object pointer is at header + 0x08 (what PlatformConstruct wrote), which is
// why the offsets above are 0x08 past the header offsets in the type header.
//
// The 25-parameter signature is counted against the MANGLED NAME, not against
// the decompiler's parameter list — the recorded rule for anything past the
// fourth argument (reference_hidden_retval_and_int_pointer_args.md). Reading it
// out: (int, const TriangleList*, int, const TriangleList*, then four groups of
// (int, ushort*, int, void*, int, int), then const Box&) — where `1` and `2`
// are back-references to `PEAG` (ushort*) and `PEAX` (void*).
//
// ORACLE: POST-STATE only. The four uploads go to the GPU through dx11/; a
// replay would re-create and re-fill four vertex and four index buffers per
// call, and the second Create on an already-sized buffer is not a proven no-op.
// The check verifies the bounds copy and the two array sizes — the part of the
// body that is this port's own arithmetic — and leaves the eight virtual
// uploads to the return value, which the caller already tests.
// ============================================================================
static bool RtGraphics_EdgeMesh_Set(void* self,
                                    int triCount,  const void* triLists,
                                    int edgeCount, const void* edgeLists,
                                    int i0, uint16_t* ib0, int v0s, void* vb0, int v0n, int v0x,
                                    uint16_t* ib1, int v1s, void* vb1, int v1n, int v1x,
                                    uint16_t* ib2, int v2s, void* vb2, int v2n, int v2x,
                                    uint16_t* ib3, int v3s, void* vb3, int v3n,
                                    const void* box) {
    RT_DIFF_BEGIN("Graphics::EdgeMesh::Set");

    EdgeMesh* em = reinterpret_cast<EdgeMesh*>(self);

    for (int i = 0; i < 6; ++i)
        std::memcpy(B(self) + 0x168 + i * 4, B(box) + i * 4, 4);

    RtTriListArrayResize()(&em->_triangleLists, triCount);
    for (int i = 0; i < em->_triangleLists._size; ++i)
        std::memcpy(em->_triangleLists._data + i * 0x18, B(triLists) + i * 0x18, 0x18);

    RtTriListArrayResize()(&em->_edgeLists, edgeCount);
    for (int i = 0; i < em->_edgeLists._size; ++i)
        std::memcpy(em->_edgeLists._data + i * 0x18, B(edgeLists) + i * 0x18, 0x18);

    // vt[1] = Create(bytes, flags, data=null); vt[2] = SetData(bytes, flags, data).
    typedef bool (*BufCreateFn)(void* buf, int bytes, int flags, void* data);
    struct Upload {
        size_t   headerOff;   // the buffer OBJECT pointer, header + 0x08
        int      bytes;
        void*    data;
    };
    const Upload uploads[8] = {
        { 0x70, i0  * 2,   ib0 },  { 0x50, v0s * v0n, vb0 },
        { 0xb0, v0x * 2,   ib1 },  { 0x90, v1s * v1n, vb1 },
        { 0xf0, v1x * 2,   ib2 },  { 0xd0, v2s * v2n, vb2 },
        { 0x130, v2x * 2,  ib3 },  { 0x110, v3s * v3n, vb3 },
    };

    bool ours = true;
    for (int i = 0; i < 8 && ours; ++i) {
        void* buf = Rd<void*>(self, uploads[i].headerOff);
        void** vt = *reinterpret_cast<void***>(buf);
        BufCreateFn create = reinterpret_cast<BufCreateFn>(vt[1]);   // vt + 0x08
        BufCreateFn fill   = reinterpret_cast<BufCreateFn>(vt[2]);   // vt + 0x10
        ours = create(buf, uploads[i].bytes, 0, nullptr)
            && fill(buf, uploads[i].bytes, 0, uploads[i].data);
    }

    if (rt_diff_on__) {
        bool boxOk = std::memcmp(B(self) + 0x168, box, 24) == 0;
        bool sizes = em->_triangleLists._size == triCount && em->_edgeLists._size == edgeCount;
        char a[112], b[112];
        std::snprintf(a, sizeof(a), "bounds=%d sizes=%d uploads=%d(unchecked)",
                      boxOk, sizes, ours);
        std::snprintf(b, sizeof(b), "bounds=1 sizes=1 uploads=%d(unchecked)", ours);
        RT_DIFF_SHADOW(boxOk && sizes, a, b);
    }
    return ours;
}

// ============================================================================
// Graphics::EdgeMeshInstance::EdgeMeshInstance()
//   ??0EdgeMeshInstance@Graphics@@QEAA@XZ
//   retail 0x1800a3ab0, ordinal 163
//
// Geometry base ctor, vtable stamp, and ONE store: `_mesh = 0`.
//
// *** +0xc8 IS DELIBERATELY LEFT UNINITIALISED, BECAUSE RETAIL LEAVES IT
// UNINITIALISED. *** The copy ctor (0x1800174f0) copies both +0xc0 and +0xc8,
// so the field exists and is propagated, but the default ctor never writes it;
// only EdgeMeshInstance::SetMesh does. This is an uninitialised read in the
// SHIPPED DLL of exactly the kind Wave 5 found in `RemoveMesh`
// (`Entry::_userValue2`). Zeroing it here would make our ctor differ from
// retail's on a live shared object, which is the one thing WAVE6.md says a
// Video/Graphics lane must not do.
//
// ORACLE: FULL, through a scratch buffer — `Geometry::Geometry` has no side
// effects. +0xc8 is excluded from the comparison for the reason above: two
// different stacks hold two different pieces of garbage there, and comparing
// them would report a divergence on every single call.
// ============================================================================
static void* RtGraphics_EdgeMeshInstance_Ctor(void* self) {
    RT_DIFF_BEGIN("Graphics::EdgeMeshInstance::EdgeMeshInstance()");

    EdgeMeshInstance* emi = reinterpret_cast<EdgeMeshInstance*>(self);
    RtGeometryCtor()(self);
    emi->_base._vtable = RtEdgeMeshInstanceVtable();
    emi->_mesh.p = nullptr;
    // NO STORE TO _followed (+0xc8). See the banner.

    if (rt_diff_on__) {
        uint8_t scratch[0xd0];
        std::memset(scratch, 0xcd, sizeof(scratch));
        typedef void* (*Fn)(void*);
        RtOriginal<Fn>("??0EdgeMeshInstance@Graphics@@QEAA@XZ")(scratch);
        // Skip Geometry's uninitialised gaps (+0x14, +0xbc) and _followed (+0xc8).
        bool head = std::memcmp(B(self) + 0x00, scratch + 0x00, 0x14) == 0;
        bool body = std::memcmp(B(self) + 0x18, scratch + 0x18, 0xbc - 0x18) == 0;
        bool mesh = std::memcmp(B(self) + 0xc0, scratch + 0xc0, 8) == 0;
        char a[112], b[112];
        std::snprintf(a, sizeof(a), "head=%d body=%d mesh=%d followed=skipped(uninit)",
                      head, body, mesh);
        std::snprintf(b, sizeof(b), "head=1 body=1 mesh=1 followed=skipped(uninit)");
        RT_DIFF_SHADOW(head && body && mesh, a, b);
    }
    return self;
}

// ============================================================================
// Graphics::EdgeMeshInstance::EdgeMeshInstance(const EdgeMeshInstance&)
//   ??0EdgeMeshInstance@Graphics@@QEAA@AEBV01@@Z
//   retail 0x1800174f0, ordinal 162
//
// Geometry copy ctor, vtable stamp, copy +0xc0 and +0xc8. Note that copying
// +0xc8 propagates whatever the source holds there, INCLUDING the garbage a
// default-constructed source still has — again, faithful.
//
// A STALE COMMENT THIS PORT DISPROVES, quoted so the next reader recognises it.
// `engine/funcs/engine_funcs.h:5515` says
//     "// ??0EdgeMeshInstance@Graphics@@QEAA@XZ   (Runtime 0x1800174f0)"
// and `engine/funcs/engine_funcs.cpp:3389-3392` uses that address as a FALLBACK
// for the DEFAULT ctor, taken when the by-name resolve of the default ctor
// returns null. The export directory of the shipped DLL says 0x1800174f0
// is the COPY ctor (ordinal 162) and the default ctor is 0x1800a3ab0 (ordinal
// 163). The fallback is latent, not live — the name resolves — but if it ever
// fired it would run the copy ctor with an uninitialised RDX and dereference it
// twice. Lane E is not allowed to edit engine/funcs/, so the correction is
// reported to the wave lead instead; see WAVE6_LANE_E.md.
//
// ORACLE: POST-STATE (invariant), for the same reason as MeshInstance's copy
// ctor: destroying a scratch copy would run Geometry::~Geometry over an attach
// array shared with the source and clear live siblings' parent links.
// ============================================================================
static void* RtGraphics_EdgeMeshInstance_CopyCtor(void* self, const void* other) {
    RT_DIFF_BEGIN("Graphics::EdgeMeshInstance::EdgeMeshInstance(const&)");

    EdgeMeshInstance* emi = reinterpret_cast<EdgeMeshInstance*>(self);
    const EdgeMeshInstance* src = reinterpret_cast<const EdgeMeshInstance*>(other);

    RtGeometryCopyCtor()(self, other);
    emi->_base._vtable = RtEdgeMeshInstanceVtable();
    emi->_mesh = src->_mesh;
    emi->_followed = src->_followed;

    if (rt_diff_on__) {
        bool mesh = emi->_mesh.p == src->_mesh.p;
        bool foll = emi->_followed == src->_followed;
        bool vt   = emi->_base._vtable == RtEdgeMeshInstanceVtable();
        char a[112], b[112];
        std::snprintf(a, sizeof(a), "mesh=%d followed=%d vtable=%d", mesh, foll, vt);
        std::snprintf(b, sizeof(b), "mesh=1 followed=1 vtable=1");
        RT_DIFF_SHADOW(mesh && foll && vt, a, b);
    }
    return self;
}

// ============================================================================
// Graphics::EdgeMeshInstance::SetMesh(ExternalPtr<EdgeMesh>, MeshInstance*)
//   ?SetMesh@EdgeMeshInstance@Graphics@@QEAAXV?$ExternalPtr@VEdgeMesh@
//    Graphics@@@Object@@PEAVMeshInstance@2@@Z
//   retail 0x1800a3a20, ordinal 3914
//
// Store the followed MeshInstance FIRST (+0xc8) and the mesh handle second
// (+0xc0) — retail's order, kept — then OR the four materials' type flags into
// Geometry::_materialTypeFlags in the order +0x38, +0x28, +0x40, +0x30, then
// Geometry::SetBounds(mesh->_bounds).
//
// The OR order is observable: retail writes _materialTypeFlags back to memory
// after EACH of the four ORs (the decompile shows four separate stores), so a
// re-entrant read between them would see the partial value. Reproduced.
//
// ORACLE: FULL by replay. Two idempotent stores, four idempotent ORs, and
// SetBounds — whose own body compares before it writes and only ORs a dirty bit
// — so a second call changes nothing.
// ============================================================================
static void RtGraphics_EdgeMeshInstance_SetMesh(void* self, ExternalPtr mesh, void* followed) {
    RT_DIFF_BEGIN("Graphics::EdgeMeshInstance::SetMesh");

    EdgeMeshInstance* emi = reinterpret_cast<EdgeMeshInstance*>(self);
    emi->_followed = reinterpret_cast<MeshInstance*>(followed);
    emi->_mesh = mesh;

    const size_t order[4] = { 0x38, 0x28, 0x40, 0x30 };
    for (int i = 0; i < 4; ++i) {
        const void* mat = Rd<void*>(mesh.p, order[i]);
        emi->_base._materialTypeFlags |=
            1u << (static_cast<uint8_t>(Rd<int32_t>(mat, 0x20)) & 0x1f);
    }

    RtGeometrySetBounds()(self, B(mesh.p) + 0x168);

    if (rt_diff_on__) {
        uint8_t ours[0xd0];
        std::memcpy(ours, self, sizeof(ours));
        typedef void (*Fn)(void*, ExternalPtr, void*);
        RtOriginal<Fn>(
            "?SetMesh@EdgeMeshInstance@Graphics@@QEAAXV?$ExternalPtr@VEdgeMesh@Graphics@@@Object@@PEAVMeshInstance@2@@Z")(
            self, mesh, followed);
        // Skip Geometry's two uninitialised gaps (+0x14, +0xbc).
        bool head = std::memcmp(ours + 0x00, self, 0x14) == 0;
        bool body = std::memcmp(ours + 0x18, B(self) + 0x18, 0xbc - 0x18) == 0;
        bool tail = std::memcmp(ours + 0xc0, B(self) + 0xc0, 0x10) == 0;
        char a[112], b[112];
        std::snprintf(a, sizeof(a), "head=%d body=%d tail=%d", head, body, tail);
        std::snprintf(b, sizeof(b), "head=1 body=1 tail=1");
        RT_DIFF_SHADOW(head && body && tail, a, b);
    }
}

// ============================================================================
// Registration.
//
// Every name below is a string LITERAL at the registration call, never one of
// the local constants, because tools/runtime_census.py cannot see through a
// NAMED CONSTANT — which is exactly how Wave 3's Object count stood at 21/41
// when it was really 27/41 for a whole wave
// (reference_census_cannot_see_nonliteral_registrations.md). The census CAN
// join adjacent literals since Wave 5 lane E added concat_literal() to both
// tools, so the long template names are wrapped for readability.
//
// GetNodeIndexFromName is ALSO registered by ADDRESS. It is the one function in
// this lane with a live `ResolveAddr(Module::Runtime, ...)` call site
// (code/Game/Action/showtool.cpp:169); leaving it name-only would put the
// held-item bone lookup on the DLL while every other consumer used the port —
// two implementations taking turns, which is the failure mode Wave 5 lane D
// documented for Picking::Scene::AddMesh.
//
// The four functions in this lane's scope that are NOT registered are listed
// below the register block, each with its reason.
// ============================================================================
int RtGraphicsMeshRegister() {
    int count = 0;

    // ---- Graphics::MeshInstance ----
    RegisterRuntimeExport("??0MeshInstance@Graphics@@QEAA@XZ",
                          (void*)&RtGraphics_MeshInstance_Ctor); ++count;
    RegisterRuntimeExport("??0MeshInstance@Graphics@@QEAA@AEBV01@@Z",
                          (void*)&RtGraphics_MeshInstance_CopyCtor); ++count;
    RegisterRuntimeExport("??1MeshInstance@Graphics@@UEAA@XZ",
                          (void*)&RtGraphics_MeshInstance_Dtor); ++count;
    RegisterRuntimeExport("?IsAnimating@MeshInstance@Graphics@@QEBA_NXZ",
                          (void*)&RtGraphics_MeshInstance_IsAnimating); ++count;
    RegisterRuntimeExport("?GetNodeToWorld@MeshInstance@Graphics@@QEBA?AVTransform@Math@@H@Z",
                          (void*)&RtGraphics_MeshInstance_GetNodeToWorld); ++count;
    RegisterRuntimeExport("?RootAnimationOffset@MeshInstance@Graphics@@QEBA?AVVector3@Math@@XZ",
                          (void*)&RtGraphics_MeshInstance_RootAnimationOffset); ++count;
    RegisterRuntimeExport("?UpdateHierarchy@MeshInstance@Graphics@@QEAAXXZ",
                          (void*)&RtGraphics_MeshInstance_UpdateHierarchy); ++count;
    RegisterRuntimeExport("?SetMaterialInstance@MeshInstance@Graphics@@QEAAXHV?$ExternalPtr@"
                          "VMaterialInstance@Graphics@@@Object@@@Z",
                          (void*)&RtGraphics_MeshInstance_SetMaterialInstance); ++count;
    RegisterRuntimeExport("?SetMesh@MeshInstance@Graphics@@QEAAXV?$ExternalPtr@"
                          "VGraphicsMesh@Graphics@@@Object@@@Z",
                          (void*)&RtGraphics_MeshInstance_SetMesh); ++count;
    RegisterRuntimeExport("?PlayAnimation@MeshInstance@Graphics@@QEAAXV?$ExternalPtr@"
                          "VAnimationGroup@Graphics@@@Object@@HAEBVTime@System@@1M@Z",
                          (void*)&RtGraphics_MeshInstance_PlayAnimation); ++count;

    // ---- Graphics::GraphicsMesh ----
    RegisterRuntimeExport("?GetMaterialList@GraphicsMesh@Graphics@@QEBAAEBV?$Array@"
                          "V?$ExternalPtr@VMaterialInstance@Graphics@@@Object@@@Collection@@H@Z",
                          (void*)&RtGraphics_GraphicsMesh_GetMaterialList); ++count;
    RegisterRuntimeExport("?GetNodeIndexFromName@GraphicsMesh@Graphics@@QEBAHPEBD@Z",
                          (void*)&RtGraphics_GraphicsMesh_GetNodeIndexFromName); ++count;
    RegisterRuntimeAddr(0x1800fee90, (void*)&RtGraphics_GraphicsMesh_GetNodeIndexFromName);

    // ---- Graphics::EdgeMesh ----
    RegisterRuntimeExport("??0EdgeMesh@Graphics@@QEAA@XZ",
                          (void*)&RtGraphics_EdgeMesh_Ctor); ++count;
    RegisterRuntimeExport("??0TriangleVertex@EdgeMesh@Graphics@@QEAA@AEBVVector3@Math@@@Z",
                          (void*)&RtGraphics_EdgeMesh_TriangleVertex_Ctor); ++count;
    RegisterRuntimeExport("??0EdgeVertex@EdgeMesh@Graphics@@QEAA@AEBUTriangleVertex@12@0H@Z",
                          (void*)&RtGraphics_EdgeMesh_EdgeVertex_Ctor); ++count;
    RegisterRuntimeExport("?Set@EdgeMesh@Graphics@@QEAA_NHPEBUTriangleList@GraphicsMesh@2@"
                          "H0HPEAGHPEAXHH1H2HH1H2HH1H2HAEBVBox@Volume@@@Z",
                          (void*)&RtGraphics_EdgeMesh_Set); ++count;
    RegisterRuntimeExport("?SetMaterials@EdgeMesh@Graphics@@QEAAXV?$ExternalPtr@"
                          "VMaterialInstance@Graphics@@@Object@@000@Z",
                          (void*)&RtGraphics_EdgeMesh_SetMaterials); ++count;

    // ---- Graphics::EdgeMeshInstance ----
    RegisterRuntimeExport("??0EdgeMeshInstance@Graphics@@QEAA@XZ",
                          (void*)&RtGraphics_EdgeMeshInstance_Ctor); ++count;
    RegisterRuntimeExport("??0EdgeMeshInstance@Graphics@@QEAA@AEBV01@@Z",
                          (void*)&RtGraphics_EdgeMeshInstance_CopyCtor); ++count;
    RegisterRuntimeExport("?SetMesh@EdgeMeshInstance@Graphics@@QEAAXV?$ExternalPtr@"
                          "VEdgeMesh@Graphics@@@Object@@PEAVMeshInstance@2@@Z",
                          (void*)&RtGraphics_EdgeMeshInstance_SetMesh); ++count;

    return count;   // 20
}

// ============================================================================
// HELD BACK — the four names in this lane's scope that are NOT registered.
// ============================================================================
//
// 1. ?AddDrawItems@MeshInstance@Graphics@@UEBAXAEBVScene@2@AEAVRenderer@2@@Z
//    retail 0x180100f20, ordinal 1550.
//    TWO independent reasons, either of which is sufficient.
//    (a) It is VIRTUAL and it is the DLL that dispatches it. Objects built by
//        the ctor above carry ??_7MeshInstance@Graphics@@6B@, the DLL's own
//        vtable, and the only caller is the DLL's Graphics::Renderer walking
//        the scene. Registering the export would redirect nothing that runs;
//        the port would be a registered function with zero callers, which
//        reference_uncalled_port_is_not_verified.md is explicitly about.
//    (b) Registering it by ADDRESS would be ACTIVE HARM. Our tree's only use of
//        0x180100f20 is `standalone/scene_watch.cpp:401`, and it is not a call
//        — it is an IDENTITY TOKEN. `IsMeshInstance()` resolves the address and
//        compares it against `vtable[3]` of a live object to decide whether the
//        object is a MeshInstance. Register the address and ResolveAddr hands
//        back OUR function, the compare fails for every object, and
//        `BANISHED_LOD_CENSUS` / `BANISHED_FORCE_LOD0` silently see an empty
//        scene. Exactly the shape of `0x1400f2f00` in PLAN §1.1 ("must stay
//        unregistered": registering makes the dispatcher call itself).
//    The body is reversed and understood (it is transcribed in
//    WAVE6_LANE_E.md); it depends on Graphics::Renderer::AddDrawItem, which is
//    lane F's. If lane F ever needs it, it must first give scene_watch a
//    different identity token.
//
// 2. ?Serialize@MeshInstance@Graphics@@QEAAXAEAVSerializer@IO@@@Z
//    retail 0x1800ff680, ordinal 3707.
//    NO SAFE ORACLE, and the failure mode is the worst one this project has.
//    The body forwards to AnimationState::Serialize, then serialises the
//    material count through the Encoding vtable slot at +0xa0, then (on the
//    read path) resizes the material array and streams it. A differential run
//    would consume or emit the record TWICE and desync the stream — and a
//    stream desync does not surface where it happens, it surfaces later on an
//    innocent record (reference_slot3_wrapper_must_resolve_first.md,
//    reference_serialize_lazy_resolve_and_wrong_slot.md). Wave 5 held four
//    functions back for the same reason (constructing an IO::Serializer emits a
//    12-byte header and admits no oracle) and this lane applies that standard
//    unchanged. There is a second, independent hazard: the Encoding vtable
//    OFFSETS DIFFER between retail and the profile build (+0xa0 here vs +0xb8
//    there, reference_encoding_vtable_offsets_differ_between_builds.md), so a
//    port written from the profile decompile would desync every save while
//    looking correct.
//    It stays on the DLL, where dispatch slot 0x140152990 already points it.
//
// 3. ?_rootOnlyUpdates@MeshInstance@Graphics@@0_NA
//    retail 0x1801fc70b, ordinal 4796.
//    A DATA export — a single static bool, not a function. The runtime registry
//    redirects CALLS; there is nothing to register, and its three consumers
//    (citizenai_executor.cpp, address_registry.cpp:1086,
//    perf_experimental.cpp:125) already resolve it correctly and read/write it
//    in place. Wave 3 and Wave 4 left `FileSystem::_singleton` and
//    `MemoryAllocator::_singleton` unregistered for the same reason: a data
//    symbol cannot be registered before cutover. Verified present in the export
//    directory so that the counts in WAVE6.md are not silently short by one.
//
// 4. ?_typeInfo@GraphicsMesh@Graphics@@0VBaseTypeInfo@Object@@A
//    retail 0x1801fa708, ordinal 4869.
//    Also a DATA export — the class's Object::BaseTypeInfo instance, resolved
//    by `standalone/reg_unreg_lane3.cpp:193` and stored into an exe .rdata
//    slot. Same reasoning as 3.
//
// ============================================================================
// NOT A HOLD-BACK, BUT SAY IT PLAINLY: the EdgeMesh family may not execute.
//
// `standalone/address_registry.cpp:584` describes RegisterPlayerRenderBatch —
// the selection-outline chain that is the ONLY consumer of EdgeMesh,
// EdgeMeshInstance and the two vertex ctors — as "GATED default-OFF on
// BANISHED_SELECTION_OUTLINE". If that is still true, all seven EdgeMesh ports
// above will report `calls=0` in the diff summary, and a port with zero callers
// is not verified no matter how clean the run looks
// (reference_uncalled_port_is_not_verified.md). The wave lead should take one
// run with BANISHED_SELECTION_OUTLINE=1 and a building selected, and read the
// `calls=` column, not just `divergences=`.
// ============================================================================
