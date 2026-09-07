// ============================================================================
// runtime/src/io/serializer.cpp
// Clean, debuggable reimplementation of IO::Serializer (the read/write
// dispatch layer) — byte-accurate to Runtime-x64.dll.
// ============================================================================
//
// WHY: we are chasing a deserialize byte-misalignment in the menu-scene
// geometry load. To step the serializer with source and watch the stream
// read-position advance, we reimplement the IO::Serializer forwarders in clean
// C++ and (optionally) route the exe's deserialize calls through them.
//
// ----------------------------------------------------------------------------
// HOW THE ENGINE DOES IT (decode / loading path)
// ----------------------------------------------------------------------------
// Every IO::Serializer::Serialize* method in Runtime-x64.dll is a pure
// tail-jump forwarder. Disassembly of Serialize(int&) @ 0x1800061d0:
//     MOV RCX, [RCX+0x8]          ; RCX = serializer->m_encoding
//     MOV RAX, [RCX]              ; RAX = encoding->vtable
//     JMP  [RAX+0xA0]             ; tail-call encoding vtable slot (RDX/R8 pass through)
//
// The Encoding is a BinaryReader (load) or BinaryWriter (save), constructed by
// the Serializer ctor from the Stream's encoding factory. BinaryReader's read
// methods call Stream::Read (Stream vtable +0x18); for an IO::MemoryStream that
// is MemoryStream::Read @ 0x1800ae950:
//     if (m_cursor + count <= m_size) {
//         memcpy(dst, m_base + m_cursor, count);
//         m_cursor += count;           // <-- THE CURSOR (MemoryStream + 0x24)
//     }
//
// So these forwarders DO NOT themselves advance any cursor — they dispatch to
// the encoding, which dispatches to the stream, which advances m_cursor. Our
// clean reimpl reproduces the exact `encoding->vtable[slot](encoding, args)`
// indirect call, so the stream position advances identically.
//
// ----------------------------------------------------------------------------
// RUNTIME ADDRESSES (Runtime-x64.dll, image base 0x180000000)
// ----------------------------------------------------------------------------
//   Serializer::Serializer(Stream&,int)            0x1800b9960  (version IGNORED)
//   Serializer::~Serializer()                      0x1800b99b0
//   Serializer::Serialize(int&)        enc+0xA0    0x1800061d0
//   Serializer::Serialize(uint&)       enc+0x98    0x1800061f0
//   Serializer::Serialize(float&)      enc+0x80    0x180021660
//   Serializer::Serialize(bool&)       enc+0x78    0x1800061e0
//   Serializer::Serialize(uchar&)      enc+0xB8    0x180055c80
//   Serializer::Serialize(cnt,int*)    enc+0x48    0x180077660
//   Serializer::Serialize(cnt,ushort*) enc+0x50    0x180034fe0
//   Serializer::Serialize(cnt,short*)  enc+0x58    0x18007c400
//   Serializer::Serialize(cnt,uchar*)  enc+0x60    0x180054160
//   Serializer::SerializeBytes(sz,p)   enc+0xC8    0x1800346d0
//   Serializer::GetVersion()           enc+0x08    0x1800070b0
//   Serializer::GetEncoding()          [stream+0x10] 0x180006290
//
// ----------------------------------------------------------------------------
// A/B GATE
// ----------------------------------------------------------------------------
// Each wrapper checks BANISHED_REIMPL_SERIALIZER once (cached). UNSET/"0" =>
// tail-call the resolved Runtime original (default build byte-identical).
// SET ("1") => run the clean reimpl below.
// ============================================================================

#include "serializer.h"
#include "../../../engine/types/io_serializer.h"
#include "resolve.h"

#include <cstdlib>
#include <cstdio>
#include <cstdint>

using io::Serializer;
using io::Encoding;
namespace ev = io::enc_vtbl;

// ----------------------------------------------------------------------------
// Encoding vtable indirect-call helpers.
// Reproduce exactly: RCX = encoding, JMP [encoding->vtable + slot].
// ----------------------------------------------------------------------------
namespace {

inline void* EncVtblSlot(const Encoding* enc, int byteOffset) {
    // enc->vtable is void**; slot is at (vtable byte base + byteOffset).
    uint8_t* vt = reinterpret_cast<uint8_t*>(enc->vtable);
    return *reinterpret_cast<void**>(vt + byteOffset);
}

// One-shot warning when an unresolved Runtime fallback is hit.
void WarnOnce(const char* what) {
    static const char* warned[24] = {0};
    for (int i = 0; i < 24; ++i) {
        if (warned[i] == what) return;
        if (warned[i] == nullptr) { warned[i] = what; break; }
    }
    fprintf(stderr, "[RtSerializer] WARNING: Runtime fallback '%s' unresolved (NULL)\n", what);
    fflush(stderr);
}

// Resolve a Runtime IO::Serializer export once (cached at call site).
inline void* ResolveOnce(void*& cache, const char* mangled) {
    if (!cache) cache = ResolveExport(mangled);
    return cache;
}

} // namespace

// ----------------------------------------------------------------------------
// A/B flag — cached read of BANISHED_REIMPL_SERIALIZER.
// ----------------------------------------------------------------------------
extern "C" bool RtSerializer_ReimplEnabled() {
    static int cached = -1;
    if (cached < 0) {
        const char* e = std::getenv("BANISHED_REIMPL_SERIALIZER");
        cached = (e && e[0] && e[0] != '0' &&
                  !(e[0] == 'f' && e[1] == 'a')) ? 1 : 0; // unset / "0" / "false" => off
        fprintf(stderr, "[RtSerializer] reimpl path %s\n", cached ? "ENABLED" : "disabled (Runtime fallback)");
        fflush(stderr);
    }
    return cached != 0;
}

// ============================================================================
// Forwarders
// ============================================================================
//
// Pattern for each:
//   if (!RtSerializer_ReimplEnabled()) { <tail-call Runtime original>; return; }
//   <clean reimpl: encoding->vtable[slot](encoding, args...)>
//
// The clean body matches the Runtime forwarder instruction-for-instruction:
// load encoding = self->m_encoding, then indirect-call its vtable slot with the
// same (encoding, arg, name) arguments.

extern "C" {

// ---- Serialize(int&) — encoding vtable +0xA0 -------------------------------
void RtSerializer_Serialize_Int(Serializer* self, int* v, const char* name) {
    if (!RtSerializer_ReimplEnabled()) {
        static void* orig = nullptr;
        ResolveOnce(orig, "?Serialize@Serializer@IO@@QEAAXAEAHPEBD@Z");
        if (!orig) { WarnOnce("Serialize(int&)"); return; }
        using fn_t = void(*)(Serializer*, int*, const char*);
        reinterpret_cast<fn_t>(orig)(self, v, name);
        return;
    }
    Encoding* enc = self->m_encoding;
    using fn_t = void(*)(Encoding*, int*, const char*);
    reinterpret_cast<fn_t>(EncVtblSlot(enc, ev::kSerializeInt))(enc, v, name);
}

// ---- Serialize(uint&) — encoding vtable +0x98 ------------------------------
void RtSerializer_Serialize_UInt(Serializer* self, uint32_t* v, const char* name) {
    if (!RtSerializer_ReimplEnabled()) {
        static void* orig = nullptr;
        ResolveOnce(orig, "?Serialize@Serializer@IO@@QEAAXAEAIPEBD@Z");
        if (!orig) { WarnOnce("Serialize(uint&)"); return; }
        using fn_t = void(*)(Serializer*, uint32_t*, const char*);
        reinterpret_cast<fn_t>(orig)(self, v, name);
        return;
    }
    Encoding* enc = self->m_encoding;
    using fn_t = void(*)(Encoding*, uint32_t*, const char*);
    reinterpret_cast<fn_t>(EncVtblSlot(enc, ev::kSerializeUInt))(enc, v, name);
}

// ---- Serialize(float&) — encoding vtable +0x80 -----------------------------
void RtSerializer_Serialize_Float(Serializer* self, float* v, const char* name) {
    if (!RtSerializer_ReimplEnabled()) {
        static void* orig = nullptr;
        ResolveOnce(orig, "?Serialize@Serializer@IO@@QEAAXAEAMPEBD@Z");
        if (!orig) { WarnOnce("Serialize(float&)"); return; }
        using fn_t = void(*)(Serializer*, float*, const char*);
        reinterpret_cast<fn_t>(orig)(self, v, name);
        return;
    }
    Encoding* enc = self->m_encoding;
    using fn_t = void(*)(Encoding*, float*, const char*);
    reinterpret_cast<fn_t>(EncVtblSlot(enc, ev::kSerializeFloat))(enc, v, name);
}

// ---- Serialize(bool&) — encoding vtable +0x78 ------------------------------
void RtSerializer_Serialize_Bool(Serializer* self, bool* v, const char* name) {
    if (!RtSerializer_ReimplEnabled()) {
        static void* orig = nullptr;
        ResolveOnce(orig, "?Serialize@Serializer@IO@@QEAAXAEA_NPEBD@Z");
        if (!orig) { WarnOnce("Serialize(bool&)"); return; }
        using fn_t = void(*)(Serializer*, bool*, const char*);
        reinterpret_cast<fn_t>(orig)(self, v, name);
        return;
    }
    Encoding* enc = self->m_encoding;
    using fn_t = void(*)(Encoding*, bool*, const char*);
    reinterpret_cast<fn_t>(EncVtblSlot(enc, ev::kSerializeBool))(enc, v, name);
}

// ---- Serialize(uchar&) — encoding vtable +0xB8 -----------------------------
void RtSerializer_Serialize_UChar(Serializer* self, uint8_t* v, const char* name) {
    if (!RtSerializer_ReimplEnabled()) {
        static void* orig = nullptr;
        ResolveOnce(orig, "?Serialize@Serializer@IO@@QEAAXAEAEPEBD@Z");
        if (!orig) { WarnOnce("Serialize(uchar&)"); return; }
        using fn_t = void(*)(Serializer*, uint8_t*, const char*);
        reinterpret_cast<fn_t>(orig)(self, v, name);
        return;
    }
    Encoding* enc = self->m_encoding;
    using fn_t = void(*)(Encoding*, uint8_t*, const char*);
    reinterpret_cast<fn_t>(EncVtblSlot(enc, ev::kSerializeUChar))(enc, v, name);
}

// ---- Serialize(count, int*) — encoding vtable +0x48 ------------------------
void RtSerializer_Serialize_IntArr(Serializer* self, int count, int32_t* data, const char* name) {
    if (!RtSerializer_ReimplEnabled()) {
        static void* orig = nullptr;
        ResolveOnce(orig, "?Serialize@Serializer@IO@@QEAAXHPEAHPEBD@Z");
        if (!orig) { WarnOnce("Serialize(cnt,int*)"); return; }
        using fn_t = void(*)(Serializer*, int, int32_t*, const char*);
        reinterpret_cast<fn_t>(orig)(self, count, data, name);
        return;
    }
    Encoding* enc = self->m_encoding;
    using fn_t = void(*)(Encoding*, int, int32_t*, const char*);
    reinterpret_cast<fn_t>(EncVtblSlot(enc, ev::kSerializeIntArr))(enc, count, data, name);
}

// ---- Serialize(count, ushort*) — encoding vtable +0x50 ---------------------
void RtSerializer_Serialize_UShortArr(Serializer* self, int count, uint16_t* data, const char* name) {
    if (!RtSerializer_ReimplEnabled()) {
        static void* orig = nullptr;
        ResolveOnce(orig, "?Serialize@Serializer@IO@@QEAAXHPEAGPEBD@Z");
        if (!orig) { WarnOnce("Serialize(cnt,ushort*)"); return; }
        using fn_t = void(*)(Serializer*, int, uint16_t*, const char*);
        reinterpret_cast<fn_t>(orig)(self, count, data, name);
        return;
    }
    Encoding* enc = self->m_encoding;
    using fn_t = void(*)(Encoding*, int, uint16_t*, const char*);
    reinterpret_cast<fn_t>(EncVtblSlot(enc, ev::kSerializeU16Arr))(enc, count, data, name);
}

// ---- Serialize(count, short*) — encoding vtable +0x58 ----------------------
void RtSerializer_Serialize_ShortArr(Serializer* self, int count, int16_t* data, const char* name) {
    if (!RtSerializer_ReimplEnabled()) {
        static void* orig = nullptr;
        ResolveOnce(orig, "?Serialize@Serializer@IO@@QEAAXHPEAFPEBD@Z");
        if (!orig) { WarnOnce("Serialize(cnt,short*)"); return; }
        using fn_t = void(*)(Serializer*, int, int16_t*, const char*);
        reinterpret_cast<fn_t>(orig)(self, count, data, name);
        return;
    }
    Encoding* enc = self->m_encoding;
    using fn_t = void(*)(Encoding*, int, int16_t*, const char*);
    reinterpret_cast<fn_t>(EncVtblSlot(enc, ev::kSerializeI16Arr))(enc, count, data, name);
}

// ---- Serialize(count, uchar*) — encoding vtable +0x60 ----------------------
void RtSerializer_Serialize_UCharArr(Serializer* self, int count, uint8_t* data, const char* name) {
    if (!RtSerializer_ReimplEnabled()) {
        static void* orig = nullptr;
        ResolveOnce(orig, "?Serialize@Serializer@IO@@QEAAXHPEAEPEBD@Z");
        if (!orig) { WarnOnce("Serialize(cnt,uchar*)"); return; }
        using fn_t = void(*)(Serializer*, int, uint8_t*, const char*);
        reinterpret_cast<fn_t>(orig)(self, count, data, name);
        return;
    }
    Encoding* enc = self->m_encoding;
    using fn_t = void(*)(Encoding*, int, uint8_t*, const char*);
    reinterpret_cast<fn_t>(EncVtblSlot(enc, ev::kSerializeU8Arr))(enc, count, data, name);
}

// ---- SerializeBytes(size, void*) — encoding vtable +0xC8 -------------------
void RtSerializer_SerializeBytes(Serializer* self, int size, void* ptr, const char* name) {
    if (!RtSerializer_ReimplEnabled()) {
        static void* orig = nullptr;
        ResolveOnce(orig, "?SerializeBytes@Serializer@IO@@QEAAXHPEAXPEBD@Z");
        if (!orig) { WarnOnce("SerializeBytes"); return; }
        using fn_t = void(*)(Serializer*, int, void*, const char*);
        reinterpret_cast<fn_t>(orig)(self, size, ptr, name);
        return;
    }
    Encoding* enc = self->m_encoding;
    using fn_t = void(*)(Encoding*, int, void*, const char*);
    reinterpret_cast<fn_t>(EncVtblSlot(enc, ev::kSerializeBytes))(enc, size, ptr, name);
}

// ---- GetVersion() — encoding vtable +0x08 ----------------------------------
int RtSerializer_GetVersion(Serializer* self) {
    if (!RtSerializer_ReimplEnabled()) {
        static void* orig = nullptr;
        ResolveOnce(orig, "?GetVersion@Serializer@IO@@QEBAHXZ");
        if (!orig) { WarnOnce("GetVersion"); return 0; }
        using fn_t = int(*)(Serializer*);
        return reinterpret_cast<fn_t>(orig)(self);
    }
    Encoding* enc = self->m_encoding;
    using fn_t = int(*)(Encoding*);
    return reinterpret_cast<fn_t>(EncVtblSlot(enc, ev::kGetVersion))(enc);
}

// ---- GetEncoding() — returns m_stream->m_encoding (the factory @ stream+0x10)
// Original (0x180006290): return *(Encoding**)(*(Stream**)this + 0x10);
//   i.e. the factory object stored on the stream, NOT serializer->m_encoding.
const Encoding* RtSerializer_GetEncoding(Serializer* self) {
    if (!RtSerializer_ReimplEnabled()) {
        static void* orig = nullptr;
        ResolveOnce(orig, "?GetEncoding@Serializer@IO@@QEAAAEBVEncoding@2@XZ");
        if (!orig) { WarnOnce("GetEncoding"); return nullptr; }
        using fn_t = const Encoding*(*)(Serializer*);
        return reinterpret_cast<fn_t>(orig)(self);
    }
    return self->m_stream->m_encoding;
}

} // extern "C"

// ============================================================================
// Wave 4 lane A registration hook. Scaffolding only until the lane lands; see
// rt_io_port.cpp for the wave contract and the switches.
// ============================================================================
int RtIoSerializerRegister() { return 0; }
