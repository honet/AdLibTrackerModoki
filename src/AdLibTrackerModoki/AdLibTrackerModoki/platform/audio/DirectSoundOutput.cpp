#include "DirectSoundOutput.h"

#include <algorithm>
#include <cstring>

#include "../../core/audio/IAudioSource.h"

#pragma comment(lib, "dsound.lib")

namespace
{
    // バッファ監視の間隔
    constexpr LONGLONG PollIntervalUs = 2000;
}

DirectSoundOutput::DirectSoundOutput(HWND hwnd, uint32_t sampleRate, uint32_t latencyMs)
    : hwnd_(hwnd)
    , sampleRate_(sampleRate)
{
    latencyBytes_ = (sampleRate_ * latencyMs / 1000) * BytesPerFrame;
    bufferBytes_ = latencyBytes_ * 4;
}

DirectSoundOutput::~DirectSoundOutput()
{
    stop();
}

bool DirectSoundOutput::createBuffer()
{
    if (FAILED(DirectSoundCreate8(nullptr, &ds_, nullptr))) {
        return false;
    }
    if (FAILED(ds_->SetCooperativeLevel(hwnd_, DSSCL_PRIORITY))) {
        return false;
    }

    WAVEFORMATEX wfx = {};
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = Channels;
    wfx.nSamplesPerSec = sampleRate_;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = BytesPerFrame;
    wfx.nAvgBytesPerSec = sampleRate_ * BytesPerFrame;

    DSBUFFERDESC desc = {};
    desc.dwSize = sizeof(desc);
    desc.dwFlags = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_GLOBALFOCUS;
    desc.dwBufferBytes = bufferBytes_;
    desc.lpwfxFormat = &wfx;

    if (FAILED(ds_->CreateSoundBuffer(&desc, &buffer_, nullptr))) {
        return false;
    }

    // 無音で埋めておく
    void* p1 = nullptr;
    DWORD n1 = 0;
    if (FAILED(buffer_->Lock(0, 0, &p1, &n1, nullptr, nullptr, DSBLOCK_ENTIREBUFFER))) {
        return false;
    }
    std::memset(p1, 0, n1);
    buffer_->Unlock(p1, n1, nullptr, 0);
    return true;
}

bool DirectSoundOutput::start(IAudioSource* source)
{
    stop();

    if (!buffer_ && !createBuffer()) {
        buffer_.Reset();
        ds_.Reset();
        return false;
    }

    source_ = source;
    scratch_.resize(bufferBytes_ / sizeof(int16_t));
    writePos_ = 0;
    buffer_->SetCurrentPosition(0);
    if (FAILED(buffer_->Play(0, 0, DSBPLAY_LOOPING))) {
        return false;
    }

    running_ = true;
    thread_ = std::thread(&DirectSoundOutput::threadMain, this);
    return true;
}

void DirectSoundOutput::stop()
{
    running_ = false;
    if (thread_.joinable()) {
        thread_.join();
    }
    if (buffer_) {
        buffer_->Stop();
    }
    source_ = nullptr;
}

void DirectSoundOutput::threadMain()
{
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

    HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr,
        CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);

    // 再生カーソルの更新間隔の分だけ余裕を持たせる
    const uint32_t marginBytes = (sampleRate_ / 100) * BytesPerFrame;

    while (running_) {
        DWORD play = 0, write = 0;
        if (SUCCEEDED(buffer_->GetCurrentPosition(&play, &write))) {
            const uint32_t ahead = (writePos_ + bufferBytes_ - play) % bufferBytes_;
            const uint32_t safe = (write + bufferBytes_ - play) % bufferBytes_;

            // 書き込み位置が再生カーソルに追い越された (アンダーラン) 場合は書き込みカーソルからやり直す
            uint32_t current = ahead;
            if (ahead < safe || ahead >= bufferBytes_ / 2) {
                writePos_ = write;
                current = safe;
            }

            const uint32_t target = (std::max)(latencyBytes_, safe + marginBytes);
            if (current < target) {
                const uint32_t bytes = (target - current) / BytesPerFrame * BytesPerFrame;
                if (bytes > 0) {
                    fill(bytes);
                }
            }
        }

        if (timer) {
            LARGE_INTEGER due;
            due.QuadPart = -PollIntervalUs * 10;
            SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE);
            WaitForSingleObject(timer, INFINITE);
        } else {
            Sleep(1);
        }
    }

    if (timer) {
        CloseHandle(timer);
    }
}

void DirectSoundOutput::fill(uint32_t bytes)
{
    source_->render(scratch_.data(), bytes / BytesPerFrame);

    void* p1 = nullptr;
    void* p2 = nullptr;
    DWORD n1 = 0, n2 = 0;
    HRESULT hr = buffer_->Lock(writePos_, bytes, &p1, &n1, &p2, &n2, 0);
    if (hr == DSERR_BUFFERLOST) {
        buffer_->Restore();
        hr = buffer_->Lock(writePos_, bytes, &p1, &n1, &p2, &n2, 0);
    }
    if (FAILED(hr)) {
        return;
    }

    const auto* src = reinterpret_cast<const uint8_t*>(scratch_.data());
    std::memcpy(p1, src, n1);
    if (p2) {
        std::memcpy(p2, src + n1, n2);
    }
    buffer_->Unlock(p1, n1, p2, n2);

    writePos_ = (writePos_ + bytes) % bufferBytes_;
}
