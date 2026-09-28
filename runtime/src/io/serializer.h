#pragma once
// ============================================================================
// runtime/src/io/serializer.h — the ported IO::Serializer surface (Wave 4, A)
// ============================================================================
//
// Byte-faithful reimplementations of the 22 exported IO::Serializer members our
// tree reaches. See serializer.cpp for the retail addresses, the pinned
// ReaderWriter vtable offsets and the oracle story; see
// runtime/types/io/Serializer.h for the layouts.
//
// ----------------------------------------------------------------------------
// TWO ENTRY PATHS, ONE IMPLEMENTATION
// ----------------------------------------------------------------------------
// These functions are reached two ways, and that is deliberate — they are the
// SAME function pointer both times, not two ports of one class:
//
//   1. RegisterRuntimeExport(<mangled>, &RtSerializer_X) in
//      RtIoSerializerRegister(). Flips every ResolveExport call site
//      (engine/funcs/engine_funcs.cpp's fn_* table, and any direct caller).
//
//   2. RegisterDispatchSlot(0x1401524xx, &RtSerializer_X) in
//      standalone/address_registry.cpp. The exe's retail-shaped code reads
//      `*(fn**)slot` directly and never consults ResolveExport, so those 14
//      slots must be bound to something; they are bound to these functions.
//      They are NOT a second redirection mechanism — removing them would leave
//      the slots null and EXEC-of-0 at the first save.
//
// ----------------------------------------------------------------------------
// BANISHED_REIMPL_SERIALIZER IS RETIRED
// ----------------------------------------------------------------------------
// The 2025-era version of this file gated every wrapper on
// BANISHED_REIMPL_SERIALIZER, defaulting OFF, so the shipped build tail-called
// the DLL. That gate is gone and replaced by the wave switches
// the old DLL-era switch / the old DLL-era switch (runtime/src/io/rt_io_port.cpp),
// which default ON. RtSerializer_ReimplEnabled() no longer exists.
//
// The kill switch has to be tested INSIDE each body rather than expressed by
// "do not register", because path 2 above binds the dispatch slots
// unconditionally from AddressRegistryInit(). With the old DLL-era switch
// nothing is registered, the internal flag stays false, and every one of these
// functions tail-calls the shipped DLL — i.e. both paths are killed together.
// ============================================================================

#include <cstdint>

#include "../../types/io/Serializer.h"

extern "C" {

// ---- lifetime --------------------------------------------------------------
// The ctor returns `this` in RAX, as every MSVC ctor does. It also CONSUMES OR
// EMITS 12 BYTES of stream (see runtime/types/io/Serializer.h) — it is not a
// cheap wrapper construction.
rt::io::Serializer* RtSerializer_Ctor(rt::io::Serializer* self, rt::io::Stream* stream, int version);

// ---- accessors -------------------------------------------------------------
int                     RtSerializer_GetVersion (rt::io::Serializer* self);

// ---- scalar forwarders (this, T&, name) ------------------------------------
void RtSerializer_Serialize_Bool  (rt::io::Serializer* self, bool*     v, const char* name);
void RtSerializer_Serialize_SChar (rt::io::Serializer* self, int8_t*   v, const char* name);
void RtSerializer_Serialize_UChar (rt::io::Serializer* self, uint8_t*  v, const char* name);
void RtSerializer_Serialize_Short (rt::io::Serializer* self, int16_t*  v, const char* name);
void RtSerializer_Serialize_UShort(rt::io::Serializer* self, uint16_t* v, const char* name);
void RtSerializer_Serialize_Int   (rt::io::Serializer* self, int32_t*  v, const char* name);
void RtSerializer_Serialize_UInt  (rt::io::Serializer* self, uint32_t* v, const char* name);
void RtSerializer_Serialize_Float (rt::io::Serializer* self, float*    v, const char* name);

// ---- array forwarders (this, count, T*, name) ------------------------------
// The char / signed char forms are here because two exe dispatch slots
// (0x140152490 and 0x140152478) are real PE imports of them, even though no
// ResolveExport site in the tree names them. See serializer.cpp.
void RtSerializer_Serialize_CharArr  (rt::io::Serializer* self, int n, char*     d, const char* name);
void RtSerializer_Serialize_SCharArr (rt::io::Serializer* self, int n, int8_t*   d, const char* name);
void RtSerializer_Serialize_BoolArr  (rt::io::Serializer* self, int n, bool*     d, const char* name);
void RtSerializer_Serialize_UCharArr (rt::io::Serializer* self, int n, uint8_t*  d, const char* name);
void RtSerializer_Serialize_ShortArr (rt::io::Serializer* self, int n, int16_t*  d, const char* name);
void RtSerializer_Serialize_UShortArr(rt::io::Serializer* self, int n, uint16_t* d, const char* name);
void RtSerializer_Serialize_IntArr   (rt::io::Serializer* self, int n, int32_t*  d, const char* name);
void RtSerializer_Serialize_UIntArr  (rt::io::Serializer* self, int n, uint32_t* d, const char* name);
void RtSerializer_Serialize_FloatArr (rt::io::Serializer* self, int n, float*    d, const char* name);

} // extern "C"

// Wave 4 lane A registration hook, called from rt_io_port.cpp under
// the old DLL-era switch. Returns the number of exports registered.
int RtIoSerializerRegister();
