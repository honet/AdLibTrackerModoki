// AdLibTrackerModoki.cpp : アプリケーションのエントリ ポイントを定義します。
//

#include "framework.h"
#include "AdLibTrackerModoki.h"

#include <commdlg.h>
#include <shellapi.h>

#include <memory>
#include <mutex>
#include <string>

#include "core/opl/Opl3Emulator.h"
#include "core/player/A2Player.h"
#include "core/player/SampleClockDriver.h"
#include "core/song/A2mLoader.h"
#include "platform/audio/DirectSoundOutput.h"

#define MAX_LOADSTRING 100

// グローバル変数:
HINSTANCE hInst;                                // 現在のインターフェイス
WCHAR szTitle[MAX_LOADSTRING];                  // タイトル バーのテキスト
WCHAR szWindowClass[MAX_LOADSTRING];            // メイン ウィンドウ クラス名
HWND g_hWnd = nullptr;                          // メイン ウィンドウ

// サウンド:
// 実機ドライバに差し替える場合は g_oplDevice を実機実装にし、SampleClockDriver の代わりに
// タイマーで A2Player::timerTick() を呼ぶ駆動部を使う (g_audioOutput は不要になる)。
std::unique_ptr<Opl3Emulator> g_oplEmulator;
std::unique_ptr<A2Player> g_player;
std::unique_ptr<SampleClockDriver> g_driver;
std::unique_ptr<IAudioOutput> g_audioOutput;
IOplDevice* g_oplDevice = nullptr;

constexpr UINT_PTR IDT_REFRESH = 1;

static bool InitSound(HWND hWnd);
static void ShutdownSound();
static void OpenSong(HWND hWnd, const std::wstring& path);
static void ShowOpenDialog(HWND hWnd);
static void PlaySong();
static void StopSong();
static void PaintStatus(HWND hWnd, HDC hdc);

// このコード モジュールに含まれる関数の宣言を転送します:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);

    // グローバル文字列を初期化する
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_ADLIBTRACKERMODOKI, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // アプリケーション初期化の実行:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    // コマンドラインで渡された曲を開いて再生する
    if (lpCmdLine && *lpCmdLine)
    {
        int argc = 0;
        LPWSTR* argv = CommandLineToArgvW(lpCmdLine, &argc);
        if (argv && argc > 0)
        {
            OpenSong(g_hWnd, argv[0]);
        }
        LocalFree(argv);
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_ADLIBTRACKERMODOKI));

    MSG msg;

    // メイン メッセージ ループ:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
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
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
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
//        メイン プログラム ウィンドウを作成および表示します。
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // グローバル変数にインスタンス ハンドルを格納する

   HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }
   g_hWnd = hWnd;

   if (!InitSound(hWnd))
   {
      MessageBoxW(hWnd, L"サウンドの初期化に失敗しました。", szTitle, MB_OK | MB_ICONWARNING);
   }

   DragAcceptFiles(hWnd, TRUE);
   SetTimer(hWnd, IDT_REFRESH, 50, nullptr);

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

//
//  関数: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  目的: メイン ウィンドウのメッセージを処理します。
//
//  WM_COMMAND  - アプリケーション メニューの処理
//  WM_PAINT    - メイン ウィンドウを描画する
//  WM_DESTROY  - 中止メッセージを表示して戻る
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // 選択されたメニューの解析:
            switch (wmId)
            {
            case IDM_OPEN:
                ShowOpenDialog(hWnd);
                break;
            case IDM_PLAY:
                PlaySong();
                break;
            case IDM_STOP:
                StopSong();
                break;
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            PaintStatus(hWnd, hdc);
            EndPaint(hWnd, &ps);
        }
        break;
    case WM_TIMER:
        if (wParam == IDT_REFRESH)
        {
            InvalidateRect(hWnd, nullptr, TRUE);
        }
        break;
    case WM_DROPFILES:
        {
            HDROP hDrop = reinterpret_cast<HDROP>(wParam);
            WCHAR path[MAX_PATH];
            if (DragQueryFileW(hDrop, 0, path, MAX_PATH) > 0)
            {
                OpenSong(hWnd, path);
            }
            DragFinish(hDrop);
        }
        break;
    case WM_DESTROY:
        KillTimer(hWnd, IDT_REFRESH);
        ShutdownSound();
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// バージョン情報ボックスのメッセージ ハンドラーです。
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

//
//  関数: InitSound(HWND)
//
//  目的: OPL3 デバイス・プレイヤー・音声出力を初期化します。
//
static bool InitSound(HWND hWnd)
{
    const uint32_t sampleRate = Opl3Emulator::NativeSampleRate;
    g_oplEmulator = std::make_unique<Opl3Emulator>(sampleRate);
    g_oplDevice = g_oplEmulator.get();

    g_player = std::make_unique<A2Player>(*g_oplDevice);
    g_driver = std::make_unique<SampleClockDriver>(*g_player, *g_oplEmulator, sampleRate);

    g_audioOutput = std::make_unique<DirectSoundOutput>(hWnd, sampleRate);
    if (!g_audioOutput->start(g_driver.get()))
    {
        g_audioOutput.reset();
        return false;
    }
    return true;
}

static void ShutdownSound()
{
    if (g_audioOutput)
    {
        g_audioOutput->stop();
        g_audioOutput.reset();
    }
    g_driver.reset();
    g_player.reset();
    g_oplDevice = nullptr;
    g_oplEmulator.reset();
}

//
//  関数: OpenSong(HWND, const std::wstring&)
//
//  目的: 曲を読み込んで再生を開始します。
//
static void OpenSong(HWND hWnd, const std::wstring& path)
{
    if (!g_player)
    {
        return;
    }

    auto song = std::make_shared<Song>();
    std::wstring error;
    if (!LoadA2m(path, *song, Song::Defaults(), error))
    {
        MessageBoxW(hWnd, (L"読み込みに失敗しました。\n" + path + L"\n" + error).c_str(), szTitle, MB_OK | MB_ICONWARNING);
        return;
    }

    {
        std::lock_guard lock(g_driver->mutex());
        g_player->setSong(song);
        g_player->startPlaying();
    }

    SetWindowTextW(hWnd, (song->title + L" - " + szTitle).c_str());
    InvalidateRect(hWnd, nullptr, TRUE);
}

static void ShowOpenDialog(HWND hWnd)
{
    WCHAR path[MAX_PATH] = L"";
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hWnd;
    ofn.lpstrFilter = L"AdLib Tracker II モジュール (*.a2m)\0*.a2m\0すべてのファイル (*.*)\0*.*\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (GetOpenFileNameW(&ofn))
    {
        OpenSong(hWnd, path);
    }
}

static void PlaySong()
{
    if (g_player)
    {
        std::lock_guard lock(g_driver->mutex());
        g_player->startPlaying();
    }
}

static void StopSong()
{
    if (g_player)
    {
        std::lock_guard lock(g_driver->mutex());
        g_player->stopPlaying();
    }
}

// CP437 の Pascal 文字列を表示用に変換する
static std::wstring ToWide(const std::string& s)
{
    if (s.empty())
    {
        return std::wstring();
    }
    const int n = MultiByteToWideChar(437, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(437, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}

//
//  関数: PaintStatus(HWND, HDC)
//
//  目的: 再生状態を表示します (フェーズ3 で Direct2D のテキスト画面に置き換える暫定表示)。
//
static void PaintStatus(HWND hWnd, HDC hdc)
{
    RECT rc;
    GetClientRect(hWnd, &rc);
    InflateRect(&rc, -16, -16);

    std::wstring text = L"ファイルを開く: Ctrl+O またはドラッグ&ドロップ   再生: F5   停止: F8\n\n";
    if (g_player)
    {
        std::lock_guard lock(g_driver->mutex());
        const auto& song = g_player->song();
        const FixedSongData& sd = song->data();
        const wchar_t* status = L"停止中";
        if (g_player->playStatus() == A2Player::PlayStatus::Playing) status = L"再生中";
        else if (g_player->playStatus() == A2Player::PlayStatus::Paused) status = L"一時停止中";

        const int seconds = static_cast<int>(g_player->timePlaying());
        WCHAR buf[512];
        swprintf_s(buf,
            L"ファイル: %s\n曲名: %s\n作者: %s\n\n"
            L"状態: %s\n"
            L"オーダー: %02X   パターン: %02X   行: %02X\n"
            L"スピード: %u   テンポ: %u   トラック数: %u\n"
            L"時間: %02d:%02d\n",
            song->title.c_str(), ToWide(sd.songname.str()).c_str(), ToWide(sd.composer.str()).c_str(),
            status,
            g_player->currentOrder(), g_player->currentPattern(), g_player->currentLine(),
            g_player->currentSpeed(), g_player->currentTempo(), sd.nm_tracks,
            seconds / 60, seconds % 60);
        text += buf;
    }
    text += std::wstring(L"\n音源: ") + (g_oplDevice ? g_oplDevice->name() : L"(なし)") +
            L"   出力: " + (g_audioOutput ? L"DirectSound" : L"(なし)");

    DrawTextW(hdc, text.c_str(), -1, &rc, DT_LEFT | DT_TOP);
}
