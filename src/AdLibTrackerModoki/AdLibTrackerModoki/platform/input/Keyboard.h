#pragma once

#include <cstdint>

// キーボード入力 (adt2keyb.pas の SDL 版の移植)
//
// getkey() は BIOS 互換のキーコード (スキャンコード << 8 | ASCII) を返す。
// scankey() は AT2 のスキャンコード (SC_*) ごとの押下状態を返す (左右の Alt/Ctrl は 0xFC..0xFF の仮想コード)。
// getkey()/keypressed() はメッセージ処理と画面描画を行いながら入力を待つ (オリジナルと同じブロッキング型)。

constexpr uint8_t SC_ESCAPE = 0x01;
constexpr uint8_t SC_1 = 0x02;
constexpr uint8_t SC_2 = 0x03;
constexpr uint8_t SC_3 = 0x04;
constexpr uint8_t SC_4 = 0x05;
constexpr uint8_t SC_5 = 0x06;
constexpr uint8_t SC_6 = 0x07;
constexpr uint8_t SC_7 = 0x08;
constexpr uint8_t SC_8 = 0x09;
constexpr uint8_t SC_9 = 0x0A;
constexpr uint8_t SC_0 = 0x0B;
constexpr uint8_t SC_MINUS = 0x0C;
constexpr uint8_t SC_EQUALS = 0x0D;
constexpr uint8_t SC_BACKSPACE = 0x0E;
constexpr uint8_t SC_TAB = 0x0F;
constexpr uint8_t SC_Q = 0x10;
constexpr uint8_t SC_W = 0x11;
constexpr uint8_t SC_E = 0x12;
constexpr uint8_t SC_R = 0x13;
constexpr uint8_t SC_T = 0x14;
constexpr uint8_t SC_Y = 0x15;
constexpr uint8_t SC_U = 0x16;
constexpr uint8_t SC_I = 0x17;
constexpr uint8_t SC_O = 0x18;
constexpr uint8_t SC_P = 0x19;
constexpr uint8_t SC_LEFTBRACKET = 0x1A;
constexpr uint8_t SC_RIGHTBRACKET = 0x1B;
constexpr uint8_t SC_ENTER = 0x1C;
constexpr uint8_t SC_A = 0x1E;
constexpr uint8_t SC_S = 0x1F;
constexpr uint8_t SC_D = 0x20;
constexpr uint8_t SC_F = 0x21;
constexpr uint8_t SC_G = 0x22;
constexpr uint8_t SC_H = 0x23;
constexpr uint8_t SC_J = 0x24;
constexpr uint8_t SC_K = 0x25;
constexpr uint8_t SC_L = 0x26;
constexpr uint8_t SC_SEMICOLON = 0x27;
constexpr uint8_t SC_QUOTE = 0x28;
constexpr uint8_t SC_TILDE = 0x29;
constexpr uint8_t SC_LSHIFT = 0x2A;
constexpr uint8_t SC_BACKSLASH = 0x2B;
constexpr uint8_t SC_Z = 0x2C;
constexpr uint8_t SC_X = 0x2D;
constexpr uint8_t SC_C = 0x2E;
constexpr uint8_t SC_V = 0x2F;
constexpr uint8_t SC_B = 0x30;
constexpr uint8_t SC_N = 0x31;
constexpr uint8_t SC_M = 0x32;
constexpr uint8_t SC_COMMA = 0x33;
constexpr uint8_t SC_PERIOD = 0x34;
constexpr uint8_t SC_SLASH = 0x35;
constexpr uint8_t SC_RSHIFT = 0x36;
constexpr uint8_t SC_MULTIPLY = 0x37;
constexpr uint8_t SC_SPACE = 0x39;
constexpr uint8_t SC_CAPSLOCK = 0x3A;
constexpr uint8_t SC_F1 = 0x3B;
constexpr uint8_t SC_F2 = 0x3C;
constexpr uint8_t SC_F3 = 0x3D;
constexpr uint8_t SC_F4 = 0x3E;
constexpr uint8_t SC_F5 = 0x3F;
constexpr uint8_t SC_F6 = 0x40;
constexpr uint8_t SC_F7 = 0x41;
constexpr uint8_t SC_F8 = 0x42;
constexpr uint8_t SC_F9 = 0x43;
constexpr uint8_t SC_F10 = 0x44;
constexpr uint8_t SC_NUMLOCK = 0x45;
constexpr uint8_t SC_SCROLLLOCK = 0x46;
constexpr uint8_t SC_HOME = 0x47;
constexpr uint8_t SC_UP = 0x48;
constexpr uint8_t SC_PAGEUP = 0x49;
constexpr uint8_t SC_MINUS2 = 0x4A;
constexpr uint8_t SC_LEFT = 0x4B;
constexpr uint8_t SC_RIGHT = 0x4D;
constexpr uint8_t SC_PLUS = 0x4E;
constexpr uint8_t SC_END = 0x4F;
constexpr uint8_t SC_DOWN = 0x50;
constexpr uint8_t SC_PAGEDOWN = 0x51;
constexpr uint8_t SC_INSERT = 0x52;
constexpr uint8_t SC_DELETE = 0x53;
constexpr uint8_t SC_F11 = 0x57;
constexpr uint8_t SC_F12 = 0x58;
constexpr uint8_t SC_LWIN = 0x7D;
constexpr uint8_t SC_RWIN = 0x7E;
constexpr uint8_t SC_MENU = 0x7F;
constexpr uint8_t SC_LALT = 0xFC;
constexpr uint8_t SC_RALT = 0xFD;
constexpr uint8_t SC_LCTRL = 0xFE;
constexpr uint8_t SC_RCTRL = 0xFF;

// キーコード (typcons1.inc の一部)
constexpr uint16_t kF1 = 0x3b00, kF2 = 0x3c00, kF3 = 0x3d00, kF4 = 0x3e00, kF5 = 0x3f00;
constexpr uint16_t kF6 = 0x4000, kF7 = 0x4100, kF8 = 0x4200, kF9 = 0x4300, kF10 = 0x4400;
constexpr uint16_t kESC = 0x011b, kENTER = 0x1c0d, kTAB = 0x0f09, kSPACE = 0x3920, kBkSPC = 0x0e08;
constexpr uint16_t kUP = 0x4800, kDOWN = 0x5000, kLEFT = 0x4b00, kRIGHT = 0x4d00;
constexpr uint16_t kPgUP = 0x4900, kPgDOWN = 0x5100, kHOME = 0x4700, kEND = 0x4f00;
constexpr uint16_t kINSERT = 0x5200, kDELETE = 0x5300;
constexpr uint16_t kAlt1 = 0x7800, kAlt2 = 0x7900, kAlt3 = 0x7a00;
constexpr uint16_t kCtrlO = 0x180f, kCtrlE = 0x1205;

// ウィンドウプロシージャから呼ぶ。キーメッセージを処理した場合は true を返す。
bool keyboard_handle_message(unsigned int message, uintptr_t wParam, intptr_t lParam);

void keyboard_init();
void keyboard_reset_buffer();
void keyboard_poll_input();
bool keypressed();
uint16_t getkey();
bool scankey(uint8_t scancode);
bool CapsLock();
bool NumLock();
bool shift_pressed();
bool left_shift_pressed();
bool right_shift_pressed();
bool alt_pressed();
bool ctrl_pressed();
bool ctrl_tab_pressed();
bool LookUpKey(uint16_t key, const uint16_t* table, uint8_t size);
