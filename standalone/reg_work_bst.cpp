// standalone/reg_work_bst.cpp
// Wave 2 (Work) — simple-BST (AVL) family registration.
//
// Hand-ported + Ghidra-verified AVL insert/find/remove/serialize for the citizen-AI
// work-tracking tree (bodies in code/Game/Work/work_base_bst.cpp). Integrator wires
// RegisterWorkBstBatch() into RegisterCodeFunctions() in standalone/address_registry.cpp.
#ifdef STANDALONE_EXE
#include "resolve.h"
#include <cstdint>

extern void* WorkBST_RegisterWork_1a30(uintptr_t*, void*, void*);   // 0x140021a30 AVL insert
extern void  WorkBST_BSTRemove_a6e0(uintptr_t*, void*);             // 0x14001a6e0 AVL delete
extern void  WorkBST_BSTFindRemove_25520(uintptr_t*, void*);        // 0x140025520 find-by-key + remove
extern void  WorkBST_BST_Serialize_296b0(uintptr_t*, void*);        // 0x1400296b0 serialize

void RegisterWorkBstBatch() {
    RegisterExeAddr(0x140021a30, reinterpret_cast<uintptr_t>(&WorkBST_RegisterWork_1a30));
    RegisterExeAddr(0x14001a6e0, reinterpret_cast<uintptr_t>(&WorkBST_BSTRemove_a6e0));
    RegisterExeAddr(0x140025520, reinterpret_cast<uintptr_t>(&WorkBST_BSTFindRemove_25520));
    RegisterExeAddr(0x1400296b0, reinterpret_cast<uintptr_t>(&WorkBST_BST_Serialize_296b0));
}
#endif
