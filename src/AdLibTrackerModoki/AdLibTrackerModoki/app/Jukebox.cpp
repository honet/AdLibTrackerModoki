#include "Jukebox.h"

#include <algorithm>
#include <filesystem>
#include <random>
#include <system_error>
#include <vector>

#include <windows.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <shobjidl.h>

#include "Settings.h"
#include "../Resource.h"
#include "../core/song/ModuleLoader.h"

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "comctl32.lib")

namespace
{
    std::vector<std::wstring> g_sorted;   // 名前順の曲リスト
    std::vector<std::wstring> g_list;     // 演奏順 (シャッフル中は並べ替えたもの)
    size_t g_index = 0;
    bool g_active = false;
    bool g_shuffle = false;
    std::wstring g_folder;                // 曲リストを作ったフォルダ
    uint32_t g_list_version = 0;          // 曲リストの並びが変わるたびに増やす
    const std::wstring g_empty;

    std::mt19937& rng()
    {
        static std::mt19937 engine(std::random_device{}());
        return engine;
    }

    // エクスプローラーと同じ並び (数字を数値として比べる)
    bool explorer_less(const std::wstring& a, const std::wstring& b)
    {
        return StrCmpLogicalW(a.c_str(), b.c_str()) < 0;
    }

    void collect(const std::filesystem::path& folder, bool subfolders, std::vector<std::wstring>& out)
    {
        namespace fs = std::filesystem;
        std::error_code ec;
        std::vector<std::wstring> files;
        std::vector<fs::path> dirs;
        for (fs::directory_iterator it(folder, fs::directory_options::skip_permission_denied, ec), end;
             !ec && it != end; it.increment(ec)) {
            std::error_code ec2;
            if (it->is_directory(ec2)) {
                if (subfolders) dirs.push_back(it->path());
            } else if (it->is_regular_file(ec2)) {
                const std::wstring path = it->path().wstring();
                if (GetModuleKind(path) == ModuleKind::Song) files.push_back(path);
            }
        }
        std::sort(files.begin(), files.end(), explorer_less);
        out.insert(out.end(), files.begin(), files.end());

        // サブフォルダはそのフォルダのファイルの後に、名前順でたどる
        std::sort(dirs.begin(), dirs.end(),
                  [](const fs::path& a, const fs::path& b) { return explorer_less(a.wstring(), b.wstring()); });
        for (const auto& dir : dirs) collect(dir, true, out);
    }

    // first 番目以降をシャッフルする
    void shuffle_from(size_t first)
    {
        if (first < g_list.size()) std::shuffle(g_list.begin() + static_cast<ptrdiff_t>(first), g_list.end(), rng());
    }
}

bool jukebox_build(const std::wstring& folder, bool subfolders, bool shuffle)
{
    std::vector<std::wstring> files;
    collect(folder, subfolders, files);
    if (files.empty()) return false;

    g_sorted = std::move(files);
    g_folder = folder;
    g_list = g_sorted;
    g_shuffle = shuffle;
    g_index = 0;
    if (g_shuffle) shuffle_from(0);
    ++g_list_version;
    g_active = true;
    return true;
}

bool jukebox_active()
{
    return g_active;
}

void jukebox_stop()
{
    g_active = false;
}

size_t jukebox_count()
{
    return g_list.size();
}

size_t jukebox_index()
{
    return g_index;
}

const std::wstring& jukebox_current()
{
    return g_index < g_list.size() ? g_list[g_index] : g_empty;
}

const std::wstring& jukebox_step(int dir)
{
    if (g_list.empty()) return g_empty;
    const size_t n = g_list.size();
    if (dir >= 0) {
        if (++g_index >= n) {
            g_index = 0;
            if (g_shuffle && n > 1) {
                // 1 周したら並べ直す (同じ曲が続かないよう、直前の曲は先頭にしない)
                const std::wstring last = g_list[n - 1];
                shuffle_from(0);
                if (g_list[0] == last) std::swap(g_list[0], g_list[n - 1]);
                ++g_list_version;
            }
        }
    } else {
        g_index = g_index == 0 ? n - 1 : g_index - 1;
    }
    return g_list[g_index];
}

bool jukebox_jump(size_t index)
{
    if (index >= g_list.size()) return false;
    g_index = index;
    g_active = true;
    return true;
}

const std::vector<std::wstring>& jukebox_list()
{
    return g_list;
}

const std::wstring& jukebox_folder()
{
    return g_folder;
}

uint32_t jukebox_list_version()
{
    return g_list_version;
}

void jukebox_set_shuffle(bool shuffle)
{
    g_shuffle = shuffle;
    if (g_list.empty()) return;
    ++g_list_version;
    const std::wstring current = g_list[g_index];
    g_list = g_sorted;
    if (shuffle) {
        // 今の曲を先頭に置き、残りを並べ替える
        const auto it = std::find(g_list.begin(), g_list.end(), current);
        if (it != g_list.end()) std::iter_swap(g_list.begin(), it);
        g_index = 0;
        shuffle_from(1);
    } else {
        const auto it = std::find(g_list.begin(), g_list.end(), current);
        g_index = it != g_list.end() ? static_cast<size_t>(it - g_list.begin()) : 0;
    }
}

bool jukebox_pick_folder(HWND owner, std::wstring& folder)
{
    const HRESULT hr_init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    bool picked = false;

    IFileOpenDialog* dialog = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) {
        DWORD options = 0;
        dialog->GetOptions(&options);
        dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
        dialog->SetTitle(L"Jukebox - Select a folder");
        if (!folder.empty()) {
            IShellItem* initial = nullptr;
            if (SUCCEEDED(SHCreateItemFromParsingName(folder.c_str(), nullptr, IID_PPV_ARGS(&initial)))) {
                dialog->SetFolder(initial);
                initial->Release();
            }
        }
        if (SUCCEEDED(dialog->Show(owner))) {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dialog->GetResult(&item))) {
                PWSTR path = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                    folder = path;
                    picked = true;
                    CoTaskMemFree(path);
                }
                item->Release();
            }
        }
        dialog->Release();
    }

    if (SUCCEEDED(hr_init)) CoUninitialize();
    return picked;
}

namespace
{
    INT_PTR CALLBACK settings_dialog_proc(HWND dlg, UINT message, WPARAM wParam, LPARAM)
    {
        switch (message) {
        case WM_INITDIALOG: {
            const AppSettings& s = app_settings();
            SendDlgItemMessageW(dlg, IDC_JUKEBOX_LOOPS_SPIN, UDM_SETRANGE32, 1, 99);
            SendDlgItemMessageW(dlg, IDC_JUKEBOX_MINUTES_SPIN, UDM_SETRANGE32, 1, 59);
            SetDlgItemInt(dlg, IDC_JUKEBOX_LOOPS, static_cast<UINT>(s.jukebox_loops), FALSE);
            SetDlgItemInt(dlg, IDC_JUKEBOX_MINUTES, static_cast<UINT>(s.jukebox_max_minutes), FALSE);
            CheckDlgButton(dlg, IDC_JUKEBOX_SUBFOLDERS, s.jukebox_subfolders ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(dlg, IDC_JUKEBOX_FADE, s.jukebox_fade ? BST_CHECKED : BST_UNCHECKED);
            return TRUE;
        }
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
            case IDOK: {
                BOOL ok_loops = FALSE;
                BOOL ok_minutes = FALSE;
                const UINT loops = GetDlgItemInt(dlg, IDC_JUKEBOX_LOOPS, &ok_loops, FALSE);
                const UINT minutes = GetDlgItemInt(dlg, IDC_JUKEBOX_MINUTES, &ok_minutes, FALSE);
                if (!ok_loops || loops < 1 || loops > 99) {
                    MessageBoxW(dlg, L"Loops must be between 1 and 99.", L"Jukebox", MB_OK | MB_ICONWARNING);
                    SetFocus(GetDlgItem(dlg, IDC_JUKEBOX_LOOPS));
                    return TRUE;
                }
                if (!ok_minutes || minutes < 1 || minutes > 59) {
                    MessageBoxW(dlg, L"Maximum time must be between 1 and 59 minutes.", L"Jukebox", MB_OK | MB_ICONWARNING);
                    SetFocus(GetDlgItem(dlg, IDC_JUKEBOX_MINUTES));
                    return TRUE;
                }
                AppSettings& s = app_settings();
                s.jukebox_loops = static_cast<int>(loops);
                s.jukebox_max_minutes = static_cast<int>(minutes);
                s.jukebox_subfolders = IsDlgButtonChecked(dlg, IDC_JUKEBOX_SUBFOLDERS) == BST_CHECKED;
                s.jukebox_fade = IsDlgButtonChecked(dlg, IDC_JUKEBOX_FADE) == BST_CHECKED;
                settings_save();
                EndDialog(dlg, IDOK);
                return TRUE;
            }
            case IDCANCEL:
                EndDialog(dlg, IDCANCEL);
                return TRUE;
            }
            break;
        }
        return FALSE;
    }
}

bool jukebox_settings_dialog(HWND owner)
{
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_UPDOWN_CLASS };
    InitCommonControlsEx(&icc);
    return DialogBoxParamW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDD_JUKEBOX_SETTINGS), owner,
                           settings_dialog_proc, 0) == IDOK;
}
