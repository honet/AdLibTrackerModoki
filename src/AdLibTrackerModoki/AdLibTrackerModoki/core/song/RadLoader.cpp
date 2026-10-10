#include "TrackerLoaders.h"

#include <vector>

#include "EffectDefs.h"
#include "LoaderCommon.h"

using namespace loader;

namespace
{
#pragma pack(push, 1)
    struct Header
    {
        char ident[16];   // 'RAD by REALiTY!!'
        uint8_t rmver;    // 10h
        uint8_t xbyte;    // bit7: 説明文あり、bit6: slow-timer、bit[4..0]: 初期スピード
    };
#pragma pack(pop)

    static_assert(sizeof(Header) == 18);

    const char RAD_ID[] = "RAD by REALiTY!!";

    void import_rad_event(Song& song, uint8_t pattern, uint8_t line, uint8_t channel, uint8_t byte1, uint8_t byte2, uint8_t byte3)
    {
        Chunk chunk{};
        const uint8_t ins = static_cast<uint8_t>((byte2 >> 4) + ((byte1 >> 7) << 4));
        if (ins != 0) chunk.instr_def = ins;

        if ((byte1 & 0x0f) >= 1 && (byte1 & 0x0f) <= 12) {
            chunk.note = static_cast<uint8_t>(12 * ((byte1 >> 4) & 7) + (byte1 & 0x0f) + 1);
        } else if ((byte1 & 0x0f) == 0x0f) {
            chunk.note = BYTE_NULL;
        }

        // 音量スライドの値 (1..49: 下げる、51..99: 上げる) を 2 つのエフェクト列に分けて設定する
        const auto vol_slide = [&chunk, byte3](uint8_t def) {
            if (byte3 >= 1 && byte3 <= 49) {
                chunk.effect_def = def;
                chunk.effect = static_cast<uint8_t>(pas_max(byte3, 15));
                if (byte3 > 15) {
                    chunk.effect_def2 = def;
                    chunk.effect2 = static_cast<uint8_t>(pas_max(byte3 - 15, 15));
                }
            } else if (byte3 >= 51 && byte3 <= 99) {
                chunk.effect_def = def;
                chunk.effect = static_cast<uint8_t>(pas_max(byte3 - 50, 15) * 16);
                if (byte3 - 50 > 15) {
                    chunk.effect_def2 = def;
                    chunk.effect2 = static_cast<uint8_t>(pas_max(byte3 - 50 - 15, 15));
                }
            }
        };

        switch (byte2 & 0x0f) {
        case 0x01:   // PORTAMENTO (FREQUENCY SLIDE) UP
            chunk.effect_def = ef_FSlideUp;
            chunk.effect = byte3;
            break;
        case 0x02:   // PORTAMENTO (FREQUENCY SLIDE) DOWN
            chunk.effect_def = ef_FSlideDown;
            chunk.effect = byte3;
            break;
        case 0x03:   // PORTAMENTO TO NOTE
            chunk.effect_def = ef_TonePortamento;
            chunk.effect = byte3;
            break;
        case 0x05:   // PORTAMENTO TO NOTE WITH VOLUME SLIDE
            vol_slide(ef_TPortamVolSlide);
            break;
        case 0x0a:   // VOLUME SLIDE
            vol_slide(ef_VolSlide);
            break;
        case 0x0c:   // SET VOLUME
            chunk.effect_def = ef_SetInsVolume;
            chunk.effect = byte3 < 64 ? byte3 : 63;
            break;
        case 0x0d:   // JUMP TO NEXT PATTERN IN ORDER LIST
            chunk.effect_def = ef_PatternBreak;
            chunk.effect = byte3 < 64 ? byte3 : 63;
            break;
        case 0x0f:   // SET SPEED
            chunk.effect_def = ef_SetSpeed;
            chunk.effect = byte3;
            break;
        }

        // specific corrections for RAd-Tracker event
        const bool porta = chunk.effect_def == ef_TonePortamento || chunk.effect_def == ef_TPortamVolSlide;
        if (porta && chunk.note == BYTE_NULL) chunk.note = 0;
        if (porta) chunk.instr_def = 0;
        if (chunk.note == 0) chunk.instr_def = 0;
        song.putChunk(pattern, line, static_cast<uint8_t>(channel + 1), chunk);
    }
}

bool LoadRad(const std::wstring& path, Song& song, const Song::Defaults& defaults, std::wstring& error)
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
    if (file.size() < sizeof(header)) return corrupted();
    std::memcpy(&header, file.data(), sizeof(header));
    if (std::memcmp(header.ident, RAD_ID, 16) != 0) return corrupted();
    if (header.rmver != 0x10) {
        error = L"未対応の形式バージョンです";
        return false;
    }

    // buf1 (オリジナルのグローバルなバッファ)。Move でずらした後ろには元の内容が残る。
    std::vector<uint8_t> buf1(GENERIC_IO_BUFFER_SIZE, 0);
    int64_t temp = static_cast<int64_t>(std::min(file.size() - sizeof(header), buf1.size()));
    std::memcpy(buf1.data(), file.data() + sizeof(header), static_cast<size_t>(temp));

    // buf1 の範囲外 (オリジナルでは前後のメモリ) は 0 として読む
    const auto b = [&buf1](int64_t index) -> uint8_t {
        return (index >= 0 && index < static_cast<int64_t>(buf1.size())) ? buf1[static_cast<size_t>(index)] : 0;
    };
    // Move(buf1[from], buf1, count)
    const auto shift = [&buf1](int64_t from, int64_t count) {
        if (count > 0 && from >= 0 && from < static_cast<int64_t>(buf1.size())) {
            const size_t n = static_cast<size_t>(std::min<int64_t>(count, static_cast<int64_t>(buf1.size()) - from));
            std::memmove(buf1.data(), buf1.data() + from, n);
        }
    };

    int64_t temp2 = 0;
    int64_t offs0 = sizeof(header);

    if ((header.xbyte | 0x80) == header.xbyte) {
        while (temp2 < temp && buf1[static_cast<size_t>(temp2)] != 0) ++temp2;
        if (temp2 >= temp) return corrupted();

        // 説明文 (dscbuf) は mod_description が OFF の既定値では表示しないため読み飛ばす
        offs0 += temp2 + 1;
        temp -= temp2 + 1;
        shift(temp2 + 1, temp);
    }

    song.init(defaults);
    FixedSongData& songdata = song.data();

    songdata.patt_len = 64;
    songdata.nm_tracks = 9;   // adjust_tracks = TRUE
    songdata.tempo = (header.xbyte | 0x40) == header.xbyte ? 18 : 50;
    songdata.speed = ((header.xbyte & 0x1f) >= 1 && (header.xbyte & 0x1f) <= 31) ? (header.xbyte & 0x1f) : 2;

    temp2 = 0;
    int64_t temp3;
    do {
        temp3 = b(temp2);
        ++temp2;
        if (temp3 != 0 && temp2 + 11 < temp) {
            uint8_t data[12];
            for (int i = 0; i < 12; ++i) data[i] = b(temp2 + i);
            import_hsc_instrument(songdata, static_cast<uint8_t>(temp3), data);
            at1(songdata.instr_data, static_cast<int>(temp3)).fine_tune = 0;
            temp2 += 11;
        }
    } while (!(temp3 == 0 || temp3 >= temp));

    offs0 += temp2;
    temp -= temp2;
    shift(temp2, temp);

    const uint8_t order_len = buf1[0];
    offs0 += order_len + 1;
    if (order_len != 0) copyToField(songdata, songdata.pattern_order, buf1.data() + 1, order_len);

    offs0 += 32 * 2;
    temp -= order_len + 1 + 32 * 2;

    uint16_t pattoffs[32];
    std::memcpy(pattoffs, buf1.data() + order_len + 1, sizeof(pattoffs));
    shift(order_len + 32 * 2 + 1, temp);

    const int64_t temp5 = temp;
    for (int pat = 0; pat <= 31; ++pat) {
        int64_t line = 0;
        int64_t pos = 0;
        if (pattoffs[pat] != 0 && pattoffs[pat] <= file.size()) {
            const int64_t base = static_cast<int64_t>(pattoffs[pat]) - offs0;
            do {
                line = b(base + pos);
                int64_t temp4;
                do {
                    ++pos;
                    temp4 = b(base + pos);
                    if ((b(base + pos + 2) & 0x0f) != 0) {
                        if ((temp4 & 0x0f) <= 8) {
                            import_rad_event(song, static_cast<uint8_t>(pat), static_cast<uint8_t>(line & 0x3f),
                                             static_cast<uint8_t>(temp4 & 0x0f), b(base + pos + 1), b(base + pos + 2),
                                             b(base + pos + 3));
                        }
                        pos += 3;
                    } else {
                        if ((temp4 & 0x0f) <= 8) {
                            import_rad_event(song, static_cast<uint8_t>(pat), static_cast<uint8_t>(line & 0x3f),
                                             static_cast<uint8_t>(temp4 & 0x0f), b(base + pos + 1), b(base + pos + 2), 0);
                        }
                        pos += 2;
                    }
                } while (!((temp4 | 0x80) == temp4 || pos > temp5));
                ++pos;
            } while (!((line | 0x80) == line || pos > temp5));
        }
    }
    return true;
}
