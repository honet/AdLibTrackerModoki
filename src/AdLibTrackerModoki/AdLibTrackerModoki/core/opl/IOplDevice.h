#pragma once

#include <cstdint>

// OPL3 音源デバイスの抽象インターフェース
//
// プレイヤーやエディタはこのインターフェース経由でのみレジスタを書き込む。
// 実装例:
//   - Opl3Emulator   : Nuked OPL3 によるソフトウェアエミュレーション (PCM は IAudioSource で取り出す)
//   - Opl3Hardware   : 実チップへ書き込む (現在はスタブ)
//
// レジスタアドレスは 0x000-0x0FF がバンク0、0x100-0x1FF がバンク1 (OPL3 拡張部)。
class IOplDevice
{
public:
    virtual ~IOplDevice() = default;

    // チップを初期状態に戻す
    virtual void reset() = 0;

    // レジスタ書き込み
    virtual void writeReg(uint16_t reg, uint8_t value) = 0;

    // 溜めている書き込みをデバイスへ送り出す (タイマー周期ごとに呼ばれる)。
    // 書き込みを即時反映するデバイスは何もしなくてよい。
    virtual void flush() {}

    // 表示用のデバイス名
    virtual const wchar_t* name() const = 0;
};
