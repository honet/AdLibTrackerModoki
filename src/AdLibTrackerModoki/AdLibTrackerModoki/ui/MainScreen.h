#pragma once

#include <windows.h>

// メイン画面 (STATUS / PATTERN ORDER / パターン / アナライザ) とプレイヤー操作のメインループ
//
// adt2ext2.pas の PROGRAM_SCREEN_init、realtime.inc、ipattern.inc / ipattord.inc の表示処理と
// PATTERN_trace、および PATTERN_edit のうちカーソル移動・再生操作を移植したもの。
// 編集機能はフェーズ 5 以降で追加する。
namespace at2
{

// メインループ。ESC / F10 またはウィンドウを閉じると戻る。
void run_main_screen(HWND hwnd);

} // namespace at2
