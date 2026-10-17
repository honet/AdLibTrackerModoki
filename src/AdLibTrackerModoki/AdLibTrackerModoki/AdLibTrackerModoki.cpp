// AdLibTrackerModoki.cpp : アプリケーションのエントリ ポイントを定義します。
//

#include "framework.h"
#include "AdLibTrackerModoki.h"

#include <shellapi.h>

#include <string>

#include "app/Settings.h"
#include "app/Sound.h"
#include "platform/System.h"
#include "platform/input/Keyboard.h"
#include "ui/MainScreen.h"
#include "ui/TxtScrIO.h"

#define MAX_LOADSTRING 100

// グローバル変数:
HINSTANCE hInst;                                // 現在のインターフェイス
WCHAR szTitle[MAX_LOADSTRING];                  // タイトル バーのテキスト
WCHAR szWindowClass[MAX_LOADSTRING];            // メイン ウィンドウ クラス名

// このコード モジュールに含まれる関数の宣言を転送します:
ATOM                MyRegisterClass(HINSTANCE hInstance);
HWND                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);

    // 高 DPI 環境でもドットを等倍で扱えるようにする
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // グローバル文字列を初期化する
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_ADLIBTRACKERMODOKI, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // アプリケーション初期化の実行:
    HWND hWnd = InitInstance(hInstance, nCmdShow);
    if (!hWnd)
    {
        return FALSE;
    }

    if (!sys_init(hWnd))
    {
        MessageBoxW(hWnd, L"画面の初期化に失敗しました。", szTitle, MB_OK | MB_ICONERROR);
        DestroyWindow(hWnd);
        return FALSE;
    }
    // 前回終了時の画面モード・拡大率・音源を設定ファイルから復元する
    settings_load();
    const AppSettings& settings = app_settings();
    sys_set_window_scale(settings.window_scale);
    sys_set_screen_mode(settings.screen_mode);
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    sound_set_keyon_delay(settings.keyon_delay);
    if (!sound_init(hWnd, settings.emulator))
    {
        if (settings.emulator == EmulatorType::C86ctl)
        {
            // 実機が使えなければ AT2 互換エミュレータで起動する (設定は実機のまま残す)
            std::wstring error;
            const bool ok = sound_set_emulator(EmulatorType::At2, error);
            MessageBoxW(hWnd, ok ? L"OPL3 実機が使えないため、AT2 互換エミュレータで起動します。"
                                 : L"サウンドの初期化に失敗しました。",
                        szTitle, MB_OK | MB_ICONWARNING);
        }
        else
        {
            MessageBoxW(hWnd, L"サウンドの初期化に失敗しました。", szTitle, MB_OK | MB_ICONWARNING);
        }
    }

    // コマンドラインで渡された曲を開く
    if (lpCmdLine && *lpCmdLine)
    {
        int argc = 0;
        LPWSTR* argv = CommandLineToArgvW(lpCmdLine, &argc);
        if (argv && argc > 0)
        {
            sys_add_dropped_file(argv[0]);
        }
        LocalFree(argv);
    }

    // メインループ (本家と同じく、キー入力待ちの中でメッセージ処理と描画を行う)
    at2::run_main_screen(hWnd);

    sound_done();
    sys_done();
    if (IsWindow(hWnd))
    {
        DestroyWindow(hWnd);
    }
    sys_pump_messages();
    return 0;
}



//
//  関数: MyRegisterClass()
//
//  目的: ウィンドウ クラスを登録します。
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ADLIBTRACKERMODOKI));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = nullptr;
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_ADLIBTRACKERMODOKI);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   関数: InitInstance(HINSTANCE, int)
//
//   目的: インスタンス ハンドルを保存して、メイン ウィンドウを作成します
//
//   コメント:
//
//        この関数で、グローバル変数でインスタンス ハンドルを保存し、
//        メイン プログラム ウィンドウを作成します (表示は画面の初期化後に行う)。
//
HWND InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   UNREFERENCED_PARAMETER(nCmdShow);
   hInst = hInstance; // グローバル変数にインスタンス ハンドルを格納する

   // クライアント領域は画面解像度の整数倍に保つので、モニタの大きさになる最大化は使わない
   HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

   if (hWnd)
   {
      DragAcceptFiles(hWnd, TRUE);
   }
   return hWnd;
}

// ウィンドウの拡大率が変わっていれば設定ファイルに保存する
// (枠のドラッグ中の拡大率はドラッグを終えたときに確定するので、ドラッグ中には保存されない)
static void save_window_scale()
{
    AppSettings& settings = app_settings();
    if (settings.window_scale == sys_window_scale()) return;
    settings.window_scale = sys_window_scale();
    settings_save();
}

//
//  関数: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  目的: メイン ウィンドウのメッセージを処理します。
//
//  キー入力はキーボードモジュールへ渡し、Windows 既定の Alt / F10 によるメニュー操作は行わない
//  (メニューはマウスでのみ操作する)。メニューのコマンドはメインループへ渡す。
//  ウィンドウを閉じる操作は終了要求としてメインループへ伝える。
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
        keyboard_handle_message(message, wParam, lParam);
        if (message == WM_SYSKEYDOWN && wParam == VK_F4)
        {
            // Alt+F4 は通常どおりウィンドウを閉じる
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
        break;
    case WM_SYSCHAR:
        break;
    case WM_COMMAND:
        sys_add_command(LOWORD(wParam));
        break;
    case WM_INITMENUPOPUP:
        {
            HMENU hMenu = reinterpret_cast<HMENU>(wParam);
            CheckMenuRadioItem(hMenu, IDM_VIEW_MODE0, IDM_VIEW_MODE2,
                               IDM_VIEW_MODE0 + at2::program_screen_mode, MF_BYCOMMAND);
            {
                UINT emu = IDM_EMU_AT2;
                switch (sound_emulator()) {
                case EmulatorType::At2: emu = IDM_EMU_AT2; break;
                case EmulatorType::Nuked: emu = IDM_EMU_NUKED; break;
                case EmulatorType::Ymfm: emu = IDM_EMU_YMFM; break;
                case EmulatorType::C86ctl: emu = IDM_EMU_HARDWARE; break;
                }
                // ID が連番でないため CheckMenuRadioItem の範囲指定は使えない。1 項目ずつ付け外しする
                for (UINT id : { IDM_EMU_AT2, IDM_EMU_NUKED, IDM_EMU_YMFM, IDM_EMU_HARDWARE }) {
                    MENUITEMINFOW mii = { sizeof(mii) };
                    mii.fMask = MIIM_FTYPE | MIIM_STATE;
                    if (!GetMenuItemInfoW(hMenu, id, FALSE, &mii)) continue;
                    mii.fType |= MFT_RADIOCHECK;
                    mii.fState = (mii.fState & ~MFS_CHECKED) | (id == emu ? MFS_CHECKED : MFS_UNCHECKED);
                    SetMenuItemInfoW(hMenu, id, FALSE, &mii);
                }
                const bool delayable = emu == IDM_EMU_NUKED || emu == IDM_EMU_YMFM;
                EnableMenuItem(hMenu, IDM_EMU_KEYON_DELAY, MF_BYCOMMAND | (delayable ? MF_ENABLED : MF_GRAYED));
                CheckMenuItem(hMenu, IDM_EMU_KEYON_DELAY, MF_BYCOMMAND | (sound_keyon_delay() ? MF_CHECKED : MF_UNCHECKED));
            }
        }
        break;
    case WM_EXITMENULOOP:
        // メニュー操作中に離されたキーが押されたままにならないようにする
        keyboard_init();
        break;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
        {
            break;
        }
        return DefWindowProc(hWnd, message, wParam, lParam);
    case WM_KILLFOCUS:
        // フォーカスを失っている間に離されたキーが押されたままにならないようにする
        keyboard_init();
        break;
    case WM_SIZE:
        sys_on_resize(wParam);
        save_window_scale();
        break;
    case WM_SIZING:
        sys_on_sizing(wParam, reinterpret_cast<RECT*>(lParam));
        return TRUE;
    case WM_GETMINMAXINFO:
        sys_on_get_min_max_info(reinterpret_cast<MINMAXINFO*>(lParam));
        break;
    case WM_ENTERSIZEMOVE:
        sys_on_enter_size_move();
        break;
    case WM_EXITSIZEMOVE:
        sys_on_exit_size_move();
        save_window_scale();
        break;
    case WM_DPICHANGED:
        sys_on_dpi_changed(HIWORD(wParam), reinterpret_cast<const RECT*>(lParam));
        break;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            BeginPaint(hWnd, &ps);
            sys_on_paint();
            EndPaint(hWnd, &ps);
        }
        break;
    case WM_DROPFILES:
        {
            HDROP hDrop = reinterpret_cast<HDROP>(wParam);
            WCHAR path[MAX_PATH];
            if (DragQueryFileW(hDrop, 0, path, MAX_PATH) > 0)
            {
                sys_add_dropped_file(path);
            }
            DragFinish(hDrop);
        }
        break;
    case WM_CLOSE:
        _force_program_quit = true;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}
