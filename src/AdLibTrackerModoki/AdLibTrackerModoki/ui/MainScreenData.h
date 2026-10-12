#pragma once

#include <cstdint>

// メイン画面用の固定データ (typcons1.inc / typcons2.inc)
namespace at2
{

extern const char* const patt_win_tracks[3][5];
extern const char* const patt_tab_str[5];
extern const char* const note_layout[12 * 8 + 2];
extern const char fx_digits[48 + 1];

// 1..22 (オーダー窓の列位置)
constexpr uint8_t pos1[23] = { 0, 7, 14, 21, 28, 35, 42, 49, 56, 63, 70, 77, 84, 91, 98, 105, 112, 119, 126, 133, 140, 147, 154 };

// 1..44 (パターン窓のトラック内の列位置: ノート / 楽器 / エフェクト1 / エフェクト2)
constexpr uint8_t pos3[45] = { 0,
    0, 4, 7, 11,  15, 19, 22, 26,  30, 34, 37, 41,  45, 49, 52, 56,  60, 64, 67, 71,  75, 79, 82, 86,
    90, 94, 97, 101,  105, 109, 112, 116,  120, 124, 127, 131,  135, 139, 142, 146,  150, 154, 157, 161 };

// 配色 (typcons1.inc の既定値)
constexpr uint8_t analyzer_bckg = 0x10;
constexpr uint8_t analyzer = 0x03;
constexpr uint8_t analyzer_ovrllvol = 0x03;
constexpr uint8_t analyzer_volumelvl = 0x07;
constexpr uint8_t analyzer_modulator = 0x02;
constexpr uint8_t analyzer_carrier = 0x03;
constexpr uint8_t pattern_bckg = 0x10;
constexpr uint8_t pattern_border = 0x0f;
constexpr uint8_t pattern_pos_indic = 0x0b;
constexpr uint8_t pattern_pan_indic = 0x03;
constexpr uint8_t pattern_gpan_indic = 0x0f;
constexpr uint8_t pattern_lock_indic = 0x07;
constexpr uint8_t pattern_4op_indic = 0x0e;
constexpr uint8_t pattern_perc_indic = 0x0f;
constexpr uint8_t pattern_chan_indic = 0x0e;
constexpr uint8_t pattern_row_bckg = 0x50;
constexpr uint8_t pattern_row_bckg_p = 0xc0;
constexpr uint8_t pattern_row_bckg_m = 0xb0;
constexpr uint8_t pattern_block_bckg = 0x20;
constexpr uint8_t pattern_line = 0x03;
constexpr uint8_t pattern_line_p = 0x00;
constexpr uint8_t pattern_line_m = 0x0b;
constexpr uint8_t pattern_hi_line = 0x0b;
constexpr uint8_t pattern_hi_line_m = 0x03;
constexpr uint8_t pattern_note = 0x07;
constexpr uint8_t pattern_hi_note = 0x0f;
constexpr uint8_t pattern_note0 = 0x03;
constexpr uint8_t pattern_hi_note0 = 0x0c;
constexpr uint8_t pattern_note_hid = 0x09;
constexpr uint8_t pattern_hi_note_h = 0x07;
constexpr uint8_t pattern_inst = 0x02;
constexpr uint8_t pattern_hi_inst = 0x0a;
constexpr uint8_t pattern_inst0 = 0x02;
constexpr uint8_t pattern_hi_inst0 = 0x0a;
constexpr uint8_t pattern_cmnd = 0x07;
constexpr uint8_t pattern_hi_cmnd = 0x0f;
constexpr uint8_t pattern_cmnd0 = 0x03;
constexpr uint8_t pattern_hi_cmnd0 = 0x0b;
constexpr uint8_t pattern_note_m = 0x09;
constexpr uint8_t pattern_note0_m = 0x09;
constexpr uint8_t pattern_note_hid_m = 0x0d;
constexpr uint8_t pattern_inst_m = 0x02;
constexpr uint8_t pattern_inst0_m = 0x02;
constexpr uint8_t pattern_cmnd_m = 0x09;
constexpr uint8_t pattern_cmnd0_m = 0x09;
constexpr uint8_t pattern_note_b = 0x0f;
constexpr uint8_t pattern_note0_b = 0x0b;
constexpr uint8_t pattern_inst_b = 0x0a;
constexpr uint8_t pattern_inst0_b = 0x0a;
constexpr uint8_t pattern_cmnd_b = 0x0f;
constexpr uint8_t pattern_cmnd0_b = 0x0b;
constexpr uint8_t pattern_fix_note_b = 0x0e;
constexpr uint8_t pattern_fix_note = 0x0b;
constexpr uint8_t pattern_hi_fx_note = 0x0e;
constexpr uint8_t pattern_fix_note_m = 0x0e;
constexpr uint8_t pattern_cmnd_ctrl = 0x0d;
constexpr uint8_t main_background = 0x10;
constexpr uint8_t main_title = 0x0b;
constexpr uint8_t main_border = 0x0b;
constexpr uint8_t main_stat_line = 0x03;
constexpr uint8_t main_hi_stat_line = 0x0b;
constexpr uint8_t main_dis_stat_line = 0x05;
constexpr uint8_t main_behavior = 0x0d;
constexpr uint8_t main_behavior_dis = 0x05;
constexpr uint8_t status_background = 0x10;
constexpr uint8_t status_border = 0x0e;
constexpr uint8_t status_static_txt = 0x0b;
constexpr uint8_t status_dynamic_txt = 0x0f;
constexpr uint8_t status_play_state = 0x07;
constexpr uint8_t status_text_dis = 0x05;
constexpr uint8_t order_background = 0x10;
constexpr uint8_t order_hi_bckg = 0x50;
constexpr uint8_t order_border = 0x0a;
constexpr uint8_t order_entry = 0x03;
constexpr uint8_t order_hi_entry = 0x0b;
constexpr uint8_t order_pattn = 0x07;
constexpr uint8_t order_pattn_jump = 0x02;
constexpr uint8_t order_hi_pattn = 0x0f;
constexpr uint8_t order_played_b = 0xc0;
constexpr uint8_t order_played = 0x00;
constexpr uint8_t scrollbar_bckg = 0x30;
constexpr uint8_t scrollbar_text = 0x00;
constexpr uint8_t scrollbar_mark = 0x0b;
constexpr uint8_t scrollbar_2nd_mark = 0x0f;

} // namespace at2
