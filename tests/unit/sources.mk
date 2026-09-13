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
