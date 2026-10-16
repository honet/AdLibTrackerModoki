#include "Sound.h"

#include <memory>
#include <mutex>

#include "../core/opl/Opl3Emulator.h"
#include "../core/opl/Opl3EmulatorAt2.h"
#include "../core/opl/Opl3EmulatorYmfm.h"
#include "../core/player/A2Player.h"
#include "../core/player/SampleClockDriver.h"
#include "../core/song/ModuleLoader.h"
#include "../platform/audio/DirectSoundOutput.h"
#include "../platform/hardware/Opl3Hardware.h"
#include "../platform/player/TimerDriver.h"

namespace
{
    HWND g_hwnd = nullptr;
    EmulatorType g_emulatorType = EmulatorType::At2;
    std::unique_ptr<Opl3EmulatorAt2> g_emulatorAt2;
    std::unique_ptr<Opl3Emulator> g_emulatorNuked;
    std::unique_ptr<Opl3EmulatorYmfm> g_emulatorYmfm;
    std::unique_ptr<Opl3Hardware> g_hardware;
    IOplDevice* g_oplDevice = nullptr;
    IAudioSource* g_oplSource = nullptr;
    std::unique_ptr<A2Player> g_player;
    std::unique_ptr<SampleClockDriver> g_sampleDriver;
    std::unique_ptr<TimerDriver> g_timerDriver;
    IPlayerDriver* g_driver = nullptr;
    std::unique_ptr<IAudioOutput> g_audioOutput;
    std::wstring g_error;    // 直近の start_sound() が失敗した理由

    // エミュレータ: サンプル数で駆動し、PCM を DirectSound で鳴らす
    bool start_emulator()
    {
        const uint32_t sampleRate = Opl3EmulatorAt2::NativeSampleRate;
        switch (g_emulatorType) {
        case EmulatorType::Nuked:
            g_emulatorNuked = std::make_unique<Opl3Emulator>(sampleRate);
            g_oplDevice = g_emulatorNuked.get();
            g_oplSource = g_emulatorNuked.get();
            break;
        case EmulatorType::Ymfm:
            g_emulatorYmfm = std::make_unique<Opl3EmulatorYmfm>();
            g_oplDevice = g_emulatorYmfm.get();
            g_oplSource = g_emulatorYmfm.get();
            break;
        default:
            g_emulatorAt2 = std::make_unique<Opl3EmulatorAt2>();
            g_oplDevice = g_emulatorAt2.get();
            g_oplSource = g_emulatorAt2.get();
            break;
        }

        g_player = std::make_unique<A2Player>(*g_oplDevice);
        g_sampleDriver = std::make_unique<SampleClockDriver>(*g_player, *g_oplSource, sampleRate);
        g_driver = g_sampleDriver.get();

        g_audioOutput = std::make_unique<DirectSoundOutput>(g_hwnd, sampleRate);
        if (!g_audioOutput->start(g_sampleDriver.get())) {
            g_audioOutput.reset();
            return false;
        }
        return true;
    }

    // 実機: タイマーで駆動する (PC 側の音声出力は使わない)
    bool start_hardware()
    {
        g_hardware = std::make_unique<Opl3Hardware>();
        g_oplDevice = g_hardware.get();

        g_player = std::make_unique<A2Player>(*g_oplDevice);
        g_timerDriver = std::make_unique<TimerDriver>(*g_player, *g_oplDevice);
        g_driver = g_timerDriver.get();

        // 接続できなくてもプレイヤーは残す (曲の編集はできるようにする)
        if (!g_hardware->open()) {
            g_error = g_hardware->lastError();
            return false;
        }
        g_hardware->reset();
        return g_timerDriver->start();
    }

    bool start_sound()
    {
        g_error.clear();
        return g_emulatorType == EmulatorType::Hardware ? start_hardware() : start_emulator();
    }
}

bool sound_init(HWND hwnd)
{
    g_hwnd = hwnd;
    return start_sound();
}

void sound_done()
{
    if (g_audioOutput) {
        g_audioOutput->stop();
        g_audioOutput.reset();
    }
    if (g_timerDriver) g_timerDriver->stop();
    if (g_hardware) {
        // 鳴りっぱなしにならないよう、チップを初期化してから切断する
        g_hardware->reset();
        g_hardware->flush();
        g_hardware->close();
    }
    g_driver = nullptr;
    g_sampleDriver.reset();
    g_timerDriver.reset();
    g_player.reset();
    g_oplDevice = nullptr;
    g_oplSource = nullptr;
    g_emulatorAt2.reset();
    g_emulatorNuked.reset();
    g_emulatorYmfm.reset();
    g_hardware.reset();
}

bool sound_open_song(const std::wstring& path, uint8_t pattern, bool to_marked, std::wstring& error)
{
    if (!g_player) {
        error = L"サウンドが初期化されていません";
        return false;
    }

    switch (GetModuleKind(path)) {
    case ModuleKind::Song: {
        auto song = std::make_shared<Song>();
        if (!LoadSongModule(path, *song, Song::Defaults(), error)) return false;

        std::lock_guard lock(g_driver->mutex());
        g_player->setSong(song);
        return true;
    }

    case ModuleKind::Pattern: {
        // 再生中の曲のパターンを書き換えるため、演奏を止めてから読み込む
        std::lock_guard lock(g_driver->mutex());
        std::shared_ptr<Song> song = g_player->song();
        g_player->stopPlaying();
        const bool ok = LoadPatternModule(path, *song, pattern, to_marked, error);
        g_player->setSong(song);
        return ok;
    }

    default:
        error = L"対応していない形式です";
        return false;
    }
}

void sound_play()
{
    if (!g_player) return;
    std::lock_guard lock(g_driver->mutex());
    g_player->startPlaying();
}

void sound_stop()
{
    if (!g_player) return;
    std::lock_guard lock(g_driver->mutex());
    g_player->stopPlaying();
}

bool sound_set_emulator(EmulatorType type, std::wstring& error)
{
    if (type == g_emulatorType) return true;

    std::shared_ptr<Song> song;
    if (g_player) {
        std::lock_guard lock(g_driver->mutex());
        song = g_player->song();
    }

    const EmulatorType previous = g_emulatorType;
    sound_done();
    g_emulatorType = type;
    bool ok = start_sound();
    error = g_error;

    // 実機が使えなければ元のエミュレータに戻し、演奏を続けられるようにする
    if (!ok && type == EmulatorType::Hardware && previous != EmulatorType::Hardware) {
        sound_done();
        g_emulatorType = previous;
        start_sound();
    }

    if (g_player && song) {
        std::lock_guard lock(g_driver->mutex());
        g_player->setSong(song);
    }
    return ok;
}

EmulatorType sound_emulator()
{
    return g_emulatorType;
}

A2Player* sound_player()
{
    return g_player.get();
}

std::mutex& sound_mutex()
{
    static std::mutex dummy;
    return g_driver ? g_driver->mutex() : dummy;
}

PlayerStatus sound_status()
{
    PlayerStatus s;
    s.device = g_oplDevice ? g_oplDevice->name() : L"";
    s.output = g_audioOutput != nullptr;
    if (!g_player) return s;

    std::lock_guard lock(g_driver->mutex());
    const auto& song = g_player->song();
    s.title = song->title;
    s.songname = song->data().songname.str();
    s.composer = song->data().composer.str();
    s.playing = g_player->playStatus() == A2Player::PlayStatus::Playing;
    s.order = g_player->currentOrder();
    s.pattern = g_player->currentPattern();
    s.line = g_player->currentLine();
    s.speed = g_player->currentSpeed();
    s.tempo = g_player->currentTempo();
    s.tracks = song->data().nm_tracks;
    s.time = g_player->timePlaying();
    return s;
}
