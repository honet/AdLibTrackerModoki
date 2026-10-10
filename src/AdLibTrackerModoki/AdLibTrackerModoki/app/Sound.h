#pragma once

#include <cstdint>
#include <mutex>
#include <string>

#include <windows.h>

// サウンド全体 (OPL3 デバイス・プレイヤー・音声出力) の管理
//
// 実機ドライバに差し替える場合は IOplDevice を実機実装にし、SampleClockDriver の代わりに
// タイマーで A2Player::timerTick() を呼ぶ駆動部を使う (音声出力は不要になる)。

enum class EmulatorType
{
    At2,     // 本家と同じ出音の AT2 互換エミュレータ (既定)
    Nuked,   // Nuked OPL3 1.8
};

struct PlayerStatus
{
    std::wstring title;      // ファイル名
    std::string songname;    // CP437
    std::string composer;    // CP437
    bool playing = false;
    uint8_t order = 0;
    uint8_t pattern = 0;
    uint8_t line = 0;
    uint8_t speed = 0;
    uint8_t tempo = 0;
    uint8_t tracks = 0;
    double time = 0;
    std::wstring device;     // 音源名
    bool output = false;     // 音声出力が使えるか
};

bool sound_init(HWND hwnd);
void sound_done();

// 曲を読み込む (本家と同じく再生は停止状態になる)。
// A2P (パターン) の場合は曲はそのままで、パターン pattern へ読み込む。
bool sound_open_song(const std::wstring& path, uint8_t pattern, std::wstring& error);
void sound_play();
void sound_stop();

// エミュレータを切り替える (読み込み済みの曲は引き継ぎ、再生は停止する)
bool sound_set_emulator(EmulatorType type);
EmulatorType sound_emulator();

PlayerStatus sound_status();

class A2Player;

// プレイヤーと、オーディオスレッドとの排他用ロック (プレイヤーを触る間は必ずロックを取ること)
// エミュレータを切り替えるとプレイヤーは作り直されるため、ポインタを保持し続けないこと。
A2Player* sound_player();
std::mutex& sound_mutex();
