// ============================================================================
// tests/unit/rb_engine_abi_test.cpp — ENDGAME B.3.8 block RB, the Engine loops
//
// B38_RULES D1 / RULE B8-1 on the four loop bases LOOP derives its five exe
// loops from (GameUpdateLoop : UpdateLoop, MenuDialogLoopT : DialogLoopT, the
// splash : DialogLoaderLoopT, two : LoaderLoop). Each real ctor/dtor shares
// ONE body with its door (`InitFields` / `ReleaseFields`, reached by the doors
// through `X::DoorBody`).
//
// PART A — STAMP ORDER through test subclasses: every shared body runs under
//   ITS class's compiler table (base -> derived on the way in, derived -> base
//   on the way out), in retail's order. For the two Dialog loops the
//   `_dialogFilename` String copy (a member initialiser) is already in place
//   when InitFields runs, and ~String runs AFTER ReleaseFields and BEFORE the
//   base's teardown — retail's ~DialogLoopT order (UI, String, base).
// PART B — THE DOOR REACHES THE SAME BODY with a stand-in minted table.
//
// NOT PROVEN HERE: the bodies (runtime/src/engine/loops.cpp stands on the
// registry). They are RECORDING MARKERS below, as are the loops' virtuals (so
// that GCC emits the four vtables in this translation unit; nothing else in
// the unit binary defines them — engine_loop_abi_test.cpp measures slots with
// pointers to members and links no body).
// ============================================================================
#include "unit.h"

#include "runtime/types/engine/UpdateLoop.h"

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <new>
#include <type_traits>

extern const void* const g_rbUlVtGroup[]  __asm__("_ZTVN6Engine10UpdateLoopE");
extern const void* const g_rbLlVtGroup[]  __asm__("_ZTVN6Engine10LoaderLoopE");
extern const void* const g_rbDlVtGroup[]  __asm__("_ZTVN6Engine11DialogLoopTE");
extern const void* const g_rbDllVtGroup[] __asm__("_ZTVN6Engine17DialogLoaderLoopTE");

namespace {

const void* UlVt()  { return &g_rbUlVtGroup[2]; }
const void* LlVt()  { return &g_rbLlVtGroup[2]; }
const void* DlVt()  { return &g_rbDlVtGroup[2]; }
const void* DllVt() { return &g_rbDllVtGroup[2]; }
const void* VptrOf(const void* obj) { return *static_cast<const void* const*>(obj); }

// An ordered log of (event, vptr) pairs.
enum Ev { kUlInit = 1, kLlInit, kDlInit, kDllInit, kUlRel, kDlRel, kDllRel };
struct Entry { int ev; const void* vptr; const char* filename; };
Entry g_log[16];
int   g_n = 0;
void Log(int ev, const void* self, const char* fn = nullptr) {
    if (g_n < 16) g_log[g_n++] = Entry{ev, VptrOf(self), fn};
}
void Clear() { g_n = 0; }

int64_t g_delaySeen = 0;
void*   g_nextSeen  = nullptr;

const void* const kFakeMinted[4] = {};

}  // namespace

// A System::String is its refcounted char* (String.h).
static const char* Raw(const System::String& s) {
    return *reinterpret_cast<const char* const*>(&s);
}

// ===========================================================================
// Markers: the shared bodies and the loops' own virtuals.
// ===========================================================================
namespace Engine {
void UpdateLoop::InitFields()    { Log(kUlInit, this); _loadBit = 5; _loadProgress = 0; }
void UpdateLoop::ReleaseFields() { Log(kUlRel, this); }
void LoaderLoop::InitFields(UpdateLoop* nextLoop, int64_t delayTicks) {
    Log(kLlInit, this);
    g_nextSeen = nextLoop; g_delaySeen = delayTicks;
    _nextLoop = nextLoop; _delayTicks = delayTicks; _autoAdvance = 1;
}
void DialogLoopT::InitFields()       { Log(kDlInit, this, Raw(_dialogFilename)); }
void DialogLoopT::ReleaseFields()    { Log(kDlRel, this, Raw(_dialogFilename)); }
void DialogLoaderLoopT::InitFields() { Log(kDllInit, this, Raw(_dialogFilename)); }
void DialogLoaderLoopT::ReleaseFields() { Log(kDllRel, this, Raw(_dialogFilename)); }

void UpdateLoop::DrawFailed() {}
void UpdateLoop::SaveRestore(bool, Save::SavedState*) {}
void UpdateLoop::OnVideoError() {}
void UpdateLoop::OnVideoRestart() {}
const char* UpdateLoop::GetResourceFilename() { return nullptr; }
bool UpdateLoop::AllowsModChanges() { return false; }

void LoaderLoop::Startup() {}
void LoaderLoop::Shutdown() {}
bool LoaderLoop::Update(const System::Time&) { return false; }
void LoaderLoop::Draw(float) {}
void LoaderLoop::Load() {}
void LoaderLoop::OnVideoError() {}

void DialogLoopT::Startup() {}
void DialogLoopT::Shutdown() {}
bool DialogLoopT::Update(const System::Time&) { return false; }
void DialogLoopT::Draw(float) {}
void DialogLoopT::Load() {}

void DialogLoaderLoopT::Startup() {}
void DialogLoaderLoopT::Shutdown() {}
bool DialogLoaderLoopT::Update(const System::Time&) { return false; }
void DialogLoaderLoopT::Draw(float) {}
void DialogLoaderLoopT::Load() {}
}  // namespace Engine

namespace {

// ---- test subclasses: stand-ins for LOOP's five exe loops -----------------
struct RbGameLoop final : Engine::UpdateLoop {
    Engine::UpdateLoop* DeletingDtor(unsigned) override { return this; }
    void Startup() override {}
    void Shutdown() override {}
    bool Update(const System::Time&) override { return true; }
    void Draw(float) override {}
    void Load() override {}
};
struct RbLoaderLoop final : Engine::LoaderLoop {
    RbLoaderLoop(Engine::UpdateLoop* next, const System::Time& delay)
        : Engine::LoaderLoop(next, delay) {}
    Engine::UpdateLoop* DeletingDtor(unsigned) override { return this; }
};
struct RbMenuLoop final : Engine::DialogLoopT {
    explicit RbMenuLoop(const System::String& f) : Engine::DialogLoopT(f) {}
    Engine::UpdateLoop* DeletingDtor(unsigned) override { return this; }
    Object::InternalPtr<UI::DialogInstance>* CreateLoopDialog(
            Object::InternalPtr<UI::DialogInstance>* ret) override { return ret; }
};
struct RbSplashLoop final : Engine::DialogLoaderLoopT {
    RbSplashLoop(const System::String& f, Engine::UpdateLoop* next, const System::Time& d)
        : Engine::DialogLoaderLoopT(f, next, d) {}
    Engine::UpdateLoop* DeletingDtor(unsigned) override { return this; }
    Object::InternalPtr<UI::DialogInstance>* CreateLoopDialog(
            Object::InternalPtr<UI::DialogInstance>* ret) override { return ret; }
};

const System::Time& TimeOf(const int64_t& ticks) {
    return *reinterpret_cast<const System::Time*>(&ticks);   // one int64 at +0
}

// ===========================================================================
// PART A
// ===========================================================================
UNIT_TEST(rb_updateloop_ctor_dtor_run_shared_bodies_in_order) {
    Clear();
    alignas(8) unsigned char buf[sizeof(RbGameLoop)];
    RbGameLoop* l = ::new (static_cast<void*>(buf)) RbGameLoop;
    CHECK_EQ(g_n, 1);
    CHECK_EQ(g_log[0].ev, (int)kUlInit);
    CHECK(g_log[0].vptr == UlVt());
    CHECK(VptrOf(l) != UlVt());
    CHECK_EQ(l->GetLoadBit(), 5);
    l->~RbGameLoop();
    CHECK_EQ(g_n, 2);
    CHECK_EQ(g_log[1].ev, (int)kUlRel);
    CHECK(g_log[1].vptr == UlVt());
}

UNIT_TEST(rb_loaderloop_ctor_dtor_stamp_order) {
    Clear();
    int nextToken = 0;
    const int64_t delay = 12345;
    alignas(8) unsigned char buf[sizeof(RbLoaderLoop)];
    RbLoaderLoop* l = ::new (static_cast<void*>(buf)) RbLoaderLoop(
        reinterpret_cast<Engine::UpdateLoop*>(&nextToken), TimeOf(delay));
    CHECK_EQ(g_n, 2);
    CHECK_EQ(g_log[0].ev, (int)kUlInit);  CHECK(g_log[0].vptr == UlVt());
    CHECK_EQ(g_log[1].ev, (int)kLlInit);  CHECK(g_log[1].vptr == LlVt());
    CHECK(g_nextSeen == static_cast<void*>(&nextToken));
    CHECK_EQ(g_delaySeen, delay);          // the Time& argument's ticks
    CHECK(VptrOf(l) != LlVt());
    l->~RbLoaderLoop();
    CHECK_EQ(g_n, 3);
    CHECK_EQ(g_log[2].ev, (int)kUlRel);   CHECK(g_log[2].vptr == UlVt());
}

UNIT_TEST(rb_dialogloopt_ctor_dtor_order_is_retails) {
    Clear();
    System::String name("Dialog/MainMenu.rsc");
    alignas(8) unsigned char buf[sizeof(RbMenuLoop)];
    RbMenuLoop* l = ::new (static_cast<void*>(buf)) RbMenuLoop(name);
    CHECK_EQ(g_n, 2);
    CHECK_EQ(g_log[0].ev, (int)kUlInit);  CHECK(g_log[0].vptr == UlVt());
    CHECK_EQ(g_log[1].ev, (int)kDlInit);  CHECK(g_log[1].vptr == DlVt());
    // The String copy (member initialiser) precedes the shared body.
    CHECK(g_log[1].filename == Raw(name));   // copy shares the buffer (refcount)
    l->~RbMenuLoop();
    CHECK_EQ(g_n, 4);
    CHECK_EQ(g_log[2].ev, (int)kDlRel);   CHECK(g_log[2].vptr == DlVt());
    CHECK(g_log[2].filename == Raw(name));   // ~String not yet run
    CHECK_EQ(g_log[3].ev, (int)kUlRel);   CHECK(g_log[3].vptr == UlVt());
}

UNIT_TEST(rb_dialogloaderloopt_ctor_dtor_order_is_retails) {
    Clear();
    System::String name("Dialog/Splash.rsc");
    const int64_t delay = 7;
    alignas(8) unsigned char buf[sizeof(RbSplashLoop)];
    RbSplashLoop* l = ::new (static_cast<void*>(buf)) RbSplashLoop(name, nullptr, TimeOf(delay));
    CHECK_EQ(g_n, 3);
    CHECK_EQ(g_log[0].ev, (int)kUlInit);  CHECK(g_log[0].vptr == UlVt());
    CHECK_EQ(g_log[1].ev, (int)kLlInit);  CHECK(g_log[1].vptr == LlVt());
    CHECK_EQ(g_log[2].ev, (int)kDllInit); CHECK(g_log[2].vptr == DllVt());
    CHECK(g_log[2].filename == Raw(name));
    l->~RbSplashLoop();
    CHECK_EQ(g_n, 5);
    CHECK_EQ(g_log[3].ev, (int)kDllRel);  CHECK(g_log[3].vptr == DllVt());
    CHECK_EQ(g_log[4].ev, (int)kUlRel);   CHECK(g_log[4].vptr == UlVt());
}

// ===========================================================================
// PART B
// ===========================================================================
UNIT_TEST(rb_loop_doors_reach_the_same_bodies) {
    alignas(8) unsigned char raw[sizeof(Engine::DialogLoaderLoopT)] = {};
    *reinterpret_cast<const void**>(raw) = kFakeMinted;
    Clear();
    Engine::UpdateLoop::DoorBody::Init(reinterpret_cast<Engine::UpdateLoop*>(raw));
    Engine::LoaderLoop::DoorBody::Init(reinterpret_cast<Engine::LoaderLoop*>(raw), nullptr, 3);
    Engine::DialogLoaderLoopT::DoorBody::Init(reinterpret_cast<Engine::DialogLoaderLoopT*>(raw));
    Engine::DialogLoaderLoopT::DoorBody::Release(reinterpret_cast<Engine::DialogLoaderLoopT*>(raw));
    Engine::UpdateLoop::DoorBody::Release(reinterpret_cast<Engine::UpdateLoop*>(raw));
    Engine::DialogLoopT::DoorBody::Init(reinterpret_cast<Engine::DialogLoopT*>(raw));
    Engine::DialogLoopT::DoorBody::Release(reinterpret_cast<Engine::DialogLoopT*>(raw));
    CHECK_EQ(g_n, 7);
    const int want[7] = {kUlInit, kLlInit, kDllInit, kDllRel, kUlRel, kDlInit, kDlRel};
    for (int i = 0; i < 7; ++i) {
        CHECK_EQ(g_log[i].ev, want[i]);
        CHECK(g_log[i].vptr == static_cast<const void*>(kFakeMinted));
    }
    CHECK_EQ(g_delaySeen, (int64_t)3);
}

UNIT_TEST(rb_loop_bases_shape) {
    CHECK(std::is_abstract<Engine::UpdateLoop>::value);
    CHECK(std::is_abstract<Engine::LoaderLoop>::value);
    CHECK(std::is_abstract<Engine::DialogLoopT>::value);
    CHECK(std::is_abstract<Engine::DialogLoaderLoopT>::value);
    CHECK(!std::has_virtual_destructor<Engine::UpdateLoop>::value);
    CHECK(!std::has_virtual_destructor<Engine::DialogLoaderLoopT>::value);
    CHECK_EQ(sizeof(RbGameLoop), (size_t)0x28);     // a subclass adds nothing by itself
    CHECK_EQ(sizeof(RbSplashLoop), (size_t)0x188);
}

}  // namespace
