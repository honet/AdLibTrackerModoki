#include "SampleClockDriver.h"

#include <algorithm>
#include <cmath>

#include "A2Player.h"
#include "../audio/IAudioSource.h"

SampleClockDriver::SampleClockDriver(A2Player& player, IAudioSource& opl, uint32_t sampleRate, uint32_t timerSlowdown)
    : player_(player)
    , opl_(opl)
    , sampleRate_(sampleRate)
    , timerSlowdown_(timerSlowdown)
{
}

// snd_SetTimer
uint32_t SampleClockDriver::sampleFrameSize() const
{
    const int32_t hz = std::max<int32_t>(player_.timerHz(), 18);
    const double size = std::nearbyint(static_cast<double>(sampleRate_) / hz * (1 + timerSlowdown_ / 100.0));
    return std::max<uint32_t>(static_cast<uint32_t>(size), 1);
}

void SampleClockDriver::render(int16_t* buffer, uint32_t frames)
{
    std::lock_guard lock(mutex_);

    while (frames > 0) {
        // オリジナルはサンプルごとにカウンタを進め、sample_frame_size に達したサンプルの生成前にタイマー処理を行う
        const uint32_t frameSize = sampleFrameSize();
        if (counter_ + 1 >= frameSize) {
            counter_ = 0;
            player_.timerTick();
            opl_.render(buffer, 1);
            buffer += 2;
            --frames;
            continue;
        }

        const uint32_t n = std::min(frames, frameSize - 1 - counter_);
        opl_.render(buffer, n);
        counter_ += n;
        buffer += 2 * n;
        frames -= n;
    }
}
