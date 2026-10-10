#include "Sound.h"

#include <memory>
#include <mutex>

#include "../core/opl/Opl3Emulator.h"
#include "../core/opl/Opl3EmulatorAt2.h"
#include "../core/player/A2Player.h"
#include "../core/player/SampleClockDriver.h"
#include "../core/song/A2mLoader.h"
#include "../platform/audio/DirectSoundOutput.h"

namespace
{
    HWND g_hwnd = nullptr;
    EmulatorType g_emulatorType = EmulatorType::At2;
    std::unique_ptr<Opl3EmulatorAt2> g_emulatorAt2;
    std::unique_ptr<Opl3Emulator> g_emulatorNuked;
    IOplDevice* g_oplDevice = nullptr;
    IAudioSource* g_oplSource = nullptr;
    std::unique_ptr<A2Player> g_player;
    std::unique_ptr<SampleClockDriver> g_driver;
    std::unique_ptr<IAudioOutput> g_audioOutput;

    bool start_sound()
    {
        const uint32_t sampleRate = Opl3EmulatorAt2::NativeSampleRate;
        if (g_emulatorType == EmulatorType::At2) {
            g_emulatorAt2 = std::make_unique<Opl3EmulatorAt2>();
            g_oplDevice = g_emulatorAt2.get();
            g_oplSource = g_emulatorAt2.get();
        } else {
            g_emulatorNuked = std::make_unique<Opl3Emulator>(sampleRate);
            g_oplDevice = g_emulatorNuked.get();
            g_oplSource = g_emulatorNuked.get();
        }

        g_player = std::make_unique<A2Player>(*g_oplDevice);
        g_driver = std::make_unique<SampleClockDriver>(*g_player, *g_oplSource, sampleRate);

        g_audioOutput = std::make_unique<DirectSoundOutput>(g_hwnd, sampleRate);
        if (!g_audioOutput->start(g_driver.get())) {
            g_audioOutput.reset();
            return false;
        }
        return true;
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
    g_driver.reset();
    g_player.reset();
    g_oplDevice = nullptr;
    g_oplSource = nullptr;
    g_emulatorAt2.reset();
    g_emulatorNuked.reset();
}

bool sound_open_song(const std::wstring& path, std::wstring& error)
{
    if (!g_player) {
        error = L"サウンドが初期化されていません";
        return false;
    }

    auto song = std::make_shared<Song>();
    if (!LoadA2m(path, *song, Song::Defaults(), error)) return false;

    std::lock_guard lock(g_driver->mutex());
    g_player->setSong(song);
    return true;
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

bool sound_set_emulator(EmulatorType type)
{
    if (type == g_emulatorType) return true;

    std::shared_ptr<Song> song;
    if (g_player) {
        std::lock_guard lock(g_driver->mutex());
        song = g_player->song();
    }

    sound_done();
    g_emulatorType = type;
    const bool ok = start_sound();

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
