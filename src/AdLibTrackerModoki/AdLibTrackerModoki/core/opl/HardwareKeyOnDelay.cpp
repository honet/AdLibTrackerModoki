#include "HardwareKeyOnDelay.h"

#include <windows.h>

namespace
{
    constexpr uint8_t KeyOnBit = 0x20;

    // B0-B8 / 1B0-1B8 なら 0-17、それ以外は -1
    int key_index(uint16_t reg)
    {
        const uint8_t r = static_cast<uint8_t>(reg);
        if (r < 0xb0 || r > 0xb8) return -1;
        return (reg & 0x100 ? 9 : 0) + (r - 0xb0);
    }

    uint16_t key_reg(int index)
    {
        return static_cast<uint16_t>(index < 9 ? 0xb0 + index : 0x1b0 + index - 9);
    }

    int64_t qpc_now()
    {
        LARGE_INTEGER t;
        QueryPerformanceCounter(&t);
        return t.QuadPart;
    }
}

HardwareKeyOnDelay::HardwareKeyOnDelay(IOplDevice& device)
    : device_(device)
{
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    qpcFrequency_ = f.QuadPart;
}

void HardwareKeyOnDelay::setEnabled(bool enabled)
{
    std::lock_guard lock(mutex_);
    enabled_ = enabled;
    if (!enabled_ && anyPending_) {
        // キーオフを送ってから保留分を書く (次の flush で送られる)
        device_.flush();
        waitGap();
        applyPending();
    }
}

void HardwareKeyOnDelay::reset()
{
    std::lock_guard lock(mutex_);
    keys_ = {};
    anyPending_ = false;
    device_.reset();
}

void HardwareKeyOnDelay::writeReg(uint16_t reg, uint8_t value)
{
    std::lock_guard lock(mutex_);

    const int index = key_index(reg);
    if (index < 0) {
        device_.writeReg(reg, value);
        return;
    }

    KeyReg& k = keys_[index];
    // 保留中のレジスタへの書き込みは、保留している値を置き換える (書き込み順を保つ)
    if (k.pending) {
        k.pendingValue = value;
        return;
    }
    if (enabled_ && (value & KeyOnBit) && k.offSinceFlush) {
        k.pending = true;
        k.pendingValue = value;
        anyPending_ = true;
        return;
    }

    device_.writeReg(reg, value);
    k.applied = value;
    if (!(value & KeyOnBit) && k.onAtFlush) k.offSinceFlush = true;
}

void HardwareKeyOnDelay::flush()
{
    std::lock_guard lock(mutex_);

    // キーオフを含む保留分以外を送り出す
    device_.flush();
    markFlushed();

    if (anyPending_) {
        // キーオフがチップのエンベロープに見えるまで待ってから、保留していたキーオンを送る
        waitGap();
        applyPending();
        device_.flush();
        markFlushed();
    }
}

void HardwareKeyOnDelay::applyPending()
{
    if (!anyPending_) return;
    for (int i = 0; i < Channels; i++) {
        KeyReg& k = keys_[i];
        if (!k.pending) continue;
        device_.writeReg(key_reg(i), k.pendingValue);
        k.applied = k.pendingValue;
        k.pending = false;
    }
    anyPending_ = false;
}

void HardwareKeyOnDelay::markFlushed()
{
    for (KeyReg& k : keys_) {
        k.onAtFlush = (k.applied & KeyOnBit) != 0;
        k.offSinceFlush = false;
    }
}

void HardwareKeyOnDelay::waitGap() const
{
    // 100us 程度なのでスリープせず空回しで待つ
    const int64_t end = qpc_now() + qpcFrequency_ * KeyOnGapMicroseconds / 1000000;
    while (qpc_now() < end) YieldProcessor();
}
