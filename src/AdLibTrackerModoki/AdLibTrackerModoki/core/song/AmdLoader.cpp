#include "TrackerLoaders.h"

#include <vector>

#include "EffectDefs.h"
#include "LoaderCommon.h"

using namespace loader;

namespace
{
#pragma pack(push, 1)
    struct InsDat   // tINSDAT
    {
        char iName[23];
        uint8_t iData[11];
    };

    struct Header   // tHEADER
    {
        char sname[24];      // ASCIIZ
        char aname[24];      // ASCIIZ
        InsDat instr[26];
        uint8_t snlen;       // 曲の長さ
        uint8_t nopat;       // パターン数 - 1
        uint8_t order[128];
        char ident[9];
        uint8_t versn;       // 10h: 通常、11h: パック形式
    };
#pragma pack(pop)

    static_assert(sizeof(Header) == 1072);

    const char id_amd[] = "<o\xEFQU\xEERoR";
    const char id_xms[] = "MaDoKaN96";

    uint8_t dec2hex(uint8_t dec) { return static_cast<uint8_t>((dec / 10) * 16 + dec % 10); }

    void import_amd_instrument(FixedSongData& songdata, uint8_t inst, const uint8_t* data)
    {
        Instrument& ins = at1(songdata.instr_data, inst);
        ins.fm_data.AM_VIB_EG_modulator = data[0];
        ins.fm_data.KSL_VOLUM_modulator = data[1];
        ins.fm_data.ATTCK_DEC_modulator = data[2];
        ins.fm_data.SUSTN_REL_modulator = data[3];
        ins.fm_data.WAVEFORM_modulator = data[4] & 3;
        ins.fm_data.AM_VIB_EG_carrier = data[5];
        ins.fm_data.KSL_VOLUM_carrier = data[6];
        ins.fm_data.ATTCK_DEC_carrier = data[7];
        ins.fm_data.SUSTN_REL_carrier = data[8];
        ins.fm_data.WAVEFORM_carrier = data[9] & 3;
        ins.fm_data.FEEDBACK_FM = data[10] & 0x0f;
        ins.panning = 0;
        ins.fine_tune = 0;
    }

    void import_amd_event(Song& song, uint8_t pattern, uint8_t line, uint8_t channel, uint8_t byte1, uint8_t byte2, uint8_t byte3)
    {
        Chunk chunk{};
        const uint8_t ins = static_cast<uint8_t>((byte2 >> 4) + ((byte1 & 1) << 4));
        if (ins != 0) chunk.instr_def = ins;

        if ((byte1 >> 4) >= 1 && (byte1 >> 4) <= 12) chunk.note = static_cast<uint8_t>(12 * ((byte1 >> 1) & 7) + (byte1 >> 4));

        const uint8_t param = byte3 & 0x7f;
        switch (byte2 & 0x0f) {
        case 0x00:   // ARPEGGIO
            chunk.effect_def = ef_Arpeggio;
            chunk.effect = dec2hex(param);
            break;
        case 0x01:   // SLIDE FREQUENCY UP
            chunk.effect_def = ef_FSlideUp;
            chunk.effect = param;
            break;
        case 0x02:   // SLIDE FREQUENCY DOWN
            chunk.effect_def = ef_FSlideDown;
            chunk.effect = param;
            break;
        case 0x03:   // SET CARRIER/MODULATOR INTENSITY
            if (param / 10 >= 1 && param / 10 <= 9) {
                chunk.effect_def = ef_SetCarrierVol;
                chunk.effect = static_cast<uint8_t>((param / 10) * 7);
            } else if (param % 10 >= 1 && param % 10 <= 9) {
                chunk.effect_def = ef_SetModulatorVol;
                chunk.effect = static_cast<uint8_t>((param % 10) * 7);
            }
            break;
        case 0x04:   // SET THE VOLUME
            chunk.effect_def = ef_SetInsVolume;
            chunk.effect = param < 64 ? param : 63;
            break;
        case 0x05:   // JUMP INTO PATTERN
            chunk.effect_def = ef_PositionJump;
            chunk.effect = param < 100 ? param : 99;
            break;
        case 0x06:   // PATTERNBREAK
            chunk.effect_def = ef_PatternBreak;
            chunk.effect = param < 64 ? param : 63;
            break;
        case 0x07:   // SET SONGSPEED
            if (param < 99) {
                if (param >= 1 && param <= 31) {
                    chunk.effect_def = ef_SetSpeed;
                    chunk.effect = param;
                } else {
                    chunk.effect_def = ef_SetTempo;
                    chunk.effect = param == 0 ? 18 : param;
                }
            }
            break;
        case 0x08:   // TONEPORTAMENTO
            chunk.effect_def = ef_TonePortamento;
            chunk.effect = param;
            break;
        case 0x09:   // EXTENDED COMMAND
            if (param < 60) {
                switch (param / 10) {
                case 0:   // DEFINE CELL-TREMOLO
                    if (param % 10 < 2) {
                        chunk.effect_def = ef_Extended;
                        chunk.effect = dec2hex(param);
                    }
                    break;
                case 1:   // DEFINE CELL-VIBRATO
                    if (param % 10 < 2) {
                        chunk.effect_def = ef_Extended;
                        chunk.effect = static_cast<uint8_t>(0x10 + dec2hex(param));
                    }
                    break;
                case 2:   // INCREASE VOLUME FAST
                    chunk.effect_def = ef_VolSlide;
                    chunk.effect = static_cast<uint8_t>((param % 10) * 16);
                    break;
                case 3:   // DECREASE VOLUME FAST
                    chunk.effect_def = ef_VolSlide;
                    chunk.effect = param % 10;
                    break;
                case 4:   // INCREASE VOLUME FINE
                    chunk.effect_def = ef_Extended2;
                    chunk.effect = static_cast<uint8_t>(ef_ex2_VolSlideUpXF * 16 + param % 10);
                    break;
                case 5:   // DECREASE VOLUME FINE
                    chunk.effect_def = ef_Extended2;
                    chunk.effect = static_cast<uint8_t>(ef_ex2_VolSlideDnXF * 16 + param % 10);
                    break;
                }
            }
            break;
        }

        // specific corrections for Amusic event
        if (chunk.note == 0) chunk.instr_def = 0;
        song.putChunk(pattern, line, channel, chunk);
    }

    void import_amd_packed_patterns(Song& song, const std::vector<uint8_t>& data, uint8_t patterns)
    {
        // data の範囲外 (Word の添字で 64KB まで) は 0 として読む
        const auto b = [&data](uint32_t i) -> uint8_t { return i < data.size() ? data[i] : 0; };

        // track_order: [0..$3f][1..9] of Word。オリジナルは patterns が 63 を超えると後続の変数へはみ出すが、
        // ここでは 256 パターン分の領域を確保して、ファイルの内容をそのまま読む
        uint16_t track_order[256][10] = {};
        uint16_t temp = static_cast<uint16_t>((patterns + 1) * 9 * 2);
        for (int p = 0; p <= patterns; ++p) {
            for (int c = 1; c <= 9; ++c) {
                const uint32_t i = static_cast<uint32_t>((p * 9 + (c - 1)) * 2);
                track_order[p][c] = static_cast<uint16_t>(b(i) + (b(i + 1) << 8));
            }
        }

        const uint16_t tracks = static_cast<uint16_t>(b(temp) + (b(temp + 1u) << 8));
        temp = static_cast<uint16_t>(temp + 2);

        uint16_t temp2 = 0, temp3 = 0, temp4 = 0;
        uint8_t count = 0;
        do {
            if (count == 0) {
                if (temp3 == 0) {
                    temp2 = static_cast<uint16_t>(b(temp) + (b(temp + 1u) << 8));
                    temp = static_cast<uint16_t>(temp + 2);
                }

                if ((b(temp) | 0x80) != b(temp)) {
                    if (temp2 / 9 <= 0x3f && temp2 % 9 < 9) {
                        import_amd_event(song, static_cast<uint8_t>(temp2 / 9), static_cast<uint8_t>(temp3),
                                         static_cast<uint8_t>(temp2 % 9 + 1), b(temp + 2u), b(temp + 1u), b(temp));
                    }
                    temp = static_cast<uint16_t>(temp + 3);
                } else {
                    count = static_cast<uint8_t>((b(temp) & 0x7f) - 1);
                    temp = static_cast<uint16_t>(temp + 1);
                }
            } else {
                --count;
            }

            ++temp3;
            if (temp3 > 0x3f) {
                temp3 = 0;
                count = 0;
                ++temp4;
            }
        } while (temp4 < tracks);

        Chunk track[64];
        for (int p = 0; p <= patterns; ++p) {
            for (int c = 1; c <= 9; ++c) {
                const uint16_t t = track_order[p][c];
                const uint16_t src = t / 9;
                if (t < 64 * 9) {
                    for (int line = 0; line <= 0x3f; ++line) {
                        song.getChunk(static_cast<uint8_t>(src), static_cast<uint8_t>(line), static_cast<uint8_t>(t % 9 + 1), track[line]);
                    }
                    for (int line = 0; line <= 0x3f; ++line) {
                        song.putChunk(static_cast<uint8_t>(p), static_cast<uint8_t>(line), static_cast<uint8_t>(c), track[line]);
                    }
                }
            }
        }
    }
}

bool LoadAmd(const std::wstring& path, Song& song, const Song::Defaults& defaults, std::wstring& error)
{
    std::vector<uint8_t> file;
    if (!readFile(path, file)) {
        error = L"ファイルを開けません";
        return false;
    }

    Header header{};
    if (file.size() < sizeof(header)) {
        error = L"ファイルが壊れています";
        return false;
    }
    std::memcpy(&header, file.data(), sizeof(header));
    if (std::memcmp(header.ident, id_amd, 9) != 0 && std::memcmp(header.ident, id_xms, 9) != 0) {
        error = L"ファイルが壊れています";
        return false;
    }
    if (header.versn != 0x10 && header.versn != 0x11) {
        error = L"未対応の形式バージョンです";
        return false;
    }

    // buf1 (+ buf3) に読み込んだヘッダ以降のデータ。範囲外は 0
    std::vector<uint8_t> buf1(file.begin() + sizeof(header), file.end());
    if (buf1.size() < 0x10000 + 4) buf1.resize(0x10000 + 4, 0);

    song.init(defaults);
    FixedSongData& songdata = song.data();

    songdata.patt_len = 64;
    songdata.nm_tracks = 9;   // adjust_tracks = TRUE
    songdata.tempo = 50;
    songdata.speed = 6;

    for (int temp2 = 0; temp2 <= header.snlen - 1; ++temp2) {
        if (temp2 < 128 && header.order[temp2] <= header.nopat) songdata.pattern_order[temp2] = header.order[temp2];
    }

    for (int temp2 = 0; temp2 <= 25; ++temp2) {
        import_amd_instrument(songdata, static_cast<uint8_t>(temp2 + 1), header.instr[temp2].iData);
        auto& name = at1(songdata.instr_names, temp2 + 1);
        name.assign(pcopy(name.str(), 1, 9) + truncate_string(char_array(header.instr[temp2].iName)));
    }

    if (header.versn == 0x10) {
        size_t pos = 0;
        const auto get_byte = [&]() -> uint8_t { return pos < buf1.size() ? buf1[pos++] : (++pos, 0); };
        for (int temp2 = 0; temp2 <= header.nopat; ++temp2) {
            for (int temp3 = 0; temp3 <= 0x3f; ++temp3) {
                for (int temp4 = 1; temp4 <= 9; ++temp4) {
                    const uint8_t byte3 = get_byte();
                    const uint8_t byte2 = get_byte();
                    const uint8_t byte1 = get_byte();
                    import_amd_event(song, static_cast<uint8_t>(temp2), static_cast<uint8_t>(temp3), static_cast<uint8_t>(temp4),
                                     byte1, byte2, byte3);
                }
            }
        }
    } else {
        import_amd_packed_patterns(song, buf1, header.nopat);
    }

    songdata.common_flag = songdata.common_flag | 0x80;
    songdata.songname.assign(CutStr(asciiz_string(char_array(header.sname))));
    songdata.composer.assign(CutStr(asciiz_string(char_array(header.aname))));
    import_old_flags(songdata);
    return true;
}
