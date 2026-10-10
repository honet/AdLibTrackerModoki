#include "LoaderCommon.h"

#include <filesystem>
#include <fstream>
#include <set>

#include "Depack.h"
#include "EffectDefs.h"

namespace loader
{

// 展開結果を dst へ容量の範囲でコピーする (オリジナルは展開先へ直接書き込む)
void copyTo(void* dst, size_t capacity, const std::vector<uint8_t>& src)
{
    copyTo(dst, capacity, src.data(), src.size());
}

void copyTo(void* dst, size_t capacity, const uint8_t* src, size_t size)
{
    if (size > 0) std::memcpy(dst, src, std::min(capacity, size));
}

bool readFile(const std::wstring& path, std::vector<uint8_t>& data)
{
    std::ifstream f(std::filesystem::path(path), std::ios::binary);
    if (!f) return false;
    data.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return true;
}

std::string lower_name_only(const std::wstring& path)
{
    const std::wstring name = std::filesystem::path(path).filename().wstring();
    std::string result;
    for (const wchar_t c : name) {
        if (c >= L'A' && c <= L'Z') result += static_cast<char>(c - L'A' + 'a');
        else if (c >= 0x20 && c < 0x7f) result += static_cast<char>(c);
        else result += '_';
    }
    return result;
}

// Pascal の Copy(str, index, count)
std::string pcopy(const std::string& s, size_t index, size_t count)
{
    if (index < 1 || index > s.size()) return std::string();
    return s.substr(index - 1, count);
}

// ----- 旧形式の取り込み -----

void import_old_a2m_event1(OldImportContext& ctx, uint8_t patt, uint8_t line, uint8_t chan,
                           const OldChunk& old_chunk, bool processing_whole_song)
{
    enum : uint8_t
    {
        fx_Arpeggio = 0x00, fx_FSlideUp = 0x01, fx_FSlideDown = 0x02, fx_FSlideUpFine = 0x03,
        fx_FSlideDownFine = 0x04, fx_TonePortamento = 0x05, fx_TPortamVolSlide = 0x06, fx_Vibrato = 0x07,
        fx_VibratoVolSlide = 0x08, fx_SetOpIntensity = 0x09, fx_SetInsVolume = 0x0a, fx_PatternBreak = 0x0b,
        fx_PatternJump = 0x0c, fx_SetTempo = 0x0d, fx_SetTimer = 0x0e, fx_Extended = 0x0f,
    };
    enum : uint8_t
    {
        fx_ex_DefAMdepth = 0x00, fx_ex_DefVibDepth = 0x01, fx_ex_DefWaveform = 0x02, fx_ex_ManSlideUp = 0x03,
        fx_ex_ManSlideDown = 0x04, fx_ex_VSlideUp = 0x05, fx_ex_VSlideDown = 0x06, fx_ex_VSlideUpFine = 0x07,
        fx_ex_VSlideDownFine = 0x08, fx_ex_RetrigNote = 0x09, fx_ex_SetAttckRate = 0x0a, fx_ex_SetDecayRate = 0x0b,
        fx_ex_SetSustnLevel = 0x0c, fx_ex_SetReleaseRate = 0x0d, fx_ex_SetFeedback = 0x0e, fx_ex_ExtendedCmd = 0x0f,
    };

    Chunk chunk{};
    chunk.note = old_chunk.note;
    chunk.instr_def = old_chunk.instr_def;
    chunk.effect_def = old_chunk.effect_def;
    chunk.effect = old_chunk.effect;

    const uint8_t hi = old_chunk.effect / 16;
    const uint8_t lo = old_chunk.effect % 16;

    switch (old_chunk.effect_def) {
    case fx_Arpeggio:        chunk.effect_def = ef_Arpeggio; break;
    case fx_FSlideUp:        chunk.effect_def = ef_FSlideUp; break;
    case fx_FSlideDown:      chunk.effect_def = ef_FSlideDown; break;
    case fx_FSlideUpFine:    chunk.effect_def = ef_FSlideUpFine; break;
    case fx_FSlideDownFine:  chunk.effect_def = ef_FSlideDownFine; break;
    case fx_TonePortamento:  chunk.effect_def = ef_TonePortamento; break;
    case fx_TPortamVolSlide: chunk.effect_def = ef_TPortamVolSlide; break;
    case fx_Vibrato:         chunk.effect_def = ef_Vibrato; break;
    case fx_VibratoVolSlide: chunk.effect_def = ef_VibratoVolSlide; break;
    case fx_SetInsVolume:    chunk.effect_def = ef_SetInsVolume; break;
    case fx_PatternJump:     chunk.effect_def = ef_PositionJump; break;
    case fx_PatternBreak:    chunk.effect_def = ef_PatternBreak; break;
    case fx_SetTempo:        chunk.effect_def = ef_SetSpeed; break;
    case fx_SetTimer:        chunk.effect_def = ef_SetTempo; break;

    case fx_SetOpIntensity:
        if (hi != 0) {
            chunk.effect_def = ef_SetCarrierVol;
            chunk.effect = static_cast<uint8_t>(3 + hi * 4);
        } else if (lo != 0) {
            chunk.effect_def = ef_SetModulatorVol;
            chunk.effect = static_cast<uint8_t>(3 + lo * 4);
        } else {
            chunk.effect_def = 0;
        }
        break;

    case fx_Extended:
        switch (hi) {
        case fx_ex_DefAMdepth:
            chunk.effect_def = ef_Extended;
            chunk.effect = ef_ex_SetTremDepth * 16 + lo;
            break;
        case fx_ex_DefVibDepth:
            chunk.effect_def = ef_Extended;
            chunk.effect = ef_ex_SetVibDepth * 16 + lo;
            break;
        case fx_ex_DefWaveform:
            chunk.effect_def = ef_SetWaveform;
            if (lo <= 3) chunk.effect = static_cast<uint8_t>(lo * 16 + 0x0f);
            else if (lo <= 7) chunk.effect = static_cast<uint8_t>(0xf0 + lo - 4);
            break;
        case fx_ex_VSlideUp:
            chunk.effect_def = ef_VolSlide;
            chunk.effect = static_cast<uint8_t>(lo * 16);
            break;
        case fx_ex_VSlideDown:
            chunk.effect_def = ef_VolSlide;
            chunk.effect = lo;
            break;
        case fx_ex_VSlideUpFine:
            chunk.effect_def = ef_VolSlideFine;
            chunk.effect = static_cast<uint8_t>(lo * 16);
            break;
        case fx_ex_VSlideDownFine:
            chunk.effect_def = ef_VolSlideFine;
            chunk.effect = lo;
            break;
        case fx_ex_ManSlideUp:
            chunk.effect_def = ef_Extended2;
            chunk.effect = ef_ex2_FineTuneUp * 16 + lo;
            break;
        case fx_ex_ManSlideDown:
            chunk.effect_def = ef_Extended2;
            chunk.effect = ef_ex2_FineTuneDown * 16 + lo;
            break;
        case fx_ex_RetrigNote:
            chunk.effect_def = ef_RetrigNote;
            chunk.effect = static_cast<uint8_t>(lo + 1);
            break;
        case fx_ex_SetAttckRate:
            chunk.effect_def = ef_Extended;
            chunk.effect = lo;
            chunk.effect += !ctx.adsr_carrier[chan] ? ef_ex_SetAttckRateM * 16 : ef_ex_SetAttckRateC * 16;
            break;
        case fx_ex_SetDecayRate:
            chunk.effect_def = ef_Extended;
            chunk.effect = lo;
            chunk.effect += !ctx.adsr_carrier[chan] ? ef_ex_SetDecayRateM * 16 : ef_ex_SetDecayRateC * 16;
            break;
        case fx_ex_SetSustnLevel:
            chunk.effect_def = ef_Extended;
            chunk.effect = lo;
            chunk.effect += !ctx.adsr_carrier[chan] ? ef_ex_SetSustnLevelM * 16 : ef_ex_SetSustnLevelC * 16;
            break;
        case fx_ex_SetReleaseRate:
            chunk.effect_def = ef_Extended;
            chunk.effect = lo;
            chunk.effect += !ctx.adsr_carrier[chan] ? ef_ex_SetRelRateM * 16 : ef_ex_SetRelRateC * 16;
            break;
        case fx_ex_SetFeedback:
            chunk.effect_def = ef_Extended;
            chunk.effect = ef_ex_SetFeedback * 16 + lo;
            break;
        case fx_ex_ExtendedCmd:
            if (lo <= 9) {
                chunk.effect_def = ef_Extended;
                chunk.effect = ef_ex_ExtendedCmd2 * 16;
                switch (lo) {
                case 0: chunk.effect += ef_ex_cmd2_RSS; break;
                case 1: chunk.effect += ef_ex_cmd2_LockVol; break;
                case 2: chunk.effect += ef_ex_cmd2_UnlockVol; break;
                case 3: chunk.effect += ef_ex_cmd2_LockVP; break;
                case 4: chunk.effect += ef_ex_cmd2_UnlockVP; break;
                case 5:
                    chunk.effect_def = processing_whole_song ? 255 : 0;
                    chunk.effect = 0;
                    ctx.adsr_carrier[chan] = true;
                    break;
                case 6:
                    chunk.effect_def = processing_whole_song ? 255 : 0;
                    chunk.effect = processing_whole_song ? 1 : 0;
                    ctx.adsr_carrier[chan] = false;
                    break;
                case 7: chunk.effect += ef_ex_cmd2_VSlide_car; break;
                case 8: chunk.effect += ef_ex_cmd2_VSlide_mod; break;
                case 9: chunk.effect += ef_ex_cmd2_VSlide_def; break;
                }
            } else {
                chunk.effect_def = 0;
                chunk.effect = 0;
            }
            break;
        }
        break;
    }

    ctx.song.putChunk(patt, line, chan, chunk);
}

void replace_old_adsr(OldImportContext& ctx, uint8_t patterns)
{
    FixedSongData& songdata = ctx.song.data();
    ctx.adsr_carrier.fill(false);

    std::set<uint8_t> patts;
    int order = 0;
    uint8_t patt = 0xff;
    do {
        if (songdata.pattern_order[order] >= 0x80) {
            ++order;
        } else {
            patt = songdata.pattern_order[order];
            uint8_t patt_break = 0xff;
            for (int temp2 = 0; temp2 <= 0x3f; ++temp2) {
                for (int temp3 = 1; temp3 <= 9; ++temp3) {
                    Chunk chunk{};
                    ctx.song.getChunk(patt, static_cast<uint8_t>(temp2), static_cast<uint8_t>(temp3), chunk);
                    Chunk chunk2 = chunk;

                    if (chunk.effect_def == ef_PositionJump || chunk.effect_def == ef_PatternBreak) {
                        patt_break = static_cast<uint8_t>(temp2);
                    }

                    if (chunk.effect_def == 0xff || chunk.effect_def == ef_Extended) {
                        if (chunk.effect_def == 0xff) {
                            chunk2.effect_def = 0;
                            chunk2.effect = 0;
                            if (temp2 <= patt_break) {
                                if (chunk.effect == 0) ctx.adsr_carrier[temp3] = true;
                                else if (chunk.effect == 1) ctx.adsr_carrier[temp3] = false;
                            }
                        }

                        if (chunk.effect_def == ef_Extended) {
                            const uint8_t lo = chunk.effect % 16;
                            switch (chunk.effect / 16) {
                            case ef_ex_SetAttckRateM:
                            case ef_ex_SetAttckRateC:
                                chunk2.effect = (ctx.adsr_carrier[temp3] ? ef_ex_SetAttckRateC : ef_ex_SetAttckRateM) * 16 + lo;
                                break;
                            case ef_ex_SetDecayRateM:
                            case ef_ex_SetDecayRateC:
                                chunk2.effect = (ctx.adsr_carrier[temp3] ? ef_ex_SetDecayRateC : ef_ex_SetDecayRateM) * 16 + lo;
                                break;
                            case ef_ex_SetSustnLevelM:
                            case ef_ex_SetSustnLevelC:
                                chunk2.effect = (ctx.adsr_carrier[temp3] ? ef_ex_SetSustnLevelC : ef_ex_SetSustnLevelM) * 16 + lo;
                                break;
                            case ef_ex_SetRelRateM:
                            case ef_ex_SetRelRateC:
                                chunk2.effect = (ctx.adsr_carrier[temp3] ? ef_ex_SetRelRateC : ef_ex_SetRelRateM) * 16 + lo;
                                break;
                            }
                        }

                        if (patts.count(songdata.pattern_order[order]) == 0) {
                            if (chunk.effect_def != chunk2.effect_def || chunk.effect != chunk2.effect) {
                                ctx.song.putChunk(patt, static_cast<uint8_t>(temp2), static_cast<uint8_t>(temp3), chunk2);
                            }
                        }
                    }
                }
            }
            ++order;
            patts.insert(patt);
        }
    } while (!(patt >= patterns || order > 0x7f));
}

void import_old_a2m_patterns1(OldImportContext& ctx, const OldVariableData1& old_hash_buffer, int block, int count)
{
    for (int patt = 0; patt <= std::min(count - 1, 15); ++patt) {
        for (int line = 0; line <= 0x3f; ++line) {
            for (int chan = 1; chan <= 9; ++chan) {
                import_old_a2m_event1(ctx, static_cast<uint8_t>(block * 16 + patt), static_cast<uint8_t>(line),
                                      static_cast<uint8_t>(chan), old_hash_buffer[patt][line][chan - 1], true);
            }
        }
    }
}

void import_old_a2m_event2(Song& song, uint8_t patt, uint8_t line, uint8_t chan, const OldChunk& old_chunk)
{
    constexpr uint8_t ef_ManualFSlide = 22;

    Chunk chunk{};
    chunk.note = old_chunk.note;
    chunk.instr_def = old_chunk.instr_def;

    if (old_chunk.effect_def != ef_ManualFSlide) {
        chunk.effect_def = old_chunk.effect_def;
        chunk.effect = old_chunk.effect;
    } else if (old_chunk.effect / 16 != 0) {
        chunk.effect_def = ef_Extended2;
        chunk.effect = static_cast<uint8_t>(ef_ex2_FineTuneUp * 16 + old_chunk.effect / 16);
    } else {
        chunk.effect_def = ef_Extended2;
        chunk.effect = static_cast<uint8_t>(ef_ex2_FineTuneDown * 16 + old_chunk.effect % 16);
    }

    song.putChunk(patt, line, chan, chunk);
}

void import_old_a2m_patterns2(Song& song, const OldVariableData2& hash_buffer, int block, int count)
{
    for (int patt = 0; patt <= std::min(count - 1, 7); ++patt) {
        for (int line = 0; line <= 0x3f; ++line) {
            for (int chan = 1; chan <= 18; ++chan) {
                import_old_a2m_event2(song, static_cast<uint8_t>(block * 8 + patt), static_cast<uint8_t>(line),
                                      static_cast<uint8_t>(chan), hash_buffer[patt][chan - 1][line]);
            }
        }
    }
}

void import_old_flags(FixedSongData& songdata)
{
    if ((songdata.common_flag | 2) == songdata.common_flag) {
        for (auto& f : songdata.lock_flags) f |= 0x10;
    }
    if ((songdata.common_flag | 4) == songdata.common_flag) {
        for (auto& f : songdata.lock_flags) f |= 0x20;
    }
    if ((songdata.common_flag | 0x20) == songdata.common_flag) {
        for (auto& f : songdata.lock_flags) f &= ~3;
    }
}

void import_old_songdata(FixedSongData& songdata, const OldFixedSongData& old_songdata)
{
    songdata.songname.assign(old_songdata.songname);
    songdata.composer.assign(old_songdata.composer);

    for (int temp = 1; temp <= 250; ++temp) {
        at1(songdata.instr_names, temp).assign(at1(old_songdata.instr_names, temp));
        Instrument& ins = at1(songdata.instr_data, temp);
        const OldInstrument& old = at1(old_songdata.instr_data, temp);
        ins.fm_data = old.fm_data;
        ins.panning = old.panning;
        ins.fine_tune = old.fine_tune;
        ins.perc_voice = 0;
    }

    std::memcpy(songdata.pattern_order, old_songdata.pattern_order, sizeof(old_songdata.pattern_order));

    songdata.tempo = old_songdata.tempo;
    songdata.speed = old_songdata.speed;
    songdata.common_flag = old_songdata.common_flag;
    import_old_flags(songdata);
}

void init_old_songdata(OldFixedSongData& old_songdata)
{
    std::memset(&old_songdata, 0, sizeof(old_songdata));
    std::memset(old_songdata.pattern_order, 0x80, sizeof(old_songdata.pattern_order));
    for (int temp = 1; temp <= 250; ++temp) {
        at1(old_songdata.instr_names, temp).assign(" iNS_" + byte2hex(static_cast<uint8_t>(temp)) + "\xF7 ");
    }
}

uint16_t calc_max_speedup(uint8_t tempo)
{
    constexpr bool timer_fix = true;
    uint16_t result = MAX_IRQ_FREQ / tempo;
    int32_t temp;
    do {
        if (tempo == 18 && timer_fix) temp = static_cast<int32_t>((tempo + 0.2) * 20);
        else temp = 250;
        while (temp % (tempo * result) != 0) ++temp;
        if (temp <= MAX_IRQ_FREQ) ++result;
    } while (temp <= MAX_IRQ_FREQ);
    return result - 1;
}

// 旧形式 (1..8) の 1 ブロックを展開する
void decompressOld(uint8_t method, const std::vector<uint8_t>& buf, uint16_t size, std::vector<uint8_t>& out)
{
    switch (method) {
    case 0: out.assign(buf.begin(), buf.begin() + std::min<size_t>(size, buf.size())); break;   // Move
    case 1: LZSS_decompress(buf.data(), buf.size(), size, out); break;
    case 2: LZW_decompress(buf.data(), buf.size(), out); break;
    case 3: SIXPACK_decompress(buf.data(), buf.size(), size, out); break;
    }
}

void normalize_names(FixedSongData& songdata)
{
    for (int index = 1; index <= 255; ++index) {
        auto& name = at1(songdata.instr_names, index);
        name.assign(" iNS_" + byte2hex(static_cast<uint8_t>(index)) + "\xF7 " + pcopy(name.str(), 10, 32));
    }
    for (int index = 0; index <= 0x7f; ++index) {
        auto& name = songdata.pattern_names[index];
        name.assign(" PAT_" + byte2hex(static_cast<uint8_t>(index)) + "  \xF7 " + pcopy(name.str(), 12, 30));
    }
}

void limit_macro_speedup(FixedSongData& songdata)
{
    // オリジナルは変更した旨をダイアログで知らせる (ダイアログはフェーズ 6 で実装する)
    if (songdata.tempo == 0) return;
    const uint16_t max_speedup = calc_max_speedup(songdata.tempo);
    if (songdata.macro_speedup > max_speedup) songdata.macro_speedup = max_speedup;
}

} // namespace loader
