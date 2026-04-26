#pragma once
#include "DialogLoaderLoopT.h"
// Shared body of Engine::DialogLoaderLoopT::Draw (Runtime 0x1800dae20).
// Called from engine/reimpl (DLL hook) and standalone/splash_loop_stubs (standalone vtable slot).
void DialogLoaderLoopT_Draw_Impl(DialogLoaderLoopT* self);
