// ============================================================================
// runtime/src/system/crc.cpp — System::CRC::Compute
// ============================================================================
//
// ?Compute@CRC@System@@SAIPEBXHI@Z  (retail 0x1800c5490, ordinal 1752;
// profile build: same name). Static, three args: RCX = data, EDX = byte count,
// R8D = running crc. Read out of the retail body:
//
//     1800c5490  TEST EDX,EDX / JLE done          ; 0 < count
//     loop:      MOVZX EAX,byte [RCX] / INC RCX
//                MOVZX ECX,R8B / XOR ECX,EAX       ; (crc & 0xff) ^ byte
//                MOV EAX,[_crcTable + RCX*4]
//                SHR R8D,8 / XOR R8D,EAX           ; table[...] ^ (crc >> 8)
//                DEC RDX / JNZ loop
//     done:      MOV EAX,R8D / NOT EAX / RET       ; ~crc
//
// Note what it does NOT do: it never complements the seed on entry. A caller
// that wants a standard CRC-32 passes 0xFFFFFFFF itself; a caller chaining
// chunks passes ~previousResult (which is what the persistence jobs do — see
// io/persistence_worker_win.cpp's Crc32Chunked).
//
// The table is System::CRC::_crcTable (exported DATA, retail 0x1801f9080),
// verified byte-for-byte against the textbook reflected CRC-32 table in
// object/type_system.cpp, so it is generated here rather than read from the
// DLL, for the same reason that file gives: the body has to survive cutover.
// Generated locally rather than shared, on the census rule the persistence
// files record (each file's own dependencies are what the census greps).
//
// Opened as an address door by io/persistence_saveinfo_job.cpp (batch 8b) and
// closed here.
// ============================================================================
#include <cstdint>

#include "../../funcs/rt_port.h"

namespace {

const uint32_t* CrcTable() {
    static uint32_t table[256];
    static bool built = false;
    if (!built) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1u) ? ((c >> 1) ^ 0xEDB88320u) : (c >> 1);
            table[i] = c;
        }
        built = true;
    }
    return table;
}

}  // namespace

extern "C" uint32_t RtSystem_CRC_Compute(const void* data, int count, uint32_t crc) {
    if (count > 0) {
        const uint32_t* const table = CrcTable();
        const uint8_t* p = static_cast<const uint8_t*>(data);
        uint64_t n = static_cast<uint32_t>(count);
        do {
            const uint8_t b = *p++;
            crc = table[(crc & 0xffu) ^ b] ^ (crc >> 8);
        } while (--n != 0);
    }
    return ~crc;
}

int RtSystemCrcRegister() {
    RegisterRuntimeExport("?Compute@CRC@System@@SAIPEBXHI@Z", (void*)&RtSystem_CRC_Compute);
    RegisterRuntimeAddr(0x1800c5490ull, (void*)&RtSystem_CRC_Compute);
    return 1;
}
