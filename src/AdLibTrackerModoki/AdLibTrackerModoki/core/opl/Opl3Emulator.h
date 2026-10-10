#pragma once

#include <memory>
#include <mutex>

#include "IOplDevice.h"
#include "../audio/IAudioSource.h"

struct _opl3_chip;

// Nuked OPL3 によるソフトウェア OPL3
//
// writeReg() は任意のスレッドから、render() はオーディオ出力スレッドから呼ばれる。
class Opl3Emulator : public IOplDevice, public IAudioSource
{
public:
    // OPL3 のネイティブレート (14.31818MHz / 288)
    static constexpr uint32_t NativeSampleRate = 49716;

    explicit Opl3Emulator(uint32_t sampleRate = NativeSampleRate);
    ~Opl3Emulator() override;

    void reset() override;
    void writeReg(uint16_t reg, uint8_t value) override;
    const wchar_t* name() const override { return L"Nuked OPL3"; }

    void render(int16_t* buffer, uint32_t frames) override;

private:
    std::unique_ptr<_opl3_chip> chip_;
    uint32_t sampleRate_;
    std::mutex mutex_;
};
