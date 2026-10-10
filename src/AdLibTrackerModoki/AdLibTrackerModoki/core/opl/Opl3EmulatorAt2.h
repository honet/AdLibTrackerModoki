#pragma once

#include <memory>
#include <mutex>

#include "IOplDevice.h"
#include "../audio/IAudioSource.h"

// AdLib Tracker II SDL 版の OPL3 エミュレータ (opl3emu.pas の移植)
//
// NukedOPL3 1.6 を元にした AT2 独自の実装で、本家と同じ出音にするために使う。
// Nuked OPL3 1.8 (Opl3Emulator) との主な違い:
//   - キーオンを書いた瞬間にエンベロープがアタックから再開する (キーオフ直後の再キーオンで再発音する)
//   - 0x105 (NEW) に関係なく常に OPL3 として動作する
//   - リサンプルせず、1 回の生成で OPL3 の 1 サンプル (49716Hz) を出力する
//
// writeReg() は任意のスレッドから、render() はオーディオ出力スレッドから呼ばれる。
class Opl3EmulatorAt2 : public IOplDevice, public IAudioSource
{
public:
    static constexpr uint32_t NativeSampleRate = 49716;

    Opl3EmulatorAt2();
    ~Opl3EmulatorAt2() override;

    void reset() override;
    void writeReg(uint16_t reg, uint8_t value) override;
    const wchar_t* name() const override { return L"AT2 OPL3 エミュレータ"; }

    void render(int16_t* buffer, uint32_t frames) override;

private:
    struct Chip;
    std::unique_ptr<Chip> chip_;
    std::mutex mutex_;
};
