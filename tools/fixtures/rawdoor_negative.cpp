// FIXTURE -- rawdoor_audit.py --self-test NEGATIVE CONTROL.  Not compiled, not
// in ROOTS, never scanned by an ordinary audit run.
//
// Nothing here may be reported.  A checker with only a positive control can be
// made to pass by flagging everything; these are the shapes that are CORRECT
// and whose verdicts this wave paid for.

using ReleaseFn = void (*)(void*);
using IsPausedFn = bool (*)(const void*, int);

extern bool rt_diff_on__;

void fixture_guarded_call(const void* self, int t, bool ours) {
    // The oracle, correctly fenced.  Resolve AND call inside the guard.
    if (rt_diff_on__) {
        bool theirs = RT_ORIGINAL_ONCE(IsPausedFn, "?IsPaused@Scene@Audio@@QEBA_NH@Z")(self, t);
        RT_DIFF_SHADOW(theirs == ours, "IsPaused");
    }
}

void fixture_resolve_only(const void* self, int t, bool ours) {
    // ?IsEffectPlaying's shape: the pointer is STORED outside the guard and
    // every call through it is inside one.  Resolve-outside-guard is harmless
    // (ResolveExportRaw returns null safely with no module handle); this is the
    // distinction lane V drew and the hardened matcher must keep drawing it.
    IsPausedFn playing = RtOriginal<IsPausedFn>("?IsEffectPlaying@Scene@Audio@@QEBA_NH@Z");
    if (rt_diff_on__) {
        bool theirs = playing(self, t);
        RT_DIFF_SHADOW(theirs == ours, "IsEffectPlaying");
    }
}

bool fixture_compared_not_called(const void* self) {
    // Compared against, never invoked -- `==` must not read as an assignment.
    IsPausedFn probe = RtOriginal<IsPausedFn>("?IsPaused@Scene@Audio@@QEBA_NH@Z");
    return probe == nullptr;
}
