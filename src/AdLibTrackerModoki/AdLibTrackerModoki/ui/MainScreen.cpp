#include "MainScreen.h"

#include <commdlg.h>
#include <timeapi.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cwchar>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "DialogIO.h"
#include "MainScreenData.h"
#include "StringIO.h"
#include "TxtScrIO.h"
#include "../Resource.h"
#include "../app/Sound.h"
#include "../core/player/A2Player.h"
#include "../core/song/EffectDefs.h"
#include "../core/song/ModuleLoader.h"
#include "../core/song/ModuleSaver.h"
#include "../platform/System.h"
#include "../platform/input/Keyboard.h"

namespace at2
{

namespace
{

using PlayStatus = A2Player::PlayStatus;

constexpr uint8_t BYTE_NULL = 0xff;
constexpr uint16_t WORD_NULL = 0xffff;
constexpr uint8_t fixed_note_flag = 0x90;

// tPLAY_STATUS の並び (isPlaying, isPaused, isStopped)
constexpr int isPlaying = 0;
constexpr int isPaused = 1;
constexpr int isStopped = 2;

const std::string _ADT2_TITLE_STRING_ = "/\xB4" "DLiB TR/\xB4" "CK3R ][ Modoki";

HWND g_hwnd = nullptr;

// --- 設定 (adtrack2.ini の既定値) ---
uint8_t command_typing = 1;
bool cycle_pattern = true;
bool linefeed = true;
bool midiboard = true;
bool highlight_controls = true;
bool scroll_bars = true;
uint8_t pattern_layout = 0;
bool trace_by_default = true;
bool nosync_by_default = false;
double decay_bar_rise = 3.0;
double decay_bar_fall = 0.3;

// --- 画面・編集の状態 (adt2unit.pas / adt2sys.pas / typcons1.inc のグローバル変数) ---
// 編集機能は未実装のため、マーク・ノート記録などの状態は既定値のまま
bool tracing = false;
bool track_notes = false;
bool marking = false;
bool mark_lines = false;
bool jump_mark_mode = false;
bool no_status_refresh = false;
bool really_no_status_refresh = false;
bool do_synchronize = false;
bool force_scrollbars = false;
bool no_trace_pattord = false;
bool module_archived = false;
bool _realtime_gfx_no_update = false;

std::wstring songdata_source;   // 読み込んだ (保存した) 曲のパス

uint8_t current_octave = 4;
uint8_t current_inst = 1;
uint8_t instrum_page = 1;
uint8_t chan_pos = 1;
uint8_t pattord_page = 0, pattord_hpos = 1, pattord_vpos = 1;
uint8_t pattern_patt = 0, pattern_page = 0, pattern_hpos = 1;
uint8_t _pattedit_lastpos = 0;
const char* const* patt_win = patt_win_tracks[0];   // patt_win[1..5] は patt_win[0..4]

bool blink_flag = false;
int32_t blink_ticks = 0;
bool _NRECM_blink_flag = false;
int32_t _NRECM_blink_ticks = 0;
bool _IRQFREQ_blink_flag = false;
int32_t _IRQFREQ_blink_ticks = 0;
bool _IRQFREQ_update_event = false;

uint8_t _traceprc_last_order = 0;
uint8_t _traceprc_last_pattern = 0;
uint8_t _traceprc_last_line = 0;

int8_t _name_scrl_shift_ctr = 1;
uint8_t _name_scrl_shift = 0;
int32_t _name_scrl_pending_frames = 0;

// status_layout[tPLAY_STATUS] (Pascal の 1 始まりの添字 n は [n - 1])
std::string status_layout[3] = { "PLAYiNG   ", "PAUSED    ", "STOPPED   " };

DWORD g_timer_last = 0;

inline uint8_t B(int value) { return static_cast<uint8_t>(value); }
inline uint8_t B(double value) { return static_cast<uint8_t>(static_cast<int>(value)); }   // Round の結果用

// ---------------------------------------------------------------------------
// プレイヤーへのアクセス
// ---------------------------------------------------------------------------

// オーディオスレッドとの排他。Pascal 版ではタイマー割り込みと画面処理が同じ変数を触るため、
// 画面処理からプレイヤーの状態を参照・変更する間はこのロックを取る。
// 入れ子で取れるようにしている (draw_screen を呼ぶ間は取らないこと)。
int g_lock_depth = 0;
std::mutex* g_locked_mutex = nullptr;

struct PlayerLock
{
    PlayerLock()
    {
        if (g_lock_depth++ == 0) {
            g_locked_mutex = &sound_mutex();
            g_locked_mutex->lock();
        }
    }
    ~PlayerLock()
    {
        if (--g_lock_depth == 0) {
            g_locked_mutex->unlock();
            g_locked_mutex = nullptr;
        }
    }
    PlayerLock(const PlayerLock&) = delete;
    PlayerLock& operator=(const PlayerLock&) = delete;
};

// PlayerLock を取っている間に、ダイアログなど draw_screen を呼ぶ処理を行うため一時的にロックを外す
struct PlayerUnlock
{
    int saved_depth;
    PlayerUnlock() : saved_depth(g_lock_depth)
    {
        if (saved_depth > 0) {
            g_lock_depth = 0;
            g_locked_mutex->unlock();
            g_locked_mutex = nullptr;
        }
    }
    ~PlayerUnlock()
    {
        if (saved_depth > 0) {
            g_locked_mutex = &sound_mutex();
            g_locked_mutex->lock();
            g_lock_depth = saved_depth;
        }
    }
    PlayerUnlock(const PlayerUnlock&) = delete;
    PlayerUnlock& operator=(const PlayerUnlock&) = delete;
};

A2Player& P() { return *sound_player(); }
FixedSongData& songdata() { return P().songdata(); }
int play_status() { return static_cast<int>(P().play_status); }

bool in_4op_tracks_hi(uint8_t chan) { return chan == 1 || chan == 3 || chan == 5 || chan == 10 || chan == 12 || chan == 14; }
bool in_4op_tracks_lo(uint8_t chan) { return chan == 2 || chan == 4 || chan == 6 || chan == 11 || chan == 13 || chan == 15; }

// songdata.flag_4op のうちチャンネルの組に対応するビット (該当しないチャンネルは 0)
uint8_t flag_4op_bit(uint8_t chan)
{
    switch (chan) {
    case 1: case 2: return 0x01;
    case 3: case 4: return 0x02;
    case 5: case 6: return 0x04;
    case 10: case 11: return 0x08;
    case 12: case 13: return 0x10;
    case 14: case 15: return 0x20;
    default: return 0;
    }
}

// get_chunk (SDL 版は範囲外のとき chunk を変更しない)
void get_chunk(uint8_t pattern, uint8_t line, uint8_t channel, Chunk& chunk)
{
    if (pattern < 128 && channel >= 1 && channel <= 20) chunk = P().song()->chunk(pattern, channel, line);
}

std::string to_cp437(const std::wstring& w)
{
    if (w.empty()) return std::string();
    const int n = WideCharToMultiByte(437, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, "_", nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(437, 0, w.data(), static_cast<int>(w.size()), s.data(), n, "_", nullptr);
    return s;
}

// songdata_title (ファイル名。未読み込みなら 'noname.')
std::string songdata_title()
{
    const std::wstring& title = P().song()->title;
    return title.empty() ? std::string("noname.") : to_cp437(title);
}

// FilterStr2(str, _valid_characters_fname, '_')
std::string filter_fname(std::string str)
{
    for (char& c : str) {
        const uint8_t b = static_cast<uint8_t>(c);
        if (!(b == 0 || (b >= 32 && b <= 127) || b == 255)) c = '_';
    }
    return str;
}

// FSplit による名前部分と拡張子部分
std::string BaseNameOnly(const std::string& path)
{
    const size_t slash = path.find_last_of("\\/:");
    std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
    const size_t dot = name.find_last_of('.');
    return dot == std::string::npos ? name : name.substr(0, dot);
}

std::string ExtOnly(const std::string& path)
{
    const size_t slash = path.find_last_of("\\/:");
    const std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
    const size_t dot = name.find_last_of('.');
    if (dot == std::string::npos) return std::string();
    std::string ext = name.substr(dot + 1);
    for (char& c : ext) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return ext;
}

// ---------------------------------------------------------------------------
// 位置計算 (adt2unit.pas / ipattern.inc)
// ---------------------------------------------------------------------------

uint8_t count_channel(uint8_t hpos)
{
    const uint8_t n = B(_pattedit_lastpos / MAX_TRACKS);
    if (hpos % n == 0) return B(hpos / n + chan_pos - 1);
    return B(hpos / n + chan_pos);
}

uint8_t count_pos(uint8_t hpos)
{
    const uint8_t n = B(_pattedit_lastpos / MAX_TRACKS);
    if (n == 0) return 0;
    if (hpos % n == 0) return B(n - 1);
    return B(hpos % n - 1);
}

void count_order(uint8_t& entries)
{
    const A2Player& p = P();
    uint8_t index = 0;
    uint8_t index2 = 0;
    // オリジナルは 2 つのジャンプが互いを指していると無限ループになるため、回数の上限を設ける
    for (int guard = 0; guard < 0x10000; ++guard) {
        const uint8_t order = const_cast<A2Player&>(p).order_at(index);
        if (order != 0x80) {
            if (order > 0x80) {
                if (order - 0x80 != index2) {
                    index = B(order - 0x80);
                    index2 = index;
                } else {
                    break;
                }
            }
        } else {
            break;
        }
        if (index < 0x80) ++index;
        if (index > 0x7f) break;
    }
    entries = index;
}

uint8_t last_chan_pos()
{
    if (songdata().nm_tracks > MAX_TRACKS) return B(std::min(16, songdata().nm_tracks - MAX_TRACKS + 1));
    return 1;
}

uint8_t last_hpos()
{
    return B(std::min<int>(_pattedit_lastpos, songdata().nm_tracks * (_pattedit_lastpos / MAX_TRACKS)));
}

// chan_pos / hpos が表示できる範囲に収まるよう戻す (PATTERN_page_refresh / PATTERN_edit の先頭)
void normalize_track_position(uint8_t& hpos)
{
    const uint8_t n = B(_pattedit_lastpos / MAX_TRACKS);
    for (int guard = 0; guard < 1000 && !(chan_pos <= last_chan_pos() && hpos <= last_hpos()); ++guard) {
        uint8_t temp;
        if (hpos > n) {
            hpos = B(hpos - n);
            temp = hpos % n > 0 ? B(hpos % n - 1) : B(n - 1);
            hpos = B(hpos - temp);
        } else if (chan_pos > 1) {
            --chan_pos;
            temp = hpos % n > 0 ? B(hpos % n - 1) : B(n - 1);
            hpos = B(hpos - temp);
        } else if (cycle_pattern) {
            chan_pos = last_chan_pos();
            hpos = last_hpos();
        }
    }
}

// pos4 (typcons1.inc): コマンド入力ありのレイアウトでのトラック内の桁位置 (1..110)
uint8_t pos4(uint8_t hpos)
{
    static const uint8_t col[10] = { 0, 2, 4, 5, 7, 8, 9, 11, 12, 13 };
    const int i = hpos - 1;
    return B(i / 10 * 15 + col[i % 10]);
}

// ---------------------------------------------------------------------------
// スクロールバー (adt2unit.pas)
// ---------------------------------------------------------------------------

uint8_t _partial(uint8_t max, uint8_t val, uint8_t base)
{
    const double temp1 = static_cast<double>(max) / base;
    double temp2 = static_cast<double>(max) / base / 2;
    uint8_t temp3 = 0;
    while (temp2 < val) {
        temp2 = temp2 + temp1;
        ++temp3;
    }
    return temp3;
}

uint8_t hscroll_bar(uint8_t x, uint8_t y, uint8_t size, uint16_t len1, uint16_t len2, uint16_t pos, uint8_t atr1, uint8_t atr2)
{
    if (size > work_maxcol - x) size = B(work_maxcol - x);
    if (size < 5) size = 5;

    uint8_t temp;
    if (size - 2 - 1 < 10) temp = _partial(B(len1), B(len2), B(size - 2 - 1));
    else temp = _partial(B(len1), B(len2), B(size - 2 - 1 - 2));

    if (pos == temp && !force_scrollbars) return temp;

    if (size < len1 * 4 && len1 > 4) {
        pos = temp;
        show_str(x, y, "\x11" + ExpStrL("", B(size - 2), '\xB0') + "\x10", atr1);
        if (size - 2 - 1 < 10) show_str(B(x + 1 + temp), y, "\xB2", atr2);
        else show_str(B(x + 1 + temp), y, "\xB2\xB2\xB2", atr2);
    } else {
        show_str(x, y, "\x11" + ExpStrL("", B(size - 2), '\xB1') + "\x10", atr1);
    }
    return B(pos);
}

uint8_t vscroll_bar(uint8_t x, uint8_t y, uint8_t size, uint16_t len1, uint16_t len2, uint16_t pos, uint8_t atr1, uint8_t atr2)
{
    if (size > work_maxln - y) size = B(work_maxln - y);
    if (size < 5) size = 5;

    uint8_t temp;
    if (size - 2 - 1 < 10) temp = _partial(B(len1), B(len2), B(size - 2 - 1));
    else temp = _partial(B(len1), B(len2), B(size - 2 - 1 - 2));

    if (pos == temp && !force_scrollbars) return temp;

    if (size < len1 * 4 && len1 > 5) {
        pos = temp;
        show_vstr(x, y, "\x1E" + ExpStrL("", B(size - 2), '\xB0') + "\x1F", atr1);
        if (size - 2 - 1 < 10) show_str(x, B(y + 1 + temp), "\xB2", atr2);
        else show_vstr(x, B(y + 1 + temp), "\xB2\xB2\xB2", atr2);
    } else {
        show_vstr(x, y, "\x1E" + ExpStrL("", B(size - 2), '\xB1') + "\x1F", atr1);
    }
    return B(pos);
}

// ---------------------------------------------------------------------------
// 画面の初期化 (adt2ext2.pas の PROGRAM_SCREEN_init)
// ---------------------------------------------------------------------------

// 画面モードに応じた値を設定する (txtscrio.pas の TxtScrIO_Init の後半)
void init_screen_layout()
{
    if (command_typing == 0) _pattedit_lastpos = B(4 * MAX_TRACKS);
    else _pattedit_lastpos = B(10 * MAX_TRACKS);

    int temp;
    switch (MAX_COLUMNS) {
    case 120: temp = 1; break;
    case 180: temp = 2; break;
    default: temp = 0; break;
    }
    patt_win = patt_win_tracks[temp];

    // 画面を作り直したのでスクロールバーを描き直させる
    scroll_pos0 = scroll_pos1 = scroll_pos2 = scroll_pos3 = scroll_pos4 = BYTE_NULL;
}

void PROGRAM_SCREEN_init()
{
    fr_setting.shadow_enabled = false;
    Frame(screen_ptr, 1, B(MAX_PATTERN_ROWS + 12), MAX_COLUMNS, B(MAX_PATTERN_ROWS + 22),
          main_background + main_border, "",
          main_background + main_title,
          frame_double);
    Frame(screen_ptr, 1, 1, MAX_COLUMNS, B(MAX_PATTERN_ROWS + 12),
          main_background + main_border, "- " + _ADT2_TITLE_STRING_ + " -",
          main_background + main_border,
          frame_single);
    Frame(screen_ptr, 2, 2, 24, 7,
          status_background + status_border, " STATUS ",
          status_background + status_border,
          frame_double);
    Frame(screen_ptr, 25, 2, B(25 + MAX_ORDER_COLS * 7 - 1 + PATTORD_xshift * 2), 7,
          order_background + order_border, " PATTERN ORDER (  ) ",
          order_background + order_border,
          frame_double);

    fr_setting.shadow_enabled = true;
    area_x1 = 0;
    area_y1 = 0;
    area_x2 = 0;
    area_y2 = 0;

    const uint8_t y0 = B(MAX_PATTERN_ROWS + 13);
    ShowVStr(screen_ptr, 2, y0, ExpStrL("", 9, ' '), analyzer_bckg + analyzer);
    ShowVStr(screen_ptr, 3, y0, "MAX   MiN", analyzer_bckg + analyzer);

    for (int temp = 5; temp <= MAX_COLUMNS - 6; ++temp) {
        ShowVStr(screen_ptr, B(temp), y0, "\xF2\xE0\xE0\xE0\xE0\xE0\xE0\xE0\xF3", analyzer_bckg + analyzer);
    }

    ShowVStr(screen_ptr, 4, y0, ExpStrL("", 9, ' '), analyzer_bckg + analyzer);
    for (int x = MAX_COLUMNS - 5; x <= MAX_COLUMNS - 1; ++x) {
        ShowVStr(screen_ptr, B(x), y0, ExpStrL("", 9, ' '), analyzer_bckg + analyzer);
    }

    const uint8_t xdb = B(MAX_COLUMNS - 4);
    ShowStr(screen_ptr, xdb, B(MAX_PATTERN_ROWS + 13), "dB", analyzer_bckg + analyzer);
    ShowStr(screen_ptr, xdb, B(MAX_PATTERN_ROWS + 14), "\xE0" "47", analyzer_bckg + analyzer);
    ShowStr(screen_ptr, xdb, B(MAX_PATTERN_ROWS + 15), "\xE0", analyzer_bckg + analyzer);
    ShowStr(screen_ptr, xdb, B(MAX_PATTERN_ROWS + 16), "\xE0" "23", analyzer_bckg + analyzer);
    ShowStr(screen_ptr, xdb, B(MAX_PATTERN_ROWS + 17), "\xE0", analyzer_bckg + analyzer);
    ShowStr(screen_ptr, xdb, B(MAX_PATTERN_ROWS + 18), "\xE0" "12", analyzer_bckg + analyzer);
    ShowStr(screen_ptr, xdb, B(MAX_PATTERN_ROWS + 19), "\xE0" "4", analyzer_bckg + analyzer);
    ShowStr(screen_ptr, xdb, B(MAX_PATTERN_ROWS + 20), "\xE0" "2", analyzer_bckg + analyzer);

    ShowCStr(screen_ptr, 3, 3, "~ORDER/PATTERN ~  /",
             status_background + status_dynamic_txt,
             status_background + status_static_txt);
    ShowCStr(screen_ptr, 3, 4, "~ROW           ~",
             status_background + status_dynamic_txt,
             status_background + status_static_txt);
    ShowCStr(screen_ptr, 3, 5, "~SPEED/TEMPO   ~  /",
             status_background + status_dynamic_txt,
             status_background + status_static_txt);

    ShowStr(screen_ptr, 2, 8, patt_win[0], pattern_bckg + pattern_border);
    ShowStr(screen_ptr, 2, 9, patt_win[1], pattern_bckg + pattern_border);
    ShowStr(screen_ptr, 2, 10, patt_win[2], pattern_bckg + pattern_border);
    for (int temp = 11; temp <= 11 + MAX_PATTERN_ROWS - 1; ++temp) {
        ShowStr(screen_ptr, 2, B(temp), patt_win[3], pattern_bckg + pattern_border);
    }
    ShowStr(screen_ptr, 2, B(11 + MAX_PATTERN_ROWS), patt_win[4], pattern_bckg + pattern_border);
}

// ---------------------------------------------------------------------------
// アナライザ (realtime.inc)
// ---------------------------------------------------------------------------

std::string _decay_bar_str(uint8_t value)
{
    std::string result;
    do {
        if (value > 15) {
            result += '\xDB';
            value = B(value - 15);
        }
        if (value <= 15 && value != 0) result += static_cast<char>(224 + value);
    } while (!(value <= 15));
    return FlipStr(result);
}

void show_bar(uint8_t chan, uint8_t level, uint8_t xpos, uint8_t from_pos, uint8_t to_pos, uint8_t attr)
{
    for (int temp = from_pos; temp <= to_pos; ++temp) {
        show_vcstr(B(xpos + temp - 1 + (chan - 1) * 15), B(MAX_PATTERN_ROWS + 14),
                   ExpStrL("~" + _decay_bar_str(B(std::nearbyint(105.0 / 63 * level))) + "~", 9, '\xE0'),
                   analyzer_bckg + analyzer,
                   B(analyzer_bckg + (attr >> 4)));
    }
}

void decay_bars_refresh()
{
    A2Player& p = P();
    show_bar(1, p.overall_volume, 5, 1, 2, analyzer_ovrllvol + (analyzer_ovrllvol << 4));
    show_bar(1, p.global_volume, B(MAX_COLUMNS - 7), 1, 2, analyzer_volumelvl + (analyzer_volumelvl << 4));

    const double speedup_factor = 2.5 * (100.0 / sdl_frame_rate);
    const double speedown_factor = 2.5 * (100.0 / sdl_frame_rate);

    uint8_t chan = chan_pos;
    for (int temp = 1; temp <= MAX_TRACKS; ++temp) {
        A2Player::DecayBar& db = p.decay_bar[chan];
        A2Player::VolumBar& vb = p.volum_bar[chan];

        if (db.dir1 == 1) db.lvl1 = db.lvl1 + db.dir1 * (decay_bar_rise * speedup_factor);
        else db.lvl1 = db.lvl1 + db.dir1 * (decay_bar_fall * speedown_factor);

        if (db.dir2 == 1) db.lvl2 = db.lvl2 + db.dir2 * (decay_bar_rise * speedup_factor);
        else db.lvl2 = db.lvl2 + db.dir2 * (decay_bar_fall * speedown_factor);

        if (db.lvl1 < 0) db.lvl1 = 0;
        if (db.lvl1 > 63) db.lvl1 = 63;
        if (db.lvl1 >= p.carrier_vol[chan]) db.dir1 = -1;

        if (db.lvl2 < 0) db.lvl2 = 0;
        if (db.lvl2 > 63) db.lvl2 = 63;
        if (db.lvl2 >= p.modulator_vol[chan]) db.dir2 = -1;

        if (vb.dir == 1) vb.lvl = vb.lvl + vb.dir * (decay_bar_rise * speedup_factor);
        else vb.lvl = vb.lvl + vb.dir * (decay_bar_fall * speedown_factor);

        if (vb.lvl < 0) vb.lvl = 0;
        if (vb.lvl > 63) vb.lvl = 63;
        const bool perc = p.percussion_mode && chan >= 17 && chan <= 20;
        const bool am = (p.ins_parameter(p.voice_table[chan], 10) & 1) == 1;
        if (!perc) {
            if (am) {
                if (vb.lvl >= (p.carrier_vol[chan] + p.modulator_vol[chan]) / 2) vb.dir = -1;
            } else if (vb.lvl >= p.carrier_vol[chan]) {
                vb.dir = -1;
            }
        } else if (vb.lvl >= p.modulator_vol[chan]) {
            vb.dir = -1;
        }

        const uint8_t lvl1 = B(std::nearbyint(db.lvl1));
        const uint8_t lvl2 = B(std::nearbyint(db.lvl2));
        if (am || perc) {
            show_bar(B(temp), lvl2, 8, 1, 6, analyzer + (analyzer_modulator << 4));
        } else {
            show_bar(B(temp), B(63 - A2Player::scale_volume(B(63 - lvl2), B(63 - lvl1))), 8, 1, 6,
                     analyzer + (analyzer_modulator << 4));
        }

        if (!perc) show_bar(B(temp), lvl1, 8, 8, 14, analyzer + (analyzer_carrier << 4));
        else show_bar(B(temp), 0, 8, 8, 14, analyzer + (analyzer_carrier << 4));
        ++chan;
    }
}

// ---------------------------------------------------------------------------
// STATUS 窓・トラック表示 (realtime.inc の status_refresh)
// ---------------------------------------------------------------------------

std::string _vol_bar_str(uint8_t value)
{
    std::string result;
    do {
        if (value > 3) {
            result += '\xA8';
            value = B(value - 3);
        }
        if (value <= 3 && value != 0) result += static_cast<char>(164 + value);
    } while (!(value <= 3));
    return result;
}

std::string volslide_str(uint8_t type, const std::string& previous)
{
    switch (type) {
    case 0: return "   ";
    case 1: return "C\x0A ";
    case 2: return "M\x0A ";
    case 3: return "&\x0A ";
    default: return previous;
    }
}

// チャンネルが鳴っていない (または停止中) ときのトラック見出しの表示
void show_idle_track_indicator(int temp2, uint8_t chan)
{
    A2Player& p = P();
    const uint8_t x = B(12 + (temp2 - 1) * 15);
    if (!p.percussion_mode && chan >= 16 && chan <= 20) {
        show_str(x, 9, "\xFA\xFA " + ExpStrL("", 7, ' '), pattern_bckg + pattern_pan_indic);
        return;
    }
    const uint8_t bit = flag_4op_bit(chan);
    if (bit != 0) {
        if ((songdata().flag_4op & bit) == bit) {
            show_cstr(x, 9, "\xFA\xFA ~" + ExpStrR("\x04\x03", 7, ' ') + "~",
                      pattern_bckg + pattern_pan_indic, pattern_bckg + pattern_4op_indic);
        } else {
            show_cstr(x, 9, "\xFA\xFA ~" + ExpStrR("", 7, ' ') + "~",
                      pattern_bckg + pattern_pan_indic, pattern_bckg + pattern_4op_indic);
        }
        return;
    }
    static const char* const perc_names[5] = { "BD", "SD", "TT", "TC", "HH" };
    if (chan >= 16 && chan <= 20) {
        show_cstr(x, 9, "\xFA\xFA ~" + ExpStrR(perc_names[chan - 16], 7, ' ') + "~",
                  pattern_bckg + pattern_pan_indic, pattern_bckg + pattern_perc_indic);
        return;
    }
    show_str(x, 9, "\xFA\xFA " + ExpStrL("", 7, ' '), pattern_bckg + pattern_pan_indic);
}

void status_refresh()
{
    if (really_no_status_refresh) return;
    A2Player& p = P();
    FixedSongData& sd = songdata();
    const int ps = play_status();

    show_str(21, 6, std::string(1, status_layout[ps][7]) + status_layout[ps][8],
             status_background + status_play_state);

    if (no_status_refresh) {
        uint8_t chan = chan_pos;
        for (int temp2 = 1; temp2 <= MAX_TRACKS; ++temp2) {
            if (p.channel_flag[chan]) {
                show_str(B(8 + (temp2 - 1) * 15), B(MAX_PATTERN_ROWS + 11), ExpStrR("", 14, '\xCD'),
                         pattern_bckg + pattern_border);
            }
            ++chan;
        }
        return;
    }

    std::string temps = status_layout[ps];
    show_str(14, 6, ExpStrR(temps, 10, ' '), status_background + status_play_state);
    if (!(area_x1 + area_y1 == 0) && area_x1 <= 20 && area_y1 <= 6) {
        show_str(2, 1, " " + ExpStrR(temps, 10, ' '), main_background + main_border);
    } else {
        show_str(2, 1, ExpStrR("", 12, '\xC4'), main_background + main_border);
    }

    if (p.single_play) return;

    if (p.play_single_patt) {
        show_str(20, 3, byte2hex(p.current_pattern) + "  ", pattern_bckg + status_dynamic_txt);
    } else {
        uint8_t temp = p.order_at(p.current_order);
        if (!(temp <= 0x7f)) temp = 0;
        if (!p.replay_forbidden && p.repeat_pattern && !p.calibrating) {
            show_str(20, 3, byte2hex(temp) + " \x13", pattern_bckg + status_dynamic_txt);
        } else if (!p.calibrating) {
            show_str(20, 3, byte2hex(temp) + "  ", status_background + status_dynamic_txt);
        } else {
            show_str(20, 3, byte2hex(temp), status_background + status_dynamic_txt);
        }
    }

    if (!p.play_single_patt) show_str(17, 3, byte2hex(p.current_order), status_background + status_dynamic_txt);
    else show_str(17, 3, "--", status_background + status_dynamic_txt);

    show_str(17, 4, byte2hex(p.current_line), status_background + status_dynamic_txt);

    // _IRQFREQ_update_event (Ctrl+↑↓ によるタイマー周波数の調整) は未実装のため常に SPEED/TEMPO を表示する
    if (!_IRQFREQ_update_event) {
        show_str(3, 5, "SPEED/TEMPO     /", status_background + status_static_txt);
        show_str(17, 5, byte2hex(p.speed), status_background + status_dynamic_txt);
        if (p.tempo < 100) {
            if (p.tempo == 18 && p.timer_fix) {
                show_str(20, 5, byte2dec(p.tempo) + "\x05\xAE", status_background + status_dynamic_txt);
            } else {
                show_str(20, 5, byte2dec(p.tempo) + "\xAE ", status_background + status_dynamic_txt);
            }
        } else {
            show_str(20, 5, byte2dec(p.tempo) + "\xAE", status_background + status_dynamic_txt);
        }
    }

    if (ps != isStopped) {
        temps = " ~" + byte2dec(B(std::min(p.song_timer / 60, 99))) + ":" +
                byte2dec(B(std::min(p.song_timer % 60, 99))) + "." +
                static_cast<char>(48 + std::min(p.song_timer_tenths / 10, 9)) + "~ ";
    } else {
        temps = " 00~:~00~.~0 ";
    }

    uint8_t playtime_attr1[2], playtime_attr2[2], playtime_attr3[2];
    playtime_attr1[0] = status_background + status_text_dis;
    playtime_attr1[1] = B((status_border << 4) & 0xf0);
    playtime_attr2[0] = status_background + status_border;
    playtime_attr2[1] = status_background + status_border;
    playtime_attr3[0] = main_background + main_border;
    playtime_attr3[1] = B((main_border << 4) & 0xf0);

    if (_IRQFREQ_update_event) {
        const int shift = p.IRQ_freq_shift + p.playback_speed_shift;
        if (shift > 0) temps = "+" + Num2str(shift, 10) + "\xAE";
        else if (shift < 0) temps = "-" + Num2str(std::abs(shift), 10) + "\xAE";
        else temps = Num2str(p.IRQ_freq + shift, 10) + "\xAE";
        temps = "~ ~ " + CenterStr(temps, 5) + " ~ ~";
    }

    const int blink = (_IRQFREQ_blink_flag && _IRQFREQ_update_event) ? 1 : 0;
    show_cstr(13, 7, temps, playtime_attr1[blink], playtime_attr2[blink]);

    if (!(area_x1 + area_y1 == 0) && area_x1 <= 20 && area_y1 <= 6) {
        show_cstr(16, 1, temps, playtime_attr3[blink], main_background + main_border);
    } else {
        show_str(16, 1, ExpStrR("", 11, '\xC4'), main_background + main_border);
    }

    if (!_IRQFREQ_blink_flag && _IRQFREQ_update_event) _IRQFREQ_update_event = false;

    const std::string title = songdata_title();
    temps = BaseNameOnly(filter_fname(title));

    while (8 + _name_scrl_shift > static_cast<int>(temps.size()) && _name_scrl_shift > 0) --_name_scrl_shift;

    show_str(3, 6, "[" + ExpStrR(iCASE(Copy(temps, 1 + _name_scrl_shift, 8)), 8, ' ') + "]",
             status_background + status_dynamic_txt);
    show_str(3, 7, "[" + ExpStrR(iCASE(Copy(ExtOnly(title), 1, 3)), 3, ' ') + "]",
             status_background + status_border);

    if (_name_scrl_pending_frames != 0) {
        --_name_scrl_pending_frames;
    } else {
        _name_scrl_pending_frames = 20;
        if (_name_scrl_shift_ctr > 0) {
            if (8 + _name_scrl_shift < static_cast<int>(temps.size())) {
                _name_scrl_shift = B(_name_scrl_shift + _name_scrl_shift_ctr);
            } else {
                _name_scrl_shift_ctr = -1;
                _name_scrl_pending_frames = _name_scrl_pending_frames * 10;
            }
        } else if (_name_scrl_shift > 0) {
            _name_scrl_shift = B(_name_scrl_shift + _name_scrl_shift_ctr);
        } else {
            _name_scrl_shift_ctr = 1;
            _name_scrl_pending_frames = _name_scrl_pending_frames * 50;
        }
    }

    // WAV 録音 (sdl_opl3_emulator) は未実装のため、録音していないときの表示のみ
    if (!(area_x1 + area_y1 == 0) && area_x1 <= 20 && area_y1 <= 6) {
        show_str(13, 1, "\xC4\xC4\xC4", main_background + main_border);
    } else if (!module_archived) {
        show_cstr(8, 7, "~[\x1D]~\xCD\xCD", status_background + status_border, main_background + main_behavior);
    } else {
        show_str(8, 7, "\xCD\xCD\xCD\xCD\xCD", status_background + status_border);
    }

    const uint8_t ys = B(MAX_PATTERN_ROWS + 12);
    if (command_typing == 1) {
        if (!cycle_pattern) show_str(B(MAX_COLUMNS - 17), ys, "A", main_background + main_behavior);
        else show_str(B(MAX_COLUMNS - 17), ys, "F", main_background + main_behavior);
    } else if (command_typing == 2 && !cycle_pattern) {
        show_str(B(MAX_COLUMNS - 17), ys, "S", main_background + main_behavior);
    } else {
        show_str(B(MAX_COLUMNS - 17), ys, "\xC4", main_background + main_border);
    }

    if (sd.flag_4op != 0) {
        show_cstr(B(MAX_COLUMNS - 43), ys, "~[~\x04\x03~]~", main_background + main_hi_stat_line, main_background + main_stat_line);
    } else {
        show_cstr(B(MAX_COLUMNS - 43), ys, "~[~\x02\x03~]~", main_background + main_hi_stat_line, main_background + main_stat_line);
    }

    for (int temp = 1; temp <= 8; ++temp) {
        if (temp != current_octave) show_str(B(30 + temp), ys, std::string(1, static_cast<char>(48 + temp)), main_background + main_stat_line);
        else show_str(B(30 + temp), ys, std::string(1, static_cast<char>(48 + temp)), main_background + main_hi_stat_line);
    }

    const int nm_tracks_shown = std::min<int>(MAX_TRACKS, sd.nm_tracks);
    uint8_t chan = chan_pos;
    temps.clear();
    for (int temp2 = 1; temp2 <= nm_tracks_shown; ++temp2) {
        uint8_t temp;
        if (!p.pan_lock[chan] || (ps == isStopped && !p.debugging)) temp = pattern_bckg + pattern_pan_indic;
        else temp = pattern_bckg + pattern_gpan_indic;

        const uint8_t x = B(12 + (temp2 - 1) * 15);
        if (!(ps == isStopped && !p.debugging && !p.panlock) && p.channel_flag[chan]) {
            if (p.percussion_mode && chan >= 16 && chan <= 20) {
                static const char* const perc[5] = { "~BD~ ", "~SD~ ", "~TT~ ", "~TC~ ", "~HH~ " };
                temps = perc[chan - 16];
            } else {
                const uint8_t bit = flag_4op_bit(chan);
                if (bit != 0 && (sd.flag_4op | bit) == sd.flag_4op) {
                    if (!p.vol4op_lock[chan]) temps = "~\x04\x03~ ";
                    else temps = "~\x04\x03+~";
                } else {
                    temps = volslide_str(p.volslide_type[chan], temps);
                }
            }

            if (p.volume_lock[chan]) temps += "V+";
            if (p.peak_lock[chan]) temps += "P+";

            const uint8_t pan = (p.panlock && ps == isStopped && !p.debugging) ? B(sd.lock_flags[chan - 1] & 3)
                                                                                 : p.panning_table[chan];
            switch (pan) {
            case 0: show_str(x, 9, "\x15\x16 ", temp); break;
            case 1: show_str(x, 9, "\x15  ", temp); break;
            case 2: show_str(x, 9, " \x16 ", temp); break;
            default: break;
            }

            if (!(chan >= 16 && chan <= 20)) {
                show_cstr(B(x + 3), 9, ExpStrR(temps, 7, ' '), pattern_bckg + pattern_lock_indic, pattern_bckg + pattern_4op_indic);
            } else {
                show_cstr(B(x + 3), 9, ExpStrR(temps, 7, ' '), pattern_bckg + pattern_lock_indic, pattern_bckg + pattern_perc_indic);
            }
        } else {
            show_idle_track_indicator(temp2, chan);
        }

        const uint8_t xb = B(8 + (temp2 - 1) * 15);
        const uint8_t yb = B(MAX_PATTERN_ROWS + 11);
        if (!p.channel_flag[chan]) {
            show_str(xb, yb, "\xCD\xCD\xCD", pattern_bckg + pattern_border);
            show_str(B(11 + (temp2 - 1) * 15), yb, " () OFF ", pattern_bckg + pattern_chan_indic);
            if (temp2 < nm_tracks_shown) show_str(B(19 + (temp2 - 1) * 15), yb, "\xCD\xCD\xCD\xCF", pattern_bckg + pattern_border);
            else show_str(B(19 + (temp2 - 1) * 15), yb, "\xCD\xCD\xCD\xCA", pattern_bckg + pattern_border);
            show_idle_track_indicator(temp2, chan);
        } else if (!(screen_scroll_offset > 10 || !is_default_screen_mode())) {
            const bool is4op = p.is_4op_chan(chan);
            if (!is4op || (is4op && ((in_4op_tracks_lo(chan) && temp2 == 1) ||
                                     (in_4op_tracks_hi(chan) && temp2 == nm_tracks_shown)))) {
                const uint8_t temp_lvl = B(std::nearbyint(42.0 / 63 * p.volum_bar[chan].lvl));
                if (temp2 < nm_tracks_shown) {
                    show_str(xb, yb, ExpStrR(_vol_bar_str(temp_lvl), 14, '\xCD') + "\xCF", pattern_bckg + pattern_border);
                } else {
                    show_str(xb, yb, ExpStrR(_vol_bar_str(temp_lvl), 14, '\xCD') + "\xCA", pattern_bckg + pattern_border);
                }
            } else if (in_4op_tracks_hi(chan)) {
                const uint8_t temp_lvl = B(std::nearbyint(84.0 / 63 * p.volum_bar[chan].lvl));
                std::string bar = _vol_bar_str(temp_lvl);
                if (bar.size() > 14) {
                    bar[14] = '\xA9';
                    bar = ExpStrR(bar, 29, '\xCD');
                } else {
                    bar = ExpStrR(bar, 29, '\xCD');
                    bar[14] = '\xCF';
                }
                if (temp2 < nm_tracks_shown - 1) bar += '\xCF';
                else bar += '\xCA';
                show_str(xb, yb, bar, pattern_bckg + pattern_border);
            }
        } else {
            show_str(xb, yb, Copy(patt_win[4], 7 + (temp2 - 1) * 15, 15), pattern_bckg + pattern_border);
        }
        ++chan;
    }
}

// ---------------------------------------------------------------------------
// パターン窓 (ipattern.inc)
// ---------------------------------------------------------------------------

void PATTERN_tabs_refresh()
{
    if (command_typing == 0) return;
    const int last = std::min<int>(chan_pos + MAX_TRACKS - 1, songdata().nm_tracks);
    const uint8_t cur = count_channel(pattern_hpos);
    for (int temp = chan_pos; temp <= last; ++temp) {
        if (temp != cur) {
            show_str(B(8 + (temp - (chan_pos - 1) - 1) * 15), 10, patt_tab_str[0], pattern_bckg + pattern_border);
        }
    }

    const uint8_t x = B(8 + (cur - (chan_pos - 1) - 1) * 15);
    const char* str = nullptr;
    switch (count_pos(pattern_hpos)) {
    case 0: case 1: str = patt_tab_str[1]; break;
    case 2: case 3: str = patt_tab_str[2]; break;
    case 4: case 5: case 6: str = patt_tab_str[3]; break;
    case 7: case 8: case 9: str = patt_tab_str[4]; break;
    default: break;
    }
    if (str) show_cstr(x, 10, str, pattern_bckg + pattern_border, pattern_bckg + pattern_pos_indic);
}

uint16_t get_4op_to_test();

void STATUS_LINE_refresh()
{
    if (really_no_status_refresh) return;
    A2Player& p = P();
    const int ps = play_status();
    const uint8_t ys = B(MAX_PATTERN_ROWS + 12);

    const uint16_t pair = get_4op_to_test();
    if (pair != 0) {
        show_cstr(40, ys, "~[~" + byte2hex(static_cast<uint8_t>(pair >> 8)) + "~+~" + byte2hex(static_cast<uint8_t>(pair & 0xff)) + "~]~",
                  main_background + main_hi_stat_line, main_background + main_stat_line);
    } else {
        show_cstr(40, ys, "~[~" + byte2hex(current_inst) + "~]~\xC4\xC4\xC4",
                  main_background + main_border, main_background + main_stat_line);
    }

    show_cstr(B(MAX_COLUMNS - 39), ys,
              "~[LN:~" + ExpStrL(Num2str(pattern_page, 10), 3, '0') + "/" +
                  ExpStrL(Num2str(songdata().patt_len - 1, 10), 3, '0') + "~;~",
              main_background + main_hi_stat_line, main_background + main_stat_line);
    show_cstr(B(MAX_COLUMNS - 27), ys,
              "~TRK:~" + byte2dec(count_channel(pattern_hpos)) + "/" + byte2dec(songdata().nm_tracks) + "~]~",
              main_background + main_hi_stat_line, main_background + main_stat_line);

    if (midiboard || (p.debugging && ps == isStopped)) show_str(3, ys, "MBOARD", main_background + main_hi_stat_line);
    else show_str(3, ys, "MBOARD", main_background + main_stat_line);

    if (!p.debugging && tracing && ps != isStopped) show_str(10, ys, "TRACE", main_background + main_hi_stat_line);
    else show_str(10, ys, "TRACE", main_background + main_stat_line);

    if (p.debugging && ps != isStopped) show_str(16, ys, "DEBUG", main_background + main_hi_stat_line);
    else show_str(16, ys, "DEBUG", main_background + main_stat_line);

    if (p.debugging && ps == isStopped) show_str(22, ys, "TRACKiNG", main_background + main_hi_stat_line);
    else show_str(22, ys, "TRACKiNG", main_background + main_stat_line);

    if (linefeed && command_typing != 0) show_str(B(MAX_COLUMNS - 16), ys, "\x7F", main_background + main_behavior);
    else show_str(B(MAX_COLUMNS - 16), ys, "\x7F", main_background + main_behavior_dis);

    if (jump_mark_mode) show_str(B(MAX_COLUMNS - 15), ys, "\x17", main_background + main_behavior);
    else show_str(B(MAX_COLUMNS - 15), ys, "\x17", main_background + main_behavior_dis);
}

uint16_t PRED1(int value) { return static_cast<uint16_t>(value > 1 ? value - 1 : 1); }

bool is_control_effect(uint8_t def, uint8_t value)
{
    return def == ef_PositionJump || def == ef_PatternBreak || def == ef_SetSpeed || def == ef_SetTempo ||
           (def == ef_Extended && (value / 16 == ef_ex_PatternLoop || value / 16 == ef_ex_PatternLoopRec));
}

void _set_attr(const Chunk& chunk, uint8_t _bckg, uint8_t _fix_note, uint8_t _note, uint8_t _note0,
               uint8_t _inst, uint8_t _inst0, uint16_t& attr, uint8_t& attr2)
{
    if (chunk.note != 0) {
        if (chunk.note >= fixed_note_flag + 1 && chunk.note <= fixed_note_flag + 12 * 8 + 1) {
            attr = static_cast<uint16_t>(attr + ((_bckg + _fix_note) << 8));
        } else {
            attr = static_cast<uint16_t>(attr + ((_bckg + _note) << 8));
        }
    } else {
        attr = static_cast<uint16_t>(attr + ((_bckg + _note0) << 8));
    }

    if (chunk.instr_def != 0) attr2 = B(_bckg + _inst);
    else attr2 = B(_bckg + _inst0);
}

char fx_digit(uint8_t def)
{
    // fx_digits は 48 文字。範囲外はオリジナルでは後続のメモリを読むため、ここでは '?' とする
    return def < 48 ? fx_digits[def] : '?';
}

void PATTERN_page_refresh(uint8_t page)
{
    A2Player& p = P();
    FixedSongData& sd = songdata();
    const int ps = play_status();
    const int MID = MAX_PATTERN_ROWS / 2;

    for (int temp = sd.nm_tracks + 1; temp <= MAX_TRACKS; ++temp) {
        show_str(B(8 + (temp - 1) * 15), 10, patt_tab_str[0], pattern_bckg + pattern_border);
        show_str(B(11 + (temp - 1) * 15), 9, ExpStrL("", 10, ' '), pattern_bckg + pattern_border);
    }

    PATTERN_tabs_refresh();
    const uint16_t last_line = static_cast<uint16_t>(sd.patt_len - 1);
    if (page > last_line) page = B(last_line);
    if (pattern_page > last_line) pattern_page = B(last_line);

    normalize_track_position(pattern_hpos);

    const uint8_t spos = page < MID - 1 ? B(MID - 1 - page) : 0;
    const int lim = static_cast<int16_t>(last_line) - (MID - 1) - 1;
    const uint8_t epos = page > lim ? B(page - lim) : 0;

    std::string dummy_str;
    if (spos != 0 || epos != 0) {
        for (int temp2 = chan_pos; temp2 <= chan_pos + MAX_TRACKS - 1; ++temp2) {
            const int k = temp2 - (chan_pos - 1);
            dummy_str = k != MAX_TRACKS ? "\xB3" : "";

            for (int temp3 = 1; temp3 <= spos; ++temp3) {
                show_str(3, B(10 + temp3), ExpStrL("", 4, ' ') + "\xBA", pattern_bckg + pattern_border);
                show_str(B(MAX_COLUMNS - 8), B(10 + temp3), "\xBA" + ExpStrL("", 4, ' '), pattern_bckg + pattern_border);
                show_str(B(8 + pos3[k * 4 - 3]), B(10 + temp3), ExpStrL("", 14, ' ') + dummy_str, pattern_bckg + pattern_border);
            }
            for (int temp3 = MAX_PATTERN_ROWS; temp3 >= MAX_PATTERN_ROWS - epos + 1; --temp3) {
                show_str(3, B(10 + temp3), ExpStrL("", 4, ' ') + "\xBA", pattern_bckg + pattern_border);
                show_str(B(MAX_COLUMNS - 8), B(10 + temp3), "\xBA" + ExpStrL("", 4, ' '), pattern_bckg + pattern_border);
                show_str(B(8 + pos3[k * 4 - 3]), B(10 + temp3), ExpStrL("", 14, ' ') + dummy_str, pattern_bckg + pattern_border);
            }
        }
    }

    show_str(B(17 + (MAX_COLUMNS - 19) / 2), 8, byte2hex(pattern_patt), pattern_bckg + pattern_border);

    Chunk chunk = {};
    for (int temp1 = 1 + spos; temp1 <= MAX_PATTERN_ROWS - epos; ++temp1) {
        const uint8_t line = B(page + temp1 - MID);
        const uint8_t y = B(10 + temp1);
        const std::string num = byte2hex(line);

        uint8_t _row_bckg = BYTE_NULL, _row_bckg2 = 0, _row_fgnd = 0;
        if (tracing && ps != isStopped) {
            if (temp1 == MID) {
                _row_bckg = pattern_row_bckg_p;
                _row_bckg2 = pattern_bckg;
                _row_fgnd = pattern_line_p;
            }
        } else if (pattern_patt == p.current_pattern && line == p.current_line && ps != isStopped) {
            _row_bckg = pattern_row_bckg_p;
            _row_bckg2 = pattern_bckg;
            _row_fgnd = pattern_line_p;
        }

        // 行番号 (マーク中の表示は編集機能とともに実装する)
        bool centered;   // カーソル行の形式 (#16 XX #17)
        const bool marked_line = p.mark_line != 0 && (line == 0 || line % p.mark_line == 0);
        if (!(p.mark_line != 0 && mark_lines)) {
            if (temp1 != MID) {
                if (_row_bckg == BYTE_NULL) { _row_bckg = pattern_bckg; _row_bckg2 = pattern_bckg; _row_fgnd = pattern_line; }
                centered = false;
            } else {
                if (_row_bckg == BYTE_NULL) { _row_bckg = pattern_row_bckg; _row_bckg2 = pattern_bckg; _row_fgnd = pattern_hi_line; }
                centered = true;
            }
        } else if (temp1 != MID) {
            if (marked_line) {
                if (_row_bckg == BYTE_NULL) { _row_bckg = pattern_row_bckg; _row_bckg2 = pattern_bckg; _row_fgnd = pattern_hi_line; }
            } else {
                if (_row_bckg == BYTE_NULL) { _row_bckg = pattern_bckg; _row_bckg2 = pattern_bckg; _row_fgnd = pattern_line; }
            }
            centered = false;
        } else {
            if (_row_bckg == BYTE_NULL) { _row_bckg = pattern_row_bckg_m; _row_bckg2 = pattern_bckg; _row_fgnd = pattern_hi_line_m; }
            centered = true;
        }

        if (!centered) {
            show_cstr(3, y, " " + num + " ~\xBA~", B(_row_bckg + _row_fgnd), B(_row_bckg2 + pattern_border));
            show_cstr(B(MAX_COLUMNS - 8), y, "~\xBA~ " + num + " ", B(_row_bckg + _row_fgnd), B(_row_bckg2 + pattern_border));
        } else {
            show_cstr(3, y, "\x10" + num + "\x11~\xBA~", B(_row_bckg + _row_fgnd), B(_row_bckg2 + pattern_border));
            show_cstr(B(MAX_COLUMNS - 8), y, "~\xBA~\x10" + num + "\x11", B(_row_bckg + _row_fgnd), B(_row_bckg2 + pattern_border));
        }

        for (int temp2 = chan_pos; temp2 <= chan_pos + MAX_TRACKS - 1; ++temp2) {
            get_chunk(pattern_patt, line, B(temp2), chunk);
            const bool fx1 = !(chunk.effect_def == 0 && chunk.effect == 0);
            const bool fx2 = !(chunk.effect_def2 == 0 && chunk.effect2 == 0);
            uint16_t attr;
            uint8_t attr2 = 0, attr3;

            if (temp1 == MID) {
                if (!(p.mark_line != 0 && mark_lines)) {
                    attr = fx1 ? pattern_row_bckg + pattern_hi_cmnd : pattern_row_bckg + pattern_hi_cmnd0;
                    attr3 = fx2 ? pattern_row_bckg + pattern_hi_cmnd : pattern_row_bckg + pattern_hi_cmnd0;
                    _set_attr(chunk, pattern_row_bckg, pattern_hi_fx_note, pattern_hi_note, pattern_hi_note0,
                              pattern_hi_inst, pattern_hi_inst0, attr, attr2);
                } else {
                    attr = fx1 ? pattern_row_bckg_m + pattern_cmnd_m : pattern_row_bckg_m + pattern_cmnd0_m;
                    attr3 = fx2 ? pattern_row_bckg_m + pattern_cmnd_m : pattern_row_bckg_m + pattern_cmnd0_m;
                    _set_attr(chunk, pattern_row_bckg_m, pattern_fix_note_m, pattern_note_m, pattern_note0_m,
                              pattern_inst_m, pattern_inst0_m, attr, attr2);
                }
            } else if (marked_line && mark_lines) {
                attr = fx1 ? pattern_row_bckg + pattern_hi_cmnd : pattern_row_bckg + pattern_hi_cmnd0;
                attr3 = fx2 ? pattern_row_bckg + pattern_hi_cmnd : pattern_row_bckg + pattern_hi_cmnd0;
                _set_attr(chunk, pattern_row_bckg, pattern_hi_fx_note, pattern_hi_note, pattern_hi_note0,
                          pattern_hi_inst, pattern_hi_inst0, attr, attr2);
            } else {
                if (fx1) {
                    attr = (!highlight_controls || !is_control_effect(chunk.effect_def, chunk.effect))
                               ? pattern_bckg + pattern_cmnd : pattern_bckg + pattern_cmnd_ctrl;
                } else {
                    attr = pattern_bckg + pattern_cmnd0;
                }
                if (fx2) {
                    attr3 = (!highlight_controls || !is_control_effect(chunk.effect_def2, chunk.effect2))
                                ? pattern_bckg + pattern_cmnd : pattern_bckg + pattern_cmnd_ctrl;
                } else {
                    attr3 = pattern_bckg + pattern_cmnd0;
                }
                _set_attr(chunk, pattern_bckg, pattern_fix_note, pattern_note, pattern_note0,
                          pattern_inst, pattern_inst0, attr, attr2);
            }

            const int k = temp2 - (chan_pos - 1);
            uint8_t dummy_atr = B((attr3 & 0xf0) + pattern_border);
            dummy_str = k != MAX_TRACKS ? "\xB3" : "";

            const uint8_t note_attr = B(attr >> 8);
            const uint8_t cmnd_attr = B(attr & 0xff);
            const uint8_t xn = B(8 + pos3[k * 4 - 3]);
            const uint8_t xi = B(8 + pos3[k * 4 - 2]);
            const uint8_t xc = B(8 + pos3[k * 4 - 1]);
            const uint8_t xc2 = B(8 + pos3[k * 4]);

            if (temp2 > std::min<int>(chan_pos + MAX_TRACKS - 1, sd.nm_tracks)) {
                show_str(xn, y, "    ", note_attr);
                show_str(xi, y, "   ", attr2);
                show_str(xc, y, "    ", cmnd_attr);
                show_cstr_alt(xc2, y, "   \n" + dummy_str + "\n", attr3, dummy_atr);
                continue;
            }

            // ノート
            static const char* const null_note[3] = { "\xFA\xFA\xFA ", "\xFA\xFA\xFA ", "\xFA\xFA\xFA " };
            static const char* const keyoff_note[3] = { "\xFE\xFE\xFE ", "\xCD\xCD\xCD ", "^^\xFA " };
            const uint8_t layout = pattern_layout <= 2 ? pattern_layout : 0;
            if (chunk.note == 0) {
                show_str(xn, y, null_note[layout], note_attr);
            } else if (chunk.note >= 1 && chunk.note <= 12 * 8 + 1) {
                show_str(xn, y, std::string(note_layout[chunk.note]) + " ", note_attr);
            } else if (chunk.note >= fixed_note_flag + 1 && chunk.note <= fixed_note_flag + 12 * 8 + 1) {
                show_str(xn, y, std::string(note_layout[chunk.note - fixed_note_flag]) + " ", note_attr);
            } else if (chunk.note == BYTE_NULL) {
                show_str(xn, y, keyoff_note[layout], note_attr);
            }

            // 楽器・エフェクト
            static const char inst_fill[3] = { '\xF9', '\xFA', '\xFA' };
            static const char* const null_fx[3] = { "\xFA\xFA\xFA", "\xF9\xF9\xF9", "\xFA\xF9\xF9" };
            if (chunk.instr_def == 0) show_str(xi, y, std::string(2, inst_fill[layout]) + " ", attr2);
            else show_str(xi, y, ExpStrL(Num2str(chunk.instr_def, 16), 2, inst_fill[layout]) + " ", attr2);

            if (!fx1) show_str(xc, y, std::string(null_fx[layout]) + " ", cmnd_attr);
            else show_str(xc, y, fx_digit(chunk.effect_def) + byte2hex(chunk.effect) + " ", cmnd_attr);

            if (!fx2) show_cstr_alt(xc2, y, std::string(null_fx[layout]) + "\n" + dummy_str + "\n", attr3, dummy_atr);
            else show_cstr_alt(xc2, y, fx_digit(chunk.effect_def2) + byte2hex(chunk.effect2) + "\n" + dummy_str + "\n", attr3, dummy_atr);
        }
    }

    STATUS_LINE_refresh();
    for (int temp1 = chan_pos; temp1 <= chan_pos + MAX_TRACKS - 1; ++temp1) {
        std::string temps;
        const uint8_t ch = B(temp1);
        if (!p.is_4op_chan(ch)) temps = "~ ~" + ExpStrR(Num2str(temp1, 10), 2, ' ') + "~ ~";
        else if (in_4op_tracks_hi(ch)) temps = "~\xF4~" + ExpStrR(Num2str(temp1, 10), 2, ' ') + "~ ~";
        else temps = "~\xF5~" + ExpStrR(Num2str(temp1, 10), 2, ' ') + "~ ~";

        const uint8_t chan_attr = pattern_bckg + pattern_border;
        show_cstr(B(8 + (temp1 - (chan_pos - 1) - 1) * 15), 9, temps, chan_attr, pattern_bckg + pattern_4op_indic);
    }

    const uint8_t sb_y = 8;
    const uint8_t sb_size = B(MAX_PATTERN_ROWS + 4);
    if (scroll_bars) {
        scroll_pos2 = vscroll_bar(B(MAX_COLUMNS - 2), sb_y, sb_size, PRED1(sd.patt_len), page, scroll_pos2,
                                  scrollbar_bckg + scrollbar_text, scrollbar_bckg + scrollbar_mark);
        scroll_pos3 = hscroll_bar(B(MAX_COLUMNS - 14), B(MAX_PATTERN_ROWS + 12), 13, PRED1(sd.nm_tracks),
                                  B(count_channel(pattern_hpos) - 1), scroll_pos3,
                                  scrollbar_bckg + scrollbar_text, scrollbar_bckg + scrollbar_mark);
        if (pattern_patt <= 127) {
            scroll_pos4 = vscroll_bar(B(MAX_COLUMNS - 1), sb_y, sb_size, 127, pattern_patt, scroll_pos4,
                                      scrollbar_bckg + scrollbar_text, scrollbar_bckg + scrollbar_2nd_mark);
        } else {
            scroll_pos4 = vscroll_bar(B(MAX_COLUMNS - 1), sb_y, sb_size, 1, 1, BYTE_NULL,
                                      scrollbar_bckg + scrollbar_text, scrollbar_bckg + scrollbar_mark);
        }
    } else {
        scroll_pos2 = vscroll_bar(B(MAX_COLUMNS - 2), sb_y, sb_size, 1, 1, BYTE_NULL,
                                  scrollbar_bckg + scrollbar_text, scrollbar_bckg + scrollbar_mark);
        scroll_pos3 = hscroll_bar(B(MAX_COLUMNS - 14), B(MAX_PATTERN_ROWS + 12), 13, 1, 1, BYTE_NULL,
                                  scrollbar_bckg + scrollbar_text, scrollbar_bckg + scrollbar_mark);
        scroll_pos4 = vscroll_bar(B(MAX_COLUMNS - 1), sb_y, sb_size, 1, 1, BYTE_NULL,
                                  scrollbar_bckg + scrollbar_text, scrollbar_bckg + scrollbar_mark);
    }
}

// ---------------------------------------------------------------------------
// PATTERN ORDER 窓 (ipattord.inc)
// ---------------------------------------------------------------------------

void PATTERN_ORDER_page_refresh(uint8_t page)
{
    A2Player& p = P();
    const int ps = play_status();

    show_str(B(25 + 6 + PATTORD_xshift + (MAX_ORDER_COLS * 7 - 1) / 2 + (MAX_ORDER_COLS * 7 - 1) % 2), 2,
             byte2hex(B(pattord_vpos + 4 * (pattord_hpos + pattord_page - 1) - 1)),
             order_background + order_border);

    const uint8_t xstart = B(20 + PATTORD_xshift);
    for (int temp1 = 1; temp1 <= MAX_ORDER_COLS; ++temp1) {
        for (int temp2 = 3; temp2 <= 6; ++temp2) {
            const uint8_t index = B(temp2 - 2 + 4 * (temp1 + page - 1) - 1);
            const uint8_t order = p.order_at(index);
            uint8_t attr_lo, attr_hi;   // LO = パターン番号、HI = オーダー番号
            if (!tracing || p.play_single_patt) {
                if (order < 0x80) { attr_lo = order_background + order_hi_pattn; attr_hi = order_background + order_hi_entry; }
                else { attr_lo = order_background + order_entry; attr_hi = order_background + order_hi_entry; }
            } else if (p.current_order == index) {
                attr_lo = order_hi_bckg + order_hi_pattn;
                attr_hi = order_hi_bckg + order_hi_entry;
            } else if (order < 0x80) {
                attr_lo = order_background + order_pattn;
                attr_hi = order_background + order_entry;
            } else {
                attr_lo = order_background + order_pattn_jump;
                attr_hi = order_background + order_entry;
            }

            const uint8_t x = B(xstart + pos1[temp1] - 1);
            const uint8_t y = B(temp2);
            if (order < 0x80) {
                if (p.current_order == index && ps != isStopped && !p.play_single_patt) {
                    show_cstr(x, y, byte2hex(index) + "~ ~", order_played_b + order_played, attr_hi);
                } else {
                    show_str(x, y, byte2hex(index) + " ", attr_hi);
                }
                if (!no_trace_pattord) show_str(B(x + 3), y, byte2hex(order), attr_lo);
            } else {
                show_str(x, y, byte2hex(index) + " ", attr_hi);
                if (!no_trace_pattord) show_str(B(x + 3), y, byte2hex(B(order - 0x80)), attr_lo);
            }
            if (!tracing && temp1 <= MAX_ORDER_COLS - 1) show_str(B(x + 5), y, "\xF0", order_background + order_border);
        }
    }

    uint8_t temp1 = 0;
    if (tracing) count_order(temp1);
    const uint16_t pos = static_cast<uint16_t>((page + pattord_hpos - 1) * 4 + pattord_vpos);
    if (scroll_bars) {
        if (program_screen_mode != 2) {
            if (temp1 != 0) {
                scroll_pos0 = vscroll_bar(B(MAX_COLUMNS - 2), 2, 6, temp1, pos, scroll_pos0,
                                          scrollbar_bckg + scrollbar_text, scrollbar_bckg + scrollbar_mark);
            } else {
                scroll_pos0 = vscroll_bar(B(MAX_COLUMNS - 2), 2, 6, 1, 1, BYTE_NULL,
                                          scrollbar_bckg + scrollbar_text, scrollbar_bckg + scrollbar_mark);
            }
        }
        scroll_pos1 = vscroll_bar(B(MAX_COLUMNS - 1), 2, 6, 0x7f, pos, scroll_pos1,
                                  scrollbar_bckg + scrollbar_text, scrollbar_bckg + scrollbar_2nd_mark);
    } else {
        if (program_screen_mode != 2) {
            scroll_pos0 = vscroll_bar(B(MAX_COLUMNS - 2), 2, 6, 1, 1, BYTE_NULL,
                                      scrollbar_bckg + scrollbar_text, scrollbar_bckg + scrollbar_mark);
        }
        scroll_pos1 = vscroll_bar(B(MAX_COLUMNS - 1), 2, 6, 1, 1, BYTE_NULL,
                                  scrollbar_bckg + scrollbar_text, scrollbar_bckg + scrollbar_mark);
    }
}

// 演奏中のオーダーが見える PATTERN ORDER 窓の位置を求める
void locate_order(uint8_t order, uint8_t& page, uint8_t& hpos, uint8_t& vpos)
{
    page = 0;
    hpos = 1;
    vpos = 1;
    for (int guard = 0; guard < 512 && order != B(vpos + 4 * (hpos + page - 1) - 1); ++guard) {
        if (vpos < 4) {
            ++vpos;
        } else if (hpos < MAX_ORDER_COLS) {
            ++hpos;
            vpos = 1;
        } else if (page < 23 - (MAX_ORDER_COLS - 9)) {
            ++page;
            vpos = 1;
        }
    }
}

// ---------------------------------------------------------------------------
// リアルタイム処理 (adt2unit.pas)
// ---------------------------------------------------------------------------

void trace_update_proc()
{
    A2Player& p = P();
    p.space_pressed = scankey(SC_SPACE);
    if (play_status() != isPlaying) return;
    if (_traceprc_last_order != p.current_order || _traceprc_last_pattern != p.current_pattern ||
        _traceprc_last_line != p.current_line) {
        _traceprc_last_order = p.current_order;
        _traceprc_last_pattern = p.order_at(p.current_order);
        _traceprc_last_line = p.current_line;

        if (tracing) {
            if (!p.play_single_patt) pattern_patt = p.order_at(p.current_order);
            pattern_page = p.current_line;
            locate_order(p.current_order, pattord_page, pattord_hpos, pattord_vpos);
        }

        PATTERN_ORDER_page_refresh(pattord_page);
        PATTERN_page_refresh(pattern_page);
    }
}

// timer_poll_proc の画面関係の部分 (点滅の周期と演奏時間の表示用カウンタ)
void timer_poll_proc()
{
    A2Player& p = P();
    if (++blink_ticks > 50) {
        blink_flag = !blink_flag;
        blink_ticks = 0;
    }
    if (++_NRECM_blink_ticks > 50) {
        _NRECM_blink_flag = !_NRECM_blink_flag;
        _NRECM_blink_ticks = 0;
    }
    if (++_IRQFREQ_blink_ticks > 25) {
        _IRQFREQ_blink_flag = !_IRQFREQ_blink_flag;
        _IRQFREQ_blink_ticks = 0;
    }

    if (p.play_status == PlayStatus::Playing && !(p.debugging && (!p.space_pressed || p.no_step_debugging))) {
        p.song_timer_tenths = p.timer_temp;
        if (p.song_timer_tenths >= 100) p.song_timer_tenths = 0;
        if (p.timer_temp < 100) {
            ++p.timer_temp;
        } else {
            ++p.song_timer;
            p.timer_temp = 1;
        }
    } else if (p.debugging && (!p.space_pressed || p.no_step_debugging)) {
        if (!p.pattern_delay) p.synchronize_song_timer();
    }

    if (p.song_timer > 3600 - 1) {
        p.song_timer = 0;
        p.timer_temp = 0;
        p.song_timer_tenths = 0;
    }
}

// SDL 版では timer_poll_proc を SDL_AddTimer で (1000 / Hz) ms ごとに呼ぶ。
// SDL 1.2 のタイマーは 10ms 単位に切り上げられるため、通常の周波数 (100Hz 以上) では 100Hz で動き、
// 演奏時間の 1/100 秒のカウンタが実時間に合う。これを経過時間から再現する。
void timer_poll_catchup()
{
    const DWORD now = timeGetTime();
    const int32_t hz = std::max<int32_t>(P().timerHz(), 1);
    DWORD interval = static_cast<DWORD>(1000 / hz);
    interval = std::max<DWORD>((interval + 9) / 10 * 10, 10);
    if (now - g_timer_last > 1000) g_timer_last = now - interval;
    while (now - g_timer_last >= interval) {
        g_timer_last += interval;
        timer_poll_proc();
    }
}

void realtime_gfx_poll_proc()
{
    if (_realtime_gfx_no_update || !sound_player()) return;
    PlayerLock lock;
    A2Player& p = P();
    timer_poll_catchup();

    if (blink_flag) {
        if (p.debugging && p.play_status == PlayStatus::Stopped) status_layout[isStopped][8] = '\x09';
        else status_layout[isStopped][8] = ' ';
        if (!p.debugging) status_layout[isPlaying][8] = '\x10';
        else status_layout[isPlaying][8] = '\x09';
        status_layout[isPaused][7] = '\x08';
    } else {
        status_layout[isPlaying][8] = ' ';
        status_layout[isPaused][7] = ' ';
        status_layout[isStopped][8] = ' ';
    }

    decay_bars_refresh();
    if (do_synchronize) synchronize_screen();
    if (_name_scrl_pending_frames > 0) --_name_scrl_pending_frames;
    ++_cursor_blink_pending_frames;
    status_refresh();
    STATUS_LINE_refresh();
    trace_update_proc();
}

// fade_out_playback (SDL 版)。画面のフェードアウト (vid_FadeOut) は行わない。
void fade_out_playback(bool fade_screen)
{
    const int factor = fade_screen ? 255 : 63;
    uint8_t temp2;
    uint8_t temp3 = 0;
    bool stopped;
    {
        PlayerLock lock;
        A2Player& p = P();
        temp2 = p.global_volume > 0 ? B(factor / p.global_volume) : 0;
        p.fade_out_volume = 63;
        stopped = p.play_status == PlayStatus::Stopped;
    }

    if (stopped) return;
    for (int temp = 1; temp <= factor; ++temp) {
        if (++temp3 > temp2) {
            temp3 = 0;
            {
                PlayerLock lock;
                A2Player& p = P();
                p.fade_out_volume = B(p.fade_out_volume - 1);
                p.set_global_volume();
            }
            if (fade_screen || temp % 5 == 0) {
                _draw_screen_without_delay = true;
                draw_screen();
                keyboard_reset_buffer();
            }
        }
    }
}

// ---------------------------------------------------------------------------
// 操作
// ---------------------------------------------------------------------------

void POSITIONS_reset()
{
    pattord_page = 0;
    pattord_hpos = 1;
    pattord_vpos = 1;
    instrum_page = 1;
    pattern_page = 0;
    pattern_hpos = 1;

    const uint8_t first = songdata().pattern_order[0];
    pattern_patt = first > 0x7f ? 0 : first;

    chan_pos = 1;
    PATTERN_ORDER_page_refresh(0);
    PATTERN_page_refresh(0);
}

// calibrate_player の画面処理 (ロックを取った状態で呼ぶ)
void calibrate_player(uint8_t order, uint8_t line, bool line_dependent)
{
    A2Player& p = P();
    p.calibrate_player(order, line, line_dependent);

    uint8_t page, hpos, vpos;
    locate_order(p.current_order, page, hpos, vpos);
    if (tracing) {
        PATTERN_ORDER_page_refresh(page);
        PATTERN_page_refresh(line);
    }
    keyboard_reset_buffer();
}

void start_playing()
{
    P().startPlaying();
}

void stop_playing()
{
    P().stopPlaying();
}

void refresh_windows()
{
    PlayerLock lock;
    PATTERN_ORDER_page_refresh(pattord_page);
    PATTERN_page_refresh(pattern_page);
}

void load_song(const std::wstring& path)
{
    fade_out_playback(false);

    const bool is_pattern = GetModuleKind(path) == ModuleKind::Pattern;
    std::wstring error;
    if (!sound_open_song(path, pattern_patt, error)) {
        MessageBoxW(g_hwnd, error.c_str(), L"AdLibTrackerModoki", MB_OK | MB_ICONWARNING);
        keyboard_init();
        refresh_windows();
        return;
    }

    PlayerLock lock;
    A2Player& p = P();
    // A2P はパターンを読み込むだけなので、表示位置と曲の保存状態はそのまま
    if (!is_pattern) {
        songdata_source = path;
        module_archived = true;
        _name_scrl_shift = 0;
        _name_scrl_shift_ctr = 1;
        _name_scrl_pending_frames = 0;
        POSITIONS_reset();
    }
    force_scrollbars = true;
    PATTERN_ORDER_page_refresh(pattord_page);
    PATTERN_page_refresh(pattern_page);
    force_scrollbars = false;
    std::fill(std::begin(p.ai_table), std::end(p.ai_table), uint8_t(0));
    no_status_refresh = false;
}

// F3: ファイルを開く (本クローン版では Windows のコモンダイアログを使う)
void FILE_open()
{
    WCHAR buf[MAX_PATH] = L"";
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwnd;
    ofn.lpstrFilter = ModuleFileFilter();
    ofn.lpstrFile = buf;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    const bool ok = GetOpenFileNameW(&ofn) != FALSE;
    // ダイアログ表示中に離されたキーが押されたままにならないようにする
    keyboard_init();
    if (ok) load_song(buf);
}

// ---------------------------------------------------------------------------
// ダイアログ・進捗表示の補助 (txtscrio.pas / adt2unit.pas / adt2extn.pas)
// ---------------------------------------------------------------------------

// ダイアログを閉じるとき、背景のウィンドウを描き直してから画面へ戻す (txtscrio.pas の move2screen)
void move2screen()
{
    HideCursor();
    const tSCREEN_MEM_PTR screen_ptr_backup = screen_ptr;
    screen_ptr = move_to_screen_data;
    area_x1 = 0;
    area_y1 = 0;
    area_x2 = 0;
    area_y2 = 0;
    scroll_pos0 = BYTE_NULL;
    scroll_pos1 = BYTE_NULL;
    scroll_pos2 = BYTE_NULL;
    scroll_pos3 = BYTE_NULL;
    scroll_pos4 = BYTE_NULL;
    {
        PlayerLock lock;
        PATTERN_ORDER_page_refresh(pattord_page);
        PATTERN_page_refresh(pattern_page);
        status_refresh();
        decay_bars_refresh();
    }
    ScreenMemCopy(screen_ptr, screen_ptr_backup);
    screen_ptr = screen_ptr_backup;
    SetCursor(cursor_backup);
}

void centered_frame(uint8_t& xstart, uint8_t& ystart, uint8_t hsize, uint8_t vsize, const std::string& name,
                    uint8_t atr1, uint8_t atr2, const std::string& border)
{
    xstart = B((work_maxcol - hsize) / 2);
    ystart = B((work_maxln - vsize) / 2 + (work_maxln - vsize) % 2);
    Frame(centered_frame_vdest, xstart, ystart, B(xstart + hsize), B(ystart + vsize), atr1, name, atr2, border);
}

constexpr uint32_t DWORD_NULL = 0xffffffff;

uint8_t progress_xstart = 0, progress_ystart = 0;
uint8_t progress_step = 0, progress_num_steps = 0;
uint32_t progress_value = 0;
uint8_t progress_old_value = BYTE_NULL;

int pas_round(double value) { return static_cast<int>(std::nearbyint(value)); }

// adt2extn.pas の show_progress (refresh_dif = 0 は 1 引数版)
void show_progress(uint32_t value, int refresh_dif = 0)
{
    if (progress_num_steps == 0 || progress_value == 0) return;
    if (value != DWORD_NULL) {
        int progress_new_value;
        if (progress_num_steps == 1) {
            progress_new_value = pas_round(40.0 / progress_value * value);
        } else {
            progress_new_value = pas_round(40.0 / progress_num_steps * (progress_step - 1) +
                                           40.0 / progress_num_steps / progress_value * value);
        }
        progress_new_value = std::min(progress_new_value, 40);   // Pascal の max
        const bool refresh = refresh_dif == 0 ? progress_new_value != progress_old_value
                                              : std::abs(progress_new_value - progress_old_value) >= refresh_dif ||
                                                    progress_new_value == 40;
        if (refresh) {
            progress_old_value = B(progress_new_value);
            ShowStr(screen_ptr, B(progress_xstart + 35), B(progress_ystart - 1),
                    ExpStrL(Num2str(pas_round(100.0 / 40 * progress_new_value), 10) + "%", 5, ' '),
                    dialog_background + dialog_hi_text);
            ShowCStr(screen_ptr, progress_xstart, progress_ystart,
                     "~" + ExpStrL("", B(progress_new_value), '\xDB') + "~" + ExpStrL("", B(40 - progress_new_value), '\xDB'),
                     dialog_background + dialog_prog_bar1, dialog_background + dialog_prog_bar2);
            draw_screen();
        }
    } else {
        ShowStr(screen_ptr, B(progress_xstart + 35), B(progress_ystart - 1), ExpStrL("0%", 5, ' '),
                dialog_background + dialog_hi_text);
        ShowStr(screen_ptr, progress_xstart, progress_ystart, ExpStrL("", 40, '\xDB'), dialog_background + dialog_prog_bar1);
        draw_screen();
    }
}

// ---------------------------------------------------------------------------
// 終了確認・NUKE (adt2extn.pas)
// ---------------------------------------------------------------------------

// 終了するなら true を返す (オリジナルは fkey を kESC にする)
bool QUIT_request()
{
    if (_force_program_quit) return true;

    const uint8_t temp = Dialog("...AND YOU WiLL KNOW MY NAME iS THE LORD, WHEN i LAY$"
                                "MY VENGEANCE UPON THEE...$",
                                "~Q~UiT$~O~OOPS$",
                                " EZECHiEL 25:17 ", 1);

    if (dl_environment.keystroke != kESC && temp == 1) {
        _force_program_quit = true;
        return true;
    }
    return false;
}

uint8_t clearpos = 1;

// 4OP の楽器の名前の先頭の印を付け直す (instedit.inc の update_4op_flag_marks)
void update_4op_flag_marks()
{
    FixedSongData& sd = songdata();
    const auto check_4op_flag = [&sd](int ins) {
        for (int idx = 1; idx <= sd.ins_4op_flags.num_4op && idx <= 128; ++idx) {
            if (sd.ins_4op_flags.idx_4op[idx - 1] == ins) return true;
        }
        return false;
    };
    const auto is_flag_chr = [](char c) { return c == '\xAC' || c == '\xAD'; };

    for (int temp = 1; temp <= 254; ++temp) {
        if (!check_4op_flag(temp)) {
            auto& n1 = at1(sd.instr_names, temp);
            auto& n2 = at1(sd.instr_names, temp + 1);
            if (is_flag_chr(n1.data[0])) n1.data[0] = ' ';
            if (is_flag_chr(n2.data[0])) n2.data[0] = ' ';
        }
    }
    for (int temp = 1; temp <= 254; ++temp) {
        if (check_4op_flag(temp)) {
            auto& n1 = at1(sd.instr_names, temp);
            auto& n2 = at1(sd.instr_names, temp + 1);
            if (n1.data[0] == ' ') n1.data[0] = '\xAC';
            if (n2.data[0] == ' ') n2.data[0] = '\xAD';
        }
    }
}

// F4 / Ctrl+A
// オリジナルの項目のうち、未実装の画面 (パターン一覧・マクロエディタなど) の位置の初期化は、
// その画面の実装とともに追加する。
void NUKE(uint8_t spos)
{
    const uint8_t temp1 = Dialog("SO YOU THiNK iT REALLY SUCKS, DON'T YOU?$"
                                 "WHAT DO YOU WANT TO BE NUKED?$",
                                 "~O~RDER$~P~ATTERNS$iNSTR [$~N~AMES$~R~EGS$~M~ACROS$]$ARP/~V~iB$~A~LL$",
                                 " NUKE'M ", spos);

    clearpos = temp1;
    if (dl_environment.keystroke == kESC) return;

    if (temp1 == 9) {
        fade_out_playback(false);
        PlayerLock lock;
        A2Player& p = P();
        stop_playing();
        p.tempo = Song::Defaults().tempo;
        p.speed = Song::Defaults().speed;
        p.mark_line = 4;
        p.IRQ_freq_shift = 0;
        p.playback_speed_shift = 0;

        // init_songdata (曲名は 'noname.' になる)
        auto song = std::make_shared<Song>();
        song->init(Song::Defaults());
        p.setSong(song);

        POSITIONS_reset();
        std::fill(std::begin(p.channel_flag), std::end(p.channel_flag), true);
        track_notes = false;
        current_inst = 1;
        module_archived = true;
        return;
    }

    PlayerLock lock;
    FixedSongData& sd = songdata();
    switch (temp1) {
    case 1:
        std::memset(sd.pattern_order, 0x80, sizeof(sd.pattern_order));
        PATTERN_ORDER_page_refresh(pattord_page);
        break;

    case 2:
        std::memset(&P().song()->patterns(), 0, sizeof(PatternData));
        PATTERN_ORDER_page_refresh(pattord_page);
        PATTERN_page_refresh(pattern_page);
        for (int temp2 = 0; temp2 <= 0x7f; ++temp2) {
            sd.pattern_names[temp2].assign(" PAT_" + byte2hex(B(temp2)) + "  \xF7 ");
        }
        break;

    case 4:
        for (int temp2 = 1; temp2 <= 255; ++temp2) {
            auto& name = at1(sd.instr_names, temp2);
            const char mark = name.data[0];
            std::memset(name.data + 1, 0, sizeof(name.data) - 1);
            name.assign(std::string(1, mark) + "iNS_" + byte2hex(B(temp2)) + "\xF7 ");
        }
        break;

    case 5:
        std::memset(sd.instr_data, 0, sizeof(sd.instr_data));
        std::memset(&sd.ins_4op_flags, 0, sizeof(sd.ins_4op_flags));
        update_4op_flag_marks();
        break;

    case 6:
        std::memset(sd.instr_macros, 0, sizeof(sd.instr_macros));
        break;

    case 8:
        std::memset(sd.macro_table, 0, sizeof(sd.macro_table));
        break;

    default:
        break;
    }
    module_archived = false;
}

// ---------------------------------------------------------------------------
// 保存 (adt2extn.pas の FILE_save / _a2m_saver / _a2t_saver / _a2p_saver)
// ---------------------------------------------------------------------------

std::wstring replace_ext(const std::wstring& path, const wchar_t* ext)
{
    std::filesystem::path p(path);
    p.replace_extension(ext);
    return p.wstring();
}

// 保存先をコモンダイアログで選ぶ (本クローン版の独自 UI)。キャンセルされたら空を返す。
std::wstring choose_save_path(const wchar_t* ext)
{
    std::wstring filter;
    const wchar_t* title;
    if (std::wcscmp(ext, L"a2m") == 0) {
        filter = L"AdLib Tracker II モジュール (*.a2m)";
        title = L"A2M 形式で保存";
    } else if (std::wcscmp(ext, L"a2t") == 0) {
        filter = L"AdLib Tracker II タイニーモジュール (*.a2t)";
        title = L"A2T 形式で保存";
    } else {
        filter = L"AdLib Tracker II パターン (*.a2p)";
        title = L"パターンを A2P 形式で保存";
    }
    filter += L'\0';
    filter += L"*.";
    filter += ext;
    filter += L'\0';
    filter += L'\0';

    WCHAR buf[MAX_PATH] = L"";
    std::wstring dir;
    if (!songdata_source.empty()) {
        const std::filesystem::path p(replace_ext(songdata_source, ext));
        wcsncpy_s(buf, p.filename().c_str(), _TRUNCATE);
        dir = p.parent_path().wstring();
    }

    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwnd;
    ofn.lpstrFilter = filter.c_str();
    ofn.lpstrFile = buf;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrInitialDir = dir.empty() ? nullptr : dir.c_str();
    ofn.lpstrTitle = title;
    ofn.lpstrDefExt = ext;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    const bool ok = GetSaveFileNameW(&ofn) != FALSE;
    // ダイアログ表示中に離されたキーが押されたままにならないようにする
    keyboard_init();
    if (!ok) return std::wstring();
    // オリジナルと同じく拡張子は ext にする
    return replace_ext(buf, ext);
}

SongSaveState song_save_state()
{
    const A2Player& p = P();
    SongSaveState state;
    state.common_flag = B(int(p.speed_update) + (int(p.lockvol) << 1) + (int(p.lockVP) << 2) + (p.tremolo_depth << 3) +
                          (p.vibrato_depth << 4) + (int(p.panlock) << 5) + (int(p.percussion_mode) << 6) +
                          (int(p.volume_scaling) << 7));
    state.mark_line = p.mark_line;
    state.tempo_finetune = p.IRQ_freq_shift;
    return state;
}

// 保存の本体。保存中も演奏を続けられるよう、曲の複製を保存してから曲データへ書き戻す。
void save_song(const std::wstring& path, const wchar_t* ext)
{
    const bool is_a2p = std::wcscmp(ext, L"a2p") == 0;
    const bool is_a2t = std::wcscmp(ext, L"a2t") == 0;
    const std::string saver_title = is_a2p ? " A2P SAVER " : (is_a2t ? " A2T SAVER " : " A2M SAVER ");

    auto copy = std::make_unique<Song>();
    SongSaveState state;
    uint8_t pattern;
    {
        PlayerLock lock;
        copy->data() = songdata();
        copy->patterns() = P().song()->patterns();
        copy->title = P().song()->title;
        state = song_save_state();
        pattern = pattern_patt;
    }

    uint8_t xstart = 0, ystart = 0;
    bool window_shown = false;
    bool new_block = true;
    SaveProgress progress;
    progress.begin = [&](uint8_t num_steps) {
        ScreenMemCopy(screen_ptr, screen_backup);
        centered_frame_vdest = screen_ptr;
        HideCursor();
        centered_frame(xstart, ystart, 43, 3, is_a2t ? " A2T FiLE " : " A2M FiLE ",
                       dialog_background + dialog_border, dialog_background + dialog_title, frame_double);
        window_shown = true;
        progress_xstart = B(xstart + 2);
        progress_ystart = B(ystart + 2);
        progress_step = 1;
        progress_num_steps = num_steps;
        ShowCStr(screen_ptr, B(xstart + 2), B(ystart + 1),
                 is_a2t ? "COMPRESSiNG TiNY MODULE DATA..." : "COMPRESSiNG MODULE DATA...",
                 dialog_background + dialog_text, dialog_background + dialog_hi_text);
        show_progress(DWORD_NULL);
    };
    progress.step = [&]() {
        ++progress_step;
        new_block = true;
    };
    progress.value = [&](uint32_t done, uint32_t total) {
        // LZH_compress の開始時の progress_old_value := BYTE_NULL; progress_value := size
        if (new_block) {
            progress_old_value = BYTE_NULL;
            new_block = false;
        }
        progress_value = total;
        show_progress(done, 3);
    };

    const auto restore = [&]() {
        if (!window_shown) return;
        move_to_screen_data = screen_backup;
        move_to_screen_area[1] = xstart;
        move_to_screen_area[2] = ystart;
        move_to_screen_area[3] = B(xstart + 43 + 2 + 1);
        move_to_screen_area[4] = B(ystart + 3 + 1);
        move2screen();
        progress_num_steps = 0;
        progress_value = 0;
    };

    SaveResult result;
    if (is_a2p) result = SaveA2p(path, *copy, pattern);
    else if (is_a2t) result = SaveA2t(path, *copy, state, &progress);
    else result = SaveA2m(path, *copy, state, &progress);
    restore();

    if (result == SaveResult::WriteError) {
        Dialog("ERROR WRiTiNG DATA - DiSK ERROR?$SAViNG STOPPED$", "~O~KAY$", saver_title, 1);
        return;
    }

    {
        // 保存の際に書き換えた曲データ (名前の未使用部分の 0 埋め、common_flag、bpm_data) を戻す
        PlayerLock lock;
        songdata() = copy->data();
        if (result == SaveResult::Ok && !is_a2p) {
            P().song()->title = std::filesystem::path(path).filename().wstring();
            module_archived = true;
        }
    }

    if (result == SaveResult::DiskFull) {
        Dialog("ERROR WRiTiNG DATA - DiSK FULL?$SAViNG STOPPED$", "~O~KAY$", saver_title, 1);
    }
}

// F2 / Ctrl+F2 / Alt+F2 (quick: Shift+F2 は前回のファイル名でそのまま保存する)
void FILE_save(const wchar_t* ext, bool quick)
{
    std::wstring path;
    if (quick && std::wcscmp(ext, L"a2p") != 0 && !songdata_source.empty()) {
        path = replace_ext(songdata_source, ext);
    } else {
        path = choose_save_path(ext);
        if (path.empty()) return;
    }
    songdata_source = path;
    save_song(path, ext);
}

// 単独パターン再生中のデバッグ (トレース中の一時停止) を解除する (ipattern.inc の pause_debugging)
void pause_debugging()
{
    PlayerLock lock;
    A2Player& p = P();
    if (p.debugging) {
        p.debugging = false;
        p.replay_forbidden = true;
        p.play_status = PlayStatus::Paused;
        PATTERN_ORDER_page_refresh(pattord_page);
        PATTERN_page_refresh(pattern_page);
    }
}

// 保存・NUKE のキー (停止中・トレース中で共通)。処理したら true を返す。
bool process_file_keys(uint16_t fkey, bool in_trace)
{
    switch (fkey) {
    case kF2:
    case kShF2:
    case kCtrlS:
        if (in_trace) pause_debugging();
        FILE_save(L"a2m", fkey == kShF2);
        if (in_trace) keyboard_reset_buffer();
        return true;

    case kCtrlF2:
        if (in_trace) pause_debugging();
        FILE_save(L"a2t", false);
        if (in_trace) keyboard_reset_buffer();
        return true;

    case kAltF2:
        // トレース中はオリジナルでも受け付けない
        if (in_trace) return false;
        FILE_save(L"a2p", false);
        return true;

    case kF4:
    case kCtrlA:
        NUKE(clearpos);
        return true;

    default:
        return false;
    }
}

void change_screen_mode(uint8_t mode)
{
    if (mode == program_screen_mode) return;
    sys_set_screen_mode(mode);
    init_screen_layout();
    PROGRAM_SCREEN_init();

    PlayerLock lock;
    if (pattord_hpos > MAX_ORDER_COLS) pattord_hpos = MAX_ORDER_COLS;
    if (pattord_page > 23 - (MAX_ORDER_COLS - 9)) pattord_page = B(23 - (MAX_ORDER_COLS - 9));
    _traceprc_last_order = BYTE_NULL;
    _traceprc_last_pattern = BYTE_NULL;
    _traceprc_last_line = BYTE_NULL;
    PATTERN_ORDER_page_refresh(pattord_page);
    PATTERN_page_refresh(pattern_page);
    decay_bars_refresh();
    status_refresh();
}

void change_emulator(EmulatorType type)
{
    fade_out_playback(false);
    // プレイヤーが作り直されるため、曲は引き継がれるが演奏は停止する
    sound_set_emulator(type);
    refresh_windows();
}

// メニューのコマンドとドロップされたファイルを処理する。
// キー操作と同じ扱いにするコマンドはそのキーコードを返す (なければ WORD_NULL)。
uint16_t process_events()
{
    int id;
    while (sys_take_command(id)) {
        switch (id) {
        case IDM_FILE_NEW: NUKE(9); break;   // NUKE'M の ALL を選んだ状態で開く
        case IDM_FILE_OPEN: return kF3;
        case IDM_FILE_SAVE: return kShF2;
        case IDM_FILE_SAVEAS: return kF2;
        case IDM_FILE_SAVE_A2T: return kCtrlF2;
        case IDM_FILE_SAVE_A2P: return kAltF2;
        case IDM_PLAY_PLAY: return kF5;
        case IDM_PLAY_PAUSE: return kF6;
        case IDM_PLAY_STOP: return kF7;
        case IDM_EXIT: return kF10;
        case IDM_VIEW_MODE0: change_screen_mode(0); break;
        case IDM_VIEW_MODE1: change_screen_mode(1); break;
        case IDM_VIEW_MODE2: change_screen_mode(2); break;
        case IDM_EMU_AT2: change_emulator(EmulatorType::At2); break;
        case IDM_EMU_NUKED: change_emulator(EmulatorType::Nuked); break;
        case IDM_EMU_HARDWARE: change_emulator(EmulatorType::Hardware); break;
        default: break;
        }
    }

    std::wstring path;
    if (sys_take_dropped_file(path)) load_song(path);
    return WORD_NULL;
}

// トラックの ON/OFF (Alt+1..0、Shift で 11..20)
void toggle_channel(uint8_t temp)
{
    A2Player& p = P();
    p.channel_flag[temp] = !p.channel_flag[temp];
    if (!p.channel_flag[temp]) p.reset_chan_data(temp);
    if (p.is_4op_chan(temp)) {
        const uint8_t pair = in_4op_tracks_hi(temp) ? B(temp + 1) : B(temp - 1);
        p.channel_flag[pair] = p.channel_flag[temp];
        if (!p.channel_flag[pair]) p.reset_chan_data(pair);
    }
}

uint8_t chpos = 1;

// トラック 10..20 の ON/OFF を選ぶダイアログ (Alt+0 で表示中のトラックに 10 がない場合など)
void track_onoff_dialog()
{
    std::string temps;
    {
        PlayerLock lock;
        if (!P().percussion_mode) temps = "1~0~$1~1~$1~2~$1~3~$1~4~$1~5~$1~6~$1~7~$1~8~$1~9~$2~0~$";
        else temps = "1~0~$1~1~$1~2~$1~3~$1~4~$1~5~$16 ~B~D$17 ~S~D$18 ~T~T$19 T~C~$20 ~H~H$";
        temps = FlipStr(temps);
        for (int temp = 10; temp <= 20; ++temp) {
            if (temp > songdata().nm_tracks) {
                for (int i = 0; i < 2; ++i) {
                    const size_t p = temps.find('~');
                    if (p != std::string::npos) temps.erase(p, 1);
                }
            }
        }
        temps = FlipStr(temps);
    }
    if (temps.find('~') == std::string::npos) return;

    chpos = Dialog("USE CURSOR KEYS OR DiRECTLY PRESS HOTKEY TO TOGGLE TRACK ON/OFF$", temps, " TRACK ON/OFF ", chpos);
    if (dl_environment.keystroke == kESC) return;

    PlayerLock lock;
    A2Player& p = P();
    const uint8_t chan = B(9 + chpos);
    p.channel_flag[chan] = !p.channel_flag[chan];
    if (!p.channel_flag[chan]) p.reset_chan_data(chan);
    if (p.is_4op_chan(chan)) {
        if (chan == 10 || chan == 12 || chan == 14) {
            p.channel_flag[chan + 1] = p.channel_flag[chan];
            if (!p.channel_flag[chan + 1]) p.reset_chan_data(B(chan + 1));
        } else if (chan == 11 || chan == 13 || chan == 15) {
            p.channel_flag[chan - 1] = p.channel_flag[chan];
            if (!p.channel_flag[chan - 1]) p.reset_chan_data(B(chan - 1));
        }
    }
}

// Alt+0 でトラックを選ぶダイアログが必要になった (ロックを外してから track_onoff_dialog を呼ぶ)
bool pending_track_dialog = false;

// トレース中・編集中で共通のトラック操作キー
// トラックの ON/OFF のキー。該当するキーなら true を返す。
bool process_channel_keys(uint16_t fkey)
{
    A2Player& p = P();
    FixedSongData& sd = songdata();
    switch (fkey) {
    case kAstrsk:
    case kNPastr:
        for (int temp = 1; temp <= sd.nm_tracks; ++temp) {
            p.channel_flag[temp] = !p.channel_flag[temp];
            if (!p.channel_flag[temp]) p.reset_chan_data(B(temp));
        }
        break;

    case kAltS:
        if (!track_notes) {
            for (int temp = 1; temp <= sd.nm_tracks; ++temp) p.channel_flag[temp] = false;
            for (int temp = 1; temp <= sd.nm_tracks; ++temp) {
                if (temp == count_channel(pattern_hpos)) {
                    p.channel_flag[temp] = true;
                    if (p.is_4op_chan(B(temp))) {
                        if (in_4op_tracks_hi(B(temp))) p.channel_flag[temp + 1] = true;
                        else p.channel_flag[temp - 1] = true;
                    }
                }
            }
            for (int temp = 1; temp <= sd.nm_tracks; ++temp) {
                if (!p.channel_flag[temp]) p.reset_chan_data(B(temp));
            }
        }
        break;

    case kAltR:
        for (int temp = 1; temp <= sd.nm_tracks && temp <= 20; ++temp) p.channel_flag[temp] = true;
        break;

    default:
        if (fkey >= kAlt1 && fkey <= kAlt0 && (fkey & 0xff) == 0) {
            if (fkey != kAlt0) {
                const uint8_t temp = shift_pressed() ? B((fkey >> 8) - 0x77 + 10) : B((fkey >> 8) - 0x77);
                if (temp <= sd.nm_tracks) toggle_channel(temp);
            } else if (shift_pressed() || (10 >= chan_pos && 10 <= chan_pos + MAX_TRACKS - 1) || sd.nm_tracks == 10) {
                toggle_channel(10);
            } else {
                // それ以外はトラックを選ぶダイアログを出す (ロックを外して呼ぶ)
                pending_track_dialog = true;
            }
            return true;
        }
        return false;
    }
    return true;
}

// Ctrl+ENTER: 次のオーダーへ進む (単独パターン再生中はパターンの先頭へ戻る)。ロックを外して呼ぶ。
void jump_to_next_order()
{
    {
        PlayerLock lock;
        A2Player& p = P();
        if (p.play_single_patt) {
            p.current_line = 0;
            PATTERN_ORDER_page_refresh(0);
            PATTERN_page_refresh(0);
            return;
        }
    }

    no_status_refresh = true;
    fade_out_playback(false);
    {
        PlayerLock lock;
        A2Player& p = P();
        if (p.current_order < 0x7f && p.play_status != PlayStatus::Stopped) {
            if (p.order_at(B(p.current_order + 1)) < 0x80) {
                calibrate_player(B(p.current_order + 1), 0, false);
            } else if (p.calc_following_order(B(p.current_order + 1)) != -1) {
                calibrate_player(B(p.calc_following_order(B(p.current_order + 1))), 0, false);
            }
        } else if (p.calc_following_order(0) != -1) {
            calibrate_player(B(p.calc_following_order(0)), 0, false);
        }
    }
    no_status_refresh = false;
}

// ---------------------------------------------------------------------------
// デバッグ情報 (adt2extn.pas の DEBUG_INFO)
// ---------------------------------------------------------------------------

// 配色 (typcons1.inc の既定値)
constexpr uint8_t debug_info_bckg = 0x40;
constexpr uint8_t debug_info_bckg2 = 0x90;
constexpr uint8_t debug_info_border = 0x05;
constexpr uint8_t debug_info_border2 = 0x0f;
constexpr uint8_t debug_info_title = 0x0b;
constexpr uint8_t debug_info_topic = 0x0e;
constexpr uint8_t debug_info_txt = 0x07;
constexpr uint8_t debug_info_hi_txt = 0x0f;
constexpr uint8_t debug_info_txt_hid = 0x06;
constexpr uint8_t debug_info_mod = 0x02;
constexpr uint8_t debug_info_hi_mod = 0x0a;
constexpr uint8_t debug_info_car = 0x03;
constexpr uint8_t debug_info_hi_car = 0x0b;
constexpr uint8_t debug_info_perc = 0x08;
constexpr uint8_t debug_info_bpm = 0x08;

// typcons2.inc
const char* const debug_win_str1[3] = {
    "     TRACK     ~\xB3~          iNSTRUMENT          ~\xB3~NOTE~\xB3 ~FX N\xF9" "1~ \xB3 ~FX N\xF9" "2~ \xB3~FREQ~\xB3 ~VOL",
    "\xC4\xC4\xC2\xC4\xC4\xC2\xC4\xC4\xC4\xC2\xC4\xC4\xC2\xC4\xC4\xC5\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC5\xC4\xC4\xC4\xC4\xC5\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC5\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC5\xC4\xC4\xC4\xC4\xC5\xC4\xC4\xC2\xC4\xC4",
    "\xC4\xC4\xC1\xC4\xC4\xC1\xC4\xC4\xC4\xC1\xC4\xC4\xC1\xC4\xC4\xC1\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC1\xC4\xC4\xC4\xC4\xC1\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC1\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC1\xC4\xC4\xC4\xC4\xC1\xC4\xC4\xC1\xC4\xC4",
};
const char* const debug_win_str2[3] = {
    "TRACK~\xB3~iNS~\xB3~NOTE~\xB3 ~FX N\xF9" "1~ \xB3 ~FX N\xF9" "2~ \xB3~FREQ~\xB3~CN/FB/ADSR/WF/KSL/MUL/TRM/ViB/KSR/EG~\xB3 ~VOL",
    "\xC4\xC4\xC2\xC4\xC4\xC5\xC4\xC4\xC4\xC5\xC4\xC4\xC4\xC4\xC5\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC5\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC5\xC4\xC4\xC4\xC4\xC5\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC2\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC5\xC4\xC4\xC2\xC4\xC4",
    "\xC4\xC4\xC1\xC4\xC4\xC1\xC4\xC4\xC4\xC1\xC4\xC4\xC4\xC4\xC1\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC1\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC1\xC4\xC4\xC4\xC4\xC1\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC1\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC1\xC4\xC4\xC1\xC4\xC4",
};
const char* const debug_win_str3[3] = {
    "TRACK~\xB3~iNS~\xB3~NOTE~\xB3 ~FX N\xF9" "1~ \xB3 ~FX N\xF9" "2~ \xB3~MACRO FM-REG~ \xB3~MACRO ARPG~\xB3~MACRO ViBR ~\xB3~FREQ~\xB3 ~VOL",
    "\xC4\xC4\xC2\xC4\xC4\xC5\xC4\xC4\xC4\xC5\xC4\xC4\xC4\xC4\xC5\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC5\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC5\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC5\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC5\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC5\xC4\xC4\xC4\xC4\xC5\xC4\xC4\xC2\xC4\xC4",
    "\xC4\xC4\xC1\xC4\xC4\xC1\xC4\xC4\xC4\xC1\xC4\xC4\xC4\xC4\xC1\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC1\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC1\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC1\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC1\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC1\xC4\xC4\xC4\xC4\xC1\xC4\xC4\xC1\xC4\xC4",
};

// typcons1.inc
const char* const macro_retrig_str[8] = { "\xFA", "\x9F", "\x0B", "\xFA", "\x0D", "\xFA", "\xFA", "\xFA" };

// adt2extn.pas (DEBUG_INFO の前の定数)
const char _perc_char[5] = { '\xA0', '\xA1', '\xA2', '\xA3', '\xA4' };   // [1..5]
const char* const _panning_str[4] = { "\x15\x16", "\x15`\x16`", "`\x15`\x16", "`\x15\x16`" };
const char* const _connection[2] = { "FM", "AM" };
const char _off_on[4][2] = { { '\xFA', 'T' }, { '\xFA', 'V' }, { '\xFA', 'K' }, { '\xFA', 'S' } };   // [1..4, 0..1]
const char* const _win_title[2] = { " DEBUG iNFO ", "" };
const std::string _contxt_str = " LSHiFT/RSHiFT \xC4\x10 TOGGLE DETAiLS ";

// ?debug_info?details_flag (0: 概要, 1: 詳細, 2: マクロの詳細)
uint8_t debug_info_details_flag = 0;

const std::string NOFX = "        ";

std::string effect_str(uint8_t effect_def, uint8_t effect, uint8_t effect_def2)
{
    const bool up = effect / 16 != 0;
    switch (effect_def) {
    case ef_Arpeggio: return effect != 0 ? "Arpeggio" : NOFX;
    case ef_VolSlide: return up ? "VolSld\x18 " : "VolSld\x19 ";
    case ef_VolSlideFine: return up ? "VolSld\x12 " : "VolSld\x17 ";
    case ef_TPortamVolSlide: return up ? "Por\x0DVSl\x18" : "Por\x0DVSl\x19";
    case ef_VibratoVolSlide: return up ? "VibrVSl\x18" : "VibrVSl\x19";
    case ef_TPortamVSlideFine: return up ? "Por\x0DVSl\x12" : "Por\x0DVSl\x17";
    case ef_VibratoVSlideFine: return up ? "VibrVSl\x12" : "VibrVSl\x17";
    case ef_ArpggVSlide: return up ? "ArpgVSl\x18" : "ArpgVSl\x19";
    case ef_ArpggVSlideFine: return up ? "ArpgVSl\x12" : "ArpgVSl\x17";
    case ef_SetWaveform:
        if (!(effect % 16 <= 7)) return "SetW\x1A" "Car";
        if (!(effect / 16 <= 7)) return "SetW\x1AMod";
        return "SetWform";
    case ef_FSlideUpVSlide: return up ? "Por\x18VSl\x18" : "Por\x18VSl\x19";
    case ef_FSlUpVSlF: return up ? "Por\x18VSl\x12" : "Por\x18VSl\x17";
    case ef_FSlideDownVSlide: return up ? "Por\x19VSl\x18" : "Por\x19VSl\x19";
    case ef_FSlDownVSlF: return up ? "Por\x19VSl\x12" : "Por\x19VSl\x17";
    case ef_FSlUpFineVSlide: return up ? "Por\x12VSl\x18" : "Por\x12VSl\x19";
    case ef_FSlUpFineVSlF: return up ? "Por\x12VSl\x12" : "Por\x12VSl\x17";
    case ef_FSlDownFineVSlide: return up ? "Por\x17VSl\x18" : "Por\x17VSl\x19";
    case ef_FSlDownFineVSlF: return up ? "Por\x17VSl\x12" : "Por\x17VSl\x17";
    case ef_FSlideUp: return "Porta\x18  ";
    case ef_FSlideDown: return "Porta\x19  ";
    case ef_TonePortamento: return "Porta\x0D  ";
    case ef_Vibrato: return "Vibrato ";
    case ef_FSlideUpFine: return "Porta\x12  ";
    case ef_FSlideDownFine: return "Porta\x17  ";
    case ef_SetCarrierVol: return "SetCVol ";
    case ef_SetModulatorVol: return "SetMVol ";
    case ef_PositionJump: return "PosJump ";
    case ef_SetInsVolume: return "SetVol  ";
    case ef_PatternBreak: return "PatBreak";
    case ef_SetTempo: return "SetTempo";
    case ef_SetSpeed: return "SetSpeed";
    case ef_RetrigNote: return "Retrig\x0D ";
    case ef_MultiRetrigNote: return "MulRetr\x0D";
    case ef_Tremolo: return "Tremolo ";
    case ef_Tremor: return "Tremor  ";
    case ef_SetGlobalVolume: return "SetGlVol";
    case ef_ForceInsVolume: return "ForceVol";

    case ef_Extended:
        switch (effect / 16) {
        case ef_ex_SetTremDepth: return "SetTremD";
        case ef_ex_SetVibDepth: return "SetVibrD";
        case ef_ex_SetAttckRateM: return "[A]DSR\x1AM";
        case ef_ex_SetDecayRateM: return "A[D]SR\x1AM";
        case ef_ex_SetSustnLevelM: return "AD[S]R\x1AM";
        case ef_ex_SetRelRateM: return "ADS[R]\x1AM";
        case ef_ex_SetAttckRateC: return "[A]DSR\x1A" "C";
        case ef_ex_SetDecayRateC: return "A[D]SR\x1A" "C";
        case ef_ex_SetSustnLevelC: return "AD[S]R\x1A" "C";
        case ef_ex_SetRelRateC: return "ADS[R]\x1A" "C";
        case ef_ex_SetFeedback: return "SetFeedb";
        case ef_ex_PatternLoop: return "PatLoop ";
        case ef_ex_PatternLoopRec: return "PatLoopR";
        case ef_ex_SetPanningPos:
            switch (effect % 16) {
            case 0: return "SetPan\x1A" "C";
            case 1: return "SetPan\x1AL";
            case 2: return "SetPan\x1AR";
            default: return NOFX;   // オリジナルは結果が未定義
            }
        case ef_ex_ExtendedCmd:
            switch (effect % 16) {
            case ef_ex_cmd_MKOffLoopDi: return "\x0CLoopOff";
            case ef_ex_cmd_MKOffLoopEn: return "\x0CLoopOn ";
            case ef_ex_cmd_TPortaFKdis: return "Porta\x0DK-";
            case ef_ex_cmd_TPortaFKenb: return "Porta\x0DK+";
            case ef_ex_cmd_RestartEnv: return "RstrtEnv";
            case ef_ex_cmd_4opVlockOff: return "VLock\x04\x03-";
            case ef_ex_cmd_4opVlockOn: return "VLock\x04\x03+";
            case ef_ex_cmd_ForceBpmSld: return "BpmSlide";
            default: return NOFX;   // オリジナルは結果が未定義
            }
        case ef_ex_ExtendedCmd2:
            switch (effect % 16) {
            case ef_ex_cmd2_RSS: return "RelSS   ";
            case ef_ex_cmd2_ResetVol: return "ResetVol";
            case ef_ex_cmd2_LockVol: return "VolLock+";
            case ef_ex_cmd2_UnlockVol: return "VolLock-";
            case ef_ex_cmd2_LockVP: return "LockVP+ ";
            case ef_ex_cmd2_UnlockVP: return "LockVP- ";
            case ef_ex_cmd2_VSlide_car: return "VSld\x1A" "Car";
            case ef_ex_cmd2_VSlide_mod: return "VSld\x1AMod";
            case ef_ex_cmd2_VSlide_def: return "VSld\x1A" "Def";
            case ef_ex_cmd2_LockPan: return "PanLock+";
            case ef_ex_cmd2_UnlockPan: return "PanLock-";
            case ef_ex_cmd2_VibrOff: return "VibrOff ";
            case ef_ex_cmd2_TremOff: return "TremOff ";
            case ef_ex_cmd2_FVib_FGFS:
                return (effect_def2 != ef_GlobalFSlideUp && effect_def2 != ef_GlobalFSlideDown) ? "VibrFine" : "GlPortaF";
            case ef_ex_cmd2_FTrm_XFGFS:
                return (effect_def2 != ef_GlobalFSlideUp && effect_def2 != ef_GlobalFSlideDown) ? "TremFine" : "GlPortXF";
            case ef_ex_cmd2_NoRestart: return "ArpVibNR";
            default: return NOFX;
            }
        default: return NOFX;
        }

    case ef_Extended2:
        switch (effect / 16) {
        case ef_ex2_PatDelayFrame: return "PatDelF ";
        case ef_ex2_PatDelayRow: return "PatDelR ";
        case ef_ex2_NoteDelay: return "Delay\x0D  ";
        case ef_ex2_NoteCut: return "Cut\x0D    ";
        case ef_ex2_GlVolSlideUp: return "GlVolSl\x18";
        case ef_ex2_GlVolSlideDn: return "GlVolSl\x19";
        case ef_ex2_GlVolSlideUpF: return "GlVolSl\x12";
        case ef_ex2_GlVolSlideDnF: return "GlVolSl\x17";
        case ef_ex2_FineTuneUp: return "FTune\x18  ";
        case ef_ex2_FineTuneDown: return "FTune\x19  ";
        case ef_ex2_GlVolSldUpXF: return "GVolSl\x0C\x12";
        case ef_ex2_GlVolSldDnXF: return "GVolSl\x0C\x17";
        case ef_ex2_VolSlideUpXF: return "VolSld\x0C\x12";
        case ef_ex2_VolSlideDnXF: return "VolSld\x0C\x17";
        case ef_ex2_FreqSlideUpXF: return "Porta\x0C\x12 ";
        case ef_ex2_FreqSlideDnXF: return "Porta\x0C\x17 ";
        default: return NOFX;
        }

    case ef_SwapArpeggio: return "ArpT\x1A" + byte2hex(effect) + " ";
    case ef_SwapVibrato: return "VibT\x1A" + byte2hex(effect) + " ";

    case ef_Extended3:
        switch (effect / 16) {
        case ef_ex3_SetConnection: return effect % 16 == 0 ? "Conct\x1A" "FM" : "Conct\x1A" "AM";
        case ef_ex3_SetMultipM: return "Multip\x1AM";
        case ef_ex3_SetKslM: return "KSL\x1AM   ";
        case ef_ex3_SetTremoloM: return "Trem\x1AM  ";
        case ef_ex3_SetVibratoM: return "Vibr\x1AM  ";
        case ef_ex3_SetKsrM: return "KSR\x1AM   ";
        case ef_ex3_SetSustainM: return "Sustn\x1AM ";
        case ef_ex3_SetMultipC: return "Multip\x1A" "C";
        case ef_ex3_SetKslC: return "KSL\x1A" "C   ";
        case ef_ex3_SetTremoloC: return "Trem\x1A" "C  ";
        case ef_ex3_SetVibratoC: return "Vibr\x1A" "C  ";
        case ef_ex3_SetKsrC: return "KSR\x1A" "C   ";
        case ef_ex3_SetSustainC: return "Sustn\x1A" "C ";
        default: return NOFX;   // オリジナルは結果が未定義
        }

    case ef_ExtraFineArpeggio: return "Arpggio\x0C";
    case ef_ExtraFineVibrato: return "Vibrato\x0C";
    case ef_ExtraFineTremolo: return "Tremolo\x0C";
    case ef_SetCustomSpeedTab: return "SetCusST";
    case ef_GlobalFSlideUp: return "GlPorta\x18";
    case ef_GlobalFSlideDown: return "GlPorta\x19";
    default: return NOFX;
    }
}

// note_layout の範囲外 (キーオフ 0xFF など) はオリジナルでは配列の後ろのメモリを読むが、ここでは空欄にする
std::string note_layout_at(int index)
{
    return (index >= 0 && index <= 12 * 8 + 1) ? std::string(note_layout[index]) : std::string("   ");
}

std::string note_str(uint8_t note)
{
    if (note < 100) return note_layout_at(note) + " ";
    if ((note & 0x7f) != 0) return note_layout_at(note & 0x7f) + "\x0C";
    return note_layout_at(0) + " ";
}

std::string cstr2str(std::string str)
{
    const size_t n = str.size();
    for (size_t temp = 1; temp <= n; ++temp) {
        if (temp <= str.size() && str[temp - 1] == '~') str.erase(temp - 1, 1);
    }
    return str;
}

std::string _macro_str(const std::string& str, int null_byte)
{
    if (null_byte != 0) return str;
    return ExpStrL("", C3StrLen(str), ' ');
}

std::string _freq_slide_str(int value)
{
    if (value == 0) return "`\x0A`";
    if (value > 0) return "\x18";
    return "\x19";
}

constexpr uint16_t MACRO_IDLE = 0x0fff;
const char* const note_keyoff_str[2] = { "`\x0C`", "\x0C" };

std::string _macro_pos_str_fm(uint16_t pos, uint16_t len, uint8_t keyoff_pos, uint8_t duration, uint8_t retrig_note,
                              int freq_slide)
{
    const char* keyoff = note_keyoff_str[(pos >= keyoff_pos && keyoff_pos > 0) ? 1 : 0];
    const char* retrig = macro_retrig_str[retrig_note & 7];
    if (pos <= 255) {
        return byte2hex(B(pos)) + "/" + byte2hex(B(len)) + ":" + byte2hex(duration) + " " + retrig + _freq_slide_str(freq_slide) +
               keyoff + "`\xFB`";
    }
    if (pos == MACRO_IDLE) return "\xFA\xFA\x0C\xFA\xFA:\xFA\xFA     ";
    return byte2hex(B(len)) + "/" + byte2hex(B(len)) + ":" + byte2hex(duration) + " " + retrig + _freq_slide_str(freq_slide) +
           keyoff + "\xFB";
}

std::string _macro_pos_str_av(uint16_t pos, uint16_t len, uint8_t keyoff_pos, const std::string& slide_str)
{
    const char* keyoff = note_keyoff_str[(pos >= keyoff_pos && keyoff_pos > 0) ? 1 : 0];
    if (pos <= 255) return byte2hex(B(pos)) + "/" + byte2hex(B(len)) + " " + slide_str + keyoff;
    if (pos == MACRO_IDLE) return "\xFA\xFA\x0C\xFA\xFA " + slide_str + keyoff;
    return byte2hex(B(len)) + "/" + byte2hex(B(len)) + " " + slide_str + keyoff;
}

// 曲データの中だけを読む (オリジナルは範囲外の添字でも読むが、曲データの外は 0 とする)
template <class T>
T songdata_read(uintptr_t addr)
{
    const auto lo = reinterpret_cast<uintptr_t>(&songdata());
    const uintptr_t hi = lo + sizeof(FixedSongData);
    T value{};
    if (addr >= lo && addr + sizeof(T) <= hi) std::memcpy(&value, reinterpret_cast<const void*>(addr), sizeof(T));
    return value;
}

// instr_macros[table] / instr_macros[table].data[pos] / macro_table[table] の位置 (添字は 1 始まり)
uintptr_t instr_macros_addr(int table)
{
    return reinterpret_cast<uintptr_t>(songdata().instr_macros) + static_cast<intptr_t>(table - 1) * sizeof(RegisterTable);
}

uintptr_t instr_macros_data_addr(int table, int pos)
{
    return instr_macros_addr(table) + offsetof(RegisterTable, data) + static_cast<intptr_t>(pos - 1) * sizeof(RegisterTableDef);
}

uintptr_t macro_table_addr(int table)
{
    return reinterpret_cast<uintptr_t>(songdata().macro_table) + static_cast<intptr_t>(table - 1) * sizeof(MacroTable);
}

// adt2unit.pas の calc_max_speedup
uint16_t calc_max_speedup(uint8_t tempo)
{
    if (tempo == 0) return 1;
    constexpr int MAX_IRQ_FREQ = 1000;
    uint16_t result = static_cast<uint16_t>(MAX_IRQ_FREQ / tempo);
    int32_t temp;
    do {
        if (tempo == 18 && P().timer_fix) temp = static_cast<int32_t>((tempo + 0.2) * 20);
        else temp = 250;
        while (temp % (tempo * result) != 0) ++temp;
        if (temp <= MAX_IRQ_FREQ) ++result;
    } while (temp <= MAX_IRQ_FREQ);
    return static_cast<uint16_t>(result - 1);
}

// adt2unit.pas の calc_realtime_bpm_speed (sdl_timer_slowdown = 2)
double calc_realtime_bpm_speed(uint8_t tempo, uint8_t speed, uint8_t rows_per_beat)
{
    const A2Player& p = P();
    if (speed == 0 || rows_per_beat == 0 || p.IRQ_freq == 0) return 0;
    return tempo * 60.0 / speed / rows_per_beat *
           (1 + static_cast<double>(p.IRQ_freq_shift + p.playback_speed_shift) / p.IRQ_freq - 2 / 100.0);
}

// デバッグ情報の 1 回分の表示 (ロックを取った状態で呼ぶ)
void debug_info_refresh(uint8_t xstart, uint8_t ystart, bool _ctrl_alt_flag, bool _details_flag, bool _macro_details_flag)
{
    A2Player& p = P();
    FixedSongData& sd = songdata();
    const uint8_t nm = sd.nm_tracks;
    const uint8_t _win_attr[2] = { B(debug_info_bckg + debug_info_border2), B(debug_info_bckg + debug_info_border) };
    const bool stopped = p.play_status == PlayStatus::Stopped;

    if (_ctrl_alt_flag) {
        const int contxt_len = static_cast<int>(_contxt_str.size());
        if (!_details_flag) {
            ShowStr(screen_ptr, B(xstart + 83 - contxt_len), B(ystart + nm + 6), _contxt_str,
                    B(debug_info_bckg + debug_info_topic));
        } else {
            ShowStr(screen_ptr, B(xstart + 83 - contxt_len), B(ystart + nm + 6),
                    ExpStrL("", B(contxt_len), '\xCD'), B(debug_info_bckg + debug_info_border));
        }
    }

    const char* const* win_str = !_details_flag ? debug_win_str1 : (!_macro_details_flag ? debug_win_str2 : debug_win_str3);
    const char* page_str = !_details_flag ? " [~1/3~] " : (!_macro_details_flag ? " [~2/3~] " : " [~3/3~] ");
    ShowCStr(screen_ptr, B(xstart + 2), B(ystart + 1), win_str[0], B(debug_info_bckg + debug_info_topic),
             B(debug_info_bckg + debug_info_border));
    ShowStr(screen_ptr, B(xstart + 2), B(ystart + 2), win_str[1], B(debug_info_bckg + debug_info_border));
    ShowStr(screen_ptr, B(xstart + 2), B(ystart + nm + 3), win_str[2], B(debug_info_bckg + debug_info_border));
    if (!_ctrl_alt_flag) {
        ShowCStr(screen_ptr, B(xstart + 76), B(ystart + nm + 6), page_str, _win_attr[_ctrl_alt_flag ? 1 : 0],
                 B(debug_info_bckg + debug_info_topic));
    }

    std::string temps, temps2, temps3;
    if (!p.play_single_patt && !p.replay_forbidden && p.repeat_pattern) temps = "~\x13~";
    else temps = "\x13";

    const uint8_t txt = B(debug_info_bckg + debug_info_txt);
    const uint8_t hi_txt = B(debug_info_bckg + debug_info_hi_txt);
    if (!p.play_single_patt) {
        ShowCStr(screen_ptr, B(xstart + 2), B(ystart + nm + 4),
                 "~ORDER/PATTERN/ROW~  " + byte2hex(p.current_order) + "/" + byte2hex(p.current_pattern) + "/" +
                     byte2hex(p.current_line),
                 txt, hi_txt);
    } else {
        ShowCStr(screen_ptr, B(xstart + 2), B(ystart + nm + 4),
                 "~ORDER/PATTERN/ROW~  --/" + byte2hex(p.current_pattern) + "/" + byte2hex(p.current_line), txt, hi_txt);
    }
    ShowCStr(screen_ptr, B(xstart + 30), B(ystart + nm + 4), temps, B(debug_info_bckg + debug_info_txt_hid), txt);

    if (p.tempo == 18 && p.timer_fix) temps = Num2str(p.tempo, 10) + "\x05\xAE";
    else temps = Num2str(p.tempo, 10) + "\xAE";

    if (p._macro_speedup() == 1) {
        temps2 = temps;
    } else {
        const uint16_t temp = calc_max_speedup(p.tempo);
        if (p._macro_speedup() <= temp) temps2 = Num2str(p.tempo * p._macro_speedup(), 10) + "\xAE";
        else temps2 = Num2str(p.tempo * temp, 10) + "\xAE";
    }
    ShowCStr(screen_ptr, B(xstart + 2), B(ystart + nm + 5),
             "~SPEED/TEMPO/MACROS~ " + byte2hex(p.speed) + "/" + ExpStrR(temps + "/" + temps2, 9, ' '), txt, hi_txt);

    temps = p.current_tremolo_depth == 0 ? "1dB" : "4.8dB";
    temps2 = p.current_vibrato_depth == 0 ? "7%    " : "14%   ";
    ShowCStr(screen_ptr, B(xstart + 36), B(ystart + nm + 4), "~TREMOLO/ViBRATO DEPTH~ " + temps + "/" + temps2, txt, hi_txt);
    ShowCStr(screen_ptr, B(xstart + 36), B(ystart + nm + 5),
             "~GLOBAL VOLUME~         " + ExpStrR(Num2str(p.global_volume, 16), 2, '0'), txt, hi_txt);

    temps = " " + ExpStrL(Num2str(p.song_timer / 60, 10), 2, '0') + ":" + ExpStrL(Num2str(p.song_timer % 60, 10), 2, '0') + "." +
            static_cast<char>('0' + p.song_timer_tenths / 10) + " ";
    if (!stopped) temps = "~" + temps + "~";
    ShowCStr(screen_ptr, B(xstart + 74), B(ystart + nm + 4), temps, txt, hi_txt);

    temps = Bpm2str(calc_realtime_bpm_speed(p.tempo, p.speed, p.mark_line)) + " BPM";
    const int shift = p.IRQ_freq_shift + p.playback_speed_shift;
    if (shift > 0) temps += " [+" + Num2str(shift, 10) + "\xAE]";
    else if (shift < 0) temps += " [-" + Num2str(std::abs(shift), 10) + "\xAE]";
    ShowStr(screen_ptr, B(xstart + 62), B(ystart + nm + 5), ExpStrL(temps, 20, ' '), B(debug_info_bckg + debug_info_bpm));

    const uint8_t current_track = count_channel(pattern_hpos);
    for (int temp = 1; temp <= nm; ++temp) {
        const uint8_t chan = B(temp);
        const uint8_t bckg_attr = (!_ctrl_alt_flag && temp == current_track) ? debug_info_bckg2 : debug_info_bckg;
        const bool on = p.channel_flag[chan];
        const bool fresh = p.event_new[chan];
        const uint8_t hid = B(bckg_attr + debug_info_txt_hid);
        const uint8_t border = B(bckg_attr + debug_info_border);
        const uint8_t atr1 = on ? B(bckg_attr + (fresh ? debug_info_hi_txt : debug_info_txt)) : hid;
        const uint8_t atr2 = atr1;
        const uint8_t atr3 = on ? B(bckg_attr + (fresh ? debug_info_hi_car : debug_info_car)) : hid;
        const uint8_t atr4 = on ? B(bckg_attr + (fresh ? debug_info_hi_mod : debug_info_mod)) : hid;
        const uint8_t blank = B(bckg_attr + (bckg_attr >> 4));

        if (p.percussion_mode && temp >= 16 && temp <= 20) {
            temps = std::string(1, _perc_char[temp - 16]);
        } else {
            uint8_t bit = 0;
            switch (temp) {
            case 1: case 2: bit = 1; break;
            case 3: case 4: bit = 2; break;
            case 5: case 6: bit = 4; break;
            case 10: case 11: bit = 8; break;
            case 12: case 13: bit = 0x10; break;
            case 14: case 15: bit = 0x20; break;
            default: break;
            }
            if (bit != 0 && (sd.flag_4op | bit) == sd.flag_4op) temps = in_4op_tracks_hi(chan) ? "\xAC" : "\xAD";
            else temps = " ";
        }
        ShowStr(screen_ptr, B(xstart + 1), B(ystart + temp + 2), temps, B(bckg_attr + debug_info_perc));
        ShowCStr(screen_ptr, B(xstart + 2), B(ystart + temp + 2), ExpStrL(Num2str(temp, 10), 2, ' '), atr1, hid);

        const Chunk& ev = p.event_table[chan];
        if (!_details_flag) {
            if (ev.instr_def >= 1) {
                temps = ExpStrR(cstr2str(Copy(at1(sd.instr_names, ev.instr_def).str(), 2, 30)), 30, ' ');
                temps.erase(6, 1);
                temps.insert(6, "~\xF7~");
            } else {
                temps = ExpStrR("      ~\xF7~", 30 + 2, ' ');
            }
        } else if (ev.instr_def >= 1) {
            temps = "i" + byte2hex(ev.instr_def);
        } else {
            temps = ExpStrR("", 3, ' ');
        }

        const uint8_t temp2 = (stopped && !p.debugging) ? 3 : p.panning_table[chan];
        const uint8_t lock = at1(sd.lock_flags, temp);
        switch ((lock >> 2) & 3) {
        case 0: temps2 = "\xFA\x18\x19"; break;
        case 1: temps2 = "C\x18\x19"; break;
        case 2: temps2 = "M\x18\x19"; break;
        default: temps2 = "&\x18\x19"; break;
        }
        if (((lock >> 2) & 3) == 0 || (stopped && !p.debugging)) temps2 = "`" + temps2 + "`";
        if (p.lockvol && (lock | 0x10) == lock) temps2 += "~\xB3~V+";
        else temps2 += "~\xB3~`V+`";
        if (p.lockVP && (lock | 0x20) == lock) temps2 += "~\xB3~P+";
        else temps2 += "~\xB3~`P+`";

        if (!(p.is_4op_chan(chan) && in_4op_tracks_hi(chan))) temps3 = ExpStrL(Num2str(p.freqtable2[chan], 16), 4, '0');
        else temps3 = "    ";

        const std::string panning = "~\xB3~" + std::string(_panning_str[temp2 & 3]) + "~\xB3~";
        ShowC3Str(screen_ptr, B(xstart + 4), B(ystart + temp + 2), panning, p.pan_lock[chan] ? atr2 : atr3, border, hid);

        const std::string fx = note_str(ev.note) + "~\xB3~" + effect_str(ev.effect_def, ev.effect, ev.effect_def2) + "~\xB3~" +
                               effect_str(ev.effect_def2, ev.effect2, ev.effect_def) + "~\xB3~";
        const bool perc_17_20 = p.percussion_mode && temp >= 17 && temp <= 20;
        const A2Player::FmParameterTable& fm = p.fmpar_table[chan];

        if (!_details_flag) {
            ShowC3Str(screen_ptr, B(xstart + 8), B(ystart + temp + 2), temps2 + "~\xB3~", atr2, border, hid);
            ShowCStr(screen_ptr, B(xstart + 18), B(ystart + temp + 2), temps + "~\xB3~" + fx + temps3 + "~\xB3~", atr1, border);
        } else if (!_macro_details_flag) {
            ShowCStr(screen_ptr, B(xstart + 8), B(ystart + temp + 2), temps + "~\xB3~" + fx + temps3 + "~\xB3~", atr1, border);

            if (!perc_17_20) {
                ShowStr(screen_ptr, B(xstart + 40), B(ystart + temp + 2),
                        std::string(_connection[fm.connect & 1]) + " " + Num2str(fm.feedb, 16) + " ", atr1);
            } else {
                ShowStr(screen_ptr, B(xstart + 40), B(ystart + temp + 2), ExpStrL("", 5, ' '), atr1);
            }

            const std::string car = Num2str(fm.adsrw_car.attck, 16) + Num2str(fm.adsrw_car.dec, 16) +
                                    Num2str(fm.adsrw_car.sustn, 16) + Num2str(fm.adsrw_car.rel, 16) + " " +
                                    Num2str(fm.adsrw_car.wform, 16) + " " + Num2str(fm.kslC, 16) + " " +
                                    Num2str(fm.multipC, 16) + " " + _off_on[0][fm.tremC & 1] + _off_on[1][fm.vibrC & 1] +
                                    _off_on[2][fm.ksrC & 1] + _off_on[3][fm.sustC & 1] + "~\xB3~";
            ShowCStr(screen_ptr, B(xstart + 45), B(ystart + temp + 2), car, perc_17_20 ? blank : atr3, border);

            const std::string mod = Num2str(fm.adsrw_mod.attck, 16) + Num2str(fm.adsrw_mod.dec, 16) +
                                    Num2str(fm.adsrw_mod.sustn, 16) + Num2str(fm.adsrw_mod.rel, 16) + " " +
                                    Num2str(fm.adsrw_mod.wform, 16) + " " + Num2str(fm.kslM, 16) + " " +
                                    Num2str(fm.multipM, 16) + " " + _off_on[0][fm.tremM & 1] + _off_on[1][fm.vibrM & 1] +
                                    _off_on[2][fm.ksrM & 1] + _off_on[3][fm.sustM & 1] + "~\xB3~";
            ShowCStr(screen_ptr, B(xstart + 61), B(ystart + temp + 2), mod, atr4, border);
        } else {
            ShowCStr(screen_ptr, B(xstart + 8), B(ystart + temp + 2), temps + "~\xB3~" + fx, atr1, border);

            const A2Player::MacroState& mt = p.macro_table[chan];
            const uintptr_t rt = instr_macros_addr(mt.fmreg_table);
            const uintptr_t rd = instr_macros_data_addr(mt.fmreg_table, mt.fmreg_pos);
            const uint8_t fm_length = songdata_read<uint8_t>(rt + offsetof(RegisterTable, length));
            const std::string fmreg = _macro_pos_str_fm(
                mt.fmreg_pos, fm_length, songdata_read<uint8_t>(rt + offsetof(RegisterTable, keyoff_pos)),
                songdata_read<uint8_t>(rd + offsetof(RegisterTableDef, duration)),
                B(songdata_read<uint8_t>(rd + offsetof(RegisterTableDef, fm_data) + offsetof(FmInstData, FEEDBACK_FM)) >> 5),
                songdata_read<int16_t>(rd + offsetof(RegisterTableDef, freq_slide)));

            const uintptr_t at = macro_table_addr(mt.arpg_table) + offsetof(MacroTable, arpeggio);
            const std::string arpg = byte2hex(mt.arpg_table) + "\xF6" +
                                     _macro_pos_str_av(mt.arpg_pos, songdata_read<uint8_t>(at + offsetof(ArpeggioTable, length)),
                                                       songdata_read<uint8_t>(at + offsetof(ArpeggioTable, keyoff_pos)), "");

            const uintptr_t vt = macro_table_addr(mt.vib_table) + offsetof(MacroTable, vibrato);
            const int8_t vib_value = songdata_read<int8_t>(vt + offsetof(VibratoTable, data) + static_cast<intptr_t>(mt.vib_pos - 1));
            const std::string vibr = byte2hex(mt.vib_table) + "\xF6" +
                                     _macro_pos_str_av(mt.vib_pos, songdata_read<uint8_t>(vt + offsetof(VibratoTable, length)),
                                                       songdata_read<uint8_t>(vt + offsetof(VibratoTable, keyoff_pos)),
                                                       _freq_slide_str(vib_value));

            ShowC3Str(screen_ptr, B(xstart + 35), B(ystart + temp + 2),
                      _macro_str(fmreg, fm_length) + "~\xB3~" + _macro_str(arpg, mt.arpg_table) + "~\xB3~" +
                          _macro_str(vibr, mt.vib_table) + "~\xB3~" + temps3 + "~\xB3~",
                      atr1, border, hid);
        }

        ShowCStr(screen_ptr, B(xstart + 77), B(ystart + temp + 2),
                 ExpStrL(Num2str(p.carrier_vol[chan], 16), 2, '0') + "~\xB3~", perc_17_20 ? blank : atr3, border);
        ShowCStr(screen_ptr, B(xstart + 80), B(ystart + temp + 2), ExpStrL(Num2str(p.modulator_vol[chan], 16), 2, '0') + " ",
                 atr4, border);
    }
}

// Ctrl+D、または Ctrl+Alt を押している間 (その場合は Ctrl+Alt を離すと閉じる)
// F1 (ヘルプ) はヘルプの実装とともに対応する。
void DEBUG_INFO()
{
    const bool _ctrl_alt_flag = ctrl_pressed() && alt_pressed();
    const uint8_t _win_attr[2] = { B(debug_info_bckg + debug_info_border2), B(debug_info_bckg + debug_info_border) };
    bool _reset_state = false;
    bool _details_flag = false, _macro_details_flag = false;
    bool old_debugging = false, old_replay_forbidden = false;
    PlayStatus old_play_status = PlayStatus::Stopped;

    if (!_ctrl_alt_flag) {
        _details_flag = debug_info_details_flag >= 1;
        _macro_details_flag = debug_info_details_flag >= 2;
    }

    for (;;) {   // _jmp1
        uint8_t nm;
        {
            PlayerLock lock;
            nm = songdata().nm_tracks;
        }
        uint8_t xstart, ystart;
        ScreenMemCopy(screen_ptr, screen_backup);
        centered_frame_vdest = screen_ptr;
        HideCursor();
        centered_frame(xstart, ystart, 83, B(nm + 6), _win_title[_ctrl_alt_flag ? 1 : 0], _win_attr[_ctrl_alt_flag ? 1 : 0],
                       B(debug_info_bckg + debug_info_title), frame_double);

        uint16_t fkey;
        do {
            if (_ctrl_alt_flag) {
                _details_flag = shift_pressed();
                _macro_details_flag = !left_shift_pressed() && right_shift_pressed();
            }

            {
                PlayerLock lock;
                A2Player& p = P();
                if (p.space_pressed && p.play_status != PlayStatus::Stopped) {
                    if (!_ctrl_alt_flag && ctrl_pressed() && !alt_pressed() && !shift_pressed()) {
                        p.debugging = false;
                        _reset_state = false;
                    } else if (!_reset_state) {
                        _reset_state = true;
                        old_debugging = p.debugging;
                        old_play_status = p.play_status;
                        old_replay_forbidden = p.replay_forbidden;
                        p.debugging = true;
                        p.play_status = PlayStatus::Playing;
                        p.replay_forbidden = false;
                        STATUS_LINE_refresh();
                    }
                }
                debug_info_refresh(xstart, ystart, _ctrl_alt_flag, _details_flag, _macro_details_flag);
            }

            fkey = WORD_NULL;
            if (_ctrl_alt_flag) {
                if (keypressed()) keyboard_reset_buffer();
            } else if (keypressed()) {
                fkey = getkey();
            }

            if (scankey(SC_SPACE)) {
                bool stopped;
                uint8_t pos;
                {
                    PlayerLock lock;
                    stopped = P().play_status == PlayStatus::Stopped;
                    pos = P().calc_pattern_pos(pattern_patt);
                }
                if (stopped) {
                    fade_out_playback(false);
                    PlayerLock lock;
                    A2Player& p = P();
                    if (pos != BYTE_NULL) {
                        p.calibrate_player(pos, pattern_page, true);
                        if (p.play_status != PlayStatus::Stopped) {
                            p.debugging = true;
                            p.play_status = PlayStatus::Playing;
                            p.replay_forbidden = false;
                            PATTERN_ORDER_page_refresh(pattord_page);
                            PATTERN_page_refresh(pattern_page);
                            tracing = true;
                        }
                    } else {
                        p.play_single_patt = true;
                        p.no_sync_playing = true;
                        p.start_pattern = pattern_patt;
                        p.start_line = pattern_page;
                        start_playing();
                        p.debugging = true;
                        tracing = true;
                    }
                } else if (!tracing) {
                    PlayerLock lock;
                    P().debugging = true;
                    tracing = true;
                }
            }

            switch (fkey) {
            case kCtLEFT:
            case kCtRGHT: {
                PlayerLock lock;
                A2Player& p = P();
                if (!p.debugging && p.play_status == PlayStatus::Playing) {
                    if (fkey == kCtLEFT) p.rewind = true;
                    else p.fast_forward = true;
                }
                break;
            }

            case kTAB:
                if (!_details_flag) {
                    _details_flag = true;
                } else if (!_macro_details_flag) {
                    _macro_details_flag = true;
                } else {
                    _details_flag = false;
                    _macro_details_flag = false;
                }
                break;

            case kBkSPC: {
                PlayerLock lock;
                A2Player& p = P();
                if (!p.replay_forbidden) p.repeat_pattern = !p.repeat_pattern;
                break;
            }

            case kPgUP:
            case kHOME:
            case kPgDOWN:
            case kEND: {
                PlayerLock lock;
                if (fkey == kPgUP || fkey == kHOME) {
                    chan_pos = 1;
                    pattern_hpos = 1;
                } else {
                    chan_pos = last_chan_pos();
                    pattern_hpos = last_hpos();
                }
                PATTERN_ORDER_page_refresh(pattord_page);
                PATTERN_page_refresh(pattern_page);
                break;
            }

            case kUP:
            case kDOWN: {
                PlayerLock lock;
                const uint8_t n = B(_pattedit_lastpos / MAX_TRACKS);
                bool moved = false;
                if (fkey == kUP) {
                    if (chan_pos > 1) {
                        --chan_pos;
                        moved = true;
                    } else if (pattern_hpos > n) {
                        pattern_hpos = B(pattern_hpos - n);
                        moved = true;
                    }
                } else {
                    if (chan_pos < last_chan_pos()) {
                        ++chan_pos;
                        moved = true;
                    } else if (pattern_hpos <= last_hpos() - n) {
                        pattern_hpos = B(pattern_hpos + n);
                        moved = true;
                    }
                }
                if (moved) {
                    PATTERN_ORDER_page_refresh(pattord_page);
                    PATTERN_page_refresh(pattern_page);
                }
                break;
            }

            case kCHmins:
            case kNPmins:
            case kCtHOME:
            case kCHplus:
            case kNPplus:
            case kCtEND: {
                const bool backward = fkey == kCHmins || fkey == kNPmins || fkey == kCtHOME;
                const bool keep_line = fkey == kCtHOME || fkey == kCtEND;
                int target = -1;
                uint8_t temp2 = 0;
                {
                    PlayerLock lock;
                    A2Player& p = P();
                    if (!p.play_single_patt) {
                        uint8_t temp = p.current_order;
                        temp2 = p.current_line;
                        if (backward) {
                            while (temp > 0 && !(p.order_at(B(temp - 1)) < 0x80)) --temp;
                            if (temp > 0) {
                                --temp;
                                if (p.order_at(temp) < 0x80) target = temp;
                            }
                        } else {
                            while (temp < 0x7f && p.order_at(B(temp + 1)) > 0x80) ++temp;
                            if (temp < 0x7f) {
                                ++temp;
                                if (p.order_at(temp) < 0x80) target = temp;
                            }
                        }
                    }
                }
                if (target >= 0) {
                    fade_out_playback(false);
                    PlayerLock lock;
                    calibrate_player(B(target), keep_line ? temp2 : 0, false);
                }
                break;
            }

            case kCtENTR:
                jump_to_next_order();
                break;

            case kAlt0: {
                PlayerLock lock;
                const uint8_t nm_tracks = songdata().nm_tracks;
                if ((shift_pressed() && nm_tracks > 9) || nm_tracks == 10) {
                    toggle_channel(10);
                    fkey = WORD_NULL;
                }
                break;
            }

            case kAstrsk:
            case kNPastr:
            case kAltS:
            case kAltR: {
                PlayerLock lock;
                process_channel_keys(fkey);
                break;
            }

            default:
                if (fkey >= kAlt1 && fkey <= kAlt9 && (fkey & 0xff) == 0) {
                    PlayerLock lock;
                    process_channel_keys(fkey);
                }
                break;
            }

            draw_screen();
        } while (!((!_ctrl_alt_flag && (fkey == kESC || fkey == kAlt0)) || (_ctrl_alt_flag && !(ctrl_pressed() && alt_pressed())) ||
                   _force_program_quit));

        if (!_ctrl_alt_flag) keyboard_reset_buffer();
        if (fkey != kAlt0 && _reset_state) {
            PlayerLock lock;
            A2Player& p = P();
            p.debugging = old_debugging;
            p.play_status = old_play_status;
            p.replay_forbidden = old_replay_forbidden;
        }
        if (!_ctrl_alt_flag) debug_info_details_flag = B(int(_details_flag) + int(_macro_details_flag));

        move_to_screen_data = screen_backup;
        move_to_screen_area[1] = xstart;
        move_to_screen_area[2] = ystart;
        move_to_screen_area[3] = B(xstart + 83 + 2);
        move_to_screen_area[4] = B(ystart + nm + 6 + 1);
        move2screen();

        if (fkey == kAlt0) {
            {
                PlayerLock lock;
                P().no_step_debugging = true;
            }
            track_onoff_dialog();
            draw_screen();
            keyboard_reset_buffer();
            {
                PlayerLock lock;
                P().no_step_debugging = false;
            }
            if (!_force_program_quit) continue;
        }
        break;
    }
}

// ---------------------------------------------------------------------------
// トレース (ipattern.inc の PATTERN_trace)
// ---------------------------------------------------------------------------

uint16_t PATTERN_trace()
{
    {
        PlayerLock lock;
        if (play_status() == isStopped) return 0;
    }
    _traceprc_last_order = BYTE_NULL;
    _traceprc_last_pattern = BYTE_NULL;
    _traceprc_last_line = BYTE_NULL;

    HideCursor();
    const uint8_t old_pattord_page = pattord_page;
    const uint8_t old_pattord_hpos = pattord_hpos;
    const uint8_t old_pattord_vpos = pattord_vpos;
    const uint8_t old_pattern_patt = pattern_patt;
    const uint8_t old_pattern_page = pattern_page;
    const uint8_t old_pattern_hpos = pattern_hpos;
    bool reset_pos = true;
    bool nope = false;
    tracing = true;

    uint16_t fkey2 = WORD_NULL;
    for (;;) {
        nope = false;
        {
            PlayerLock lock;
            trace_update_proc();
        }
        if (ctrl_pressed() && alt_pressed()) {
            DEBUG_INFO();
            PlayerLock lock;
            PATTERN_ORDER_page_refresh(pattord_page);
            PATTERN_page_refresh(pattern_page);
        }

        uint16_t key = process_events();
        if (key == WORD_NULL) {
            if (keypressed()) key = getkey();
        }

        if (key != WORD_NULL) {
            fkey2 = key;
            if (fkey2 == kF3 || fkey2 == kShF3 || fkey2 == kCtrlL) {
                FILE_open();
            } else if (fkey2 == kCtrlD) {
                pause_debugging();
                DEBUG_INFO();
                keyboard_reset_buffer();
            } else if (fkey2 == kCtENTR) {
                jump_to_next_order();
            } else if (fkey2 == kF10) {
                if (QUIT_request()) nope = true;
                else fkey2 = WORD_NULL;   // やめた場合は終了扱いにしない
            } else if (process_file_keys(fkey2, true)) {
                // 保存・NUKE
            } else if (fkey2 == kF7) {
                fade_out_playback(false);
                PlayerLock lock;
                A2Player& p = P();
                p.debugging = false;
                stop_playing();
                nope = true;
                if (p.play_status != PlayStatus::Stopped) std::fill(std::begin(p.ai_table), std::end(p.ai_table), uint8_t(0));
            } else if (fkey2 == kCHmins || fkey2 == kNPmins || fkey2 == kCtHOME ||
                       fkey2 == kCHplus || fkey2 == kNPplus || fkey2 == kCtEND) {
                // 前後のオーダーへ移動する
                const bool backward = fkey2 == kCHmins || fkey2 == kNPmins || fkey2 == kCtHOME;
                const bool keep_line = fkey2 == kCtHOME || fkey2 == kCtEND;
                int target = -1;
                uint8_t temp2 = 0;
                {
                    PlayerLock lock;
                    A2Player& p = P();
                    if (!p.play_single_patt) {
                        uint8_t temp = p.current_order;
                        temp2 = p.current_line;
                        if (backward) {
                            while (temp > 0 && !(p.order_at(B(temp - 1)) < 0x80)) --temp;
                            if (temp > 0) {
                                --temp;
                                if (p.order_at(temp) < 0x80) target = temp;
                            }
                        } else {
                            while (temp < 0x7f && p.order_at(B(temp + 1)) > 0x80) ++temp;
                            if (temp < 0x7f) {
                                ++temp;
                                if (p.order_at(temp) < 0x80) target = temp;
                            }
                        }
                    }
                }
                if (target >= 0) {
                    fade_out_playback(false);
                    PlayerLock lock;
                    calibrate_player(B(target), keep_line ? temp2 : 0, false);
                }
            } else {
                PlayerLock lock;
                A2Player& p = P();
                switch (fkey2) {
                case kUP:
                    if (!shift_pressed() && (track_notes || (!p.debugging && p.play_status == PlayStatus::Playing))) p.rewind = true;
                    break;

                case kDOWN:
                    if (!shift_pressed() && (track_notes || (!p.debugging && p.play_status == PlayStatus::Playing))) p.fast_forward = true;
                    break;

                case kLEFT:
                case kShTAB:
                    if (!track_notes) {
                        if (chan_pos > 1) {
                            --chan_pos;
                            PATTERN_ORDER_page_refresh(pattord_page);
                            PATTERN_page_refresh(pattern_page);
                        } else if (pattern_hpos > _pattedit_lastpos / MAX_TRACKS) {
                            pattern_hpos = B(pattern_hpos - _pattedit_lastpos / MAX_TRACKS);
                            PATTERN_ORDER_page_refresh(pattord_page);
                            PATTERN_page_refresh(pattern_page);
                        }
                    }
                    break;

                case kRIGHT:
                case kTAB:
                    if (!track_notes) {
                        if (chan_pos < last_chan_pos()) {
                            ++chan_pos;
                            PATTERN_ORDER_page_refresh(pattord_page);
                            PATTERN_page_refresh(pattern_page);
                        } else if (pattern_hpos <= last_hpos() - _pattedit_lastpos / MAX_TRACKS) {
                            pattern_hpos = B(pattern_hpos + _pattedit_lastpos / MAX_TRACKS);
                            PATTERN_ORDER_page_refresh(pattord_page);
                            PATTERN_page_refresh(pattern_page);
                        }
                    }
                    break;

                case kENTER:
                case kESC:
                    nope = true;
                    p.debugging = false;
                    if (fkey2 == kENTER || (shift_pressed() && fkey2 == kESC)) {
                        reset_pos = false;
                        if (fkey2 == kENTER) {
                            p.replay_forbidden = true;
                            p.play_status = PlayStatus::Paused;
                        }
                        PATTERN_ORDER_page_refresh(pattord_page);
                        PATTERN_page_refresh(pattern_page);
                    }
                    break;

                case kF5:
                case kAltF5:
                case kShF5:
                    if (p.play_single_patt && p.play_status == PlayStatus::Paused) {
                        p.replay_forbidden = false;
                        p.play_status = PlayStatus::Playing;
                    } else if (p.play_status == PlayStatus::Playing) {
                        p.debugging = false;
                        if ((!nosync_by_default && fkey2 == kAltF5) || (nosync_by_default && fkey2 == kF5)) p.no_sync_playing = true;
                    } else if (p.play_status == PlayStatus::Paused) {
                        p.debugging = false;
                        p.replay_forbidden = false;
                        p.play_status = PlayStatus::Playing;
                        if ((!nosync_by_default && fkey2 == kAltF5) || (nosync_by_default && fkey2 == kF5)) p.no_sync_playing = true;
                    }
                    break;

                case kF6:
                    if (p.play_status == PlayStatus::Playing) {
                        if (!p.debugging) {
                            p.replay_forbidden = true;
                            p.play_status = PlayStatus::Paused;
                        }
                        p.debugging = false;
                    } else if (p.play_status == PlayStatus::Paused) {
                        p.debugging = false;
                        p.replay_forbidden = false;
                        p.play_status = PlayStatus::Playing;
                    }
                    break;

                case kF8:
                case kAltF8:
                case kShF8:
                case kF9:
                case kAltF9:
                case kShF9: {
                    const bool repeat = fkey2 == kF9 || fkey2 == kAltF9 || fkey2 == kShF9;
                    if (p.play_single_patt && p.play_status == PlayStatus::Paused) {
                        p.replay_forbidden = false;
                        p.play_status = PlayStatus::Playing;
                    } else if (p.play_status == PlayStatus::Playing) {
                        p.debugging = false;
                        p.repeat_pattern = repeat;
                        if (!repeat) {
                            if ((!nosync_by_default && fkey2 == kAltF8) || (nosync_by_default && fkey2 == kF8)) p.no_sync_playing = true;
                        } else {
                            // オリジナルのまま (nosync_by_default のとき F8 を見ている)
                            if ((!nosync_by_default && fkey2 == kAltF9) || (nosync_by_default && fkey2 == kF8)) p.no_sync_playing = true;
                        }
                    } else if (p.play_status == PlayStatus::Paused) {
                        p.debugging = false;
                        p.repeat_pattern = repeat;
                        p.replay_forbidden = false;
                        p.play_status = PlayStatus::Playing;
                        const uint16_t alt = repeat ? kAltF9 : kAltF8;
                        const uint16_t plain = repeat ? kF9 : kF8;
                        if ((!nosync_by_default && fkey2 == alt) || (nosync_by_default && fkey2 == plain)) p.no_sync_playing = true;
                    }
                    break;
                }

                default:
                    process_channel_keys(fkey2);
                    break;
                }
            }
        }

        if (pending_track_dialog) {
            pending_track_dialog = false;
            track_onoff_dialog();
        }
        draw_screen();

        bool stopped;
        {
            PlayerLock lock;
            stopped = play_status() == isStopped;
        }
        if (nope || stopped || _force_program_quit) break;
    }

    if (fkey2 == kF10 || _force_program_quit) return fkey2;
    if (use_large_cursor) WideCursor();
    else ThinCursor();
    tracing = false;

    if (nope && reset_pos) {
        pattord_page = old_pattord_page;
        pattord_hpos = old_pattord_hpos;
        pattord_vpos = old_pattord_vpos;
        pattern_patt = old_pattern_patt;
        pattern_page = old_pattern_page;
        pattern_hpos = old_pattern_hpos;
    }

    PlayerLock lock;
    P().debugging = false;
    PATTERN_ORDER_page_refresh(pattord_page);
    PATTERN_page_refresh(pattern_page);
    return fkey2;
}

// ---------------------------------------------------------------------------
// 停止中・トレースしていないときの操作 (ipattern.inc の PATTERN_edit のうちカーソル移動と再生操作)
// ---------------------------------------------------------------------------

// F5 / F8 / F9 などで再生を始めた後、トレースに入るかどうか (Shift で既定の動作を反転する)
bool want_trace()
{
    return (shift_pressed() && !trace_by_default) || (!shift_pressed() && trace_by_default);
}

// 再生系のキー。トレースに入る場合は true を返す。
bool process_play_keys(uint16_t fkey, uint8_t pattern)
{
    PlayerLock lock;
    A2Player& p = P();
    const bool nosync_f5 = (!nosync_by_default && fkey == kAltF5) || (nosync_by_default && fkey == kF5);

    switch (fkey) {
    case kF5:
    case kAltF5:
    case kShF5:
        if (p.play_single_patt && p.play_status == PlayStatus::Paused) {
            p.replay_forbidden = false;
            p.play_status = PlayStatus::Playing;
            return want_trace();
        }
        switch (p.play_status) {
        case PlayStatus::Playing:
            if (want_trace()) {
                if (nosync_f5) p.no_sync_playing = true;
                return true;
            }
            return false;
        case PlayStatus::Stopped:
            if (nosync_f5) p.no_sync_playing = true;
            start_playing();
            return want_trace();
        case PlayStatus::Paused:
            p.replay_forbidden = false;
            p.play_status = PlayStatus::Playing;
            if (nosync_f5) p.no_sync_playing = true;
            return want_trace();
        }
        return false;

    case kF6:
        if (p.play_status == PlayStatus::Playing) {
            p.replay_forbidden = true;
            p.play_status = PlayStatus::Paused;
        } else if (p.play_status == PlayStatus::Paused) {
            p.replay_forbidden = false;
            p.play_status = PlayStatus::Playing;
        }
        return false;

    case kF8:
    case kAltF8:
    case kShF8:
    case kF9:
    case kAltF9:
    case kShF9: {
        const bool repeat = fkey == kF9 || fkey == kAltF9 || fkey == kShF9;
        const uint16_t alt = repeat ? kAltF9 : kAltF8;
        const uint16_t plain = repeat ? kF9 : kF8;
        const bool nosync = (!nosync_by_default && fkey == alt) || (nosync_by_default && fkey == plain);

        if (p.play_single_patt && p.play_status == PlayStatus::Paused) {
            p.replay_forbidden = false;
            p.play_status = PlayStatus::Playing;
            return want_trace();
        }
        switch (p.play_status) {
        case PlayStatus::Playing:
            p.debugging = false;
            p.repeat_pattern = repeat;
            if (nosync) p.no_sync_playing = true;
            if (want_trace()) return true;
            if (p.calc_pattern_pos(pattern) != BYTE_NULL) {
                stop_playing();
                p.debugging = false;
                if (nosync) p.no_sync_playing = true;
                calibrate_player(p.calc_pattern_pos(pattern), 0, false);
                p.repeat_pattern = repeat;
            }
            return false;
        case PlayStatus::Stopped:
            if (p.calc_pattern_pos(pattern) != BYTE_NULL) {
                p.debugging = false;
                if (nosync) p.no_sync_playing = true;
                calibrate_player(p.calc_pattern_pos(pattern), 0, false);
                p.repeat_pattern = repeat;
                return want_trace();
            }
            return false;
        case PlayStatus::Paused:
            p.debugging = false;
            p.repeat_pattern = repeat;
            p.replay_forbidden = false;
            p.play_status = PlayStatus::Playing;
            if (nosync) p.no_sync_playing = true;
            return want_trace();
        }
        return false;
    }

    case kF7:
        return false;

    default:
        return false;
    }
}

// ---------------------------------------------------------------------------
// パターンの編集 (ipattern.inc の PATTERN_edit とその補助処理)
// ---------------------------------------------------------------------------

// 設定値 (typcons1.inc の既定値。設定画面は未実装)
bool lf_in_mboard_mode = false;
bool update_ins = false;
bool keep_track_pos = false;
uint8_t backspace_dir = 1;
const char b_note = 'b';
constexpr uint8_t pattern_input_bckg = 0xb0;
constexpr uint8_t pattern_input = 0x00;
constexpr uint8_t pattern_input_warn = 0x80;

// MIDI ボード (キーボードを鍵盤として使う) のキー (typcons1.inc、[1..29])
const uint8_t board_scancodes[29] = {
    0x2c, 0x1f, 0x2d, 0x20, 0x2e, 0x2f, 0x22, 0x30, 0x23, 0x31, 0x24, 0x32,
    0x10, 0x03, 0x11, 0x04, 0x12, 0x13, 0x06, 0x14, 0x07, 0x15, 0x08, 0x16, 0x17, 0x0a, 0x18, 0x0b, 0x19,
};

// ノートレコーダーの状態 (ノートレコーダーは未実装のため track_notes は FALSE のまま)
bool track_notes_ins = false;
uint8_t track_chan_start = 1;
uint8_t nm_track_chan = 1;
uint8_t old_nm_track_chan = 0;
uint8_t old_track_chan_start = 0;

// ブロックの選択 (ブロック操作は未実装。PATTERN_position_preview が使う)
uint8_t block_xstart = 1;
uint8_t block_ystart = 0;
bool discard_block = false;

inline uint8_t LO(uint16_t value) { return static_cast<uint8_t>(value & 0xff); }
inline uint8_t HI(uint16_t value) { return static_cast<uint8_t>(value >> 8); }
inline char UpCase(char c) { return (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c; }

std::string Upper(std::string str)
{
    for (char& c : str) c = UpCase(c);
    return str;
}

void cancel_note_recorder()
{
    if (track_notes) {
        old_nm_track_chan = nm_track_chan;
        old_track_chan_start = track_chan_start;
        track_notes = false;
        nm_track_chan = 1;
    }
}

// put_chunk (SDL 版)
void put_chunk(uint8_t pattern, uint8_t line, uint8_t channel, const Chunk& chunk)
{
    if (pattern < 128 && channel >= 1 && channel <= 20) {
        P().song()->putChunk(pattern, line, channel, chunk);
        module_archived = false;
    }
}

// 楽器データ (songdata.instr_data[ins]) の先頭
const uint8_t* instr_data_bytes(uint8_t ins)
{
    return reinterpret_cast<const uint8_t*>(&at1(songdata().instr_data, ins));
}

// --- 元に戻す (Ctrl+Z) ---
// オリジナルは SizeOf(tPATTERN_DATA) DIV 16 バイト、つまり pattern_patt から 8 パターン分を保存する

bool pattern_undo_flag = false;
uint8_t pattern_undo_patt = 0;
std::vector<Chunk> pattern_undo_data;

Chunk* pattern_data_ptr(uint8_t pattern)
{
    return &P().song()->patterns().block[pattern / 8][pattern % 8][0][0];
}

size_t pattern_undo_size(uint8_t pattern)
{
    return static_cast<size_t>(std::min(8, 128 - pattern)) * 20 * 256;
}

void _save_pattern_to_undo()
{
    pattern_undo_flag = true;
    pattern_undo_patt = pattern_patt;
    const Chunk* src = pattern_data_ptr(pattern_undo_patt);
    pattern_undo_data.assign(src, src + pattern_undo_size(pattern_undo_patt));
}

void _restore_pattern_from_undo()
{
    if (pattern_undo_flag) {
        std::copy(pattern_undo_data.begin(), pattern_undo_data.end(), pattern_data_ptr(pattern_undo_patt));
        pattern_undo_flag = false;
    }
}

// --- 楽器の印 (instedit.inc) ---

constexpr char _ins_mark_chr = '\x10';

bool check_4op_flag(uint8_t ins)
{
    const FixedSongData& sd = songdata();
    for (int idx = 1; idx <= sd.ins_4op_flags.num_4op && idx <= 128; ++idx) {
        if (sd.ins_4op_flags.idx_4op[idx - 1] == ins) return true;
    }
    return false;
}

char& instr_name_1st_char(int ins) { return at1(songdata().instr_names, ins).data[0]; }

uint8_t _1st_marked()
{
    for (int temp = 1; temp <= 255; ++temp) {
        if (instr_name_1st_char(temp) == _ins_mark_chr) return B(temp);
    }
    return 0;
}

uint8_t _2nd_marked()
{
    const uint8_t first = _1st_marked();
    if (first != 0) {
        for (int temp = first + 1; temp <= 255; ++temp) {
            if (instr_name_1st_char(temp) == _ins_mark_chr) return B(temp);
        }
    }
    return 0;
}

uint8_t marked_instruments()
{
    uint8_t result = 0;
    for (int temp = 1; temp <= 255; ++temp) {
        if (instr_name_1st_char(temp) == _ins_mark_chr) ++result;
    }
    return result;
}

void reset_marked_instruments()
{
    if (marked_instruments() < 2) {
        if (instr_name_1st_char(instrum_page) == _ins_mark_chr) instr_name_1st_char(instrum_page) = ' ';
    } else {
        instr_name_1st_char(_2nd_marked()) = ' ';
        instr_name_1st_char(_1st_marked()) = ' ';
    }
    update_4op_flag_marks();
}

// 4OP の楽器の組 (上位バイト: 1 つ目、下位バイト: 2 つ目)
uint16_t get_4op_to_test()
{
    uint16_t result = 0;
    if (songdata().flag_4op != 0) {
        if (check_4op_flag(current_inst)) {
            result = static_cast<uint16_t>(current_inst + 1 + (current_inst << 8));
        } else if (current_inst > 1 && check_4op_flag(B(current_inst - 1))) {
            result = static_cast<uint16_t>(current_inst + ((current_inst - 1) << 8));
        }
    }
    return result;
}

// エフェクトの値が範囲内か。command_typing のときは範囲内に直す。
bool correct_range(uint8_t fxdef, uint8_t& fxdata)
{
    bool result = false;
    switch (fxdef) {
    case ef_Arpeggio:
    case ef_FSlideUp:
    case ef_FSlideDown:
    case ef_FSlideUpFine:
    case ef_FSlideDownFine:
    case ef_FSlideUpVSlide:
    case ef_FSlUpVSlF:
    case ef_FSlideDownVSlide:
    case ef_FSlDownVSlF:
    case ef_FSlUpFineVSlide:
    case ef_FSlUpFineVSlF:
    case ef_FSlDownFineVSlide:
    case ef_FSlDownFineVSlF:
    case ef_TonePortamento:
    case ef_TPortamVolSlide:
    case ef_TPortamVSlideFine:
    case ef_Vibrato:
    case ef_VibratoVolSlide:
    case ef_VibratoVSlideFine:
    case ef_VolSlide:
    case ef_VolSlideFine:
    case ef_ArpggVSlide:
    case ef_ArpggVSlideFine:
    case ef_Tremolo:
    case ef_Tremor:
    case ef_ExtraFineVibrato:
    case ef_ExtraFineTremolo:
    case ef_SwapArpeggio:
    case ef_SwapVibrato:
    case ef_SetCustomSpeedTab:
    case ef_GlobalFSlideUp:
    case ef_GlobalFSlideDown:
        result = true;
        break;

    case ef_SetSpeed:
    case ef_SetTempo:
    case ef_ExtraFineArpeggio:
    case ef_RetrigNote:
    case ef_MultiRetrigNote:
        if (fxdata != 0) {
            result = true;
        } else if (command_typing != 0) {
            fxdata = fxdef != ef_MultiRetrigNote ? 1 : 0x10;
            result = true;
        }
        break;

    case ef_SetCarrierVol:
    case ef_SetModulatorVol:
    case ef_SetInsVolume:
    case ef_ForceInsVolume:
    case ef_SetGlobalVolume:
        if (fxdata <= 63) {
            result = true;
        } else if (command_typing != 0) {
            fxdata = 0;
            result = true;
        }
        break;

    case ef_PatternBreak:
        result = true;
        break;

    case ef_PositionJump:
        if (fxdata <= 127) {
            result = true;
        } else if (command_typing != 0) {
            fxdata = 0;
            result = true;
        }
        break;

    case ef_SetWaveform: {
        const auto valid = [](int v) { return v <= 7 || v == 0x0f; };
        if (valid(fxdata / 16) && valid(fxdata % 16)) {
            result = true;
        } else if (command_typing != 0) {
            if (valid(fxdata / 16)) fxdata &= 0xf0;
            else if (valid(fxdata % 16)) fxdata &= 0x0f;
            else fxdata = 0;
            result = true;
        }
        break;
    }

    case ef_Extended: {
        const int hi = fxdata >> 4;
        const int lo = fxdata & 0x0f;
        bool valid;
        switch (hi) {
        case 0x0: case 0x1: valid = lo <= 1; break;
        case 0xa: case 0xe: valid = lo <= 7; break;
        case 0xb: valid = lo <= 2; break;
        default: valid = true; break;
        }
        if (valid) {
            result = true;
        } else if (command_typing != 0) {
            fxdata &= 0xf0;
            result = true;
        }
        break;
    }

    case ef_Extended2:
        if ((fxdata & 0x0f) != 0) {
            result = true;
        } else if (command_typing != 0) {
            fxdata = B((fxdata & 0xf0) + 1);
            result = true;
        }
        break;

    case ef_Extended3: {
        const int hi = fxdata >> 4;
        const int lo = fxdata & 0x0f;
        bool valid;
        switch (hi) {
        case 0x1: case 0x7: valid = true; break;
        case 0x2: case 0x8: valid = lo <= 3; break;
        case 0x0: case 0x3: case 0x4: case 0x5: case 0x6: case 0x9: case 0xa: case 0xb: case 0xc: valid = lo <= 1; break;
        default: valid = false; break;
        }
        if (valid) {
            result = true;
        } else if (command_typing != 0) {
            if (hi <= 0x0c) fxdata = B((fxdata & 0xf0) + 1);
            else fxdata = 1;
            result = true;
        }
        break;
    }

    default:
        break;
    }
    return result;
}

// エフェクトの文字から番号へ (見つからなければ BYTE_NULL)
uint8_t FX(char chr)
{
    const char* p = std::strchr(fx_digits, UpCase(chr));
    if (chr == '\0' || p == nullptr) return BYTE_NULL;
    return B(static_cast<int>(p - fx_digits));
}

// 位置を指して確認するときの画面 (ipattern.inc の PATTERN_position_preview)
// mode 0: 位置を保存、1: 指定位置を表示、BYTE_NULL: 保存した位置に戻す
void PATTERN_position_preview(uint8_t pattern, uint8_t line, uint8_t channel, uint8_t mode)
{
    static uint8_t old_pattern_patt, old_pattern_page, old_pattern_hpos, old_chan_pos;
    static uint8_t old_block_xstart, old_block_ystart;
    static bool old_marking;

    if (mode == 0) {
        old_pattern_patt = pattern_patt;
        old_pattern_page = pattern_page;
        old_pattern_hpos = pattern_hpos;
        old_chan_pos = chan_pos;
        old_block_xstart = block_xstart;
        old_block_ystart = block_ystart;
        old_marking = marking;
    } else if (mode == 1) {
        pattern_patt = pattern;
        pattern_page = line;
        if (channel > chan_pos + MAX_TRACKS - 1) {
            while (channel > chan_pos + MAX_TRACKS - 1) ++chan_pos;
        } else {
            while (channel < chan_pos) --chan_pos;
        }
        while (channel > chan_pos + MAX_TRACKS - 1 && chan_pos < last_chan_pos()) ++chan_pos;
        if (count_channel(pattern_hpos) < channel) {
            while (count_channel(pattern_hpos) != channel) ++pattern_hpos;
        } else {
            while (count_channel(pattern_hpos) != channel) --pattern_hpos;
        }
        while (count_pos(pattern_hpos) != 0) --pattern_hpos;
        block_xstart = count_channel(pattern_hpos);
        block_ystart = line;
        marking = true;
        cancel_note_recorder();
        discard_block = true;
        PATTERN_ORDER_page_refresh(pattord_page);
        PATTERN_page_refresh(pattern_page);
    } else if (mode == BYTE_NULL) {
        pattern_patt = old_pattern_patt;
        pattern_page = old_pattern_page;
        pattern_hpos = old_pattern_hpos;
        chan_pos = old_chan_pos;
        block_xstart = old_block_xstart;
        block_ystart = old_block_ystart;
        marking = old_marking;
        discard_block = false;
        PATTERN_ORDER_page_refresh(pattord_page);
        PATTERN_page_refresh(pattern_page);
    }
}

// --- 移調 (adt2extn.pas の transpose_custom_area) ---

enum class TransposeType { Up, Down, CurrentIns, AllIns };

// ロックを取った状態で呼ぶ (確認のダイアログの間はロックを外す)
void transpose_custom_area(TransposeType type1, TransposeType type2, uint8_t patt0, uint8_t patt1,
                           uint8_t track0, uint8_t track1, uint8_t line0, uint8_t line1, uint8_t factor)
{
    A2Player& p = P();
    const bool backup_replay_forbidden = p.replay_forbidden;
    const PlayStatus backup_play_status = p.play_status;
    p.replay_forbidden = true;
    if (p.play_status != PlayStatus::Stopped) p.play_status = PlayStatus::Paused;
    PATTERN_position_preview(BYTE_NULL, BYTE_NULL, BYTE_NULL, 0);

    const bool up = type1 == TransposeType::Up;
    const bool current_ins = type2 == TransposeType::CurrentIns;
    const auto is_fixed_or_null = [](uint8_t note) {
        return (note >= fixed_note_flag + 1 && note <= fixed_note_flag + 12 * 8 + 1) || note == BYTE_NULL;
    };
    // 移調すると範囲を超えるか
    const auto overflow = [&](const Chunk& chunk) {
        if (up) {
            if (!current_ins) return !(chunk.note + factor <= 12 * 8 + 1) && !is_fixed_or_null(chunk.note);
            return !(chunk.note < 12 * 8 + 1) && !is_fixed_or_null(chunk.note) && chunk.instr_def == current_inst;
        }
        return !(chunk.note >= factor + 1) && !(chunk.note == 0 || is_fixed_or_null(chunk.note)) &&
               (!current_ins || chunk.instr_def == current_inst);
    };
    const auto overflow_dialog = [&](uint8_t patt, uint8_t line, uint8_t track, const char* keys) -> uint8_t {
        PATTERN_position_preview(patt, line, track, 1);
        keyboard_reset_buffer();
        PlayerUnlock unlock;
        return Dialog("PATTERN #" + byte2hex(patt) + ", " + "LiNE #" + byte2hex(line) + ", " + "TRACK #" +
                          Num2str(track, 10) + "$" + "NOTE OVERFLOW$",
                      keys, " TRANSPOSE ", 1);
    };

    uint8_t _1st_choice = BYTE_NULL;
    bool _break = false;
    bool _continue = true;

    for (int temp3 = patt0; temp3 <= patt1 && !_break; ++temp3) {
        for (int temp2 = track0; temp2 <= track1 && !_break; ++temp2) {
            for (int temp1 = line0; temp1 <= line1; ++temp1) {
                Chunk chunk{};
                get_chunk(B(temp3), B(temp1), B(temp2), chunk);
                if (overflow(chunk)) {
                    _1st_choice = overflow_dialog(B(temp3), B(temp1), B(temp2), "~E~RASE$ERASE ~A~LL$~S~KiP$S~K~iP ALL$~C~ANCEL$");
                    _break = true;
                    if (!(dl_environment.keystroke != kESC && _1st_choice != 5)) _continue = false;
                    break;
                }
            }
        }
    }

    _break = false;
    bool skip_all = false;
    bool erase_all = false;

    if (_continue) {
        for (int temp3 = patt0; temp3 <= patt1 && !_break; ++temp3) {
            for (int temp2 = track0; temp2 <= track1 && !_break; ++temp2) {
                for (int temp1 = line0; temp1 <= line1; ++temp1) {
                    Chunk chunk{};
                    get_chunk(B(temp3), B(temp1), B(temp2), chunk);
                    if (overflow(chunk)) {
                        if (_1st_choice != BYTE_NULL) {
                            switch (_1st_choice) {
                            case 1:
                            case 2:
                                chunk.note = 0;
                                chunk.instr_def = 0;
                                if (_1st_choice == 2) erase_all = true;
                                break;
                            case 3:
                            case 4:
                                if (_1st_choice == 4) skip_all = true;
                                break;
                            }
                            put_chunk(B(temp3), B(temp1), B(temp2), chunk);
                            _1st_choice = BYTE_NULL;
                            continue;
                        }

                        if (skip_all) continue;
                        if (erase_all) {
                            chunk.note = 0;
                            chunk.instr_def = 0;
                            put_chunk(B(temp3), B(temp1), B(temp2), chunk);
                            continue;
                        }

                        const uint8_t temp = overflow_dialog(B(temp3), B(temp1), B(temp2), "~E~RASE$ERASE ~A~LL$~S~KiP$S~K~iP ALL$CANCEL$");
                        if (dl_environment.keystroke != kESC) {
                            if (temp == 5) {
                                _break = true;
                                break;
                            }
                            if (temp == 1 || temp == 2) {
                                chunk.note = 0;
                                chunk.instr_def = 0;
                                if (temp == 2) erase_all = true;
                            } else if (temp == 4) {
                                skip_all = true;
                            }
                            put_chunk(B(temp3), B(temp1), B(temp2), chunk);
                        }
                    } else if (!(chunk.note == 0 || is_fixed_or_null(chunk.note)) &&
                               (!current_ins || chunk.instr_def == current_inst)) {
                        chunk.note = up ? B(chunk.note + factor) : B(chunk.note - factor);
                        put_chunk(B(temp3), B(temp1), B(temp2), chunk);
                    }
                }
            }
        }
    }

    PATTERN_position_preview(BYTE_NULL, BYTE_NULL, BYTE_NULL, BYTE_NULL);
    p.replay_forbidden = backup_replay_forbidden;
    p.play_status = backup_play_status;
}

// --- MIDI ボードで入力した行を鳴らす (PATTERN_edit 内の処理) ---

void preview_pattern_line(uint8_t pattern, uint8_t page)
{
    A2Player& p = P();
    const bool backup_replay_forbidden = p.replay_forbidden;
    const PlayStatus backup_play_status = p.play_status;

    decltype(p.event_table) event_table_backup;
    decltype(p.voice_table) voice_table_backup;
    decltype(p.volume_table) volume_table_backup;
    decltype(p.pan_lock) pan_lock_backup;
    decltype(p.volume_lock) volume_lock_backup;
    decltype(p.peak_lock) peak_lock_backup;
    decltype(p.panning_table) panning_table_backup;
    decltype(p.fmpar_table) fmpar_table_backup;
    decltype(p.freq_table) freq_table_backup;
    decltype(p.freqtable2) freqtable2_backup;
    decltype(p.keyoff_loop) keyoff_loop_backup;

    if (backup_play_status != PlayStatus::Stopped) {
        p.replay_forbidden = true;
        if (p.play_status != PlayStatus::Stopped) p.play_status = PlayStatus::Paused;
        p.nul_volume_bars();

        std::memcpy(event_table_backup, p.event_table, sizeof(p.event_table));
        std::memcpy(voice_table_backup, p.voice_table, sizeof(p.voice_table));
        std::memcpy(volume_table_backup, p.volume_table, sizeof(p.volume_table));
        std::memcpy(pan_lock_backup, p.pan_lock, sizeof(p.pan_lock));
        std::memcpy(volume_lock_backup, p.volume_lock, sizeof(p.volume_lock));
        std::memcpy(peak_lock_backup, p.peak_lock, sizeof(p.peak_lock));
        std::memcpy(panning_table_backup, p.panning_table, sizeof(p.panning_table));
        std::fill(std::begin(p.pan_lock), std::end(p.pan_lock), false);
        std::fill(std::begin(p.volume_lock), std::end(p.volume_lock), false);
        std::fill(std::begin(p.peak_lock), std::end(p.peak_lock), false);

        p.reset_player();
        std::memcpy(fmpar_table_backup, p.fmpar_table, sizeof(p.fmpar_table));
        std::memcpy(freq_table_backup, p.freq_table, sizeof(p.freq_table));
        std::memcpy(freqtable2_backup, p.freqtable2, sizeof(p.freqtable2));
        std::memcpy(keyoff_loop_backup, p.keyoff_loop, sizeof(p.keyoff_loop));
        std::fill(std::begin(p.keyoff_loop), std::end(p.keyoff_loop), false);
    }

    if (!(p.debugging && p.play_status == PlayStatus::Stopped)) {
        p.init_player();
        p.debugging = true;
    }

    const uint8_t old_order = p.current_order;
    const uint8_t old_pattern = p.current_pattern;
    const uint8_t old_line = p.current_line;
    const uint8_t old_speed = p.speed;
    const uint8_t old_tempo = p.tempo;
    p.current_order = 0;
    p.current_pattern = pattern;
    p.current_line = page;
    p.single_play = true;
    p.poll_proc();
    p.single_play = false;
    p.current_order = old_order;
    p.current_pattern = old_pattern;
    p.current_line = old_line;
    p.speed = old_speed;
    p.tempo = old_tempo;

    p.play_status = PlayStatus::Stopped;
    PATTERN_ORDER_page_refresh(pattord_page);
    PATTERN_page_refresh(page);

    if (backup_play_status != PlayStatus::Stopped) {
        std::memcpy(p.event_table, event_table_backup, sizeof(p.event_table));
        std::memcpy(p.voice_table, voice_table_backup, sizeof(p.voice_table));
        std::memcpy(p.volume_table, volume_table_backup, sizeof(p.volume_table));
        std::memcpy(p.fmpar_table, fmpar_table_backup, sizeof(p.fmpar_table));
        std::memcpy(p.panning_table, panning_table_backup, sizeof(p.panning_table));
        p.reset_player();

        std::memcpy(p.pan_lock, pan_lock_backup, sizeof(p.pan_lock));
        std::memcpy(p.volume_lock, volume_lock_backup, sizeof(p.volume_lock));
        std::memcpy(p.peak_lock, peak_lock_backup, sizeof(p.peak_lock));

        std::memcpy(p.freq_table, freq_table_backup, sizeof(p.freq_table));
        std::memcpy(p.freqtable2, freqtable2_backup, sizeof(p.freqtable2));
        std::memcpy(p.keyoff_loop, keyoff_loop_backup, sizeof(p.keyoff_loop));
        std::memset(p.macro_table, 0, sizeof(p.macro_table));
        p.replay_forbidden = backup_replay_forbidden;
        p.play_status = backup_play_status;
        p.debugging = false;
    }
}

// F11 / F12 (押している間に切り替える)
void wait_until_F11_F12_released()
{
    do {
        draw_screen();
        keyboard_poll_input();
    } while ((scankey(SC_F11) || scankey(SC_F12)) && !_force_program_quit);
    keyboard_reset_buffer();
}

void process_F11_F12()
{
    if (scankey(SC_F11) && !ctrl_pressed() && !alt_pressed() && !shift_pressed()) {
        if (command_typing != 0) {
            switch (command_typing) {
            case 1:
                if (cycle_pattern) {
                    command_typing = 2;
                    cycle_pattern = false;
                } else {
                    cycle_pattern = true;
                }
                break;
            case 2:
                command_typing = 1;
                cycle_pattern = false;
                break;
            }
        }
        {
            PlayerLock lock;
            status_refresh();
        }
        wait_until_F11_F12_released();
        keyboard_reset_buffer();
    }

    if (scankey(SC_F12) && !ctrl_pressed() && !alt_pressed()) {
        if (!shift_pressed()) linefeed = !linefeed;
        else jump_mark_mode = !jump_mark_mode;
        {
            PlayerLock lock;
            status_refresh();
        }
        wait_until_F11_F12_released();
        keyboard_reset_buffer();
    }
}

// MIDI ボードで入力しても debugging を解除しないキー (カーソル移動など)
bool is_navigation_key(uint16_t fkey)
{
    switch (fkey) {
    case kUP: case kDOWN: case kHOME: case kEND: case kLEFT: case kRIGHT:
    case kPgUP: case kPgDOWN: case kCtPgUP: case kCtPgDN: case kTAB: case kShTAB:
    case kCtLbr: case kCtRbr: case kCHlbrk: case kCHrbrk: case kCtHOME: case kCtEND:
    case kAltM: case kAltL: case kCtENTR: case kF1:
        return true;
    }
    if (shift_pressed()) {
        switch (fkey) {
        case kNPHOME: case kNPPgUP: case kNPEND: case kNPPgDN:
            return true;
        }
    }
    return false;
}

// 次の行へ (Pascal の「If page < PRED(patt_len) then If cond then Inc(page) else else If cycle_pattern then page := 0」)
void next_line(uint8_t& page, bool cond)
{
    if (page < songdata().patt_len - 1) {
        if (cond) ++page;
    } else if (cycle_pattern) {
        page = 0;
    }
}

// 戻り値は終了の原因となったキー (kESC / kF10)
uint16_t PATTERN_edit(uint8_t& pattern, uint8_t& page, uint8_t& hpos)
{
    uint8_t chan;
    {
        PlayerLock lock;
        chan = count_channel(hpos);
    }

    // PATTERN_trace から戻ったとき、F10 で終了するなら true
    const auto run_trace = []() -> bool {
        PlayerUnlock unlock;
        const uint16_t temp1 = PATTERN_trace();
        return temp1 == kF10 || _force_program_quit;
    };

    for (;;) {
        if (_force_program_quit) return kESC;

        {
            PlayerLock lock;
            const uint16_t last_line = static_cast<uint16_t>(songdata().patt_len - 1);
            if (page > last_line) page = B(last_line);
            normalize_track_position(hpos);
            PATTERN_ORDER_page_refresh(pattord_page);
            PATTERN_page_refresh(page);
        }

        if (command_typing != 0) GotoXY(B(8 + pos4(hpos)), B(11 + (MAX_PATTERN_ROWS / 2 - 1)));
        else GotoXY(B(8 + pos3[hpos]), B(11 + (MAX_PATTERN_ROWS / 2 - 1)));

        if (use_large_cursor) WideCursor();
        else ThinCursor();

        // デバッグ情報で Space を押して再生を始めた場合はトレースに入る
        const auto trace_after_debug_info = [&]() -> bool {
            if (!tracing) return false;
            tracing = false;
            return run_trace();
        };

        if (ctrl_pressed() && alt_pressed()) {
            DEBUG_INFO();
            {
                PlayerLock lock;
                PATTERN_ORDER_page_refresh(pattord_page);
                PATTERN_page_refresh(pattern_page);
            }
            if (trace_after_debug_info()) return kF10;
            continue;
        }

        const uint8_t old_patt_page = page;
        const uint8_t old_hpos = hpos;
        uint16_t fkey = process_events();
        if (fkey == WORD_NULL) {
            if (!keypressed()) {
                draw_screen();
                process_F11_F12();
                continue;
            }
            fkey = getkey();
        }
        if (_force_program_quit) return kESC;

        {
            PlayerLock lock;
            A2Player& p = P();
            FixedSongData& sd = songdata();
            const uint16_t last_line = static_cast<uint16_t>(sd.patt_len - 1);
            const uint8_t n = B(_pattedit_lastpos / MAX_TRACKS);
            const uint8_t page_step = B(std::min<int>(16, sd.patt_len));
            bool nope = false;
            bool trace = false;
            Chunk chunk{};
            Chunk chunk2{};

            // Shift+カーソルキーでのブロックの選択は未実装

            switch (fkey) {
            case kCHlbrk:
            case kCtLbr:
                if (!shift_pressed() && current_inst > 1) {
                    if (!(marked_instruments() == 2)) {
                        --current_inst;
                        reset_marked_instruments();
                    } else if (!(current_inst == HI(get_4op_to_test()) || current_inst == LO(get_4op_to_test()))) {
                        --current_inst;
                    } else {
                        while (current_inst > 1 && (current_inst == HI(get_4op_to_test()) || current_inst == LO(get_4op_to_test())))
                            --current_inst;
                    }
                    instrum_page = current_inst;
                    keyboard_reset_buffer();
                }
                break;

            case kCHrbrk:
            case kCtRbr:
                if (!shift_pressed() && current_inst < 255) {
                    if (!(marked_instruments() == 2)) {
                        ++current_inst;
                        reset_marked_instruments();
                    } else if (!(current_inst == HI(get_4op_to_test()) || current_inst == LO(get_4op_to_test()))) {
                        ++current_inst;
                    } else {
                        while (current_inst < 255 && (current_inst == HI(get_4op_to_test()) || current_inst == LO(get_4op_to_test())))
                            ++current_inst;
                    }
                    instrum_page = current_inst;
                    keyboard_reset_buffer();
                }
                break;

            case kLEFT:
                if (marking) {
                    if (hpos > n) hpos = B(hpos - n);
                    else if (chan_pos > 1) --chan_pos;
                } else if (hpos > 1) {
                    --hpos;
                } else if (chan_pos > 1) {
                    --chan_pos;
                    hpos = n;
                } else if (cycle_pattern) {
                    chan_pos = last_chan_pos();
                    hpos = last_hpos();
                }
                break;

            case kRIGHT:
                if (marking) {
                    if (hpos <= last_hpos() - n) hpos = B(hpos + n);
                    else if (chan_pos < last_chan_pos()) ++chan_pos;
                } else if (hpos < last_hpos()) {
                    ++hpos;
                } else if (chan_pos < last_chan_pos()) {
                    ++chan_pos;
                    hpos = B(last_hpos() - (n - 1));
                } else if (cycle_pattern) {
                    chan_pos = 1;
                    hpos = 1;
                }
                break;

            case kUP:
                if (shift_pressed() && track_notes) {
                    // ノートレコーダーの補正 (未実装)
                } else if (page > 0) {
                    --page;
                } else if (!marking && cycle_pattern) {
                    page = B(last_line);
                }
                break;

            case kDOWN:
                if (shift_pressed() && track_notes) {
                    // ノートレコーダーの補正 (未実装)
                } else if (page < last_line) {
                    ++page;
                } else if (!marking && cycle_pattern) {
                    page = 0;
                }
                break;

            case kCHplus:
            case kNPplus:
                if (!marking) {
                    if (pattern < 127) ++pattern;
                }
                break;

            case kCHmins:
            case kNPmins:
                if (!marking) {
                    if (pattern > 0) --pattern;
                }
                break;

            case kPgUP:
                if (!shift_pressed() || marking) {
                    if (page > page_step) page = B(page - page_step);
                    else page = 0;
                } else if (pattern > 0) {
                    --pattern;
                }
                break;

            case kPgDOWN:
                if (!shift_pressed() || marking) {
                    if (page < last_line - page_step) page = B(page + page_step);
                    else page = B(last_line);
                } else if (pattern < 127) {
                    ++pattern;
                }
                break;

            case kNPUp:
            case kNPDown:
                if (NumLock()) nope = true;
                break;

            case kNPPgUP:
                if (NumLock()) {
                    nope = true;
                } else if (shift_pressed()) {
                    if (!marking && pattern > 0) {
                        --pattern;
                    } else if (marking) {
                        if (page > page_step) page = B(page - page_step);
                        else page = 0;
                    }
                }
                break;

            case kNPPgDN:
                if (NumLock()) {
                    nope = true;
                } else if (shift_pressed()) {
                    if (!marking && pattern < 127) {
                        ++pattern;
                    } else if (marking) {
                        if (page < last_line - page_step) page = B(page + page_step);
                        else page = B(last_line);
                    }
                }
                break;

            case kCtHOME:
                if (pattern > 0) {
                    --pattern;
                    page = B(last_line);
                }
                break;

            case kCtEND:
                if (pattern < 127) {
                    ++pattern;
                    page = 0;
                }
                break;

            case kHOME:
                if (!shift_pressed() || marking) {
                    if (page != 0) page = 0;
                    else { chan_pos = 1; hpos = 1; }
                } else if (chan_pos + hpos - 1 != 1) {
                    chan_pos = 1;
                    hpos = 1;
                } else if (page != 0) {
                    page = 0;
                } else {
                    pattern = 0;
                }
                break;

            case kEND:
                if (!shift_pressed() || marking) {
                    if (page != last_line) page = B(last_line);
                    else { chan_pos = last_chan_pos(); hpos = last_hpos(); }
                } else if (chan_pos + hpos != last_chan_pos() + last_hpos()) {
                    chan_pos = last_chan_pos();
                    hpos = last_hpos();
                } else if (page != last_line) {
                    page = B(last_line);
                } else {
                    pattern = 127;
                }
                break;

            case kNPHOME:
                if (NumLock()) {
                    nope = true;
                } else if (shift_pressed()) {
                    if (!marking) {
                        if (chan_pos + hpos - 1 != 1) { chan_pos = 1; hpos = 1; }
                        else if (page != 0) page = 0;
                        else pattern = 0;
                    } else if (page != 0) {
                        page = 0;
                    } else {
                        chan_pos = 1;
                        hpos = 1;
                    }
                }
                break;

            case kNPEND:
                if (NumLock()) {
                    nope = true;
                } else if (shift_pressed()) {
                    if (!marking) {
                        if (chan_pos + hpos != last_chan_pos() + last_hpos()) { chan_pos = last_chan_pos(); hpos = last_hpos(); }
                        else if (page != last_line) page = B(last_line);
                        else pattern = 127;
                    } else if (page != last_line) {
                        page = B(last_line);
                    } else {
                        chan_pos = last_chan_pos();
                        hpos = last_hpos();
                    }
                }
                break;

            case kAltL:
                // 行の印の設定 (LINE_MARKING_SETUP) は未実装
                break;

            case kAltM:
                if (p.mark_line != 0) {
                    mark_lines = !mark_lines;
                    PATTERN_ORDER_page_refresh(pattord_page);
                    PATTERN_page_refresh(pattern_page);
                }
                break;

            case kShTAB:
                if (hpos > n) {
                    hpos = B(hpos - n);
                    if (!keep_track_pos) {
                        const uint8_t temp = hpos % n > 0 ? B(hpos % n - 1) : B(n - 1);
                        hpos = B(hpos - temp);
                    }
                } else if (chan_pos > 1) {
                    --chan_pos;
                    if (!keep_track_pos) {
                        const uint8_t temp = hpos % n > 0 ? B(hpos % n - 1) : B(n - 1);
                        hpos = B(hpos - temp);
                    }
                } else if (cycle_pattern) {
                    chan_pos = last_chan_pos();
                    if (!keep_track_pos) hpos = last_hpos();
                }
                break;

            case kTAB:
                if (hpos <= last_hpos() - n) {
                    hpos = B(hpos + n);
                    if (!keep_track_pos) {
                        const uint8_t temp = hpos % n > 0 ? B(hpos % n - 1) : B(n - 1);
                        hpos = B(hpos - temp);
                    }
                } else if (chan_pos < last_chan_pos()) {
                    ++chan_pos;
                    if (!keep_track_pos) {
                        const uint8_t temp = hpos % n > 0 ? B(hpos % n - 1) : B(n - 1);
                        hpos = B(hpos - temp);
                    }
                } else if (cycle_pattern) {
                    chan_pos = 1;
                    if (!keep_track_pos) hpos = 1;
                }
                break;

            case kSPACE:
                if (ctrl_pressed() && !shift_pressed() && !alt_pressed()) {
                    // ノートレコーダー (Ctrl+Space) は未実装
                    fkey = WORD_NULL;
                } else if (track_notes) {
                    track_notes_ins = !alt_pressed();
                } else if (!ctrl_pressed() && shift_pressed() && !alt_pressed()) {
                    midiboard = !midiboard;
                    fkey = WORD_NULL;
                } else if (!shift_pressed() && !ctrl_pressed() && !alt_pressed()) {
                    if (!midiboard) next_line(page, linefeed || !lf_in_mboard_mode);
                }
                break;

            case kINSERT:
            case kNPins:
            case kDELETE:
            case kNPdel: {
                const bool np = fkey == kNPins || fkey == kNPdel;
                if (np && NumLock()) {
                    nope = true;
                    break;
                }
                if (np && !shift_pressed()) break;
                _save_pattern_to_undo();
                // Shift を押しているときは全トラック
                const int ch0 = shift_pressed() ? 1 : chan;
                const int ch1 = shift_pressed() ? sd.nm_tracks : chan;
                if (fkey == kINSERT || fkey == kNPins) {
                    for (int temp1 = sd.patt_len - 2; temp1 >= page; --temp1) {
                        for (int temp2 = ch0; temp2 <= ch1; ++temp2) {
                            get_chunk(pattern, B(temp1), B(temp2), chunk);
                            put_chunk(pattern, B(temp1 + 1), B(temp2), chunk);
                        }
                    }
                    for (int temp2 = ch0; temp2 <= ch1; ++temp2) {
                        chunk = Chunk{};
                        put_chunk(pattern, page, B(temp2), chunk);
                    }
                } else {
                    for (int temp1 = page; temp1 <= sd.patt_len - 2; ++temp1) {
                        for (int temp2 = ch0; temp2 <= ch1; ++temp2) {
                            get_chunk(pattern, B(temp1 + 1), B(temp2), chunk);
                            put_chunk(pattern, B(temp1), B(temp2), chunk);
                        }
                    }
                    for (int temp2 = ch0; temp2 <= ch1; ++temp2) {
                        chunk = Chunk{};
                        put_chunk(pattern, B(last_line), B(temp2), chunk);
                    }
                }
                break;
            }

            case kAltB:
            case kAltQ:
            case kCtrlK:
            case kCtrlC:
            case kAltP:
            case kCtrlV:
            case kAltV:
            case kCtrlX:
            case kCtrlN:
            case kAltC:
            case kCtrlB:
            case kCtrlT:
            case kCtrlR:
            case kCtrlO:
            case kCtrlP:
            case kCtrlF:
            case kCtrlH:
            case kCtrlI:
            case kCtrlE:
            case kCtrlQ:
            case kCtrlG:
            case kCtrlM:
            case kF1:
            case kShF6:
            case kAltF6:
            case kCtrlF8:
            case kCtrlF9:
                // ブロック操作・ノートレコーダー・各種画面 (未実装)
                break;

            case kBkSPC:
                if (track_notes) {
                    // ノートレコーダー (未実装)
                } else {
                    get_chunk(pattern, pattern_page, chan, chunk);
                    const uint8_t cp = count_pos(hpos);
                    // 4OP の組の相手のトラックのノート (と楽器) も消す
                    const auto clear_pair = [&](bool clear_instr) {
                        if (p.is_4op_chan(chan) && in_4op_tracks_lo(chan)) {
                            get_chunk(pattern, pattern_page, B(chan - 1), chunk2);
                            chunk2.note = 0;
                            if (clear_instr) chunk2.instr_def = 0;
                            put_chunk(pattern, pattern_page, B(chan - 1), chunk2);
                        } else if (p.is_4op_chan(chan) && in_4op_tracks_hi(chan)) {
                            get_chunk(pattern, pattern_page, B(chan + 1), chunk2);
                            chunk2.note = 0;
                            if (clear_instr) chunk2.instr_def = 0;
                            put_chunk(pattern, pattern_page, B(chan + 1), chunk2);
                        }
                    };
                    // 0: ノート、1: 楽器、2: エフェクト 1、3: エフェクト 2
                    int field = -1;
                    if (command_typing != 0) {
                        if (cp <= 1) field = 0;
                        else if (cp <= 3) field = 1;
                        else if (cp <= 6) field = 2;
                        else if (cp <= 9) field = 3;
                    } else if (cp <= 3) {
                        field = cp;
                    }
                    switch (field) {
                    case 0:
                        chunk.note = 0;
                        clear_pair(false);
                        if (midiboard) {
                            chunk.instr_def = 0;
                            clear_pair(true);
                        }
                        break;
                    case 1:
                        chunk.instr_def = 0;
                        clear_pair(true);
                        break;
                    case 2:
                        chunk.effect_def = 0;
                        chunk.effect = 0;
                        break;
                    case 3:
                        chunk.effect_def2 = 0;
                        chunk.effect2 = 0;
                        break;
                    }
                    put_chunk(pattern, pattern_page, chan, chunk);
                    switch (backspace_dir) {
                    case 1:
                        next_line(page, linefeed);
                        break;
                    case 2:
                        if (page > 0) --page;
                        else if (cycle_pattern) page = B(last_line);
                        break;
                    }
                }
                break;

            case kCtPgUP:
            case kCtPgDN:
                _save_pattern_to_undo();
                // ブロックを選択しているときはブロック (ブロックの選択は未実装)
                transpose_custom_area(fkey == kCtPgUP ? TransposeType::Up : TransposeType::Down, TransposeType::AllIns,
                                      pattern, pattern, chan, chan, page, page, 1);
                break;

            case kCtrlZ:
                _restore_pattern_from_undo();
                break;

            case kCtrlD:
                if (!marking) {
                    {
                        PlayerUnlock unlock;
                        DEBUG_INFO();
                    }
                    if (tracing) {
                        tracing = false;
                        if (run_trace()) return kF10;
                    }
                }
                break;

            case kCtLEFT:
                if (track_notes) {
                    // ノートレコーダー (未実装)
                } else if (!p.debugging && p.play_status == PlayStatus::Playing) {
                    p.rewind = true;
                }
                break;

            case kCtRGHT:
                if (track_notes) {
                    // ノートレコーダー (未実装)
                } else if (p.play_status == PlayStatus::Playing) {
                    p.fast_forward = true;
                }
                break;

            case kCtENTR:
                if (p.play_single_patt) {
                    p.current_line = 0;
                } else {
                    no_status_refresh = true;
                    {
                        PlayerUnlock unlock;
                        fade_out_playback(false);
                    }
                    if (p.current_order < 0x7f && p.play_status != PlayStatus::Stopped) {
                        if (p.order_at(B(p.current_order + 1)) < 0x80) {
                            calibrate_player(B(p.current_order + 1), 0, false);
                        } else if (p.calc_following_order(B(p.current_order + 1)) != -1) {
                            calibrate_player(B(p.calc_following_order(B(p.current_order + 1))), 0, false);
                        }
                    } else if (p.debugging && p.play_status == PlayStatus::Stopped) {
                        // enter_debug_mode(SUCC(pattern), 0) (デバッグモードは未実装)
                    }
                    no_status_refresh = false;
                }
                break;

            case kF2: case kShF2: case kCtrlS:
            case kCtrlF2:
            case kAltF2:
            case kF4: case kCtrlA: {
                PlayerUnlock unlock;
                process_file_keys(fkey, false);
                break;
            }

            case kF3:
            case kShF3:
            case kCtrlL: {
                PlayerUnlock unlock;
                FILE_open();
                break;
            }

            case kF7: {
                {
                    PlayerUnlock unlock;
                    fade_out_playback(false);
                }
                stop_playing();
                if (p.play_status != PlayStatus::Stopped) std::fill(std::begin(p.ai_table), std::end(p.ai_table), uint8_t(0));
                break;
            }

            case kF5: case kAltF5: case kShF5:
            case kF6:
            case kF8: case kAltF8: case kShF8:
            case kF9: case kAltF9: case kShF9:
                trace = process_play_keys(fkey, pattern);
                break;

            case kF10:
            case kESC:
                if (fkey == kESC && track_notes) {
                    cancel_note_recorder();
                } else {
                    bool quit;
                    {
                        PlayerUnlock unlock;
                        quit = QUIT_request();
                    }
                    if (quit) return kESC;
                    if (fkey == kESC) nope = true;
                }
                break;

            case kENTER:
                if (!shift_pressed()) {
                    nope = true;
                } else {
                    // 固定ノートの切り替え (ブロックを選択しているときはブロック。ブロックの選択は未実装)
                    _save_pattern_to_undo();
                    get_chunk(pattern, page, chan, chunk);
                    if (chunk.note >= 1 && chunk.note <= 12 * 8 + 1) chunk.note = B(chunk.note + fixed_note_flag);
                    else if (chunk.note >= fixed_note_flag + 1 && chunk.note <= fixed_note_flag + 12 * 8 + 1)
                        chunk.note = B(chunk.note - fixed_note_flag);
                    put_chunk(pattern, page, chan, chunk);
                }
                break;

            default:
                if (!process_channel_keys(fkey) && !scankey(SC_F11) && !scankey(SC_F12)) nope = true;
                break;
            }

            if (pending_track_dialog) {
                pending_track_dialog = false;
                PlayerUnlock unlock;
                track_onoff_dialog();
            }

            if (trace && run_trace()) return kF10;

            // --- MIDI ボード ---
            if ((nope || fkey == kSPACE || fkey == kWeird) && midiboard) {
                nope = false;
                const uint8_t curr_ch = track_notes ? track_chan_start : chan;
                uint8_t track_ch = 0;
                bool flag = false;
                uint8_t track_ch_key[21];   // [1..20]
                std::fill(std::begin(track_ch_key), std::end(track_ch_key), BYTE_NULL);
                if (!ctrl_pressed() && !alt_pressed()) {
                    for (int idx = 1; idx <= 29; ++idx) {
                        if (scankey(board_scancodes[idx - 1])) {
                            flag = true;
                            if (!track_notes || !(track_ch < nm_track_chan)) {
                                track_ch_key[1] = B(idx);
                                break;
                            }
                            ++track_ch;
                            track_ch_key[track_ch] = B(idx);
                        }
                    }
                }

                const uint16_t fkey_X = fkey;
                const uint8_t cp = count_pos(hpos);
                if (fkey == kSPACE || ((fkey == kWeird || (flag && cp == 0)) && !(fkey == kWeird && cp >= 2)) || track_notes) {
                    const uint8_t temp1 = track_notes ? nm_track_chan : 1;
                    uint8_t idx2 = 1;

                    if (track_notes && !((command_typing == 0 && cp == 0) || (command_typing != 0 && cp < 2))) {
                        if (fkey != kSPACE && fkey != kENTER) nope = true;
                    }

                    if (!nope && fkey != kSPACE) {
                        for (int idx = 1; idx <= temp1; ++idx) {
                            const int note = track_ch_key[idx2] + 12 * (current_octave - 1);
                            if (!(fkey == kWeird || (track_ch_key[idx2] != BYTE_NULL && note >= 1 && note <= 12 * 8 + 1))) continue;
                            if (fkey == kSPACE ||
                                (track_notes && mark_lines && p.mark_line != 0 && pattern_page % p.mark_line != 0))
                                continue;

                            const uint8_t ch = B(curr_ch + idx - 1);
                            get_chunk(pattern, pattern_page, ch, chunk);
                            if (fkey != kWeird) {
                                if (!right_shift_pressed()) chunk.note = B(note);
                                else chunk.note = B(note + fixed_note_flag);
                                if (!(chunk.effect_def == ef_TonePortamento || chunk.effect_def == ef_TPortamVolSlide)) {
                                    const uint16_t pair = get_4op_to_test();
                                    if (!(pair != 0)) chunk.instr_def = current_inst;
                                    else if (in_4op_tracks_lo(ch)) chunk.instr_def = LO(pair);
                                    else chunk.instr_def = HI(pair);
                                    if (p.is_4op_chan(ch) && in_4op_tracks_lo(ch)) {
                                        const uint8_t ch2 = B(ch - 1);
                                        get_chunk(pattern, pattern_page, ch2, chunk2);
                                        chunk2.note = 0;
                                        if (!track_notes || track_notes_ins) chunk2.instr_def = HI(pair) != 0 ? HI(pair) : current_inst;
                                        else chunk2.instr_def = std::max<uint8_t>(p.voice_table[ch2], 1);
                                        if (p.channel_flag[ch2]) put_chunk(pattern, pattern_page, ch2, chunk2);
                                    } else if (p.is_4op_chan(ch) && in_4op_tracks_hi(ch)) {
                                        const uint8_t ch2 = B(ch + 1);
                                        get_chunk(pattern, pattern_page, ch2, chunk2);
                                        chunk2.note = 0;
                                        if (!track_notes || track_notes_ins) chunk2.instr_def = LO(pair) != 0 ? LO(pair) : current_inst;
                                        else chunk2.instr_def = std::max<uint8_t>(p.voice_table[ch2], 1);
                                        if (p.channel_flag[ch2]) put_chunk(pattern, pattern_page, ch2, chunk2);
                                    }
                                }
                            } else {
                                chunk.note = BYTE_NULL;
                                chunk.instr_def = 0;
                            }

                            if (p.channel_flag[ch] && !p.ignore_note_once[ch]) {
                                p.ignore_note_once[ch] = true;
                                put_chunk(pattern_patt, pattern_page, ch, chunk);
                                if (chunk.instr_def != 0) p.load_instrument(instr_data_bytes(chunk.instr_def), ch);
                                if (p.is_4op_chan(ch)) {
                                    const uint8_t ch2 = in_4op_tracks_lo(ch) ? B(ch - 1) : B(ch + 1);
                                    get_chunk(pattern_patt, pattern_page, ch2, chunk2);
                                    p.load_instrument(instr_data_bytes(chunk2.instr_def), ch2);
                                }
                                if (chunk.note != BYTE_NULL) p.output_note(chunk.note & 0x7f, chunk.instr_def, ch, true, true);
                                ++idx2;
                            }
                        }
                    }

                    if (!nope) {
                        fkey = kSPACE;
                        if (track_notes) {
                            // ノートレコーダー (未実装)
                        } else if (sd.pattern_order[pattern] != BYTE_NULL &&
                                   !(p.play_status != PlayStatus::Stopped && fkey_X == kSPACE)) {
                            preview_pattern_line(pattern, page);
                            next_line(page, linefeed || !lf_in_mboard_mode);

                            if ((left_shift_pressed() || jump_mark_mode) && p.mark_line != 0 && mark_lines) {
                                const uint8_t old_line = page;
                                while (page % p.mark_line != 0) {
                                    if (page < last_line) {
                                        ++page;
                                    } else if (cycle_pattern) {
                                        page = 0;
                                    } else {
                                        page = old_line;
                                        break;
                                    }
                                }
                            }
                        } else {
                            next_line(page, linefeed || !lf_in_mboard_mode);
                        }
                    }
                } else if (!is_navigation_key(fkey)) {
                    nope = true;
                    p.debugging = false;
                }
            } else if (!is_navigation_key(fkey)) {
                p.debugging = false;
            }

            if (track_notes && chan != count_channel(old_hpos)) cancel_note_recorder();

            if (page != old_patt_page || (!linefeed && lf_in_mboard_mode)) {
                for (int idx = 1; idx <= 20; ++idx) p.ignore_note_once[idx] = false;
            }

            chan = count_channel(hpos);

            // --- MIDI ボードを使わないときのノートの入力 ---
            const char key_chr = UpCase(static_cast<char>(LO(fkey)));
            const auto is_note_letter = [](char c) { return c == 'A' || c == UpCase(b_note) || (c >= 'C' && c <= 'G'); };
            if (nope && !midiboard && count_pos(hpos) == 0 && is_note_letter(key_chr)) {
                nope = false;
                is_setting.append_enabled = false;
                is_setting.character_set = CharRange('1', '9') | CharSet("a") | CharSet(std::string(1, b_note)) |
                                           CharRange('c', 'g') | CharSet("A") | CharSet(std::string(1, UpCase(b_note))) |
                                           CharRange('C', 'F') | CharSet("#-");
                is_environment.locate_pos = 2;
                std::string tstr(1, static_cast<char>(LO(fkey)));
                is_setting.terminate_keys[2] = kTAB;

                do {
                    // tstr が空になったときの tstr[1] (Pascal の文字列の残り)
                    const char stale_1st = tstr.empty() ? '\0' : tstr[0];
                    {
                        PlayerUnlock unlock;
                        tstr = InputStr(tstr, B(8 + pos3[1 + (chan - (chan_pos - 1) - 1) * 4]), B(11 + (MAX_PATTERN_ROWS / 2 - 1)), 3, 3,
                                        pattern_input_bckg + pattern_input, pattern_input_warn + pattern_input);
                    }
                    is_setting.append_enabled = true;

                    const char first = tstr.empty() ? stale_1st : tstr[0];
                    if (is_note_letter(UpCase(first)) &&
                        (is_environment.keystroke == kENTER || is_environment.keystroke == kTAB)) {
                        nope = false;
                        if (tstr.size() == 2) {
                            if (tstr[1] >= '1' && tstr[1] <= '9') tstr.insert(1, "-");
                            else if (tstr[1] == '-' || tstr[1] == '#') tstr += Num2str(current_octave, 10);
                        }
                        if (tstr.size() == 1) tstr += "-" + Num2str(current_octave, 10);

                        // 見つかったノートを書き込む (4OP の組の相手のノートは消す)
                        const auto put_note = [&](int temp1) {
                            nope = true;
                            get_chunk(pattern, page, chan, chunk);
                            chunk.note = B(temp1);
                            put_chunk(pattern, page, chan, chunk);
                            if (p.is_4op_chan(chan)) {
                                const uint8_t ch2 = in_4op_tracks_hi(chan) ? B(chan + 1) : B(chan - 1);
                                get_chunk(pattern, page, ch2, chunk2);
                                chunk2.note = 0;
                                put_chunk(pattern, page, ch2, chunk2);
                            }
                        };
                        for (int temp1 = 1; temp1 <= 12 * 8 + 1; ++temp1) {
                            if (Upper(tstr) == note_layout[temp1]) {
                                put_note(temp1);
                                break;
                            }
                        }
                        if (!nope && tstr.size() == 2) {
                            for (int temp1 = 1; temp1 <= 12 * 8 + 1; ++temp1) {
                                if (Copy(Upper(tstr), 1, 2) == Copy(note_layout[temp1], 1, 2)) {
                                    put_note(temp1);
                                    break;
                                }
                            }
                        }

                        if (nope && linefeed && is_environment.keystroke == kENTER) next_line(page, true);
                        if (tstr.empty()) nope = true;
                        if (nope && is_environment.keystroke == kTAB && hpos + 1 <= last_hpos()) ++hpos;
                    }
                } while (!(is_environment.keystroke == kESC || nope || _force_program_quit));
                is_setting.terminate_keys[2] = 0;
                nope = false;
            }

            // command_typing = 0 (コマンド入力なし) の楽器・エフェクトの入力は、設定画面とともに追加する

            // --- コマンド入力 (カーソル位置の桁を直接書き換える) ---
            const auto is_command_chr = [](char c) {
                return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || std::strchr("&%!@=#$~^`><", c) != nullptr;
            };
            const auto is_hex_chr = [](char c) { return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'); };
            const uint8_t cp = count_pos(hpos);
            if (!marking && nope && command_typing != 0 && cp > 0 && key_chr != '\0' && is_command_chr(key_chr)) {
                const char key_raw = static_cast<char>(LO(fkey));
                const int key_dec = Str2num(std::string(1, key_raw), 10);
                const int key_hex = Str2num(std::string(1, key_raw), 16);
                // Shift を押したまま入力できるエフェクト
                const auto shift_ok = [&]() {
                    switch (FX(key_raw)) {
                    case ef_Extended2: case ef_Extended3: case ef_SetGlobalVolume:
                    case ef_ExtraFineArpeggio: case ef_ExtraFineVibrato: case ef_ExtraFineTremolo:
                    case ef_ForceInsVolume: case ef_SwapArpeggio: case ef_SwapVibrato:
                    case ef_SetCustomSpeedTab: case ef_GlobalFSlideUp: case ef_GlobalFSlideDown:
                        return true;
                    }
                    return false;
                };
                // 書き込むチャンネル (ノートレコーダーのときは複数)
                const int nm = track_notes ? nm_track_chan : 1;
                const int ch_start = track_notes ? track_chan_start : chan;

                switch (cp) {
                case 1:
                    if (!(key_chr >= '1' && key_chr <= '9')) {
                        nope = false;
                    } else {
                        nope = true;
                        get_chunk(pattern, page, chan, chunk);
                        if (chunk.note >= 1 && chunk.note <= 12 * 8 + 1)
                            chunk.note = B((chunk.note - 1) % 12 + 1 + 12 * (key_dec - 1));
                        else if (chunk.note >= fixed_note_flag + 1 && chunk.note <= fixed_note_flag + 12 * 8 + 1)
                            chunk.note = B((chunk.note - 1) % 12 + 1 + 12 * (key_dec - 1) + fixed_note_flag);
                        if ((chunk.note >= 1 && chunk.note <= 12 * 8 + 1) ||
                            (chunk.note >= fixed_note_flag + 1 && chunk.note <= fixed_note_flag + 12 * 8 + 1))
                            put_chunk(pattern, page, chan, chunk);
                        else
                            nope = false;
                    }
                    break;

                case 2:
                case 3:
                    if (!is_hex_chr(key_chr)) {
                        nope = false;
                    } else {
                        nope = true;
                        for (int idx = 1; idx <= nm; ++idx) {
                            const uint8_t ch = B(ch_start + idx - 1);
                            if (track_notes && !p.channel_flag[ch]) continue;
                            get_chunk(pattern, page, ch, chunk);
                            if (track_notes && idx > 1) {
                                get_chunk(pattern, page, track_chan_start, chunk2);
                                chunk.instr_def = chunk2.instr_def;
                            }
                            if (cp == 2) chunk.instr_def = B(key_hex * 0x10 + (chunk.instr_def & 0x0f));
                            else chunk.instr_def = B(key_hex + (chunk.instr_def & 0xf0));
                            if (!shift_pressed()) {
                                put_chunk(pattern, page, ch, chunk);
                                if (chunk.instr_def != 0 && update_ins) {
                                    current_inst = chunk.instr_def;
                                    instrum_page = current_inst;
                                    reset_marked_instruments();
                                }
                            } else {
                                nope = false;
                            }
                        }
                    }
                    break;

                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9: {
                    nope = true;
                    const bool fx2 = cp >= 7;
                    const int col = fx2 ? cp - 3 : cp;   // 4: 種類、5: 上位桁、6: 下位桁
                    if (track_notes && nm_track_chan > 1) _save_pattern_to_undo();
                    for (int idx = 1; idx <= nm; ++idx) {
                        const uint8_t ch = B(ch_start + idx - 1);
                        if (track_notes && !p.channel_flag[ch]) continue;
                        get_chunk(pattern, page, ch, chunk);
                        uint8_t& def = fx2 ? chunk.effect_def2 : chunk.effect_def;
                        uint8_t& val = fx2 ? chunk.effect2 : chunk.effect;
                        if (track_notes && idx > 1) {
                            get_chunk(pattern, page, track_chan_start, chunk2);
                            def = fx2 ? chunk2.effect_def2 : chunk2.effect_def;
                            val = fx2 ? chunk2.effect2 : chunk2.effect;
                        }
                        switch (col) {
                        case 4:
                            def = FX(key_raw);
                            if ((def == ef_SetSpeed || def == ef_SetTempo) && val == 0) {
                                if (def == ef_SetSpeed) val = sd.speed;
                                else val = sd.tempo;
                            }
                            break;
                        case 5:
                            if (!is_hex_chr(key_chr)) nope = false;
                            else val = B(key_hex * 0x10 + (val & 0x0f));
                            break;
                        case 6:
                            if (!is_hex_chr(key_chr)) nope = false;
                            else val = B(key_hex + (val & 0xf0));
                            break;
                        }
                        if (correct_range(def, val) && !(shift_pressed() && !shift_ok())) put_chunk(pattern, page, ch, chunk);
                        else nope = false;
                    }
                    break;
                }
                }

                if (command_typing == 2 && (cp == 2 || cp == 5 || cp == 8)) {
                    ++hpos;
                } else if (nope && linefeed) {
                    if (command_typing == 2 && (cp == 3 || cp == 6 || cp == 9)) --hpos;
                    next_line(page, linefeed);
                }
            }

            // Enter (ウィンドウの切り替え) は未実装
        }

        draw_screen();
        process_F11_F12();
    }
}

} // namespace

void run_main_screen(HWND hwnd)
{
    g_hwnd = hwnd;
    if (!sound_player()) return;

    init_screen_layout();
    PROGRAM_SCREEN_init();
    {
        PlayerLock lock;
        POSITIONS_reset();
        decay_bars_refresh();
        status_refresh();
    }

    GotoXY(B(8 + pos4(pattern_hpos)), B(11 + (MAX_PATTERN_ROWS / 2 - 1)));
    if (use_large_cursor) WideCursor();
    else ThinCursor();

    g_timer_last = timeGetTime();
    keyboard_init();
    {
        PlayerLock lock;
        stop_playing();
    }

    sys_set_realtime_proc(realtime_gfx_poll_proc);
    move_to_screen_routine = move2screen;
    do_synchronize = true;
    for (;;) {
        const uint16_t fkey = PATTERN_edit(pattern_patt, pattern_page, pattern_hpos);
        if (fkey == kESC || fkey == kF10 || _force_program_quit) break;
    }

    if (!tracing) {
        if (use_large_cursor) WideCursor();
        else ThinCursor();
    }
    do_synchronize = false;
    fade_out_playback(true);
    {
        PlayerLock lock;
        stop_playing();
    }
    sys_set_realtime_proc(nullptr);
    move_to_screen_routine = nullptr;
}

} // namespace at2
