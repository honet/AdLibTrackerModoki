#include "ModuleLoader.h"

#include <filesystem>

#include "A2mLoader.h"
#include "A2pLoader.h"
#include "A2tLoader.h"
#include "LoaderCommon.h"
#include "TrackerLoaders.h"

namespace
{
    std::wstring lower_ext(const std::wstring& path)
    {
        std::wstring ext = std::filesystem::path(path).extension().wstring();
        if (!ext.empty() && ext[0] == L'.') ext.erase(0, 1);
        for (wchar_t& c : ext) {
            if (c >= L'A' && c <= L'Z') c = static_cast<wchar_t>(c - L'A' + L'a');
        }
        return ext;
    }

    // FilterStr2 (stringio.pas) で使う、0x80..0xA5 の文字の置き換え先
    const char _treat_char[] = "CueaaaaceeeiiiAAE_AooouuyOU_____aiounN";

    bool in_valid_characters(uint8_t c)   // _valid_characters
    {
        return c == 0 || (c >= 32 && c <= 127) || (c >= 176 && c <= 223) || c == 254 || c == 255;
    }

    std::string FilterStr2(std::string str, char chr1)
    {
        for (char& ch : str) {
            const uint8_t c = static_cast<uint8_t>(ch);
            if (in_valid_characters(c)) continue;
            if (c >= 0x80 && c <= 0xa5) ch = _treat_char[c - 0x80];
            else ch = chr1;
        }
        return str;
    }

    // cstr2str (adt2extn.pas)。オリジナルは For ループ中に Delete するため、連続する '~' の 2 つ目は残る。
    std::string cstr2str(std::string str)
    {
        const size_t len = str.size();
        for (size_t temp = 1; temp <= len; ++temp) {
            if (temp <= str.size() && str[temp - 1] == '~') str.erase(temp - 1, 1);
        }
        return str;
    }

    // FILE_open のロード後の処理 (load_flag <> BYTE_NULL)。曲名・作者名・楽器名の使えない文字を置き換える。
    void filter_names(FixedSongData& songdata)
    {
        songdata.songname.assign(FilterStr2(songdata.songname.str(), '_'));
        songdata.composer.assign(FilterStr2(songdata.composer.str(), '_'));
        for (int index = 1; index <= 255; ++index) {
            auto& name = at1(songdata.instr_names, index);
            name.assign(loader::pcopy(name.str(), 1, 9) +
                        FilterStr2(loader::pcopy(cstr2str(name.str()), 10, 32), '_'));
        }
    }
}

ModuleKind GetModuleKind(const std::wstring& path)
{
    const std::wstring ext = lower_ext(path);
    if (ext == L"a2m" || ext == L"a2t" || ext == L"hsc" || ext == L"rad" || ext == L"s3m") return ModuleKind::Song;
    if (ext == L"a2p") return ModuleKind::Pattern;
    return ModuleKind::Unsupported;
}

bool LoadSongModule(const std::wstring& path, Song& song, const Song::Defaults& defaults, std::wstring& error)
{
    const std::wstring ext = lower_ext(path);
    bool ok = false;
    if (ext == L"a2m") ok = LoadA2m(path, song, defaults, error);
    else if (ext == L"a2t") ok = LoadA2t(path, song, defaults, error);
    else if (ext == L"hsc") ok = LoadHsc(path, song, defaults, error);
    else if (ext == L"rad") ok = LoadRad(path, song, defaults, error);
    else if (ext == L"s3m") ok = LoadS3m(path, song, defaults, error);
    else error = L"対応していない形式です";
    if (!ok) return false;

    // load_flag = 1 の処理 (楽器名・パターン名の付け直し) と、使えない文字の置き換え
    loader::normalize_names(song.data());
    filter_names(song.data());
    song.title = std::filesystem::path(path).filename().wstring();
    return true;
}

bool LoadPatternModule(const std::wstring& path, Song& song, uint8_t pattern, std::wstring& error)
{
    if (!LoadA2p(path, song, pattern, error)) return false;
    filter_names(song.data());
    return true;
}

const wchar_t* ModuleFileFilter()
{
    return L"すべての対応形式\0*.a2m;*.a2t;*.a2p;*.hsc;*.rad;*.s3m\0"
           L"AdLib Tracker II (*.a2m;*.a2t;*.a2p)\0*.a2m;*.a2t;*.a2p\0"
           L"HSC-Tracker (*.hsc)\0*.hsc\0"
           L"Reality AdLib Tracker (*.rad)\0*.rad\0"
           L"Scream Tracker 3 (*.s3m)\0*.s3m\0"
           L"すべてのファイル (*.*)\0*.*\0";
}
