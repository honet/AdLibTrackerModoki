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

    // ウィンドウの拡大率 (クライアント領域 = 画面解像度 × 拡大率)
    constexpr int MAX_WINDOW_SCALE = 4;
    int g_window_scale = 1;         // ユーザーが選んだ拡大率 (ini に保存する)
    int g_current_scale = 1;        // 実際の拡大率 (モニタに入りきらなければ g_window_scale より小さい)
    bool g_window_ready = false;    // 画面モードが決まり、ウィンドウの大きさを合わせられる
    bool g_in_size_move = false;    // ユーザーがウィンドウを移動・大きさ変更中
    bool g_resized_in_drag = false; // 今回のドラッグで大きさを変えた
    bool g_adjusting = false;       // fit 処理中の WM_SIZE を無視する

    // 拡大率 scale のときのウィンドウの大きさ (メニューは 1 行とみなす)
    SIZE window_size_for_scale(int scale, UINT dpi)
    {
        const DWORD style = static_cast<DWORD>(GetWindowLongPtrW(g_hwnd, GWL_STYLE));
        const DWORD ex_style = static_cast<DWORD>(GetWindowLongPtrW(g_hwnd, GWL_EXSTYLE));
        RECT rc = { 0, 0, SCREEN_RES_x * scale, SCREEN_RES_y * scale };
        AdjustWindowRectExForDpi(&rc, style, GetMenu(g_hwnd) != nullptr, ex_style, dpi);
        return { rc.right - rc.left, rc.bottom - rc.top };
    }

    // 作業領域に入りきる最大の拡大率
    int max_fit_scale(const RECT& work, UINT dpi)
    {
        for (int s = MAX_WINDOW_SCALE; s > 1; --s) {
            const SIZE size = window_size_for_scale(s, dpi);
            if (size.cx <= work.right - work.left && size.cy <= work.bottom - work.top) return s;
        }
        return 1;
    }

    RECT monitor_work_area(HMONITOR monitor)
    {
        MONITORINFO mi = { sizeof(mi) };
        GetMonitorInfoW(monitor, &mi);
        return mi.rcWork;
    }

    // ウィンドウを (x, y) に置き、クライアント領域を画面解像度 × scale にする
    void set_client_size(int x, int y, int scale, UINT dpi)
    {
        g_adjusting = true;
        g_current_scale = scale;
        const SIZE size = window_size_for_scale(scale, dpi);
        SetWindowPos(g_hwnd, nullptr, x, y, size.cx, size.cy, SWP_NOZORDER | SWP_NOACTIVATE);

        // メニューバーが折り返して 2 行以上になると、その分クライアント領域が低くなるので足す
        RECT client;
        GetClientRect(g_hwnd, &client);
        const LONG lack = SCREEN_RES_y * scale - (client.bottom - client.top);
        if (lack != 0) {
            SetWindowPos(g_hwnd, nullptr, 0, 0, size.cx, size.cy + lack, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE);
        }
        g_adjusting = false;
    }

    // 作業領域の中央に、選ばれた拡大率 (入りきらなければ下げる) でウィンドウを置く
    void fit_window_to_screen()
    {
        const RECT work = monitor_work_area(MonitorFromWindow(g_hwnd, MONITOR_DEFAULTTONEAREST));
        const UINT dpi = GetDpiForWindow(g_hwnd);
        const int scale = std::min(g_window_scale, max_fit_scale(work, dpi));
        const SIZE size = window_size_for_scale(scale, dpi);
        const int x = work.left + std::max<LONG>(0, (work.right - work.left - size.cx) / 2);
        const int y = work.top + std::max<LONG>(0, (work.bottom - work.top - size.cy) / 2);
        set_client_size(x, y, scale, dpi);
    }

    // クライアント領域の大きさから一番近い拡大率を選ぶ
    int nearest_scale(LONG client_w, LONG client_h, const RECT& work, UINT dpi)
    {
        const double ratio = (static_cast<double>(client_w) / SCREEN_RES_x +
                              static_cast<double>(client_h) / SCREEN_RES_y) / 2;
        const int scale = static_cast<int>(ratio + 0.5);
        return std::clamp(scale, 1, max_fit_scale(work, dpi));
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
    g_window_ready = true;
    fit_window_to_screen();
}

void sys_set_window_scale(int scale)
{
    g_window_scale = std::clamp(scale, 1, MAX_WINDOW_SCALE);
    if (g_window_ready) fit_window_to_screen();
}

int sys_window_scale()
{
    return g_window_scale;
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

void sys_on_resize(WPARAM type)
{
    g_renderer.resize();

    // スナップなど WM_SIZING を通らない大きさの変更も、一番近い拡大率に合わせ直す
    if (!g_window_ready || g_in_size_move || g_adjusting || type == SIZE_MINIMIZED) return;
    RECT client;
    GetClientRect(g_hwnd, &client);
    const LONG w = client.right - client.left;
    const LONG h = client.bottom - client.top;
    if (w == SCREEN_RES_x * g_current_scale && h == SCREEN_RES_y * g_current_scale) return;

    const UINT dpi = GetDpiForWindow(g_hwnd);
    const RECT work = monitor_work_area(MonitorFromWindow(g_hwnd, MONITOR_DEFAULTTONEAREST));
    RECT window;
    GetWindowRect(g_hwnd, &window);
    g_window_scale = nearest_scale(w, h, work, dpi);
    set_client_size(window.left, window.top, g_window_scale, dpi);
}

void sys_on_sizing(WPARAM edge, RECT* rc)
{
    if (!g_window_ready) return;

    // 今のメニューの行数を含めた枠の大きさ
    RECT window, client;
    GetWindowRect(g_hwnd, &window);
    GetClientRect(g_hwnd, &client);
    const LONG frame_w = (window.right - window.left) - (client.right - client.left);
    const LONG frame_h = (window.bottom - window.top) - (client.bottom - client.top);

    LONG client_w = rc->right - rc->left - frame_w;
    LONG client_h = rc->bottom - rc->top - frame_h;
    // 辺のドラッグではその向きの大きさだけで拡大率を決める
    if (edge == WMSZ_LEFT || edge == WMSZ_RIGHT) client_h = client_w * SCREEN_RES_y / SCREEN_RES_x;
    if (edge == WMSZ_TOP || edge == WMSZ_BOTTOM) client_w = client_h * SCREEN_RES_x / SCREEN_RES_y;

    const UINT dpi = GetDpiForWindow(g_hwnd);
    const RECT work = monitor_work_area(MonitorFromRect(rc, MONITOR_DEFAULTTONEAREST));
    const int scale = nearest_scale(client_w, client_h, work, dpi);
    const LONG w = SCREEN_RES_x * scale + frame_w;
    const LONG h = SCREEN_RES_y * scale + frame_h;

    // ドラッグしていない側の辺を固定する
    if (edge == WMSZ_LEFT || edge == WMSZ_TOPLEFT || edge == WMSZ_BOTTOMLEFT) {
        rc->left = rc->right - w;
    } else {
        rc->right = rc->left + w;
    }
    if (edge == WMSZ_TOP || edge == WMSZ_TOPLEFT || edge == WMSZ_TOPRIGHT) {
        rc->top = rc->bottom - h;
    } else {
        rc->bottom = rc->top + h;
    }
    // 選ばれた拡大率はドラッグを終えたときに確定する
    g_current_scale = scale;
    g_resized_in_drag = true;
}

void sys_on_get_min_max_info(MINMAXINFO* info)
{
    if (!g_window_ready) return;
    const UINT dpi = GetDpiForWindow(g_hwnd);
    const SIZE size = window_size_for_scale(1, dpi);
    info->ptMinTrackSize.x = size.cx;
    info->ptMinTrackSize.y = size.cy;
}

void sys_on_enter_size_move()
{
    g_in_size_move = true;
    g_resized_in_drag = false;
}

void sys_on_exit_size_move()
{
    g_in_size_move = false;
    if (!g_window_ready) return;
    // 移動だけのときは、モニタに入りきらず下げている拡大率を選ばれた値にしない
    if (g_resized_in_drag) g_window_scale = g_current_scale;
    // ドラッグ中にメニューの折り返しが変わってずれた分を直す
    RECT window;
    GetWindowRect(g_hwnd, &window);
    set_client_size(window.left, window.top, g_current_scale, GetDpiForWindow(g_hwnd));
}

void sys_on_dpi_changed(UINT dpi, const RECT* suggested)
{
    if (!g_window_ready) {
        SetWindowPos(g_hwnd, nullptr, suggested->left, suggested->top,
                     suggested->right - suggested->left, suggested->bottom - suggested->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        return;
    }
    // 枠の太さが変わってもクライアント領域の大きさ (ドット数) は保つ
    const RECT work = monitor_work_area(MonitorFromRect(suggested, MONITOR_DEFAULTTONEAREST));
    const int scale = std::min(g_window_scale, max_fit_scale(work, dpi));
    set_client_size(suggested->left, suggested->top, scale, dpi);
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
