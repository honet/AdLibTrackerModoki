#pragma once

#include <cstdint>
#include <string>

// ダイアログ (dialogio.pas の Dialog の移植)
//
// 文字列は CP437 のバイト列。text / keys は '$' 区切り、keys の '~X~' はショートカットキーを表す
// ('~' のない項目は選べない)。
// 文字列入力 ('%string_input%') と Menu / Fselect は、使う機能の実装とともに追加する。
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

} // namespace at2
