// src/diag/savesnap_probe.h -- TEMPORARY PROBE (SAVESNAP investigation, not for merge).
#pragma once
class GameUpdateLoop;
namespace IO { class Stream; }
namespace Save { class SavedState; }

void SaveSnapProbe_Mark(IO::Stream& stream, const char* name);
void SaveSnapProbe_AfterSave(GameUpdateLoop* gul, Save::SavedState* saved, double realMs);
void SaveSnapProbe_UpdateEnd(GameUpdateLoop* gul);
void SaveSnapProbe_Event(char kind);   // 'U' at an Update's end, 'D' at a Draw's start
void SaveSnapProbe_UpdateBegin(GameUpdateLoop* gul);
