#pragma once

#include <cstdint>
#include <string>

// テキスト画面 (txtscrio.pas の SDL 版の移植)
//
// 画面メモリは 1 セル 2 バイト ([文字コード, 属性])、属性は下位 4bit が前景色・上位 4bit が背景色。
// 座標は Pascal 版と同じく 1 始まり。文字列は CP437 のバイト列として扱う。
// 後続フェーズで AT2 の UI コードを移植しやすいよう、名前はオリジナルに合わせている。
// (GetCursor など Win32 API と衝突する名前があるため namespace at2 に置く)

namespace at2
{

constexpr int MAX_SCREEN_MEM_SIZE = 180 * 60 * 2;

using tSCREEN_MEM = uint8_t[MAX_SCREEN_MEM_SIZE];
using tSCREEN_MEM_PTR = uint8_t*;

extern uint16_t SCREEN_RES_x;
extern uint16_t SCREEN_RES_y;
extern uint8_t MAX_COLUMNS;
extern uint8_t MAX_ROWS;
extern uint8_t MAX_TRACKS;
extern uint8_t MAX_ORDER_COLS;
extern uint8_t MAX_PATTERN_ROWS;
extern uint8_t INSCTRL_xshift;
extern int8_t INSCTRL_yshift;
extern uint8_t INSEDIT_yshift;
extern uint8_t PATTORD_xshift;
extern uint8_t GOTOXY_xshift;
extern int32_t SCREEN_MEM_SIZE;

extern tSCREEN_MEM temp_screen;
extern tSCREEN_MEM temp_screen2;
extern tSCREEN_MEM screen_backup;
extern tSCREEN_MEM scr_backup;
extern tSCREEN_MEM scr_backup2;
extern tSCREEN_MEM screen_mirror;
extern tSCREEN_MEM screen_emulator;
extern tSCREEN_MEM text_screen_shadow;

extern tSCREEN_MEM_PTR screen_ptr;
extern tSCREEN_MEM_PTR centered_frame_vdest;
extern tSCREEN_MEM_PTR move_to_screen_data;
extern uint8_t move_to_screen_area[5];   // [1..4]

// ダイアログを閉じるときに背景を描き直す処理 (オリジナルの move_to_screen_routine。メイン画面が move2screen を設定する)
// 未設定なら保存しておいた画面をそのまま戻す。
extern void (*move_to_screen_routine)();

// 画面モード: 0 = 90x40 (720x480、30 行表示でスクロール), 1 = 120x50 (960x800), 2 = 180x60 (1440x960)
extern uint8_t program_screen_mode;

extern uint8_t MaxLn;
extern uint8_t MaxCol;
extern uint8_t hard_maxcol;
extern uint8_t hard_maxln;
extern uint8_t work_maxcol;
extern uint8_t work_maxln;
extern uint8_t scr_font_width;
extern uint8_t scr_font_height;

// show_* の書き込み除外領域 (表示中のウィンドウとその影)
extern uint8_t area_x1, area_y1, area_x2, area_y2;
extern uint8_t scroll_pos0, scroll_pos1, scroll_pos2, scroll_pos3, scroll_pos4;

extern int32_t cursor_backup;

// カーソル・スクロール (adt2sys.pas / adt2unit.pas)
extern uint16_t virtual_cur_pos;     // 下位 = X、上位 = Y
extern uint16_t virtual_cur_shape;   // 上位 = 開始ライン、下位 = 終了ライン
extern int32_t virtual_screen__first_row;
extern uint16_t screen_scroll_offset;
extern bool cursor_sync;
extern int32_t _cursor_blink_factor;
extern int32_t _cursor_blink_pending_frames;

enum : uint8_t
{
    Black = 0x00, Blue = 0x01, Green = 0x02, Cyan = 0x03, Red = 0x04, Magenta = 0x05, Brown = 0x06, LGray = 0x07,
    DGray = 0x08, LBlue = 0x09, LGreen = 0x0a, LCyan = 0x0b, LRed = 0x0c, LMagenta = 0x0d, Yellow = 0x0e, White = 0x0f,
    Blink = 0x80,
};

void ShowStr(tSCREEN_MEM_PTR dest, uint8_t x, uint8_t y, const std::string& str, uint8_t attr);
void ShowVStr(tSCREEN_MEM_PTR dest, uint8_t x, uint8_t y, const std::string& str, uint8_t attr);
void ShowCStr(tSCREEN_MEM_PTR dest, uint8_t x, uint8_t y, const std::string& str, uint8_t atr1, uint8_t atr2);
void ShowCStr2(tSCREEN_MEM_PTR dest, uint8_t x, uint8_t y, const std::string& str, uint8_t atr1, uint8_t atr2);
void ShowVCStr(tSCREEN_MEM_PTR dest, uint8_t x, uint8_t y, const std::string& str, uint8_t atr1, uint8_t atr2);
void ShowVCStr2(tSCREEN_MEM_PTR dest, uint8_t x, uint8_t y, const std::string& str, uint8_t atr1, uint8_t atr2);
void ShowC3Str(tSCREEN_MEM_PTR dest, uint8_t x, uint8_t y, const std::string& str, uint8_t atr1, uint8_t atr2, uint8_t atr3);
void ShowVC3Str(tSCREEN_MEM_PTR dest, uint8_t x, uint8_t y, const std::string& str, uint8_t atr1, uint8_t atr2, uint8_t atr3);
void ShowC4Str(tSCREEN_MEM_PTR dest, uint8_t x, uint8_t y, const std::string& str, uint8_t atr1, uint8_t atr2, uint8_t atr3, uint8_t atr4);
void show_str(uint8_t xpos, uint8_t ypos, const std::string& str, uint8_t color);
void show_cstr(uint8_t xpos, uint8_t ypos, const std::string& str, uint8_t attr1, uint8_t attr2);
void show_cstr_alt(uint8_t xpos, uint8_t ypos, const std::string& str, uint8_t attr1, uint8_t attr2);
void show_vstr(uint8_t xpos, uint8_t ypos, const std::string& str, uint8_t color);
void show_vcstr(uint8_t xpos, uint8_t ypos, const std::string& str, uint8_t attr1, uint8_t attr2);

uint8_t CStrLen(const std::string& str);
uint8_t CStr2Len(const std::string& str);
uint8_t C3StrLen(const std::string& str);

void ScreenMemCopy(const uint8_t* source, tSCREEN_MEM_PTR dest);
void move2screen_alt();
void TxtScrIO_Init(uint8_t screen_mode);
bool is_default_screen_mode();
bool is_scrollable_screen_mode();

struct tFRAME_SETTING
{
    bool shadow_enabled;
    bool wide_range_type;
    bool zooming_enabled;
    bool update_area;
};
extern tFRAME_SETTING fr_setting;

// border: 8 文字 (左上、上、右上、左、右、左下、下、右下)
void Frame(tSCREEN_MEM_PTR dest, uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t atr1,
           const std::string& title, uint8_t atr2, const std::string& border);

uint8_t WhereX();
uint8_t WhereY();
void GotoXY(uint8_t x, uint8_t y);
int32_t GetCursor();
void SetCursor(int32_t cursor);
void ThinCursor();
void WideCursor();
void HideCursor();
uint16_t GetCursorShape();
void SetCursorShape(uint16_t shape);

// 画面スクロール位置を反映する (adt2unit.pas の synchronize_screen の SDL 版)
void synchronize_screen();

// 枠の文字 (typcons2.inc)
extern const std::string frame_single;
extern const std::string frame_double;
extern const std::string frame_double_side;
extern const std::string frame_double_top;

} // namespace at2
