#include "TrackerLoaders.h"

#include <set>
#include <vector>

#include "EffectDefs.h"
#include "LoaderCommon.h"

using namespace loader;

namespace
{
    // tHSC_DATA (instr: [128][12], order: [50], patts: [50][64][9] of Word)
    // HSC ファイルのオーダーリストは 51 バイトだが、レコードは 50 バイトとして読むため、
    // patts はファイルのパターンデータより 1 バイト前から始まる (イベントの取り出し方もそれに合わせてある)。
    constexpr size_t HSC_INSTR_SIZE = 128 * 12;
    constexpr size_t HSC_ORDER_SIZE = 50;
    constexpr size_t HSC_PATTS_SIZE = 50 * 64 * 9 * 2;
    constexpr size_t HSC_DATA_SIZE = HSC_INSTR_SIZE + HSC_ORDER_SIZE + HSC_PATTS_SIZE;

    struct HscData
    {
        std::vector<uint8_t> buf;   // tHSC_DATA (最後のイベントが範囲外を読むため後ろに余白を付ける)

        HscData() : buf(HSC_DATA_SIZE + 2, 0) {}

        uint8_t* instr(int index) { return buf.data() + index * 12; }
        uint8_t& order(int index) { return buf[HSC_INSTR_SIZE + index]; }
        uint8_t* patts() { return buf.data() + HSC_INSTR_SIZE + HSC_ORDER_SIZE; }
    };

    bool is_note_byte(uint8_t b) { return (b >= 1 && b <= 12 * 8 + 1) || b == 0x00 || b == 0x7f || b == 0x80; }

    bool is_effect_byte(uint8_t b)
    {
        const uint8_t hi = b & 0xf0;
        return hi == 0x00 || hi == 0x10 || hi == 0x20 || hi == 0xa0 || hi == 0xb0 || hi == 0xc0 || hi == 0xf0;
    }

    void import_hsc_event(Song& song, uint8_t patt, uint8_t line, uint8_t chan, uint16_t event, bool fix_c_note_bug)
    {
        const uint8_t hi = event >> 8;
        const uint8_t lo = event & 0xff;
        Chunk chunk{};
        if (hi >= 1 && hi <= 12 * 8 + 1) {
            // REGULAR NOTE
            if (!fix_c_note_bug) {
                chunk.note = hi;
            } else {
                chunk.note = static_cast<uint8_t>(hi + 1);
                if (chunk.note > 12 * 8 + 1) chunk.note = 12 * 8 + 1;
            }
        } else if (hi == 0x7f) {
            // PAUSE
            chunk.note = BYTE_NULL;
        } else if (hi == 0x80) {
            // INSTRUMENT
            chunk.effect_def = ef_Extended;
            chunk.effect = ef_ex_ExtendedCmd2 * 16 + ef_ex_cmd2_ResetVol;
            chunk.instr_def = static_cast<uint8_t>(lo + 1);
            chunk.note = BYTE_NULL;
        }

        if (hi != 0x80) {
            switch (lo & 0xf0) {
            case 0x00:   // PATTERNBREAK
                if ((lo & 0x0f) == 1) chunk.effect_def = ef_PatternBreak;
                break;
            case 0x10:   // MANUAL SLIDE UP
                chunk.effect_def = ef_Extended2;
                chunk.effect = static_cast<uint8_t>(ef_ex2_FineTuneUp * 16 + pas_max((lo & 0x0f) + 1, 15));
                break;
            case 0x20:   // MANUAL SLIDE DOWN
                chunk.effect_def = ef_Extended2;
                chunk.effect = static_cast<uint8_t>(ef_ex2_FineTuneDown * 16 + pas_max((lo & 0x0f) + 1, 15));
                break;
            case 0xa0:   // SET CARRIER VOLUME
                chunk.effect_def = ef_SetCarrierVol;
                chunk.effect = static_cast<uint8_t>(63 - (lo & 0x0f) * 4);
                chunk.instr_def = static_cast<uint8_t>(lo + 1);
                break;
            case 0xb0:   // SET MODULATOR VOLUME
                chunk.effect_def = ef_SetModulatorVol;
                chunk.effect = static_cast<uint8_t>(63 - (lo & 0x0f) * 4);
                break;
            case 0xc0:   // SET INSTRUMENT VOLUME
                chunk.effect_def = ef_SetInsVolume;
                chunk.effect = static_cast<uint8_t>(63 - (lo & 0x0f) * 4);
                break;
            case 0xf0:   // SET SPEED
                chunk.effect_def = ef_SetSpeed;
                chunk.effect = static_cast<uint8_t>((lo & 0x0f) + 1);
                break;
            }
        }
        song.putChunk(patt, line, chan, chunk);
    }

    // tHSC_PATTERNS をワード配列として見たときの _hsc_event
    uint16_t _hsc_event(const uint8_t* data, int patt, int line, int chan)
    {
        const size_t word = (static_cast<size_t>(patt) * 64 + line) * 9 + (chan - 1);
        return static_cast<uint16_t>(data[(word + 1) * 2] + (data[word * 2 + 1] << 8));
    }

}

namespace loader
{
    void import_hsc_patterns(Song& song, const uint8_t* data, uint8_t patterns, bool fix_c_note_bug)
    {
        FixedSongData& songdata = song.data();
        uint8_t voice[10];
        for (int temp = 1; temp <= 9; ++temp) voice[temp] = accurate_conv ? 0 : static_cast<uint8_t>(temp);

        for (int temp = 0; temp <= 0x31; ++temp) {
            for (int temp2 = 0; temp2 <= 0x3f; ++temp2) {
                for (int temp3 = 1; temp3 <= 9; ++temp3) {
                    const uint16_t event = _hsc_event(data, temp, temp2, temp3);
                    if (event != 0) {
                        import_hsc_event(song, static_cast<uint8_t>(temp), static_cast<uint8_t>(temp2),
                                         static_cast<uint8_t>(temp3), event, fix_c_note_bug);
                    }
                }
            }
        }

        std::set<uint8_t> patts;
        int order = 0;
        uint8_t patt = BYTE_NULL;
        do {
            if (songdata.pattern_order[order] > 0x31) {
                ++order;
            } else {
                patt = songdata.pattern_order[order];
                uint8_t patt_break = BYTE_NULL;
                for (int temp2 = 0; temp2 <= 0x3f; ++temp2) {
                    for (int temp3 = 1; temp3 <= 9; ++temp3) {
                        Chunk chunk{};
                        song.getChunk(patt, static_cast<uint8_t>(temp2), static_cast<uint8_t>(temp3), chunk);
                        const uint16_t event = _hsc_event(data, patt, temp2, temp3);
                        const uint8_t hi = event >> 8;
                        const uint8_t lo = event & 0xff;

                        if (hi >= 1 && hi <= 12 * 8 + 1) {
                            // REGULAR NOTE
                            if (accurate_conv && voice[temp3] == 0) {
                                voice[temp3] = static_cast<uint8_t>(temp3);
                                chunk.instr_def = voice[temp3];
                            }
                            if (!accurate_conv) chunk.instr_def = voice[temp3];
                        } else if (hi == 0x80) {
                            // INSTRUMENT
                            if (temp2 != patt_break) {
                                voice[temp3] = static_cast<uint8_t>(lo + 1);
                                if (!accurate_conv) {
                                    chunk.instr_def = voice[temp3];
                                    chunk.note = BYTE_NULL;
                                }
                            }
                        }

                        if (hi != 0x80) {
                            switch (lo & 0xf0) {
                            case 0x00:   // PATTERNBREAK
                                if ((lo & 0x0f) == 1) patt_break = static_cast<uint8_t>(temp2 + 1);
                                break;
                            case 0xa0:   // SET CARRIER VOLUME
                            case 0xb0:   // SET MODULATOR VOLUME
                            case 0xc0:   // SET INSTRUMENT VOLUME
                                if (chunk.instr_def == 0 && !accurate_conv) chunk.instr_def = voice[temp3];
                                else if (chunk.instr_def == 0 && voice[temp3] == 0) chunk.instr_def = static_cast<uint8_t>(temp3);
                                break;
                            }
                        }

                        if (patts.count(songdata.pattern_order[order]) == 0) {
                            song.putChunk(patt, static_cast<uint8_t>(temp2), static_cast<uint8_t>(temp3), chunk);
                        }
                    }
                }
                ++order;
                patts.insert(patt);
            }
        } while (!(patt >= patterns || order > 0x7f));
    }
}

bool LoadHsc(const std::wstring& path, Song& song, const Song::Defaults& defaults, std::wstring& error)
{
    std::vector<uint8_t> file;
    if (!readFile(path, file)) {
        error = L"ファイルを開けません";
        return false;
    }

    HscData hscbuf;
    const size_t temp = std::min(file.size(), HSC_DATA_SIZE);
    std::memcpy(hscbuf.buf.data(), file.data(), temp);
    if (temp < HSC_INSTR_SIZE + HSC_ORDER_SIZE) {
        error = L"ファイルが壊れています";
        return false;
    }

    for (int temp2 = 0; temp2 <= 0x31; ++temp2) {
        if (hscbuf.order(temp2) > 0xb0) hscbuf.order(temp2) = 0x80;
    }

    uint8_t* patts = hscbuf.patts();
    for (size_t temp3 = 0; temp3 < temp - HSC_INSTR_SIZE - HSC_ORDER_SIZE; temp3 += 2) {
        if (!is_note_byte(patts[temp3 + 1]) || !is_effect_byte(patts[temp3])) {
            if (!is_note_byte(patts[temp3 + 1])) patts[temp3 + 1] = 0x00;
            if (!is_effect_byte(patts[temp3])) patts[temp3] = 0;
        }
    }

    song.init(defaults);
    FixedSongData& songdata = song.data();

    songdata.patt_len = 64;
    songdata.nm_tracks = 9;   // adjust_tracks = TRUE

    songdata.common_flag = songdata.common_flag | 2;
    songdata.tempo = 18;
    songdata.speed = 2;
    import_old_flags(songdata);

    for (int temp2 = 0; temp2 <= 0x31; ++temp2) songdata.pattern_order[temp2] = hscbuf.order(temp2);

    const int patterns = (static_cast<int>(temp) - static_cast<int>(HSC_INSTR_SIZE + HSC_ORDER_SIZE) - 1) / 0x480;
    import_hsc_patterns(song, patts, static_cast<uint8_t>(patterns), fix_c_note_bug);

    // specific corrections for HSC-Tracker instrument
    static const uint8_t HSC_KSL[4] = { 0, 3, 2, 1 };
    for (int temp2 = 0; temp2 <= 0x7f; ++temp2) {
        import_hsc_instrument(songdata, static_cast<uint8_t>(temp2 + 1), hscbuf.instr(temp2));
        FmInstData& fm = at1(songdata.instr_data, temp2 + 1).fm_data;
        fm.KSL_VOLUM_modulator = static_cast<uint8_t>((fm.KSL_VOLUM_modulator & 0x3f) + (HSC_KSL[fm.KSL_VOLUM_modulator >> 6] << 6));
        fm.KSL_VOLUM_carrier = static_cast<uint8_t>((fm.KSL_VOLUM_carrier & 0x3f) + (HSC_KSL[fm.KSL_VOLUM_carrier >> 6] << 6));
    }
    return true;
}
