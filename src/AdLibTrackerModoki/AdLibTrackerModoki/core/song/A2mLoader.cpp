#include "A2mLoader.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <set>
#include <vector>

#include "Depack.h"
#include "EffectDefs.h"

namespace
{
    constexpr uint8_t FFVER_A2M = 14;
    constexpr size_t OLD_A2M_HEADER_SIZE = 26;
    constexpr uint16_t MAX_IRQ_FREQ = 1000;

#pragma pack(push, 1)
    struct OldHeader   // tOLD_HEADER
    {
        char ident[10];
        uint32_t crc32;
        uint8_t ffver;
        uint8_t patts;
        uint16_t b0len, b1len, b2len, b3len, b4len, b5len, b6len, b7len, b8len;
    };

    struct Header   // tHEADER
    {
        char ident[10];
        uint32_t crc32;
        uint8_t ffver;
        uint8_t patts;
        uint32_t b0len;
        uint32_t b1len[16];
    };
#pragma pack(pop)

    static_assert(sizeof(OldHeader) == 34);
    static_assert(sizeof(Header) == 84);

    const char A2M_ID[] = "_A2module_";

    // BlockReadF / SeekF 相当
    class Reader
    {
    public:
        explicit Reader(const std::vector<uint8_t>& data) : data_(data) {}

        // 読めたバイト数を返す
        size_t read(void* dst, size_t size)
        {
            const size_t n = std::min(size, data_.size() - std::min(pos_, data_.size()));
            if (n > 0) std::memcpy(dst, data_.data() + pos_, n);
            pos_ += n;
            return n;
        }

        // ブロックを読み込む (読めたサイズが要求と一致すれば true)
        bool readBlock(std::vector<uint8_t>& buf, size_t size)
        {
            buf.resize(size);
            return read(buf.data(), size) == size;
        }

        bool seek(size_t pos)
        {
            if (pos > data_.size()) return false;
            pos_ = pos;
            return true;
        }

    private:
        const std::vector<uint8_t>& data_;
        size_t pos_ = 0;
    };

    // 展開結果を dst へ容量の範囲でコピーする (オリジナルは展開先へ直接書き込む)
    void copyTo(void* dst, size_t capacity, const std::vector<uint8_t>& src)
    {
        std::memcpy(dst, src.data(), std::min(capacity, src.size()));
    }

    // Pascal の Copy(str, index, count)
    std::string pcopy(const std::string& s, size_t index, size_t count)
    {
        if (index < 1 || index > s.size()) return std::string();
        return s.substr(index - 1, count);
    }

    // Pascal の Insert(prefix, str, 1) (String[42] に収まるよう切り詰める)
    template <size_t N>
    void pinsertHead(PString<N>& str, const std::string& prefix)
    {
        str.assign(prefix + str.str());
    }

    // ----- 旧形式の取り込み -----

    struct OldImportContext
    {
        Song& song;
        std::array<bool, 10> adsr_carrier{};   // [1..9]
    };

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
}

bool LoadA2m(const std::wstring& path, Song& song, const Song::Defaults& defaults, std::wstring& error)
{
    std::vector<uint8_t> file;
    {
        std::ifstream f(std::filesystem::path(path), std::ios::binary);
        if (!f) {
            error = L"ファイルを開けません";
            return false;
        }
        file.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    }

    const auto corrupted = [&error] {
        error = L"ファイルが壊れています";
        return false;
    };

    Reader reader(file);
    Header header{};
    if (reader.read(&header, sizeof(header)) != sizeof(header) ||
        std::memcmp(header.ident, A2M_ID, 10) != 0) {
        return corrupted();
    }
    if (header.ffver < 1 || header.ffver > FFVER_A2M) {
        error = L"未対応の形式バージョンです (" + std::to_wstring(header.ffver) + L")";
        return false;
    }

    // 旧形式の展開先 (オリジナルのグローバル変数 old_songdata / old_hash_buffer / hash_buffer)
    struct OldBuffers
    {
        OldFixedSongData old_songdata;
        OldVariableData1 old_hash_buffer;
        OldVariableData2 hash_buffer;
    };
    auto old = std::make_unique<OldBuffers>();
    std::memset(old.get(), 0, sizeof(OldBuffers));
    OldFixedSongData* old_songdata = &old->old_songdata;
    OldVariableData1* old_hash_buffer = &old->old_hash_buffer;
    OldVariableData2* hash_buffer = &old->hash_buffer;
    init_old_songdata(*old_songdata);

    FixedSongData& songdata = song.data();
    std::vector<uint8_t> buf1;
    std::vector<uint8_t> unpacked;

    if (header.ffver >= 1 && header.ffver <= 4) {
        OldImportContext ctx{ song };
        reader.seek(0);
        OldHeader header2{};
        if (reader.read(&header2, sizeof(header2)) != sizeof(header2) ||
            std::memcmp(header2.ident, A2M_ID, 10) != 0) {
            return corrupted();
        }

        const uint16_t xlen[3] = { header2.b2len, header2.b3len, header2.b4len };
        const int blocks = (header2.patts - 1) / 16;

        if (!reader.seek(OLD_A2M_HEADER_SIZE)) return corrupted();
        uint32_t crc = 0xffffffff;
        if (!reader.readBlock(buf1, header2.b0len)) return corrupted();
        crc = Update32(buf1.data(), buf1.size(), crc);
        if (!reader.readBlock(buf1, header2.b1len)) return corrupted();
        crc = Update32(buf1.data(), buf1.size(), crc);
        for (int temp2 = 0; temp2 <= 2; ++temp2) {
            if (blocks > temp2) {
                if (!reader.readBlock(buf1, xlen[temp2])) return corrupted();
                crc = Update32(buf1.data(), buf1.size(), crc);
            }
        }
        crc = Update32(&header2.b0len, 2, crc);
        crc = Update32(&header2.b1len, 2, crc);
        for (int temp2 = 0; temp2 <= 2; ++temp2) {
            crc = Update32(&xlen[temp2], 2, crc);
        }
        if (crc != header2.crc32) {
            error = L"CRC が一致しません";
            return false;
        }

        song.init(defaults);
        songdata.patt_len = 64;
        songdata.nm_tracks = 9;   // adjust_tracks = TRUE

        const uint8_t method = static_cast<uint8_t>(4 - header2.ffver);   // 4:Move 3:LZSS 2:LZW 1:SixPack

        reader.seek(OLD_A2M_HEADER_SIZE);
        if (!reader.readBlock(buf1, header2.b0len)) return corrupted();
        decompressOld(method, buf1, header2.b0len, unpacked);
        copyTo(old_songdata, sizeof(OldFixedSongData), unpacked);

        for (int temp = 1; temp <= 250; ++temp) {
            at1(old_songdata->instr_data, temp).panning = 0;
        }

        if (!reader.readBlock(buf1, header2.b1len)) return corrupted();
        decompressOld(method, buf1, header2.b1len, unpacked);
        copyTo(old_hash_buffer, sizeof(OldVariableData1), unpacked);
        import_old_a2m_patterns1(ctx, *old_hash_buffer, 0, 16);

        for (int temp2 = 0; temp2 <= 2; ++temp2) {
            if (blocks > temp2) {
                if (!reader.readBlock(buf1, xlen[temp2])) return corrupted();
                decompressOld(method, buf1, xlen[temp2], unpacked);
                copyTo(old_hash_buffer, sizeof(OldVariableData1), unpacked);
                import_old_a2m_patterns1(ctx, *old_hash_buffer, temp2 + 1, 16);
            }
        }

        replace_old_adsr(ctx, header2.patts);
        import_old_songdata(songdata, *old_songdata);
    }

    if (header.ffver >= 5 && header.ffver <= 8) {
        reader.seek(0);
        OldHeader header2{};
        if (reader.read(&header2, sizeof(header2)) != sizeof(header2) ||
            std::memcmp(header2.ident, A2M_ID, 10) != 0) {
            return corrupted();
        }

        const uint16_t xlen[7] = { header2.b2len, header2.b3len, header2.b4len, header2.b5len,
                                   header2.b6len, header2.b7len, header2.b8len };
        const int blocks = (header2.patts - 1) / 8;

        uint32_t crc = 0xffffffff;
        if (!reader.readBlock(buf1, header2.b0len)) return corrupted();
        crc = Update32(buf1.data(), buf1.size(), crc);
        if (!reader.readBlock(buf1, header2.b1len)) return corrupted();
        crc = Update32(buf1.data(), buf1.size(), crc);
        for (int temp2 = 0; temp2 <= 6; ++temp2) {
            if (blocks > temp2) {
                if (!reader.readBlock(buf1, xlen[temp2])) return corrupted();
                crc = Update32(buf1.data(), buf1.size(), crc);
            }
        }
        crc = Update32(&header2.b0len, 2, crc);
        crc = Update32(&header2.b1len, 2, crc);
        for (int temp2 = 0; temp2 <= 6; ++temp2) {
            crc = Update32(&xlen[temp2], 2, crc);
        }
        if (crc != header2.crc32) {
            error = L"CRC が一致しません";
            return false;
        }

        song.init(defaults);
        songdata.patt_len = 64;
        songdata.nm_tracks = 18;   // adjust_tracks = TRUE

        const uint8_t method = static_cast<uint8_t>(8 - header2.ffver);   // 8:Move 7:LZSS 6:LZW 5:SixPack

        if (!reader.seek(sizeof(OldHeader))) return corrupted();
        if (!reader.readBlock(buf1, header2.b0len)) return corrupted();
        decompressOld(method, buf1, header2.b0len, unpacked);
        copyTo(old_songdata, sizeof(OldFixedSongData), unpacked);

        if (!reader.readBlock(buf1, header2.b1len)) return corrupted();
        decompressOld(method, buf1, header2.b1len, unpacked);
        copyTo(hash_buffer, sizeof(OldVariableData2), unpacked);
        import_old_a2m_patterns2(song, *hash_buffer, 0, 8);

        for (int temp2 = 0; temp2 <= 6; ++temp2) {
            if (blocks > temp2) {
                if (!reader.readBlock(buf1, xlen[temp2])) return corrupted();
                // オリジナルは 2 ブロック目以降も b2len をサイズとして渡している
                decompressOld(method, buf1, header2.b2len, unpacked);
                copyTo(hash_buffer, sizeof(OldVariableData2), unpacked);
                import_old_a2m_patterns2(song, *hash_buffer, temp2 + 1, 8);
            }
        }
        import_old_songdata(songdata, *old_songdata);
    }

    if (header.ffver >= 9 && header.ffver <= FFVER_A2M) {
        const bool lzh = header.ffver >= 12;
        const int blocks = (header.patts - 1) / 8;

        uint32_t crc = 0xffffffff;
        if (!reader.readBlock(buf1, header.b0len)) return corrupted();
        crc = Update32(buf1.data(), buf1.size(), crc);
        if (!reader.readBlock(buf1, header.b1len[0])) return corrupted();
        crc = Update32(buf1.data(), buf1.size(), crc);
        for (int temp2 = 1; temp2 <= 15; ++temp2) {
            if (blocks > temp2 - 1) {
                if (!reader.readBlock(buf1, header.b1len[temp2])) return corrupted();
                crc = Update32(buf1.data(), buf1.size(), crc);
            }
        }
        crc = Update32(&header.b0len, 2, crc);
        for (int temp2 = 0; temp2 <= 15; ++temp2) {
            crc = Update32(&header.b1len[temp2], 2, crc);
        }
        if (crc != header.crc32) {
            error = L"CRC が一致しません";
            return false;
        }

        song.init(defaults);

        auto unpack = [&](uint32_t size) {
            if (lzh) LZH_decompress(buf1.data(), buf1.size(), size, unpacked);
            else APACK_decompress(buf1.data(), buf1.size(), unpacked);
        };
        auto copyToPatterns = [&](int block) {
            const size_t offset = block * sizeof(VariableData);
            copyTo(reinterpret_cast<uint8_t*>(&song.patterns()) + offset, sizeof(PatternData) - offset, unpacked);
        };

        if (!reader.seek(sizeof(Header))) return corrupted();
        if (!reader.readBlock(buf1, header.b0len)) return corrupted();
        unpack(header.b0len);
        copyTo(&songdata, sizeof(FixedSongData), unpacked);

        if (!reader.readBlock(buf1, header.b1len[0])) return corrupted();

        if (header.ffver == 9) import_old_flags(songdata);

        for (int temp = 1; temp <= 255; ++temp) {
            auto& name = at1(songdata.instr_names, temp);
            pinsertHead(name, std::string(1, name.data[0]) + "iNS_" + byte2hex(static_cast<uint8_t>(temp)) + "\xF7 ");
        }
        if (header.ffver >= 11) {
            for (int temp = 0; temp <= 0x7f; ++temp) {
                auto& name = songdata.pattern_names[temp];
                pinsertHead(name, std::string(1, name.data[0]) + "PAT_" + byte2hex(static_cast<uint8_t>(temp)) + "  \xF7 ");
            }
        }

        unpack(header.b1len[0]);
        copyToPatterns(0);
        for (int temp2 = 1; temp2 <= 15; ++temp2) {
            if (blocks > temp2 - 1) {
                if (!reader.readBlock(buf1, header.b1len[temp2])) return corrupted();
                unpack(header.b1len[temp2]);
                copyToPatterns(temp2);
            }
        }
    }

    // ロード後の共通処理 (load_flag = 1 の名前の正規化)
    for (int index = 1; index <= 255; ++index) {
        auto& name = at1(songdata.instr_names, index);
        name.assign(" iNS_" + byte2hex(static_cast<uint8_t>(index)) + "\xF7 " + pcopy(name.str(), 10, 32));
    }
    for (int index = 0; index <= 0x7f; ++index) {
        auto& name = songdata.pattern_names[index];
        name.assign(" PAT_" + byte2hex(static_cast<uint8_t>(index)) + "  \xF7 " + pcopy(name.str(), 12, 30));
    }

    if (songdata.tempo != 0) {
        const uint16_t max_speedup = calc_max_speedup(songdata.tempo);
        if (songdata.macro_speedup > max_speedup) {
            songdata.macro_speedup = max_speedup;
        }
    }

    song.title = std::filesystem::path(path).filename().wstring();
    return true;
}
