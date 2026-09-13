# Sources from the tree that the unit tests link against. One family per line; keep it
# to the classes under test and what they need (allocator, crc). Each core batch appends.
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
