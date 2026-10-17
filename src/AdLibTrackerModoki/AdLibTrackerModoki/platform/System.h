#pragma once

#include <cstdint>
#include <string>

#include <windows.h>

struct Rgb;

// システム層 (adt2sys.pas の SDL 版相当): 画面描画・メッセージ処理・終了要求

extern bool _force_program_quit;           // ウィンドウが閉じられた
extern bool _draw_screen_without_delay;    // 次の draw_screen でフレーム待ちをしない
extern int32_t sdl_frame_rate;             // 画面のフレームレート (fps)

bool sys_init(HWND hwnd);
void sys_done();

// 画面モードを切り替え、ウィンドウを画面サイズに合わせる (TxtScrIO_Init を含む)
void sys_set_screen_mode(uint8_t mode);

// パレット (16 色) を設定する
void vid_SetRGBPalette(const Rgb* palette);

// テキスト画面が変わっていれば表示し、フレームレートを保つよう待つ
void draw_screen();

// 溜まっているウィンドウメッセージを処理する
void sys_pump_messages();

// ウィンドウの拡大率 (1〜4)。クライアント領域は画面解像度 × 拡大率に保つ
// (モニタに入りきらないときは一時的に下げるが、選ばれた値は変えない)
void sys_set_window_scale(int scale);
int sys_window_scale();

// ウィンドウプロシージャから呼ぶ
void sys_on_resize(WPARAM type);
void sys_on_sizing(WPARAM edge, RECT* rc);
void sys_on_get_min_max_info(MINMAXINFO* info);
void sys_on_enter_size_move();
void sys_on_exit_size_move();
void sys_on_dpi_changed(UINT dpi, const RECT* suggested);
void sys_on_paint();
void sys_add_dropped_file(const std::wstring& path);

// ドロップされたファイルを 1 つ取り出す
bool sys_take_dropped_file(std::wstring& path);

// メニューのコマンドを積む / 1 つ取り出す
void sys_add_command(int id);
bool sys_take_command(int& id);
bool sys_has_pending_events();   // 未処理のコマンドまたはドロップされたファイルがある

// draw_screen の先頭で毎フレーム呼ぶ処理を登録する (オリジナルの realtime_gfx_poll_proc)
void sys_set_realtime_proc(void (*proc)());
