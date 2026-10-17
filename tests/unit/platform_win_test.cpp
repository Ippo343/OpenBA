// ============================================================================
// tests/unit/platform_win_test.cpp -- the src/platform/win file layer (file_win.cpp)
// ============================================================================
// The Windows twin of platform_posix_test.cpp: compiled everywhere, EMPTY off Windows.
// Over a fresh directory under the OS temp dir (never the repo, never the game dir):
//   * FindFirst / FindNext / FindClose: every file matching the pattern is listed exactly
//     once, in NTFS's upper-case collation order (pinned when the temp volume is NTFS, the
//     order the POSIX twin reproduces), the pattern matches case-insensitively, "*" lists
//     "." and ".." first as directories, no match is kInvalidFindHandle;
//   * FileMove refuses an existing target (0, both files untouched) and moves otherwise;
//     FileReplace (MoveFileExW REPLACE_EXISTING) overwrites it;
//   * FileCreateTemp creates a writable file under the temp dir whose name carries the
//     prefix, and the handle / FileOpenRead / FileSeek32 / FileRead / FileSize /
//     FileLastWrite round trip, then FileDelete removes it;
//   * a directory and a file name with spaces work through every primitive;
//   * PathAttributes: a directory, a file, a missing path.
// Everything the tests create is removed again, whatever the checks say.
// ============================================================================
#if defined(_WIN32)

#include "unit.h"

#include "platform_file.h"
#include "platform_thread.h"
#include "platform_time.h"   // FileTimeFromUnix

#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

using Platform::PathChar;

std::u16string U16(const std::string& s) {
    std::u16string out;
    for (unsigned char c : s) out.push_back(static_cast<char16_t>(c));   // ASCII names only
    return out;
}
std::string U8(const PathChar* w) {
    std::string out;
    for (; *w != 0; ++w) out.push_back(static_cast<char>(*w));
    return out;
}
const wchar_t* W(const std::u16string& s) { return reinterpret_cast<const wchar_t*>(s.c_str()); }  // CAST-OK: win32

// A fresh directory under GetTempPath; the destructor deletes every file it holds, the
// files in its immediate subdirectories, those subdirectories, and the directory itself
// (one level of recursion: as deep as any test here creates), so a CHECK that fails
// before a test's own cleanup leaves nothing behind.
struct ScratchDir {
    std::string path;
    explicit ScratchDir(const char* tag) {
        wchar_t tmp[MAX_PATH];
        const DWORD n = GetTempPathW(MAX_PATH, tmp);
        if (n == 0 || n >= MAX_PATH) return;
        std::string base;
        for (DWORD i = 0; i < n; ++i) base.push_back(static_cast<char>(tmp[i]));
        if (!base.empty() && base.back() == '\\') base.pop_back();
        char name[128];
        std::snprintf(name, sizeof(name), "banished_win_unit_%s_%lu_%lu", tag,
                      static_cast<unsigned long>(GetCurrentProcessId()),
                      static_cast<unsigned long>(GetTickCount()));
        path = base + "\\" + name;
        if (!CreateDirectoryW(W(U16(path)), nullptr)) path.clear();
    }
    ~ScratchDir() {
        if (path.empty()) return;
        ClearDir(U16(path), true);
        RemoveDirectoryW(W(U16(path)));
    }
    // Deletes every file directly under `dir`; with `subdirs`, also clears (one level)
    // and removes each immediate subdirectory.
    static void ClearDir(const std::u16string& dir, bool subdirs) {
        WIN32_FIND_DATAW fd;
        HANDLE h = FindFirstFileW(W(dir + u"\\*"), &fd);
        if (h == INVALID_HANDLE_VALUE) return;
        do {
            std::u16string f = dir + u"\\";
            for (const wchar_t* p = fd.cFileName; *p; ++p) f.push_back(static_cast<char16_t>(*p));
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                if (!subdirs) continue;
                if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
                ClearDir(f, false);
                RemoveDirectoryW(W(f));
            } else {
                DeleteFileW(W(f));
            }
        } while (FindNextFileW(h, &fd));
        ::FindClose(h);
    }
    std::string File(const char* name) const { return path + "\\" + name; }
};

bool Touch(const std::string& path, const char* text) {
    Platform::FileHandle h = Platform::FileCreateWrite(U16(path).c_str());
    if (h == Platform::kInvalidFileHandle) return false;
    uint32_t written = 0;
    Platform::FileWrite(h, text, static_cast<uint32_t>(std::strlen(text)), &written);
    Platform::FileClose(h);
    return written == std::strlen(text);
}

std::string Slurp(const std::string& path) {
    std::string out;
    Platform::FileHandle h = Platform::FileOpenRead(U16(path).c_str());
    if (h == Platform::kInvalidFileHandle) return out;
    char buf[256];
    uint32_t got = 0;
    while (Platform::FileRead(h, buf, sizeof(buf), &got) && got > 0) out.append(buf, got);
    Platform::FileClose(h);
    return out;
}

bool Exists(const std::string& path) {
    return Platform::PathAttributes(U16(path).c_str()) != Platform::kInvalidPathAttributes;
}

bool VolumeIsNtfs(const std::string& path) {
    const std::u16string root = U16(path.substr(0, 3));   // "C:\"
    wchar_t fs[32] = {};
    if (!GetVolumeInformationW(W(root), nullptr, 0, nullptr, nullptr, nullptr, fs, 32)) return false;
    return std::wcscmp(fs, L"NTFS") == 0;
}

std::vector<std::string> List(const std::string& pattern, bool* sawDirectory = nullptr) {
    std::vector<std::string> names;
    Platform::FindData fd;
    Platform::FindHandle h = Platform::FindFirst(U16(pattern).c_str(), &fd);
    if (h == Platform::kInvalidFindHandle) return names;
    do {
        names.push_back(U8(fd.name));
        if (sawDirectory && (fd.attributes & Platform::kFileAttributeDirectory)) *sawDirectory = true;
    } while (Platform::FindNext(h, &fd));
    CHECK_EQ(Platform::FindClose(h), 1);
    return names;
}

}  // namespace

UNIT_TEST(win_find_first_lists_every_match_once_in_ntfs_order) {
    ScratchDir d("find");
    if (d.path.empty()) UNIT_SKIP("no writable temp directory");
    for (const char* n : {"b.PKG", "_x.pkg", "Z.pkg", "c.txt", "A.pkg", "a2.pkm"}) CHECK(Touch(d.File(n), "x"));

    bool dirSeen = false;
    std::vector<std::string> names = List(d.File("*.pkg"), &dirSeen);
    CHECK(!dirSeen);
    // Completeness: the four .pkg files (b.PKG by the case-insensitive pattern), each once.
    std::vector<std::string> sorted = names;
    std::sort(sorted.begin(), sorted.end());
    const std::vector<std::string> want = {"A.pkg", "Z.pkg", "_x.pkg", "b.PKG"};
    CHECK(sorted == want);
    // Order: NTFS returns the upper-case collation (A, B, Z, _): pinned on NTFS, noted elsewhere.
    const std::vector<std::string> ntfsOrder = {"A.pkg", "b.PKG", "Z.pkg", "_x.pkg"};
    if (VolumeIsNtfs(d.path)) CHECK(names == ntfsOrder);
    else std::printf("  note: %s is not on NTFS; the enumeration order is not pinned\n", d.path.c_str());

    // "*" lists "." and ".." first, as directories, then the six files.
    Platform::FindData fd;
    Platform::FindHandle h = Platform::FindFirst(U16(d.File("*")).c_str(), &fd);
    CHECK(h != Platform::kInvalidFindHandle);
    if (h != Platform::kInvalidFindHandle) {
        CHECK(U8(fd.name) == ".");
        CHECK((fd.attributes & Platform::kFileAttributeDirectory) != 0);
        CHECK_EQ(Platform::FindNext(h, &fd), 1);
        CHECK(U8(fd.name) == "..");
        CHECK((fd.attributes & Platform::kFileAttributeDirectory) != 0);
        int files = 0;
        while (Platform::FindNext(h, &fd)) ++files;
        CHECK_EQ(files, 6);
        CHECK_EQ(Platform::FindClose(h), 1);
    }
    // No match: the invalid handle (ERROR_FILE_NOT_FOUND).
    CHECK(Platform::FindFirst(U16(d.File("*.sav")).c_str(), &fd) == Platform::kInvalidFindHandle);
    // A pattern that names one file exactly lists just it.
    CHECK(List(d.File("c.txt")) == std::vector<std::string>{"c.txt"});
}

UNIT_TEST(win_file_move_refuses_an_existing_target_and_replace_overwrites) {
    ScratchDir d("move");
    if (d.path.empty()) UNIT_SKIP("no writable temp directory");
    CHECK(Touch(d.File("src.bin"), "source"));
    CHECK(Touch(d.File("dst.bin"), "already here"));
    const std::u16string from = U16(d.File("src.bin")), to = U16(d.File("dst.bin"));
    CHECK_EQ(Platform::FileMove(from.c_str(), to.c_str()), 0);        // MoveFileW: ERROR_ALREADY_EXISTS
    CHECK(Slurp(d.File("dst.bin")) == "already here");
    CHECK(Slurp(d.File("src.bin")) == "source");

    // Onto a free name: moved, the source gone.
    const std::u16string moved = U16(d.File("moved.bin"));
    CHECK_EQ(Platform::FileMove(from.c_str(), moved.c_str()) != 0, true);
    CHECK(!Exists(d.File("src.bin")));
    CHECK(Slurp(d.File("moved.bin")) == "source");

    // FileReplace (the save writer's final step) overwrites the existing target.
    CHECK_EQ(Platform::FileReplace(moved.c_str(), to.c_str()) != 0, true);
    CHECK(!Exists(d.File("moved.bin")));
    CHECK(Slurp(d.File("dst.bin")) == "source");

    // Moving a missing source fails and creates nothing.
    CHECK_EQ(Platform::FileMove(U16(d.File("absent.bin")).c_str(), U16(d.File("new.bin")).c_str()), 0);
    CHECK(!Exists(d.File("new.bin")));
    CHECK_EQ(Platform::FileDelete(to.c_str()) != 0, true);
    CHECK(!Exists(d.File("dst.bin")));
}

UNIT_TEST(win_temp_file_round_trip_and_cleanup) {
    PathChar tempName[Platform::kMaxPath];
    const std::u16string prefix = u"bns";
    Platform::FileHandle h = Platform::FileCreateTemp(prefix.c_str(), tempName);
    CHECK(h != Platform::kInvalidFileHandle && h != nullptr);
    if (h == Platform::kInvalidFileHandle || h == nullptr) UNIT_SKIP("no temp file could be created");
    const std::string name = U8(tempName);
    CHECK(name.find("\\bns") != std::string::npos);          // GetTempFileNameW: prefix + hex + .tmp
    CHECK(name.size() > 4 && name.compare(name.size() - 4, 4, ".tmp") == 0);
    CHECK(Exists(name));

    uint32_t written = 0, got = 0;
    CHECK_EQ(Platform::FileWrite(h, "0123456789", 10, &written), 1);
    CHECK_EQ(written, 10u);
    CHECK_EQ(Platform::FileSize(h), 10u);
    uint64_t ft = 0;
    CHECK_EQ(Platform::FileLastWrite(h, &ft), 1);
    CHECK(ft > Platform::FileTimeFromUnix(1577836800, 0));   // after 2020-01-01
    CHECK_EQ(Platform::FileClose(h), 1);

    h = Platform::FileOpenRead(tempName);
    CHECK(h != Platform::kInvalidFileHandle);
    if (h != Platform::kInvalidFileHandle) {
        CHECK_EQ(Platform::FileSeek32(h, 4), 4u);
        char buf[16] = {};
        CHECK_EQ(Platform::FileRead(h, buf, sizeof(buf), &got), 1);   // a short read at EOF succeeds
        CHECK_EQ(got, 6u);
        CHECK(std::memcmp(buf, "456789", 6) == 0);
        CHECK_EQ(Platform::FileRead(h, buf, sizeof(buf), &got), 1);   // at EOF: ok, 0 bytes
        CHECK_EQ(got, 0u);
        CHECK_EQ(Platform::FileClose(h), 1);
    }
    CHECK_EQ(Platform::FileDelete(tempName) != 0, true);
    CHECK(!Exists(name));
    CHECK(Platform::FileOpenRead(tempName) == Platform::kInvalidFileHandle);

    // The local date / time formatter over that stamp: "HH:MM" has ':' at [2].
    PathChar text[Platform::kDateTimeChars];
    const uint32_t when[2] = {static_cast<uint32_t>(ft), static_cast<uint32_t>(ft >> 32)};
    Platform::FormatLocalDateTime(when, true, text);
    CHECK(text[0] != 0 && text[2] == u':');
    Platform::FormatLocalDateTime(when, false, text);
    CHECK(text[0] != 0);
}

UNIT_TEST(win_paths_with_spaces_and_path_attributes) {
    ScratchDir d("space");
    if (d.path.empty()) UNIT_SKIP("no writable temp directory");
    const std::string sub = d.File("with space");
    CHECK_EQ(Platform::DirCreate(U16(sub).c_str()) != 0, true);
    CHECK_EQ(Platform::DirCreate(U16(sub).c_str()), 0);                  // already there
    CHECK_EQ(Platform::PathAttributes(U16(sub).c_str()) & Platform::kFileAttributeDirectory,
             Platform::kFileAttributeDirectory);

    const std::string file = sub + "\\my save 1.sav";
    CHECK(Touch(file, "spaced"));
    CHECK(Slurp(file) == "spaced");
    CHECK_EQ(Platform::PathAttributes(U16(file).c_str()) & Platform::kFileAttributeDirectory, 0u);
    CHECK(List(sub + "\\*.sav") == std::vector<std::string>{"my save 1.sav"});
    CHECK(Platform::PathAttributes(U16(sub + "\\absent file.sav").c_str()) == Platform::kInvalidPathAttributes);

    // Move within the spaced directory, then delete; remove the directory ourselves too
    // (the scratch destructor would, but the explicit RemoveDirectoryW is the check).
    const std::string file2 = sub + "\\my save 2.sav";
    CHECK_EQ(Platform::FileMove(U16(file).c_str(), U16(file2).c_str()) != 0, true);
    CHECK(!Exists(file) && Slurp(file2) == "spaced");
    CHECK_EQ(Platform::FileDelete(U16(file2).c_str()) != 0, true);
    CHECK(RemoveDirectoryW(W(U16(sub))) != 0);
    CHECK(!Exists(sub));
}

#endif  // _WIN32
