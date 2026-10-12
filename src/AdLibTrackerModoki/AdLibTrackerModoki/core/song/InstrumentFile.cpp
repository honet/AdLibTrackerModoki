#include "InstrumentFile.h"

#include <filesystem>
#include <fstream>
#include <vector>

#include "Depack.h"
#include "LoaderCommon.h"
#include "ModuleLoader.h"
#include "Pack.h"

using namespace loader;

namespace a2ins
{

namespace
{
    constexpr uint8_t FFVER_A2I = 10;
    constexpr uint8_t FFVER_A2F = 2;

#pragma pack(push, 1)
    struct A2iOldHeader   // tOLD_HEADER (A2I 形式 1..8)
    {
        char ident[7];
        uint16_t crc16;
        uint8_t ffver;
        uint8_t b0len;
    };

    struct A2iHeader   // tHEADER (A2I 形式 9, 10)
    {
        char ident[7];
        uint16_t crc16;
        uint8_t ffver;
        uint16_t b0len;
    };

    struct A2fHeader
    {
        char ident[18];
        uint32_t crc32;
        uint8_t ffver;
        uint16_t b0len;
    };
#pragma pack(pop)

    static_assert(sizeof(A2iOldHeader) == 11);
    static_assert(sizeof(A2iHeader) == 12);
    static_assert(sizeof(A2fHeader) == 25);

    const char A2I_ID[] = "_A2ins_";
    const char A2F_ID[] = "_a2ins_w/fm-macro_";

    std::wstring lower_ext(const std::wstring& path)
    {
        std::wstring ext = std::filesystem::path(path).extension().wstring();
        if (!ext.empty() && ext[0] == L'.') ext.erase(0, 1);
        for (wchar_t& c : ext) {
            if (c >= L'A' && c <= L'Z') c = static_cast<wchar_t>(c - L'A' + L'a');
        }
        return ext;
    }

    // buf[pos] から Pascal の String (長さ + 文字) を読む。pos は読んだ後の位置に進める
    std::string read_pstring(std::vector<uint8_t>& buf, size_t& pos)
    {
        if (buf.size() < pos + 256) buf.resize(pos + 256, 0);
        const uint8_t len = buf[pos];
        std::string str(reinterpret_cast<const char*>(buf.data()) + pos + 1, len);
        pos += 1 + len;
        return str;
    }

    template <class T>
    void read_struct(std::vector<uint8_t>& buf, size_t& pos, T& dst)
    {
        if (buf.size() < pos + sizeof(T)) buf.resize(pos + sizeof(T), 0);
        std::memcpy(&dst, buf.data() + pos, sizeof(T));
        pos += sizeof(T);
    }

    void set_instr_name(FixedSongData& songdata, uint8_t ins, const std::string& name)
    {
        auto& instr_name = at1(songdata.instr_names, ins);
        instr_name.assign(pcopy(instr_name.str(), 1, 9) + truncate_string(name));
    }

    // 4OP の組でない楽器を読み込む前に、current_inst の組を解除する
    void reset_current_4op(FixedSongData& songdata, uint8_t current_inst)
    {
        const uint16_t ins_4op = check_4op_instrument(songdata, current_inst);
        if (ins_4op != 0) {
            reset_4op_flag(songdata, static_cast<uint8_t>(ins_4op >> 8));
            update_4op_flag_marks(songdata);
        }
    }

    // 4OP の組の楽器を読み込む位置 (current_inst の組を作り、その 1 つ目)
    uint8_t set_current_4op(FixedSongData& songdata, uint8_t current_inst)
    {
        uint8_t _4op_ins_idx = current_inst;
        set_4op_flag(songdata, current_inst);
        update_4op_flag_marks(songdata);
        if (_4op_ins_idx == 255) --_4op_ins_idx;
        return _4op_ins_idx;
    }

    bool load_a2i(const std::wstring& path, const std::vector<uint8_t>& file, FixedSongData& songdata, uint8_t current_inst,
                  std::wstring& error)
    {
        const auto corrupted = [&error] {
            error = L"ファイルが壊れています";
            return false;
        };
        const auto crc_failed = [&error] {
            error = L"CRC が一致しません";
            return false;
        };

        Reader reader(file);
        std::vector<uint8_t> buf2;
        std::vector<uint8_t> buf3;

        A2iOldHeader header{};
        if (reader.read(&header, sizeof(header)) != sizeof(header) || std::memcmp(header.ident, A2I_ID, 7) != 0) {
            return corrupted();
        }
        if (header.ffver < 1 || header.ffver > FFVER_A2I) {
            error = L"未対応の形式バージョンです (" + std::to_wstring(header.ffver) + L")";
            return false;
        }

        if (header.ffver >= 1 && header.ffver <= 8) {
            if (!reader.readInto(buf2, header.b0len)) return corrupted();
            uint16_t crc = 0xffff;
            crc = Update16(&header.b0len, 1, crc);
            crc = Update16(buf2.data(), header.b0len, crc);
            if (crc != header.crc16) return crc_failed();

            // 形式 1..4 と 5..8 は同じ並び (展開方法が 4..1 / 8..5 の順に Move / LZSS / LZW / SixPack)
            decompressOld(static_cast<uint8_t>(header.ffver <= 4 ? 4 - header.ffver : 8 - header.ffver), buf2, header.b0len, buf3);

            // old_songdata の楽器 1 を経由して取り込む (import_single_old_instrument)
            size_t pos = 0;
            OldInstrument old_ins{};
            read_struct(buf3, pos, old_ins);
            std::string temp_str = read_pstring(buf3, pos);
            if (header.ffver <= 4) old_ins.panning = 0;
            if (temp_str.empty()) temp_str = lower_name_only(path);

            PString<32> old_name{};
            old_name.assign(" iNS_01\xF7 " + truncate_string(temp_str));
            auto& instr_name = at1(songdata.instr_names, current_inst);
            instr_name.assign(pcopy(instr_name.str(), 1, 9) + pcopy(old_name.str(), 10, 22));
            Instrument& ins = at1(songdata.instr_data, current_inst);
            ins.fm_data = old_ins.fm_data;
            ins.panning = old_ins.panning;
            ins.fine_tune = old_ins.fine_tune;
            ins.perc_voice = 0;
            return true;
        }

        reader.seek(0);
        A2iHeader header2{};
        if (reader.read(&header2, sizeof(header2)) != sizeof(header2) || std::memcmp(header2.ident, A2I_ID, 7) != 0) {
            return corrupted();
        }
        if (!reader.readInto(buf2, header2.b0len)) return corrupted();
        uint16_t crc = 0xffff;
        crc = Update16(&header2.b0len, 1, crc);
        crc = Update16(buf2.data(), header2.b0len, crc);
        if (crc != header2.crc16) return crc_failed();

        if (header2.ffver == 9) {
            APACK_decompress(buf2.data(), header2.b0len, buf3);
            size_t pos = 0;
            read_struct(buf3, pos, at1(songdata.instr_data, current_inst));
            std::string temp_str = read_pstring(buf3, pos);
            if (temp_str.empty()) temp_str = lower_name_only(path);
            set_instr_name(songdata, current_inst, temp_str);
            return true;
        }

        // 形式 10 (FFVER_A2I)
        const size_t temp2 = LZH_decompress(buf2.data(), header2.b0len, header2.b0len, buf3);
        size_t temp = 0;
        Instrument temp_ins{}, temp_ins2{};
        read_struct(buf3, temp, temp_ins);   // instrument data
        std::string temp_str = read_pstring(buf3, temp);   // instrument name
        std::string temp_str2;
        const bool _4op_ins_flag = temp < temp2;   // more data present => 4op instrument
        if (_4op_ins_flag) {
            read_struct(buf3, temp, temp_ins2);
            temp_str2 = read_pstring(buf3, temp);
        }

        if (!_4op_ins_flag) {
            reset_current_4op(songdata, current_inst);
            at1(songdata.instr_data, current_inst) = temp_ins;
            if (temp_str.empty()) temp_str = lower_name_only(path);
            set_instr_name(songdata, current_inst, temp_str);
        } else {
            const uint8_t _4op_ins_idx = set_current_4op(songdata, current_inst);
            at1(songdata.instr_data, _4op_ins_idx) = temp_ins;
            if (temp_str.empty()) temp_str = lower_name_only(path) + " [4OP 1/2]";
            set_instr_name(songdata, _4op_ins_idx, temp_str);
            at1(songdata.instr_data, _4op_ins_idx + 1) = temp_ins2;
            if (temp_str2.empty()) temp_str2 = lower_name_only(path) + " [4OP 2/2]";
            set_instr_name(songdata, static_cast<uint8_t>(_4op_ins_idx + 1), temp_str2);
        }
        return true;
    }

    bool load_a2f(const std::wstring& path, const std::vector<uint8_t>& file, FixedSongData& songdata, uint8_t current_inst,
                  std::wstring& error)
    {
        const auto corrupted = [&error] {
            error = L"ファイルが壊れています";
            return false;
        };

        Reader reader(file);
        std::vector<uint8_t> buf2;
        std::vector<uint8_t> buf3;

        A2fHeader header{};
        if (reader.read(&header, sizeof(header)) != sizeof(header) || std::memcmp(header.ident, A2F_ID, 18) != 0) {
            return corrupted();
        }
        if (header.ffver < 1 || header.ffver > FFVER_A2F) {
            error = L"未対応の形式バージョンです (" + std::to_wstring(header.ffver) + L")";
            return false;
        }

        if (!reader.readInto(buf2, header.b0len)) return corrupted();
        uint32_t crc = 0xffffffff;
        crc = Update32(&header.b0len, 1, crc);
        crc = Update32(buf2.data(), header.b0len, crc);
        if (crc != header.crc32) {
            error = L"CRC が一致しません";
            return false;
        }

        if (header.ffver == 1) {
            APACK_decompress(buf2.data(), header.b0len, buf3);
            size_t pos = 0;
            read_struct(buf3, pos, at1(songdata.instr_data, current_inst));
            std::string temp_str = read_pstring(buf3, pos);
            if (temp_str.empty()) temp_str = lower_name_only(path);
            set_instr_name(songdata, current_inst, temp_str);
            read_struct(buf3, pos, at1(songdata.instr_macros, current_inst));
            read_struct(buf3, pos, at1(songdata.dis_fmreg_col, current_inst));
            return true;
        }

        // 形式 2 (FFVER_A2F)
        const size_t temp2 = LZH_decompress(buf2.data(), header.b0len, header.b0len, buf3);
        size_t temp = 0;
        Instrument temp_ins{}, temp_ins2{};
        RegisterTable temp_macro{}, temp_macro2{};
        DisFmregCol temp_dis_fmreg_col{}, temp_dis_fmreg_col2{};
        read_struct(buf3, temp, temp_ins);                       // instrument data
        std::string temp_str = read_pstring(buf3, temp);        // instrument name
        read_struct(buf3, temp, temp_macro);                     // FM-macro data
        read_struct(buf3, temp, temp_dis_fmreg_col);             // disabled FM-macro column data
        std::string temp_str2;
        const bool _4op_ins_flag = temp < temp2;   // more data present => 4op instrument
        if (_4op_ins_flag) {
            read_struct(buf3, temp, temp_ins2);
            temp_str2 = read_pstring(buf3, temp);
            read_struct(buf3, temp, temp_macro2);
            read_struct(buf3, temp, temp_dis_fmreg_col2);
        }

        const auto put = [&](uint8_t ins, const Instrument& data, const RegisterTable& macro, const DisFmregCol& dis) {
            at1(songdata.instr_data, ins) = data;
            at1(songdata.instr_macros, ins) = macro;
            std::memcpy(at1(songdata.dis_fmreg_col, ins), dis, sizeof(DisFmregCol));
        };

        if (!_4op_ins_flag) {
            reset_current_4op(songdata, current_inst);
            put(current_inst, temp_ins, temp_macro, temp_dis_fmreg_col);
            if (temp_str.empty()) temp_str = lower_name_only(path);
            set_instr_name(songdata, current_inst, temp_str);
        } else {
            const uint8_t _4op_ins_idx = set_current_4op(songdata, current_inst);
            put(_4op_ins_idx, temp_ins, temp_macro, temp_dis_fmreg_col);
            if (temp_str.empty()) temp_str = lower_name_only(path) + " [4OP 1/2]";
            set_instr_name(songdata, _4op_ins_idx, temp_str);
            put(static_cast<uint8_t>(_4op_ins_idx + 1), temp_ins2, temp_macro2, temp_dis_fmreg_col2);
            if (temp_str2.empty()) temp_str2 = lower_name_only(path) + " [4OP 2/2]";
            set_instr_name(songdata, static_cast<uint8_t>(_4op_ins_idx + 1), temp_str2);
        }
        return true;
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

    void append(std::vector<uint8_t>& buf, const void* data, size_t size)
    {
        const auto* p = static_cast<const uint8_t*>(data);
        buf.insert(buf.end(), p, p + size);
    }

    // 楽器データと名前 (Copy(instr_names[ins], 10, 32) を Pascal の String として) を置く。
    // with_macros なら FM マクロと無効にした列も続けて置く (A2F)。
    // オリジナルはマクロの直後に arpeggio_table / vibrato_table を 0 にする書き込みをするが、
    // 位置がずれていて dis_fmreg_col で上書きされるため、マクロはそのまま保存される。
    void append_instrument(std::vector<uint8_t>& buf, const FixedSongData& songdata, uint8_t ins, bool with_macros)
    {
        append(buf, &at1(songdata.instr_data, ins), sizeof(Instrument));
        const std::string temp_str = pcopy(at1(songdata.instr_names, ins).str(), 10, 32);
        buf.push_back(static_cast<uint8_t>(temp_str.size()));
        append(buf, temp_str.data(), temp_str.size());
        if (with_macros) {
            append(buf, &at1(songdata.instr_macros, ins), sizeof(RegisterTable));
            append(buf, at1(songdata.dis_fmreg_col, ins), sizeof(DisFmregCol));
        }
    }

    std::vector<uint8_t> instrument_block(const FixedSongData& songdata, uint8_t current_inst, bool with_macros)
    {
        std::vector<uint8_t> buf2;
        const uint16_t ins_4op = check_4op_instrument(songdata, current_inst);
        if (ins_4op != 0) {
            append_instrument(buf2, songdata, static_cast<uint8_t>(ins_4op >> 8), with_macros);     // 4OP 1/2
            append_instrument(buf2, songdata, static_cast<uint8_t>(ins_4op & 0xff), with_macros);   // 4OP 2/2
        } else {
            append_instrument(buf2, songdata, current_inst, with_macros);
        }
        return buf2;
    }
}

bool check_4op_flag(const FixedSongData& songdata, uint8_t ins)
{
    for (int idx = 1; idx <= songdata.ins_4op_flags.num_4op && idx <= 128; ++idx) {
        if (songdata.ins_4op_flags.idx_4op[idx - 1] == ins) return true;
    }
    return false;
}

uint16_t check_4op_instrument(const FixedSongData& songdata, uint8_t ins)
{
    if (check_4op_flag(songdata, ins)) return static_cast<uint16_t>(ins + 1 + (ins << 8));
    if (ins > 1 && check_4op_flag(songdata, static_cast<uint8_t>(ins - 1))) return static_cast<uint16_t>(ins + ((ins - 1) << 8));
    return 0;
}

void reset_4op_flag(FixedSongData& songdata, uint8_t ins)
{
    if (!check_4op_flag(songdata, ins)) return;
    Ins4opFlags temp{};
    const Ins4opFlags& flags = songdata.ins_4op_flags;
    int idx = 1;
    while (idx <= flags.num_4op && flags.idx_4op[idx - 1] < ins) temp.idx_4op[temp.num_4op++] = flags.idx_4op[idx++ - 1];
    ++idx;
    while (idx <= flags.num_4op) temp.idx_4op[temp.num_4op++] = flags.idx_4op[idx++ - 1];
    songdata.ins_4op_flags = temp;
}

void set_4op_flag(FixedSongData& songdata, uint8_t ins)
{
    if (ins == 255) return;
    if (check_4op_flag(songdata, ins)) reset_4op_flag(songdata, ins);
    if (ins > 1 && check_4op_flag(songdata, static_cast<uint8_t>(ins - 1))) reset_4op_flag(songdata, static_cast<uint8_t>(ins - 1));
    if (ins < 255 && check_4op_flag(songdata, static_cast<uint8_t>(ins + 1))) reset_4op_flag(songdata, static_cast<uint8_t>(ins + 1));
    Ins4opFlags temp{};
    const Ins4opFlags& flags = songdata.ins_4op_flags;
    int idx = 1;
    while (idx <= flags.num_4op && flags.idx_4op[idx - 1] < ins) temp.idx_4op[temp.num_4op++] = flags.idx_4op[idx++ - 1];
    temp.idx_4op[temp.num_4op++] = ins;
    while (idx <= flags.num_4op) temp.idx_4op[temp.num_4op++] = flags.idx_4op[idx++ - 1];
    songdata.ins_4op_flags = temp;
}

void update_4op_flag_marks(FixedSongData& songdata)
{
    const auto is_flag_chr = [](char c) { return c == _4op_flag_chr_beg || c == _4op_flag_chr_end; };
    for (int temp = 1; temp <= 254; ++temp) {
        if (!check_4op_flag(songdata, static_cast<uint8_t>(temp))) {
            auto& n1 = at1(songdata.instr_names, temp);
            auto& n2 = at1(songdata.instr_names, temp + 1);
            if (is_flag_chr(n1.data[0])) n1.data[0] = ' ';
            if (is_flag_chr(n2.data[0])) n2.data[0] = ' ';
        }
    }
    for (int temp = 1; temp <= 254; ++temp) {
        if (check_4op_flag(songdata, static_cast<uint8_t>(temp))) {
            auto& n1 = at1(songdata.instr_names, temp);
            auto& n2 = at1(songdata.instr_names, temp + 1);
            if (n1.data[0] == ' ') n1.data[0] = _4op_flag_chr_beg;
            if (n2.data[0] == ' ') n2.data[0] = _4op_flag_chr_end;
        }
    }
}

bool LoadInstrumentFile(const std::wstring& path, FixedSongData& songdata, uint8_t current_inst, std::wstring& error)
{
    const std::wstring ext = lower_ext(path);
    if (ext != L"a2i" && ext != L"a2f") {
        error = L"対応していない形式です";
        return false;
    }

    std::vector<uint8_t> file;
    if (!readFile(path, file)) {
        error = L"ファイルを開けません";
        return false;
    }

    const bool ok = ext == L"a2i" ? load_a2i(path, file, songdata, current_inst, error)
                                  : load_a2f(path, file, songdata, current_inst, error);
    if (!ok) return false;
    filter_loaded_names(songdata);
    return true;
}

SaveResult SaveA2i(const std::wstring& path, const FixedSongData& songdata, uint8_t current_inst)
{
    Writer f(path);
    if (!f.is_open()) return SaveResult::WriteError;

    A2iHeader header{};
    std::memcpy(header.ident, A2I_ID, 7);
    header.ffver = FFVER_A2I;
    if (!f.write(&header, sizeof(header))) return f.fail();

    const std::vector<uint8_t> buf2 = instrument_block(songdata, current_inst, false);
    std::vector<uint8_t> buf3;
    const size_t temp2 = LZH_compress(buf2.data(), static_cast<uint32_t>(buf2.size()), buf3);
    if (!f.write(buf3.data(), temp2)) return f.fail();

    header.b0len = static_cast<uint16_t>(temp2);
    uint16_t crc = 0xffff;
    crc = Update16(&header.b0len, 1, crc);
    crc = Update16(buf3.data(), header.b0len, crc);
    header.crc16 = crc;
    if (!f.write_at(0, &header, sizeof(header))) return f.fail();
    return SaveResult::Ok;
}

SaveResult SaveA2f(const std::wstring& path, const FixedSongData& songdata, uint8_t current_inst)
{
    Writer f(path);
    if (!f.is_open()) return SaveResult::WriteError;

    A2fHeader header{};
    std::memcpy(header.ident, A2F_ID, 18);
    header.ffver = FFVER_A2F;
    if (!f.write(&header, sizeof(header))) return f.fail();

    const std::vector<uint8_t> buf2 = instrument_block(songdata, current_inst, true);
    std::vector<uint8_t> buf3;
    const size_t temp2 = LZH_compress(buf2.data(), static_cast<uint32_t>(buf2.size()), buf3);
    if (!f.write(buf3.data(), temp2)) return f.fail();

    header.b0len = static_cast<uint16_t>(temp2);
    header.crc32 = 0xffffffff;
    header.crc32 = Update32(&header.b0len, 1, header.crc32);
    header.crc32 = Update32(buf3.data(), header.b0len, header.crc32);
    if (!f.write_at(0, &header, sizeof(header))) return f.fail();
    return SaveResult::Ok;
}

} // namespace a2ins
