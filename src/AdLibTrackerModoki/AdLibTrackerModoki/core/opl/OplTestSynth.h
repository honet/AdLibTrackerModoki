#pragma once

#include <array>

class IOplDevice;

// 動作確認用の簡易シンセ
//
// 2オペレータの固定音色を全18チャンネルに設定し、noteOn/noteOff で鳴らす。
// プレイヤー実装までの暫定機能。
class OplTestSynth
{
public:
    static constexpr int NumChannels = 18;

    explicit OplTestSynth(IOplDevice& device);

    // チップを OPL3 モードで初期化し、全チャンネルにテスト音色を設定する
    void init();

    // note: MIDI ノート番号 (60 = C4)
    void noteOn(int note);
    void noteOff(int note);
    void allNotesOff();

private:
    void writeChannelFreq(int ch, int note, bool keyOn);

    IOplDevice& device_;
    std::array<int, NumChannels> channelNote_;   // 各チャンネルで鳴っているノート (-1 = 空き)
    int nextChannel_ = 0;
};
