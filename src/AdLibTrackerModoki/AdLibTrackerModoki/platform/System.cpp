#include "System.h"

#include <algorithm>
#include <cstring>
#include <deque>
#include <vector>

#include <timeapi.h>

#include "input/Keyboard.h"
#include "video/Direct2DRenderer.h"
#include "../ui/Font8x16.h"
#include "../ui/TxtScrIO.h"

#pragma comment(lib, "winmm.lib")

using namespace at2;

bool _force_program_quit = false;
bool _draw_screen_without_delay = false;
int32_t sdl_frame_rate = 100;

namespace
{
    HWND g_hwnd = nullptr;
    Direct2DRenderer g_renderer;
    std::vector<uint32_t> g_frame;      // ピクセル化したフレーム (BGRA)
    uint32_t g_palette[16] = {};
    bool g_force_redraw = true;
    DWORD g_frame_start = 0;
    std::deque<std::wstring> g_dropped_files;
    std::deque<int> g_commands;
    void (*g_realtime_proc)() = nullptr;

    // adt2sys.pas の draw_SDL_screen
    void draw_SDL_screen(uint8_t columns, uint8_t rows, uint32_t scr_res)
    {
        if (_cursor_blink_pending_frames >= _cursor_blink_factor) {
            _cursor_blink_pending_frames = 0;
            cursor_sync = !cursor_sync;
        }

        uint32_t s_ofs = 0;
        uint32_t v_ofs = 0;
        uint32_t skip = static_cast<uint32_t>(virtual_screen__first_row);
        const uint8_t cur_x = virtual_cur_pos & 0xff;
        const uint8_t cur_y = virtual_cur_pos >> 8;
        const uint8_t shape_lo = virtual_cur_shape & 0xff;
        const uint8_t shape_hi = virtual_cur_shape >> 8;

        for (uint8_t pos_y = 1; pos_y <= rows; ++pos_y) {
            for (uint8_t byte_pos = 0; byte_pos <= 15; ++byte_pos) {
                for (uint8_t pos_x = 1; pos_x <= columns; ++pos_x) {
                    const uint8_t chr = screen_ptr[s_ofs];
                    const uint8_t attr = screen_ptr[s_ofs + 1];
                    const uint8_t bit_mask = font8x16[chr * 16 + byte_pos];
                    for (int bit_pos = 8; bit_pos >= 1; --bit_pos) {
                        if (skip != 0) {
                            --skip;
                        } else if (v_ofs < scr_res) {
                            const bool cursor = cursor_sync && pos_x == cur_x && pos_y == cur_y &&
                                                byte_pos >= shape_hi && byte_pos <= shape_lo;
                            const bool fg = cursor || (bit_mask & (1 << (bit_pos - 1))) != 0;
                            g_frame[v_ofs] = g_palette[fg ? (attr & 0x0f) : (attr >> 4)];
                            ++v_ofs;
                        }
                    }
                    s_ofs += 2;
                }
                s_ofs -= columns * 2;
            }
            s_ofs += columns * 2;
        }
    }

    // adt2sys.pas の draw_screen_proc
    void draw_screen_proc()
    {
        // 本家は画面の内容が変わらなくてもカーソルが点滅するので、点滅を切り替える時期にも描き直す
        const bool blink_due = _cursor_blink_pending_frames >= _cursor_blink_factor;
        if (!g_force_redraw && !blink_due && std::memcmp(screen_ptr, screen_mirror, SCREEN_MEM_SIZE) == 0) return;
        ScreenMemCopy(screen_ptr, screen_mirror);
        g_force_redraw = false;
        _cursor_blink_factor = static_cast<int32_t>(13.0 / 100 * sdl_frame_rate + 0.5);

        const uint32_t res = static_cast<uint32_t>(SCREEN_RES_x) * SCREEN_RES_y;
        g_frame.assign(res, 0);
        draw_SDL_screen(MAX_COLUMNS, MAX_ROWS, res);
        g_renderer.present(g_frame.data(), SCREEN_RES_x, SCREEN_RES_y);
    }

    // Ctrl+Tab+カーソルキーで画面をスクロールする (オリジナルではタイマー処理内)
    void process_screen_scroll()
    {
        if (!ctrl_tab_pressed() || !is_scrollable_screen_mode()) return;

        const uint16_t max_offset = static_cast<uint16_t>(16 * MaxLn - 16 * hard_maxln);
        const uint16_t old = screen_scroll_offset;
        if (scankey(SC_UP) && screen_scroll_offset > 0) screen_scroll_offset = static_cast<uint16_t>(std::max(0, screen_scroll_offset - 2));
        if (scankey(SC_DOWN) && screen_scroll_offset < max_offset) screen_scroll_offset = static_cast<uint16_t>(screen_scroll_offset + 2);
        if (scankey(SC_PAGEUP)) screen_scroll_offset = 0;
        if (scankey(SC_PAGEDOWN)) screen_scroll_offset = max_offset;
        if (screen_scroll_offset != old) {
            synchronize_screen();
            g_force_redraw = true;
        }
    }

    // クライアント領域が画面サイズの整数倍になるようウィンドウの大きさを決める
    void fit_window_to_screen()
    {
        HMONITOR monitor = MonitorFromWindow(g_hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi = { sizeof(mi) };
        GetMonitorInfoW(monitor, &mi);
        const LONG work_w = mi.rcWork.right - mi.rcWork.left;
        const LONG work_h = mi.rcWork.bottom - mi.rcWork.top;

        const DWORD style = static_cast<DWORD>(GetWindowLongPtrW(g_hwnd, GWL_STYLE));
        const DWORD ex_style = static_cast<DWORD>(GetWindowLongPtrW(g_hwnd, GWL_EXSTYLE));
        const UINT dpi = GetDpiForWindow(g_hwnd);

        int scale = 1;
        for (int s = 4; s >= 1; --s) {
            RECT rc = { 0, 0, SCREEN_RES_x * s, SCREEN_RES_y * s };
            AdjustWindowRectExForDpi(&rc, style, GetMenu(g_hwnd) != nullptr, ex_style, dpi);
            if (rc.right - rc.left <= work_w && rc.bottom - rc.top <= work_h) {
                scale = s;
                break;
            }
        }

        RECT rc = { 0, 0, SCREEN_RES_x * scale, SCREEN_RES_y * scale };
        AdjustWindowRectExForDpi(&rc, style, GetMenu(g_hwnd) != nullptr, ex_style, dpi);
        const int w = rc.right - rc.left;
        const int h = rc.bottom - rc.top;
        const int x = mi.rcWork.left + std::max<LONG>(0, (work_w - w) / 2);
        const int y = mi.rcWork.top + std::max<LONG>(0, (work_h - h) / 2);
        SetWindowPos(g_hwnd, nullptr, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

bool sys_init(HWND hwnd)
{
    g_hwnd = hwnd;
    timeBeginPeriod(1);
    vid_SetRGBPalette(rgb_color);
    keyboard_init();
    if (!g_renderer.init(hwnd)) return false;
    g_frame_start = timeGetTime();
    return true;
}

void sys_done()
{
    g_renderer.shutdown();
    timeEndPeriod(1);
}

void sys_set_screen_mode(uint8_t mode)
{
    TxtScrIO_Init(mode);
    std::memset(screen_mirror, 0, sizeof(screen_mirror));
    g_force_redraw = true;
    fit_window_to_screen();
}

void vid_SetRGBPalette(const Rgb* palette)
{
    for (int i = 0; i < 16; ++i) {
        g_palette[i] = 0xff000000u | (palette[i].r << 16) | (palette[i].g << 8) | palette[i].b;
    }
    g_force_redraw = true;
}

void draw_screen()
{
    if (g_realtime_proc) g_realtime_proc();
    process_screen_scroll();
    draw_screen_proc();

    if (_draw_screen_without_delay) {
        _draw_screen_without_delay = false;
    } else {
        // フレームレートを保つ (最低 2ms は待つ)
        const DWORD actual_frame_end = timeGetTime();
        DWORD frame_end = g_frame_start + 1000 / std::max(sdl_frame_rate, 1);
        if (static_cast<int32_t>(actual_frame_end + 2 - frame_end) > 0) frame_end = actual_frame_end + 2;
        Sleep(frame_end - actual_frame_end);
        g_frame_start = timeGetTime();
    }
}

void sys_pump_messages()
{
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            _force_program_quit = true;
            continue;
        }
        DispatchMessageW(&msg);
    }
}

void sys_on_resize()
{
    g_renderer.resize();
}

void sys_on_paint()
{
    g_renderer.redraw();
}

void sys_add_dropped_file(const std::wstring& path)
{
    g_dropped_files.push_back(path);
}

bool sys_take_dropped_file(std::wstring& path)
{
    if (g_dropped_files.empty()) return false;
    path = g_dropped_files.front();
    g_dropped_files.pop_front();
    return true;
}

void sys_add_command(int id)
{
    g_commands.push_back(id);
}

bool sys_take_command(int& id)
{
    if (g_commands.empty()) return false;
    id = g_commands.front();
    g_commands.pop_front();
    return true;
}

bool sys_has_pending_events()
{
    return !g_commands.empty() || !g_dropped_files.empty();
}

void sys_set_realtime_proc(void (*proc)())
{
    g_realtime_proc = proc;
}
