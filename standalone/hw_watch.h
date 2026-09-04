// standalone/hw_watch.h — hardware WRITE watchpoints on the main thread (diagnostic).
#pragma once
extern "C" {
void HwWatch_Init();                                   // call once on the main thread
int  HwWatch_Arm(void* addr, int len, const char* why); // len 1/2/4/8; rotates DR0..DR3; returns slot
int  HwWatch_Armed();
int  HwWatch_OnSingleStep(void* exceptionPointers, void (*logStack)(void* ctx));
}

// Main (sim) thread handle opened by HwWatch_Init (SUSPEND_RESUME|GET/SET_CONTEXT), or null.
void* HwWatch_MainThreadHandle();
