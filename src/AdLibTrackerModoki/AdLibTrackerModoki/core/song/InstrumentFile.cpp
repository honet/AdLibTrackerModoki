#include "InstrumentFile.h"

#include <filesystem>
#include <fstream>
#include <memory>
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

    constexpr uint8_t FFVER_A2B = 10;
    constexpr uint8_t FFVER_A2W = 3;

#pragma pack(push, 1)
    struct A2bOldHeader   // tOLD_HEADER (A2B 形式 1..8)
    {
        char ident[11];
        uint32_t crc32;
        uint8_t ffver;
        uint16_t b0len;
    };

    struct A2bHeader   // tHEADER (A2B 形式 9, 10)
    {
        char ident[11];
        uint32_t crc32;
        uint8_t ffver;
        uint32_t b0len;
    };

    struct A2wOldHeader   // tOLD_HEADER (A2W 形式 1)
    {
        char ident[20];
        uint32_t crc32;
        uint8_t ffver;
        uint32_t b0len;
        uint32_t b1len;
    };

    struct A2wHeader   // tHEADER (A2W 形式 2, 3)
    {
        char ident[20];
        uint32_t crc32;
        uint8_t ffver;
        uint32_t b0len;
        uint32_t b1len;
        uint32_t b2len;
    };
#pragma pack(pop)

    static_assert(sizeof(A2bOldHeader) == 18);
    static_assert(sizeof(A2bHeader) == 20);
    static_assert(sizeof(A2wOldHeader) == 33);
    static_assert(sizeof(A2wHeader) == 37);

    const char A2B_ID[] = "_A2insbank_";
    const char A2W_ID[] = "_A2insbank_w/macros_";

    // 楽器名の先頭 9 文字 (印と ' iNS_xx'#247' ') を外す。外した印を marks[1..255] に入れる
    void strip_instr_names(FixedSongData& songdata, char (&marks)[256])
    {
        for (int temp = 1; temp <= 255; ++temp) {
            auto& name = at1(songdata.instr_names, temp);
            marks[temp] = name.data[0];
            delete_head(name, 9);
            fill_after_length(name, 32 - name.len);
        }
    }

    // 楽器名の先頭に印と番号を付け直す
    void restore_instr_names(FixedSongData& songdata, const char (&marks)[256])
    {
        for (int temp = 1; temp <= 255; ++temp) {
            pinsertHead(at1(songdata.instr_names, temp),
                        std::string(1, marks[temp]) + "iNS_" + byte2hex(static_cast<uint8_t>(temp)) + "\xF7 ");
        }
    }

    void save_instr_marks(const FixedSongData& songdata, char (&marks)[256])
    {
        for (int temp = 1; temp <= 255; ++temp) marks[temp] = at1(songdata.instr_names, temp).data[0];
    }

    // Move(buf, dst, size)。展開結果が size に足りなければ残りは 0 とする
    void move_from(const std::vector<uint8_t>& buf, size_t ofs, void* dst, size_t size)
    {
        const size_t n = buf.size() > ofs ? std::min(size, buf.size() - ofs) : 0;
        if (n > 0) std::memcpy(dst, buf.data() + ofs, n);
        if (n < size) std::memset(static_cast<uint8_t*>(dst) + n, 0, size - n);
    }

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
            error = L"The file is corrupted";
            return false;
        };
        const auto crc_failed = [&error] {
            error = L"CRC mismatch";
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
            error = L"Unsupported format version (" + std::to_wstring(header.ffver) + L")";
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
            error = L"The file is corrupted";
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
            error = L"Unsupported format version (" + std::to_wstring(header.ffver) + L")";
            return false;
        }

        if (!reader.readInto(buf2, header.b0len)) return corrupted();
        uint32_t crc = 0xffffffff;
        crc = Update32(&header.b0len, 1, crc);
        crc = Update32(buf2.data(), header.b0len, crc);
        if (crc != header.crc32) {
            error = L"CRC mismatch";
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

    // 他のトラッカーの楽器ファイル (adt2ext3.pas の cif / fin / ins / sbi / sgi_file_loader)。
    // FM のデータ・panning・fine_tune だけを書き換え、perc_voice はそのまま残す (オリジナルと同じ)。

    // 楽器名を Copy(instr_names[ins], 1, 9) + name にする
    void put_instr_name(FixedSongData& songdata, uint8_t ins, const std::string& name)
    {
        auto& instr_name = at1(songdata.instr_names, ins);
        instr_name.assign(pcopy(instr_name.str(), 1, 9) + name);
    }

    // BlockRead(f, buffer, SizeOf(buffer), temp) と同じく、ファイルの先頭から最大 size バイトを読む。
    // 読めなかった部分は 0 とする (オリジナルは初期化していないスタックの値)。読めたバイト数を返す。
    size_t read_head(const std::vector<uint8_t>& file, void* buffer, size_t size)
    {
        const size_t n = std::min(size, file.size());
        std::memset(buffer, 0, size);
        if (n > 0) std::memcpy(buffer, file.data(), n);
        return n;
    }

#pragma pack(push, 1)
    struct CifData   // tCIF_DATA
    {
        char ident[20];
        uint8_t idata[11];
        uint8_t resrv;
        char iname[20];
    };

    struct FinData   // tFIN_DATA
    {
        char dname[12];
        char iname[27];
        uint8_t idata[11];
    };

    struct InsData   // tINS_DATA
    {
        uint8_t idata[11];
        uint8_t slide;
        uint8_t _SAdT[19];
    };

    struct SbiData   // tSBI_DATA
    {
        char ident[4];
        char iname[32];
        uint8_t idata[11];
        uint8_t dummy[5];
    };
#pragma pack(pop)

    static_assert(sizeof(CifData) == 52);
    static_assert(sizeof(FinData) == 50);
    static_assert(sizeof(InsData) == 31);
    static_assert(sizeof(SbiData) == 52);

    constexpr size_t SGI_DATA_SIZE = 26;   // tSGI_DATA

    // INS の並びの判別に使う設定 (adtrack2.ini の force_ins。本クローン版は既定値の 0 に固定)
    constexpr uint8_t force_ins = 0;

    bool load_cif(const std::wstring& path, const std::vector<uint8_t>& file, FixedSongData& songdata, uint8_t current_inst,
                  std::wstring& error)
    {
        static const char id[] = "<CUD-FM-Instrument>\x1A";
        constexpr size_t MIN_CIF_SIZE = sizeof(CifData::ident) + sizeof(CifData::idata) + sizeof(CifData::resrv);
        CifData buffer;
        const size_t temp = read_head(file, &buffer, sizeof(buffer));
        if (!(temp >= MIN_CIF_SIZE && std::memcmp(buffer.ident, id, 20) == 0)) {
            error = L"The file is corrupted";
            return false;
        }

        import_hsc_instrument(songdata, current_inst, buffer.idata);
        at1(songdata.instr_data, current_inst).fine_tune = 0;

        std::string temp_str = truncate_string(char_array(buffer.iname));
        if (temp_str.empty()) temp_str = lower_name_only(path);
        put_instr_name(songdata, current_inst, temp_str);
        return true;
    }

    bool load_fin(const std::wstring& path, const std::vector<uint8_t>& file, FixedSongData& songdata, uint8_t current_inst,
                  std::wstring& error)
    {
        FinData buffer;
        if (read_head(file, &buffer, sizeof(buffer)) != sizeof(buffer)) {
            error = L"The file is corrupted";
            return false;
        }

        import_fin_instrument(songdata, current_inst, buffer.idata);
        // iname は 27 文字なので、常に iname を使う
        std::string temp_str;
        if (truncate_string(char_array(buffer.iname)).size() <= 32) {
            temp_str = truncate_string(char_array(buffer.iname));
        } else {
            temp_str = truncate_string(char_array(buffer.dname));
            for (char& c : temp_str) {
                if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
            }
        }
        if (temp_str.empty()) temp_str = lower_name_only(path);
        put_instr_name(songdata, current_inst, pcopy(temp_str, 1, 32));
        return true;
    }

    bool load_ins(const std::wstring& path, const std::vector<uint8_t>& file, FixedSongData& songdata, uint8_t current_inst,
                  std::wstring& error)
    {
        InsData buffer;
        if (file.size() > sizeof(buffer)) {
            error = L"Unsupported format";
            return false;
        }
        const size_t temp = read_head(file, &buffer, file.size());

        // tADTRACK2_INS として見て、波形とフィードバックが範囲内か
        const auto correct_ins = [](const uint8_t* data) { return data[8] <= 3 && data[9] <= 3 && data[10] <= 15; };

        switch (force_ins) {
        case 0:
            if (temp == 12) import_standard_instrument(songdata, current_inst, buffer.idata);
            if (temp == 12 && !correct_ins(buffer.idata)) import_hsc_instrument(songdata, current_inst, buffer.idata);
            else if (temp > 12) import_sat_instrument(songdata, current_inst, buffer.idata);
            break;
        case 1: import_hsc_instrument(songdata, current_inst, buffer.idata); break;
        case 2: import_sat_instrument(songdata, current_inst, buffer.idata); break;
        case 3: import_standard_instrument(songdata, current_inst, buffer.idata); break;
        }

        put_instr_name(songdata, current_inst, lower_name_only(path));
        return true;
    }

    bool load_sbi(const std::wstring& path, const std::vector<uint8_t>& file, FixedSongData& songdata, uint8_t current_inst,
                  std::wstring& error)
    {
        SbiData buffer;
        if (!(read_head(file, &buffer, sizeof(buffer)) == sizeof(buffer) && std::memcmp(buffer.ident, "SBI\x1A", 4) == 0)) {
            error = L"The file is corrupted";
            return false;
        }

        import_standard_instrument(songdata, current_inst, buffer.idata);
        std::string temp_str = truncate_string(char_array(buffer.iname));
        if (temp_str.empty()) temp_str = lower_name_only(path);
        put_instr_name(songdata, current_inst, pcopy(temp_str, 1, 32));
        return true;
    }

    // import_sgi_instrument (楽器データ全体を 0 にしてから入れる)
    void import_sgi_instrument(FixedSongData& songdata, uint8_t inst, const uint8_t* data)
    {
        Instrument& ins = at1(songdata.instr_data, inst);
        ins = Instrument{};
        FmInstData& fm = ins.fm_data;
        fm.ATTCK_DEC_modulator = static_cast<uint8_t>((data[1] & 0x0f) + ((data[0] & 0x0f) << 4));
        fm.SUSTN_REL_modulator = static_cast<uint8_t>((data[3] & 0x0f) + ((data[2] & 0x0f) << 4));
        fm.WAVEFORM_modulator = static_cast<uint8_t>(data[4] & 3);
        fm.KSL_VOLUM_modulator = static_cast<uint8_t>((data[7] & 0x3f) + ((data[6] & 3) << 6));
        fm.AM_VIB_EG_modulator = static_cast<uint8_t>((data[5] & 0x0f) + ((data[8] & 1) << 4) + ((data[11] & 1) << 5) +
                                                      ((data[10] & 1) << 6) + ((data[9] & 1) << 7));
        fm.ATTCK_DEC_carrier = static_cast<uint8_t>((data[13] & 0x0f) + ((data[12] & 0x0f) << 4));
        fm.SUSTN_REL_carrier = static_cast<uint8_t>((data[15] & 0x0f) + ((data[14] & 0x0f) << 4));
        fm.WAVEFORM_carrier = static_cast<uint8_t>(data[16] & 3);
        fm.KSL_VOLUM_carrier = static_cast<uint8_t>((data[19] & 0x3f) + ((data[18] & 3) << 6));
        fm.AM_VIB_EG_carrier = static_cast<uint8_t>((data[17] & 0x0f) + ((data[20] & 1) << 4) + ((data[23] & 1) << 5) +
                                                    ((data[22] & 1) << 6) + ((data[21] & 1) << 7));
        fm.FEEDBACK_FM = static_cast<uint8_t>((data[25] & 1) + ((data[24] & 7) << 1));
    }

    bool load_sgi(const std::wstring& path, const std::vector<uint8_t>& file, FixedSongData& songdata, uint8_t current_inst,
                  std::wstring& error)
    {
        uint8_t buffer[SGI_DATA_SIZE];
        if (read_head(file, buffer, sizeof(buffer)) != sizeof(buffer)) {
            error = L"The file is corrupted";
            return false;
        }

        import_sgi_instrument(songdata, current_inst, buffer);
        put_instr_name(songdata, current_inst, pcopy(lower_name_only(path), 1, 32));
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
    using LoadFunc = bool (*)(const std::wstring&, const std::vector<uint8_t>&, FixedSongData&, uint8_t, std::wstring&);
    LoadFunc load = nullptr;
    if (ext == L"a2i") load = load_a2i;
    else if (ext == L"a2f") load = load_a2f;
    else if (ext == L"cif") load = load_cif;
    else if (ext == L"fin") load = load_fin;
    else if (ext == L"ins") load = load_ins;
    else if (ext == L"sbi") load = load_sbi;
    else if (ext == L"sgi") load = load_sgi;
    if (load == nullptr) {
        error = L"Unsupported file type";
        return false;
    }

    std::vector<uint8_t> file;
    if (!readFile(path, file)) {
        error = L"Cannot open the file";
        return false;
    }

    if (!load(path, file, songdata, current_inst, error)) return false;
    filter_loaded_names(songdata);
    return true;
}

bool ReadFileBytes(const std::wstring& path, std::vector<uint8_t>& data)
{
    return readFile(path, data);
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

bool LoadA2bBank(const std::wstring& path, FixedSongData& temp_songdata, std::wstring& error)
{
    const auto corrupted = [&error] {
        error = L"The file is corrupted";
        return false;
    };
    const auto crc_failed = [&error] {
        error = L"CRC mismatch";
        return false;
    };

    std::vector<uint8_t> file;
    if (!readFile(path, file)) {
        error = L"Cannot open the file";
        return false;
    }

    Reader reader(file);
    std::vector<uint8_t> buf1;
    std::vector<uint8_t> buf2;

    A2bOldHeader header{};
    if (reader.read(&header, sizeof(header)) != sizeof(header) || std::memcmp(header.ident, A2B_ID, 11) != 0) {
        return corrupted();
    }
    if (header.ffver < 1 || header.ffver > FFVER_A2B) {
        error = L"Unsupported format version (" + std::to_wstring(header.ffver) + L")";
        return false;
    }

    if (header.ffver <= 8) {
        auto old_songdata = std::make_unique<OldFixedSongData>();
        init_old_songdata(*old_songdata);
        if (!reader.readInto(buf1, header.b0len)) return corrupted();
        uint32_t crc = 0xffffffff;
        crc = Update32(&header.b0len, 2, crc);
        crc = Update32(buf1.data(), header.b0len, crc);
        if (crc != header.crc32) return crc_failed();

        // 形式 1..4 と 5..8 は同じ並び (展開方法が 4..1 / 8..5 の順に Move / LZSS / LZW / SixPack)
        decompressOld(static_cast<uint8_t>(header.ffver <= 4 ? 4 - header.ffver : 8 - header.ffver), buf1, header.b0len, buf2);
        copyToField(*old_songdata, old_songdata->instr_names, buf2);
        if (header.ffver <= 4) {
            for (int temp = 1; temp <= 250; ++temp) at1(old_songdata->instr_data, temp).panning = 0;
        }

        // import_old_instruments (楽器 1..250)
        for (int temp = 1; temp <= 250; ++temp) {
            auto& name = at1(temp_songdata.instr_names, temp);
            name.assign(pcopy(name.str(), 1, 9) + pcopy(at1(old_songdata->instr_names, temp).str(), 10, 22));
            Instrument& ins = at1(temp_songdata.instr_data, temp);
            const OldInstrument& old = at1(old_songdata->instr_data, temp);
            ins.fm_data = old.fm_data;
            ins.panning = old.panning;
            ins.fine_tune = old.fine_tune;
            ins.perc_voice = 0;
        }
    } else {
        reader.seek(0);
        A2bHeader header2{};
        if (reader.read(&header2, sizeof(header2)) != sizeof(header2) || std::memcmp(header2.ident, A2B_ID, 11) != 0) {
            return corrupted();
        }
        if (!reader.readInto(buf1, header2.b0len)) return corrupted();
        uint32_t crc = 0xffffffff;
        crc = Update32(&header2.b0len, 2, crc);
        crc = Update32(buf1.data(), header2.b0len, crc);
        if (crc != header2.crc32) return crc_failed();

        char temp_marks[256];
        save_instr_marks(temp_songdata, temp_marks);
        if (header.ffver == 9) {
            APACK_decompress(buf1.data(), header2.b0len, buf2);
            copyToField(temp_songdata, temp_songdata.instr_names, buf2);
        } else {
            // 形式 10 (FFVER_A2B): 楽器名・楽器データ・4OP の組
            LZH_decompress(buf1.data(), header2.b0len, header2.b0len, buf2);
            const size_t size = sizeof(temp_songdata.instr_names) + sizeof(temp_songdata.instr_data);
            move_from(buf2, 0, temp_songdata.instr_names, size);
            move_from(buf2, size, &temp_songdata.ins_4op_flags, sizeof(temp_songdata.ins_4op_flags));
        }
        restore_instr_names(temp_songdata, temp_marks);
    }

    std::memset(temp_songdata.dis_fmreg_col, 0, sizeof(temp_songdata.dis_fmreg_col));
    return true;
}

bool LoadA2wBank(const std::wstring& path, FixedSongData& temp_songdata, std::wstring& error)
{
    const auto corrupted = [&error] {
        error = L"The file is corrupted";
        return false;
    };
    const auto crc_failed = [&error] {
        error = L"CRC mismatch";
        return false;
    };

    std::vector<uint8_t> file;
    if (!readFile(path, file)) {
        error = L"Cannot open the file";
        return false;
    }

    Reader reader(file);
    std::vector<uint8_t> buf1;
    std::vector<uint8_t> buf2;
    char temp_marks[256];

    A2wHeader header{};
    if (reader.read(&header, sizeof(header)) != sizeof(header) || std::memcmp(header.ident, A2W_ID, 20) != 0) {
        return corrupted();
    }
    if (header.ffver < 1 || header.ffver > FFVER_A2W) {
        error = L"Unsupported format version (" + std::to_wstring(header.ffver) + L")";
        return false;
    }

    if (header.ffver == 1) {
        reader.seek(0);
        A2wOldHeader header2{};
        if (reader.read(&header2, sizeof(header2)) != sizeof(header2) || std::memcmp(header2.ident, A2W_ID, 20) != 0) {
            return corrupted();
        }
        uint32_t crc = 0xffffffff;
        if (!reader.readInto(buf1, header2.b0len)) return corrupted();
        crc = Update32(buf1.data(), header2.b0len, crc);
        if (!reader.readInto(buf1, header2.b1len)) return corrupted();
        crc = Update32(buf1.data(), header2.b1len, crc);
        crc = Update32(&header2.b0len, 2, crc);
        crc = Update32(&header2.b1len, 2, crc);
        if (crc != header2.crc32) return crc_failed();

        reader.seek(sizeof(header2));
        if (!reader.readInto(buf1, header2.b0len)) return corrupted();
        save_instr_marks(temp_songdata, temp_marks);
        APACK_decompress(buf1.data(), header2.b0len, buf2);
        copyToField(temp_songdata, temp_songdata.instr_names, buf2);
        restore_instr_names(temp_songdata, temp_marks);

        if (!reader.readInto(buf1, header2.b1len)) return corrupted();
        APACK_decompress(buf1.data(), header2.b1len, buf2);
        copyToField(temp_songdata, temp_songdata.macro_table, buf2);
        std::memset(temp_songdata.dis_fmreg_col, 0, sizeof(temp_songdata.dis_fmreg_col));
        return true;
    }

    // 形式 2, 3
    uint32_t crc = 0xffffffff;
    if (!reader.readInto(buf1, header.b0len)) return corrupted();
    crc = Update32(buf1.data(), header.b0len, crc);
    if (!reader.readInto(buf1, header.b1len)) return corrupted();
    crc = Update32(buf1.data(), header.b1len, crc);
    if (!reader.readInto(buf1, header.b2len)) return corrupted();
    crc = Update32(buf1.data(), header.b2len, crc);
    crc = Update32(&header.b0len, 2, crc);
    crc = Update32(&header.b1len, 2, crc);
    crc = Update32(&header.b2len, 2, crc);
    if (crc != header.crc32) return crc_failed();

    reader.seek(sizeof(header));
    if (!reader.readInto(buf1, header.b0len)) return corrupted();
    save_instr_marks(temp_songdata, temp_marks);
    if (header.ffver == 2) {
        APACK_decompress(buf1.data(), header.b0len, buf2);
        copyToField(temp_songdata, temp_songdata.instr_names, buf2);
    } else {
        // 形式 3 (FFVER_A2W): 楽器名・楽器データ・FM マクロ・4OP の組
        LZH_decompress(buf1.data(), header.b0len, header.b0len, buf2);
        const size_t size = sizeof(temp_songdata.instr_names) + sizeof(temp_songdata.instr_data) + sizeof(temp_songdata.instr_macros);
        move_from(buf2, 0, temp_songdata.instr_names, size);
        move_from(buf2, size, &temp_songdata.ins_4op_flags, sizeof(temp_songdata.ins_4op_flags));
    }
    restore_instr_names(temp_songdata, temp_marks);

    if (!reader.readInto(buf1, header.b1len)) return corrupted();
    if (header.ffver == 2) APACK_decompress(buf1.data(), header.b1len, buf2);
    else LZH_decompress(buf1.data(), header.b1len, header.b1len, buf2);
    copyToField(temp_songdata, temp_songdata.macro_table, buf2);

    if (!reader.readInto(buf1, header.b2len)) return corrupted();
    if (header.ffver == 2) APACK_decompress(buf1.data(), header.b2len, buf2);
    else LZH_decompress(buf1.data(), header.b2len, header.b2len, buf2);
    copyToField(temp_songdata, temp_songdata.dis_fmreg_col, buf2);
    return true;
}

SaveResult SaveA2b(const std::wstring& path, FixedSongData& songdata)
{
    Writer f(path);
    if (!f.is_open()) return SaveResult::WriteError;

    A2bHeader header{};
    std::memcpy(header.ident, A2B_ID, 11);
    header.ffver = FFVER_A2B;
    if (!f.write(&header, sizeof(header))) return f.fail();

    char temp_marks[256];
    strip_instr_names(songdata, temp_marks);
    std::vector<uint8_t> buf2;
    append(buf2, songdata.instr_names, sizeof(songdata.instr_names) + sizeof(songdata.instr_data));
    append(buf2, &songdata.ins_4op_flags, sizeof(songdata.ins_4op_flags));
    std::vector<uint8_t> buf1;
    const size_t temp2 = LZH_compress(buf2.data(), static_cast<uint32_t>(buf2.size()), buf1);
    restore_instr_names(songdata, temp_marks);

    if (!f.write(buf1.data(), temp2)) return f.fail();

    header.b0len = static_cast<uint32_t>(temp2);
    uint32_t crc = 0xffffffff;
    crc = Update32(&header.b0len, 2, crc);
    crc = Update32(buf1.data(), header.b0len, crc);
    header.crc32 = crc;
    if (!f.write_at(0, &header, sizeof(header))) return f.fail();
    return SaveResult::Ok;
}

SaveResult SaveA2w(const std::wstring& path, FixedSongData& songdata, const SaveProgress* progress)
{
    Writer f(path);
    if (!f.is_open()) return SaveResult::WriteError;

    if (progress && progress->begin) progress->begin(3);
    const auto step = [progress] {
        if (progress && progress->step) progress->step();
    };
    static const LzhProgress no_progress;
    const LzhProgress& value = progress ? progress->value : no_progress;

    A2wHeader header{};
    header.crc32 = 0xffffffff;
    std::memcpy(header.ident, A2W_ID, 20);
    header.ffver = FFVER_A2W;
    if (!f.write(&header, sizeof(header))) return f.fail();

    char temp_marks[256];
    strip_instr_names(songdata, temp_marks);
    std::vector<uint8_t> buf2;
    append(buf2, songdata.instr_names,
           sizeof(songdata.instr_names) + sizeof(songdata.instr_data) + sizeof(songdata.instr_macros));
    append(buf2, &songdata.ins_4op_flags, sizeof(songdata.ins_4op_flags));
    std::vector<uint8_t> buf1;
    header.b0len = static_cast<uint32_t>(LZH_compress(buf2.data(), static_cast<uint32_t>(buf2.size()), buf1, value));
    step();
    restore_instr_names(songdata, temp_marks);
    if (!f.write(buf1.data(), header.b0len)) return f.fail();
    header.crc32 = Update32(buf1.data(), header.b0len, header.crc32);

    header.b1len = static_cast<uint32_t>(LZH_compress(reinterpret_cast<const uint8_t*>(songdata.macro_table),
                                                      sizeof(songdata.macro_table), buf1, value));
    step();
    if (!f.write(buf1.data(), header.b1len)) return f.fail();
    header.crc32 = Update32(buf1.data(), header.b1len, header.crc32);

    header.b2len = static_cast<uint32_t>(LZH_compress(reinterpret_cast<const uint8_t*>(songdata.dis_fmreg_col),
                                                      sizeof(songdata.dis_fmreg_col), buf1, value));
    step();
    if (!f.write(buf1.data(), header.b2len)) return f.fail();
    header.crc32 = Update32(buf1.data(), header.b2len, header.crc32);

    header.crc32 = Update32(&header.b0len, 2, header.crc32);
    header.crc32 = Update32(&header.b1len, 2, header.crc32);
    header.crc32 = Update32(&header.b2len, 2, header.crc32);
    if (!f.write_at(0, &header, sizeof(header))) return f.fail();
    return SaveResult::Ok;
}

} // namespace a2ins
