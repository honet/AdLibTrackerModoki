#pragma once

#include <cstdint>
#include <string>

#include "Song.h"

// 曲ファイルの読み込み (adt2extn.pas の FILE_open のうち、拡張子による振り分けとロード後の処理)
//
// 本家の F3 で開ける形式を拡張子で判別して各ローダーを呼ぶ。

enum class ModuleKind
{
    Song,          // 曲全体を読み込む形式 (A2M / A2T など)
    Pattern,       // 現在のパターンへ読み込む形式 (A2P)
    Unsupported,
};

ModuleKind GetModuleKind(const std::wstring& path);

// 曲全体を読み込む形式のファイルを song へ読み込む。失敗時は false を返し、error に理由を入れる。
bool LoadSongModule(const std::wstring& path, Song& song, const Song::Defaults& defaults, std::wstring& error);

// A2P を song のパターン pattern (to_marked のときはマークしたパターンすべて) へ読み込む
bool LoadPatternModule(const std::wstring& path, Song& song, uint8_t pattern, bool to_marked, std::wstring& error);

// ロード後の処理: 曲名・作者名・楽器名の使えない文字を置き換える (FILE_open の load_flag <> BYTE_NULL の処理)
void filter_loaded_names(FixedSongData& songdata);

// ファイル選択ダイアログ用のフィルタ (本家の FILE_open のマスクと同じ拡張子)
const wchar_t* ModuleFileFilter();
