#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

// AdLib Tracker II の曲データ構造 (typcons1.inc の移植)
//
// オリジナルは {$PACKRECORDS 1} のレコードをファイルから展開したバイト列で直接上書きするため、
// ここでも 1 バイト境界のパック構造体として同じメモリ配置を再現する。
// Pascal の 1 始まり配列は 0 始まりで保持し、参照は at1() を使う。

#pragma pack(push, 1)

// Pascal の String[N] (先頭 1 バイトが長さ)
template <size_t N>
struct PString
{
    uint8_t len;
    char data[N];

    std::string str() const { return std::string(data, std::min<size_t>(len, N)); }

    void assign(const char* s, size_t n)
    {
        len = static_cast<uint8_t>(std::min(n, N));
        std::memcpy(data, s, len);
    }
    void assign(const std::string& s) { assign(s.data(), s.size()); }

    template <size_t M>
    void assign(const PString<M>& s) { assign(s.data, std::min<size_t>(s.len, M)); }
};

struct FmInstData
{
    uint8_t AM_VIB_EG_modulator;
    uint8_t AM_VIB_EG_carrier;
    uint8_t KSL_VOLUM_modulator;
    uint8_t KSL_VOLUM_carrier;
    uint8_t ATTCK_DEC_modulator;
    uint8_t ATTCK_DEC_carrier;
    uint8_t SUSTN_REL_modulator;
    uint8_t SUSTN_REL_carrier;
    uint8_t WAVEFORM_modulator;
    uint8_t WAVEFORM_carrier;
    uint8_t FEEDBACK_FM;
};

struct Instrument   // tADTRACK2_INS
{
    FmInstData fm_data;
    uint8_t panning;
    int8_t fine_tune;
    uint8_t perc_voice;
};

struct OldInstrument   // tOLD_ADTRACK2_INS
{
    FmInstData fm_data;
    uint8_t panning;
    int8_t fine_tune;
};

struct ArpeggioTable
{
    uint8_t length;
    uint8_t speed;
    uint8_t loop_begin;
    uint8_t loop_length;
    uint8_t keyoff_pos;
    uint8_t data[255];
};

struct VibratoTable
{
    uint8_t length;
    uint8_t speed;
    uint8_t delay;
    uint8_t loop_begin;
    uint8_t loop_length;
    uint8_t keyoff_pos;
    int8_t data[255];
};

struct RegisterTableDef
{
    FmInstData fm_data;
    int16_t freq_slide;
    uint8_t panning;
    uint8_t duration;
};

struct RegisterTable
{
    uint8_t length;
    uint8_t loop_begin;
    uint8_t loop_length;
    uint8_t keyoff_pos;
    uint8_t arpeggio_table;
    uint8_t vibrato_table;
    RegisterTableDef data[255];
};

struct MacroTable
{
    ArpeggioTable arpeggio;
    VibratoTable vibrato;
};

using DisFmregCol = uint8_t[28];   // tDIS_FMREG_COL (Boolean 配列)

struct Ins4opFlags
{
    uint8_t num_4op;
    uint8_t idx_4op[128];
};

struct OldFixedSongData   // tOLD_FIXED_SONGDATA
{
    PString<42> songname;
    PString<42> composer;
    PString<32> instr_names[250];
    OldInstrument instr_data[250];
    uint8_t pattern_order[128];
    uint8_t tempo;
    uint8_t speed;
    uint8_t common_flag;
};

struct FixedSongData   // tFIXED_SONGDATA
{
    PString<42> songname;
    PString<42> composer;
    PString<42> instr_names[255];
    Instrument instr_data[255];
    RegisterTable instr_macros[255];
    MacroTable macro_table[255];
    uint8_t pattern_order[128];
    uint8_t tempo;
    uint8_t speed;
    uint8_t common_flag;
    uint16_t patt_len;
    uint8_t nm_tracks;
    uint16_t macro_speedup;
    uint8_t flag_4op;
    uint8_t lock_flags[20];
    PString<42> pattern_names[128];
    DisFmregCol dis_fmreg_col[255];
    Ins4opFlags ins_4op_flags;
    uint8_t reserved_data[1024];
    struct
    {
        uint8_t rows_per_beat;
        int16_t tempo_finetune;
    } bpm_data;
};

struct OldChunk   // tOLD_CHUNK
{
    uint8_t note;
    uint8_t instr_def;
    uint8_t effect_def;
    uint8_t effect;
};

struct Chunk   // tCHUNK
{
    uint8_t note;
    uint8_t instr_def;
    uint8_t effect_def;
    uint8_t effect;
    uint8_t effect_def2;
    uint8_t effect2;
};

// tVARIABLE_DATA: [パターン MOD 8][チャンネル-1][行]
using VariableData = Chunk[8][20][256];

// tPATTERN_DATA: [パターン DIV 8] (16 ブロック × 8 パターン = 128 パターン)
struct PatternData
{
    VariableData block[16];
};

// tOLD_VARIABLE_DATA1: [パターン][行][チャンネル-1]
using OldVariableData1 = OldChunk[16][64][9];

// tOLD_VARIABLE_DATA2: [パターン][チャンネル-1][行]
using OldVariableData2 = OldChunk[8][18][64];

#pragma pack(pop)

static_assert(sizeof(Instrument) == 14);
static_assert(sizeof(RegisterTable) == 3831);
static_assert(sizeof(MacroTable) == 521);
static_assert(sizeof(OldFixedSongData) == 11717);
static_assert(sizeof(FixedSongData) == 1138338);
static_assert(sizeof(VariableData) == 8 * 20 * 256 * 6);

constexpr size_t INSTRUMENT_SIZE = sizeof(Instrument);
constexpr size_t PATTERN_SIZE = 20 * 256 * sizeof(Chunk);

// Pascal の 1 始まり配列 (array[1..N]) を参照する。
// index = 0 の場合はオリジナルと同じく配列直前のメモリ (= 構造体内の前のフィールド) を読む。
template <class T, size_t N>
inline const T& at1(const T (&arr)[N], int index)
{
    return *reinterpret_cast<const T*>(reinterpret_cast<const uint8_t*>(arr) + (index - 1) * static_cast<ptrdiff_t>(sizeof(T)));
}

template <class T, size_t N>
inline T& at1(T (&arr)[N], int index)
{
    return *reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(arr) + (index - 1) * static_cast<ptrdiff_t>(sizeof(T)));
}
