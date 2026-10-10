#pragma once

#include <string>

#include "Song.h"

// 他のトラッカー形式のローダー (iloaders.inc の移植)
//
// いずれも成功すると song を読み込んだ内容で置き換える。失敗時は false を返し、error に理由を入れる。
// 変換の設定 (fix_c_note_bug など) は adtrack2.ini の既定値を使う。

// HSC-Tracker
bool LoadHsc(const std::wstring& path, Song& song, const Song::Defaults& defaults, std::wstring& error);

// Reality AdLib Tracker (RAD 1.0)
bool LoadRad(const std::wstring& path, Song& song, const Song::Defaults& defaults, std::wstring& error);

// Scream Tracker 3 (AdLib 楽器のみ)
bool LoadS3m(const std::wstring& path, Song& song, const Song::Defaults& defaults, std::wstring& error);
