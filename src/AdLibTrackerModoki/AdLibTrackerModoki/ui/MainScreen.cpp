#include "MainScreen.h"

#include <commdlg.h>
#include <timeapi.h>

#include <algorithm>
#include <cmath>
#include <mutex>
#include <string>

#include "MainScreenData.h"
#include "StringIO.h"
#include "TxtScrIO.h"
#include "../Resource.h"
#include "../app/Sound.h"
#include "../core/player/A2Player.h"
#include "../core/song/EffectDefs.h"
#include "../core/song/ModuleLoader.h"
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

const std::string _ADT2_TITLE_STRING_ = "/\xB4" "DLiB TR/\xB4" "CK3R ][";

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
bool use_large_cursor = false;
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

uint8_t current_octave = 4;
uint8_t current_inst = 1;
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

void STATUS_LINE_refresh()
{
    if (really_no_status_refresh) return;
    A2Player& p = P();
    const int ps = play_status();
    const uint8_t ys = B(MAX_PATTERN_ROWS + 12);

    // get_4op_to_test (4OP 用に選んだ楽器の組) は楽器選択が未実装のため常に 0
    show_cstr(40, ys, "~[~" + byte2hex(current_inst) + "~]~\xC4\xC4\xC4",
              main_background + main_border, main_background + main_stat_line);

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
        case IDM_FILE_OPEN: return kF3;
        case IDM_PLAY_PLAY: return kF5;
        case IDM_PLAY_PAUSE: return kF6;
        case IDM_PLAY_STOP: return kF7;
        case IDM_EXIT: _force_program_quit = true; return WORD_NULL;
        case IDM_VIEW_MODE0: change_screen_mode(0); break;
        case IDM_VIEW_MODE1: change_screen_mode(1); break;
        case IDM_VIEW_MODE2: change_screen_mode(2); break;
        case IDM_EMU_AT2: change_emulator(EmulatorType::At2); break;
        case IDM_EMU_NUKED: change_emulator(EmulatorType::Nuked); break;
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

// トレース中・編集中で共通のトラック操作キー
void process_channel_keys(uint16_t fkey)
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
            }
            // それ以外はトラックを選ぶダイアログを出す (ダイアログの実装とともに対応する)
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

        uint16_t key = process_events();
        if (key == WORD_NULL) {
            if (keypressed()) key = getkey();
        }

        if (key != WORD_NULL) {
            fkey2 = key;
            if (fkey2 == kF3 || fkey2 == kShF3 || fkey2 == kCtrlL) {
                FILE_open();
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
                    calibrate_player(B(target), keep_line ? temp2 : 0, true);
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

                case kCtENTR:
                    if (p.play_single_patt) {
                        p.current_line = 0;
                        PATTERN_ORDER_page_refresh(0);
                        PATTERN_page_refresh(0);
                    }
                    break;

                case kF10:
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
                calibrate_player(p.calc_pattern_pos(pattern), 0, true);
                p.repeat_pattern = repeat;
            }
            return false;
        case PlayStatus::Stopped:
            if (p.calc_pattern_pos(pattern) != BYTE_NULL) {
                p.debugging = false;
                if (nosync) p.no_sync_playing = true;
                calibrate_player(p.calc_pattern_pos(pattern), 0, true);
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

// 戻り値は終了の原因となったキー (kESC / kF10)
uint16_t PATTERN_edit(uint8_t& pattern, uint8_t& page, uint8_t& hpos)
{
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

        uint16_t fkey = process_events();
        if (fkey == WORD_NULL) {
            if (!keypressed()) {
                draw_screen();
                continue;
            }
            fkey = getkey();
        }
        if (_force_program_quit) return kESC;

        bool trace = false;
        switch (fkey) {
        case kF10:
        case kESC:
            return fkey;

        case kF3:
        case kShF3:
        case kCtrlL:
            FILE_open();
            break;

        case kF7:
            fade_out_playback(false);
            {
                PlayerLock lock;
                A2Player& p = P();
                stop_playing();
                if (p.play_status != PlayStatus::Stopped) std::fill(std::begin(p.ai_table), std::end(p.ai_table), uint8_t(0));
            }
            break;

        case kF5: case kAltF5: case kShF5:
        case kF6:
        case kF8: case kAltF8: case kShF8:
        case kF9: case kAltF9: case kShF9:
            trace = process_play_keys(fkey, pattern);
            break;

        default: {
            PlayerLock lock;
            const uint16_t last_line = static_cast<uint16_t>(songdata().patt_len - 1);
            const uint8_t n = B(_pattedit_lastpos / MAX_TRACKS);
            const uint8_t page_step = B(std::min<int>(16, songdata().patt_len));
            switch (fkey) {
            case kLEFT:
                if (hpos > 1) --hpos;
                else if (chan_pos > 1) { --chan_pos; hpos = n; }
                else if (cycle_pattern) { chan_pos = last_chan_pos(); hpos = last_hpos(); }
                break;

            case kRIGHT:
                if (hpos < last_hpos()) ++hpos;
                else if (chan_pos < last_chan_pos()) { ++chan_pos; hpos = B(last_hpos() - (n - 1)); }
                else if (cycle_pattern) { chan_pos = 1; hpos = 1; }
                break;

            case kUP:
                if (page > 0) --page;
                else if (cycle_pattern) page = B(last_line);
                break;

            case kDOWN:
                if (page < last_line) ++page;
                else if (cycle_pattern) page = 0;
                break;

            case kPgUP:
                if (!shift_pressed()) {
                    if (page > page_step) page = B(page - page_step);
                    else page = 0;
                } else if (pattern > 0) {
                    --pattern;
                }
                break;

            case kPgDOWN:
                if (!shift_pressed()) {
                    if (page < last_line - page_step) page = B(page + page_step);
                    else page = B(last_line);
                } else if (pattern < 127) {
                    ++pattern;
                }
                break;

            case kCHplus:
            case kNPplus:
                if (pattern < 127) ++pattern;
                break;

            case kCHmins:
            case kNPmins:
                if (pattern > 0) --pattern;
                break;

            case kCtHOME:
                if (pattern > 0) { --pattern; page = B(last_line); }
                break;

            case kCtEND:
                if (pattern < 127) { ++pattern; page = 0; }
                break;

            case kHOME:
                if (!shift_pressed()) {
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
                if (!shift_pressed()) {
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

            case kShTAB:
                if (hpos > n) {
                    hpos = B(hpos - n);
                    const uint8_t temp = hpos % n > 0 ? B(hpos % n - 1) : B(n - 1);
                    hpos = B(hpos - temp);
                } else if (chan_pos > 1) {
                    --chan_pos;
                    const uint8_t temp = hpos % n > 0 ? B(hpos % n - 1) : B(n - 1);
                    hpos = B(hpos - temp);
                } else if (cycle_pattern) {
                    chan_pos = last_chan_pos();
                    hpos = last_hpos();
                }
                break;

            case kTAB:
                if (hpos <= last_hpos() - n) {
                    hpos = B(hpos + n);
                    const uint8_t temp = hpos % n > 0 ? B(hpos % n - 1) : B(n - 1);
                    hpos = B(hpos - temp);
                } else if (chan_pos < last_chan_pos()) {
                    ++chan_pos;
                    const uint8_t temp = hpos % n > 0 ? B(hpos % n - 1) : B(n - 1);
                    hpos = B(hpos - temp);
                } else if (cycle_pattern) {
                    chan_pos = 1;
                    hpos = 1;
                }
                break;

            case kCtENTR: {
                A2Player& p = P();
                if (p.play_single_patt) p.current_line = 0;
                break;
            }

            default:
                process_channel_keys(fkey);
                break;
            }
            break;
        }
        }

        if (trace) {
            const uint16_t temp1 = PATTERN_trace();
            if (temp1 == kF10 || _force_program_quit) return kF10;
        }
        draw_screen();
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
}

} // namespace at2
