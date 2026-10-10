#pragma once

#include <cstdint>
#include <string>

#include "Song.h"

// A2P (AdLib Tracker II pattern) ローダー (iloaders.inc の a2p_file_loader の移植)
//
// 曲は置き換えず、パターン pattern の内容とパターン名だけを読み込んだもので上書きする。
// (オリジナルはパターン一覧で複数のパターンを選んでいればそのすべてへ読み込むが、
//  パターン一覧は編集機能とともに実装するため、ここでは 1 パターンのみ)
// 対応形式バージョン: 1..11
bool LoadA2p(const std::wstring& path, Song& song, uint8_t pattern, std::wstring& error);
