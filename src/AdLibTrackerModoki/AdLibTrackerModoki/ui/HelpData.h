#pragma once

// 本家のヘルプの本文 (adt2text.pas の help_data)
namespace at2
{

constexpr int HELP_LINES = 1159;   // SDL 版

// [0..HELP_LINES-1] (オリジナルは [1..HELP_LINES])。
// '@topic:名前' は話題の始まり、'@input:名前' は設定で変わる行 (HELP が差し替える)
extern const char* const help_data[HELP_LINES];

} // namespace at2
