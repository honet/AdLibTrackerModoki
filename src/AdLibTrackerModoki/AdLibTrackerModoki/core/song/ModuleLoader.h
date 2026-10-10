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

// A2P を song のパターン pattern へ読み込む
bool LoadPatternModule(const std::wstring& path, Song& song, uint8_t pattern, std::wstring& error);

// ファイル選択ダイアログ用のフィルタ (本家の FILE_open のマスクと同じ拡張子)
const wchar_t* ModuleFileFilter();
