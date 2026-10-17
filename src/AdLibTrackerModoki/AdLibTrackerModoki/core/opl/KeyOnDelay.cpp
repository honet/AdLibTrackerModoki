#include "KeyOnDelay.h"

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
}

KeyOnDelay::KeyOnDelay(IOplDevice& device, IAudioSource& source)
    : device_(device)
    , source_(source)
{
}

void KeyOnDelay::setEnabled(bool enabled)
{
    std::lock_guard lock(mutex_);
    enabled_ = enabled;
    if (!enabled_) applyPending();
}

void KeyOnDelay::reset()
{
    std::lock_guard lock(mutex_);
    keys_ = {};
    anyPending_ = false;
    device_.reset();
}

void KeyOnDelay::writeReg(uint16_t reg, uint8_t value)
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
    if (enabled_ && (value & KeyOnBit) && k.offSinceSample) {
        k.pending = true;
        k.pendingValue = value;
        anyPending_ = true;
        return;
    }

    device_.writeReg(reg, value);
    k.applied = value;
    if (!(value & KeyOnBit) && k.onAtSample) k.offSinceSample = true;
}

void KeyOnDelay::render(int16_t* buffer, uint32_t frames)
{
    std::lock_guard lock(mutex_);
    if (frames == 0) return;

    if (anyPending_) {
        // キーオフを 1 サンプル鳴らしてから、保留していたキーオンを書く
        source_.render(buffer, 1);
        markSampled();
        applyPending();
        buffer += 2;
        --frames;
        if (frames == 0) return;
    }

    source_.render(buffer, frames);
    markSampled();
}

void KeyOnDelay::applyPending()
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

void KeyOnDelay::markSampled()
{
    for (KeyReg& k : keys_) {
        k.onAtSample = (k.applied & KeyOnBit) != 0;
        k.offSinceSample = false;
    }
}
