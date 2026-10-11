#pragma once

#include <cstdint>
#include <mutex>
#include <thread>

#include <windows.h>

#include "../../core/player/IPlayerDriver.h"

class A2Player;
class IOplDevice;

// 実機 OPL3 用の再生駆動 (DOS 版の PIT 割り込みによる timer_poll_proc 相当)
//
// 専用スレッドが高精度タイマーで待機し、A2Player::timerHz() の周期で
// A2Player::timerTick() を呼んでから IOplDevice::flush() で書き込みを送り出す。
// 発火時刻は QueryPerformanceCounter 上の絶対時刻で管理し、周期の誤差を累積させない。
//
// SDL 版の sdl_timer_slowdown はサンプル生成の遅れを補う SDL 固有の補正のため適用しない。
class TimerDriver : public IPlayerDriver
{
public:
    TimerDriver(A2Player& player, IOplDevice& opl);
    ~TimerDriver() override;

    bool start();
    void stop();

    std::mutex& mutex() override { return mutex_; }

private:
    void threadMain();
    void waitUntil(int64_t qpc);

    A2Player& player_;
    IOplDevice& opl_;
    std::mutex mutex_;
    std::thread thread_;

    HANDLE timer_ = nullptr;        // 待機用のウェイタブルタイマー
    HANDLE stopEvent_ = nullptr;    // スレッド終了要求
    bool highResolution_ = false;   // 高精度ウェイタブルタイマーが使えるか
    int64_t qpcFrequency_ = 0;
};
