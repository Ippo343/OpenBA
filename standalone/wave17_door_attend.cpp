// ============================================================================
// standalone/wave17_door_attend.cpp — Wave 17 lane CEIL
//
// DOOR ATTENDANCE. "Was this exported DATA symbol's door ever opened, and in
// which phase?"
//
// WHY THIS EXISTS. A readiness screen says a minted row is SAFE TO STAMP; it
// does not say anything dispatches it. Lane TINFO's rule stands: only an
// attendance counter separates SAFE from NEVER RAN
// (reference_a_green_readiness_screen_is_not_a_route.md). But the 62 rows this
// lane triages are read through a dozen different call sites in five
// subsystems, and instrumenting each one is 62 edits to bodies this lane is
// forbidden to touch.
//
// The one thing every one of those sites has in common is the DOOR: each
// reaches its row through ResolveExport(<mangled>), either directly (a
// function-local `static void* v = ResolveExport(...)`) or through
// RT_DATA_SLOT -> RtResolveCached -> ResolveExport. Counting HERE covers all
// 62 with one instrument and no edit to any ported body.
//
// WHAT THE NUMBER MEANS, STATED BEFORE IT IS READ — because misreading it is
// the whole risk. Nearly every consumer caches the answer in a function-local
// static, so:
//
//     post >= 1   the consumer RAN at least once. This is the attendance
//                 fact, and it is sound.
//     post == 1   says NOTHING about how often. A `static void* v =
//                 ResolveExport(...)` inside a body called 100,000 times
//                 reads exactly 1 here. It is NOT "ran once".
//     post == 0   the door was NEVER opened after init on this route: no
//                 consumer of this symbol ran.
//
// So this screen can prove ATTENDANCE and can prove ABSENCE. It cannot measure
// frequency, and it prints nothing that could be read as frequency.
//
// PHASE. A resolve during RuntimePortInit is a REGISTRATION, not a consumer
// running — standalone/reg_unreg_lane3.cpp eagerly resolves several
// ?_typeInfo@ rows to build dispatch slots. Counting that as attendance would
// hand every such row a false route. init and post are counted separately and
// only `post` is attendance.
//
// POSITIVE CONTROL, and it must print non-zero before any zero below is worth
// anything (reference_probe_needs_an_effectiveness_control.md): the watch list
// always carries ?_singleton@MainLoop@Engine@@1PEAV12@EA, which
// mainloop_video.cpp reads through RT_DATA_SLOT on the main loop path. If that
// reads 0 the instrument is not wired to the door and every other zero is an
// artefact — the summary says exactly that and SUPPRESSES the table rather
// than printing a page of plausible zeros.
// ============================================================================

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

struct Watch {
    const char* sym;
    unsigned    init;     // resolves before RuntimePortInit finished
    unsigned    post;     // resolves after  -- this is the attendance number
};

// The 62 subject rows plus the control. Spelled as literals so
// tools/runtime_census.py and tools/export_name_audit.py can both see them.
Watch g_watch[] = {
    // ---- control (MUST read post>0) ----
    { "?_singleton@MainLoop@Engine@@1PEAV12@EA", 0, 0 },
    // ---- Video: 13 vtables ----
    { "??_7ConstantBufferData@Video@@6B@", 0, 0 },
    { "??_7ContextData@Video@@6B@", 0, 0 },
    { "??_7DeviceData@Video@@6B@", 0, 0 },
    { "??_7ImageBuffer@Video@@6B@", 0, 0 },
    { "??_7ImageBufferData@Video@@6B@", 0, 0 },
    { "??_7IndexBufferData@Video@@6B@", 0, 0 },
    { "??_7PixelProgramData@Video@@6B@", 0, 0 },
    { "??_7RenderBufferData@Video@@6B@", 0, 0 },
    { "??_7RenderStateData@Video@@6B@", 0, 0 },
    { "??_7VertexBufferData@Video@@6B@", 0, 0 },
    { "??_7VertexLayoutData@Video@@6B@", 0, 0 },
    { "??_7VertexProgramData@Video@@6B@", 0, 0 },
    { "??_7VideoProgramData@Video@@6B@", 0, 0 },
    // ---- Video: 4 statics, 2 singletons, 6 typeinfo ----
    { "?_firstBuffer@IndexBuffer@Video@@0PEAV12@EA", 0, 0 },
    { "?_firstBuffer@VertexBuffer@Video@@0PEAV12@EA", 0, 0 },
    { "?_firstRenderBuffer@RenderBuffer@Video@@0PEAV12@EA", 0, 0 },
    { "?_firstSurface@ImageBuffer@Video@@0PEAV12@EA", 0, 0 },
    { "?_singleton@Configuration@Video@@0PEAV12@EA", 0, 0 },
    { "?_singleton@Device@Video@@0PEAV12@EA", 0, 0 },
    { "?_typeInfo@ImageBuffer@Video@@0VBaseTypeInfo@Object@@A", 0, 0 },
    { "?_typeInfo@PixelProgram@Video@@0VBaseTypeInfo@Object@@A", 0, 0 },
    { "?_typeInfo@RenderState@Video@@0VBaseTypeInfo@Object@@A", 0, 0 },
    { "?_typeInfo@VertexLayout@Video@@0VBaseTypeInfo@Object@@A", 0, 0 },
    { "?_typeInfo@VertexProgram@Video@@0VBaseTypeInfo@Object@@A", 0, 0 },
    { "?_typeInfo@VideoProgram@Video@@0VBaseTypeInfo@Object@@A", 0, 0 },
    // ---- IO: 13 vtables (5 of them have NO minted row) ----
    { "??_7BinaryFileStream@IO@@6B@", 0, 0 },
    { "??_7BinaryReader@IO@@6B@", 0, 0 },
    { "??_7BinaryWriter@IO@@6B@", 0, 0 },
    { "??_7CompressedFileStream@IO@@6B@", 0, 0 },
    { "??_7CompressedMemoryStream@IO@@6B@", 0, 0 },
    { "??_7CompressedStream@IO@@6B@", 0, 0 },
    { "??_7FileStream@IO@@6B@", 0, 0 },
    { "??_7MemoryStream@IO@@6B@", 0, 0 },
    { "??_7PackageFile@IO@@6B@", 0, 0 },
    { "??_7ReaderWriter@IO@@6B@", 0, 0 },
    { "??_7Stream@IO@@6B@", 0, 0 },
    { "??_7TextFileStream@IO@@6B@", 0, 0 },
    { "??_7VisitorStream@IO@@6B@", 0, 0 },
    // ---- IO: 3 singletons, 1 typeinfo ----
    { "?_singleton@FileSystem@IO@@0PEAV12@EA", 0, 0 },
    { "?_singleton@Modification@IO@@2PEAV12@EA", 0, 0 },
    { "?_singleton@Persistence@IO@@0PEAV12@EA", 0, 0 },
    { "?_typeInfo@PackageFile@IO@@0VBaseTypeInfo@Object@@A", 0, 0 },
    // ---- Graphics: 8 vtables ----
    { "??_7AnimationEvent@Graphics@@6B@", 0, 0 },
    { "??_7Geometry@Graphics@@6B@", 0, 0 },
    { "??_7Light@Graphics@@6B@", 0, 0 },
    { "??_7MaterialInstance@Graphics@@6B@", 0, 0 },
    { "??_7Particle@Graphics@@6B@", 0, 0 },
    { "??_7ParticleInstance@Graphics@@6B@", 0, 0 },
    { "??_7ParticleSystemInstance@Graphics@@6B@", 0, 0 },
    { "??_7SunLight@Graphics@@6B@", 0, 0 },
    // ---- Graphics: 3 statics, 1 singleton, 8 typeinfo ----
    { "?_frameIndex@ParticleInstance@Graphics@@1HA", 0, 0 },
    { "?_rng@Particle@Graphics@@1VRandom@System@@A", 0, 0 },
    { "?_rootOnlyUpdates@MeshInstance@Graphics@@0_NA", 0, 0 },
    { "?_singleton@AnimationManager@Graphics@@0PEAV12@EA", 0, 0 },
    { "?_typeInfo@EdgeMesh@Graphics@@0VBaseTypeInfo@Object@@A", 0, 0 },
    { "?_typeInfo@FrameCopyImageBuffer@Graphics@@0VBaseTypeInfo@Object@@A", 0, 0 },
    { "?_typeInfo@GeneratedImageBuffer@Graphics@@0VBaseTypeInfo@Object@@A", 0, 0 },
    { "?_typeInfo@GraphicsMesh@Graphics@@0VBaseTypeInfo@Object@@A", 0, 0 },
    { "?_typeInfo@Material@Graphics@@0VBaseTypeInfo@Object@@A", 0, 0 },
    { "?_typeInfo@MaterialInstance@Graphics@@0VBaseTypeInfo@Object@@A", 0, 0 },
    { "?_typeInfo@Particle@Graphics@@0VBaseTypeInfo@Object@@A", 0, 0 },
    { "?_typeInfo@ReflectionImageBuffer@Graphics@@0VBaseTypeInfo@Object@@A", 0, 0 },
};

const int kWatchCount = (int)(sizeof(g_watch) / sizeof(g_watch[0]));

bool g_initDone = false;

bool Enabled() {
    static int on = -1;
    if (on < 0) {
        const char* e = std::getenv("BANISHED_W17_ATTEND");
        on = (e && *e && std::strcmp(e, "0") != 0 && std::strcmp(e, "false") != 0) ? 1 : 0;
    }
    return on != 0;
}

}  // namespace

void Wave17DoorAttendMarkInitDone() { g_initDone = true; }

// Called from ResolveExport on EVERY call, before the registry lookup, so a
// site that caches still records its one real resolve.
void Wave17DoorAttendNote(const char* mangled) {
    if (!mangled || !Enabled()) return;
    for (int i = 0; i < kWatchCount; ++i) {
        if (std::strcmp(g_watch[i].sym, mangled) == 0) {
            if (g_initDone) ++g_watch[i].post; else ++g_watch[i].init;
            return;
        }
    }
}

void Wave17DoorAttendReport() {
    if (!Enabled()) return;
    const unsigned ctrlPost = g_watch[0].post;
    const unsigned ctrlAny  = g_watch[0].post + g_watch[0].init;
    printf("\n[W17Attend] ==== door attendance (62 subject rows + 1 control) ====\n");
    printf("[W17Attend] post>0 means the consumer RAN. post==1 does NOT mean "
           "'ran once' -- nearly every site caches in a function-local static.\n");
    printf("[W17Attend] CONTROL %s: init=%u post=%u -> %s\n",
           g_watch[0].sym, g_watch[0].init, ctrlPost, ctrlPost ? "GREEN" : "RED");
    if (!ctrlPost) {
        printf("[W17Attend] CONTROL IS RED (any=%u). The instrument is not wired to "
               "the door on this route, so every zero below would be an artefact. "
               "TABLE SUPPRESSED.\n", ctrlAny);
        std::fflush(stdout);
        return;
    }
    int zero = 0, live = 0;
    for (int i = 1; i < kWatchCount; ++i) {
        const Watch& w = g_watch[i];
        if (w.post) ++live; else ++zero;
        printf("[W17Attend] %-8s init=%-4u post=%-6u %s\n",
               w.post ? "ATTENDED" : "never", w.init, w.post, w.sym);
    }
    printf("[W17Attend] %d of %d subject rows had their door opened AFTER init; "
           "%d never did.\n", live, kWatchCount - 1, zero);
    std::fflush(stdout);
}
