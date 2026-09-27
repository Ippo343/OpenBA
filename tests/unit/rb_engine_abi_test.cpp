// ============================================================================
// tests/unit/rb_engine_abi_test.cpp — ENDGAME B.3.8 block RB, the Engine loops
//
// B38_RULES D1 / RULE B8-1 on the four loop bases LOOP derives its five exe
// loops from (GameUpdateLoop : UpdateLoop, MenuDialogLoopT : DialogLoopT, the
// splash : DialogLoaderLoopT, two : LoaderLoop). Each real ctor/dtor shares
// ONE body (`InitFields` / `ReleaseFields`). ENDGAME B.3.9 ENG W2: the doors
// and `X::DoorBody` are gone; PART B pins the bases' own concrete slot 0.
//
// PART A — STAMP ORDER through test subclasses: every shared body runs under
//   ITS class's compiler table (base -> derived on the way in, derived -> base
//   on the way out), in retail's order. For the two Dialog loops the
//   `_dialogFilename` String copy (a member initialiser) is already in place
//   when InitFields runs, and ~String runs AFTER ReleaseFields and BEFORE the
//   base's teardown — retail's ~DialogLoopT order (UI, String, base).
// PART B — each base's concrete DeletingDtor: scalar arm (free on &1) and the
//   VECTOR arm (count at this-8, destroy downward, return this-8).
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
#include <cstdlib>
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
struct Entry { int ev; const void* vptr; const char* filename; const void* self; };
Entry g_log[16];
int   g_n = 0;
void Log(int ev, const void* self, const char* fn = nullptr) {
    if (g_n < 16) g_log[g_n++] = Entry{ev, VptrOf(self), fn, self};
}
void Clear() { g_n = 0; }

int64_t g_delaySeen = 0;
void*   g_nextSeen  = nullptr;

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
// PART B -- ENDGAME B.3.9 ENG W2: the doors are gone; slot 0 is each base's own
// concrete DeletingDtor with the retired RT_DELDTOR rows' VECTOR arms.
// ===========================================================================
UNIT_TEST(b39_eng_loaderloop_is_concrete_and_its_ctor_public) {
    Clear();
    const int64_t delay = 3;
    void* mem = std::malloc(sizeof(Engine::LoaderLoop));
    Engine::LoaderLoop* l = ::new (mem) Engine::LoaderLoop(nullptr, TimeOf(delay));
    CHECK_EQ(g_n, 2);
    CHECK_EQ(g_log[0].ev, (int)kUlInit);  CHECK(g_log[0].vptr == UlVt());
    CHECK_EQ(g_log[1].ev, (int)kLlInit);  CHECK(g_log[1].vptr == LlVt());
    CHECK(VptrOf(l) == LlVt());           // the COMPILER's table, not a minted one
    CHECK_EQ(g_delaySeen, delay);
    // DeletingDtor(0): destroy, no free, returns this.
    CHECK(l->DeletingDtor(0) == static_cast<Engine::UpdateLoop*>(l));
    CHECK_EQ(g_n, 3);
    CHECK_EQ(g_log[2].ev, (int)kUlRel);   CHECK(g_log[2].vptr == UlVt());
    // DeletingDtor(1): destroy and free (the host MemFree is std::free).
    l = ::new (mem) Engine::LoaderLoop(nullptr, TimeOf(delay));
    Clear();
    CHECK(l->DeletingDtor(1) == static_cast<Engine::UpdateLoop*>(l));
    CHECK_EQ(g_n, 1);
    CHECK_EQ(g_log[0].ev, (int)kUlRel);
}

UNIT_TEST(b39_eng_loaderloop_deletingdtor_vector_arm) {
    // MSVC's new[] block: int32 count at -8, then the 0x48-byte elements.
    unsigned char* block = static_cast<unsigned char*>(std::malloc(8 + 3 * 0x48));
    *reinterpret_cast<int32_t*>(block) = 3;
    const int64_t delay = 1;
    Engine::LoaderLoop* e[3];
    for (int i = 0; i < 3; ++i)
        e[i] = ::new (block + 8 + i * 0x48) Engine::LoaderLoop(nullptr, TimeOf(delay));
    Clear();
    // flags 3: destroy all three DOWNWARD, free the block, return this - 8.
    CHECK(e[0]->DeletingDtor(3) == reinterpret_cast<Engine::UpdateLoop*>(block));
    CHECK_EQ(g_n, 3);
    for (int i = 0; i < 3; ++i) {
        CHECK_EQ(g_log[i].ev, (int)kUlRel);
        CHECK(g_log[i].self == static_cast<const void*>(e[2 - i]));
    }
}

UNIT_TEST(b39_eng_loop_bases_deletingdtor_runs_the_whole_chain) {
    System::String name("Dialog/MainMenu.rsc");
    alignas(8) unsigned char buf[sizeof(RbMenuLoop)];
    RbMenuLoop* m = ::new (static_cast<void*>(buf)) RbMenuLoop(name);
    Clear();
    // The BASE's slot 0 body (qualified: RbMenuLoop has its own).
    CHECK(m->Engine::DialogLoopT::DeletingDtor(0) == static_cast<Engine::UpdateLoop*>(m));
    CHECK_EQ(g_n, 2);
    CHECK_EQ(g_log[0].ev, (int)kDlRel);   CHECK(g_log[0].vptr == DlVt());
    CHECK_EQ(g_log[1].ev, (int)kUlRel);   CHECK(g_log[1].vptr == UlVt());

    const int64_t delay = 9;
    alignas(8) unsigned char buf2[sizeof(RbSplashLoop)];
    RbSplashLoop* s = ::new (static_cast<void*>(buf2)) RbSplashLoop(name, nullptr, TimeOf(delay));
    Clear();
    CHECK(s->Engine::DialogLoaderLoopT::DeletingDtor(0) == static_cast<Engine::UpdateLoop*>(s));
    CHECK_EQ(g_n, 2);
    CHECK_EQ(g_log[0].ev, (int)kDllRel);  CHECK(g_log[0].vptr == DllVt());
    CHECK_EQ(g_log[1].ev, (int)kUlRel);   CHECK(g_log[1].vptr == UlVt());

    alignas(8) unsigned char buf3[sizeof(RbGameLoop)];
    RbGameLoop* g = ::new (static_cast<void*>(buf3)) RbGameLoop;
    Clear();
    CHECK(g->Engine::UpdateLoop::DeletingDtor(0) == static_cast<Engine::UpdateLoop*>(g));
    CHECK_EQ(g_n, 1);
    CHECK_EQ(g_log[0].ev, (int)kUlRel);
}

UNIT_TEST(rb_loop_bases_shape) {
    CHECK(std::is_abstract<Engine::UpdateLoop>::value);          // pure 1,2,3,4,6 (retail)
    CHECK(!std::is_abstract<Engine::LoaderLoop>::value);         // concrete, as in retail
    CHECK(std::is_abstract<Engine::DialogLoopT>::value);         // pure 12 (retail)
    CHECK(std::is_abstract<Engine::DialogLoaderLoopT>::value);
    CHECK(!std::is_destructible<Engine::LoaderLoop>::value);     // protected dtor
    CHECK(!std::has_virtual_destructor<Engine::LoaderLoop>::value);
    CHECK(!std::has_virtual_destructor<Engine::DialogLoopT>::value);
    CHECK(!std::has_virtual_destructor<Engine::UpdateLoop>::value);
    CHECK(!std::has_virtual_destructor<Engine::DialogLoaderLoopT>::value);
    CHECK_EQ(sizeof(RbGameLoop), (size_t)0x28);     // a subclass adds nothing by itself
    CHECK_EQ(sizeof(RbSplashLoop), (size_t)0x188);
}

}  // namespace
