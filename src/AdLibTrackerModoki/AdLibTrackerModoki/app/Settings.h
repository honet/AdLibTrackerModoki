#pragma once

#include <cstdint>

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
};

// 起動時に ini から読み込んだ値 (メニューで切り替えるたびに更新して保存する)
AppSettings& app_settings();

void settings_load();
void settings_save();
