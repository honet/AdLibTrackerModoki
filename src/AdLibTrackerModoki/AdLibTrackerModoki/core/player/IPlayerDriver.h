#pragma once

#include <mutex>

// A2Player::timerTick() を周期的に呼ぶ再生駆動部の共通インターフェース
//
// 実装:
//   - SampleClockDriver : エミュレータ用。出力サンプル数を数えて timerTick() を呼ぶ
//   - TimerDriver       : 実機用。高精度タイマーで timerTick() を呼ぶ
class IPlayerDriver
{
public:
    virtual ~IPlayerDriver() = default;

    // プレイヤーを操作するときはこのロックを取ること (駆動スレッドと排他する)
    virtual std::mutex& mutex() = 0;
};
