#pragma once

#include <array>
#include <cstdint>
#include <mutex>

#include "IOplDevice.h"

// キーオンを遅らせるデコレータ (実機用)
//
// A2Player は同じティックの中でキーオフ (B0 = 0) とキーオン (B0 = fnum | 0x20) を続けて書く。
// 実チップもエンベロープはチップのサンプル周期 (約 20us) ごとにしかキーの状態を見ないため、
// キーオフとキーオンがほぼ連続でチップに届くとキーオフが見えず、再発音しないことがある。
//
// エミュレータ用の KeyOnDelay の「サンプル生成」を「flush (デバイスへの送出)」に置き換えたもの。
// 直前の flush の時点でキーオンだったチャンネルに、flush を挟まずキーオフ → キーオンと書かれたときは
// キーオンの書き込みを保留する。flush() では保留分以外を送り出し、KeyOnGapMicroseconds 待ってから
// 保留していたキーオンを書いてもう一度送り出す。
//
// writeReg() は任意のスレッドから、flush() は TimerDriver のスレッドから呼ばれる。
class HardwareKeyOnDelay : public IOplDevice
{
public:
    // キーオフを送ってから保留していたキーオンを送るまでの待ち時間 (チップの約 5 サンプル分)
    static constexpr int64_t KeyOnGapMicroseconds = 100;

    explicit HardwareKeyOnDelay(IOplDevice& device);

    // false にすると何もせず素通しする (保留中の書き込みはすぐに反映する)
    void setEnabled(bool enabled);
    bool enabled() const { return enabled_; }

    void reset() override;
    void writeReg(uint16_t reg, uint8_t value) override;
    void flush() override;
    const wchar_t* name() const override { return device_.name(); }

private:
    static constexpr int Channels = 18;    // B0-B8 / 1B0-1B8

    struct KeyReg
    {
        uint8_t applied = 0;           // デバイスへ書いた最後の値
        bool onAtFlush = false;        // 直前の flush の時点でキーオンだったか
        bool offSinceFlush = false;    // 直前の flush のあとでキーオフを書いたか
        bool pending = false;          // キーオンの書き込みを保留しているか
        uint8_t pendingValue = 0;
    };

    void applyPending();
    void markFlushed();
    void waitGap() const;

    IOplDevice& device_;
    bool enabled_ = true;
    bool anyPending_ = false;
    std::array<KeyReg, Channels> keys_{};
    int64_t qpcFrequency_ = 0;
    std::mutex mutex_;
};
