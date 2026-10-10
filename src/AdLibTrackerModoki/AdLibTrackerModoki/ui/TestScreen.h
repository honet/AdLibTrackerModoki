#pragma once

#include <windows.h>

// フェーズ3 の確認用画面 (テキスト画面・キーボード入力・再生状態の表示)
//
// 本家と同じく getkey() でキー入力を待つループで動く。ウィンドウが閉じられると戻る。
// 操作: Ctrl+O 開く / F5 再生 / F8 停止 / F9 エミュレータ切替 / Alt+1..3 画面モード / ESC 終了
void run_test_screen(HWND hwnd);
