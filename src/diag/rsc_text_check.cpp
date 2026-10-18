// ============================================================================
// src/diag/rsc_text_check.cpp -- the KITFREE cross-check (rsc_text_check.h)
// ============================================================================
// NOT RETAIL. The write mirrors External::Load's own stream (external_load.cpp
// steps 3-8 in the writing direction): an IO::Serializer over a MemoryStream
// emits the 12-byte record header, then the magic 0x5412a91f, the type id, the
// file version, the dependency count (0: a provided object carries no
// compile-time dependency table) and the object's own Serialize; ~Serializer
// writes the trailer and back-patches the length. The kit's .crs holds the
// same stream LZF-compressed, with its dependency entries, which the compare
// script skips.
// ============================================================================

#include "rsc_text_check.h"

#include <cstdint>
#include <cstdio>
#include <string>

#include "ext/resources/Resources.h"
#include "ext/resources/text_provider.h"
#include "io/FileSystem.h"
#include "io/Serializer.h"
#include "io/Stream.h"
#include "object/External.h"
#include "system/Configuration.h"
#include "system/Filename.h"
#include "system/String.h"

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

namespace Diag {

namespace {

constexpr int32_t kCompiledMagic = 0x5412a91f;

void MakeDir(const char* dir) {
#ifdef _WIN32
    _mkdir(dir);
#else
    mkdir(dir, 0755);
#endif
}

bool WriteObject(Object::External* obj, const std::string& file) {
    constexpr int kCapacity = 1 << 20;
    IO::MemoryStream ms;
    if (!ms.Create(kCapacity) || ms._data == nullptr) return false;
    {
        IO::Serializer ser(ms, 0);
        IO::ReaderWriter* const rw = ser._readerWriter;
        int32_t magic = kCompiledMagic;
        rw->SerializeInt(magic, nullptr);
        uint32_t typeId = obj->Type();
        rw->SerializeUInt(typeId, nullptr);
        int32_t version = obj->GetFileVersion();
        rw->SerializeInt(version, nullptr);
        int32_t deps = 0;
        rw->SerializeInt(deps, nullptr);
        obj->Serialize(ms);
    }
    if (ms._cursor >= static_cast<uint32_t>(kCapacity - 16)) return false;
    FILE* f = std::fopen(file.c_str(), "wb");
    if (f == nullptr) return false;
    const bool ok = std::fwrite(ms._data, 1, ms._cursor, f) == ms._cursor;
    std::fclose(f);
    return ok;
}

}  // namespace

// What the factory can give us right now: the bases our text derives from and
// the game resources it references, each as "in a package?" and "loads?".
void Probe() {
    static const char* const kPaths[] = {
        "Dialog/Mod.rsc:labelTitle", "Dialog/Mod.rsc:textHeader", "Dialog/Mod.rsc:headerLine",
        "Dialog/MessageBox.rsc", "Dialog/MessageBox.rsc:main", "Dialog/MessageBox.rsc:text",
        "Dialog/StringTable.rsc:general", "Dialog/ToolTip.rsc", "Font/FontMedium.rsc",
        "Dialog/SharedElements.rsc:dialogBorder", "Dialog/MainMenu.rsc:buttonMod",
    };
    IO::FileSystem* const fs = IO::FileSystem::_singleton;
    Object::ExternalFactory* const f = Object::ExternalFactory::_singleton;
    for (const char* p : kPaths) {
        std::string named = p;
        if (named.find(':') == std::string::npos) named += ":resource";
        const System::Filename compiled = Object::ExternalFactory::GetCompiledPath(System::Filename(named.c_str()));
        const bool inPack = fs != nullptr && compiled._data != nullptr && fs->ExistsInPackfile(compiled);
        const Object::External* o = f != nullptr ? f->FindOrLoad(System::Filename(p)) : nullptr;
        std::fprintf(stderr, "[RscCheck] probe %-42s packed=%d loaded=%s\n", p, inPack ? 1 : 0,
                     o ? o->TypeName() : "NO");
    }
}

void RscTextCheck(const char* outDir) {
    MakeDir(outDir);
    Probe();
    const int n = Ext::Resources::TextObjectCount();
    int written = 0, failed = 0;
    for (int i = 0; i < n; ++i) {
        std::string path;
        if (!Ext::Resources::TextObjectAt(i, path)) break;
        Object::External* obj = Ext::Resources::Provide(path.c_str());
        if (obj == nullptr) {
            std::fprintf(stderr, "[RscCheck] %s: not provided\n", path.c_str());
            ++failed;
            continue;
        }
        // "Dialog/Extensions.rsc:dialogRoot" -> "Extensions_dialogRoot.bin", the kit's naming.
        std::string stem = path.substr(path.rfind('/') + 1);
        const size_t dot = stem.find(".rsc:");
        stem = stem.substr(0, dot) + "_" + stem.substr(dot + 5);
        const std::string file = std::string(outDir) + "/" + stem + ".bin";
        if (WriteObject(obj, file)) ++written;
        else { std::fprintf(stderr, "[RscCheck] %s: write failed\n", path.c_str()); ++failed; }
        delete obj;   // built outside the factory: ours to free
    }
    std::fprintf(stderr, "[RscCheck] %d object(s) written to %s, %d failed\n", written, outDir, failed);
}

void RscTextCheckIfArmed(const char* outDir) {
    const System::Configuration* cfg = System::Configuration::_singleton;
    if (cfg == nullptr) return;
    const System::String chk = cfg->GetCommandLineString(System::String("rsccheck"));
    if (chk._data == nullptr || chk._data[0] == '\0') return;
    static std::string done;   // "|dir|dir|": each outDir once
    const std::string key = std::string("|") + outDir + "|";
    if (done.find(key) != std::string::npos) return;
    done += key;
    RscTextCheck(outDir);
}

}  // namespace Diag
