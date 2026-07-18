// standalone/reg_work_cellbst.cpp
// Wave 2 (Work) — cell-BST (AVL) family registration.
//
// Hand-ported + Ghidra-verified AVL insert/remove/serialize for the ClearArea / cell-keyed
// work tree (bodies in code/Game/Work/work_base_cellbst.cpp). Integrator wires
// RegisterWorkCellBstBatch() into RegisterCodeFunctions() in standalone/address_registry.cpp.
#include "resolve.h"
#include <cstdint>

extern void* WorkCellBST_Insert_9a40(uintptr_t*, void*, void*);   // 0x140029a40 AVL insert
extern void  WorkCellBST_Remove_ca60(uintptr_t*, void*);          // 0x14001ca60 AVL delete
extern void  WorkCellBST_Serialize_9de0(uintptr_t*, void*);       // 0x140029de0 serialize

void RegisterWorkCellBstBatch() {
    RegisterExeAddr(0x140029a40, reinterpret_cast<uintptr_t>(&WorkCellBST_Insert_9a40));
    RegisterExeAddr(0x14001ca60, reinterpret_cast<uintptr_t>(&WorkCellBST_Remove_ca60));
    RegisterExeAddr(0x140029de0, reinterpret_cast<uintptr_t>(&WorkCellBST_Serialize_9de0));
}
