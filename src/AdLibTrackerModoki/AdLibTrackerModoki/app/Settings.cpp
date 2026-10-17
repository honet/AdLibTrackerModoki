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

    wchar_t name[32] = {};
    GetPrivateProfileStringW(L"Sound", L"Emulator", L"", name, ARRAYSIZE(name), path.c_str());
    for (const auto& e : kEmulatorNames) {
        if (_wcsicmp(name, e.name) == 0) s.emulator = e.type;
    }

    s.keyon_delay = GetPrivateProfileIntW(L"Sound", L"KeyOnDelay", s.keyon_delay ? 1 : 0, path.c_str()) != 0;

    g_settings = s;
}

void settings_save()
{
    const std::wstring path = ini_path();
    const AppSettings& s = g_settings;

    const wchar_t* emulator = kEmulatorNames[0].name;
    for (const auto& e : kEmulatorNames) {
        if (e.type == s.emulator) emulator = e.name;
    }

    WritePrivateProfileStringW(L"Display", L"ScreenMode", std::to_wstring(s.screen_mode).c_str(), path.c_str());
    WritePrivateProfileStringW(L"Sound", L"Emulator", emulator, path.c_str());
    WritePrivateProfileStringW(L"Sound", L"KeyOnDelay", s.keyon_delay ? L"1" : L"0", path.c_str());
}
