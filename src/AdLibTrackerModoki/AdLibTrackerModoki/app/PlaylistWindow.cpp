#include "PlaylistWindow.h"

#include <filesystem>
#include <string>
#include <vector>

#include <commctrl.h>

#include "Jukebox.h"
#include "Settings.h"
#include "../Resource.h"

#pragma comment(lib, "comctl32.lib")

namespace
{
    const wchar_t kClassName[] = L"AdLibTrackerModokiPlaylist";
    const wchar_t kCurrentMark[] = L"▶ ";   // 演奏中の曲の印

    HWND g_owner = nullptr;
    HWND g_wnd = nullptr;
    HWND g_list = nullptr;

    uint32_t g_shown_version = 0;               // 表示している曲リストの版 (jukebox_list_version)
    bool g_shown_any = false;                    // 一度でも曲リストを表示したか
    size_t g_marked = SIZE_MAX;                  // 印を付けている行
    bool g_has_folder_column = false;

    bool g_selection_pending = false;
    size_t g_selection = 0;

    int scaled(int value)
    {
        const UINT dpi = g_wnd ? GetDpiForWindow(g_wnd) : USER_DEFAULT_SCREEN_DPI;
        return MulDiv(value, static_cast<int>(dpi), USER_DEFAULT_SCREEN_DPI);
    }

    std::wstring number_text(size_t row, bool marked)
    {
        return (marked ? std::wstring(kCurrentMark) : std::wstring()) + std::to_wstring(row + 1);
    }

    // 曲のフォルダを、曲リストを作ったフォルダからの相対パスで返す (同じフォルダなら空)
    std::wstring relative_folder(const std::wstring& path)
    {
        namespace fs = std::filesystem;
        std::error_code ec;
        const fs::path rel = fs::path(path).parent_path().lexically_relative(fs::path(jukebox_folder()));
        if (rel.empty() || rel == L".") return std::wstring();
        return rel.wstring();
    }

    void set_columns(bool folder_column)
    {
        while (ListView_DeleteColumn(g_list, 0)) {
        }
        LVCOLUMNW col = {};
        col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
        col.fmt = LVCFMT_RIGHT;
        col.cx = scaled(52);
        col.pszText = const_cast<wchar_t*>(L"#");
        ListView_InsertColumn(g_list, 0, &col);
        col.fmt = LVCFMT_LEFT;
        col.cx = scaled(220);
        col.pszText = const_cast<wchar_t*>(L"File");
        ListView_InsertColumn(g_list, 1, &col);
        if (folder_column) {
            col.cx = scaled(140);
            col.pszText = const_cast<wchar_t*>(L"Folder");
            ListView_InsertColumn(g_list, 2, &col);
        }
        g_has_folder_column = folder_column;
    }

    // 曲リストを作り直す
    void rebuild()
    {
        const std::vector<std::wstring>& list = jukebox_list();
        std::vector<std::wstring> folders(list.size());
        bool any_folder = false;
        for (size_t i = 0; i < list.size(); ++i) {
            folders[i] = relative_folder(list[i]);
            if (!folders[i].empty()) any_folder = true;
        }

        SendMessageW(g_list, WM_SETREDRAW, FALSE, 0);
        ListView_DeleteAllItems(g_list);
        if (any_folder != g_has_folder_column) set_columns(any_folder);
        ListView_SetItemCount(g_list, static_cast<int>(list.size()));
        for (size_t i = 0; i < list.size(); ++i) {
            std::wstring number = number_text(i, false);
            LVITEMW item = {};
            item.mask = LVIF_TEXT;
            item.iItem = static_cast<int>(i);
            item.pszText = number.data();
            ListView_InsertItem(g_list, &item);
            std::wstring name = std::filesystem::path(list[i]).filename().wstring();
            ListView_SetItemText(g_list, static_cast<int>(i), 1, name.data());
            if (any_folder) ListView_SetItemText(g_list, static_cast<int>(i), 2, folders[i].data());
        }
        SendMessageW(g_list, WM_SETREDRAW, TRUE, 0);
        InvalidateRect(g_list, nullptr, TRUE);

        g_marked = SIZE_MAX;
        g_shown_version = jukebox_list_version();
        g_shown_any = true;
    }

    void set_mark(size_t row, bool marked)
    {
        if (row >= static_cast<size_t>(ListView_GetItemCount(g_list))) return;
        std::wstring number = number_text(row, marked);
        ListView_SetItemText(g_list, static_cast<int>(row), 0, number.data());
    }

    void update_title()
    {
        std::wstring title = L"Jukebox Playlist";
        const size_t n = jukebox_list().size();
        if (n > 0) title += L" - " + std::to_wstring(n) + L" songs";
        if (!jukebox_active() && n > 0) title += L" (stopped)";
        SetWindowTextW(g_wnd, title.c_str());
    }

    void save_placement()
    {
        if (!g_wnd) return;
        WINDOWPLACEMENT wp = { sizeof(wp) };
        if (!GetWindowPlacement(g_wnd, &wp)) return;
        AppSettings& s = app_settings();
        const RECT& r = wp.rcNormalPosition;
        s.playlist_x = r.left;
        s.playlist_y = r.top;
        s.playlist_width = r.right - r.left;
        s.playlist_height = r.bottom - r.top;
    }

    void request_play(int row)
    {
        if (row < 0 || static_cast<size_t>(row) >= jukebox_list().size()) return;
        g_selection = static_cast<size_t>(row);
        g_selection_pending = true;
        PostMessageW(g_owner, WM_COMMAND, MAKEWPARAM(IDM_JUKEBOX_PLAY_SELECTED, 0), 0);
    }

    LRESULT CALLBACK wnd_proc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message) {
        case WM_SIZE:
            if (g_list) MoveWindow(g_list, 0, 0, LOWORD(lParam), HIWORD(lParam), TRUE);
            return 0;
        case WM_SETFOCUS:
            if (g_list) SetFocus(g_list);
            return 0;
        case WM_NOTIFY: {
            const NMHDR* hdr = reinterpret_cast<const NMHDR*>(lParam);
            if (hdr->hwndFrom == g_list && hdr->code == LVN_ITEMACTIVATE) {
                // ダブルクリック・Enter
                request_play(reinterpret_cast<const NMITEMACTIVATE*>(lParam)->iItem);
                return 0;
            }
            break;
        }
        case WM_CLOSE:
            playlist_window_show(false);
            return 0;
        case WM_DPICHANGED: {
            const RECT* r = reinterpret_cast<const RECT*>(lParam);
            SetWindowPos(hwnd, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            return 0;
        }
        }
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
}

void playlist_window_init(HWND owner)
{
    g_owner = owner;
    const HINSTANCE inst = GetModuleHandleW(nullptr);

    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_LISTVIEW_CLASSES };
    InitCommonControlsEx(&icc);

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = kClassName;
    wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(IDI_SMALL));
    wc.hIconSm = wc.hIcon;
    RegisterClassExW(&wc);

    // 保存した位置がどのモニタにも掛からなければ既定の位置にする
    const AppSettings& s = app_settings();
    int x = CW_USEDEFAULT;
    int y = CW_USEDEFAULT;
    int w = CW_USEDEFAULT;
    int h = CW_USEDEFAULT;
    if (s.playlist_width > 0 && s.playlist_height > 0) {
        const RECT r = { s.playlist_x, s.playlist_y, s.playlist_x + s.playlist_width, s.playlist_y + s.playlist_height };
        if (MonitorFromRect(&r, MONITOR_DEFAULTTONULL) != nullptr) {
            x = r.left;
            y = r.top;
            w = r.right - r.left;
            h = r.bottom - r.top;
        }
    }

    g_wnd = CreateWindowExW(WS_EX_TOOLWINDOW, kClassName, L"Jukebox Playlist", WS_OVERLAPPEDWINDOW,
                            x, y, w, h, owner, nullptr, inst, nullptr);
    if (!g_wnd) return;
    if (w == CW_USEDEFAULT) {
        // 既定の大きさ: メインウィンドウの右隣
        RECT owner_rect = {};
        GetWindowRect(owner, &owner_rect);
        SetWindowPos(g_wnd, nullptr, owner_rect.right, owner_rect.top, scaled(460), scaled(480),
                     SWP_NOZORDER | SWP_NOACTIVATE);
    }

    g_list = CreateWindowExW(0, WC_LISTVIEWW, L"",
                             WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS |
                                 LVS_NOSORTHEADER,
                             0, 0, 0, 0, g_wnd, nullptr, inst, nullptr);
    ListView_SetExtendedListViewStyle(g_list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    set_columns(false);

    RECT client = {};
    GetClientRect(g_wnd, &client);
    MoveWindow(g_list, 0, 0, client.right, client.bottom, FALSE);
    update_title();
}

void playlist_window_done()
{
    if (!g_wnd) return;
    save_placement();
    settings_save();
    DestroyWindow(g_wnd);
    g_wnd = nullptr;
    g_list = nullptr;
}

void playlist_window_show(bool show, bool remember)
{
    if (!g_wnd) return;
    if (show) {
        playlist_window_sync();
        ShowWindow(g_wnd, SW_SHOWNOACTIVATE);
    } else {
        save_placement();
        ShowWindow(g_wnd, SW_HIDE);
    }
    if (remember) {
        app_settings().playlist_visible = show;
        settings_save();
    }
}

bool playlist_window_visible()
{
    return g_wnd && IsWindowVisible(g_wnd);
}

void playlist_window_sync()
{
    if (!g_wnd) return;
    if (!g_shown_any || g_shown_version != jukebox_list_version()) rebuild();

    const size_t current = jukebox_active() ? jukebox_index() : SIZE_MAX;
    if (current != g_marked) {
        if (g_marked != SIZE_MAX) set_mark(g_marked, false);
        if (current != SIZE_MAX) {
            set_mark(current, true);
            const int row = static_cast<int>(current);
            ListView_SetItemState(g_list, -1, 0, LVIS_SELECTED);
            ListView_SetItemState(g_list, row, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            ListView_EnsureVisible(g_list, row, FALSE);
        }
        g_marked = current;
    }
    update_title();
}

bool playlist_window_take_selection(size_t& index)
{
    if (!g_selection_pending) return false;
    g_selection_pending = false;
    index = g_selection;
    return true;
}
