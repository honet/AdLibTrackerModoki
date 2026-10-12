#pragma once

#include <array>
#include <cstdint>
#include <mutex>

#include "../../core/opl/IOplDevice.h"

// 実機 OPL3 へのインターフェース
//
// writeReg() で書き込みを溜め、TimerDriver がタイマー周期ごとに呼ぶ flush() でまとめて送り出す。
// UI スレッドからの書き込み (エディタの試聴等) も次の flush() で送られる。
//
// 現在はスタブ実装で、実デバイスへは何も送らない。書き込みはシャドウレジスタに反映し、件数を数える。
class Opl3Hardware : public IOplDevice
{
public:
    Opl3Hardware() = default;
    ~Opl3Hardware() override;

    // デバイスに接続する (失敗したら false)
    bool open();
    void close();
    bool isOpen() const { return open_; }

    void reset() override;
    void writeReg(uint16_t reg, uint8_t value) override;
    void flush() override;
    const wchar_t* name() const override { return L"OPL3 実機 (スタブ)"; }

    // デバッグ用: シャドウレジスタと書き込み・送出の件数
    uint8_t shadowReg(uint16_t reg) const { return shadow_[reg & 0x1ff]; }
    uint64_t writeCount() const { return writeCount_; }
    uint64_t flushCount() const { return flushCount_; }

private:
    std::mutex mutex_;
    bool open_ = false;
    std::array<uint8_t, 0x200> shadow_{};
    uint64_t writeCount_ = 0;
    uint64_t flushCount_ = 0;
};
