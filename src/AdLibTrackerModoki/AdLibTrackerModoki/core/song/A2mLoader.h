#pragma once

#include <string>

#include "Song.h"

// A2M (AdLib Tracker II module) ローダー (iloaders.inc の a2m_file_loader の移植)
//
// 成功すると song を読み込んだ内容で置き換える。失敗時は false を返し、error に理由を入れる。
// 対応形式バージョン: 1..14
bool LoadA2m(const std::wstring& path, Song& song, const Song::Defaults& defaults, std::wstring& error);
