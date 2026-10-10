#include "A2pLoader.h"

#include <memory>
#include <vector>

#include "Depack.h"
#include "LoaderCommon.h"

using namespace loader;

namespace
{
    constexpr uint8_t FFVER_A2P = 11;

#pragma pack(push, 1)
    struct OldHeader   // tOLD_HEADER (形式 1..8)
    {
        char ident[11];
        uint32_t crc32;
        uint8_t ffver;
        uint16_t b0len;
    };

    struct Header   // tHEADER (形式 9..11)
    {
        char ident[11];
        uint32_t crc32;
        uint8_t ffver;
        uint32_t b0len;
    };
#pragma pack(pop)

    static_assert(sizeof(OldHeader) == 18);
    static_assert(sizeof(Header) == 20);

    const char A2P_ID[] = "_A2pattern_";

    void set_pattern_name(Song& song, uint8_t pattern, const std::string& name)
    {
        auto& pattern_name = song.data().pattern_names[pattern];
        pattern_name.assign(pcopy(pattern_name.str(), 1, 11) + name);
    }

    // 形式 10, 11: 展開結果はパターンデータに続いてパターン名 (String) が入っている
    void import_pattern_with_name(Song& song, uint8_t pattern, const std::wstring& path, std::vector<uint8_t>& buf2)
    {
        if (buf2.size() < PATTERN_SIZE + 256) buf2.resize(PATTERN_SIZE + 256, 0);
        std::memcpy(song.patterns().block[pattern / 8][pattern % 8], buf2.data(), PATTERN_SIZE);
        const uint8_t len = buf2[PATTERN_SIZE];
        const std::string temp_str(reinterpret_cast<const char*>(buf2.data()) + PATTERN_SIZE + 1, len);
        if (!temp_str.empty()) set_pattern_name(song, pattern, temp_str);
        else set_pattern_name(song, pattern, lower_name_only(path));
    }
}

bool LoadA2p(const std::wstring& path, Song& song, uint8_t pattern, std::wstring& error)
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
    std::vector<uint8_t> buf1;
    std::vector<uint8_t> unpacked;

    OldHeader header{};
    if (reader.read(&header, sizeof(header)) != sizeof(header) || std::memcmp(header.ident, A2P_ID, 11) != 0) {
        return corrupted();
    }
    if (header.ffver < 1 || header.ffver > FFVER_A2P) {
        error = L"未対応の形式バージョンです (" + std::to_wstring(header.ffver) + L")";
        return false;
    }

    if (header.ffver >= 1 && header.ffver <= 8) {
        if (!reader.readInto(buf1, header.b0len)) return corrupted();
        uint32_t crc = 0xffffffff;
        crc = Update32(buf1.data(), header.b0len, crc);
        crc = Update32(&header.b0len, 2, crc);
        if (crc != header.crc32) return crc_failed();
    }

    if (header.ffver >= 1 && header.ffver <= 4) {
        // オリジナルの old_hash_buffer はグローバル変数で前回の内容が残るが、ここでは 0 から始める
        struct Buffer { OldVariableData1 data; };
        auto old_hash_buffer = std::make_unique<Buffer>();
        std::memset(old_hash_buffer.get(), 0, sizeof(Buffer));
        decompressOld(static_cast<uint8_t>(4 - header.ffver), buf1, header.b0len, unpacked);
        copyTo(&old_hash_buffer->data, sizeof(OldVariableData1), unpacked);

        OldImportContext ctx{ song };
        for (int line = 0; line <= 0x3f; ++line) {
            for (int chan = 1; chan <= 9; ++chan) {
                import_old_a2m_event1(ctx, pattern, static_cast<uint8_t>(line), static_cast<uint8_t>(chan),
                                      old_hash_buffer->data[0][line][chan - 1], false);
            }
        }
        set_pattern_name(song, pattern, lower_name_only(path));
    }

    if (header.ffver >= 5 && header.ffver <= 8) {
        struct Buffer { OldVariableData2 data; };
        auto hash_buffer = std::make_unique<Buffer>();
        std::memset(hash_buffer.get(), 0, sizeof(Buffer));
        decompressOld(static_cast<uint8_t>(8 - header.ffver), buf1, header.b0len, unpacked);
        copyTo(&hash_buffer->data, sizeof(OldVariableData2), unpacked);

        for (int line = 0; line <= 0x3f; ++line) {
            for (int chan = 1; chan <= 18; ++chan) {
                import_old_a2m_event2(song, pattern, static_cast<uint8_t>(line), static_cast<uint8_t>(chan),
                                      hash_buffer->data[0][chan - 1][line]);
            }
        }
        set_pattern_name(song, pattern, lower_name_only(path));
    }

    if (header.ffver >= 9 && header.ffver <= FFVER_A2P) {
        reader.seek(0);
        Header header2{};
        if (reader.read(&header2, sizeof(header2)) != sizeof(header2) || std::memcmp(header2.ident, A2P_ID, 11) != 0) {
            return corrupted();
        }
        if (!reader.readInto(buf1, header2.b0len)) return corrupted();
        uint32_t crc = 0xffffffff;
        crc = Update32(buf1.data(), header2.b0len, crc);
        crc = Update32(&header2.b0len, 2, crc);
        if (crc != header2.crc32) return crc_failed();

        if (header2.ffver == 9) {
            APACK_decompress(buf1.data(), buf1.size(), unpacked);
            copyToField(song.patterns(), song.patterns().block[pattern / 8][pattern % 8], unpacked);
            set_pattern_name(song, pattern, lower_name_only(path));
        } else if (header2.ffver == 10) {
            APACK_decompress(buf1.data(), buf1.size(), unpacked);
            import_pattern_with_name(song, pattern, path, unpacked);
        } else {
            LZH_decompress(buf1.data(), buf1.size(), header2.b0len, unpacked);
            import_pattern_with_name(song, pattern, path, unpacked);
        }
    }

    return true;
}
