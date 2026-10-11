#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "Song.h"

// 曲ファイルのローダーで共通に使う処理 (iloaders.inc の共通部分の移植)
namespace loader
{

constexpr uint16_t MAX_IRQ_FREQ = 1000;
constexpr uint8_t BYTE_NULL = 0xff;

// adtrack2.ini の既定値 (変換の設定。設定ダイアログはフェーズ 7 で実装する)
constexpr bool adjust_tracks = true;
constexpr bool fix_c_note_bug = true;
constexpr bool accurate_conv = false;
constexpr bool fmk_flip_stereo = true;
constexpr uint8_t force_ins = 0;
constexpr bool mod_description = false;

// GENERIC_IO_BUFFER_SIZE (オリジナルの buf1 / buf2 の大きさ)
constexpr size_t GENERIC_IO_BUFFER_SIZE = 1500 * 1024;

// Pascal 版 (adt2unit.pas) の min / max。名前と逆に、min は下限で切り上げ、max は上限で切り捨てる。
inline int32_t pas_min(int32_t value, int32_t minimum) { return value > minimum ? value : minimum; }
inline int32_t pas_max(int32_t value, int32_t maximum) { return value < maximum ? value : maximum; }

// Pascal の Round (偶数丸め)
int32_t pas_round(double value);

// truncate_string (末尾の #0 / 空白 / #255 を取り除く)
std::string truncate_string(std::string str);

// asciiz_string (#0 までの文字列。#0 がなければ空文字列)
std::string asciiz_string(const std::string& str);

// Pascal の文字配列 (array[1..N] of Char) を String に変換したもの
template <size_t N>
std::string char_array(const char (&arr)[N])
{
    return std::string(arr, N);
}

// HSC-Tracker 形式の楽器 (12 バイト) を取り込む (iloaders.inc の import_hsc_instrument)
void import_hsc_instrument(FixedSongData& songdata, uint8_t inst, const uint8_t* data);

// 標準的な並びの楽器 (11 バイト) を取り込む (iloaders.inc の import_standard_instrument)
void import_standard_instrument(FixedSongData& songdata, uint8_t inst, const uint8_t* data);

// HSC 形式のパターン (tHSC_PATTERNS) を取り込む (HscLoader.cpp。MTK でも使う)
void import_hsc_patterns(Song& song, const uint8_t* data, uint8_t patterns, bool fix_c_note_bug);

// is_4op_chan (songdata.flag_4op から判定する)
bool is_4op_chan(const FixedSongData& songdata, uint8_t chan);
inline bool in_4op_tracks_hi(uint8_t chan) { return chan == 1 || chan == 3 || chan == 5 || chan == 10 || chan == 12 || chan == 14; }

// SensitiveScan (parserio.pas): buf[skip..size) から str を探す。見つからなければ -1
int32_t SensitiveScan(const uint8_t* buf, int32_t skip, int32_t size, const std::string& str);

// CutStr (stringio.pas): 前後の空白を取り除く
std::string CutStr(std::string str);

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

    // オリジナルのグローバルなバッファ buf1 への BlockReadF。
    // バッファは縮めないため、読んだサイズより後ろには以前の内容が残る (オリジナルと同じ)。
    bool readInto(std::vector<uint8_t>& buf, size_t size)
    {
        if (buf.size() < size) buf.resize(size, 0);
        return read(buf.data(), size) == size;
    }

    bool seek(size_t pos)
    {
        if (pos > data_.size()) return false;
        pos_ = pos;
        return true;
    }

    size_t size() const { return data_.size(); }

private:
    const std::vector<uint8_t>& data_;
    size_t pos_ = 0;
};

// ファイル全体を読み込む
bool readFile(const std::wstring& path, std::vector<uint8_t>& data);

// 展開結果を dst へ容量の範囲でコピーする (オリジナルは展開先へ直接書き込む)
void copyTo(void* dst, size_t capacity, const std::vector<uint8_t>& src);
void copyTo(void* dst, size_t capacity, const uint8_t* src, size_t size);

// 構造体 base のフィールド field へ、構造体の終わりまでを容量として書き込む
// (オリジナルは展開結果がフィールドより大きいと後続のフィールドへはみ出して書き込む)
template <class Base, class Field>
void copyToField(Base& base, Field& field, const std::vector<uint8_t>& src)
{
    const size_t offset = reinterpret_cast<uint8_t*>(&field) - reinterpret_cast<uint8_t*>(&base);
    copyTo(&field, sizeof(Base) - offset, src);
}

template <class Base, class Field>
void copyToField(Base& base, Field& field, const uint8_t* src, size_t size)
{
    const size_t offset = reinterpret_cast<uint8_t*>(&field) - reinterpret_cast<uint8_t*>(&base);
    copyTo(&field, sizeof(Base) - offset, src, size);
}

// Pascal の Copy(str, index, count)
std::string pcopy(const std::string& s, size_t index, size_t count);

// Pascal の Insert(prefix, str, 1) (String[N] に収まるよう切り詰める)
template <size_t N>
void pinsertHead(PString<N>& str, const std::string& prefix)
{
    str.assign(prefix + str.str());
}

// Lower(NameOnly(path)) (ファイル名を CP437 で表せない文字は '_' にする)
std::string lower_name_only(const std::wstring& path);

// ----- 旧形式の取り込み -----

struct OldImportContext
{
    Song& song;
    std::array<bool, 10> adsr_carrier{};   // [1..9]
};

void import_old_a2m_event1(OldImportContext& ctx, uint8_t patt, uint8_t line, uint8_t chan,
                           const OldChunk& old_chunk, bool processing_whole_song);
void replace_old_adsr(OldImportContext& ctx, uint8_t patterns);
void import_old_a2m_patterns1(OldImportContext& ctx, const OldVariableData1& old_hash_buffer, int block, int count);
void import_old_a2m_event2(Song& song, uint8_t patt, uint8_t line, uint8_t chan, const OldChunk& old_chunk);
void import_old_a2m_patterns2(Song& song, const OldVariableData2& hash_buffer, int block, int count);
void import_old_flags(FixedSongData& songdata);
void import_old_songdata(FixedSongData& songdata, const OldFixedSongData& old_songdata);
void init_old_songdata(OldFixedSongData& old_songdata);

uint16_t calc_max_speedup(uint8_t tempo);

// 旧形式 (A2M/A2T 1..8, A2P 1..8) の 1 ブロックを展開する (0:Move 1:LZSS 2:LZW 3:SixPack)
void decompressOld(uint8_t method, const std::vector<uint8_t>& buf, uint16_t size, std::vector<uint8_t>& out);

// ロード後の共通処理 (FILE_open の load_flag = 1 の処理のうち曲データに関わるもの。A2P 以外)
void normalize_names(FixedSongData& songdata);

// macro_speedup をタイマーの上限に収める (A2M/A2T ローダーの最後の処理)
void limit_macro_speedup(FixedSongData& songdata);

} // namespace loader
