#pragma once

#include <cstddef>

#include <windows.h>

// クローン版独自機能: ジュークボックスのプレイリスト表示ウィンドウ
//
// メインウィンドウに所有されるウィンドウで、ジュークボックスの曲リストを演奏順に表示する。
// 演奏中の曲には印を付けて選択し、見える位置までスクロールする。
// 曲をダブルクリックするか Enter を押すと、メインウィンドウへ IDM_JUKEBOX_PLAY_SELECTED を送る
// (選ばれた番号は playlist_window_take_selection で受け取る)。
// 閉じるボタンではウィンドウを隠すだけにする。位置・大きさ・表示状態は設定ファイルに保存する。

// ウィンドウを (隠した状態で) 作る。メインウィンドウを作った後に 1 回呼ぶ。
void playlist_window_init(HWND owner);

// 位置・大きさを保存してウィンドウを破棄する
void playlist_window_done();

// 表示・非表示を切り替える。remember が true なら表示状態を設定ファイルへ保存する。
void playlist_window_show(bool show, bool remember = true);
bool playlist_window_visible();

// ジュークボックスの状態 (曲リストの並び・今の曲) を表示へ反映する
void playlist_window_sync();

// ダブルクリック・Enter で選ばれた曲の番号を受け取る (なければ false)
bool playlist_window_take_selection(size_t& index);
