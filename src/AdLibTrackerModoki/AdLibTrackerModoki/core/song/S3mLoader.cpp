#include "TrackerLoaders.h"

#include <set>
#include <vector>

#include "EffectDefs.h"
#include "LoaderCommon.h"

using namespace loader;

namespace
{
#pragma pack(push, 1)
    struct S3mHeader   // tS3M_HEADER
    {
        char songname[28];   // ASCIIZ
        uint8_t byte1a;      // 1Ah
        uint8_t ftype;       // 16 = ST3 module
        uint8_t resrvd1[2];
        uint16_t ordnum;
        uint16_t insnum;
        uint16_t patnum;
        uint16_t flags;
        uint16_t cwt_v;
        uint16_t ffi;
        char id[4];          // "SCRM"
        uint8_t g_v;
        uint8_t i_s;         // initial speed
        uint8_t i_t;         // initial tempo
        uint8_t m_v;
        uint8_t u_c;
        uint8_t d_p;
        uint8_t resrvd2[8];
        uint16_t special;
        uint8_t chan_set[32];
    };

    struct S3mAdlIns   // tS3M_ADLINS
    {
        uint8_t itype;       // 2:amel 3:abd 4:asnare 5:atom 6:acym 7:ahihat
        char dosname[12];
        char id0[3];
        uint8_t fmdata[12];
        uint8_t vol;
        uint8_t dsk;
        uint8_t resrvd1[2];
        uint16_t c2spd;
        uint16_t hi_c2sp;
        uint8_t resrvd2[12];
        char smpname[28];    // ASCIIZ
        char id[4];          // "SCRI" or "SCRS"
    };
#pragma pack(pop)

    static_assert(sizeof(S3mHeader) == 96);
    static_assert(sizeof(S3mAdlIns) == 80);

    // fix_s3m_commands で使う一時的なエフェクト番号
    constexpr uint8_t temp_ef_Arpeggio = 0xf0;

    struct S3mContext
    {
        Song& song;
        uint8_t default_vol[256] = {};    // [1..99] (範囲外は 0)
        int8_t ins_c4factor[256] = {};    // [1..99] (範囲外は 0)
    };

    int32_t scale_slide(uint8_t slide)
    {
        if (slide > 16) return pas_round(16 + slide / 8.0);
        return pas_round(slide * (2 - slide / 16.0));
    }

    void import_s3m_event(S3mContext& ctx, uint8_t pattern, uint8_t line, uint8_t channel,
                          uint8_t note, uint8_t ins, uint8_t vol, uint8_t cmd, uint8_t info)
    {
        Chunk chunk{};
        chunk.instr_def = ins;

        if (note == 254) chunk.note = BYTE_NULL;
        else if (note == 255) chunk.note = 0;
        else if ((note & 0x0f) <= 11) chunk.note = static_cast<uint8_t>(12 * ((note >> 4) & 7) + (note & 0x0f) + 1);

        if (vol != BYTE_NULL) {
            chunk.effect_def2 = ef_SetInsVolume;
            chunk.effect2 = static_cast<uint8_t>(pas_max(vol, 63));
        } else if (!(note == 254 || note == 255) && ins != 0 && pas_max(ctx.default_vol[ins], 63) != 63) {
            chunk.effect_def2 = ef_SetInsVolume;
            chunk.effect2 = static_cast<uint8_t>(pas_max(ctx.default_vol[ins], 63));
        }

        const auto set = [&chunk](uint8_t def, int32_t value) {
            chunk.effect_def = def;
            chunk.effect = static_cast<uint8_t>(value);
        };

        switch (static_cast<char>(cmd + 'A' - 1)) {
        case '@':   // NONE
            chunk.effect = info;
            break;
        case 'A':   // SET SPEED
            if (info != 0) set(ef_SetSpeed, info);
            break;
        case 'B':   // JUMP TO ORDER
            if (info <= 254) set(ef_PositionJump, info);
            break;
        case 'C':   // BREAK PATTERN (Str2num(Num2str(info, 16), 10))
            if (info < 64) set(ef_PatternBreak, (info >> 4) * 10 + (info & 0x0f));
            break;
        case 'D':   // VOLUME SLIDE
            switch (info / 16) {
            case 0: set(ef_VolSlide, info % 16); break;        // DOWN NORMAL
            case 15: set(ef_VolSlideFine, info % 16); break;   // DOWN FINE
            default:
                switch (info % 16) {
                case 0: set(ef_VolSlide, info & 0xf0); break;       // UP NORMAL
                case 15: set(ef_VolSlideFine, info & 0xf0); break;  // UP FINE
                }
                break;
            }
            break;
        case 'E':   // SLIDE DOWN
            if (info / 16 <= 13) {
                set(ef_FSlideDown, scale_slide(info));
            } else if (info / 16 == 14) {
                if (info != 0) set(ef_Extended2, ef_ex2_FreqSlideDnXF * 16 + pas_min((info & 0x0f) / 4, 1));
                else set(ef_Extended2, ef_ex2_FreqSlideDnXF * 16);
            } else {
                set(ef_FSlideDownFine, info & 0x0f);
            }
            break;
        case 'F':   // SLIDE UP
            if (info / 16 <= 13) {
                set(ef_FSlideUp, scale_slide(info));
            } else if (info / 16 == 14) {
                if (info != 0) set(ef_Extended2, ef_ex2_FreqSlideUpXF * 16 + pas_min((info & 0x0f) / 4, 1));
                else set(ef_Extended2, ef_ex2_FreqSlideUpXF * 16);
            } else {
                set(ef_FSlideUpFine, info & 0x0f);
            }
            break;
        case 'G': set(ef_TonePortamento, scale_slide(info)); break;
        case 'H': set(ef_Vibrato, info); break;
        case 'U': set(ef_ExtraFineVibrato, info); break;
        case 'I': set(ef_Tremor, info); break;
        case 'J': set(temp_ef_Arpeggio, info); break;
        case 'K': set(ef_VibratoVolSlide, info); break;
        case 'L': set(ef_TPortamVolSlide, info); break;
        case 'Q': set(ef_MultiRetrigNote, (info % 16) * 16 + info / 16); break;
        case 'R': set(ef_Tremolo, info); break;
        case 'S':   // SPECIAL COMMAND
            switch (info / 16) {
            case 0x0b: set(ef_Extended, ef_ex_PatternLoop * 16 + info % 16); break;
            case 0x0c: set(ef_Extended2, ef_ex2_NoteCut * 16 + info % 16); break;
            case 0x0d: set(ef_Extended2, ef_ex2_NoteDelay * 16 + info % 16); break;
            case 0x0e: set(ef_Extended2, ef_ex2_PatDelayRow * 16 + info % 16); break;
            }
            break;
        case 'T':   // TEMPO
            if (info >= 32) set(ef_SetTempo, pas_round(info / 2.5));
            break;
        case 'V':   // SET GLOBAL VOLUME
            set(ef_SetGlobalVolume, pas_max(info, 63));
            break;
        }

        if (chunk.effect_def == 0 && chunk.effect != 0) chunk.effect = 0;
        ctx.song.putChunk(pattern, line, channel, chunk);
    }

    // experimental method to fix up note fine-tuning
    int8_t find_scale_factor(int32_t freq, int8_t& fine_tune)
    {
        static const double _factor[8] = { 1.0 / 8, 1.0 / 4, 1.0 / 2, 1, 2, 4, 8, 16 };   // [-3..3+1]
        static const uint16_t _freq[14] = { 0,   // [1..12+1]
            33453 / 4, 35441 / 4, 37679 / 4, 39772 / 4, 42441 / 4, 44744 / 4,
            47727 / 4, 50416 / 4, 53426 / 4, 56370 / 4, 59658 / 4, 63354 / 4,
            33453 / 2 };
        static const uint16_t _fm_freq[14] = { 0,
            0x156, 0x16b, 0x181, 0x198, 0x1b0, 0x1ca, 0x1e5, 0x202, 0x220, 0x241, 0x263, 0x287, 0x2ae };

        fine_tune = 0;
        for (int scaler = -3; scaler <= 3 + 1; ++scaler) {
            for (int temp = 1; temp <= 12; ++temp) {
                const double factor = _factor[scaler + 3];
                const int32_t lo = pas_round(_freq[temp] * factor);
                const int32_t hi = pas_round(_freq[temp + 1] * factor);
                if (freq >= lo && freq <= hi) {
                    const double ratio = static_cast<double>(_fm_freq[temp + 1] - _fm_freq[temp]) / (_freq[temp + 1] - _freq[temp]);
                    if (freq - lo < hi - freq) {
                        fine_tune = static_cast<int8_t>(pas_round(ratio * (freq - lo)));
                        return static_cast<int8_t>(scaler * 12 + (temp - 1));
                    }
                    fine_tune = static_cast<int8_t>(pas_round(ratio * (freq - hi)));
                    if (temp != 12) return static_cast<int8_t>(scaler * 12 + temp);
                    return static_cast<int8_t>((scaler + 1) * 12);
                }
            }
        }
        fine_tune = 0;
        return -127;
    }

    bool is_vibrato_like(uint8_t def)
    {
        return def == ef_Vibrato || def == ef_ExtraFineVibrato || def == ef_Tremolo || def == ef_Tremor || def == ef_MultiRetrigNote;
    }

    bool is_slide(uint8_t def)
    {
        return def == ef_FSlideDown || def == ef_FSlideDownFine || def == ef_FSlideUp || def == ef_FSlideUpFine ||
               def == ef_TonePortamento;
    }

    // fix_s3m_commands (iloaders.inc)
    class S3mCommandFixer
    {
    public:
        S3mCommandFixer(S3mContext& ctx) : ctx_(ctx), songdata_(ctx.song.data()) {}

        void run(uint8_t patterns)
        {
            std::memset(ins_cache, 0, sizeof(ins_cache));
            std::memset(note_cache, 0, sizeof(note_cache));
            std::memset(volsld_cache, 0, sizeof(volsld_cache));
            std::memset(slide_cache, 0, sizeof(slide_cache));
            std::memset(misc_cache, 0, sizeof(misc_cache));
            std::memset(arpg_cache, 0, sizeof(arpg_cache));

            uint8_t patt = BYTE_NULL;
            order = 0;
            do {
                if (order_byte(order) >= 0x80) {
                    ++order;
                } else {
                    patt = order_byte(order);
                    if (patts.count(patt) == 0) fix_single_pattern(patt);
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

        S3mContext& ctx_;
        FixedSongData& songdata_;
        uint8_t ins_cache[21], misc_cache[21], arpg_cache[21], volsld_cache[21], slide_cache[21], note_cache[21],
            patloop_cache[21];
        Prev prev_cache[21];
        std::set<uint8_t> patts;
        int order = 0;
        uint8_t temp = 0;   // オリジナルの外側の変数 temp (ビブラートの補正で上位 4bit を退避する)

        // songdata.pattern_order[order] (order が 128 以上のときはオリジナル同様に後続のフィールドを読む)
        uint8_t order_byte(int index) const { return reinterpret_cast<const uint8_t*>(songdata_.pattern_order)[index]; }

        void get(uint8_t patt, int line, int chan, Chunk& chunk)
        {
            ctx_.song.getChunk(patt, static_cast<uint8_t>(line), static_cast<uint8_t>(chan), chunk);
        }
        void put(uint8_t patt, int line, int chan, const Chunk& chunk)
        {
            ctx_.song.putChunk(patt, static_cast<uint8_t>(line), static_cast<uint8_t>(chan), chunk);
        }

        void fix_single_pattern(uint8_t patt)
        {
            std::memset(prev_cache, 0, sizeof(prev_cache));
            std::memset(patloop_cache, BYTE_NULL, sizeof(patloop_cache));
            uint8_t patt_break = BYTE_NULL;
            Chunk chunk{}, chunk2{};

            for (int temp2 = 0; temp2 <= 0x3f; ++temp2) {
                for (int temp3 = 1; temp3 <= 20; ++temp3) {
                    get(patt, temp2, temp3, chunk);
                    if (chunk.effect_def == ef_PositionJump || chunk.effect_def == ef_PatternBreak) patt_break = static_cast<uint8_t>(temp2);

                    if (chunk.instr_def != 0 && temp2 <= patt_break) ins_cache[temp3] = chunk.instr_def;
                    if (chunk.note >= 1 && chunk.note <= 12 * 8 + 1 && temp2 <= patt_break) note_cache[temp3] = chunk.note;

                    if (chunk.instr_def != 0 ||
                        (chunk.instr_def == 0 && chunk.note >= 1 && chunk.note <= 12 * 8 + 1 && ins_cache[temp3] != 0)) {
                        const uint8_t temp4 = chunk.instr_def != 0 ? chunk.instr_def : ins_cache[temp3];
                        if (ctx_.ins_c4factor[temp4] != 0 && patts.count(order_byte(order)) == 0) {
                            if (ctx_.ins_c4factor[temp4] != -127) {
                                chunk.note = static_cast<uint8_t>(pas_min(pas_max(chunk.note + ctx_.ins_c4factor[temp4], 12 * 8 + 1), 1));
                            } else {
                                chunk.note = 1;
                            }
                            put(patt, temp2, temp3, chunk);
                        }
                    }

                    if (chunk.effect_def == ef_Extended && chunk.effect / 16 == ef_ex_PatternLoop && chunk.effect % 16 != 0) {
                        if (!(patloop_cache[temp3] == 0 || patloop_cache[temp3] == BYTE_NULL) && temp2 != 0) {
                            if (prev_cache[temp3].effect_def == 0 && prev_cache[temp3].effect == 0) {
                                get(patt, temp2 - 1, temp3, chunk2);
                                chunk2.effect_def = ef_Extended;
                                chunk2.effect = ef_ex_PatternLoop * 16;
                                if (!(chunk2.effect_def == chunk2.effect_def2 && chunk2.effect == chunk2.effect2)) {
                                    put(patt, temp2 - 1, temp3, chunk2);
                                    prev_cache[temp3].effect_def = chunk.effect_def;
                                    prev_cache[temp3].effect = chunk.effect;
                                }
                            } else if (prev_cache[temp3].effect_def2 == 0 && prev_cache[temp3].effect2 == 0) {
                                get(patt, temp2 - 1, temp3, chunk2);
                                chunk2.effect_def2 = ef_Extended;
                                chunk2.effect2 = ef_ex_PatternLoop * 16;
                                if (!(chunk2.effect_def2 == chunk2.effect_def && chunk2.effect2 == chunk2.effect)) {
                                    put(patt, temp2 - 1, temp3, chunk2);
                                    prev_cache[temp3].effect_def2 = chunk.effect_def2;
                                    prev_cache[temp3].effect2 = chunk.effect2;
                                }
                            }
                        } else if (patloop_cache[temp3] != 0 && temp2 != 0) {
                            get(patt, 0, temp3, chunk2);
                            if (chunk2.effect_def == 0 && chunk2.effect == 0) {
                                chunk2.effect_def = ef_Extended;
                                chunk2.effect = ef_ex_PatternLoop * 16;
                                if (!(chunk2.effect_def == chunk2.effect_def2 && chunk2.effect == chunk2.effect2)) put(patt, 0, temp3, chunk2);
                            } else if (chunk2.effect_def2 == 0 && chunk2.effect2 == 0) {
                                chunk2.effect_def2 = ef_Extended;
                                chunk2.effect2 = ef_ex_PatternLoop * 16;
                                if (!(chunk2.effect_def2 == chunk2.effect_def && chunk2.effect2 == chunk2.effect)) put(patt, 0, temp3, chunk2);
                            }
                        }
                    }

                    if (temp2 <= patt_break) {
                        if (chunk.effect / 16 != 0) {
                            misc_cache[temp3] = static_cast<uint8_t>((chunk.effect & 0xf0) + (misc_cache[temp3] & 0x0f));
                        } else if (is_vibrato_like(chunk.effect_def)) {
                            chunk.effect = static_cast<uint8_t>((misc_cache[temp3] & 0xf0) + (chunk.effect & 0x0f));
                            put(patt, temp2, temp3, chunk);
                        }

                        if (chunk.effect % 16 != 0) {
                            misc_cache[temp3] = static_cast<uint8_t>((misc_cache[temp3] & 0xf0) + (chunk.effect & 0x0f));
                        } else if (is_vibrato_like(chunk.effect_def)) {
                            chunk.effect = static_cast<uint8_t>((chunk.effect & 0xf0) + (misc_cache[temp3] & 0x0f));
                            put(patt, temp2, temp3, chunk);
                        }

                        if (chunk.effect_def == temp_ef_Arpeggio) {
                            if (chunk.effect != 0) {
                                arpg_cache[temp3] = chunk.effect;
                            } else {
                                chunk.effect = arpg_cache[temp3];
                                put(patt, temp2, temp3, chunk);
                            }
                        }

                        if (is_slide(chunk.effect_def)) {
                            if (chunk.effect != 0) {
                                slide_cache[temp3] = chunk.effect;
                            } else {
                                chunk.effect = slide_cache[temp3];
                                put(patt, temp2, temp3, chunk);
                            }
                        }

                        // experimental method to fix up frequency slide
                        if (is_slide(chunk.effect_def) || chunk.effect_def == ef_Vibrato || chunk.effect_def == ef_ExtraFineVibrato) {
                            if (note_cache[temp3] != 0) {
                                const bool vib = chunk.effect_def == ef_Vibrato || chunk.effect_def == ef_ExtraFineVibrato;
                                if (vib) {
                                    temp = chunk.effect & 0xf0;
                                    chunk.effect = chunk.effect % 16;
                                }

                                static const double scale[9] = { 0, 0.55, 0.75, 0.95, 1.15, 1.35, 1.55, 1.75, 1.95 };
                                const int octave = (note_cache[temp3] - 1) / 12 + 1;
                                if (octave >= 1 && octave <= 8) {
                                    chunk.effect = static_cast<uint8_t>(pas_max(pas_round(chunk.effect * scale[octave]), 255));
                                }

                                if (vib) chunk.effect = static_cast<uint8_t>(pas_max(chunk.effect, 15) + temp);
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
                                if (chunk.effect / 16 == ef_ex2_VolSlideDnXF) volsld_cache[temp3] = chunk.effect % 16;
                                else volsld_cache[temp3] = static_cast<uint8_t>((chunk.effect % 16) << 4);
                            } else {
                                if (chunk.effect / 16 == ef_ex2_VolSlideDnXF) {
                                    chunk.effect = static_cast<uint8_t>((chunk.effect & 0xf0) + (volsld_cache[temp3] & 0x0f));
                                } else {
                                    chunk.effect = static_cast<uint8_t>((volsld_cache[temp3] & 0xf0) + (chunk.effect & 0x0f));
                                }
                                put(patt, temp2, temp3, chunk);
                            }
                        }
                    }

                    const auto is_vib = [](uint8_t def) {
                        return def == ef_Vibrato || def == ef_ExtraFineVibrato || def == ef_VibratoVolSlide;
                    };
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

                    if (chunk.effect_def == ef_Extended && chunk.effect / 16 == ef_ex_PatternLoop) {
                        patloop_cache[temp3] = chunk.effect % 16;
                    }

                    prev_cache[temp3] = { chunk.effect_def, chunk.effect, chunk.effect_def2, chunk.effect2 };

                    if (chunk.effect_def == temp_ef_Arpeggio) {
                        chunk2 = chunk;
                        chunk2.effect_def = ef_Arpeggio;
                        put(patt, temp2, temp3, chunk2);
                    }
                }
            }
        }
    };
}

bool LoadS3m(const std::wstring& path, Song& song, const Song::Defaults& defaults, std::wstring& error)
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
    S3mHeader header{};
    if (reader.read(&header, sizeof(header)) != sizeof(header) || std::memcmp(header.id, "SCRM", 4) != 0) {
        return corrupted();
    }
    if (header.byte1a != 0x1a || header.ftype != 0x10) {
        error = L"未対応の形式です";
        return false;
    }

    std::vector<uint8_t> order_list(std::max<size_t>(header.ordnum, 255), 0);   // [0..254]
    if (reader.read(order_list.data(), header.ordnum) != header.ordnum) return corrupted();

    std::vector<uint16_t> paraptr_ins(header.insnum + 1, 0);   // [1..insnum]
    if (reader.read(paraptr_ins.data() + 1, header.insnum * 2u) != header.insnum * 2u) return corrupted();

    std::vector<uint16_t> paraptr_pat(header.patnum, 0);
    if (reader.read(paraptr_pat.data(), header.patnum * 2u) != header.patnum * 2u) return corrupted();

    song.init(defaults);
    FixedSongData& songdata = song.data();
    auto ctx = std::make_unique<S3mContext>(S3mContext{ song });

    songdata.speed = header.i_s != 0 ? header.i_s : 1;
    songdata.tempo = pas_round(header.i_t / 2.5) < 255 ? static_cast<uint8_t>(pas_round(header.i_t / 2.5)) : 255;
    songdata.songname.assign(truncate_string(asciiz_string(char_array(header.songname))));
    songdata.common_flag = songdata.common_flag | 0x80;
    import_old_flags(songdata);

    // 使われている最後のチャンネル (オリジナルの For ループを抜けたときの temp)
    int last_chan = 1;
    for (int temp = 32; temp >= 1; --temp) {
        last_chan = temp;
        if (header.chan_set[temp - 1] != 255) break;
    }
    // 無効なチャンネルを channel_flag で OFF にする処理は、ロード後に channel_flag がすべて ON に
    // 戻されるため (FILE_open)、ここでは行わない

    songdata.patt_len = 64;
    songdata.nm_tracks = static_cast<uint8_t>(pas_max(last_chan, 18));   // adjust_tracks = TRUE

    for (int temp = 1; temp <= pas_max(header.ordnum, 128); ++temp) {
        switch (order_list[temp - 1]) {
        case 254: songdata.pattern_order[temp - 1] = static_cast<uint8_t>(0x80 + temp); break;
        case 255: songdata.pattern_order[temp - 1] = 0x80; break;
        default: songdata.pattern_order[temp - 1] = order_list[temp - 1]; break;
        }
    }

    for (int temp = 1; temp <= header.insnum; ++temp) {
        if (!reader.seek(paraptr_ins[temp] * 16u)) return corrupted();
        S3mAdlIns insdata{};
        if (reader.read(&insdata, sizeof(insdata)) != sizeof(insdata)) return corrupted();

        auto& name = at1(songdata.instr_names, static_cast<uint8_t>(temp));
        if (!truncate_string(char_array(insdata.smpname)).empty()) {
            name.assign(pcopy(name.str(), 1, 9) + pcopy(truncate_string(asciiz_string(char_array(insdata.smpname))), 1, 32));
        } else {
            name.assign(pcopy(name.str(), 1, 9) + truncate_string(char_array(insdata.dosname)));
        }

        if (insdata.itype >= 2 && insdata.itype <= 7) {
            if (std::memcmp(insdata.id, "SCRI", 4) != 0 && std::memcmp(insdata.id, "SCRS", 4) != 0) return corrupted();
            import_standard_instrument(songdata, static_cast<uint8_t>(temp), insdata.fmdata);
        }

        const uint8_t index = static_cast<uint8_t>(temp);
        ctx->default_vol[index] = insdata.vol;
        if (insdata.c2spd != 0 && insdata.c2spd != 8363) {
            ctx->ins_c4factor[index] = find_scale_factor(insdata.c2spd, at1(songdata.instr_data, index).fine_tune);
        }
    }

    std::vector<uint8_t> buf1(0x10000 + 16, 0);
    for (int pat = 0; pat < header.patnum; ++pat) {
        if (!reader.seek(paraptr_pat[pat] * 16u)) return corrupted();
        uint16_t patlen = 0;
        if (reader.read(&patlen, sizeof(patlen)) != sizeof(patlen)) return corrupted();
        if (patlen == 0) continue;

        std::fill(buf1.begin(), buf1.end(), uint8_t(0));
        reader.read(buf1.data(), static_cast<uint16_t>(patlen - 2));

        uint16_t index = 0;
        uint8_t row = 0;
        do {
            if (buf1[index] != 0) {
                uint8_t note = BYTE_NULL, ins = 0, vol = BYTE_NULL, cmd = 0, info = 0;
                const uint8_t temp = buf1[index++];
                const uint8_t chan = static_cast<uint8_t>((temp & 31) + 1);
                if ((temp | 0x20) == temp) {
                    note = buf1[index++];
                    ins = buf1[index++];
                }
                if ((temp | 0x40) == temp) vol = buf1[index++];
                if ((temp | 0x80) == temp) {
                    cmd = buf1[index++];
                    info = buf1[index++];
                }

                if (chan > songdata.nm_tracks) songdata.nm_tracks = static_cast<uint8_t>(pas_max(chan, 18));
                if (chan >= 1 && chan <= songdata.nm_tracks) {
                    import_s3m_event(*ctx, static_cast<uint8_t>(pat), row, chan, note, ins, vol, cmd, info);
                }
            } else {
                ++row;
                ++index;
            }
        } while (row != 64);
    }

    S3mCommandFixer(*ctx).run(static_cast<uint8_t>(header.patnum));
    return true;
}
