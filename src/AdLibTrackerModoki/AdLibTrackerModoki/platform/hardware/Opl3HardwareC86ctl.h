#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>

#include "../../core/opl/IOplDevice.h"

// 実機 OPL3 へのインターフェース
//
// writeReg() は c86ctl_outex() で c86ctl の送信バッファへ溜め、TimerDriver が tick ごとに
// (A2Player::timerTick() の直後に) 呼ぶ flush() で c86ctl_flushex() を呼び、1 tick 分の書き込みをまとめて送り出す。
// エミュレータが tick 境界のサンプル位置で書き込みを反映するのと同じく、実機でも tick 境界で反映させるため。
// UI スレッドからの書き込み (エディタの試聴等) は次の flush() か c86ctl の自動送信 (約 1ms) で送られる。
// c86ctl_flushex() が無い古い DLL では c86ctl の自動送信 (64byte 単位 / 約 1ms) に任せる。
//
// c86ctl.dll は open() で動的に読み込み、close() でデバイスを解放してから FreeLibrary する。
// DLL が無い環境でもアプリは起動でき、実機を選んだときだけ読み込む。
class Opl3HardwareC86ctl : public IOplDevice
{
public:
    Opl3HardwareC86ctl();
    ~Opl3HardwareC86ctl() override;

    // デバイスに接続する (失敗したら false。理由は lastError() で得られる)
    bool open();
    void close();
    bool isOpen() const { return open_; }
    const std::wstring& lastError() const { return lastError_; }

    void reset() override;
    void writeReg(uint16_t reg, uint8_t value) override;
    void flush() override;
    const wchar_t* name() const override { return L"OPL3 Hardware (c86ctl)"; }

    // デバッグ用: シャドウレジスタと書き込み・送出の件数
    uint8_t shadowReg(uint16_t reg) const { return shadow_[reg & 0x1ff]; }
    uint64_t writeCount() const { return writeCount_; }
    uint64_t flushCount() const { return flushCount_; }

private:
    struct Api;    // c86ctl.dll のモジュールハンドルと関数ポインタ

    void unloadLibrary();
    int findChip(int type);

    std::mutex mutex_;
    std::unique_ptr<Api> api_;
    bool open_ = false;
    std::wstring lastError_;
    std::array<uint8_t, 0x200> shadow_{};
    uint64_t writeCount_ = 0;
    uint64_t flushCount_ = 0;
    int targetChipIdx = 0;
};
