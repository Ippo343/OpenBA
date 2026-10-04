#pragma once
// ============================================================================
// runtime/types/io/PersistenceWorker.h -- ENDGAME B.3.9 IO (W2)
// ============================================================================
//
// `IO::PersistenceWorker`, the save/load worker `IO::Persistence` embeds at
// +0x10, is declared in Persistence.h since B.4 wave X (X7-RT): Persistence
// holds it as a typed member, so the class has to be complete there. This
// header keeps the worker's three enqueue entry points.
// ============================================================================

#include "Persistence.h"

// ---- io/persistence_worker_win.cpp
extern "C" void RtIo_PersistenceWorker_EnqueueDelete(IO::PersistenceWorker* worker, const System::String* filename);  // 0x1800b7390
extern "C" void RtIo_PersistenceWorker_EnqueueLoad(IO::PersistenceWorker* worker, const System::String* filename, int blockCount, const RtIo::SaveBlock* blocks);  // 0x1800b7460
extern "C" void RtIo_PersistenceWorker_EnqueueSave(IO::PersistenceWorker* worker, const System::String* filename, int blockCount, const RtIo::SaveBlock* blocks);  // 0x1800b75c0
