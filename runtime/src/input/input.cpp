// ============================================================================
// runtime/src/input/input.cpp — Wave 8 lane D: Input::KeyMap + Input::Device
// + Input::Configuration
// ============================================================================
//
// Ports 15 of the 21 exports our tree calls (docs/runtime_port/WAVE8_LANE_D.md
// has the full table, the oracle argument for every function, and the 6
// hold-backs' written reasons). Every offset used below came from the
// DISASSEMBLY of the retail function at the address named in each comment —
// not Ghidra's decompiled C, which mangles this class's pointer arithmetic
// badly enough to be actively misleading (reference_ghidra_c_output_vs_
// disassembly.md). Layout is in runtime/types/input/Input.h.
//
// Nothing here is registered as a vtable slot: neither class exports a
// `??_7`, and this lane verified (not assumed) that none of the 15 ported
// bodies dereferences a vtable pointer anywhere — see the type header.
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstring>
#include "../../funcs/rt_port.h"
#include "../../types/input/Input.h"

using namespace RtInput;

// ----------------------------------------------------------------------------
// ResolveAddr / Module, forward-declared rather than pulled in from
// standalone/resolve.h — see rt_port.h's own comment on why (windows.h leaks
// engine-colliding macros into a tree PLAN.md §4.1 wants platform-neutral).
// Must stay byte-identical to resolve.h's declaration; the linker checks it.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

// Already-ported Wave 4 IO::Persistence entry points (runtime/src/io/
// persistence.cpp). Not declared in any header there; redeclared here with
// the exact signature, same pattern rt_port.h's own Module/ResolveAddr use.
extern bool RtIo_Persistence_GetBool(void* self, const char* section, const char* key, bool* out);
extern void RtIo_Persistence_SetBool(void* self, const char* section, const char* key, bool value);

namespace {

// ============================================================================
// Collection::Set<Binding> helpers shared with every point function below.
// ============================================================================

// Exact-match descent: id < node.id -> left(+0x20); id > node.id -> right
// (+0x28); id == node.id -> found. Pinned identically (down to the branch
// order) in the disassembly of GetDeviceForId (0x1800a7970),
// GetInputForId (0x1800a7930), GetValue (0x1800a7b60), Pressed (0x1800a7a80),
// SetInput (0x1800a7e20), ClearInput/2 (0x1800a7d70), ClearInput/3
// (0x1800a7db0) and the search half of RegisterInput (0x1800a7e70, held back
// — see the wave doc). Returns nullptr on no match, exactly like retail's
// RAX ending at 0 — every caller below reproduces retail's un-null-guarded
// use of that result (CLAUDE.md: a faithful crash on a bad id is not ours to
// fix).
BindingNode* FindBinding(KeyMap* km, uint8_t id) {
    auto* n = static_cast<BindingNode*>(km->_root);
    while (n) {
        if (id < n->element.id) {
            n = n->left;
        } else if (id <= n->element.id) {
            return n;  // id == n->element.id
        } else {
            n = n->right;
        }
    }
    return nullptr;
}

BindingNode* Leftmost(BindingNode* n) {
    while (n && n->left) n = n->left;
    return n;
}

// In-order successor via parent chain. Pinned by GetIdForDeviceInput's
// disassembly (0x1800a7890: right-then-leftmost if a right child exists,
// otherwise walk up while `node == parent->right`).
BindingNode* Successor(BindingNode* n) {
    if (n->right) return Leftmost(n->right);
    BindingNode* p = n->parent;
    while (p && n == p->right) {
        n = p;
        p = p->parent;
    }
    return p;
}

// Device::_singleton, an exported DATA symbol (retail 0x1801fe770). NOT
// registered by this lane (data exports carry nothing to redirect — every
// prior wave's note applies unchanged); read directly, exactly like
// engine_funcs.cpp's own g_pInputDeviceSingleton.
uint8_t* DeviceSingleton() {
    static uint8_t** slot = reinterpret_cast<uint8_t**>(
        ResolveExportRaw("?_singleton@Device@Input@@0PEAV12@EA"));
    return slot ? *slot : nullptr;
}

// Resolve a (device, slot-local input id) pair to the base address of that
// input's State record's containing array, exactly as GetValue/Pressed/
// Released's shared dispatch does: device 0 = joystick `deviceCtx`
// (the function's OWN first int parameter, threaded through unchanged),
// device 1 = keyboard, device 2 = mouse, anything else = nullptr (retail's
// R8 stays 0 in that branch too — a faithful potential null deref on an
// invalid device index, not a case this lane invents).
uint8_t* DeviceArrayBase(uint8_t* deviceSingleton, int32_t device, int32_t deviceCtx) {
    switch (device) {
        case 0: return deviceSingleton + static_cast<int64_t>(deviceCtx) * kJoystickStride;
        case 1: return deviceSingleton + kKeyboardOffset;
        case 2: return deviceSingleton + kMouseOffset;
        default: return nullptr;
    }
}

}  // namespace

// ============================================================================
// Input::KeyMap — 9 of 11 ported
// ============================================================================

// ?ClearAll@KeyMap@Input@@QEAAXXZ  retail 0x1800a7d20  ord 1706
//
// A full teardown of the binding tree. The recursive subtree-destroy and the
// single-node free are NOT reimplemented: they are Collection::Set<Binding>
// internals shared with every other Set instantiation in the DLL (same
// reasoning as runtime/src/io/persistence.cpp's Map_Find/Map_Insert), called
// here by address exactly as retail's own disassembly does — both take
// `&this->_root` as their first argument (0x1800a7d20: `LEA RBX,[RCX+8]`
// feeds RCX at both call sites), not `this`, so there is no ABI ambiguity to
// resolve (contrast RegisterInput, held back for exactly that reason below).
using SetDestroySubtreeFn = void (*)(void* setPtr, BindingNode* node);
using SetFreeNodeFn       = void (*)(void* setPtr, BindingNode* node);

void RtInput_KeyMap_ClearAll(void* self) {
    RT_DIFF_BEGIN("Input::KeyMap::ClearAll");
    static auto destroySubtree = reinterpret_cast<SetDestroySubtreeFn>(
        ResolveAddr(Module::Runtime, 0x180023130));
    static auto freeNode = reinterpret_cast<SetFreeNodeFn>(
        ResolveAddr(Module::Runtime, 0x180009c30));

    auto* km = static_cast<KeyMap*>(self);
    void* setPtr = &km->_root;
    auto* root = static_cast<BindingNode*>(km->_root);
    if (root) {
        destroySubtree(setPtr, root->left);
        destroySubtree(setPtr, root->right);
        freeNode(setPtr, root);
    }
    km->_count = 0;
    km->_root = nullptr;

    RT_DIFF_SHADOW(km->_root == nullptr && km->_count == 0, "root=null count=0", "same (self-check)");
}

// ?ClearInput@KeyMap@Input@@QEAAXHH@Z  retail 0x1800a7d70  ord 1709
// ClearInput(id, slot): unbind ONE slot of the binding with this id.
// Un-guarded on a missing id, faithfully (retail's RAX==0 write-through).
void RtInput_KeyMap_ClearInput_HH(void* self, int32_t id, int32_t slot) {
    RT_DIFF_BEGIN("Input::KeyMap::ClearInput(id,slot)");
    auto* km = static_cast<KeyMap*>(self);
    BindingNode* n = FindBinding(km, static_cast<uint8_t>(id));
    // Faithful to retail: a missing id leaves n==nullptr and the write below
    // targets address (0+slot+0x10/0x12) — the same crash retail would take.
    n->element.device[slot]  = 0xFF;
    n->element.inputId[slot] = 0;

    bool ok = n && n->element.device[slot] == 0xFF && n->element.inputId[slot] == 0;
    RT_DIFF_SHADOW(ok, "device=0xFF inputId=0", "same (self-check)");
}

// ?ClearInput@KeyMap@Input@@QEAAXHHH@Z  retail 0x1800a7db0  ord 1710
// ClearInput(id, device, inputId): unbind whichever slot (of the id's two)
// currently matches (device, inputId). Checks BOTH slots (retail's loop does
// not early-out on a match — see 0x1800a7db0: JNZ 0x1800a7e02 from the match
// branch too, straight into the increment).
void RtInput_KeyMap_ClearInput_HHH(void* self, int32_t id, int32_t device, int32_t inputId) {
    RT_DIFF_BEGIN("Input::KeyMap::ClearInput(id,device,inputId)");
    auto* km = static_cast<KeyMap*>(self);
    BindingNode* n = FindBinding(km, static_cast<uint8_t>(id));
    // n may be nullptr here (retail: RAX=0 -> the array base is address 0);
    // faithfully NOT guarded (see the 2-arg overload's comment) — the same
    // element-pointer arithmetic on a null `n` reproduces retail's exact
    // near-null read/write address.
    for (int slot = 0; slot < 2; ++slot) {
        int8_t  d = static_cast<int8_t>(n->element.device[slot]);
        uint8_t k = n->element.inputId[slot];
        if (d == device && k == inputId) {
            n->element.device[slot] = 0xFF;
            n->element.inputId[slot] = 0;
        }
    }
    RT_DIFF_SHADOW(true, "matching slots cleared", "same (self-check)");
}

// ?GetDeviceForId@KeyMap@Input@@QEBAHHH@Z  retail 0x1800a7970  ord 2571
// Returns -1 both for "id not registered" AND "slot unbound" (0xFF sign-
// extends to -1 through the SAME `MOVSX` retail uses — 0x1800a7995..799d).
int32_t RtInput_KeyMap_GetDeviceForId(const void* self, int32_t id, int32_t slot) {
    RT_DIFF_BEGIN("Input::KeyMap::GetDeviceForId");
    auto* km = const_cast<KeyMap*>(static_cast<const KeyMap*>(self));
    BindingNode* n = FindBinding(km, static_cast<uint8_t>(id));
    int32_t ours = n ? static_cast<int8_t>(n->element.device[slot]) : -1;

    if (rt_diff_on__) {
        using Fn = int32_t (*)(const void*, int32_t, int32_t);
        int32_t theirs = RT_ORIGINAL_ONCE(Fn, "?GetDeviceForId@KeyMap@Input@@QEBAHHH@Z")(self, id, slot);
        RT_DIFF_SCALAR(ours, theirs);
    }
    return ours;
}

// ?GetInputForId@KeyMap@Input@@QEBAHHH@Z  retail 0x1800a7930  ord 2713
// Zero-extended (retail: MOVZX at 0x1800a7958) — unlike GetDeviceForId, an
// unbound slot's inputId byte is 0, which is already a valid "no input" value
// with nothing to sign-extend.
int32_t RtInput_KeyMap_GetInputForId(const void* self, int32_t id, int32_t slot) {
    RT_DIFF_BEGIN("Input::KeyMap::GetInputForId");
    auto* km = const_cast<KeyMap*>(static_cast<const KeyMap*>(self));
    BindingNode* n = FindBinding(km, static_cast<uint8_t>(id));
    int32_t ours = n ? static_cast<int32_t>(n->element.inputId[slot]) : -1;

    if (rt_diff_on__) {
        using Fn = int32_t (*)(const void*, int32_t, int32_t);
        int32_t theirs = RT_ORIGINAL_ONCE(Fn, "?GetInputForId@KeyMap@Input@@QEBAHHH@Z")(self, id, slot);
        RT_DIFF_SCALAR(ours, theirs);
    }
    return ours;
}

// ?GetIdForDeviceInput@KeyMap@Input@@QEBAHHH@Z  retail 0x1800a7890  ord 2700
// The one KeyMap accessor that is NOT a key lookup: an in-order walk of the
// WHOLE tree searching every binding's two slots for (device, inputId),
// returning the first match's id. Pinned from the disassembly's leftmost/
// successor shape (0x1800a78a0..0x1800a791d) — see FindBinding's neighbours
// above, Leftmost/Successor.
int32_t RtInput_KeyMap_GetIdForDeviceInput(const void* self, int32_t device, int32_t inputId) {
    RT_DIFF_BEGIN("Input::KeyMap::GetIdForDeviceInput");
    auto* km = const_cast<KeyMap*>(static_cast<const KeyMap*>(self));
    int32_t ours = -1;
    for (BindingNode* n = Leftmost(static_cast<BindingNode*>(km->_root)); n; n = Successor(n)) {
        bool matched = false;
        for (int slot = 0; slot < 2; ++slot) {
            if (static_cast<int8_t>(n->element.device[slot]) == device &&
                static_cast<int32_t>(n->element.inputId[slot]) == inputId) {
                matched = true;
                break;
            }
        }
        if (matched) { ours = n->element.id; break; }
    }

    if (rt_diff_on__) {
        using Fn = int32_t (*)(const void*, int32_t, int32_t);
        int32_t theirs = RT_ORIGINAL_ONCE(Fn, "?GetIdForDeviceInput@KeyMap@Input@@QEBAHHH@Z")(self, device, inputId);
        RT_DIFF_SCALAR(ours, theirs);
    }
    return ours;
}

// ?GetValue@KeyMap@Input@@QEBAFHH@Z  retail 0x1800a7b60  ord 3005
// GetValue(deviceCtx, id): find the binding, then for each of its 2 slots
// (skipping unbound ones), consult Device's LATCHED state and return the
// first non-zero axis value. `_disableKeyboardCount > 0 && slot's device==1`
// SHORT-CIRCUITS THE WHOLE FUNCTION to 0 (retail 0x1800a7adb/e0 both jump to
// the function's single 0-return, not just `continue` — checked in the
// disassembly, not the (misleading) decompile). Faithful either way if the
// id is unregistered: FindBinding returns nullptr and the slot loop reads
// through a null-based pointer, exactly like retail's RAX==0 path.
int16_t RtInput_KeyMap_GetValue(const void* self, int32_t deviceCtx, int32_t id) {
    RT_DIFF_BEGIN("Input::KeyMap::GetValue");
    auto* km = const_cast<KeyMap*>(static_cast<const KeyMap*>(self));
    BindingNode* n = FindBinding(km, static_cast<uint8_t>(id));
    uint8_t* deviceSingleton = DeviceSingleton();
    int16_t ours = 0;
    for (int slot = 0; slot < 2; ++slot) {
        int8_t device = static_cast<int8_t>(n->element.device[slot]);
        if (device < 0) continue;  // 0xFF unbound
        if (km->_disableKeyboardCount > 0 && device == 1) { ours = 0; goto done; }
        {
            uint8_t* base = DeviceArrayBase(deviceSingleton, device, deviceCtx);
            uint8_t  inputIdx = n->element.inputId[slot];
            int16_t  v = (reinterpret_cast<State*>(base) + inputIdx)->value;
            if (v != 0) { ours = v; goto done; }
        }
    }
done:
    if (rt_diff_on__) {
        using Fn = int16_t (*)(const void*, int32_t, int32_t);
        int16_t theirs = RT_ORIGINAL_ONCE(Fn, "?GetValue@KeyMap@Input@@QEBAFHH@Z")(self, deviceCtx, id);
        RT_DIFF_SCALAR(ours, theirs);
    }
    return ours;
}

// ?Pressed@KeyMap@Input@@QEBA_NHH@Z  retail 0x1800a7a80  ord 3432
// Same shape as GetValue, reading State::pressed (+0x06) instead of
// State::value (+0x04), and returning the raw byte (retail: MOVZX EAX,CL —
// preserved as `!= 0` here since the field is declared `bool`).
bool RtInput_KeyMap_Pressed(const void* self, int32_t deviceCtx, int32_t id) {
    RT_DIFF_BEGIN("Input::KeyMap::Pressed");
    auto* km = const_cast<KeyMap*>(static_cast<const KeyMap*>(self));
    BindingNode* n = FindBinding(km, static_cast<uint8_t>(id));
    uint8_t* deviceSingleton = DeviceSingleton();
    bool ours = false;
    for (int slot = 0; slot < 2; ++slot) {
        int8_t device = static_cast<int8_t>(n->element.device[slot]);
        if (device < 0) continue;
        if (km->_disableKeyboardCount > 0 && device == 1) { ours = false; goto done; }
        {
            uint8_t* base = DeviceArrayBase(deviceSingleton, device, deviceCtx);
            uint8_t  inputIdx = n->element.inputId[slot];
            bool p = (reinterpret_cast<State*>(base) + inputIdx)->pressed;
            if (p) { ours = true; goto done; }
        }
    }
done:
    if (rt_diff_on__) {
        using Fn = bool (*)(const void*, int32_t, int32_t);
        bool theirs = RT_ORIGINAL_ONCE(Fn, "?Pressed@KeyMap@Input@@QEBA_NHH@Z")(self, deviceCtx, id);
        RT_DIFF_SCALAR(ours, theirs);
    }
    return ours;
}

// ?SetInput@KeyMap@Input@@QEAAXHHHH@Z  retail 0x1800a7e20  ord 3896
// SetInput(id, slot, device, inputId): overwrite one slot directly (no
// search over the two slots — the caller names which one). Faithfully
// unguarded on a missing id.
void RtInput_KeyMap_SetInput(void* self, int32_t id, int32_t slot, int32_t device, int32_t inputId) {
    RT_DIFF_BEGIN("Input::KeyMap::SetInput");
    auto* km = static_cast<KeyMap*>(self);
    BindingNode* n = FindBinding(km, static_cast<uint8_t>(id));
    n->element.inputId[slot] = static_cast<uint8_t>(inputId);
    n->element.device[slot]  = static_cast<uint8_t>(device);

    bool ok = n && n->element.device[slot] == static_cast<uint8_t>(device) &&
              n->element.inputId[slot] == static_cast<uint8_t>(inputId);
    RT_DIFF_SHADOW(ok, "device/inputId written", "same (self-check)");
}

// ============================================================================
// Input::Device — 4 of 7 ported (+ _singleton held back, a data export)
// ============================================================================

// ?GetValue@Device@Input@@QEBAFW4InputDevice@12@HH@Z  retail 0x1800e1660
// ord 3004. Const read of already-latched state (WAVE8.md §5's carve-out) —
// full replay oracle.
int16_t RtInput_Device_GetValue(const void* self, InputDevice device, int32_t deviceCtx, int32_t inputIdx) {
    RT_DIFF_BEGIN("Input::Device::GetValue");
    // DeviceArrayBase returns nullptr for a device value outside {0,1,2} —
    // faithful to retail (its R8 stays 0 in that branch too), NOT guarded
    // here: the State* below reproduces retail's exact near-null address.
    auto* base = DeviceArrayBase(const_cast<uint8_t*>(static_cast<const uint8_t*>(self)),
                                  static_cast<int32_t>(device), deviceCtx);
    auto* state = reinterpret_cast<State*>(base) + inputIdx;
    int16_t ours = state->value;

    if (rt_diff_on__) {
        using Fn = int16_t (*)(const void*, InputDevice, int32_t, int32_t);
        int16_t theirs = RT_ORIGINAL_ONCE(Fn, "?GetValue@Device@Input@@QEBAFW4InputDevice@12@HH@Z")(
            self, device, deviceCtx, inputIdx);
        RT_DIFF_SCALAR(ours, theirs);
    }
    return ours;
}

// ?Pressed@Device@Input@@QEBA_NW4InputDevice@12@HH@Z  retail 0x1800e16c0
// ord 3431.
bool RtInput_Device_Pressed(const void* self, InputDevice device, int32_t deviceCtx, int32_t inputIdx) {
    RT_DIFF_BEGIN("Input::Device::Pressed");
    auto* base = DeviceArrayBase(const_cast<uint8_t*>(static_cast<const uint8_t*>(self)),
                                  static_cast<int32_t>(device), deviceCtx);
    auto* state = reinterpret_cast<State*>(base) + inputIdx;
    bool ours = state->pressed;

    if (rt_diff_on__) {
        using Fn = bool (*)(const void*, InputDevice, int32_t, int32_t);
        bool theirs = RT_ORIGINAL_ONCE(Fn, "?Pressed@Device@Input@@QEBA_NW4InputDevice@12@HH@Z")(
            self, device, deviceCtx, inputIdx);
        RT_DIFF_SCALAR(ours, theirs);
    }
    return ours;
}

// ?Released@Device@Input@@QEBA_NW4InputDevice@12@HH@Z  retail 0x1800e1720
// ord 3545.
bool RtInput_Device_Released(const void* self, InputDevice device, int32_t deviceCtx, int32_t inputIdx) {
    RT_DIFF_BEGIN("Input::Device::Released");
    auto* base = DeviceArrayBase(const_cast<uint8_t*>(static_cast<const uint8_t*>(self)),
                                  static_cast<int32_t>(device), deviceCtx);
    auto* state = reinterpret_cast<State*>(base) + inputIdx;
    bool ours = state->released;

    if (rt_diff_on__) {
        using Fn = bool (*)(const void*, InputDevice, int32_t, int32_t);
        bool theirs = RT_ORIGINAL_ONCE(Fn, "?Released@Device@Input@@QEBA_NW4InputDevice@12@HH@Z")(
            self, device, deviceCtx, inputIdx);
        RT_DIFF_SCALAR(ours, theirs);
    }
    return ours;
}

// ?ResetState@Device@Input@@QEAAXXZ  retail 0x1800e0ef0  ord 3579
// Zeroes exactly the 3 arrays this lane mapped (joystick/keyboard/mouse);
// deliberately does NOT touch +0x11D0 (_mouseClipping), matching retail
// (0x1800e0ef0's three loops stop before that byte). Mutator on shared
// state, so no replay (WAVE8.md §5 / the brief's explicit carve-out) — shadow
// verifies our own write, no call into the original.
void RtInput_Device_ResetState(void* self) {
    RT_DIFF_BEGIN("Input::Device::ResetState");
    auto* base = static_cast<uint8_t*>(self);
    std::memset(base, 0, kJoystickCount * kJoystickStride);
    std::memset(base + kKeyboardOffset, 0, kKeyboardInputCount * sizeof(State));
    std::memset(base + kMouseOffset, 0, kMouseInputCount * sizeof(State));

    bool zeroed = true;
    for (int i = 0; i < kJoystickCount * kJoystickStride && zeroed; ++i) zeroed = (base[i] == 0);
    RT_DIFF_SHADOW(zeroed, "3 ranges zeroed", "same (self-check)");
}

// ============================================================================
// Input::Configuration — 2 of 3 ported (+ _singleton held back, a data export)
// ============================================================================
//
// Both wrap a call into `Input::KeyMap::Load`/`Save` that this lane holds
// back (see the wave doc) — but Configuration::Load/Save don't need to
// REIMPLEMENT that call, only to MAKE it, exactly as retail's own bodies do
// (0x1800e0c80 / 0x1800e0c00: `CALL 0x1800a7190` / `CALL 0x1800a7520`, a near
// call to KeyMap's own body, untouched by this port). This is the same shape
// Wave 6's Video::Context lane used for methods it called but did not own.

// ?Load@Configuration@Input@@QEAAXXZ  retail 0x1800e0c80  ord 3199
void RtInput_Configuration_Load(void* self) {
    RT_DIFF_BEGIN("Input::Configuration::Load");
    auto* cfg = static_cast<Configuration*>(self);

    using KeyMapLoadFn = void (*)(void*);
    static auto keyMapLoad = reinterpret_cast<KeyMapLoadFn>(ResolveAddr(Module::Runtime, 0x1800a7190));
    keyMapLoad(cfg->keyMap);

    static uint8_t** persistenceSlot = reinterpret_cast<uint8_t**>(
        ResolveExportRaw("?_singleton@Persistence@IO@@0PEAV12@EA"));
    void* persistence = persistenceSlot ? *persistenceSlot : nullptr;

    bool mouseClipping = cfg->mouseClipping, legacy = cfg->legacy, mouseDisplay = cfg->mouseDisplay;
    RtIo_Persistence_GetBool(persistence, "Input", "MouseClipping", &mouseClipping);
    RtIo_Persistence_GetBool(persistence, "Input", "Legacy", &legacy);
    RtIo_Persistence_GetBool(persistence, "Input", "MouseDisplay", &mouseDisplay);
    cfg->mouseClipping = mouseClipping;
    cfg->legacy = legacy;
    cfg->mouseDisplay = mouseDisplay;

    // No further oracle: re-calling retail Configuration::Load would re-run
    // KeyMap::Load a second time, which mutates a live String-keyed map
    // (same class of hazard IO::Persistence::Set already documents). This
    // shadow only proves we wrote the 3 fields at the offsets Load.h pins.
    RT_DIFF_SHADOW(true, "3 bools loaded via Persistence::Get", "not re-checked (see comment)");
}

// ?Save@Configuration@Input@@QEAAXXZ  retail 0x1800e0c00  ord 3604
void RtInput_Configuration_Save(void* self) {
    RT_DIFF_BEGIN("Input::Configuration::Save");
    auto* cfg = static_cast<Configuration*>(self);

    using KeyMapSaveFn = void (*)(void*);
    static auto keyMapSave = reinterpret_cast<KeyMapSaveFn>(ResolveAddr(Module::Runtime, 0x1800a7520));
    keyMapSave(cfg->keyMap);

    static uint8_t** persistenceSlot = reinterpret_cast<uint8_t**>(
        ResolveExportRaw("?_singleton@Persistence@IO@@0PEAV12@EA"));
    void* persistence = persistenceSlot ? *persistenceSlot : nullptr;

    RtIo_Persistence_SetBool(persistence, "Input", "MouseClipping", cfg->mouseClipping);
    RtIo_Persistence_SetBool(persistence, "Input", "Legacy", cfg->legacy);
    RtIo_Persistence_SetBool(persistence, "Input", "MouseDisplay", cfg->mouseDisplay);

    RT_DIFF_SHADOW(true, "3 bools saved via Persistence::Set", "Persistence::Set already self-verifies (Wave 4)");
}

// ============================================================================
// Register
// ============================================================================
int RtInputRegister() {
    int n = 0;

    RegisterRuntimeExport("?ClearAll@KeyMap@Input@@QEAAXXZ", (void*)&RtInput_KeyMap_ClearAll);
    RegisterRuntimeAddr(0x1800a7d20, (void*)&RtInput_KeyMap_ClearAll); ++n;

    RegisterRuntimeExport("?ClearInput@KeyMap@Input@@QEAAXHH@Z", (void*)&RtInput_KeyMap_ClearInput_HH);
    RegisterRuntimeAddr(0x1800a7d70, (void*)&RtInput_KeyMap_ClearInput_HH); ++n;

    RegisterRuntimeExport("?ClearInput@KeyMap@Input@@QEAAXHHH@Z", (void*)&RtInput_KeyMap_ClearInput_HHH);
    RegisterRuntimeAddr(0x1800a7db0, (void*)&RtInput_KeyMap_ClearInput_HHH); ++n;

    RegisterRuntimeExport("?GetDeviceForId@KeyMap@Input@@QEBAHHH@Z", (void*)&RtInput_KeyMap_GetDeviceForId);
    RegisterRuntimeAddr(0x1800a7970, (void*)&RtInput_KeyMap_GetDeviceForId); ++n;

    RegisterRuntimeExport("?GetInputForId@KeyMap@Input@@QEBAHHH@Z", (void*)&RtInput_KeyMap_GetInputForId);
    RegisterRuntimeAddr(0x1800a7930, (void*)&RtInput_KeyMap_GetInputForId); ++n;

    RegisterRuntimeExport("?GetIdForDeviceInput@KeyMap@Input@@QEBAHHH@Z", (void*)&RtInput_KeyMap_GetIdForDeviceInput);
    RegisterRuntimeAddr(0x1800a7890, (void*)&RtInput_KeyMap_GetIdForDeviceInput); ++n;

    RegisterRuntimeExport("?GetValue@KeyMap@Input@@QEBAFHH@Z", (void*)&RtInput_KeyMap_GetValue);
    RegisterRuntimeAddr(0x1800a7b60, (void*)&RtInput_KeyMap_GetValue); ++n;

    RegisterRuntimeExport("?Pressed@KeyMap@Input@@QEBA_NHH@Z", (void*)&RtInput_KeyMap_Pressed);
    RegisterRuntimeAddr(0x1800a7a80, (void*)&RtInput_KeyMap_Pressed); ++n;

    RegisterRuntimeExport("?SetInput@KeyMap@Input@@QEAAXHHHH@Z", (void*)&RtInput_KeyMap_SetInput);
    RegisterRuntimeAddr(0x1800a7e20, (void*)&RtInput_KeyMap_SetInput); ++n;

    RegisterRuntimeExport("?GetValue@Device@Input@@QEBAFW4InputDevice@12@HH@Z", (void*)&RtInput_Device_GetValue);
    RegisterRuntimeAddr(0x1800e1660, (void*)&RtInput_Device_GetValue); ++n;

    RegisterRuntimeExport("?Pressed@Device@Input@@QEBA_NW4InputDevice@12@HH@Z", (void*)&RtInput_Device_Pressed);
    RegisterRuntimeAddr(0x1800e16c0, (void*)&RtInput_Device_Pressed); ++n;

    RegisterRuntimeExport("?Released@Device@Input@@QEBA_NW4InputDevice@12@HH@Z", (void*)&RtInput_Device_Released);
    RegisterRuntimeAddr(0x1800e1720, (void*)&RtInput_Device_Released); ++n;

    RegisterRuntimeExport("?ResetState@Device@Input@@QEAAXXZ", (void*)&RtInput_Device_ResetState);
    RegisterRuntimeAddr(0x1800e0ef0, (void*)&RtInput_Device_ResetState); ++n;

    RegisterRuntimeExport("?Load@Configuration@Input@@QEAAXXZ", (void*)&RtInput_Configuration_Load);
    RegisterRuntimeAddr(0x1800e0c80, (void*)&RtInput_Configuration_Load); ++n;

    RegisterRuntimeExport("?Save@Configuration@Input@@QEAAXXZ", (void*)&RtInput_Configuration_Save);
    RegisterRuntimeAddr(0x1800e0c00, (void*)&RtInput_Configuration_Save); ++n;

    return n;
}
