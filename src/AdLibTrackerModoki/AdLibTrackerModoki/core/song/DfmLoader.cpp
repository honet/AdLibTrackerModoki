#include "TrackerLoaders.h"

#include <set>
#include <vector>

#include "EffectDefs.h"
#include "LoaderCommon.h"

using namespace loader;

namespace
{
#pragma pack(push, 1)
    struct Header
    {
        char ident[4];             // 'DFM'#26
        uint16_t versn;
        PString<32> sname;
        uint8_t tempo;
        PString<11> instn[32];     // [1..32]
        FmInstData instd[32];      // [1..32]
        uint8_t order[128];        // [1..128]
        uint8_t patts;
    };
#pragma pack(pop)

    static_assert(sizeof(Header) == 905);

    void import_dfm_event(Song& song, uint8_t patt, uint8_t line, uint8_t chan, uint8_t byte1, uint8_t byte2)
    {
        Chunk chunk{};
        const uint8_t n = byte1 & 0x0f;
        if ((n >= 1 && n <= 12) || n == 15) {
            if (n != 15) chunk.note = static_cast<uint8_t>((n - 1) + ((byte1 >> 4) & 7) * 12 + 1);
            else chunk.note = BYTE_NULL;
        }

        switch (byte2 >> 5) {
        case 1:   // INSTRUMENT CHANGE
            chunk.instr_def = static_cast<uint8_t>((byte2 & 0x1f) + 1);
            break;
        case 2:   // SET INSTRUMENT VOLUME
            chunk.effect_def = ef_SetInsVolume;
            chunk.effect = static_cast<uint8_t>((byte2 & 0x1f) * 2);
            break;
        case 3:   // TEMPO CHANGE
            chunk.effect_def = ef_SetSpeed;
            chunk.effect = static_cast<uint8_t>((byte2 & 0x1f) + 1);
            break;
        case 4:   // SLIDE UP
            chunk.effect_def = ef_FSlideUpFine;
            chunk.effect = byte2 & 0x1f;
            break;
        case 5:   // SLIDE DOWN
            chunk.effect_def = ef_FSlideDownFine;
            chunk.effect = byte2 & 0x1f;
            break;
        case 7:   // END OF PATTERN
            chunk.effect_def = ef_PatternBreak;
            break;
        }
        song.putChunk(patt, line, chan, chunk);
    }

    void process_dfm_patterns(Song& song, uint8_t patterns)
    {
        FixedSongData& songdata = song.data();
        std::set<uint8_t> patts;
        uint8_t instr_cache[19] = {};
        int order = 0;
        uint8_t patt = BYTE_NULL;
        do {
            if (songdata.pattern_order[order] >= 0x80) {
                ++order;
            } else {
                patt = songdata.pattern_order[order];
                for (int temp2 = 0; temp2 <= 0x3f; ++temp2) {
                    for (int temp3 = 1; temp3 <= 9; ++temp3) {
                        Chunk chunk{};
                        song.getChunk(patt, static_cast<uint8_t>(temp2), static_cast<uint8_t>(temp3), chunk);
                        const bool is_note = chunk.note >= 1 && chunk.note <= 12 * 8 + 1;
                        if (chunk.instr_def != 0) {
                            chunk.effect_def = ef_Extended;
                            chunk.effect = ef_ex_ExtendedCmd2 * 16 + ef_ex_cmd2_ResetVol;
                            instr_cache[temp3] = chunk.instr_def;
                            if (!is_note && !accurate_conv) chunk.instr_def = 0;
                        } else if (is_note && chunk.instr_def == 0 && !accurate_conv) {
                            chunk.instr_def = instr_cache[temp3];
                        }

                        if (patts.count(songdata.pattern_order[order]) == 0) {
                            song.putChunk(patt, static_cast<uint8_t>(temp2), static_cast<uint8_t>(temp3), chunk);
                        }
                    }
                }
                ++order;
                patts.insert(patt);
            }
        } while (!(patt >= patterns || order > 0x7f));
    }
}

bool LoadDfm(const std::wstring& path, Song& song, const Song::Defaults& defaults, std::wstring& error)
{
    std::vector<uint8_t> file;
    if (!readFile(path, file)) {
        error = L"ファイルを開けません";
        return false;
    }

    const auto corrupted = [&error] {
        error = L"ファイルが壊れています";
        return false;
    };

    Header header{};
    if (file.size() < sizeof(header)) return corrupted();
    std::memcpy(&header, file.data(), sizeof(header));
    if (std::memcmp(header.ident, "DFM\x1A", 4) != 0) return corrupted();

    std::vector<uint8_t> buf1(file.begin() + sizeof(header), file.end());
    const size_t temp = buf1.size();
    buf1.resize(temp + 4, 0);

    song.init(defaults);
    FixedSongData& songdata = song.data();

    songdata.patt_len = 64;
    songdata.nm_tracks = 9;   // adjust_tracks = TRUE

    songdata.songname.assign(CutStr(header.sname.str()));
    songdata.tempo = 135;
    songdata.speed = static_cast<uint8_t>(header.tempo + 1);
    songdata.common_flag = songdata.common_flag | 1 | 2 | 8 | 0x10;
    import_old_flags(songdata);

    for (int temp2 = 1; temp2 <= 128; ++temp2) {
        const uint8_t order = header.order[temp2 - 1];
        if (order <= 0x7f) songdata.pattern_order[temp2 - 1] = order;
        else if (order == 0x80) break;
        else songdata.pattern_order[temp2 - 1] = static_cast<uint8_t>(0x80 + temp2);
    }

    for (int temp2 = 1; temp2 <= 32; ++temp2) {
        auto& name = at1(songdata.instr_names, temp2);
        std::string str = pcopy(name.str(), 1, 9) + CutStr(header.instn[temp2 - 1].str());
        while (!str.empty() && static_cast<uint8_t>(str.back()) < 32) str.pop_back();
        name.assign(str);
        import_standard_instrument(songdata, static_cast<uint8_t>(temp2), reinterpret_cast<const uint8_t*>(&header.instd[temp2 - 1]));
    }

    size_t temp2 = 0;
    int temp3 = 0;
    do {
        const uint8_t pattern = buf1[temp2];
        if (pattern > 127) return corrupted();
        ++temp2;
        ++temp3;

        for (int line = 0; line <= 0x3f; ++line) {
            for (int channel = 1; channel <= 9; ++channel) {
                const uint8_t byte1 = buf1[temp2];
                if (temp2 >= temp) return corrupted();
                ++temp2;

                uint8_t byte2;
                if ((byte1 | 0x80) != byte1) {
                    byte2 = 0;
                } else {
                    byte2 = buf1[temp2];
                    ++temp2;
                }
                import_dfm_event(song, pattern, static_cast<uint8_t>(line), static_cast<uint8_t>(channel), byte1, byte2);
            }
        }
    } while (!(temp2 >= temp));

    process_dfm_patterns(song, static_cast<uint8_t>(temp3));
    return true;
}
