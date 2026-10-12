#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "StringIO.h"
#include "TxtScrIO.h"

// ダイアログとメニュー (dialogio.pas の Dialog / Menu の移植)
//
// 文字列は CP437 のバイト列。text / keys は '$' 区切り、keys の '~X~' はショートカットキーを表す
// ('~' のない項目は選べない)。
// 文字列入力 ('%string_input%') と Fselect は、使う機能の実装とともに追加する。
namespace at2
{

// 配色 (typcons1.inc の既定値)
constexpr uint8_t dialog_background = 0x40;
constexpr uint8_t dialog_title = 0x0b;
constexpr uint8_t dialog_border = 0x0f;
constexpr uint8_t dialog_text = 0x0d;
constexpr uint8_t dialog_hi_text = 0x0e;
constexpr uint8_t dialog_item = 0x0f;
constexpr uint8_t dialog_short = 0x0e;
constexpr uint8_t dialog_item_dis = 0x05;
constexpr uint8_t dialog_sel_itm_bck = 0x70;
constexpr uint8_t dialog_sel_itm = 0x00;
constexpr uint8_t dialog_sel_short = 0x04;
constexpr uint8_t dialog_context = 0x0e;
constexpr uint8_t dialog_context_dis = 0x07;
constexpr uint8_t dialog_input_bckg = 0x70;
constexpr uint8_t dialog_input = 0x00;
constexpr uint8_t dialog_def_bckg = 0xc0;
constexpr uint8_t dialog_def = 0x00;
constexpr uint8_t dialog_prog_bar1 = 0x09;
constexpr uint8_t dialog_prog_bar2 = 0x0b;

struct tDIALOG_SETTING
{
    std::string frame_type;
    bool shadow_enabled;
    uint8_t title_attr;
    uint8_t box_attr;
    uint8_t text_attr;
    uint8_t text2_attr;
    uint8_t keys_attr;
    uint8_t keys2_attr;
    uint8_t short_attr;
    uint8_t short2_attr;
    uint8_t disbld_attr;
    uint8_t contxt_attr;
    uint8_t contxt2_attr;
    uint8_t xstart;
    uint8_t ystart;
    bool center_box;
    bool center_text;
    bool cycle_moves;
    bool all_enabled;
    uint16_t terminate_keys[50];   // [1..50]
};

struct tDIALOG_ENVIRONMENT
{
    uint16_t keystroke;
    std::string context;
    std::string input_str;
    uint8_t xpos, ypos;
    uint8_t xsize, ysize;
    uint8_t cur_item;
    void (*ext_proc)();
};

// DialogIO_Init で設定された状態で始まる
extern tDIALOG_SETTING dl_setting;
extern tDIALOG_ENVIRONMENT dl_environment;

// 選ばれた項目の番号 (1 始まり) を返す。押されたキーは dl_environment.keystroke に入る。
uint8_t Dialog(const std::string& text, const std::string& keys, const std::string& title, uint8_t spos);

// --- メニュー (dialogio.pas の Menu) ---

constexpr uint8_t dialog_topic = 0x0d;
constexpr uint8_t dialog_hi_topic = 0x0e;

struct tMENU_SETTING
{
    std::string frame_type;
    bool frame_enabled;
    bool shadow_enabled;
    bool posbar_enabled;
    uint8_t title_attr;
    uint8_t menu_attr;
    uint8_t text_attr;
    uint8_t text2_attr;
    uint8_t default_attr;
    uint8_t short_attr;
    uint8_t short2_attr;
    uint8_t disbld_attr;
    uint8_t contxt_attr;
    uint8_t contxt2_attr;
    uint8_t topic_attr;
    uint8_t hi_topic_attr;
    tCHARSET topic_mask_chr;
    bool center_box;
    bool cycle_moves;
    bool edit_contents;
    bool reverse_use;
    bool show_scrollbar;
    uint8_t topic_len;
    uint8_t fixed_len;
    int32_t homing_pos;
    uint16_t terminate_keys[50];   // [1..50]
};

struct tMENU_ENVIRONMENT
{
    tSCREEN_MEM_PTR v_dest;
    uint16_t keystroke;
    std::string context;
    bool unpolite;
    bool winshade;
    bool intact_area;
    uint8_t edit_pos;
    uint16_t curr_page;
    uint16_t curr_pos;
    std::string curr_item;
    void (*ext_proc)();
    void (*ext_proc_rt)();
    void (*refresh)();
    bool do_refresh;
    bool own_refresh;
    bool preview;
    uint8_t fixed_start;
    uint8_t descr_len;
    const std::vector<std::string>* descr;   // 項目の説明 (オリジナルは String[descr_len] の配列へのポインタ)
    bool is_editing;
    uint8_t xpos, ypos;
    uint8_t xsize, ysize;
    uint8_t desc_pos;
    uint8_t hlight_chrs;
};

extern tMENU_SETTING mn_setting;
extern tMENU_ENVIRONMENT mn_environment;

// 縦のスクロールバー。表示した位置を返す (前回の位置 pos と同じなら描かない)
uint16_t VScrollBar(tSCREEN_MEM_PTR dest, uint8_t x, uint8_t y, uint8_t size, uint16_t len1, uint16_t len2, uint16_t pos,
                    uint8_t atr1, uint8_t atr2);

// data[0..count-1] の項目 (各 len 文字まで) を len2 行の窓で選ばせ、選んだ項目の番号 (1 始まり) を返す。
// 押されたキーは mn_environment.keystroke に入る。edit_contents のときは data を書き換える。
uint16_t Menu(std::vector<std::string>& data, uint8_t x, uint8_t y, uint16_t spos, uint8_t len, uint8_t len2,
              uint16_t count, const std::string& title);

// --- 1 行の文字列入力 (stringio.pas の InputStr) ---

struct tINPUT_STR_SETTING
{
    bool insert_mode;
    bool replace_enabled;
    bool append_enabled;
    tCHARSET char_filter;
    tCHARSET character_set;
    tCHARSET valid_chars;
    tCHARSET word_characters;
    uint16_t terminate_keys[50];   // [1..50]
};

struct tINPUT_STR_ENVIRONMENT
{
    uint16_t keystroke;
    uint8_t locate_pos;
    bool insert_mode;
    uint32_t min_num;
    uint32_t max_num;
    std::string cur_str;
    void (*ext_proc)();
};

extern bool use_large_cursor;   // 挿入モードのカーソル形状 (設定値)
extern tINPUT_STR_SETTING is_setting;
extern tINPUT_STR_ENVIRONMENT is_environment;

// s を (x, y) に ln1 文字幅で編集する (最大 ln 文字)。終了キーは is_environment.keystroke に入る。
std::string InputStr(std::string s, uint8_t x, uint8_t y, uint8_t ln, uint8_t ln1, uint8_t atr1, uint8_t atr2);

} // namespace at2
