#pragma once

#include <cstdint>
#include <string>

#include "ModuleSaver.h"
#include "SongData.h"

// 楽器ファイル (A2I / A2F) の読み書きと、4OP の楽器の組の扱い
// (adt2ext3.pas の a2i_file_loader / a2f_file_loader、adt2extn.pas の _a2i_saver / _a2f_saver、instedit.inc)
//
// 読み書きの対象は current_inst の楽器。4OP の組の楽器なら組の 2 つをまとめて扱う。
namespace a2ins
{

// 4OP の組の 1 つ目の印 (楽器名の 1 文字目)
constexpr char _4op_flag_chr_beg = '\xAC';
constexpr char _4op_flag_chr_end = '\xAD';

// ins が 4OP の組の 1 つ目か
bool check_4op_flag(const FixedSongData& songdata, uint8_t ins);

// ins を含む 4OP の組 (上位バイト: 1 つ目、下位バイト: 2 つ目)。組でなければ 0
uint16_t check_4op_instrument(const FixedSongData& songdata, uint8_t ins);

void reset_4op_flag(FixedSongData& songdata, uint8_t ins);
void set_4op_flag(FixedSongData& songdata, uint8_t ins);

// 4OP の組の楽器の名前の 1 文字目の印を付け直す
void update_4op_flag_marks(FixedSongData& songdata);

// 拡張子で A2I / A2F を判別して current_inst へ読み込む。失敗時は false を返し、error に理由を入れる。
// 読み込んだ後、FILE_open と同じく曲名・作者名・楽器名の使えない文字を置き換える。
bool LoadInstrumentFile(const std::wstring& path, FixedSongData& songdata, uint8_t current_inst, std::wstring& error);

SaveResult SaveA2i(const std::wstring& path, const FixedSongData& songdata, uint8_t current_inst);
SaveResult SaveA2f(const std::wstring& path, const FixedSongData& songdata, uint8_t current_inst);

} // namespace a2ins
