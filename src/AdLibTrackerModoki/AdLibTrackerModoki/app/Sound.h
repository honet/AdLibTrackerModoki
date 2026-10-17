#pragma once

#include <cstdint>
#include <mutex>
#include <string>

#include <windows.h>

// サウンド全体 (OPL3 デバイス・プレイヤー・音声出力) の管理
//
// エミュレータでは SampleClockDriver が出力サンプル数で A2Player::timerTick() を呼び、PCM を DirectSound で鳴らす。
// 実機では TimerDriver が高精度タイマーで timerTick() を呼び、Opl3HardwareC86ctl へ書き込む (音声出力は使わない)。

enum class EmulatorType
{
    At2,       // 本家と同じ出音の AT2 互換エミュレータ (既定)
    Nuked,     // Nuked OPL3 1.8
    Ymfm,      // ymfm YMF262
    C86ctl,  // OPL3 実機
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

// type の音源で初期化する。実機が使えなかったときもプレイヤーは作られる (false を返す)
bool sound_init(HWND hwnd, EmulatorType type = EmulatorType::At2);
void sound_done();

// 曲を読み込む (本家と同じく再生は停止状態になる)。
// A2P (パターン) の場合は曲はそのままで、パターン pattern (to_marked のときはマークしたパターンすべて) へ読み込む。
bool sound_open_song(const std::wstring& path, uint8_t pattern, bool to_marked, std::wstring& error);
void sound_play();
void sound_stop();

// エミュレータを切り替える (読み込み済みの曲は引き継ぎ、再生は停止する)
// 実機に切り替えられなかったときは false を返して元のエミュレータに戻す (理由は error に入る)
bool sound_set_emulator(EmulatorType type, std::wstring& error);
EmulatorType sound_emulator();

// AT2 互換以外のエミュレータで、同時のキーオフ/キーオンのキーオンを 1 サンプル遅らせて再発音させるか (既定 true)
// 再生中でもすぐに反映する。AT2 互換エミュレータと実機には効かない。
void sound_set_keyon_delay(bool enabled);
bool sound_keyon_delay();

PlayerStatus sound_status();

class A2Player;

// プレイヤーと、オーディオスレッドとの排他用ロック (プレイヤーを触る間は必ずロックを取ること)
// エミュレータを切り替えるとプレイヤーは作り直されるため、ポインタを保持し続けないこと。
A2Player* sound_player();
std::mutex& sound_mutex();
