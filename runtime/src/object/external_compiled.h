#pragma once
// ============================================================================
// runtime/src/object/external_compiled.h — Wave 11 lane OBJEXT
//   `Object::ExternalFactory::GetCompiledPath` / `FindOrLoad`.
// ============================================================================

#include <cstdint>

#include "../../types/object/External.h"
#include "../../types/system/Filename.h"

namespace RtObject {

using RtSystem::Filename;

// ?GetCompiledPath@ExternalFactory@Object@@SA?AVFilename@System@@AEBV34@@Z
//                                                          retail 0x1800bec10
// A STATIC returning by value: RCX = the hidden return slot, RDX = the path.
Filename* RtObject_ExternalFactory_GetCompiledPath(Filename* ret, const Filename* path);

// ?FindOrLoad@ExternalFactory@Object@@QEAAPEBVExternal@2@AEBVFilename@System@@@Z
//                                                          retail 0x1800bf320
const External* RtObject_ExternalFactory_FindOrLoad(ExternalFactory* self,
                                                    const Filename* path);

// Effectiveness control: every counter is a branch taken INSIDE a ported body.
struct CompiledCounters {
    uint64_t getCompiledCalls;
    uint64_t findOrLoadCalls;
    uint64_t buildCalls;       // the shared six-term concatenation
    uint64_t emptyPath;        // FindOrLoad's null/zero-length early-out
    uint64_t cacheHit;
    uint64_t hitReAdded;       // ... and _loadEnabled was set, so Add ran
    uint64_t hitNotReAdded;
    uint64_t cacheMiss;
    uint64_t loaded;           // External::Load returned an object
    uint64_t loadFailed;
};
extern CompiledCounters g_cc;

}  // namespace RtObject

// runtime/src/object/rt_object_port.cpp calls this.
int RtObjectExternalCompiledRegister();
