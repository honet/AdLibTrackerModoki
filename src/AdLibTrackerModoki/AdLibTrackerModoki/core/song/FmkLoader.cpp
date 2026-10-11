#include "TrackerLoaders.h"

#include <set>
#include <vector>

#include "EffectDefs.h"
#include "LoaderCommon.h"

using namespace loader;

namespace
{
#pragma pack(push, 1)
    struct FmkHeader   // tFMK_HEADER
    {
        char id[4];             // 'FMK!'
        char songname[28];
        char composer[28];
        uint8_t bytef4;         // f4h
        uint8_t ftype;          // 1 = evolution 1, 2 = evolution 2
        uint8_t glob_var;       // bit0: stereo, 1: opl3, 2: rhythm, 3: 4.8dB tremolo, 4: 14 cent vibrato
        uint8_t base_spd;
        uint8_t init_spd;
        uint8_t reserved[9];
        uint8_t ordnum;
        uint8_t insnum;
        uint8_t patnum;
        uint8_t trk_pan[5];     // [1..5]
        uint8_t trk_set[20];    // [1..20]
    };

    struct FinData   // tFIN_DATA
    {
        char dname[12];
        char iname[27];
        uint8_t idata[11];
    };
#pragma pack(pop)

    static_assert(sizeof(FmkHeader) == 102);
    static_assert(sizeof(FinData) == 50);

    // fix_fmk_commands で使う一時的なエフェクト番号
    constexpr uint8_t temp_ef_Arpeggio = 0xf0;
    constexpr uint8_t temp_ef_rep = 0xf1;
    constexpr uint8_t temp_ef_XFVSlide = 0xf2;

    void import_fin_instrument(FixedSongData& songdata, uint8_t inst, const uint8_t* data)
    {
        Instrument& ins = at1(songdata.instr_data, inst);
        ins.fm_data.AM_VIB_EG_modulator = data[0];
        ins.fm_data.AM_VIB_EG_carrier = data[1];
        ins.fm_data.KSL_VOLUM_modulator = data[2];
        ins.fm_data.KSL_VOLUM_carrier = data[3];
        ins.fm_data.ATTCK_DEC_modulator = data[4];
        ins.fm_data.ATTCK_DEC_carrier = data[5];
        ins.fm_data.SUSTN_REL_modulator = data[6];
        ins.fm_data.SUSTN_REL_carrier = data[7];
        ins.fm_data.WAVEFORM_modulator = data[8] & 7;
        ins.fm_data.WAVEFORM_carrier = data[9] & 7;
        ins.fm_data.FEEDBACK_FM = data[10] & 0x0f;
        ins.panning = 0;
        ins.fine_tune = 0;
    }

    void import_fmk_event(Song& song, const FmkHeader& header, uint8_t pattern, uint8_t line, uint8_t channel,
                          uint8_t note, uint8_t ins, uint8_t vol, uint8_t cmd, uint8_t info)
    {
        Chunk chunk{};
        if (ins >= 1 && ins <= 99) chunk.instr_def = ins;

        if (note == 254) chunk.note = BYTE_NULL;
        else if (note == 255) chunk.note = 0;
        else if ((note & 0x0f) >= 1 && (note & 0x0f) <= 12) chunk.note = static_cast<uint8_t>(12 * (note >> 4) + (note & 0x0f));

        if (vol != BYTE_NULL) {
            chunk.effect_def2 = ef_ForceInsVolume;
            chunk.effect2 = static_cast<uint8_t>(63 - pas_max(vol, 63));
        }

        const auto set = [&chunk](uint8_t def, int value) {
            chunk.effect_def = def;
            chunk.effect = static_cast<uint8_t>(value);
        };

        // キャリア / モジュレータのパラメータ ('C' / 'M')
        const auto op_param = [&](bool carrier) {
            switch (info / 16) {
            case 1: set(ef_Extended3, (carrier ? ef_ex3_SetMultipC : ef_ex3_SetMultipM) * 16 + info % 16); break;
            case 2: set(ef_Extended3, (carrier ? ef_ex3_SetKslC : ef_ex3_SetKslM) * 16 + ((info % 16) & 3)); break;
            case 3: set(ef_Extended, (carrier ? ef_ex_SetAttckRateC : ef_ex_SetAttckRateM) * 16 + info % 16); break;
            case 4: set(ef_Extended, (carrier ? ef_ex_SetDecayRateC : ef_ex_SetDecayRateM) * 16 + info % 16); break;
            case 5: set(ef_Extended, (carrier ? ef_ex_SetSustnLevelC : ef_ex_SetSustnLevelM) * 16 + info % 16); break;
            case 6: set(ef_Extended, (carrier ? ef_ex_SetRelRateC : ef_ex_SetRelRateM) * 16 + info % 16); break;
            case 7: set(ef_SetWaveform, carrier ? ((info & 7) << 4) + 0x0f : 0xf0 + (info & 7)); break;
            case 8: set(ef_Extended, ef_ex_SetFeedback * 16 + (info & 7)); break;
            }
        };

        // ビブラート / トレモロの値 ('H' / 'R')
        const auto vib_value = [info]() -> int {
            if (info != 0 && info / 16 == 0) return 0x10 + (info & 0x0f);
            if (info != 0 && info % 16 == 0) return (info & 0xf0) + 1;
            return info;
        };

        switch (static_cast<char>(cmd + 'A' - 1)) {
        case 'A':   // SET SPEED
            if (info != 0) set(ef_SetSpeed, info);
            break;
        case 'B':   // JUMP TO ORDER
            if (info <= 254) set(ef_PositionJump, info);
            break;
        case 'C':   // CARRIER PARAM
            op_param(true);
            break;
        case 'D':   // VOLUME SLIDE
            switch (info / 16) {
            case 0:   // DOWN NORMAL
                if (info % 16 == 0) chunk.effect_def = temp_ef_XFVSlide;
                else set(ef_Extended2, ef_ex2_VolSlideDnXF * 16 + info % 16);
                break;
            case 15:   // DOWN FINE
                set(ef_VolSlideFine, info % 16);
                break;
            default:
                switch (info % 16) {
                case 0:   // UP NORMAL
                    if (info / 16 == 0) chunk.effect_def = temp_ef_XFVSlide;
                    else set(ef_Extended2, ef_ex2_VolSlideUpXF * 16 + info / 16);
                    break;
                case 15:   // UP FINE
                    set(ef_VolSlideFine, info & 0xf0);
                    break;
                }
                break;
            }
            break;
        case 'E':   // SLIDE DOWN
            if (info / 16 <= 14) set(ef_FSlideDown, info);
            else set(ef_FSlideDownFine, info & 0x0f);
            break;
        case 'F':   // SLIDE UP
            if (info / 16 <= 14) set(ef_FSlideUp, info);
            else set(ef_FSlideUpFine, info & 0x0f);
            break;
        case 'G':   // TONE PORTAMENTO
            set(ef_TonePortamento, info);
            break;
        case 'H':   // VIBRATO
            set(ef_Vibrato, vib_value());
            break;
        case 'I':   // RETRIG NOTE
            chunk.effect_def = ef_RetrigNote;
            if (info != 0) chunk.effect = static_cast<uint8_t>(pas_max(info * 2, 255));
            break;
        case 'J':   // ARPEGGIO
            set(temp_ef_Arpeggio, info);
            break;
        case 'M':   // MODULATOR PARAM
            op_param(false);
            break;
        case 'N':   // SET VIBRATO/TREMOLO WAVEFORM
            break;
        case 'P':   // BREAK PATTERN (Str2num(Num2str(info, 16), 10))
            if (info < 64) set(ef_PatternBreak, (info >> 4) * 10 + (info & 0x0f));
            break;
        case 'R':   // TREMOLO
            set(ef_Tremolo, vib_value());
            break;
        case 'S':   // STEREO CONTROL
            if ((header.glob_var & 1) == 1) {
                chunk.effect_def = ef_Extended;
                switch (info) {
                case 1: chunk.effect = ef_ex_SetPanningPos * 16 + 1; break;
                case 2: chunk.effect = ef_ex_SetPanningPos * 16 + 0; break;
                case 3: chunk.effect = ef_ex_SetPanningPos * 16 + 2; break;
                }
            }
            break;
        case 'T':   // MODULATOR VOLUME
            set(ef_SetModulatorVol, info & 0x3f);
            break;
        case 'U':   // CARRIER VOLUME
            set(ef_SetCarrierVol, info & 0x3f);
            break;
        }

        if (chunk.effect_def == 0 && chunk.effect != 0) chunk.effect = 0;
        song.putChunk(pattern, line, channel, chunk);
    }

    // fix_fmk_commands (iloaders.inc)
    class FmkCommandFixer
    {
    public:
        explicit FmkCommandFixer(Song& song) : song_(song), songdata_(song.data()) {}

        void run(uint8_t patterns)
        {
            std::memset(ins_cache, 0, sizeof(ins_cache));
            std::fill(std::begin(_1st_ins_load), std::end(_1st_ins_load), true);
            std::fill(std::begin(_speed_table_fixed), std::end(_speed_table_fixed), false);
            std::memset(xfvolsld_cache, 0, sizeof(xfvolsld_cache));
            std::memset(volsld_cache, 0, sizeof(volsld_cache));
            std::memset(slide_cache, 0, sizeof(slide_cache));
            std::memset(misc_cache, 0, sizeof(misc_cache));
            std::memset(arpg_cache, 0, sizeof(arpg_cache));
            std::memset(forcevol_cache, 0, sizeof(forcevol_cache));

            std::set<uint8_t> patts;
            int order = 0;
            uint8_t patt = BYTE_NULL;
            do {
                if (songdata_.pattern_order[order] >= 0x80) {
                    ++order;
                } else {
                    // オリジナルはオーダーに出てくるたびに (同じパターンでも) 処理する
                    patt = songdata_.pattern_order[order];
                    fix_single_pattern(patt);
                    ++order;
                    patts.insert(patt);
                }
            } while (!(patt >= patterns || order > 0x7f));

            for (int p = 0; p < patterns; ++p) {
                if (patts.count(static_cast<uint8_t>(p)) == 0) fix_single_pattern(static_cast<uint8_t>(p));
            }
        }

    private:
        struct Prev
        {
            uint8_t effect_def, effect, effect_def2, effect2;
        };

        Song& song_;
        FixedSongData& songdata_;
        uint8_t ins_cache[21], misc_cache[21], arpg_cache[21], forcevol_cache[21], volsld_cache[21], xfvolsld_cache[21],
            slide_cache[21];
        bool _1st_ins_load[21];
        bool _speed_table_fixed[256];
        Prev prev_cache[21];

        void get(uint8_t patt, int line, int chan, Chunk& chunk)
        {
            song_.getChunk(patt, static_cast<uint8_t>(line), static_cast<uint8_t>(chan), chunk);
        }
        void put(uint8_t patt, int line, int chan, const Chunk& chunk)
        {
            song_.putChunk(patt, static_cast<uint8_t>(line), static_cast<uint8_t>(chan), chunk);
        }

        void fix_single_pattern(uint8_t patt)
        {
            std::memset(prev_cache, 0, sizeof(prev_cache));
            uint8_t patt_break = BYTE_NULL;
            Chunk chunk{}, chunk2{}, chunk3{};

            if (!_speed_table_fixed[patt]) {
                for (int temp2 = 0; temp2 <= 0x3f && !_speed_table_fixed[patt]; ++temp2) {
                    for (int temp3 = 1; temp3 <= 20; ++temp3) {
                        get(patt, temp2, temp3, chunk);
                        if (chunk.effect_def == 0) {
                            chunk.effect_def = ef_SetCustomSpeedTab;
                            chunk.effect = 0xfa;
                            put(patt, temp2, temp3, chunk);
                            _speed_table_fixed[patt] = true;
                        } else if (chunk.effect_def2 == 0) {
                            chunk.effect_def2 = ef_SetCustomSpeedTab;
                            chunk.effect2 = 0xfa;
                            put(patt, temp2, temp3, chunk);
                            _speed_table_fixed[patt] = true;
                        }
                        if (_speed_table_fixed[patt]) break;
                    }
                }
            }

            for (int temp2 = 0; temp2 <= 0x3f; ++temp2) {
                for (int temp3 = 1; temp3 <= 20; ++temp3) {
                    get(patt, temp2, temp3, chunk);
                    if (chunk.effect_def == temp_ef_rep) {
                        chunk.effect_def = prev_cache[temp3].effect_def;
                        put(patt, temp2, temp3, chunk);
                    }

                    if (chunk.effect_def == temp_ef_XFVSlide) {
                        chunk.effect_def = ef_Extended2;
                        if (xfvolsld_cache[temp3] != 0) chunk.effect = static_cast<uint8_t>(ef_ex2_VolSlideUpXF * 16 + volsld_cache[temp3] / 16);
                        else chunk.effect = static_cast<uint8_t>(ef_ex2_VolSlideDnXF * 16 + volsld_cache[temp3] % 16);
                        put(patt, temp2, temp3, chunk);
                    }

                    if (chunk.effect_def == ef_PositionJump || chunk.effect_def == ef_PatternBreak) patt_break = static_cast<uint8_t>(temp2);

                    if (temp2 <= patt_break && chunk.instr_def != ins_cache[temp3] && chunk.effect_def2 != ef_ForceInsVolume) {
                        if (chunk.instr_def != 0) forcevol_cache[temp3] = 0;
                    }

                    if ((chunk.effect_def == ef_Extended && chunk.effect == ef_ex_ExtendedCmd2 * 16 + ef_ex_cmd2_ResetVol) ||
                        (chunk.effect_def2 == ef_Extended && chunk.effect2 == ef_ex_ExtendedCmd2 * 16 + ef_ex_cmd2_ResetVol)) {
                        forcevol_cache[temp3] = 0;
                    }

                    if (chunk.effect_def2 == ef_ForceInsVolume && temp2 <= patt_break) forcevol_cache[temp3] = 1;
                    if (chunk.instr_def != 0 && temp2 <= patt_break) ins_cache[temp3] = chunk.instr_def;

                    const bool is_note = chunk.note >= 1 && chunk.note <= 12 * 8 + 1;
                    if (chunk.instr_def != 0 || (chunk.instr_def == 0 && is_note && ins_cache[temp3] != 0)) {
                        put(patt, temp2, temp3, chunk);
                    }

                    if (temp2 <= patt_break) {
                        const bool vib_trem = chunk.effect_def == ef_Vibrato || chunk.effect_def == ef_Tremolo;
                        if (chunk.effect / 16 != 0) {
                            misc_cache[temp3] = static_cast<uint8_t>((chunk.effect & 0xf0) + (misc_cache[temp3] & 0x0f));
                        } else if (vib_trem) {
                            chunk.effect = static_cast<uint8_t>((misc_cache[temp3] & 0xf0) + (chunk.effect & 0x0f));
                            put(patt, temp2, temp3, chunk);
                        }

                        if (chunk.effect % 16 != 0) {
                            misc_cache[temp3] = static_cast<uint8_t>((misc_cache[temp3] & 0xf0) + (chunk.effect & 0x0f));
                        } else if (vib_trem) {
                            chunk.effect = static_cast<uint8_t>((chunk.effect & 0xf0) + (misc_cache[temp3] & 0x0f));
                            put(patt, temp2, temp3, chunk);
                        }

                        if (chunk.effect_def == ef_RetrigNote) {
                            if (chunk.effect != 0) {
                                misc_cache[temp3] = chunk.effect;
                            } else {
                                chunk.effect = misc_cache[temp3];
                                put(patt, temp2, temp3, chunk);
                            }
                        }

                        if (chunk.effect_def == temp_ef_Arpeggio) {
                            if (chunk.effect != 0) {
                                arpg_cache[temp3] = chunk.effect;
                            } else {
                                chunk.effect = arpg_cache[temp3];
                                put(patt, temp2, temp3, chunk);
                            }
                        }

                        if (chunk.effect_def == ef_FSlideDown || chunk.effect_def == ef_FSlideDownFine || chunk.effect_def == ef_FSlideUp ||
                            chunk.effect_def == ef_FSlideUpFine || chunk.effect_def == ef_TonePortamento) {
                            if (chunk.effect != 0) {
                                slide_cache[temp3] = chunk.effect;
                            } else {
                                chunk.effect = slide_cache[temp3];
                                put(patt, temp2, temp3, chunk);
                            }
                        }

                        if (chunk.effect_def == ef_Extended2 &&
                            (chunk.effect / 16 == ef_ex2_FreqSlideDnXF || chunk.effect / 16 == ef_ex2_FreqSlideUpXF)) {
                            if (chunk.effect % 16 != 0) {
                                slide_cache[temp3] = chunk.effect % 16;
                            } else {
                                chunk.effect = static_cast<uint8_t>((chunk.effect & 0xf0) + (slide_cache[temp3] & 0x0f));
                                put(patt, temp2, temp3, chunk);
                            }
                        }

                        if ((chunk.effect_def == ef_TPortamVolSlide || chunk.effect_def == ef_VibratoVolSlide ||
                             chunk.effect_def == ef_VolSlide || chunk.effect_def == ef_VolSlideFine) &&
                            temp2 <= patt_break) {
                            if (chunk.effect != 0) {
                                volsld_cache[temp3] = chunk.effect;
                            } else {
                                chunk.effect = volsld_cache[temp3];
                                put(patt, temp2, temp3, chunk);
                            }
                        }

                        if (chunk.effect_def == ef_Extended2 &&
                            (chunk.effect / 16 == ef_ex2_VolSlideDnXF || chunk.effect / 16 == ef_ex2_VolSlideUpXF)) {
                            if (chunk.effect % 16 != 0) {
                                if (chunk.effect / 16 == ef_ex2_VolSlideDnXF) {
                                    volsld_cache[temp3] = chunk.effect % 16;
                                    xfvolsld_cache[temp3] = 0;
                                } else {
                                    volsld_cache[temp3] = static_cast<uint8_t>((chunk.effect % 16) * 16);
                                    xfvolsld_cache[temp3] = 1;
                                }
                            }
                        }
                    }

                    const auto is_vib = [](uint8_t def) { return def == ef_Vibrato || def == ef_VibratoVolSlide; };
                    if (is_vib(prev_cache[temp3].effect_def) && !is_vib(chunk.effect_def)) {
                        if (chunk.effect_def == 0 && chunk.effect == 0) {
                            chunk2 = chunk;
                            chunk2.effect_def = ef_Extended;
                            chunk2.effect = ef_ex_ExtendedCmd2 * 16 + ef_ex_cmd2_VibrOff;
                            if (!(chunk2.effect_def == chunk2.effect_def2 && chunk2.effect == chunk2.effect2)) {
                                put(patt, temp2, temp3, chunk2);
                                chunk = chunk2;
                            }
                        } else if (chunk.effect_def2 == 0 && chunk.effect2 == 0) {
                            chunk2 = chunk;
                            chunk2.effect_def2 = ef_Extended;
                            chunk2.effect2 = ef_ex_ExtendedCmd2 * 16 + ef_ex_cmd2_VibrOff;
                            if (!(chunk2.effect_def2 == chunk2.effect_def && chunk2.effect2 == chunk2.effect)) {
                                put(patt, temp2, temp3, chunk2);
                                chunk = chunk2;
                            }
                        }
                    }

                    // (A) or (B and C and D) (Pascal の and は or より優先される)
                    if ((_1st_ins_load[temp3] && chunk.instr_def != 0) ||
                        (forcevol_cache[temp3] != 0 && temp2 <= patt_break && chunk.instr_def != 0)) {
                        if (chunk.effect_def2 + chunk.effect2 == 0) {
                            if (!(chunk.effect_def == ef_SetModulatorVol || chunk.effect_def == ef_SetCarrierVol)) {
                                chunk.effect_def2 = ef_Extended;
                                chunk.effect2 = ef_ex_ExtendedCmd2 * 16 + ef_ex_cmd2_ResetVol;
                            } else {
                                chunk.effect_def2 = chunk.effect_def;
                                chunk.effect2 = chunk.effect;
                                chunk.effect_def = ef_Extended;
                                chunk.effect = ef_ex_ExtendedCmd2 * 16 + ef_ex_cmd2_ResetVol;
                            }
                            put(patt, temp2, temp3, chunk);
                            forcevol_cache[temp3] = 0;
                            _1st_ins_load[temp3] = false;
                        }
                    }

                    prev_cache[temp3] = { chunk.effect_def, chunk.effect, chunk.effect_def2, chunk.effect2 };

                    if (is_4op_chan(songdata_, static_cast<uint8_t>(temp3)) && in_4op_tracks_hi(static_cast<uint8_t>(temp3))) {
                        get(patt, temp2, temp3 + 1, chunk3);
                        if (chunk.instr_def == 0 && chunk3.instr_def != 0) {
                            chunk.instr_def = ins_cache[temp3] != 0 ? ins_cache[temp3] : chunk3.instr_def;
                            put(patt, temp2, temp3, chunk);
                        }
                    }

                    if (chunk.effect_def == temp_ef_Arpeggio) {
                        chunk2 = chunk;
                        chunk2.effect_def = ef_Arpeggio;
                        put(patt, temp2, temp3, chunk2);
                    }

                    if ((chunk.effect_def == ef_SetModulatorVol || chunk.effect_def == ef_SetCarrierVol) &&
                        chunk.effect_def2 == ef_ForceInsVolume) {
                        chunk2 = chunk;
                        chunk2.effect_def = chunk.effect_def2;
                        chunk2.effect = chunk.effect2;
                        chunk2.effect_def2 = chunk.effect_def;
                        chunk2.effect2 = chunk.effect;
                        put(patt, temp2, temp3, chunk2);
                    }
                }
            }
        }
    };
}

bool LoadFmk(const std::wstring& path, Song& song, const Song::Defaults& defaults, std::wstring& error)
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
    FmkHeader header{};
    if (reader.read(&header, sizeof(header)) != sizeof(header) || std::memcmp(header.id, "FMK!", 4) != 0) return corrupted();
    if (header.bytef4 != 0xf4 || !(header.ftype == 1 || header.ftype == 2)) {
        error = L"未対応の形式です";
        return false;
    }

    // evolution 2 は trk_set が 18 チャンネル分しかない
    if (header.ftype == 2 && !reader.seek(sizeof(header) - 2)) return corrupted();

    uint8_t order_list[255] = {};
    if (header.ordnum != 0 && reader.read(order_list, header.ordnum) != header.ordnum) return corrupted();

    uint16_t paraptr_msg = 0;
    if (reader.read(&paraptr_msg, sizeof(paraptr_msg)) != sizeof(paraptr_msg)) return corrupted();

    // 曲の説明文 (mod_description が OFF の既定値では表示しないが、オリジナル同様に読めるか確かめる)
    {
        Reader msg(file);
        if (paraptr_msg != 0) {
            if (!msg.seek(paraptr_msg)) return corrupted();
            uint8_t desc_rows = 0;
            if (msg.read(&desc_rows, 1) != 1) return corrupted();
            if (desc_rows != 0) {
                std::vector<uint8_t> dscbuf(desc_rows * 20u);
                if (msg.read(dscbuf.data(), dscbuf.size()) != dscbuf.size()) return corrupted();
            }
        }
    }

    // paraptr_ins: [1..99] of Word、paraptr_pat: [0..63] of Longint (オリジナルは数が多いとはみ出す)
    std::vector<uint16_t> paraptr_ins(256 + 1, 0);
    if (header.insnum != 0 && reader.read(paraptr_ins.data() + 1, header.insnum * 2u) != header.insnum * 2u) return corrupted();
    std::vector<int32_t> paraptr_pat(256, 0);
    if (header.patnum != 0 && reader.read(paraptr_pat.data(), header.patnum * 4u) != header.patnum * 4u) return corrupted();

    song.init(defaults);
    FixedSongData& songdata = song.data();

    songdata.speed = header.init_spd != 0 ? header.init_spd : 1;
    songdata.tempo = header.base_spd != 0 ? header.base_spd : 50;
    songdata.songname.assign(truncate_string(char_array(header.songname)));
    songdata.composer.assign(truncate_string(char_array(header.composer)));
    songdata.common_flag = songdata.common_flag | 1 | 2 | 0x80;

    int temp = 1;
    for (int t = 18; t >= 1; --t) {
        temp = t;
        if (!((header.trk_set[t - 1] & 7) == 7)) break;
    }

    songdata.patt_len = 64;
    songdata.nm_tracks = static_cast<uint8_t>(temp);   // adjust_tracks = TRUE

    for (int temp2 = 1; temp2 <= temp; ++temp2) {
        if ((header.trk_set[temp2 - 1] & 7) == 6) {
            switch (temp2) {
            case 1: case 2: songdata.flag_4op |= 1; break;
            case 3: case 4: songdata.flag_4op |= 2; break;
            case 5: case 6: songdata.flag_4op |= 4; break;
            case 10: case 11: songdata.flag_4op |= 8; break;
            case 12: case 13: songdata.flag_4op |= 0x10; break;
            case 14: case 15: songdata.flag_4op |= 0x20; break;
            }
        }
    }

    if ((header.glob_var & 1) == 1) songdata.common_flag |= 0x20;
    if (((header.glob_var >> 3) & 1) == 1) songdata.common_flag |= 8;
    if (((header.glob_var >> 4) & 1) == 1) songdata.common_flag |= 0x10;

    import_old_flags(songdata);
    if ((header.glob_var & 1) == 1) {
        // _conv_fmk_pan: [0..2] = (1, 0, 2)。値 3 はオリジナルでは配列の外を読むため 0 とする
        static const uint8_t _conv_fmk_pan[4] = { 1, 0, 2, 0 };
        for (int ch = 1; ch <= 18; ++ch) {
            const uint8_t pan = (header.trk_pan[(ch - 1) / 4] >> (((ch - 1) % 4) * 2)) & 3;
            songdata.lock_flags[ch - 1] = static_cast<uint8_t>(songdata.lock_flags[ch - 1] + _conv_fmk_pan[pan]);
        }
    }

    for (int t = 1; t <= pas_max(header.ordnum, 128); ++t) {
        songdata.pattern_order[t - 1] = order_list[t - 1] == 255 ? 0x80 : order_list[t - 1];
    }

    for (int t = 1; t <= header.insnum; ++t) {
        if (!reader.seek(paraptr_ins[t])) return corrupted();
        FinData insdata{};
        if (reader.read(&insdata, sizeof(insdata)) != sizeof(insdata)) return corrupted();

        auto& name = at1(songdata.instr_names, static_cast<uint8_t>(t));
        if (!truncate_string(char_array(insdata.iname)).empty()) {
            name.assign(pcopy(name.str(), 1, 9) + pcopy(truncate_string(char_array(insdata.iname)), 1, 32));
        } else {
            name.assign(pcopy(name.str(), 1, 9) + truncate_string(char_array(insdata.dname)));
        }
        import_fin_instrument(songdata, static_cast<uint8_t>(t), insdata.idata);
    }

    std::vector<uint8_t> buf1(0x10000 + 16, 0);
    for (int pat = 0; pat < header.patnum; ++pat) {
        if (paraptr_pat[pat] < 0 || !reader.seek(static_cast<size_t>(paraptr_pat[pat]))) return corrupted();
        if (paraptr_pat[pat] == 0) continue;
        uint16_t patlen = 0;
        if (reader.read(&patlen, sizeof(patlen)) != sizeof(patlen)) return corrupted();
        if (patlen == 0) continue;

        std::fill(buf1.begin(), buf1.end(), uint8_t(0));
        reader.read(buf1.data(), patlen);

        uint16_t index = 0;
        uint8_t row = 0;
        do {
            if (buf1[index] != 0) {
                uint8_t note = BYTE_NULL, ins = 0, vol = BYTE_NULL, cmd = 0, info = 0;
                const uint8_t b = buf1[index++];
                const uint8_t chan = static_cast<uint8_t>((b & 31) + 1);
                if ((b | 0x20) == b) {
                    note = buf1[index++];
                    ins = buf1[index++];
                }
                if ((b | 0x40) == b) vol = buf1[index++];
                if ((b | 0x80) == b) {
                    cmd = buf1[index++];
                    info = buf1[index++];
                }
                if (chan - 1 >= 1 && chan - 1 <= 18) {
                    import_fmk_event(song, header, static_cast<uint8_t>(pat), row, static_cast<uint8_t>(chan - 1), note, ins, vol, cmd, info);
                }
            } else {
                ++row;
                ++index;
            }
        } while (row != 64);
    }

    FmkCommandFixer(song).run(header.patnum);
    return true;
}
