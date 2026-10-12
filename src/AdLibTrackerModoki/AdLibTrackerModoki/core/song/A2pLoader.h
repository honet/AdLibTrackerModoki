#pragma once

#include <cstdint>
#include <string>

#include "Song.h"

// A2P (AdLib Tracker II pattern) ローダー (iloaders.inc の a2p_file_loader の移植)
//
// 曲は置き換えず、パターン pattern の内容とパターン名だけを読み込んだもので上書きする。
// to_marked のときは、パターン一覧でマークしたパターン (名前の先頭が ' ' でないもの) すべてへ読み込む。
// 対応形式バージョン: 1..11
bool LoadA2p(const std::wstring& path, Song& song, uint8_t pattern, bool to_marked, std::wstring& error);
