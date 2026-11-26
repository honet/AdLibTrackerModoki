#include "Settings.h"

#include <string>

#include <windows.h>

namespace
{
    AppSettings g_settings;

    struct EmulatorName
    {
        EmulatorType type;
        const wchar_t* name;
    };

    const EmulatorName kEmulatorNames[] = {
        { EmulatorType::At2, L"AT2" },
        { EmulatorType::Nuked, L"Nuked" },
        { EmulatorType::Ymfm, L"ymfm" },
        { EmulatorType::C86ctl, L"c86ctl" },
    };

    std::wstring ini_path()
    {
        std::wstring path(MAX_PATH, L'\0');
        for (;;) {
            const DWORD len = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
            if (len == 0) return L"AdLibTrackerModoki.ini";
            if (len < path.size()) {
                path.resize(len);
                break;
            }
            path.resize(path.size() * 2);
        }
        const size_t pos = path.find_last_of(L"\\/");
        path.resize(pos == std::wstring::npos ? 0 : pos + 1);
        return path + L"AdLibTrackerModoki.ini";
    }
}

AppSettings& app_settings()
{
    return g_settings;
}

void settings_load()
{
    const std::wstring path = ini_path();
    AppSettings s;

    const UINT mode = GetPrivateProfileIntW(L"Display", L"ScreenMode", s.screen_mode, path.c_str());
    if (mode <= 2) s.screen_mode = static_cast<uint8_t>(mode);

    const UINT scale = GetPrivateProfileIntW(L"Display", L"WindowScale", s.window_scale, path.c_str());
    if (scale >= 1 && scale <= 4) s.window_scale = static_cast<int>(scale);

    wchar_t name[32] = {};
    GetPrivateProfileStringW(L"Sound", L"Emulator", L"", name, ARRAYSIZE(name), path.c_str());
    for (const auto& e : kEmulatorNames) {
        if (_wcsicmp(name, e.name) == 0) s.emulator = e.type;
    }

    s.keyon_delay = GetPrivateProfileIntW(L"Sound", L"KeyOnDelay", s.keyon_delay ? 1 : 0, path.c_str()) != 0;

    const UINT loops = GetPrivateProfileIntW(L"Jukebox", L"Loops", s.jukebox_loops, path.c_str());
    if (loops >= 1 && loops <= 99) s.jukebox_loops = static_cast<int>(loops);
    const UINT minutes = GetPrivateProfileIntW(L"Jukebox", L"MaxMinutes", s.jukebox_max_minutes, path.c_str());
    if (minutes >= 1 && minutes <= 59) s.jukebox_max_minutes = static_cast<int>(minutes);
    s.jukebox_shuffle = GetPrivateProfileIntW(L"Jukebox", L"Shuffle", s.jukebox_shuffle ? 1 : 0, path.c_str()) != 0;
    s.jukebox_subfolders = GetPrivateProfileIntW(L"Jukebox", L"Subfolders", s.jukebox_subfolders ? 1 : 0, path.c_str()) != 0;
    s.jukebox_fade = GetPrivateProfileIntW(L"Jukebox", L"FadeOut", s.jukebox_fade ? 1 : 0, path.c_str()) != 0;
    std::wstring folder(MAX_PATH * 4, L'\0');
    const DWORD len = GetPrivateProfileStringW(L"Jukebox", L"Folder", L"", folder.data(),
                                               static_cast<DWORD>(folder.size()), path.c_str());
    folder.resize(len);
    s.jukebox_folder = folder;

    g_settings = s;
}

void settings_save()
{
    const std::wstring path = ini_path();
    const AppSettings& s = g_settings;

    // フォルダ名に ANSI で表せない文字があっても保存できるよう、新しく作る ini は UTF-16 にする
    // (UTF-16 の BOM があるファイルには WritePrivateProfileString も UTF-16 で書き込む)
    if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) {
        HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h != INVALID_HANDLE_VALUE) {
            const BYTE bom[] = { 0xff, 0xfe };
            DWORD written = 0;
            WriteFile(h, bom, sizeof(bom), &written, nullptr);
            CloseHandle(h);
        }
    }

    const wchar_t* emulator = kEmulatorNames[0].name;
    for (const auto& e : kEmulatorNames) {
        if (e.type == s.emulator) emulator = e.name;
    }

    WritePrivateProfileStringW(L"Display", L"ScreenMode", std::to_wstring(s.screen_mode).c_str(), path.c_str());
    WritePrivateProfileStringW(L"Display", L"WindowScale", std::to_wstring(s.window_scale).c_str(), path.c_str());
    WritePrivateProfileStringW(L"Sound", L"Emulator", emulator, path.c_str());
    WritePrivateProfileStringW(L"Sound", L"KeyOnDelay", s.keyon_delay ? L"1" : L"0", path.c_str());
    WritePrivateProfileStringW(L"Jukebox", L"Loops", std::to_wstring(s.jukebox_loops).c_str(), path.c_str());
    WritePrivateProfileStringW(L"Jukebox", L"MaxMinutes", std::to_wstring(s.jukebox_max_minutes).c_str(), path.c_str());
    WritePrivateProfileStringW(L"Jukebox", L"Shuffle", s.jukebox_shuffle ? L"1" : L"0", path.c_str());
    WritePrivateProfileStringW(L"Jukebox", L"Subfolders", s.jukebox_subfolders ? L"1" : L"0", path.c_str());
    WritePrivateProfileStringW(L"Jukebox", L"FadeOut", s.jukebox_fade ? L"1" : L"0", path.c_str());
    WritePrivateProfileStringW(L"Jukebox", L"Folder", s.jukebox_folder.c_str(), path.c_str());
}
