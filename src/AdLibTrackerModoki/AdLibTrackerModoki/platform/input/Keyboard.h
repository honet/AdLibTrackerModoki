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

// キーコード (typcons1.inc)
constexpr uint16_t kF1 = 0x3b00, kF2 = 0x3c00, kShF2 = 0x5500, kF3 = 0x3d00, kShF3 = 0x5600, kF4 = 0x3e00;
constexpr uint16_t kF5 = 0x3f00, kShF5 = 0x5800, kF6 = 0x4000, kShF6 = 0x5900, kF7 = 0x4100, kF8 = 0x4200;
constexpr uint16_t kShF8 = 0x5b00, kF9 = 0x4300, kShF9 = 0x5c00, kF10 = 0x4400;
constexpr uint16_t kAltF2 = 0x6900, kAltF5 = 0x6c00, kAltF6 = 0x6d00, kAltF8 = 0x6f00, kAltF9 = 0x7000;
constexpr uint16_t kCtrlF2 = 0x5f00, kCtrlF3 = 0x6000, kCtrlF8 = 0x6500, kCtrlF9 = 0x6600;
constexpr uint16_t kLEFT = 0x4b00, kShLEFT = 0x4b34, kCtLEFT = 0x7300, kRIGHT = 0x4d00, kShRGHT = 0x4d36, kCtRGHT = 0x7400;
constexpr uint16_t kUP = 0x4800, kNPUp = 0x4838, kDOWN = 0x5000, kNPDown = 0x5032;
constexpr uint16_t kPgUP = 0x4900, kNPPgUP = 0x4939, kCtPgUP = 0x8400, kPgDOWN = 0x5100, kNPPgDN = 0x5133, kCtPgDN = 0x7600;
constexpr uint16_t kHOME = 0x4700, kNPHOME = 0x4737, kCtHOME = 0x7700, kEND = 0x4f00, kNPEND = 0x4f31, kCtEND = 0x7500;
constexpr uint16_t kINSERT = 0x5200, kNPins = 0x5230, kDELETE = 0x5300, kNPdel = 0x532e;
constexpr uint16_t kENTER = 0x1c0d, kCtENTR = 0x1c0a, kESC = 0x011b, kTAB = 0x0f09, kShTAB = 0x0f00;
constexpr uint16_t kSPACE = 0x3920, kBkSPC = 0x0e08, kCtBkSp = 0x0e7f, kWeird = 0x2960;
constexpr uint16_t kNPastr = 0x372a, kNPplus = 0x4e2b, kNPmins = 0x4a2d, kAstrsk = 0x092a, kCHastr = 0x092a;
constexpr uint16_t kCHplus = 0x0d2b, kCHmins = 0x0c2d, kCHlbrk = 0x1a5b, kCHrbrk = 0x1b5d, kSlash = 0x352f, kSlashR = 0x2b5c;
constexpr uint16_t kAlt1 = 0x7800, kAlt2 = 0x7900, kAlt3 = 0x7a00, kAlt4 = 0x7b00, kAlt5 = 0x7c00;
constexpr uint16_t kAlt6 = 0x7d00, kAlt7 = 0x7e00, kAlt8 = 0x7f00, kAlt9 = 0x8000, kAlt0 = 0x8100;
constexpr uint16_t kAltA = 0x1e00, kAltB = 0x3000, kAltC = 0x2e00, kAltD = 0x2000, kAltE = 0x1200, kAltF = 0x2100;
constexpr uint16_t kAltG = 0x2200, kAltH = 0x2300, kAltI = 0x1700, kAltK = 0x2500, kAltL = 0x2600, kAltM = 0x3200;
constexpr uint16_t kAltN = 0x3100, kAltO = 0x1800, kAltP = 0x1900, kAltQ = 0x1000, kAltR = 0x1300, kAltS = 0x1f00;
constexpr uint16_t kAltT = 0x1400, kAltU = 0x1600, kAltV = 0x2f00, kAltW = 0x1100, kAltX = 0x2d00, kAltZ = 0x2c00;
constexpr uint16_t kCtrlA = 0x1e01, kCtrlB = 0x3002, kCtrlC = 0x2e03, kCtrlD = 0x2004, kCtrlE = 0x1205, kCtrlF = 0x2106;
constexpr uint16_t kCtrlG = 0x2207, kCtrlH = 0x2308, kCtrlI = 0x1709, kCtrlK = 0x250b, kCtrlL = 0x260c, kCtrlM = 0x320d;
constexpr uint16_t kCtrlN = 0x310e, kCtrlO = 0x180f, kCtrlP = 0x1910, kCtrlQ = 0x1011, kCtrlR = 0x1312, kCtrlS = 0x1f13;
constexpr uint16_t kCtrlT = 0x1414, kCtrlV = 0x2f16, kCtrlW = 0x1117, kCtrlX = 0x2d18, kCtrlY = 0x1519, kCtrlZ = 0x2c1a;
constexpr uint16_t kCtLbr = 0x1a1b, kCtRbr = 0x1b1d;

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
