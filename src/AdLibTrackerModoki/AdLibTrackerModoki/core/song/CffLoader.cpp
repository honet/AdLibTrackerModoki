#include "TrackerLoaders.h"

#include <set>
#include <vector>

#include "EffectDefs.h"
#include "LoaderCommon.h"

using namespace loader;

namespace
{
#pragma pack(push, 1)
    struct Header   // tHEADER
    {
        char ident[16];
        uint8_t versn;
        uint16_t fsize;      // ファイルサイズ - 32
        uint8_t cflag;       // 1: 圧縮されている
        uint8_t resrv[12];
    };

    struct InsDat   // tINSDAT
    {
        uint8_t iData[12];
        char iName[20];
    };

    struct Headr2   // tHEADR2
    {
        InsDat instr[47];
        uint8_t nopat;
        char ascii[31];
        char writr[20];
        char sname[20];
        uint8_t order[65];
    };
#pragma pack(pop)

    static_assert(sizeof(Header) == 32);
    static_assert(sizeof(Headr2) == 1641);

    constexpr int32_t _PRE_ASCII_BLAB_SIZE = 0x5e1;   // SizeOf(tHEADR2.instr) + SizeOf(tHEADR2.nopat)
    const char cff_id[] = "<CUD-FM-File>\x1A\xDE\xE0";
    const std::string ascii_blab = "CUD-FM-File - SEND A POSTCARD -";

    // tPATDAT: [0..$24][0..$3f][1..9][0..2] of Byte
    constexpr size_t PATDAT_SIZE = 0x25 * 64 * 9 * 3;

    uint8_t dec2hex(uint8_t dec) { return static_cast<uint8_t>((dec / 10) * 16 + dec % 10); }

    // LZTYR_decompress (iloaders.inc)。出力は output の末尾へ追加していく。
    class Lztyr
    {
    public:
        Lztyr(const uint8_t* input, size_t inputSize) : input_(input), inputSize_(inputSize), buf3_(0x10000 + 256, 0), dict_(0x10000, 0) {}

        size_t decompress(std::vector<uint8_t>& output)
        {
            out_ = &output;
            input_idx_ = 0;
            cleanup();
            startup();

            for (;;) {
                // 壊れたデータで出力が際限なく増えないようにする (オリジナルはメモリを壊して止まる)
                if (out_->size() > MAX_OUTPUT) break;
                const uint32_t new_code = get_code();
                if (new_code == 0) break;   // end of data
                if (new_code == 1) {        // end of block
                    cleanup();
                    startup();
                    continue;
                }
                if (new_code == 2) {        // expand code length
                    ++code_length_;
                    continue;
                }
                if (new_code == 3) {        // RLE
                    const uint8_t old_code_length = code_length_;
                    code_length_ = 2;
                    const uint8_t repeat_length = static_cast<uint8_t>(get_code() + 1);
                    code_length_ = static_cast<uint8_t>(4 << get_code());
                    const uint32_t repeat_counter = get_code();
                    for (uint64_t idx = 0; idx < static_cast<uint64_t>(repeat_counter) * repeat_length && out_->size() <= MAX_OUTPUT; ++idx) {
                        const int64_t from = static_cast<int64_t>(out_->size()) - repeat_length;
                        out_->push_back(from >= 0 ? (*out_)[static_cast<size_t>(from)] : 0);
                    }
                    code_length_ = old_code_length;
                    startup();
                    continue;
                }

                if (new_code >= 0x104u + dict_length_) {
                    ++the_string_[0];
                    the_string_[the_string_[0]] = the_string_[1];
                } else {
                    uint8_t temp_string[256];
                    translate_code(new_code, temp_string);
                    ++the_string_[0];
                    the_string_[the_string_[0]] = temp_string[1];
                }

                expand_dictionary(the_string_);
                translate_code(new_code, the_string_);
                for (int idx = 0; idx < the_string_[0]; ++idx) out_->push_back(the_string_[idx + 1]);
            }
            return out_->size();
        }

    private:
        static constexpr size_t MAX_OUTPUT = GENERIC_IO_BUFFER_SIZE;
        const uint8_t* input_;
        size_t inputSize_;
        size_t input_idx_ = 0;
        std::vector<uint8_t>* out_ = nullptr;
        std::vector<uint8_t> buf3_;     // 辞書の文字列を置くヒープ (オリジナルの buf3)
        std::vector<uint16_t> dict_;    // _cff_dictionary (buf3 内の位置)
        uint8_t the_string_[256] = {};
        uint8_t code_length_ = 9;
        uint32_t bits_buffer_ = 0;
        uint16_t bits_left_ = 0;
        uint16_t heap_length_ = 0;
        uint16_t dict_length_ = 0;

        uint8_t in(size_t i) const { return i < inputSize_ ? input_[i] : 0; }

        uint32_t get_code()
        {
            while (bits_left_ < code_length_) {
                bits_buffer_ |= static_cast<uint32_t>(in(input_idx_)) << bits_left_;
                ++input_idx_;
                bits_left_ = static_cast<uint16_t>(bits_left_ + 8);
            }
            const uint32_t code = code_length_ >= 32 ? bits_buffer_ : bits_buffer_ & ((1u << code_length_) - 1);
            bits_buffer_ = code_length_ >= 32 ? 0 : bits_buffer_ >> code_length_;
            bits_left_ = static_cast<uint16_t>(bits_left_ - code_length_);
            return code;
        }

        void translate_code(uint32_t code, uint8_t* str)
        {
            uint8_t translated[256] = {};
            if (code >= 0x104) {
                const uint16_t pos = dict_[(code - 0x104) & 0xffff];
                std::memcpy(translated, &buf3_[pos], buf3_[pos] + 1);
            } else {
                translated[0] = 1;
                translated[1] = static_cast<uint8_t>((code - 4) & 0xff);
            }
            std::memcpy(str, translated, 256);
        }

        void startup()
        {
            const uint32_t old_code = get_code();
            translate_code(old_code, the_string_);
            for (int idx = 0; idx < the_string_[0]; ++idx) out_->push_back(the_string_[idx + 1]);
        }

        void cleanup()
        {
            code_length_ = 9;
            bits_buffer_ = 0;
            bits_left_ = 0;
            heap_length_ = 0;
            dict_length_ = 0;
        }

        void expand_dictionary(const uint8_t* str)
        {
            if (str[0] >= 0xf0) return;
            std::memcpy(&buf3_[heap_length_], str, str[0] + 1);
            dict_[dict_length_] = heap_length_;
            ++dict_length_;
            heap_length_ = static_cast<uint16_t>(heap_length_ + str[0] + 1);
        }
    };

    void import_cff_event(Song& song, uint8_t patt, uint8_t line, uint8_t chan, uint8_t byte0, uint8_t byte1, uint8_t byte2)
    {
        Chunk chunk{};
        uint8_t temp1 = byte2;
        const uint8_t temp2 = temp1 / 16;
        const uint8_t temp3 = temp1 % 16;

        switch (static_cast<char>(byte1)) {
        case 'A':   // SET SPEED
            if (temp1 > 0) { chunk.effect_def = ef_SetSpeed; chunk.effect = temp1; }
            break;
        case 'B':   // SET CARRIER WAVEFORM
            if (temp1 < 4) { chunk.effect_def = ef_SetWaveform; chunk.effect = static_cast<uint8_t>(temp1 * 16); }
            break;
        case 'C':   // SET MODULATOR VOLUME
            chunk.effect_def = ef_SetModulatorVol;
            chunk.effect = temp1 < 64 ? static_cast<uint8_t>(63 - temp1) : 0;
            break;
        case 'D':   // VOLUME SLIDE UP/DOWN
            chunk.effect_def = ef_VolSlide;
            chunk.effect = temp1;
            break;
        case 'E':   // SLIDE DOWN
            if (temp1 != 0) { chunk.effect_def = ef_FSlideDown; chunk.effect = temp1; }
            break;
        case 'F':   // SLIDE UP
            if (temp1 != 0) { chunk.effect_def = ef_FSlideUp; chunk.effect = temp1; }
            break;
        case 'G':   // SET CARRIER VOLUME
            chunk.effect_def = ef_SetCarrierVol;
            chunk.effect = temp1 < 64 ? static_cast<uint8_t>(63 - temp1) : 0;
            break;
        case 'H':   // SET TEMPO
            if (temp1 > 0) {
                chunk.effect_def = ef_SetTempo;
                if (!(temp1 > 21)) temp1 = 125;
                // temp4 は Byte のため、オリジナル同様に下位 8bit に切り詰める
                const uint8_t temp4 = static_cast<uint8_t>(1412926 / static_cast<int32_t>(temp1 >> 1));
                chunk.effect = 1;
                while (1193180 / chunk.effect > temp4 && chunk.effect < 255) ++chunk.effect;
            }
            break;
        case 'I':   // SET INSTRUMENT
            if (temp1 < 47) {
                chunk.effect_def = ef_Extended;
                chunk.effect = ef_ex_ExtendedCmd2 * 16 + ef_ex_cmd2_ResetVol;
                chunk.instr_def = static_cast<uint8_t>(temp1 + 1);
            }
            break;
        case 'J':   // ARPEGGIO
            chunk.effect_def = ef_Arpeggio;
            chunk.effect = temp1;
            break;
        case 'K':   // JUMP TO ORDER
            if (temp1 < 128) { chunk.effect_def = ef_PositionJump; chunk.effect = temp1; }
            break;
        case 'L':   // JUMP TO NEXT PATTERN IN ORDER
            chunk.effect_def = ef_PatternBreak;
            break;
        case 'M':   // SET TREMOLO HIGHER / SET VIBRATO DEEPER
            chunk.effect_def = ef_Extended;
            if (temp2 == 1 && temp3 == 0) chunk.effect = dec2hex(1);
            if (temp2 == 0 && temp3 == 1) chunk.effect = dec2hex(10);
            if (temp2 == 1 && temp3 == 1) chunk.effect = dec2hex(11);
            break;
        }

        if (byte0 >= 1 && byte0 <= 12 * 8 + 1) {
            // REGULAR NOTE
            if (!fix_c_note_bug) {
                chunk.note = byte0;
            } else {
                chunk.note = static_cast<uint8_t>(byte0 + 1);
                if (chunk.note > 12 * 8 + 1) chunk.note = 12 * 8 + 1;
            }
        } else if (byte0 == 0x6d) {
            // PAUSE
            chunk.note = BYTE_NULL;
        }

        song.putChunk(patt, line, chan, chunk);
    }

    void import_cff_patterns(Song& song, const uint8_t* data, uint8_t patterns)
    {
        FixedSongData& songdata = song.data();
        const auto ev = [data](int patt, int line, int chan, int k) { return data[((patt * 64 + line) * 9 + (chan - 1)) * 3 + k]; };

        uint8_t voice[10], arpgg[10] = {};
        for (int temp = 1; temp <= 9; ++temp) voice[temp] = accurate_conv ? 0 : static_cast<uint8_t>(temp);

        for (int temp = 0; temp <= 0x24; ++temp) {
            for (int temp2 = 0; temp2 <= 0x3f; ++temp2) {
                for (int temp3 = 1; temp3 <= 9; ++temp3) {
                    if (!(ev(temp, temp2, temp3, 0) == 0 && ev(temp, temp2, temp3, 1) == 0 && ev(temp, temp2, temp3, 2) == 0)) {
                        import_cff_event(song, static_cast<uint8_t>(temp), static_cast<uint8_t>(temp2), static_cast<uint8_t>(temp3),
                                         ev(temp, temp2, temp3, 0), ev(temp, temp2, temp3, 1), ev(temp, temp2, temp3, 2));
                    }
                }
            }
        }

        std::set<uint8_t> patts;
        int order = 0;
        uint8_t patt = BYTE_NULL;
        do {
            if (songdata.pattern_order[order] > 0x24) {
                ++order;
            } else {
                patt = songdata.pattern_order[order];
                uint8_t patt_break = BYTE_NULL;
                for (int temp2 = 0; temp2 <= 0x3f; ++temp2) {
                    for (int temp3 = 1; temp3 <= 9; ++temp3) {
                        Chunk chunk{};
                        song.getChunk(patt, static_cast<uint8_t>(temp2), static_cast<uint8_t>(temp3), chunk);
                        const uint8_t temp4 = ev(patt, temp2, temp3, 2);

                        switch (static_cast<char>(ev(patt, temp2, temp3, 1))) {
                        case 'C':   // SET MODULATOR VOLUME
                        case 'G':   // SET CARRIER VOLUME
                            if (chunk.instr_def == 0 && !accurate_conv) chunk.instr_def = voice[temp3];
                            else if (chunk.instr_def == 0 && voice[temp3] == 0) chunk.instr_def = static_cast<uint8_t>(temp3);
                            break;
                        case 'I':   // SET INSTRUMENT
                            if (temp4 < 47 && temp2 != patt_break) {
                                voice[temp3] = static_cast<uint8_t>(temp4 + 1);
                                if (!accurate_conv) chunk.instr_def = voice[temp3];
                            }
                            break;
                        case 'J':   // ARPEGGIO
                            chunk.effect_def = ef_Arpeggio;
                            if (temp4 != 0) {
                                chunk.effect = temp4;
                                arpgg[temp3] = temp4;
                            } else {
                                chunk.effect = arpgg[temp3];
                            }
                            break;
                        case 'K':   // JUMP TO ORDER
                            if (temp4 < 128) patt_break = static_cast<uint8_t>(temp2 + 1);
                            break;
                        case 'L':   // JUMP TO NEXT PATTERN IN ORDER
                            patt_break = static_cast<uint8_t>(temp2 + 1);
                            break;
                        }

                        const uint8_t note = ev(patt, temp2, temp3, 0);
                        if (note >= 1 && note <= 12 * 8 + 1) {
                            // REGULAR NOTE
                            if (accurate_conv && voice[temp3] == 0) {
                                voice[temp3] = static_cast<uint8_t>(temp3);
                                chunk.instr_def = voice[temp3];
                            }
                            if (!accurate_conv) chunk.instr_def = voice[temp3];
                        }

                        if (patts.count(songdata.pattern_order[order]) == 0) {
                            song.putChunk(patt, static_cast<uint8_t>(temp2), static_cast<uint8_t>(temp3), chunk);
                        }
                    }
                }
                ++order;
                patts.insert(patt);
            }
        } while (!(patt >= patterns || order > 0x40));
    }
}

bool LoadCff(const std::wstring& path, Song& song, const Song::Defaults& defaults, std::wstring& error)
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
    if (file.size() < sizeof(header) || file.size() > GENERIC_IO_BUFFER_SIZE) return corrupted();
    std::memcpy(&header, file.data(), sizeof(header));
    if (std::memcmp(header.ident, cff_id, 16) != 0) return corrupted();

    Headr2 headr2{};
    std::vector<uint8_t> buf1;   // パターンデータ (tPATDAT)
    if (header.cflag == 1) {
        std::vector<uint8_t> input(file);
        input.resize(GENERIC_IO_BUFFER_SIZE, 0);
        std::vector<uint8_t> hash_buffer;
        hash_buffer.reserve(0x20000);
        Lztyr lztyr(input.data() + 0x30, input.size() - 0x30);
        const size_t out_size = lztyr.decompress(hash_buffer);

        const int32_t offs = SensitiveScan(hash_buffer.data(), 0, static_cast<int32_t>(out_size), ascii_blab);
        if (offs != _PRE_ASCII_BLAB_SIZE) {
            error = L"データの展開に失敗しました";
            return false;
        }
        hash_buffer.resize(std::max(hash_buffer.size(), sizeof(Headr2)), 0);
        std::memcpy(&headr2, hash_buffer.data(), sizeof(headr2));
        buf1.assign(hash_buffer.begin() + sizeof(Headr2), hash_buffer.end());
    } else {
        if (file.size() < sizeof(header) + sizeof(headr2)) return corrupted();
        std::memcpy(&headr2, file.data() + sizeof(header), sizeof(headr2));
        if (char_array(headr2.ascii) != ascii_blab) return corrupted();
        buf1.assign(file.begin() + sizeof(header) + sizeof(headr2), file.end());
    }
    if (buf1.size() < PATDAT_SIZE) buf1.resize(PATDAT_SIZE, 0);

    song.init(defaults);
    FixedSongData& songdata = song.data();

    songdata.patt_len = 64;
    songdata.nm_tracks = 9;   // adjust_tracks = TRUE
    songdata.tempo = 51;
    songdata.speed = 6;

    for (int temp2 = 0; temp2 <= 64; ++temp2) {
        if (headr2.order[temp2] <= headr2.nopat) songdata.pattern_order[temp2] = headr2.order[temp2];
    }

    for (int temp2 = 0; temp2 <= 46; ++temp2) {
        import_hsc_instrument(songdata, static_cast<uint8_t>(temp2 + 1), headr2.instr[temp2].iData);
        at1(songdata.instr_data, temp2 + 1).fine_tune = 0;
        auto& name = at1(songdata.instr_names, temp2 + 1);
        name.assign(pcopy(name.str(), 1, 9) + truncate_string(char_array(headr2.instr[temp2].iName)));
    }

    songdata.common_flag = songdata.common_flag | 2;
    songdata.songname.assign(CutStr(char_array(headr2.sname)));
    songdata.composer.assign(CutStr(char_array(headr2.writr)));
    import_old_flags(songdata);

    import_cff_patterns(song, buf1.data(), headr2.nopat);
    return true;
}
