#include "A2mLoader.h"

#include <vector>

#include "Depack.h"
#include "LoaderCommon.h"

using namespace loader;

namespace
{
    constexpr uint8_t FFVER_A2M = 14;
    constexpr size_t OLD_A2M_HEADER_SIZE = 26;

#pragma pack(push, 1)
    struct OldHeader   // tOLD_HEADER
    {
        char ident[10];
        uint32_t crc32;
        uint8_t ffver;
        uint8_t patts;
        uint16_t b0len, b1len, b2len, b3len, b4len, b5len, b6len, b7len, b8len;
    };

    struct Header   // tHEADER
    {
        char ident[10];
        uint32_t crc32;
        uint8_t ffver;
        uint8_t patts;
        uint32_t b0len;
        uint32_t b1len[16];
    };
#pragma pack(pop)

    static_assert(sizeof(OldHeader) == 34);
    static_assert(sizeof(Header) == 84);

    const char A2M_ID[] = "_A2module_";
}

bool LoadA2m(const std::wstring& path, Song& song, const Song::Defaults& defaults, std::wstring& error)
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

    Reader reader(file);
    Header header{};
    if (reader.read(&header, sizeof(header)) != sizeof(header) ||
        std::memcmp(header.ident, A2M_ID, 10) != 0) {
        return corrupted();
    }
    if (header.ffver < 1 || header.ffver > FFVER_A2M) {
        error = L"未対応の形式バージョンです (" + std::to_wstring(header.ffver) + L")";
        return false;
    }

    // 旧形式の展開先 (オリジナルのグローバル変数 old_songdata / old_hash_buffer / hash_buffer)
    struct OldBuffers
    {
        OldFixedSongData old_songdata;
        OldVariableData1 old_hash_buffer;
        OldVariableData2 hash_buffer;
    };
    auto old = std::make_unique<OldBuffers>();
    std::memset(old.get(), 0, sizeof(OldBuffers));
    OldFixedSongData* old_songdata = &old->old_songdata;
    OldVariableData1* old_hash_buffer = &old->old_hash_buffer;
    OldVariableData2* hash_buffer = &old->hash_buffer;
    init_old_songdata(*old_songdata);

    FixedSongData& songdata = song.data();
    std::vector<uint8_t> buf1;
    std::vector<uint8_t> unpacked;

    if (header.ffver >= 1 && header.ffver <= 4) {
        OldImportContext ctx{ song };
        reader.seek(0);
        OldHeader header2{};
        if (reader.read(&header2, sizeof(header2)) != sizeof(header2) ||
            std::memcmp(header2.ident, A2M_ID, 10) != 0) {
            return corrupted();
        }

        const uint16_t xlen[3] = { header2.b2len, header2.b3len, header2.b4len };
        const int blocks = (header2.patts - 1) / 16;

        if (!reader.seek(OLD_A2M_HEADER_SIZE)) return corrupted();
        uint32_t crc = 0xffffffff;
        if (!reader.readBlock(buf1, header2.b0len)) return corrupted();
        crc = Update32(buf1.data(), buf1.size(), crc);
        if (!reader.readBlock(buf1, header2.b1len)) return corrupted();
        crc = Update32(buf1.data(), buf1.size(), crc);
        for (int temp2 = 0; temp2 <= 2; ++temp2) {
            if (blocks > temp2) {
                if (!reader.readBlock(buf1, xlen[temp2])) return corrupted();
                crc = Update32(buf1.data(), buf1.size(), crc);
            }
        }
        crc = Update32(&header2.b0len, 2, crc);
        crc = Update32(&header2.b1len, 2, crc);
        for (int temp2 = 0; temp2 <= 2; ++temp2) {
            crc = Update32(&xlen[temp2], 2, crc);
        }
        if (crc != header2.crc32) {
            error = L"CRC が一致しません";
            return false;
        }

        song.init(defaults);
        songdata.patt_len = 64;
        songdata.nm_tracks = 9;   // adjust_tracks = TRUE

        const uint8_t method = static_cast<uint8_t>(4 - header2.ffver);   // 4:Move 3:LZSS 2:LZW 1:SixPack

        reader.seek(OLD_A2M_HEADER_SIZE);
        if (!reader.readBlock(buf1, header2.b0len)) return corrupted();
        decompressOld(method, buf1, header2.b0len, unpacked);
        copyTo(old_songdata, sizeof(OldFixedSongData), unpacked);

        for (int temp = 1; temp <= 250; ++temp) {
            at1(old_songdata->instr_data, temp).panning = 0;
        }

        if (!reader.readBlock(buf1, header2.b1len)) return corrupted();
        decompressOld(method, buf1, header2.b1len, unpacked);
        copyTo(old_hash_buffer, sizeof(OldVariableData1), unpacked);
        import_old_a2m_patterns1(ctx, *old_hash_buffer, 0, 16);

        for (int temp2 = 0; temp2 <= 2; ++temp2) {
            if (blocks > temp2) {
                if (!reader.readBlock(buf1, xlen[temp2])) return corrupted();
                decompressOld(method, buf1, xlen[temp2], unpacked);
                copyTo(old_hash_buffer, sizeof(OldVariableData1), unpacked);
                import_old_a2m_patterns1(ctx, *old_hash_buffer, temp2 + 1, 16);
            }
        }

        replace_old_adsr(ctx, header2.patts);
        import_old_songdata(songdata, *old_songdata);
    }

    if (header.ffver >= 5 && header.ffver <= 8) {
        reader.seek(0);
        OldHeader header2{};
        if (reader.read(&header2, sizeof(header2)) != sizeof(header2) ||
            std::memcmp(header2.ident, A2M_ID, 10) != 0) {
            return corrupted();
        }

        const uint16_t xlen[7] = { header2.b2len, header2.b3len, header2.b4len, header2.b5len,
                                   header2.b6len, header2.b7len, header2.b8len };
        const int blocks = (header2.patts - 1) / 8;

        uint32_t crc = 0xffffffff;
        if (!reader.readBlock(buf1, header2.b0len)) return corrupted();
        crc = Update32(buf1.data(), buf1.size(), crc);
        if (!reader.readBlock(buf1, header2.b1len)) return corrupted();
        crc = Update32(buf1.data(), buf1.size(), crc);
        for (int temp2 = 0; temp2 <= 6; ++temp2) {
            if (blocks > temp2) {
                if (!reader.readBlock(buf1, xlen[temp2])) return corrupted();
                crc = Update32(buf1.data(), buf1.size(), crc);
            }
        }
        crc = Update32(&header2.b0len, 2, crc);
        crc = Update32(&header2.b1len, 2, crc);
        for (int temp2 = 0; temp2 <= 6; ++temp2) {
            crc = Update32(&xlen[temp2], 2, crc);
        }
        if (crc != header2.crc32) {
            error = L"CRC が一致しません";
            return false;
        }

        song.init(defaults);
        songdata.patt_len = 64;
        songdata.nm_tracks = 18;   // adjust_tracks = TRUE

        const uint8_t method = static_cast<uint8_t>(8 - header2.ffver);   // 8:Move 7:LZSS 6:LZW 5:SixPack

        if (!reader.seek(sizeof(OldHeader))) return corrupted();
        if (!reader.readBlock(buf1, header2.b0len)) return corrupted();
        decompressOld(method, buf1, header2.b0len, unpacked);
        copyTo(old_songdata, sizeof(OldFixedSongData), unpacked);

        if (!reader.readBlock(buf1, header2.b1len)) return corrupted();
        decompressOld(method, buf1, header2.b1len, unpacked);
        copyTo(hash_buffer, sizeof(OldVariableData2), unpacked);
        import_old_a2m_patterns2(song, *hash_buffer, 0, 8);

        for (int temp2 = 0; temp2 <= 6; ++temp2) {
            if (blocks > temp2) {
                if (!reader.readBlock(buf1, xlen[temp2])) return corrupted();
                // オリジナルは 2 ブロック目以降も b2len をサイズとして渡している
                decompressOld(method, buf1, header2.b2len, unpacked);
                copyTo(hash_buffer, sizeof(OldVariableData2), unpacked);
                import_old_a2m_patterns2(song, *hash_buffer, temp2 + 1, 8);
            }
        }
        import_old_songdata(songdata, *old_songdata);
    }

    if (header.ffver >= 9 && header.ffver <= FFVER_A2M) {
        const bool lzh = header.ffver >= 12;
        const int blocks = (header.patts - 1) / 8;

        uint32_t crc = 0xffffffff;
        if (!reader.readBlock(buf1, header.b0len)) return corrupted();
        crc = Update32(buf1.data(), buf1.size(), crc);
        if (!reader.readBlock(buf1, header.b1len[0])) return corrupted();
        crc = Update32(buf1.data(), buf1.size(), crc);
        for (int temp2 = 1; temp2 <= 15; ++temp2) {
            if (blocks > temp2 - 1) {
                if (!reader.readBlock(buf1, header.b1len[temp2])) return corrupted();
                crc = Update32(buf1.data(), buf1.size(), crc);
            }
        }
        crc = Update32(&header.b0len, 2, crc);
        for (int temp2 = 0; temp2 <= 15; ++temp2) {
            crc = Update32(&header.b1len[temp2], 2, crc);
        }
        if (crc != header.crc32) {
            error = L"CRC が一致しません";
            return false;
        }

        song.init(defaults);

        auto unpack = [&](uint32_t size) {
            if (lzh) LZH_decompress(buf1.data(), buf1.size(), size, unpacked);
            else APACK_decompress(buf1.data(), buf1.size(), unpacked);
        };
        auto copyToPatterns = [&](int block) {
            const size_t offset = block * sizeof(VariableData);
            copyTo(reinterpret_cast<uint8_t*>(&song.patterns()) + offset, sizeof(PatternData) - offset, unpacked);
        };

        if (!reader.seek(sizeof(Header))) return corrupted();
        if (!reader.readBlock(buf1, header.b0len)) return corrupted();
        unpack(header.b0len);
        copyTo(&songdata, sizeof(FixedSongData), unpacked);

        if (!reader.readBlock(buf1, header.b1len[0])) return corrupted();

        if (header.ffver == 9) import_old_flags(songdata);

        for (int temp = 1; temp <= 255; ++temp) {
            auto& name = at1(songdata.instr_names, temp);
            pinsertHead(name, std::string(1, name.data[0]) + "iNS_" + byte2hex(static_cast<uint8_t>(temp)) + "\xF7 ");
        }
        if (header.ffver >= 11) {
            for (int temp = 0; temp <= 0x7f; ++temp) {
                auto& name = songdata.pattern_names[temp];
                pinsertHead(name, std::string(1, name.data[0]) + "PAT_" + byte2hex(static_cast<uint8_t>(temp)) + "  \xF7 ");
            }
        }

        unpack(header.b1len[0]);
        copyToPatterns(0);
        for (int temp2 = 1; temp2 <= 15; ++temp2) {
            if (blocks > temp2 - 1) {
                if (!reader.readBlock(buf1, header.b1len[temp2])) return corrupted();
                unpack(header.b1len[temp2]);
                copyToPatterns(temp2);
            }
        }
    }

    limit_macro_speedup(songdata);
    return true;
}
