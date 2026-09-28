#pragma once
// code/Game/Entity/exe_bodies.h -- the exe bodies DEFINED in code/Game/Entity/ that the registry
// reaches by retail address (B3R_RULES R5, block W0-P0c).
//
// One declaration per body, copied from its DEFINITION (linkage included),
// grouped by defining file; the retail address(es) the body is registered
// under are the trailing comment. The defining .cpp includes this header so
// the compiler checks each pair. Readers call these by name (W1); nothing
// here changes what runs. Making them members is B.4's work.
//
// Declared in another header already (not redeclared here):
//   CitizenBehavior_Update                   0x14008c2b0  (code/Game/Entity/citizenbehavior_funcs.h)
//   ComponentEntity_CtorFromDescription      0x1400c3720  (code/Game/Entity/componententity_funcs.h)
//   EntityList_RegisterEntity_Impl           0x1400c48a0  (code/Game/Entity/componententity_funcs.h)
//   EntityList_PostUpdateNodeHandler         0x1400c2c70  (code/Game/Entity/entitylist_funcs.h)
//   EntityList_Update                        0x1400c4090  (code/Game/Entity/entitylist_funcs.h)
//   EntityList_PreUpdate                     0x1400c3bd0  (code/Game/Entity/entitylist_funcs.h)
//   EntityList_PostUpdateStaging             0x1400c3ab0  (code/Game/Entity/entitylist_funcs.h)
//   EntityList_PostUpdate                    0x1400c43d0  (code/Game/Entity/entitylist_funcs.h)
//   EntityList_RemoveEntity                  0x1400c38e0  (code/Game/Entity/entitylist_funcs.h)
//   EntityList_Serialize                     0x1400c44a0  (code/Game/Entity/entitylist_funcs.h)

#include <cstdint>
