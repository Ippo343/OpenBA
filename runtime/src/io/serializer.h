#pragma once
// ============================================================================
// runtime/src/io/serializer.h — clean reimplementation of IO::Serializer
// ============================================================================
//
// Debuggable C++ reimplementations of the IO::Serializer forwarder methods
// from Runtime-x64.dll, byte-accurate to the originals. See serializer.cpp
// for the decompile references and the full read-path documentation.
//
// A/B SAFETY: every wrapper below checks the env var BANISHED_REIMPL_SERIALIZER
// (cached once). When UNSET (default), it tail-calls the resolved Runtime
// original so the default build stays byte-identical. When SET to "1", it runs
// the clean reimpl (which you can step through with source).
//
// These are registered into the exe dispatch slots in
// standalone/address_registry.cpp (gated on STANDALONE_EXE).
// ============================================================================

#include <cstdint>

namespace io { struct Serializer; struct Encoding; }

extern "C" {

// Returns true once if BANISHED_REIMPL_SERIALIZER is set to a non-"0"/"false"
// value. Cached; cheap to call repeatedly. Exposed for tests/diagnostics.
bool RtSerializer_ReimplEnabled();

// ---- Forwarders matching the Runtime IO::Serializer signatures -------------
// (this = IO::Serializer*; name = field-name C string, unused by binary codec)
void  RtSerializer_Serialize_Int   (io::Serializer* self, int*      v,   const char* name);
void  RtSerializer_Serialize_UInt  (io::Serializer* self, uint32_t* v,   const char* name);
void  RtSerializer_Serialize_Float (io::Serializer* self, float*    v,   const char* name);
void  RtSerializer_Serialize_Bool  (io::Serializer* self, bool*     v,   const char* name);
void  RtSerializer_Serialize_UChar (io::Serializer* self, uint8_t*  v,   const char* name);
void  RtSerializer_Serialize_IntArr   (io::Serializer* self, int count, int32_t*  data, const char* name);
void  RtSerializer_Serialize_UShortArr(io::Serializer* self, int count, uint16_t* data, const char* name);
void  RtSerializer_Serialize_ShortArr (io::Serializer* self, int count, int16_t*  data, const char* name);
void  RtSerializer_Serialize_UCharArr (io::Serializer* self, int count, uint8_t*  data, const char* name);
void  RtSerializer_SerializeBytes  (io::Serializer* self, int size, void* ptr, const char* name);
int   RtSerializer_GetVersion      (io::Serializer* self);
const io::Encoding* RtSerializer_GetEncoding(io::Serializer* self);

} // extern "C"
