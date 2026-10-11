#include "ModuleSaver.h"

#include <filesystem>
#include <fstream>
#include <vector>

#include "Depack.h"
#include "LoaderCommon.h"
#include "Pack.h"

namespace
{
    constexpr uint8_t FFVER_A2M = 14;
    constexpr uint8_t FFVER_A2T = 14;
    constexpr uint8_t FFVER_A2P = 11;

#pragma pack(push, 1)
    struct A2mHeader
    {
        char ident[10];
        uint32_t crc32;
        uint8_t ffver;
        uint8_t patts;
        uint32_t b0len;
        uint32_t b1len[16];
    };

    struct A2tHeader
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

    struct A2pHeader
    {
        char ident[11];
        uint32_t crc32;
        uint8_t ffver;
        uint32_t b0len;
    };
#pragma pack(pop)

    static_assert(sizeof(A2mHeader) == 84);
    static_assert(sizeof(A2tHeader) == 134);
    static_assert(sizeof(A2pHeader) == 20);

    bool Empty(const void* buf, size_t size)
    {
        const auto* p = static_cast<const uint8_t*>(buf);
        for (size_t i = 0; i < size; ++i) {
            if (p[i] != 0) return false;
        }
        return true;
    }

    // count_patterns: 空でない最後のパターン + 1
    uint8_t count_patterns(const Song& song)
    {
        uint8_t patterns = 0;
        for (int temp1 = 0; temp1 < 128; ++temp1) {
            if (!Empty(song.patterns().block[temp1 / 8][temp1 % 8], PATTERN_SIZE)) patterns = static_cast<uint8_t>(temp1 + 1);
        }
        return patterns;
    }

    // count_instruments: 空でない最後の楽器
    uint8_t count_instruments(const FixedSongData& songdata)
    {
        uint8_t instruments = 255;
        while (instruments > 0 && Empty(&at1(songdata.instr_data, instruments), INSTRUMENT_SIZE)) --instruments;
        return instruments;
    }

    // FillChar(str[SUCC(Length(str))], count, 0) (count が負なら何もしない)
    template <size_t N>
    void fill_after_length(PString<N>& str, int count)
    {
        if (count > 0) std::memset(str.data + str.len, 0, std::min<size_t>(count, N - str.len));
    }

    // Delete(str, 1, count)。後ろに残った文字は消さない (オリジナルと同じ)
    template <size_t N>
    void delete_head(PString<N>& str, uint8_t count)
    {
        if (count >= str.len) {
            str.len = 0;
            return;
        }
        std::memmove(str.data, str.data + count, str.len - count);
        str.len = static_cast<uint8_t>(str.len - count);
    }

    void apply_save_state(FixedSongData& songdata, const SongSaveState& state)
    {
        songdata.bpm_data.rows_per_beat = state.mark_line;
        songdata.bpm_data.tempo_finetune = state.tempo_finetune;
    }

    class Writer
    {
    public:
        explicit Writer(const std::wstring& path) : path_(path), f_(std::filesystem::path(path), std::ios::binary | std::ios::trunc) {}

        bool is_open() const { return f_.is_open(); }

        bool write(const void* data, size_t size)
        {
            f_.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
            return static_cast<bool>(f_);
        }

        bool write_at(size_t pos, const void* data, size_t size)
        {
            f_.seekp(static_cast<std::streamoff>(pos));
            return write(data, size);
        }

        // 書き込みに失敗したらファイルを消す (EraseF)
        SaveResult fail()
        {
            f_.close();
            std::error_code ec;
            std::filesystem::remove(std::filesystem::path(path_), ec);
            return SaveResult::DiskFull;
        }

    private:
        std::wstring path_;
        std::ofstream f_;
    };

    void progress_begin(const SaveProgress* progress, int num_steps)
    {
        if (progress && progress->begin) progress->begin(static_cast<uint8_t>(num_steps));
    }

    void progress_step(const SaveProgress* progress)
    {
        if (progress && progress->step) progress->step();
    }

    const LzhProgress& progress_value(const SaveProgress* progress)
    {
        static const LzhProgress none;
        return progress ? progress->value : none;
    }
}

SaveResult SaveA2m(const std::wstring& path, Song& song, const SongSaveState& state, const SaveProgress* progress)
{
    Writer f(path);
    if (!f.is_open()) return SaveResult::WriteError;

    FixedSongData& songdata = song.data();

    fill_after_length(songdata.songname, 42 - songdata.songname.len);
    fill_after_length(songdata.composer, 42 - songdata.composer.len);
    for (int temp = 1; temp <= 255; ++temp) {
        auto& name = at1(songdata.instr_names, temp);
        fill_after_length(name, 42 - name.len);
    }
    for (int temp = 0; temp <= 0x7f; ++temp) {
        auto& name = songdata.pattern_names[temp];
        fill_after_length(name, 42 - name.len);
    }

    A2mHeader header{};
    header.patts = count_patterns(song);
    header.crc32 = 0xffffffff;
    std::memcpy(header.ident, "_A2module_", 10);

    songdata.common_flag = state.common_flag;
    apply_save_state(songdata, state);

    header.ffver = FFVER_A2M;
    if (!f.write(&header, sizeof(header))) return f.fail();
    if (header.patts == 0) header.patts = 1;
    progress_begin(progress, (header.patts - 1) / 8 + 2);

    // 名前の先頭 ("iNS_xx÷ " など) を外して圧縮する
    char temp_marks[256];
    char temp_marks2[128];
    for (int temp = 1; temp <= 255; ++temp) {
        auto& name = at1(songdata.instr_names, temp);
        temp_marks[temp] = name.data[0];
        delete_head(name, 9);
        fill_after_length(name, 32 - name.len);
    }
    for (int temp = 0; temp <= 0x7f; ++temp) {
        auto& name = songdata.pattern_names[temp];
        temp_marks2[temp] = name.data[0];
        delete_head(name, 11);
        fill_after_length(name, 32 - name.len);
    }

    std::vector<uint8_t> buf1;
    header.b0len = static_cast<uint32_t>(LZH_compress(reinterpret_cast<const uint8_t*>(&songdata), sizeof(FixedSongData), buf1, progress_value(progress)));
    for (int temp = 1; temp <= 255; ++temp) {
        loader::pinsertHead(at1(songdata.instr_names, temp),
                            std::string(1, temp_marks[temp]) + "iNS_" + byte2hex(static_cast<uint8_t>(temp)) + "\xF7 ");
    }
    for (int temp = 0; temp <= 0x7f; ++temp) {
        loader::pinsertHead(songdata.pattern_names[temp],
                            std::string(1, temp_marks2[temp]) + "PAT_" + byte2hex(static_cast<uint8_t>(temp)) + "  \xF7 ");
    }

    const bool b0_ok = f.write(buf1.data(), header.b0len);
    progress_step(progress);
    if (!b0_ok) return f.fail();
    header.crc32 = Update32(buf1.data(), header.b0len, header.crc32);

    for (int index = 0; index <= 15; ++index) {
        if (index > 0 && !((header.patts - 1) / 8 > index - 1)) continue;
        header.b1len[index] = static_cast<uint32_t>(
            LZH_compress(reinterpret_cast<const uint8_t*>(&song.patterns().block[index]), sizeof(VariableData), buf1,
                         progress_value(progress)));
        const bool ok = f.write(buf1.data(), header.b1len[index]);
        progress_step(progress);
        if (!ok) return f.fail();
        header.crc32 = Update32(buf1.data(), header.b1len[index], header.crc32);
    }

    header.crc32 = Update32(&header.b0len, 2, header.crc32);
    for (int index = 0; index <= 15; ++index) header.crc32 = Update32(&header.b1len[index], 2, header.crc32);

    if (!f.write_at(0, &header, sizeof(header))) return f.fail();
    song.title = std::filesystem::path(path).filename().wstring();
    return SaveResult::Ok;
}

SaveResult SaveA2t(const std::wstring& path, Song& song, const SongSaveState& state, const SaveProgress* progress)
{
    Writer f(path);
    if (!f.is_open()) return SaveResult::WriteError;

    FixedSongData& songdata = song.data();

    A2tHeader header{};
    header.patts = count_patterns(song);
    header.crc32 = 0xffffffff;
    std::memcpy(header.ident, "_A2tiny_module_", 15);
    header.tempo = songdata.tempo;
    header.speed = songdata.speed;
    header.cflag = state.common_flag;
    apply_save_state(songdata, state);

    header.patln = songdata.patt_len;
    header.nmtrk = songdata.nm_tracks;
    header.mcspd = songdata.macro_speedup;
    header.is4op = songdata.flag_4op;
    std::memcpy(header.locks, songdata.lock_flags, sizeof(header.locks));
    header.ffver = FFVER_A2T;

    if (!f.write(&header, sizeof(header))) return f.fail();
    if (header.patts == 0) header.patts = 1;
    progress_begin(progress, (header.patts - 1) / 8 + 6);

    const uint8_t instruments = static_cast<uint8_t>(loader::pas_min(count_instruments(songdata), 1));

    // b0: bpm_data + ins_4op_flags + reserved_data + 使われている楽器
    std::vector<uint8_t> buf2;
    const auto append = [&buf2](const void* p, size_t n) {
        const auto* b = static_cast<const uint8_t*>(p);
        buf2.insert(buf2.end(), b, b + n);
    };
    append(&songdata.bpm_data, sizeof(songdata.bpm_data));
    append(&songdata.ins_4op_flags, sizeof(songdata.ins_4op_flags));
    append(songdata.reserved_data, sizeof(songdata.reserved_data));
    append(songdata.instr_data, instruments * sizeof(Instrument));

    std::vector<uint8_t> buf1;
    header.b0len = static_cast<uint32_t>(LZH_compress_ultra(buf2.data(), static_cast<uint32_t>(buf2.size()), buf1, progress_value(progress)));
    const bool b0_ok = f.write(buf1.data(), header.b0len);
    progress_step(progress);
    if (!b0_ok) return f.fail();
    header.crc32 = Update32(buf1.data(), header.b0len, header.crc32);

    // b1..b3: 空でなければ圧縮して書く
    const auto optional_block = [&](const void* data, size_t size, uint32_t& blen) -> bool {
        if (!Empty(data, size)) {
            blen = static_cast<uint32_t>(LZH_compress_ultra(static_cast<const uint8_t*>(data), static_cast<uint32_t>(size), buf1,
                                                            progress_value(progress)));
            const bool ok = f.write(buf1.data(), blen);
            progress_step(progress);
            if (!ok) return false;
        } else {
            blen = 0;
            progress_step(progress);
        }
        header.crc32 = Update32(buf1.data(), blen, header.crc32);
        return true;
    };
    if (!optional_block(songdata.instr_macros, instruments * sizeof(RegisterTable), header.b1len)) return f.fail();
    if (!optional_block(songdata.macro_table, sizeof(songdata.macro_table), header.b2len)) return f.fail();
    if (!optional_block(songdata.dis_fmreg_col, sizeof(songdata.dis_fmreg_col), header.b3len)) return f.fail();

    header.b4len = static_cast<uint32_t>(LZH_compress_ultra(songdata.pattern_order, sizeof(songdata.pattern_order), buf1,
                                                            progress_value(progress)));
    const bool b4_ok = f.write(buf1.data(), header.b4len);
    progress_step(progress);
    if (!b4_ok) return f.fail();
    header.crc32 = Update32(buf1.data(), header.b4len, header.crc32);

    for (int index = 0; index <= 15; ++index) {
        if (index > 0 && !((header.patts - 1) / 8 > index - 1)) continue;
        size_t temp2;
        if (header.patts < (index + 1) * 8) temp2 = (header.patts - index * 8) * PATTERN_SIZE;
        else temp2 = sizeof(VariableData);
        header.b5len[index] = static_cast<uint32_t>(
            LZH_compress_ultra(reinterpret_cast<const uint8_t*>(&song.patterns().block[index]), static_cast<uint32_t>(temp2), buf1,
                               progress_value(progress)));
        const bool ok = f.write(buf1.data(), header.b5len[index]);
        progress_step(progress);
        if (!ok) return f.fail();
        header.crc32 = Update32(buf1.data(), header.b5len[index], header.crc32);
    }

    header.crc32 = Update32(&header.b0len, 2, header.crc32);
    header.crc32 = Update32(&header.b1len, 2, header.crc32);
    header.crc32 = Update32(&header.b2len, 2, header.crc32);
    header.crc32 = Update32(&header.b3len, 2, header.crc32);
    header.crc32 = Update32(&header.b4len, 2, header.crc32);
    for (int index = 0; index <= 15; ++index) header.crc32 = Update32(&header.b5len[index], 2, header.crc32);

    if (!f.write_at(0, &header, sizeof(header))) return f.fail();
    song.title = std::filesystem::path(path).filename().wstring();
    return SaveResult::Ok;
}

SaveResult SaveA2p(const std::wstring& path, Song& song, uint8_t pattern)
{
    Writer f(path);
    if (!f.is_open()) return SaveResult::WriteError;

    A2pHeader header{};
    header.crc32 = 0xffffffff;
    std::memcpy(header.ident, "_A2pattern_", 11);
    header.ffver = FFVER_A2P;

    // パターンデータに続けてパターン名 (String[30] の長さ + 文字) を置く。
    // オリジナルは名前の後ろ (31 バイト目まで) に作業用バッファの前の内容が残るが、ここでは 0 とする。
    std::vector<uint8_t> buf2(PATTERN_SIZE + 30 + 1, 0);
    std::memcpy(buf2.data(), song.patterns().block[pattern / 8][pattern % 8], PATTERN_SIZE);
    const std::string temp_str = loader::pcopy(song.data().pattern_names[pattern].str(), 12, 30);
    buf2[PATTERN_SIZE] = static_cast<uint8_t>(temp_str.size());
    std::memcpy(buf2.data() + PATTERN_SIZE + 1, temp_str.data(), temp_str.size());

    if (!f.write(&header, sizeof(header))) return f.fail();

    std::vector<uint8_t> buf1;
    header.b0len = static_cast<uint32_t>(LZH_compress(buf2.data(), static_cast<uint32_t>(buf2.size()), buf1));
    if (!f.write(buf1.data(), header.b0len)) return f.fail();

    header.crc32 = Update32(buf1.data(), header.b0len, header.crc32);
    header.crc32 = Update32(&header.b0len, 2, header.crc32);

    if (!f.write_at(0, &header, sizeof(header))) return f.fail();
    return SaveResult::Ok;
}
