#include "TimerDriver.h"

#include <algorithm>
#include <cmath>

#include <mmsystem.h>

#include "../../core/opl/IOplDevice.h"
#include "../../core/player/A2Player.h"

#pragma comment(lib, "winmm.lib")

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

namespace
{
    int64_t qpc_now()
    {
        LARGE_INTEGER t;
        QueryPerformanceCounter(&t);
        return t.QuadPart;
    }
}

TimerDriver::TimerDriver(A2Player& player, IOplDevice& opl)
    : player_(player)
    , opl_(opl)
{
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    qpcFrequency_ = f.QuadPart;
}

TimerDriver::~TimerDriver()
{
    stop();
}

bool TimerDriver::start()
{
    if (thread_.joinable()) return true;

    // Windows 10 1803 以降は高精度ウェイタブルタイマーが使える。
    // 使えない場合は通常のタイマーに timeBeginPeriod(1) を組み合わせる。
    timer_ = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    highResolution_ = timer_ != nullptr;
    if (!timer_) {
        timer_ = CreateWaitableTimerExW(nullptr, nullptr, 0, TIMER_ALL_ACCESS);
        if (!timer_) return false;
        timeBeginPeriod(1);
    }

    stopEvent_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!stopEvent_) {
        stop();
        return false;
    }

    thread_ = std::thread(&TimerDriver::threadMain, this);
    return true;
}

void TimerDriver::stop()
{
    if (thread_.joinable()) {
        SetEvent(stopEvent_);
        thread_.join();
    }
    if (stopEvent_) {
        CloseHandle(stopEvent_);
        stopEvent_ = nullptr;
    }
    if (timer_) {
        CloseHandle(timer_);
        timer_ = nullptr;
        if (!highResolution_) timeEndPeriod(1);
    }
}

void TimerDriver::waitUntil(int64_t qpc)
{
    const int64_t remain = qpc - qpc_now();
    if (remain <= 0) return;

    // 相対時間 (100ns 単位、負値)
    LARGE_INTEGER due;
    due.QuadPart = -std::max<int64_t>(remain * 10000000 / qpcFrequency_, 1);
    if (SetWaitableTimerEx(timer_, &due, 0, nullptr, nullptr, nullptr, 0)) {
        const HANDLE handles[] = { stopEvent_, timer_ };
        WaitForMultipleObjects(2, handles, FALSE, INFINITE);
    }
}

void TimerDriver::threadMain()
{
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

    // この時間以上遅れたら、取り戻そうとせず基準時刻を現在に合わせ直す
    const int64_t maxLag = qpcFrequency_ / 10;

    int64_t next = qpc_now();
    while (WaitForSingleObject(stopEvent_, 0) != WAIT_OBJECT_0) {
        int32_t hz;
        {
            std::lock_guard lock(mutex_);
            player_.timerTick();
            opl_.flush();
            hz = std::max<int32_t>(player_.timerHz(), 18);
        }

        next += static_cast<int64_t>(std::nearbyint(static_cast<double>(qpcFrequency_) / hz));
        const int64_t now = qpc_now();
        if (now - next > maxLag) next = now;

        waitUntil(next);
    }
}
