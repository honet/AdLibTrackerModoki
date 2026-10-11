#include "TrackerLoaders.h"

#include <vector>

#include "Depack.h"
#include "LoaderCommon.h"

using namespace loader;

namespace
{
#pragma pack(push, 1)
    struct Header
    {
        char id_string[18];
        uint16_t crc_16bit;
        uint16_t data_size;
    };

    // tMTK_DATA (patts は tHSC_PATTERNS)
    struct MtkData
    {
        PString<33> sname;
        PString<33> compo;
        PString<33> instn[128];
        uint8_t instt[128][12];
        uint8_t order[128];
        uint8_t patts[50 * 64 * 9 * 2];
        uint8_t dummy;
    };
#pragma pack(pop)

    static_assert(sizeof(Header) == 22);

    const char mtk_id[] = "mpu401tr\x92kk\xEEr@data";
}

bool LoadMtk(const std::wstring& path, Song& song, const Song::Defaults& defaults, std::wstring& error)
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
    if (std::memcmp(header.id_string, mtk_id, 18) != 0) return corrupted();

    const uint8_t* buf1 = file.data() + sizeof(header);
    const size_t temp = std::min(file.size() - sizeof(header), GENERIC_IO_BUFFER_SIZE);

    if (Update16(buf1, temp, 0) != header.crc_16bit) {
        error = L"CRC が一致しません";
        return false;
    }

    std::vector<uint8_t> unpacked;
    const size_t temp2 = RDC_decompress(buf1, temp, static_cast<uint16_t>(temp), unpacked);
    if (temp2 != header.data_size) return corrupted();

    // buffer2 (tMTK_DATA)。展開結果がレコードより大きい場合、オリジナルは後続のメモリへはみ出す
    auto buffer2 = std::make_unique<MtkData>();
    std::memset(buffer2.get(), 0, sizeof(MtkData));
    copyTo(buffer2.get(), sizeof(MtkData), unpacked);

    song.init(defaults);
    FixedSongData& songdata = song.data();

    songdata.patt_len = 64;
    songdata.nm_tracks = 9;   // adjust_tracks = TRUE
    songdata.common_flag = songdata.common_flag | 2;
    songdata.tempo = 18;
    songdata.speed = 2;
    import_old_flags(songdata);

    for (int i = 0; i <= 0x31; ++i) {
        songdata.pattern_order[i] = buffer2->order[i] != 0xff ? buffer2->order[i] : 0x80;
    }

    const int patterns = (header.data_size - static_cast<int>(sizeof(buffer2->sname)) - static_cast<int>(sizeof(buffer2->compo)) -
                          static_cast<int>(sizeof(buffer2->instn)) - static_cast<int>(sizeof(buffer2->instt)) -
                          static_cast<int>(sizeof(buffer2->order)) - 1) / 0x480;
    import_hsc_patterns(song, buffer2->patts, static_cast<uint8_t>(patterns), false);

    // specific corrections for MPU-401 TRAKKER instrument
    for (int i = 0; i <= 0x7f; ++i) {
        import_hsc_instrument(songdata, static_cast<uint8_t>(i + 1), buffer2->instt[i]);
        FmInstData& fm = at1(songdata.instr_data, i + 1).fm_data;
        if (fm.KSL_VOLUM_modulator > 128) fm.KSL_VOLUM_modulator = fm.KSL_VOLUM_modulator / 3;
        if (fm.KSL_VOLUM_carrier > 128) fm.KSL_VOLUM_carrier = fm.KSL_VOLUM_carrier / 3;

        auto& name = at1(songdata.instr_names, i + 1);
        name.assign(pcopy(name.str(), 1, 9) + truncate_string(pcopy(buffer2->instn[i].str(), 10, 32)));
    }

    songdata.songname.assign(CutStr(buffer2->sname.str()));
    songdata.composer.assign(CutStr(buffer2->compo.str()));
    return true;
}
