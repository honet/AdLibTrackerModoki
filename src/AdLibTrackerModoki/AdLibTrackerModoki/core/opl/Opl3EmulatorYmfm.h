#pragma once

#include <memory>
#include <mutex>

#include "IOplDevice.h"
#include "../audio/IAudioSource.h"

// ymfm (Aaron Giles) の YMF262 によるソフトウェア OPL3
//
// Nuked OPL3 との主な違い:
//   - 実チップと同じく、0x105 (NEW) が 0 の間はバンク1 (0x100-) への書き込みがバンク0に入る (0x105 自身は除く)
//   - ymfm そのままでは 0x104 による 4op の組み替えが次のサンプルまで遅れるため、書いた時点で組み替えている
//   - ymfm そのままでは 2 倍の音量になるため、オペレータの出力を 1/2 にして他のエミュレータと揃えている
//   - リサンプルせず、1 回の生成で OPL3 の 1 サンプル (49716Hz) を出力する
//
// writeReg() は任意のスレッドから、render() はオーディオ出力スレッドから呼ばれる。
class Opl3EmulatorYmfm : public IOplDevice, public IAudioSource
{
public:
    // OPL3 のマスタークロックとネイティブレート (Clock / 288)
    static constexpr uint32_t Clock = 14318180;
    static constexpr uint32_t NativeSampleRate = 49716;

    Opl3EmulatorYmfm();
    ~Opl3EmulatorYmfm() override;

    void reset() override;
    void writeReg(uint16_t reg, uint8_t value) override;
    const wchar_t* name() const override { return L"ymfm YMF262"; }

    void render(int16_t* buffer, uint32_t frames) override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::mutex mutex_;
};
