#pragma once

#include <memory>
#include <string>

#include "SongData.h"

// 曲データ一式 (songdata + pattdata)
class Song
{
public:
    // init_songdata で使われる初期値
    struct Defaults
    {
        uint8_t tempo = 50;           // init_tempo
        uint8_t speed = 6;            // init_speed
        uint16_t macro_speedup = 1;   // init_macro_speedup
        uint16_t patt_len = 64;
        uint8_t nm_tracks = 18;
        uint8_t mark_line = 4;
    };

    Song();

    // init_songdata のうち曲データに関わる部分
    void init(const Defaults& defaults);

    void getChunk(uint8_t pattern, uint8_t line, uint8_t channel, Chunk& chunk) const;
    void putChunk(uint8_t pattern, uint8_t line, uint8_t channel, const Chunk& chunk);

    FixedSongData& data() { return *songdata_; }
    const FixedSongData& data() const { return *songdata_; }

    PatternData& patterns() { return *pattdata_; }
    const PatternData& patterns() const { return *pattdata_; }

    // パターン番号・チャンネル(1始まり)・行から参照する
    Chunk& chunk(uint8_t pattern, uint8_t channel, uint8_t line)
    {
        return pattdata_->block[pattern / 8][pattern % 8][channel - 1][line];
    }

    std::wstring title;   // songdata_title

private:
    std::unique_ptr<FixedSongData> songdata_;
    std::unique_ptr<PatternData> pattdata_;
};

// Pascal の byte2hex
std::string byte2hex(uint8_t value);
