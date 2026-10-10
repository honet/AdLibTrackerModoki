#include "TestScreen.h"

#include <commdlg.h>

#include <cstdarg>
#include <cstdio>
#include <string>

#include "TxtScrIO.h"
#include "../app/Sound.h"
#include "../platform/System.h"
#include "../platform/input/Keyboard.h"

using namespace at2;

namespace
{
    // 配色 (typcons1.inc の既定値)
    constexpr uint8_t main_background = 0x10;
    constexpr uint8_t main_title = 0x0b;
    constexpr uint8_t main_border = 0x0b;
    constexpr uint8_t main_stat_line = 0x03;
    constexpr uint8_t main_hi_stat_line = 0x0b;
    constexpr uint8_t status_background = 0x10;
    constexpr uint8_t status_border = 0x0e;
    constexpr uint8_t status_static_txt = 0x0b;
    constexpr uint8_t status_dynamic_txt = 0x0f;
    constexpr uint8_t dialog_background = 0x40;
    constexpr uint8_t dialog_title = 0x0b;
    constexpr uint8_t dialog_border = 0x0f;
    constexpr uint8_t dialog_text = 0x0d;
    constexpr uint8_t dialog_hi_text = 0x0e;

    std::string to_cp437(const std::wstring& w)
    {
        if (w.empty()) return std::string();
        const int n = WideCharToMultiByte(437, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, "?", nullptr);
        std::string s(n, '\0');
        WideCharToMultiByte(437, 0, w.data(), static_cast<int>(w.size()), s.data(), n, "?", nullptr);
        return s;
    }

    std::string fit(const std::string& s, size_t width)
    {
        std::string r = s.substr(0, width);
        r.resize(width, ' ');
        return r;
    }

    std::string format(const char* fmt, ...)
    {
        char buf[256];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        return buf;
    }

    void fill_screen(uint8_t attr)
    {
        for (int i = 0; i < MaxCol * MaxLn * 2; i += 2) {
            screen_ptr[i] = ' ';
            screen_ptr[i + 1] = attr;
        }
    }

    void draw(uint16_t last_key, const std::string& message)
    {
        const PlayerStatus st = sound_status();
        const uint8_t x1 = 3;
        const uint8_t x2 = static_cast<uint8_t>(MaxCol - 2);
        const size_t width = x2 - x1 - 13;

        fill_screen(main_background);
        fr_setting.shadow_enabled = false;
        Frame(screen_ptr, 1, 1, MaxCol, hard_maxln, main_background + main_border,
              " /|DLiB TR/|CK3R ][ MODOKi ", main_background + main_title, frame_double);

        // 曲情報
        Frame(screen_ptr, x1, 3, x2, 8, status_background + status_border, " SONG ", status_background + status_static_txt, frame_single);
        ShowStr(screen_ptr, x1 + 2, 4, "FILE     ", status_background + status_static_txt);
        ShowStr(screen_ptr, x1 + 11, 4, fit(to_cp437(st.title), width), status_background + status_dynamic_txt);
        ShowStr(screen_ptr, x1 + 2, 5, "TITLE    ", status_background + status_static_txt);
        ShowStr(screen_ptr, x1 + 11, 5, fit(st.songname, width), status_background + status_dynamic_txt);
        ShowStr(screen_ptr, x1 + 2, 6, "COMPOSER ", status_background + status_static_txt);
        ShowStr(screen_ptr, x1 + 11, 6, fit(st.composer, width), status_background + status_dynamic_txt);
        ShowStr(screen_ptr, x1 + 2, 7, "DEVICE   ", status_background + status_static_txt);
        ShowStr(screen_ptr, x1 + 11, 7, fit(to_cp437(st.device) + (st.output ? "  (DirectSound)" : "  (NO OUTPUT)"), width),
                status_background + status_dynamic_txt);

        // 再生状態
        const int seconds = static_cast<int>(st.time);
        Frame(screen_ptr, x1, 10, x2, 14, status_background + status_border, " PLAYBACK ", status_background + status_static_txt, frame_single);
        ShowCStr(screen_ptr, x1 + 2, 11,
                 format("STATUS   ~%-10s~  TIME ~%02d:%02d~", st.playing ? "PLAYiNG" : "STOPPED", seconds / 60, seconds % 60),
                 status_background + status_static_txt, status_background + status_dynamic_txt);
        ShowCStr(screen_ptr, x1 + 2, 12,
                 format("ORDER    ~%02X~   PATTERN ~%02X~   LiNE ~%02X~", st.order, st.pattern, st.line),
                 status_background + status_static_txt, status_background + status_dynamic_txt);
        ShowCStr(screen_ptr, x1 + 2, 13,
                 format("SPEED    ~%02d~   TEMPO   ~%03d~  TRACKS ~%02d~", st.speed, st.tempo, st.tracks),
                 status_background + status_static_txt, status_background + status_dynamic_txt);

        // キーボード (影付きのダイアログ枠)
        fr_setting.shadow_enabled = true;
        Frame(screen_ptr, x1, 16, static_cast<uint8_t>(x1 + 50), 21, dialog_background + dialog_border, " KEYBOARD ",
              dialog_background + dialog_title, frame_double);
        fr_setting.shadow_enabled = false;
        ShowCStr(screen_ptr, x1 + 2, 17,
                 format("LAST KEY  ~$%04X~  (SCAN ~$%02X~  ASCII ~$%02X~)", last_key, last_key >> 8, last_key & 0xff),
                 dialog_background + dialog_text, dialog_background + dialog_hi_text);
        const auto flag = [](bool on, const char* name) { return on ? std::string("~") + name + "~ " : std::string(name) + " "; };
        ShowCStr(screen_ptr, x1 + 2, 18,
                 "MODIFIERS " + flag(left_shift_pressed(), "LSHIFT") + flag(right_shift_pressed(), "RSHIFT") +
                 flag(scankey(SC_LCTRL), "LCTRL") + flag(scankey(SC_RCTRL), "RCTRL") +
                 flag(scankey(SC_LALT), "LALT") + flag(scankey(SC_RALT), "RALT"),
                 dialog_background + dialog_text, dialog_background + dialog_hi_text);
        ShowCStr(screen_ptr, x1 + 2, 19, "LOCKS     " + flag(CapsLock(), "CAPS") + flag(NumLock(), "NUM"),
                 dialog_background + dialog_text, dialog_background + dialog_hi_text);
        ShowStr(screen_ptr, x1 + 2, 20, fit(message, 46), dialog_background + dialog_hi_text);

        // 操作説明
        ShowCStr(screen_ptr, 3, static_cast<uint8_t>(hard_maxln - 2),
                 format("~CTRL+O~ OPEN  ~F5~ PLAY  ~F8~ STOP  ~F9~ EMULATOR  ~ALT+1/2/3~ SCREEN MODE (%d)  ~ESC~ QUiT",
                        program_screen_mode),
                 main_background + main_stat_line, main_background + main_hi_stat_line);

        // 画面モード 0 の表示範囲外 (Ctrl+Tab+カーソルキーでスクロールして見る)
        if (MaxLn > hard_maxln) {
            for (uint8_t y = static_cast<uint8_t>(hard_maxln + 1); y <= MaxLn; ++y) {
                ShowStr(screen_ptr, 3, y, format("ROW %02d: VISIBLE ONLY BY SCROLLING WITH CTRL+TAB+UP/DOWN/PGUP/PGDN", y),
                        main_background + main_stat_line);
            }
        }
    }

    bool show_open_dialog(HWND hwnd, std::wstring& path)
    {
        WCHAR buf[MAX_PATH] = L"";
        OPENFILENAMEW ofn = {};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = hwnd;
        ofn.lpstrFilter = L"AdLib Tracker II モジュール (*.a2m)\0*.a2m\0すべてのファイル (*.*)\0*.*\0";
        ofn.lpstrFile = buf;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
        const bool ok = GetOpenFileNameW(&ofn) != FALSE;
        // ダイアログ表示中に離されたキーの状態が残らないようにする
        keyboard_init();
        if (ok) path = buf;
        return ok;
    }

    void open_song(HWND hwnd, const std::wstring& path, std::string& message)
    {
        std::wstring error;
        if (sound_open_song(path, error)) {
            message = "LOADED.";
        } else {
            message = "LOAD ERROR.";
            MessageBoxW(hwnd, (L"読み込みに失敗しました。\n" + path + L"\n" + error).c_str(), L"AdLibTrackerModoki",
                        MB_OK | MB_ICONWARNING);
            keyboard_init();
        }
    }
}

void run_test_screen(HWND hwnd)
{
    uint16_t last_key = 0;
    std::string message = "PRESS ANY KEY.";

    while (!_force_program_quit) {
        std::wstring dropped;
        if (sys_take_dropped_file(dropped)) open_song(hwnd, dropped, message);

        draw(last_key, message);
        if (!keypressed()) {
            draw_screen();
            continue;
        }

        const uint16_t key = getkey();
        if (_force_program_quit) break;
        if (key == 0xffff) continue;   // Ctrl+Tab (画面スクロール) は読み捨てられる
        last_key = key;

        switch (key) {
        case kCtrlO:
        {
            std::wstring path;
            if (show_open_dialog(hwnd, path)) open_song(hwnd, path, message);
            break;
        }
        case kF5:
            sound_play();
            message = "PLAY.";
            break;
        case kF8:
            sound_stop();
            message = "STOP.";
            break;
        case kF9:
            sound_set_emulator(sound_emulator() == EmulatorType::At2 ? EmulatorType::Nuked : EmulatorType::At2);
            message = "EMULATOR CHANGED.";
            break;
        case kAlt1:
        case kAlt2:
        case kAlt3:
            sys_set_screen_mode(static_cast<uint8_t>((key - kAlt1) >> 8));
            message = "SCREEN MODE CHANGED.";
            break;
        case kESC:
            return;
        default:
            message.clear();
            break;
        }
    }
}
