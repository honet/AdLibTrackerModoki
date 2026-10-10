#pragma once

#include <cstdint>

class IAudioSource;

// PCM 出力デバイスの抽象インターフェース
class IAudioOutput
{
public:
    virtual ~IAudioOutput() = default;

    // source から PCM を取り出して再生を開始する
    virtual bool start(IAudioSource* source) = 0;

    // 再生を停止する (以後 source は呼ばれない)
    virtual void stop() = 0;

    virtual uint32_t sampleRate() const = 0;
};
