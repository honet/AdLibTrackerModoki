#include "Opl3Hardware.h"

#include <cstdio>

#include <windows.h>

Opl3Hardware::~Opl3Hardware()
{
    close();
}

bool Opl3Hardware::open()
{
    std::lock_guard lock(mutex_);
    // TODO: デバイスを検索して接続する
    open_ = true;
    writeCount_ = 0;
    flushCount_ = 0;
    return true;
}

void Opl3Hardware::close()
{
    std::lock_guard lock(mutex_);
    if (!open_) return;

    // TODO: 発音を止めてから切断する
    wchar_t msg[128];
    swprintf_s(msg, L"Opl3Hardware: writes=%llu flushes=%llu\n",
               static_cast<unsigned long long>(writeCount_), static_cast<unsigned long long>(flushCount_));
    OutputDebugStringW(msg);
    open_ = false;
}

void Opl3Hardware::reset()
{
    std::lock_guard lock(mutex_);
    // TODO: チップのリセット (IC 端子) またはレジスタの全クリアを送る
    shadow_.fill(0);
}

void Opl3Hardware::writeReg(uint16_t reg, uint8_t value)
{
    std::lock_guard lock(mutex_);
    if (!open_) return;
    // TODO: 送信バッファへ (reg, value) を積む
    shadow_[reg & 0x1ff] = value;
    ++writeCount_;
}

void Opl3Hardware::flush()
{
    std::lock_guard lock(mutex_);
    if (!open_) return;
    // TODO: 送信バッファの内容をデバイスへ送り出す
    ++flushCount_;
}
