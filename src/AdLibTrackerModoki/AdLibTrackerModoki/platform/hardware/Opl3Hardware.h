#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>

#include "../../core/opl/IOplDevice.h"

// 実機 OPL3 へのインターフェース
//
// writeReg() で書き込みを溜め、TimerDriver がタイマー周期ごとに呼ぶ flush() でまとめて送り出す。
// UI スレッドからの書き込み (エディタの試聴等) も次の flush() で送られる。
//
// c86ctl.dll は open() で動的に読み込み、close() でデバイスを解放してから FreeLibrary する。
// DLL が無い環境でもアプリは起動でき、実機を選んだときだけ読み込む。
class Opl3Hardware : public IOplDevice
{
public:
    Opl3Hardware();
    ~Opl3Hardware() override;

    // デバイスに接続する (失敗したら false。理由は lastError() で得られる)
    bool open();
    void close();
    bool isOpen() const { return open_; }
    const std::wstring& lastError() const { return lastError_; }

    void reset() override;
    void writeReg(uint16_t reg, uint8_t value) override;
    void flush() override;
    const wchar_t* name() const override { return L"OPL3 実機 (スタブ)"; }

    // デバッグ用: シャドウレジスタと書き込み・送出の件数
    uint8_t shadowReg(uint16_t reg) const { return shadow_[reg & 0x1ff]; }
    uint64_t writeCount() const { return writeCount_; }
    uint64_t flushCount() const { return flushCount_; }

private:
    struct Api;    // c86ctl.dll のモジュールハンドルと関数ポインタ

    void unloadLibrary();

    std::mutex mutex_;
    std::unique_ptr<Api> api_;
    bool open_ = false;
    std::wstring lastError_;
    std::array<uint8_t, 0x200> shadow_{};
    uint64_t writeCount_ = 0;
    uint64_t flushCount_ = 0;
};
