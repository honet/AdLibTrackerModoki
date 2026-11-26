#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <windows.h>

// クローン版独自機能: ジュークボックス (指定フォルダ内の対応ファイルを順に演奏する)
//
// ここでは曲リストと現在位置だけを扱う。曲の読み込み・演奏開始・曲送りの判定は
// メイン画面 (ui/MainScreen.cpp の jukebox_poll など) で行う。
// A2P (パターンだけのファイル) と対応していない拡張子は曲リストに入れない。

// folder 内の対応ファイルで曲リストを作り、ジュークボックスを有効にする。
// 曲が 1 つもなければ false を返し、状態は変えない。
bool jukebox_build(const std::wstring& folder, bool subfolders, bool shuffle);

bool jukebox_active();
void jukebox_stop();

size_t jukebox_count();
size_t jukebox_index();              // 0 始まり
const std::wstring& jukebox_current();

// 曲リストを dir (+1 / -1) だけ進め、そのファイルのパスを返す。端では反対側へ折り返す。
// シャッフル中は 1 周するたびに並べ直す。
const std::wstring& jukebox_step(int dir);

// 曲リストの index 番目 (0 始まり) を今の曲にして、ジュークボックスを有効にする。
// ジュークボックスを止めた後でも、曲リストが残っていれば再開できる。
bool jukebox_jump(size_t index);

// 演奏順の曲リスト (フルパス) と、それを作ったフォルダ
const std::vector<std::wstring>& jukebox_list();
const std::wstring& jukebox_folder();

// 曲リストの並びが変わる (フォルダを開く・シャッフルの切り替え・1 周後の並べ直し) たびに増える値
uint32_t jukebox_list_version();

// シャッフルを切り替える。今の曲を先頭にして残りを並べ替える (オフなら名前順へ戻す)。
void jukebox_set_shuffle(bool shuffle);

// フォルダ選択ダイアログ。folder を初期フォルダとして開き、選ばれたら folder を書き換えて true を返す。
bool jukebox_pick_folder(HWND owner, std::wstring& folder);

// 設定ダイアログ (ループ回数・最大演奏時間・サブフォルダ・フェードアウト)。OK なら保存して true を返す。
bool jukebox_settings_dialog(HWND owner);
