// PROBE (TEMPORARY, HITCH_BGSAVE scenario 6): reverted after the measurement.
#pragma once
namespace BgSaveProbe {
inline void (*g_strRc)(const char* data) = nullptr;   // every StringRefCount(char*) (the ++ / -- sites)
inline void (*g_factoryOp)(int kind) = nullptr;       // 0 InternalFactory::AddObject, 1 RemoveObject,
                                                      // 2 ExternalFactory::FindOrLoad
}
