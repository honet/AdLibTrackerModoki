#pragma once

#include <cstdint>
#include <string>

#include "Sound.h"

// クローン版独自の設定 (画面モード・ウィンドウの拡大率・音源) の永続化
//
// 実行ファイルと同じフォルダの AdLibTrackerModoki.ini に保存する。
// ファイルがない、または値が範囲外のときは既定値を使う。

struct AppSettings
{
    uint8_t screen_mode = 0;                     // 0: 90x40, 1: 120x50, 2: 180x60
    int window_scale = 1;                        // ウィンドウの拡大率 (1〜4)
    EmulatorType emulator = EmulatorType::At2;
    bool keyon_delay = true;

    // ジュークボックス
    int jukebox_loops = 1;                       // 曲がこの回数ひと回りしたら次の曲へ (1〜99)
    int jukebox_max_minutes = 5;                 // 1 曲の最大演奏時間 (分, 1〜59)
    bool jukebox_shuffle = false;
    bool jukebox_subfolders = false;
    bool jukebox_fade = true;                    // 曲を切り替えるときにフェードアウトする
    std::wstring jukebox_folder;                 // 最後に選んだフォルダ
};

// 起動時に ini から読み込んだ値 (メニューで切り替えるたびに更新して保存する)
AppSettings& app_settings();

void settings_load();
void settings_save();
