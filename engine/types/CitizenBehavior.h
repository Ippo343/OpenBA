#pragma once
// engine/types/CitizenBehavior.h — FORWARDER (ENDGAME B.4 W0-H1)
//
// `CitizenBehavior` is declared as a real class in code/Game/Entity/CitizenBehavior.h
// (every field, retail's types, asserted; with ProfessionResourceEntry,
// ResourceNotifyEntry, PopulationSummary and PathGraph). This header forwards to
// it and still carries the `CitizenBehaviorRaw` accessor mirror that the ported
// bodies in code/Game/Entity/citizenbehavior.cpp use; the mirror goes with its
// last user (B4_RULES H10).

#include <cstdint>
#include <cstddef>
#include "Entity/CitizenBehavior.h"

// The full struct as accessed by the game code via longlong* indexing.
// Since fields are accessed as param_1[0x32] etc. (longlong* offsets),
// we define using a raw byte array with accessor methods.
struct CitizenBehaviorRaw {
    uint8_t data[0x1C0];

    // Accessors matching Ghidra's longlong* param_1 indexing
    int64_t& qword(int idx)       { return *reinterpret_cast<int64_t*>(data + idx * 8); }
    int32_t& dword_at(int offset) { return *reinterpret_cast<int32_t*>(data + offset); }
    void*&   ptr_at(int offset)   { return *reinterpret_cast<void**>(data + offset); }

    // Named accessors for key fields (from constructor and Update analysis)
    void*&   citizenArray()       { return ptr_at(0x00); }
    int32_t& citizenCount()       { return dword_at(0x08); }

    void*&   buildingArray()      { return ptr_at(0x10); }
    int32_t& buildingCount()      { return dword_at(0x18); }

    void*&   schoolArray()        { return ptr_at(0x20); }
    int32_t& schoolCount()        { return dword_at(0x28); }

    void*&   allCitizensPtr()     { return ptr_at(0xA0); }
    int32_t& allCitizensCount()   { return dword_at(0xA8); }

    void*&   professionBST()      { return ptr_at(0x100); }

    void*&   statsHistory()       { return ptr_at(0x150); }

    void*&   profResources()      { return ptr_at(0x170); } // ProfessionResourceEntry[21]

    // Resource notification entries at +0x180
    ResourceNotifyEntry* notifyEntries() {
        return reinterpret_cast<ResourceNotifyEntry*>(data + 0x180);
    }

    // Timers (from Update: param_1[0x32..0x35])
    int64_t& pathAccumTimer()     { return qword(0x32); } // +0x190
    int64_t& statsTimer()         { return qword(0x33); } // +0x198
    int64_t& notifyTimer()        { return qword(0x34); } // +0x1A0
    int32_t& notifyFlags()        { return dword_at(0x1A8); } // param_1[0x35] low dword

    // Path graph sub-object
    PathGraph*& pathGraph()       { return *reinterpret_cast<PathGraph**>(data + 0x1B8); }
                                   // param_1[0x37]
};
static_assert(sizeof(CitizenBehaviorRaw) == 0x1C0, "CitizenBehaviorRaw must be 0x1C0 bytes");
