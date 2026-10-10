#include "OplTestSynth.h"

#include <cmath>

#include "IOplDevice.h"

namespace
{
    // チャンネル (バンク内 0-8) → モジュレータのオペレータオフセット。キャリアは +3
    constexpr uint16_t SlotOffset[9] = { 0x00, 0x01, 0x02, 0x08, 0x09, 0x0A, 0x10, 0x11, 0x12 };

    constexpr double OplClockRate = 49716.0;

    uint16_t bankBase(int ch) { return ch >= 9 ? 0x100 : 0x000; }
}

OplTestSynth::OplTestSynth(IOplDevice& device)
    : device_(device)
{
    channelNote_.fill(-1);
}

void OplTestSynth::init()
{
    device_.reset();
    device_.writeReg(0x105, 0x01);   // OPL3 モード有効
    device_.writeReg(0x104, 0x00);   // 4op 接続なし
    device_.writeReg(0x001, 0x20);   // 波形選択有効 (OPL2 互換ビット)
    device_.writeReg(0x0BD, 0x00);   // リズムモード無効

    for (int ch = 0; ch < NumChannels; ++ch) {
        const uint16_t base = bankBase(ch);
        const uint16_t mod = base + SlotOffset[ch % 9];
        const uint16_t car = mod + 3;

        // AM/VIB/EGT/KSR/MULT
        device_.writeReg(0x20 + mod, 0x21);   // EGT=1, MULT=1
        device_.writeReg(0x20 + car, 0x21);
        // KSL/TL
        device_.writeReg(0x40 + mod, 0x1A);
        device_.writeReg(0x40 + car, 0x00);
        // AR/DR
        device_.writeReg(0x60 + mod, 0xF3);
        device_.writeReg(0x60 + car, 0xF2);
        // SL/RR
        device_.writeReg(0x80 + mod, 0x35);
        device_.writeReg(0x80 + car, 0x45);
        // 波形
        device_.writeReg(0xE0 + mod, 0x00);
        device_.writeReg(0xE0 + car, 0x00);
        // 出力 L+R、FB=3、FM 接続
        device_.writeReg(0xC0 + base + ch % 9, 0x30 | (3 << 1));

        writeChannelFreq(ch, 60, false);
    }

    channelNote_.fill(-1);
    nextChannel_ = 0;
}

void OplTestSynth::writeChannelFreq(int ch, int note, bool keyOn)
{
    const double freq = 440.0 * std::pow(2.0, (note - 69) / 12.0);

    // F-Number が 10bit に収まる最小のブロックを選ぶ
    int block = 0;
    int fnum = 0;
    for (block = 0; block < 8; ++block) {
        fnum = static_cast<int>(std::lround(freq * (1 << (20 - block)) / OplClockRate));
        if (fnum < 1024) {
            break;
        }
    }
    if (block == 8) {
        block = 7;
        fnum = 1023;
    }

    const uint16_t reg = bankBase(ch) + ch % 9;
    device_.writeReg(0xA0 + reg, static_cast<uint8_t>(fnum & 0xFF));
    device_.writeReg(0xB0 + reg, static_cast<uint8_t>((keyOn ? 0x20 : 0x00) | (block << 2) | (fnum >> 8)));
}

void OplTestSynth::noteOn(int note)
{
    // 同じノートが既に鳴っていれば無視 (キーリピート対策)
    for (int n : channelNote_) {
        if (n == note) {
            return;
        }
    }

    // 空きチャンネルを探す。無ければラウンドロビンで奪う
    int ch = -1;
    for (int i = 0; i < NumChannels; ++i) {
        const int c = (nextChannel_ + i) % NumChannels;
        if (channelNote_[c] < 0) {
            ch = c;
            break;
        }
    }
    if (ch < 0) {
        ch = nextChannel_;
        writeChannelFreq(ch, channelNote_[ch], false);
    }
    nextChannel_ = (ch + 1) % NumChannels;

    channelNote_[ch] = note;
    writeChannelFreq(ch, note, true);
}

void OplTestSynth::noteOff(int note)
{
    for (int ch = 0; ch < NumChannels; ++ch) {
        if (channelNote_[ch] == note) {
            writeChannelFreq(ch, note, false);
            channelNote_[ch] = -1;
        }
    }
}

void OplTestSynth::allNotesOff()
{
    for (int ch = 0; ch < NumChannels; ++ch) {
        if (channelNote_[ch] >= 0) {
            noteOff(channelNote_[ch]);
        }
    }
}
