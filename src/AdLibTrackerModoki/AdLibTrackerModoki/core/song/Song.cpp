#include "Song.h"

std::string byte2hex(uint8_t value)
{
    static const char digits[] = "0123456789ABCDEF";
    return { digits[value >> 4], digits[value & 0x0f] };
}

Song::Song()
    : songdata_(std::make_unique<FixedSongData>())
    , pattdata_(std::make_unique<PatternData>())
{
    init(Defaults());
}

void Song::init(const Defaults& defaults)
{
    std::memset(songdata_.get(), 0, sizeof(FixedSongData));
    std::memset(songdata_->pattern_order, 0x80, sizeof(songdata_->pattern_order));
    std::memset(pattdata_.get(), 0, sizeof(PatternData));

    songdata_->tempo = defaults.tempo;
    songdata_->speed = defaults.speed;
    songdata_->macro_speedup = defaults.macro_speedup;
    songdata_->patt_len = defaults.patt_len;
    songdata_->nm_tracks = defaults.nm_tracks;
    songdata_->bpm_data.rows_per_beat = defaults.mark_line;
    songdata_->bpm_data.tempo_finetune = 0;

    for (int i = 1; i <= 255; ++i) {
        at1(songdata_->instr_names, i).assign(" iNS_" + byte2hex(static_cast<uint8_t>(i)) + "\xF7 ");
    }
    for (int i = 0; i <= 0x7f; ++i) {
        songdata_->pattern_names[i].assign(" PAT_" + byte2hex(static_cast<uint8_t>(i)) + "  \xF7 ");
    }
    title.clear();
}

void Song::getChunk(uint8_t pattern, uint8_t line, uint8_t channel, Chunk& chunk) const
{
    if (pattern < 128 && channel >= 1 && channel <= 20) {
        chunk = pattdata_->block[pattern / 8][pattern % 8][channel - 1][line];
    }
}

void Song::putChunk(uint8_t pattern, uint8_t line, uint8_t channel, const Chunk& chunk)
{
    if (pattern < 128 && channel >= 1 && channel <= 20) {
        pattdata_->block[pattern / 8][pattern % 8][channel - 1][line] = chunk;
    }
}
