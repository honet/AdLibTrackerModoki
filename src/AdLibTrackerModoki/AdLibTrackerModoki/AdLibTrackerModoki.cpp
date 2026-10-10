// AdLibTrackerModoki.cpp : アプリケーションのエントリ ポイントを定義します。
//

#include "framework.h"
#include "AdLibTrackerModoki.h"

#include <memory>

#include "core/opl/Opl3Emulator.h"
#include "core/opl/OplTestSynth.h"
#include "platform/audio/DirectSoundOutput.h"

#define MAX_LOADSTRING 100

// グローバル変数:
HINSTANCE hInst;                                // 現在のインターフェイス
WCHAR szTitle[MAX_LOADSTRING];                  // タイトル バーのテキスト
WCHAR szWindowClass[MAX_LOADSTRING];            // メイン ウィンドウ クラス名

// サウンド:
// 実機ドライバに差し替える場合は g_oplDevice を実機実装にし、g_audioOutput は不要 (nullptr) になる。
std::unique_ptr<Opl3Emulator> g_oplEmulator;
std::unique_ptr<IAudioOutput> g_audioOutput;
IOplDevice* g_oplDevice = nullptr;
std::unique_ptr<OplTestSynth> g_testSynth;

static bool InitSound(HWND hWnd);
static void ShutdownSound();
static int KeyToNote(WPARAM vk);

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
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: ここにコードを挿入してください。

    // グローバル文字列を初期化する
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_ADLIBTRACKERMODOKI, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // アプリケーション初期化の実行:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
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

   if (!InitSound(hWnd))
   {
      MessageBoxW(hWnd, L"サウンドの初期化に失敗しました。", szTitle, MB_OK | MB_ICONWARNING);
   }

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
            // 暫定表示 (フェーズ3 で Direct2D のテキスト画面に置き換える)
            RECT rc;
            GetClientRect(hWnd, &rc);
            InflateRect(&rc, -16, -16);
            WCHAR text[512];
            swprintf_s(text,
                L"OPL3 テスト\n\n"
                L"デバイス: %s\n"
                L"出力: %s\n\n"
                L"Z S X D C V G B H N J M , : C-4 ～\n"
                L"Q 2 W 3 E R 5 T 6 Y 7 U I : C-5 ～",
                g_oplDevice ? g_oplDevice->name() : L"(なし)",
                g_audioOutput ? L"DirectSound" : L"(なし)");
            DrawTextW(hdc, text, -1, &rc, DT_LEFT | DT_TOP);
            EndPaint(hWnd, &ps);
        }
        break;
    case WM_KEYDOWN:
        if (g_testSynth && !(lParam & (1 << 30)))
        {
            const int note = KeyToNote(wParam);
            if (note >= 0)
            {
                g_testSynth->noteOn(note);
            }
        }
        break;
    case WM_KEYUP:
        if (g_testSynth)
        {
            const int note = KeyToNote(wParam);
            if (note >= 0)
            {
                g_testSynth->noteOff(note);
            }
        }
        break;
    case WM_KILLFOCUS:
        if (g_testSynth)
        {
            g_testSynth->allNotesOff();
        }
        break;
    case WM_DESTROY:
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
//  目的: OPL3 デバイスと音声出力を初期化します。
//
static bool InitSound(HWND hWnd)
{
    g_oplEmulator = std::make_unique<Opl3Emulator>(Opl3Emulator::NativeSampleRate);
    g_oplDevice = g_oplEmulator.get();

    g_testSynth = std::make_unique<OplTestSynth>(*g_oplDevice);
    g_testSynth->init();

    g_audioOutput = std::make_unique<DirectSoundOutput>(hWnd, Opl3Emulator::NativeSampleRate);
    if (!g_audioOutput->start(g_oplEmulator.get()))
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
    g_testSynth.reset();
    g_oplDevice = nullptr;
    g_oplEmulator.reset();
}

// テスト演奏用のキー配置 (AT2 と同じトラッカー式配列)。戻り値は MIDI ノート番号、対象外は -1
static int KeyToNote(WPARAM vk)
{
    static const struct { WPARAM vk; int note; } table[] = {
        // C-4 (60) ～
        { 'Z', 60 }, { 'S', 61 }, { 'X', 62 }, { 'D', 63 }, { 'C', 64 }, { 'V', 65 },
        { 'G', 66 }, { 'B', 67 }, { 'H', 68 }, { 'N', 69 }, { 'J', 70 }, { 'M', 71 },
        { VK_OEM_COMMA, 72 },
        // C-5 (72) ～
        { 'Q', 72 }, { '2', 73 }, { 'W', 74 }, { '3', 75 }, { 'E', 76 }, { 'R', 77 },
        { '5', 78 }, { 'T', 79 }, { '6', 80 }, { 'Y', 81 }, { '7', 82 }, { 'U', 83 },
        { 'I', 84 },
    };
    for (const auto& e : table)
    {
        if (e.vk == vk)
        {
            return e.note;
        }
    }
    return -1;
}
