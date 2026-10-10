#pragma once

#include <cstdint>

// PCM を生成するもの (オーディオ出力スレッドから呼ばれる)
class IAudioSource
{
public:
    virtual ~IAudioSource() = default;

    // 16bit ステレオ (L,R 交互) で frames フレーム分を buffer に書き込む
    virtual void render(int16_t* buffer, uint32_t frames) = 0;
};
