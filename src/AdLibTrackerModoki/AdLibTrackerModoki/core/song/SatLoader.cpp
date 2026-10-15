#include "TrackerLoaders.h"

#include <vector>

#include "EffectDefs.h"
#include "LoaderCommon.h"

using namespace loader;

namespace
{
#pragma pack(push, 1)
    struct SatHeader1   // tHEADER (SAT version 1)
    {
        char ident[4];
        uint8_t vernm;
        uint8_t instt[31][11];
        uint8_t instn[496];
        uint8_t order[255];
        uint16_t nopat;
        uint8_t snlen;
        uint8_t rspos;
        uint16_t calls;
    };

    struct SatHeader2   // tHEADR2 (SAT version 5, 6)
    {
        char ident[4];
        uint8_t vernm;
        uint8_t instt[31][15];
        uint8_t instn[496];
        uint8_t order[128];
        uint16_t nopat;
        uint8_t snlen;
        uint8_t rspos;
        uint16_t calls;
        uint8_t arpgd[512];
    };

    struct Sa2Header   // tHEADER (SA2)
    {
        char ident[4];
        uint8_t vernm;
        uint8_t instt[31][15];
        uint8_t instn[496];
        uint8_t order[128];
        uint16_t nopat;
        uint8_t snlen;
        uint8_t rspos;
        uint16_t snbpm;
        uint8_t arpgd[512];
        uint8_t ordr2[64][9];   // [0..63][1..9]
        uint16_t chans;
    };
#pragma pack(pop)

    static_assert(sizeof(SatHeader1) == 1103);
    static_assert(sizeof(SatHeader2) == 1612);
    static_assert(sizeof(Sa2Header) == 2190);

    // 楽器名の領域 (496 バイト) から inst 番目の名前を取り出す (#16 区切り)
    std::string import_sat_instrument_name(const uint8_t* data, uint8_t inst)
    {
        uint16_t temp1 = 0;
        uint8_t temp2 = 0;
        std::string temp3;
        while (temp1 < 496) {
            if (data[temp1] == 16) ++temp2;
            ++temp1;
            if (temp2 == inst + 1) {
                while (temp1 < 496 + 256 && data[temp1] >= 0x20 && temp3.size() < 22) {
                    temp3 += static_cast<char>(data[temp1]);
                    ++temp1;
                }
                break;
            }
        }
        return temp3;
    }

    uint8_t _sar(int op1, int op2) { return static_cast<uint8_t>(op1 >> op2); }

    void import_sa2_effect(uint8_t effect, uint8_t def1, uint8_t def2, uint8_t& out1, uint8_t& out2)
    {
        // 音量スライド系 (5, 6, 0a) の共通処理
        const auto vol_slide = [&](uint8_t def) {
            out1 = def;
            if (def1 + def2 != 0) {
                if (def1 >= 1 && def1 <= 15) out2 = static_cast<uint8_t>(pas_min(_sar(def1, 2), 1) * 16);
                else out2 = static_cast<uint8_t>(pas_min(_sar(def2, 2), 1));
            } else {
                out2 = static_cast<uint8_t>(def1 * 16 + def2);
            }
        };

        switch (effect) {
        case 0x00: out1 = ef_Arpeggio; out2 = static_cast<uint8_t>(def1 * 16 + def2); break;          // NORMAL PLAY OR ARPEGGIO
        case 0x01: out1 = ef_FSlideUp; out2 = static_cast<uint8_t>(def1 * 16 + def2); break;          // SLIDE UP
        case 0x02: out1 = ef_FSlideDown; out2 = static_cast<uint8_t>(def1 * 16 + def2); break;        // SLIDE DOWN
        case 0x03: out1 = ef_TonePortamento; out2 = static_cast<uint8_t>(def1 * 16 + def2); break;    // TONE PORTAMENTO
        case 0x04: out1 = ef_Vibrato; out2 = static_cast<uint8_t>(def1 * 16 + def2); break;           // VIBRATO
        case 0x05: vol_slide(ef_TPortamVolSlide); break;                                              // TONE PORTAMENTO + VOLUME SLIDE
        case 0x06: vol_slide(ef_VibratoVolSlide); break;                                              // VIBRATO + VOLUME SLIDE
        case 0x08: out1 = ef_Extended; out2 = ef_ex_ExtendedCmd2 * 16 + 0; break;                     // RELEASE SUSTAINING SOUND
        case 0x0a: vol_slide(ef_VolSlide); break;                                                     // VOLUME SLIDE
        case 0x0b:                                                                                    // POSITION JUMP
            if (def1 * 16 + def2 < 128) { out1 = ef_PositionJump; out2 = static_cast<uint8_t>(def1 * 16 + def2); }
            break;
        case 0x0c:                                                                                    // SET VOLUME
            out1 = ef_SetInsVolume;
            out2 = static_cast<uint8_t>(def1 * 16 + def2);
            if (out2 > 63) out2 = 63;
            break;
        case 0x0d:                                                                                    // PATTERN BREAK
            if (def1 * 16 + def2 < 64) { out1 = ef_PatternBreak; out2 = static_cast<uint8_t>(def1 * 16 + def2); }
            break;
        case 0x0f:                                                                                    // SET SPEED
            if (def1 * 16 + def2 < 0x20) {
                out1 = ef_SetSpeed;
                out2 = static_cast<uint8_t>(def1 * 16 + def2);
            } else if (def1 < 16 && def2 < 16) {
                out1 = ef_SetTempo;
                out2 = static_cast<uint8_t>(pas_round((def1 * 16 + def2) / 2.5));
            }
            break;
        default:
            out1 = 0;
            out2 = 0;
            break;
        }
    }

    // 共通の終端処理 (エフェクトなしで解放の指定は、ノートがなければキーオフにする)
    void put_sat_chunk(Song& song, uint8_t pattern, uint8_t line, uint8_t channel, Chunk& chunk)
    {
        if (chunk.effect_def == ef_Extended && chunk.effect == ef_ex_ExtendedCmd2 * 16 && chunk.note == 0) {
            chunk.note = BYTE_NULL;
            chunk.effect_def = 0;
            chunk.effect = 0;
        }
        song.putChunk(pattern, line, channel, chunk);
    }

    // 曲の長さと再開位置からオーダーを設定する
    // (オリジナルは For ループを抜けた後の変数で再開位置を書き込む。ループを 1 回も回らない場合は -1 とみなす)
    template <size_t N>
    void set_order(FixedSongData& songdata, const uint8_t (&order)[N], int last, uint8_t rspos)
    {
        int temp = -1;
        for (int t = 0; t <= last; ++t) {
            temp = t;
            if (t < 128 && t < static_cast<int>(N) && order[t] <= 63) songdata.pattern_order[t] = order[t];
        }
        if (rspos < 128 && temp + 1 < 128) songdata.pattern_order[temp + 1] = static_cast<uint8_t>(0x80 + rspos);
    }
}

bool LoadSat(const std::wstring& path, Song& song, const Song::Defaults& defaults, std::wstring& error)
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

    SatHeader1 header{};
    if (file.size() < sizeof(header)) return corrupted();
    std::memcpy(&header, file.data(), sizeof(header));
    if (std::memcmp(header.ident, "SAdT", 4) != 0) return corrupted();
    if (!(header.vernm == 1 || header.vernm == 5 || header.vernm == 6)) {
        error = L"未対応の形式バージョンです";
        return false;
    }

    const uint8_t SATver = header.vernm;
    SatHeader2 headr2{};
    size_t header_size = sizeof(header);
    if (SATver == 5 || SATver == 6) {
        if (file.size() < sizeof(headr2)) return corrupted();
        std::memcpy(&headr2, file.data(), sizeof(headr2));
        header_size = sizeof(headr2);
    }

    // パターンデータ (buf1 / buf3 / buf4 を通して読む)。範囲外は 0
    int32_t temp5 = static_cast<int32_t>((file.size() - header_size) / (64 * 9 * 5));
    std::vector<uint8_t> data(file.begin() + header_size, file.end());

    song.init(defaults);
    FixedSongData& songdata = song.data();

    songdata.common_flag = songdata.common_flag | 8 | 0x10;
    import_old_flags(songdata);

    songdata.patt_len = 64;
    songdata.nm_tracks = 9;   // adjust_tracks = TRUE
    for (int t = 0; t < 20; ++t) songdata.lock_flags[t] = songdata.lock_flags[t] | 4 | 8;

    songdata.speed = 6;
    if (SATver == 1) {
        songdata.tempo = header.calls < 255 ? static_cast<uint8_t>(header.calls) : 255;
        set_order(songdata, header.order, pas_max(header.snlen - 1, 127), header.rspos);
        temp5 = pas_max(temp5, header.nopat);
        for (int t = 0; t <= 0x1e; ++t) {
            import_sat_instrument(songdata, static_cast<uint8_t>(t + 1), header.instt[t]);
            auto& name = at1(songdata.instr_names, t + 1);
            name.assign(pcopy(name.str(), 1, 9) + truncate_string(import_sat_instrument_name(header.instn, static_cast<uint8_t>(t))));
        }
    } else {
        songdata.tempo = headr2.calls < 255 ? static_cast<uint8_t>(headr2.calls) : 255;
        set_order(songdata, headr2.order, headr2.snlen - 1, headr2.rspos);
        temp5 = pas_max(temp5, headr2.nopat);
        for (int t = 0; t <= 0x1e; ++t) {
            import_sat_instrument(songdata, static_cast<uint8_t>(t + 1), headr2.instt[t]);
            auto& name = at1(songdata.instr_names, t + 1);
            name.assign(pcopy(name.str(), 1, 9) + truncate_string(import_sat_instrument_name(headr2.instn, static_cast<uint8_t>(t))));
        }
    }

    const uint8_t note_inc = SATver == 1 ? 24 : (SATver == 5 ? 12 : 0);
    size_t pos = 0;
    const auto get_byte = [&]() -> uint8_t { return pos < data.size() ? data[pos++] : (++pos, 0); };

    for (int temp2 = 0; temp2 <= temp5 - 1; ++temp2) {
        for (int temp3 = 0; temp3 <= 63; ++temp3) {
            for (int temp4 = 1; temp4 <= 9; ++temp4) {
                const uint8_t byte1 = get_byte();
                const uint8_t byte2 = get_byte();
                const uint8_t byte3 = get_byte();
                const uint8_t byte4 = get_byte();
                const uint8_t byte5 = get_byte();

                Chunk chunk{};
                if (byte2 >= 1 && byte2 <= 31) chunk.instr_def = byte2;
                if (byte1 >= 1 && byte1 <= 12 * 8 + 1) chunk.note = static_cast<uint8_t>(byte1 + note_inc);
                import_sa2_effect(byte3, byte4, byte5, chunk.effect_def, chunk.effect);
                put_sat_chunk(song, static_cast<uint8_t>(temp2), static_cast<uint8_t>(temp3), static_cast<uint8_t>(temp4), chunk);
            }
        }
    }
    return true;
}

bool LoadSa2(const std::wstring& path, Song& song, const Song::Defaults& defaults, std::wstring& error)
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

    Sa2Header header{};
    if (file.size() < sizeof(header)) return corrupted();
    std::memcpy(&header, file.data(), sizeof(header));
    if (std::memcmp(header.ident, "SAdT", 4) != 0) return corrupted();
    if (!(header.vernm == 8 || header.vernm == 9)) {
        error = L"未対応の形式バージョンです";
        return false;
    }

    // version 8 には chans (2 バイト) がない
    const size_t data_pos = header.vernm == 8 ? sizeof(header) - 2 : sizeof(header);
    std::vector<uint8_t> buf1(file.begin() + data_pos, file.end());
    buf1.resize(std::max<size_t>(buf1.size(), 64 * 3 * 256 + 8), 0);

    song.init(defaults);
    FixedSongData& songdata = song.data();

    songdata.common_flag = songdata.common_flag | 8 | 0x10;
    import_old_flags(songdata);

    songdata.patt_len = 64;
    songdata.nm_tracks = 9;   // adjust_tracks = TRUE
    for (int t = 0; t < 20; ++t) songdata.lock_flags[t] = songdata.lock_flags[t] | 4 | 8;

    songdata.speed = 6;
    songdata.tempo = pas_round(header.snbpm / 2.5) < 255 ? static_cast<uint8_t>(pas_round(header.snbpm / 2.5)) : 255;

    // ordr2[pattern][channel] (範囲外はオリジナルでは後続のフィールドを読むが、ここでは 0 とする)
    const auto ordr2 = [&header](int32_t p, int c) -> uint8_t { return (p >= 0 && p < 64) ? header.ordr2[p][c - 1] : 0; };
    const int32_t last = static_cast<int32_t>(header.nopat) - 1;

    int32_t temp2 = 0, temp3 = 0, temp4 = 1;
    do {
        while (ordr2(temp2, temp4) == 0 && temp2 <= last) {
            ++temp4;
            if (temp4 > 9) {
                temp4 = 1;
                ++temp2;
            }
        }

        if (temp2 <= last) {
            const int32_t temp5 = 64 * 3 * (ordr2(temp2, temp4) - 1) + temp3 * 3;
            const auto b = [&buf1](int32_t i) -> uint8_t { return (i >= 0 && static_cast<size_t>(i) < buf1.size()) ? buf1[i] : 0; };
            const uint8_t byte1 = b(temp5), byte2 = b(temp5 + 1), byte3 = b(temp5 + 2);

            Chunk chunk{};
            const uint8_t ins = static_cast<uint8_t>(((byte1 & 1) << 4) + (byte2 >> 4));
            if (ins >= 1 && ins <= 31) chunk.instr_def = ins;
            if ((byte1 >> 1) >= 1 && (byte1 >> 1) <= 12 * 8 + 1) chunk.note = static_cast<uint8_t>(byte1 >> 1);
            import_sa2_effect(byte2 & 0x0f, byte3 >> 4, byte3 & 0x0f, chunk.effect_def, chunk.effect);
            put_sat_chunk(song, static_cast<uint8_t>(temp2), static_cast<uint8_t>(temp3), static_cast<uint8_t>(temp4), chunk);

            ++temp3;
            if (temp3 > 0x3f) {
                temp3 = 0;
                if (temp4 < 9) {
                    ++temp4;
                } else {
                    temp4 = 1;
                    ++temp2;
                }
            }
        }
    } while (!(temp2 > last));

    set_order(songdata, header.order, header.snlen - 1, header.rspos);

    for (int t = 0; t <= 0x1e; ++t) {
        import_sat_instrument(songdata, static_cast<uint8_t>(t + 1), header.instt[t]);
        auto& name = at1(songdata.instr_names, t + 1);
        name.assign(pcopy(name.str(), 1, 9) + truncate_string(import_sat_instrument_name(header.instn, static_cast<uint8_t>(t))));
    }
    return true;
}
