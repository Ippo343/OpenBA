// code/Game/Component/citizenaicomponent_serialize.cpp
// Slot-3 (+0x18) stream Serialize for CitizenAIComponent.
//
//   CitizenAIComponent::Serialize   retail 0x14010f960   object size 0xd0
//
// Save/load wave (2026-08-27): citizen entities crash on the first post-load
// tick because CitizenAIComponent's stream-Serialize vtable slot was only the
// Component-base default — the AI state (+0x28), sibling-citizen backref
// (+0x38) and the 16 per-state Time timers (+0x40..+0xc0/+0xc8) never
// round-tripped. Wired unconditionally in citizenaicomponent.cpp.
//
// Retail body (byte-faithful):
//   Component_Serialize(self, stream);
//   Serializer ser(stream, /*version*/5);
//   IBP::Serialize(self+0x38, ser); validate sibling CitizenComponent, null on mismatch.
//   Serialize_Int(ser, self+0x2c);                       // pending-state
//   if (ser.GetVersion() < 5) {
//       for 14 Time entries at self+0x40 (stride 8): Time::Serialize(entry, ser);
//       self+0xb0 = Time(0.0f);   self+0xb8 = Time(0.0f);  // v<5 stream lacked these
//   } else {
//       for 16 Time entries at self+0x40 (stride 8): Time::Serialize(entry, ser);
//   }
//   if (ser.GetVersion() > 0) { Serialize_Bool(self+0x30); Serialize_Bool(self+0x31); }
//   if (ser.GetVersion() > 1) { Serialize_Int(self+0x28); }         // aiState
//   if (ser.GetVersion() > 2) { Time::Serialize(self+0xc0, ser); }
//   if (ser.GetVersion() > 3) { Time::Serialize(self+0xc8, ser); }
//
// Sub-callees: all resolved through the shared engine_funcs.h globals already
// initialized at startup (g_fn_Serializer_Ctor/Dtor/GetVersion, g_fn_Serialize_Int,
// g_fn_Serialize_Bool, g_fn_Time_Serialize, fn_Time_Ctor_Float, fn_InternalBasePtr_Serialize,
// fn_IsDerivedFrom) plus the already-ported Component base serialize
// (Component_SerializeReimpl, component_factory_base.cpp / absorbresourcecomponent.cpp).
// No new sub-function ports were required.
//
// Validation type for +0x38 (sibling CitizenComponent) is CitizenComponent's own
// type id, 0x1401c7158 — GRADUATED default-on (see project_citid_graduated memory),
// so we resolve it the same self-contained way citizenaicomponent.cpp already does
// for its own sibling-find (GetTypeIdByName, not the raw .rdata global).

#include "resolve.h"
#include "engine_funcs.h"
#include "Object/factory_registration.h"
#include <cstdint>

extern void Component_SerializeReimpl(void* self, void* stream);

namespace {

inline uint8_t* At(void* self, ptrdiff_t off) {
    return reinterpret_cast<uint8_t*>(self) + off;
}

uint32_t CitizenAISer_TidCitizenComponent() {
    static uint32_t s_tid = 0;
    if (s_tid == 0) s_tid = ::Object::GetTypeIdByName("CitizenComponent");
    return s_tid;
}

} // namespace

extern "C" void CitizenAIComponent_Serialize(void* self, void* stream) {
    if (!self || !stream) return;
    Component_SerializeReimpl(self, stream);

    if (!g_fn_Serializer_Ctor || !g_fn_Serializer_Dtor || !g_fn_Serializer_GetVersion)
        return;

    uint8_t ser[24];
    g_fn_Serializer_Ctor(ser, stream, 5);

    if (fn_InternalBasePtr_Serialize) {
        fn_InternalBasePtr_Serialize(At(self, 0x38), ser);
        void* sibling = *reinterpret_cast<void**>(At(self, 0x38));
        if (sibling && fn_IsDerivedFrom) {
            uint32_t tid = CitizenAISer_TidCitizenComponent();
            if (tid && !fn_IsDerivedFrom(sibling, tid))
                *reinterpret_cast<void**>(At(self, 0x38)) = nullptr;
        }
    }

    if (g_fn_Serialize_Int) g_fn_Serialize_Int(ser, At(self, 0x2c), nullptr);

    int version = g_fn_Serializer_GetVersion(ser);
    uint8_t* timer = At(self, 0x40);
    if (version < 5) {
        for (int i = 0; i < 14; ++i, timer += 8)
            if (g_fn_Time_Serialize) g_fn_Time_Serialize(timer, ser);
        if (fn_Time_Ctor_Float) {
            fn_Time_Ctor_Float(At(self, 0xb0), 0.0f);
            fn_Time_Ctor_Float(At(self, 0xb8), 0.0f);
        }
    } else {
        for (int i = 0; i < 16; ++i, timer += 8)
            if (g_fn_Time_Serialize) g_fn_Time_Serialize(timer, ser);
    }

    version = g_fn_Serializer_GetVersion(ser);
    if (version > 0 && g_fn_Serialize_Bool) {
        g_fn_Serialize_Bool(ser, At(self, 0x30), nullptr);
        g_fn_Serialize_Bool(ser, At(self, 0x31), nullptr);
    }
    version = g_fn_Serializer_GetVersion(ser);
    if (version > 1 && g_fn_Serialize_Int) g_fn_Serialize_Int(ser, At(self, 0x28), nullptr);
    version = g_fn_Serializer_GetVersion(ser);
    if (version > 2 && g_fn_Time_Serialize) g_fn_Time_Serialize(At(self, 0xc0), ser);
    version = g_fn_Serializer_GetVersion(ser);
    if (version > 3 && g_fn_Time_Serialize) g_fn_Time_Serialize(At(self, 0xc8), ser);

    g_fn_Serializer_Dtor(ser);
}
