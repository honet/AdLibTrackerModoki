#pragma once

#include <cstdint>
#include <mutex>

#include "IPlayerDriver.h"
#include "../audio/IAudioSource.h"

class A2Player;

// ソフトウェア OPL3 (エミュレータ) 用の再生駆動 (adt2opl3.pas の play_callback_proc 相当)
//
// 出力サンプル数を数え、タイマー周期ごとに A2Player::timerTick() を呼んでから
// エミュレータで PCM を生成する。レジスタ書き込みがサンプル単位で正確な位置に入る。
//
// 実機 OPL3 を使う場合はこのクラスの代わりに TimerDriver を使う。
class SampleClockDriver : public IAudioSource, public IPlayerDriver
{
public:
    // timerSlowdown: タイマーの減速率 [%] (adtrack2.ini の sdl_timer_slowdown、既定 2)
    SampleClockDriver(A2Player& player, IAudioSource& opl, uint32_t sampleRate, uint32_t timerSlowdown = 2);

    void render(int16_t* buffer, uint32_t frames) override;

    // プレイヤーを操作するときはこのロックを取ること (オーディオスレッドと排他する)
    std::mutex& mutex() override { return mutex_; }

private:
    uint32_t sampleFrameSize() const;

    A2Player& player_;
    IAudioSource& opl_;
    uint32_t sampleRate_;
    uint32_t timerSlowdown_;
    uint32_t counter_ = 0;
    std::mutex mutex_;
};
