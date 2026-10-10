#include "A2tLoader.h"

#include <memory>
#include <vector>

#include "Depack.h"
#include "LoaderCommon.h"

using namespace loader;

namespace
{
    constexpr uint8_t FFVER_A2T = 14;

#pragma pack(push, 1)
    struct OldHeader1   // tOLD_HEADER1 (形式 1..4)
    {
        char ident[15];
        uint32_t crc32;
        uint8_t ffver;
        uint8_t patts;
        uint8_t tempo;
        uint8_t speed;
        uint16_t b0len, b1len, b2len, b3len, b4len, b5len;
    };

    struct OldHeader2   // tOLD_HEADER2 (形式 5..8)
    {
        char ident[15];
        uint32_t crc32;
        uint8_t ffver;
        uint8_t patts;
        uint8_t tempo;
        uint8_t speed;
        uint8_t cflag;
        uint16_t b0len, b1len, b2len, b3len, b4len, b5len, b6len, b7len, b8len, b9len;
    };

    struct OldHeader3   // tOLD_HEADER3 (形式 9)
    {
        char ident[15];
        uint32_t crc32;
        uint8_t ffver;
        uint8_t patts;
        uint8_t tempo;
        uint8_t speed;
        uint8_t cflag;
        uint16_t patln;
        uint8_t nmtrk;
        uint16_t mcspd;
        uint32_t b0len, b1len, b2len, b3len;
        uint32_t b4len[16];
    };

    struct OldHeader4   // tOLD_HEADER4 (形式 10)
    {
        char ident[15];
        uint32_t crc32;
        uint8_t ffver;
        uint8_t patts;
        uint8_t tempo;
        uint8_t speed;
        uint8_t cflag;
        uint16_t patln;
        uint8_t nmtrk;
        uint16_t mcspd;
        uint8_t is4op;
        uint8_t locks[20];
        uint32_t b0len, b1len, b2len, b3len;
        uint32_t b4len[16];
    };

    struct Header   // tHEADER (形式 11..14)
    {
        char ident[15];
        uint32_t crc32;
        uint8_t ffver;
        uint8_t patts;
        uint8_t tempo;
        uint8_t speed;
        uint8_t cflag;
        uint16_t patln;
        uint8_t nmtrk;
        uint16_t mcspd;
        uint8_t is4op;
        uint8_t locks[20];
        uint32_t b0len, b1len, b2len, b3len, b4len;
        uint32_t b5len[16];
    };
#pragma pack(pop)

    static_assert(sizeof(OldHeader1) == 35);
    static_assert(sizeof(OldHeader2) == 44);
    static_assert(sizeof(OldHeader3) == 109);
    static_assert(sizeof(OldHeader4) == 130);
    static_assert(sizeof(Header) == 134);

    const char A2T_ID[] = "_A2tiny_module_";

    // 展開先のパターンブロック (pattdata^[block]) へ、パターンデータの終わりまでを容量として書き込む
    void copyToPatterns(Song& song, int block, const std::vector<uint8_t>& unpacked)
    {
        copyToField(song.patterns(), song.patterns().block[block], unpacked);
    }

    enum class CheckResult { Ok, Corrupted, CrcFailed };

    // 形式 9..14 のブロックを読んで CRC を確かめる (blens は楽器・マクロ・オーダーなど、plens はパターン)
    template <class H>
    CheckResult check_crc(Reader& reader, const H& header, std::vector<uint8_t>& buf1,
                         const uint32_t* blens, int nblens, const uint32_t* plens)
    {
        uint32_t crc = 0xffffffff;
        for (int i = 0; i < nblens; ++i) {
            if (!reader.readInto(buf1, blens[i])) return CheckResult::Corrupted;
            crc = Update32(buf1.data(), blens[i], crc);
        }
        if (!reader.readInto(buf1, plens[0])) return CheckResult::Corrupted;
        crc = Update32(buf1.data(), plens[0], crc);
        for (int temp2 = 1; temp2 <= 15; ++temp2) {
            if ((header.patts - 1) / 8 > temp2 - 1) {
                if (!reader.readInto(buf1, plens[temp2])) return CheckResult::Corrupted;
                crc = Update32(buf1.data(), plens[temp2], crc);
            }
        }
        for (int i = 0; i < nblens; ++i) crc = Update32(&blens[i], 2, crc);
        for (int temp2 = 0; temp2 <= 15; ++temp2) crc = Update32(&plens[temp2], 2, crc);
        return crc == header.crc32 ? CheckResult::Ok : CheckResult::CrcFailed;
    }
}

bool LoadA2t(const std::wstring& path, Song& song, const Song::Defaults& defaults, std::wstring& error)
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
    const auto crc_failed = [&error] {
        error = L"CRC が一致しません";
        return false;
    };

    Reader reader(file);
    std::vector<uint8_t> buf1;   // オリジナルのグローバル変数 buf1 (読み込みサイズを超えた部分は前の内容が残る)
    std::vector<uint8_t> unpacked;

    Header header{};
    if (reader.read(&header, sizeof(header)) != sizeof(header) || std::memcmp(header.ident, A2T_ID, 15) != 0) {
        return corrupted();
    }
    if (header.ffver < 1 || header.ffver > FFVER_A2T) {
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
    OldFixedSongData& old_songdata = old->old_songdata;
    init_old_songdata(old_songdata);

    FixedSongData& songdata = song.data();

    if (header.ffver >= 1 && header.ffver <= 4) {
        OldImportContext ctx{ song };
        reader.seek(0);
        OldHeader1 header2{};
        if (reader.read(&header2, sizeof(header2)) != sizeof(header2) || std::memcmp(header2.ident, A2T_ID, 15) != 0) {
            return corrupted();
        }

        const uint16_t xlen[3] = { header2.b3len, header2.b4len, header2.b5len };
        const int blocks = (header2.patts - 1) / 16;

        uint32_t crc = 0xffffffff;
        const uint16_t blen[3] = { header2.b0len, header2.b1len, header2.b2len };
        for (const uint16_t len : blen) {
            if (!reader.readInto(buf1, len)) return corrupted();
            crc = Update32(buf1.data(), len, crc);
        }
        for (int temp2 = 0; temp2 <= 2; ++temp2) {
            if (blocks > temp2) {
                if (!reader.readInto(buf1, xlen[temp2])) return corrupted();
                crc = Update32(buf1.data(), xlen[temp2], crc);
            }
        }
        for (const uint16_t& len : blen) crc = Update32(&len, 2, crc);
        for (int temp2 = 0; temp2 <= 2; ++temp2) crc = Update32(&xlen[temp2], 2, crc);
        if (crc != header2.crc32) return crc_failed();

        song.init(defaults);
        songdata.patt_len = 64;
        songdata.nm_tracks = 9;   // adjust_tracks = TRUE

        const uint8_t method = static_cast<uint8_t>(4 - header2.ffver);   // 4:Move 3:LZSS 2:LZW 1:SixPack

        reader.seek(sizeof(OldHeader1));
        if (!reader.readInto(buf1, header2.b0len)) return corrupted();
        old_songdata.tempo = header2.tempo;
        old_songdata.speed = header2.speed;
        decompressOld(method, buf1, header2.b0len, unpacked);
        copyToField(old_songdata, old_songdata.instr_data, unpacked);

        for (int temp = 1; temp <= 250; ++temp) at1(old_songdata.instr_data, temp).panning = 0;

        if (!reader.readInto(buf1, header2.b1len)) return corrupted();
        decompressOld(method, buf1, header2.b1len, unpacked);
        copyToField(old_songdata, old_songdata.pattern_order, unpacked);

        if (!reader.readInto(buf1, header2.b2len)) return corrupted();
        std::memset(&old->old_hash_buffer, 0, sizeof(OldVariableData1));
        decompressOld(method, buf1, header2.b2len, unpacked);
        copyTo(&old->old_hash_buffer, sizeof(OldVariableData1), unpacked);
        import_old_a2m_patterns1(ctx, old->old_hash_buffer, 0, 16);

        for (int temp2 = 0; temp2 <= 2; ++temp2) {
            if (blocks > temp2) {
                if (!reader.readInto(buf1, xlen[temp2])) return corrupted();
                std::memset(&old->old_hash_buffer, 0, sizeof(OldVariableData1));
                // オリジナルは 2 ブロック目以降も b3len をサイズとして渡している
                decompressOld(method, buf1, header2.b3len, unpacked);
                copyTo(&old->old_hash_buffer, sizeof(OldVariableData1), unpacked);
                import_old_a2m_patterns1(ctx, old->old_hash_buffer, temp2 + 1, 16);
            }
        }

        replace_old_adsr(ctx, header2.patts);
        import_old_songdata(songdata, old_songdata);
    }

    if (header.ffver >= 5 && header.ffver <= 8) {
        reader.seek(0);
        OldHeader2 header3{};
        if (reader.read(&header3, sizeof(header3)) != sizeof(header3) || std::memcmp(header3.ident, A2T_ID, 15) != 0) {
            return corrupted();
        }

        const uint16_t xlen[7] = { header3.b3len, header3.b4len, header3.b5len, header3.b6len,
                                   header3.b7len, header3.b8len, header3.b9len };
        const int blocks = (header3.patts - 1) / 8;

        uint32_t crc = 0xffffffff;
        const uint16_t blen[3] = { header3.b0len, header3.b1len, header3.b2len };
        for (const uint16_t len : blen) {
            if (!reader.readInto(buf1, len)) return corrupted();
            crc = Update32(buf1.data(), len, crc);
        }
        for (int temp2 = 0; temp2 <= 6; ++temp2) {
            if (blocks > temp2) {
                if (!reader.readInto(buf1, xlen[temp2])) return corrupted();
                crc = Update32(buf1.data(), xlen[temp2], crc);
            }
        }
        for (const uint16_t& len : blen) crc = Update32(&len, 2, crc);
        for (int temp2 = 0; temp2 <= 6; ++temp2) crc = Update32(&xlen[temp2], 2, crc);
        if (crc != header3.crc32) return crc_failed();

        song.init(defaults);
        songdata.patt_len = 64;
        songdata.nm_tracks = 18;   // adjust_tracks = TRUE

        const uint8_t method = static_cast<uint8_t>(8 - header3.ffver);   // 8:Move 7:LZSS 6:LZW 5:SixPack

        reader.seek(sizeof(OldHeader2));
        if (!reader.readInto(buf1, header3.b0len)) return corrupted();
        old_songdata.tempo = header3.tempo;
        old_songdata.speed = header3.speed;
        old_songdata.common_flag = header3.cflag;
        decompressOld(method, buf1, header3.b0len, unpacked);
        copyToField(old_songdata, old_songdata.instr_data, unpacked);

        if (!reader.readInto(buf1, header3.b1len)) return corrupted();
        decompressOld(method, buf1, header3.b1len, unpacked);
        copyToField(old_songdata, old_songdata.pattern_order, unpacked);

        if (!reader.readInto(buf1, header3.b2len)) return corrupted();
        std::memset(&old->hash_buffer, 0, sizeof(OldVariableData2));
        decompressOld(method, buf1, header3.b2len, unpacked);
        copyTo(&old->hash_buffer, sizeof(OldVariableData2), unpacked);
        import_old_a2m_patterns2(song, old->hash_buffer, 0, 8);

        for (int temp2 = 0; temp2 <= 6; ++temp2) {
            if (blocks > temp2) {
                if (!reader.readInto(buf1, xlen[temp2])) return corrupted();
                std::memset(&old->hash_buffer, 0, sizeof(OldVariableData2));
                // オリジナルは 2 ブロック目以降も b3len をサイズとして渡している
                decompressOld(method, buf1, header3.b3len, unpacked);
                copyTo(&old->hash_buffer, sizeof(OldVariableData2), unpacked);
                import_old_a2m_patterns2(song, old->hash_buffer, temp2 + 1, 8);
            }
        }
        import_old_songdata(songdata, old_songdata);
    }

    if (header.ffver == 9) {
        reader.seek(0);
        OldHeader3 header4{};
        if (reader.read(&header4, sizeof(header4)) != sizeof(header4) || std::memcmp(header4.ident, A2T_ID, 15) != 0) {
            return corrupted();
        }
        const uint32_t blen[4] = { header4.b0len, header4.b1len, header4.b2len, header4.b3len };
        const CheckResult result = check_crc(reader, header4, buf1, blen, 4, header4.b4len);
        if (result != CheckResult::Ok) return result == CheckResult::Corrupted ? corrupted() : crc_failed();

        song.init(defaults);
        reader.seek(sizeof(OldHeader3));

        if (!reader.readInto(buf1, header4.b0len)) return corrupted();
        APACK_decompress(buf1.data(), buf1.size(), unpacked);
        copyToField(songdata, songdata.instr_data, unpacked);
        if (!reader.readInto(buf1, header4.b1len)) return corrupted();
        APACK_decompress(buf1.data(), buf1.size(), unpacked);
        copyToField(songdata, songdata.instr_macros, unpacked);
        if (!reader.readInto(buf1, header4.b2len)) return corrupted();
        APACK_decompress(buf1.data(), buf1.size(), unpacked);
        copyToField(songdata, songdata.macro_table, unpacked);
        if (!reader.readInto(buf1, header4.b3len)) return corrupted();

        songdata.tempo = header4.tempo;
        songdata.speed = header4.speed;
        songdata.common_flag = header4.cflag;
        songdata.patt_len = header4.patln;
        songdata.nm_tracks = header4.nmtrk;
        songdata.macro_speedup = header4.mcspd;
        import_old_flags(songdata);

        APACK_decompress(buf1.data(), buf1.size(), unpacked);
        copyToField(songdata, songdata.pattern_order, unpacked);
        if (!reader.readInto(buf1, header4.b4len[0])) return corrupted();
        APACK_decompress(buf1.data(), buf1.size(), unpacked);
        copyToPatterns(song, 0, unpacked);
        for (int temp2 = 1; temp2 <= 15; ++temp2) {
            if ((header4.patts - 1) / 8 > temp2 - 1) {
                if (!reader.readInto(buf1, header4.b4len[temp2])) return corrupted();
                APACK_decompress(buf1.data(), buf1.size(), unpacked);
                copyToPatterns(song, temp2, unpacked);
            }
        }
    }

    if (header.ffver == 10) {
        reader.seek(0);
        OldHeader4 header5{};
        if (reader.read(&header5, sizeof(header5)) != sizeof(header5) || std::memcmp(header5.ident, A2T_ID, 15) != 0) {
            return corrupted();
        }
        const uint32_t blen[4] = { header5.b0len, header5.b1len, header5.b2len, header5.b3len };
        const CheckResult result = check_crc(reader, header5, buf1, blen, 4, header5.b4len);
        if (result != CheckResult::Ok) return result == CheckResult::Corrupted ? corrupted() : crc_failed();

        song.init(defaults);
        reader.seek(sizeof(OldHeader4));

        if (!reader.readInto(buf1, header5.b0len)) return corrupted();
        APACK_decompress(buf1.data(), buf1.size(), unpacked);
        copyToField(songdata, songdata.instr_data, unpacked);
        if (!reader.readInto(buf1, header5.b1len)) return corrupted();
        APACK_decompress(buf1.data(), buf1.size(), unpacked);
        copyToField(songdata, songdata.instr_macros, unpacked);
        if (!reader.readInto(buf1, header5.b2len)) return corrupted();
        APACK_decompress(buf1.data(), buf1.size(), unpacked);
        copyToField(songdata, songdata.macro_table, unpacked);
        if (!reader.readInto(buf1, header5.b3len)) return corrupted();

        songdata.tempo = header5.tempo;
        songdata.speed = header5.speed;
        songdata.common_flag = header5.cflag;
        songdata.patt_len = header5.patln;
        songdata.nm_tracks = header5.nmtrk;
        songdata.macro_speedup = header5.mcspd;
        songdata.flag_4op = header5.is4op;
        std::memcpy(songdata.lock_flags, header5.locks, sizeof(songdata.lock_flags));

        APACK_decompress(buf1.data(), buf1.size(), unpacked);
        copyToField(songdata, songdata.pattern_order, unpacked);
        if (!reader.readInto(buf1, header5.b4len[0])) return corrupted();
        APACK_decompress(buf1.data(), buf1.size(), unpacked);
        copyToPatterns(song, 0, unpacked);
        for (int temp2 = 1; temp2 <= 15; ++temp2) {
            if ((header5.patts - 1) / 8 > temp2 - 1) {
                if (!reader.readInto(buf1, header5.b4len[temp2])) return corrupted();
                APACK_decompress(buf1.data(), buf1.size(), unpacked);
                copyToPatterns(song, temp2, unpacked);
            }
        }
    }

    if (header.ffver == 11) {
        const uint32_t blen[5] = { header.b0len, header.b1len, header.b2len, header.b3len, header.b4len };
        const CheckResult result = check_crc(reader, header, buf1, blen, 5, header.b5len);
        if (result != CheckResult::Ok) return result == CheckResult::Corrupted ? corrupted() : crc_failed();

        song.init(defaults);
        reader.seek(sizeof(Header));

        if (!reader.readInto(buf1, header.b0len)) return corrupted();
        APACK_decompress(buf1.data(), buf1.size(), unpacked);
        copyToField(songdata, songdata.instr_data, unpacked);
        if (!reader.readInto(buf1, header.b1len)) return corrupted();
        APACK_decompress(buf1.data(), buf1.size(), unpacked);
        copyToField(songdata, songdata.instr_macros, unpacked);
        if (!reader.readInto(buf1, header.b2len)) return corrupted();
        APACK_decompress(buf1.data(), buf1.size(), unpacked);
        copyToField(songdata, songdata.macro_table, unpacked);
        if (!reader.readInto(buf1, header.b3len)) return corrupted();
        APACK_decompress(buf1.data(), buf1.size(), unpacked);
        copyToField(songdata, songdata.dis_fmreg_col, unpacked);
        if (!reader.readInto(buf1, header.b4len)) return corrupted();

        songdata.tempo = header.tempo;
        songdata.speed = header.speed;
        songdata.common_flag = header.cflag;
        songdata.patt_len = header.patln;
        songdata.nm_tracks = header.nmtrk;
        songdata.macro_speedup = header.mcspd;
        songdata.flag_4op = header.is4op;
        std::memcpy(songdata.lock_flags, header.locks, sizeof(songdata.lock_flags));

        APACK_decompress(buf1.data(), buf1.size(), unpacked);
        copyToField(songdata, songdata.pattern_order, unpacked);
        if (!reader.readInto(buf1, header.b5len[0])) return corrupted();
        APACK_decompress(buf1.data(), buf1.size(), unpacked);
        copyToPatterns(song, 0, unpacked);
        for (int temp2 = 1; temp2 <= 15; ++temp2) {
            if ((header.patts - 1) / 8 > temp2 - 1) {
                if (!reader.readInto(buf1, header.b5len[temp2])) return corrupted();
                APACK_decompress(buf1.data(), buf1.size(), unpacked);
                copyToPatterns(song, temp2, unpacked);
            }
        }
    }

    if (header.ffver >= 12 && header.ffver <= FFVER_A2T) {
        const uint32_t blen[5] = { header.b0len, header.b1len, header.b2len, header.b3len, header.b4len };
        const CheckResult result = check_crc(reader, header, buf1, blen, 5, header.b5len);
        if (result != CheckResult::Ok) return result == CheckResult::Corrupted ? corrupted() : crc_failed();

        song.init(defaults);
        reader.seek(sizeof(Header));

        if (!reader.readInto(buf1, header.b0len)) return corrupted();
        const size_t temp = LZH_decompress(buf1.data(), buf1.size(), header.b0len, unpacked);
        size_t temp2 = 0;
        const auto take = [&](void* dst, size_t size) {
            copyTo(dst, size, unpacked.data() + std::min(temp2, unpacked.size()),
                   unpacked.size() - std::min(temp2, unpacked.size()));
            temp2 += size;
        };
        if (header.ffver == FFVER_A2T) take(&songdata.bpm_data, sizeof(songdata.bpm_data));
        take(&songdata.ins_4op_flags, sizeof(songdata.ins_4op_flags));
        take(songdata.reserved_data, sizeof(songdata.reserved_data));
        // Move(buf2[temp2], songdata.instr_data, temp - temp2)
        if (temp > temp2) {
            copyToField(songdata, songdata.instr_data, unpacked.data() + temp2,
                        std::min(temp, unpacked.size()) - std::min(temp2, unpacked.size()));
        }

        if (header.b1len != 0) {
            if (!reader.readInto(buf1, header.b1len)) return corrupted();
            LZH_decompress(buf1.data(), buf1.size(), header.b1len, unpacked);
            copyToField(songdata, songdata.instr_macros, unpacked);
        }
        if (header.b2len != 0) {
            if (!reader.readInto(buf1, header.b2len)) return corrupted();
            LZH_decompress(buf1.data(), buf1.size(), header.b2len, unpacked);
            copyToField(songdata, songdata.macro_table, unpacked);
        }
        if (header.b3len != 0) {
            if (!reader.readInto(buf1, header.b3len)) return corrupted();
            LZH_decompress(buf1.data(), buf1.size(), header.b3len, unpacked);
            copyToField(songdata, songdata.dis_fmreg_col, unpacked);
        }
        if (!reader.readInto(buf1, header.b4len)) return corrupted();

        songdata.tempo = header.tempo;
        songdata.speed = header.speed;
        songdata.common_flag = header.cflag;
        songdata.patt_len = header.patln;
        songdata.nm_tracks = header.nmtrk;
        songdata.macro_speedup = header.mcspd;
        songdata.flag_4op = header.is4op;
        std::memcpy(songdata.lock_flags, header.locks, sizeof(songdata.lock_flags));

        LZH_decompress(buf1.data(), buf1.size(), header.b4len, unpacked);
        copyToField(songdata, songdata.pattern_order, unpacked);
        if (!reader.readInto(buf1, header.b5len[0])) return corrupted();
        LZH_decompress(buf1.data(), buf1.size(), header.b5len[0], unpacked);
        copyToPatterns(song, 0, unpacked);
        for (int temp3 = 1; temp3 <= 15; ++temp3) {
            if ((header.patts - 1) / 8 > temp3 - 1) {
                if (!reader.readInto(buf1, header.b5len[temp3])) return corrupted();
                LZH_decompress(buf1.data(), buf1.size(), header.b5len[temp3], unpacked);
                copyToPatterns(song, temp3, unpacked);
            }
        }
    }

    limit_macro_speedup(songdata);
    return true;
}
