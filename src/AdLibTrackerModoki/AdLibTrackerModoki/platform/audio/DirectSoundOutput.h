#pragma once

#include <atomic>
#include <thread>
#include <vector>

#include <windows.h>
#include <mmsystem.h>
#include <dsound.h>
#include <wrl/client.h>

#include "../../core/audio/IAudioOutput.h"

// DirectSound によるストリーミング再生
//
// ループ再生しているセカンダリバッファに対し、専用スレッドが再生カーソルを追いかけて
// IAudioSource から取り出した PCM を書き足していく。
class DirectSoundOutput : public IAudioOutput
{
public:
    // hwnd は SetCooperativeLevel 用のウィンドウ
    DirectSoundOutput(HWND hwnd, uint32_t sampleRate, uint32_t latencyMs = 60);
    ~DirectSoundOutput() override;

    bool start(IAudioSource* source) override;
    void stop() override;
    uint32_t sampleRate() const override { return sampleRate_; }

private:
    static constexpr uint32_t Channels = 2;
    static constexpr uint32_t BytesPerFrame = Channels * sizeof(int16_t);

    bool createBuffer();
    void threadMain();
    void fill(uint32_t bytes);

    HWND hwnd_;
    uint32_t sampleRate_;
    uint32_t latencyBytes_;    // 再生カーソルからどれだけ先まで書き込んでおくか
    uint32_t bufferBytes_;     // セカンダリバッファ全体のサイズ
    uint32_t writePos_ = 0;    // 次に書き込むバッファ内オフセット

    Microsoft::WRL::ComPtr<IDirectSound8> ds_;
    Microsoft::WRL::ComPtr<IDirectSoundBuffer> buffer_;

    IAudioSource* source_ = nullptr;
    std::vector<int16_t> scratch_;
    std::thread thread_;
    std::atomic<bool> running_ = false;
};
