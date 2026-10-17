#pragma once

#include <array>
#include <cstdint>
#include <mutex>

#include "IOplDevice.h"
#include "../audio/IAudioSource.h"

// キーオンを遅らせるデコレータ (AT2 互換以外のエミュレータ用)
//
// A2Player は同じティックの中でキーオフ (B0 = 0) とキーオン (B0 = fnum | 0x20) を続けて書く。
// AT2 互換エミュレータはキーオンを書いた瞬間にエンベロープをアタックから始め直すので再発音するが、
// Nuked OPL3 / ymfm はキーの状態をサンプル生成時にしか見ないため、キーオンのままとみなしてエンベロープが継続する。
//
// 直前のサンプル生成の時点でキーオンだったチャンネルに、生成を挟まずキーオフ → キーオンと書かれたときは
// キーオンの書き込みを保留し、1 サンプル生成してから (キーオフをエンベロープに見せてから) 書き込む。
//
// writeReg() は任意のスレッドから、render() はオーディオ出力スレッドから呼ばれる。
class KeyOnDelay : public IOplDevice, public IAudioSource
{
public:
    KeyOnDelay(IOplDevice& device, IAudioSource& source);

    // false にすると何もせず素通しする (保留中の書き込みはすぐに反映する)
    void setEnabled(bool enabled);
    bool enabled() const { return enabled_; }

    void reset() override;
    void writeReg(uint16_t reg, uint8_t value) override;
    void flush() override { device_.flush(); }
    const wchar_t* name() const override { return device_.name(); }

    void render(int16_t* buffer, uint32_t frames) override;

private:
    static constexpr int Channels = 18;    // B0-B8 / 1B0-1B8

    struct KeyReg
    {
        uint8_t applied = 0;       // デバイスへ書いた最後の値
        bool onAtSample = false;   // 直前のサンプル生成の時点でキーオンだったか
        bool offSinceSample = false;   // 直前のサンプル生成のあとでキーオフを書いたか
        bool pending = false;      // キーオンの書き込みを保留しているか
        uint8_t pendingValue = 0;
    };

    void applyPending();
    void markSampled();

    IOplDevice& device_;
    IAudioSource& source_;
    bool enabled_ = true;
    bool anyPending_ = false;
    std::array<KeyReg, Channels> keys_{};
    std::mutex mutex_;
};
