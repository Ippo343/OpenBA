# Sources from the tree that the unit tests link against. One family per line; keep it
# to the classes under test and what they need (allocator, crc). Each core batch appends
# with `UNIT_TREE_SRCS +=` so parallel batches merge cleanly.
UNIT_TREE_SRCS =

# MATH-CORE (ENDGAME B.3.1 step 1): the Math value classes plus the transcendental
# seam they call through. rt_libm.cpp installs the ported MSVC CRT bodies
# (msvc_libm.cpp) behind RtLibm::Sqrt/Sin/Cos/Atan2/Pow, and its Init() also
# registers the CRT qsort pair, so msvc_qsort.cpp comes along. The port-registry
# symbols these files reference are stubbed in tests/unit/rt_port_stub.cpp.
UNIT_TREE_SRCS += runtime/src/math/vector.cpp \
                  runtime/src/math/transform.cpp \
                  runtime/src/math/from_lookat.cpp \
                  runtime/src/math/wave9_lane_p.cpp \
                  runtime/src/math/wave9_lane_gfxr.cpp \
                  runtime/funcs/rt_libm.cpp \
                  runtime/src/crt/msvc_libm.cpp \
                  runtime/src/crt/msvc_qsort.cpp
# ENDGAME B.3.1 String/Filename core (tests/unit/string_test.cpp, filename_test.cpp).
UNIT_TREE_SRCS += \
  runtime/src/system/string.cpp \
  runtime/src/system/filename.cpp

# ENDGAME B.3.1 smart-pointer core (tests/unit/object_ptr_test.cpp): header-only.
# Object::ExternalPtr<T> / InternalBasePtr / InternalPtr<T> are non-owning views
# whose whole host-testable surface is inline in runtime/types/object/*.h; their
# only out-of-line body (Serialize) needs the live serializer + factory and is
# covered by the real run, not by make unit. No tree sources to link.
UNIT_TREE_SRCS +=

# ENDGAME B.3.2 Array core (tests/unit/array_test.cpp). array.cpp carries
# `rtcoll::SetSizeCore`, `ArrayDtorImpl` and every `kSpec_*` row the template
# wraps; the tests drive those, not a copy of them. Its allocator seam falls
# back to malloc under BANISHED_UNIT_TEST (same seam as string.cpp's), and its
# by-address doors go through tests/unit/rt_port_stub.cpp's ResolveAddr.
UNIT_TREE_SRCS += runtime/src/collection/array.cpp runtime/src/collection/array_destroy.cpp
# ENDGAME B.3.2 closing pass: the ONE allocator door the containers and the
# ported collection cores share (tests/unit/alloc_door_test.cpp). It contains no
# allocator — it answers with the function to call, reading the exe dispatch
# slot through `ResolveAddr`, which rt_port_stub.cpp's `g_unitResolveAddr` hook
# lets the test drive. Nothing else in the suite reaches it: under
# BANISHED_UNIT_TEST array.cpp and linkedlist.cpp take their malloc seams.
UNIT_TREE_SRCS += runtime/src/collection/alloc_door.cpp
# ENDGAME B.3.2 Set core (tests/unit/set_test.cpp): header-only on the host.
# `Collection::Set<T, Compare, Allocator, Key>` lives in
# runtime/types/collection/Collection.h. Its out-of-line half — the two
# allocator doors, retail's inlined strcmp and one forwarder per ported AVL
# core — is runtime/src/collection/set_core.cpp, which stands on the address
# registry and the allocator singleton; under BANISHED_UNIT_TEST the header
# compiles the generic core instead (the same bodies over a node type), and the
# test drives it with a counting allocator policy. The ported cores are
# verified by the real run, not here. No tree sources to link.
UNIT_TREE_SRCS +=
# ENDGAME B.3.2 LIST core (tests/unit/list_test.cpp): Collection::LinkedList<T>
# and the two ported introsorts. linkedlist.cpp holds the node-allocation seam
# the header template calls through (and, under -DBANISHED_UNIT_TEST, the
# counting test allocator behind it — B31_RULES class rule 6); introsort.cpp
# holds rtsort::SortByKey70 (0x1800264a0). The shared MSVC sort skeleton is
# header-only (runtime/src/collection/introsort_msvc.h). Both files reach
# ResolveAddr, stubbed in tests/unit/rt_port_stub.cpp.
UNIT_TREE_SRCS += \
  runtime/src/collection/linkedlist.cpp \
  runtime/src/collection/introsort.cpp
# ENDGAME B.3.3 step 3 reflection core (tests/unit/object_typeinfo_test.cpp):
# `Object::BaseTypeInfo` and `Object::Factory` as real classes on
# `Collection::Set`. type_reflection.cpp is deliberately registry-free — the ABI
# shims and the mangled-name/address doors stay in type_system.cpp, which cannot
# link here (rt_port_stub.cpp does not stub `RtResolveCached`). Its allocator
# seam (`Object::TypeTreeAlloc`/`TypeChildAlloc`) takes a malloc branch under
# -DBANISHED_UNIT_TEST, and under that same define `Collection::Set`'s
# `AvlOps<Node,0x30>` falls back to the generic core, so set_core.cpp is not
# needed either.
UNIT_TREE_SRCS += runtime/src/object/type_reflection.cpp
# ENDGAME B.3.3 step 5, EXTERNAL (tests/unit/object_external_test.cpp):
# `Object::External` / `CompiledExternal` and the two header-inline tree finds.
# external_class.cpp is registry-free by construction — its reflection-record
# accessors take a test seam under -DBANISHED_UNIT_TEST and its only other
# dependencies are `Object::TypeTreeAlloc/Free` (type_reflection.cpp, already
# linked above), `System::Filename` (filename.cpp, likewise) and
# `RtSystem::RtMemFree` (string.cpp, likewise).
#
# `external_class_doors.cpp` is deliberately NOT here and must not be added:
# every body in it is a door into the port registry, and it is also where
# `Object::ExternalList`'s vtable is emitted for exactly that reason (GCC emits
# a vtable in the TU defining the key function, and that vtable references every
# slot — including the four that reach the registry). Nor are the thirteen
# existing `external*.cpp` bodies: they stand on `RT_EXPORT_ONCE`,
# `RT_DATA_SLOT` and `RtResolveCached`, none of which rt_port_stub.cpp provides.
# Those are verified by the real run, not here.
UNIT_TREE_SRCS += runtime/src/object/external_class.cpp

# ENDGAME B.3.3 step 5 INTERNAL core (tests/unit/object_internal_test.cpp):
# `Object::Internal` and `Object::InternalFactory` as real classes.
# internal_class.cpp is registry-free by construction — the `RtObject_*` ABI
# shims, the mangled-name/address doors and the attendance counters stay in
# runtime/src/object/internal.cpp, which cannot link here. Its seam
# (`Object::detail::InternalMemAlloc`/`InternalMemFree`/`InternalRand` and
# `InternalFactory::Get()`) takes a malloc + test-hook branch under
# -DBANISHED_UNIT_TEST, and `ResetPersistState`/`PersistObject` are compiled out
# there because they need a live IO::Stream and the Factory type tree.
# type_reflection.cpp (above) supplies `Object::Base` and `BaseTypeInfo`, and
# array.cpp (above) supplies `~Array` for the two handle-table arrays.
UNIT_TREE_SRCS += runtime/src/object/internal_class.cpp

# ENDGAME B.3.4 block 3 — the REAL `IO::Serializer` (tests/unit/io_vtable_abi_test.cpp).
# serializer_class.cpp is registry-free BY CONSTRUCTION, which is why it exists
# as its own translation unit: the 22 registered `RtSerializer_*` doors and
# `RtIoSerializerRegister()` stay in runtime/src/io/serializer.cpp, which stands
# on `RegisterRuntimeExport` and cannot link here. None of the 22 BODIES needs
# the registry — the constructor reaches the encoding factory through the
# stream's vtable, every Serialize member through the ReaderWriter's, and the
# destructor through slot 0 — so the class itself links, and the lifetime test
# placement-constructs the REAL `IO::Serializer` rather than a local mirror of
# it. A mirror tests itself. Same split as type_reflection.cpp (B.3.3 step 3).
# No other tree source is needed: the file's only include is the layout header.
UNIT_TREE_SRCS += runtime/src/io/serializer_class.cpp

# ENDGAME B.3.4b SYSTEM (4) — the REAL `System::CRC` (tests/unit/system_abi_test.cpp
# PART F). crc.cpp is the ONE file of the six this step converts that can link
# into a host test: its only includes are `rt_port.h` and the new
# `runtime/types/system/Crc.h`, its two registration calls are stubbed by
# tests/unit/rt_port_stub.cpp, and `System::CRC::Compute` is a pure function
# over a locally-generated table with no platform call and no registry lookup.
#
# So this is the one B.3.4b class whose test is BEHAVIOURAL rather than
# layout-only: PART F runs the standard CRC-32 check vector through the real
# ported member. `timer.cpp` (QueryPerformanceCounter + the five RT_DATA_SLOT
# statics), `error.cpp` (<windows.h>, MessageBoxW, three RT_DATA_SLOT statics),
# `thread_win.cpp` and `value_types.cpp` all stand on the platform or the
# registry and stay verified by the real run.
UNIT_TREE_SRCS += runtime/src/system/crc.cpp

# ENDGAME B.3.5 step 5 block H1 — the REAL Graphics animation classes
# (tests/unit/graphics_animation_abi_test.cpp). NO TREE SOURCE IS LINKED, and
# that is deliberate: every body of `Graphics::AnimationEvent`,
# `Graphics::AnimationGroup`, `Graphics::Animation`, `Graphics::AnimationState`
# and `Graphics::AnimationManager` lives in `runtime/src/graphics/animation.cpp`,
# `animationstate.cpp` and `vtbatch7_animgroup_particlesystem.cpp`, which stand
# on `RegisterRuntimeExport` / `RT_DATA_SLOT` / `ResolveAddr` / `rtcoll::MemAlloc`
# and cannot link here (the same reason `external_class_doors.cpp` is kept out).
# The two polymorphic classes' key-function bodies are therefore defined AT THE
# BOTTOM OF THE TEST FILE as markers, which is what lets GCC emit their vtables
# in that translation unit and lets the probes raw-call every slot. The test is
# an ABI test — slot order, sizes, offsets and the class/POD cross-asserts — so
# marker bodies are the right bodies. The serializers and the pose maths are
# verified by the real run.
UNIT_TREE_SRCS +=

# ENDGAME B.3.5 block A — the Video companion/VertexLayout ABI proof
# (tests/unit/video_data_abi_test.cpp). NO tree sources link: the twelve
# `Video::*Data` bases and `Video::VertexLayout` are declarations plus layout
# pins, and their bodies are registered doors in `runtime/src/video/` and
# `dx11/src/` that stand on the port registry, the address registry,
# <windows.h> and D3D11. The test checks the real classes' `sizeof`/`offsetof`
# directly (no instantiation needed) and the SLOT ABI on mirror classes
# declared in the test, exactly as tests/unit/io_vtable_abi_test.cpp does with
# `TestReaderWriter`. `Object::CompiledExternal`'s size is read through
# runtime/types/object/External.h, whose linkable half (external_class.cpp) is
# already on this list.
UNIT_TREE_SRCS +=

# ENDGAME B.3.5 block F — `Graphics::Camera`, `Graphics::Renderer`,
# `Graphics::Scene` and the four Renderer array element types
# (tests/unit/graphics_renderer_abi_test.cpp). NO TREE SOURCE IS LINKED, and
# unlike block E's Geometry test there are no marker bodies either: none of the
# seven classes is polymorphic, so there is no vtable for a translation unit to
# emit and nothing forces a key function to exist here. Every body lives in
# `runtime/src/graphics/{renderer,scene,cull,sun_camera,wave9_lane_gfxd,
# wave29_lane_gfxbody,closure_batch2b}.cpp`, which stand on
# `RegisterRuntimeExport` / `ResolveAddr` / `RT_EXPORT_ONCE` / `RT_DATA_SLOT` and
# cannot link here (the same reason `external_class_doors.cpp` is kept out), and
# writing marker bodies for members that already have real ones would be a second
# definition. So the test is LAYOUT plus two ABI properties — RULE B5-4's trivial
# copyability in both directions, and the `(this, ret)` retslot door shape on a
# NON-virtual by-value return, which is the other half of block E's ms_abi
# finding. `Math::Transform3` / `Math::Matrix` / `::Frustum` are the real classes
# (runtime/src/math/ is already on this list), so the copies it checks are real.
# The arithmetic in `Camera::IsMirrored`, `SunLight::GetSunCamera` and
# `Scene::Clean` is verified by the real run.
UNIT_TREE_SRCS +=
