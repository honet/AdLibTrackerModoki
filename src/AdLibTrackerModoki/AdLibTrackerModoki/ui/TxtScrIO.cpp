#include "TxtScrIO.h"

#include <algorithm>
#include <cstring>

namespace at2
{

uint16_t SCREEN_RES_x = 720;
uint16_t SCREEN_RES_y = 480;
uint8_t MAX_COLUMNS = 90;
uint8_t MAX_ROWS = 40;
uint8_t MAX_TRACKS = 5;
uint8_t MAX_ORDER_COLS = 9;
uint8_t MAX_PATTERN_ROWS = 18;
uint8_t INSCTRL_xshift = 0;
int8_t INSCTRL_yshift = 0;
uint8_t INSEDIT_yshift = 0;
uint8_t PATTORD_xshift = 0;
uint8_t GOTOXY_xshift = 0;
int32_t SCREEN_MEM_SIZE = MAX_SCREEN_MEM_SIZE;

tSCREEN_MEM temp_screen;
tSCREEN_MEM temp_screen2;
tSCREEN_MEM screen_backup;
tSCREEN_MEM scr_backup;
tSCREEN_MEM scr_backup2;
tSCREEN_MEM screen_mirror;
tSCREEN_MEM screen_emulator;
tSCREEN_MEM text_screen_shadow;

tSCREEN_MEM_PTR screen_ptr = text_screen_shadow;
tSCREEN_MEM_PTR centered_frame_vdest = text_screen_shadow;
tSCREEN_MEM_PTR move_to_screen_data = nullptr;
uint8_t move_to_screen_area[5] = {};

uint8_t program_screen_mode = 0;

uint8_t MaxLn = 0;
uint8_t MaxCol = 0;
uint8_t hard_maxcol = 0;
uint8_t hard_maxln = 0;
uint8_t work_maxcol = 0;
uint8_t work_maxln = 0;
uint8_t scr_font_width = 0;
uint8_t scr_font_height = 0;

uint8_t area_x1 = 0, area_y1 = 0, area_x2 = 0, area_y2 = 0;
uint8_t scroll_pos0 = 0xff, scroll_pos1 = 0xff, scroll_pos2 = 0xff, scroll_pos3 = 0xff, scroll_pos4 = 0xff;

int32_t cursor_backup = 0;

uint16_t virtual_cur_pos = 0;
uint16_t virtual_cur_shape = 0;
int32_t virtual_screen__first_row = 0;
uint16_t screen_scroll_offset = 0;
bool cursor_sync = false;
int32_t _cursor_blink_factor = 13;
int32_t _cursor_blink_pending_frames = 0;

tFRAME_SETTING fr_setting = { true, false, false, true };

const std::string frame_single = "\xDA\xC4\xBF\xB3\xB3\xC0\xC4\xD9";
const std::string frame_double = "\xC9\xCD\xBB\xBA\xBA\xC8\xCD\xBC";
const std::string frame_double_side = "\xD6\xC4\xB7\xBA\xBA\xD3\xC4\xBD";
const std::string frame_double_top = "\xD5\xCD\xB8\xB3\xB3\xD4\xCD\xBE";

namespace
{
    uint16_t absolute_pos = 0;

    // Pascal の String は最大 255 文字
    uint8_t plen(const std::string& s) { return static_cast<uint8_t>(std::min<size_t>(s.size(), 255)); }

    // 画面メモリへのセル書き込み (オリジナルは範囲外も書き込むが、ここでは画面メモリ内に限定する)
    void put_cell(tSCREEN_MEM_PTR dest, int32_t pos, uint8_t c, uint8_t attr)
    {
        if (pos >= 0 && pos + 1 < MAX_SCREEN_MEM_SIZE) {
            dest[pos] = c;
            dest[pos + 1] = attr;
        }
    }

    void put_byte(tSCREEN_MEM_PTR dest, int32_t pos, uint8_t value)
    {
        if (pos >= 0 && pos < MAX_SCREEN_MEM_SIZE) dest[pos] = value;
    }

    void PosChar(uint8_t x, uint8_t y)
    {
        absolute_pos = static_cast<uint16_t>(((y - 1) * MaxCol + x - 1) << 1);
    }

    void DupChar(uint8_t x, uint8_t y, char c, uint8_t attr, uint8_t count, tSCREEN_MEM_PTR dest)
    {
        PosChar(x, y);
        for (int i = 0; i < count; ++i) put_cell(dest, absolute_pos + i * 2, static_cast<uint8_t>(c), attr);
    }

    // show_* 系の共通処理: 表示中ウィンドウ (area) とその影を避けて書き込む
    struct AreaClip
    {
        uint8_t x12, x21, x22, y11, y21;

        AreaClip()
            : x12(static_cast<uint8_t>(area_x1 + 2)), x21(static_cast<uint8_t>(area_x2 + 1)),
              x22(static_cast<uint8_t>(area_x2 + 2)), y11(static_cast<uint8_t>(area_y1 + 1)),
              y21(static_cast<uint8_t>(area_y2 + 1))
        {
        }

        void write(int32_t pos, uint8_t x, uint8_t y, uint8_t c, uint8_t attr) const
        {
            if ((x >= x12 && x <= x22 && y == y21) || (x >= x21 && x <= x22 && y >= y11 && y <= y21)) {
                // 影の部分は文字だけを書き換える
                put_byte(screen_ptr, pos, c);
            } else if (x < area_x1 || x > area_x2 || y < area_y1 || y > area_y2) {
                // 領域外
                put_cell(screen_ptr, pos, c, attr);
            }
        }
    };

    // show_cstr / show_vcstr / show_cstr_alt の共通処理
    void show_cstr_proc(uint8_t xpos, uint8_t ypos, const std::string& str, uint8_t attr1, uint8_t attr2,
                        char mark, bool vertical)
    {
        uint8_t n = plen(str);
        if (n == 0) return;

        const AreaClip clip;
        size_t s = 0;
        uint8_t i = 1;
        uint8_t a1 = attr1, a2 = attr2;
        for (;;) {
            char c;
            uint8_t flag;
            do {
                c = str[s++];
                if (c == mark) {
                    std::swap(a1, a2);
                    --n;
                    flag = (i > n) ? 1 : 0;
                } else {
                    flag = 2;
                }
            } while (flag == 0);

            if (flag == 1) break;

            const uint8_t x = vertical ? xpos : static_cast<uint8_t>(xpos + i - 1);
            const uint8_t y = vertical ? static_cast<uint8_t>(ypos + i - 1) : ypos;
            const int32_t pos = vertical ? (((ypos + i - 2) * MaxCol + xpos - 1) << 1) : (((ypos - 1) * MaxCol + xpos + i - 2) << 1);
            clip.write(pos, x, y, static_cast<uint8_t>(c), a1);

            ++i;
            if (i > n) break;
        }
    }

    // ShowCStr 系の共通処理 (marks[k] で attrs[0] と attrs[k+1] を入れ替える)
    void show_multi_cstr(tSCREEN_MEM_PTR dest, uint8_t x, uint8_t y, const std::string& str,
                         uint8_t* attrs, const char* marks, int mark_count, bool vertical)
    {
        uint8_t n = plen(str);
        if (n == 0) return;

        size_t s = 0;
        PosChar(x, y);
        int32_t p = absolute_pos;
        for (;;) {
            char c;
            uint8_t flag;
            do {
                c = str[s++];
                flag = 2;
                for (int k = 0; k < mark_count; ++k) {
                    if (c == marks[k]) {
                        std::swap(attrs[0], attrs[k + 1]);
                        --n;
                        flag = (n == 0) ? 1 : 0;
                        break;
                    }
                }
            } while (flag == 0);

            if (flag == 1) break;

            put_cell(dest, p, static_cast<uint8_t>(c), attrs[0]);
            p += vertical ? MaxCol * 2 : 2;

            --n;
            if (n == 0) break;
        }
    }

    uint8_t count_without(const std::string& str, const char* marks)
    {
        uint8_t len = 0;
        const uint8_t n = plen(str);
        for (uint8_t i = 0; i < n; ++i) {
            if (std::strchr(marks, str[i]) == nullptr || str[i] == '\0') ++len;
        }
        return len;
    }
}

// ---------------------------------------------------------------------------
// screen_ptr への表示 (表示中ウィンドウを避ける)
// ---------------------------------------------------------------------------

void show_str(uint8_t xpos, uint8_t ypos, const std::string& str, uint8_t color)
{
    const uint8_t n = plen(str);
    if (n == 0) return;

    const AreaClip clip;
    for (uint8_t i = 1; i <= n; ++i) {
        const int32_t pos = ((ypos - 1) * MaxCol + xpos + i - 2) << 1;
        clip.write(pos, static_cast<uint8_t>(xpos + i - 1), ypos, static_cast<uint8_t>(str[i - 1]), color);
    }
}

void show_cstr(uint8_t xpos, uint8_t ypos, const std::string& str, uint8_t attr1, uint8_t attr2)
{
    show_cstr_proc(xpos, ypos, str, attr1, attr2, '~', false);
}

void show_cstr_alt(uint8_t xpos, uint8_t ypos, const std::string& str, uint8_t attr1, uint8_t attr2)
{
    show_cstr_proc(xpos, ypos, str, attr1, attr2, '\n', false);
}

void show_vstr(uint8_t xpos, uint8_t ypos, const std::string& str, uint8_t color)
{
    const uint8_t n = plen(str);
    if (n == 0) return;

    const AreaClip clip;
    for (uint8_t i = 1; i <= n; ++i) {
        const int32_t pos = ((ypos + i - 2) * MaxCol + xpos - 1) << 1;
        clip.write(pos, xpos, static_cast<uint8_t>(ypos + i - 1), static_cast<uint8_t>(str[i - 1]), color);
    }
}

void show_vcstr(uint8_t xpos, uint8_t ypos, const std::string& str, uint8_t attr1, uint8_t attr2)
{
    show_cstr_proc(xpos, ypos, str, attr1, attr2, '~', true);
}

// ---------------------------------------------------------------------------
// 任意の画面メモリへの表示
// ---------------------------------------------------------------------------

void ShowStr(tSCREEN_MEM_PTR dest, uint8_t x, uint8_t y, const std::string& str, uint8_t attr)
{
    PosChar(x, y);
    const uint8_t n = plen(str);
    for (uint8_t i = 0; i < n; ++i) put_cell(dest, absolute_pos + i * 2, static_cast<uint8_t>(str[i]), attr);
}

void ShowVStr(tSCREEN_MEM_PTR dest, uint8_t x, uint8_t y, const std::string& str, uint8_t attr)
{
    PosChar(x, y);
    const uint8_t n = plen(str);
    for (uint8_t i = 0; i < n; ++i) put_cell(dest, absolute_pos + i * MaxCol * 2, static_cast<uint8_t>(str[i]), attr);
}

void ShowCStr(tSCREEN_MEM_PTR dest, uint8_t x, uint8_t y, const std::string& str, uint8_t atr1, uint8_t atr2)
{
    uint8_t attrs[2] = { atr1, atr2 };
    show_multi_cstr(dest, x, y, str, attrs, "~", 1, false);
}

void ShowCStr2(tSCREEN_MEM_PTR dest, uint8_t x, uint8_t y, const std::string& str, uint8_t atr1, uint8_t atr2)
{
    uint8_t attrs[2] = { atr1, atr2 };
    show_multi_cstr(dest, x, y, str, attrs, "\"", 1, false);
}

void ShowVCStr(tSCREEN_MEM_PTR dest, uint8_t x, uint8_t y, const std::string& str, uint8_t atr1, uint8_t atr2)
{
    uint8_t attrs[2] = { atr1, atr2 };
    show_multi_cstr(dest, x, y, str, attrs, "~", 1, true);
}

void ShowVCStr2(tSCREEN_MEM_PTR dest, uint8_t x, uint8_t y, const std::string& str, uint8_t atr1, uint8_t atr2)
{
    uint8_t attrs[2] = { atr1, atr2 };
    show_multi_cstr(dest, x, y, str, attrs, "\"", 1, true);
}

void ShowC3Str(tSCREEN_MEM_PTR dest, uint8_t x, uint8_t y, const std::string& str, uint8_t atr1, uint8_t atr2, uint8_t atr3)
{
    uint8_t attrs[3] = { atr1, atr2, atr3 };
    show_multi_cstr(dest, x, y, str, attrs, "~`", 2, false);
}

void ShowVC3Str(tSCREEN_MEM_PTR dest, uint8_t x, uint8_t y, const std::string& str, uint8_t atr1, uint8_t atr2, uint8_t atr3)
{
    uint8_t attrs[3] = { atr1, atr2, atr3 };
    show_multi_cstr(dest, x, y, str, attrs, "~`", 2, true);
}

void ShowC4Str(tSCREEN_MEM_PTR dest, uint8_t x, uint8_t y, const std::string& str, uint8_t atr1, uint8_t atr2, uint8_t atr3, uint8_t atr4)
{
    uint8_t attrs[4] = { atr1, atr2, atr3, atr4 };
    show_multi_cstr(dest, x, y, str, attrs, "~`^", 3, false);
}

uint8_t CStrLen(const std::string& str) { return count_without(str, "~"); }
uint8_t CStr2Len(const std::string& str) { return count_without(str, "`"); }
uint8_t C3StrLen(const std::string& str) { return count_without(str, "~`"); }

void ScreenMemCopy(const uint8_t* source, tSCREEN_MEM_PTR dest)
{
    cursor_backup = GetCursor();
    std::memmove(dest, source, SCREEN_MEM_SIZE);
}

void move2screen_alt()
{
    if (move_to_screen_data == nullptr) return;

    std::memcpy(temp_screen2, screen_ptr, SCREEN_MEM_SIZE);
    for (int y = move_to_screen_area[2]; y <= move_to_screen_area[4]; ++y) {
        for (int x = move_to_screen_area[1]; x <= move_to_screen_area[3]; ++x) {
            PosChar(static_cast<uint8_t>(x), static_cast<uint8_t>(y));
            if (absolute_pos + 1 < MAX_SCREEN_MEM_SIZE) {
                temp_screen2[absolute_pos] = move_to_screen_data[absolute_pos];
                temp_screen2[absolute_pos + 1] = move_to_screen_data[absolute_pos + 1];
            }
        }
    }
    std::memcpy(screen_ptr, temp_screen2, SCREEN_MEM_SIZE);
}

// ---------------------------------------------------------------------------
// 枠
// ---------------------------------------------------------------------------

void Frame(tSCREEN_MEM_PTR dest, uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t atr1,
           const std::string& title, uint8_t atr2, const std::string& border)
{
    const bool wide_range_type = fr_setting.wide_range_type;
    const bool shadow_enabled = fr_setting.shadow_enabled;
    if (fr_setting.update_area) {
        area_x1 = x1;
        area_y1 = y1;
        area_x2 = x2;
        area_y2 = y2;
    }

    const auto b = [&border](int i) { return i < static_cast<int>(border.size()) ? border[i] : ' '; };

    int8_t xexp1, xexp2, xexp3, yexp1, yexp2;
    uint8_t y;
    if (wide_range_type) {
        xexp1 = 4;
        xexp2 = -1;
        xexp3 = 7;
        yexp1 = 1;
        yexp2 = 2;
        DupChar(static_cast<uint8_t>(x1 - 3), static_cast<uint8_t>(y1 - 1), ' ', atr1, static_cast<uint8_t>(x2 - x1 + 7), dest);
        DupChar(static_cast<uint8_t>(x1 - 3), static_cast<uint8_t>(y2 + 1), ' ', atr1, static_cast<uint8_t>(x2 - x1 + 7), dest);
        y = y1;
        do {
            DupChar(static_cast<uint8_t>(x1 - 3), y, ' ', atr1, 3, dest);
            DupChar(static_cast<uint8_t>(x2 + 1), y, ' ', atr1, 3, dest);
            ++y;
        } while (!(y > y2));
    } else {
        xexp1 = 1;
        xexp2 = 2;
        xexp3 = 1;
        yexp1 = 0;
        yexp2 = 1;
    }

    uint8_t n = static_cast<uint8_t>(x2 - x1 - 1);
    DupChar(x1, y1, b(0), atr1, 1, dest);
    DupChar(static_cast<uint8_t>(x1 + 1), y1, b(1), atr1, n, dest);
    DupChar(x2, y1, b(2), atr1, 1, dest);

    y = y1;
    do {
        ++y;
        DupChar(x1, y, b(3), atr1, 1, dest);
        DupChar(static_cast<uint8_t>(x1 + 1), y, ' ', atr1, n, dest);
        DupChar(x2, y, b(4), atr1, 1, dest);
    } while (!(y >= y2));

    DupChar(x1, y2, b(5), atr1, 1, dest);
    DupChar(static_cast<uint8_t>(x1 + 1), y2, b(6), atr1, n, dest);
    DupChar(x2, y2, b(7), atr1, 1, dest);

    n = plen(title);
    if (n != 0) {
        uint8_t x = static_cast<uint8_t>(x2 - x1 - n);
        x = static_cast<uint8_t>(static_cast<uint16_t>(x + 1) >> 1);
        PosChar(static_cast<uint8_t>(x1 + x), y1);
        for (uint8_t i = 0; i < n; ++i) put_cell(dest, absolute_pos + i * 2, static_cast<uint8_t>(title[i]), atr2);
    }

    if (shadow_enabled) {
        y = static_cast<uint8_t>(y1 - yexp1);
        do {
            ++y;
            PosChar(static_cast<uint8_t>(x2 + xexp1), y);
            put_byte(dest, absolute_pos + 1, 0x07);
            put_byte(dest, absolute_pos + 3, 0x07);
        } while (!(y > y2));

        PosChar(static_cast<uint8_t>(x1 + xexp2), static_cast<uint8_t>(y2 + yexp2));
        int32_t p = absolute_pos + 1;
        n = static_cast<uint8_t>(x2 - x1 + xexp3);
        if (MaxLn >= 60) --n;
        do {
            put_byte(dest, p, 0x07);
            p += 2;
            --n;
        } while (n != 0);
    }
}

// ---------------------------------------------------------------------------
// カーソル
// ---------------------------------------------------------------------------

uint8_t WhereX() { return virtual_cur_pos & 0xff; }
uint8_t WhereY() { return virtual_cur_pos >> 8; }
void GotoXY(uint8_t x, uint8_t y) { virtual_cur_pos = static_cast<uint16_t>(x | (y << 8)); }
int32_t GetCursor() { return 0; }

void SetCursor(int32_t cursor)
{
    virtual_cur_pos = static_cast<uint16_t>(static_cast<uint32_t>(cursor) >> 16);
    SetCursorShape(static_cast<uint16_t>(cursor & 0xffff));
}

void ThinCursor() { SetCursorShape(0x0d0e); }
void WideCursor() { SetCursorShape(0x010e); }
void HideCursor() { SetCursorShape(0x1010); }
uint16_t GetCursorShape() { return virtual_cur_shape; }
void SetCursorShape(uint16_t shape) { virtual_cur_shape = shape; }

// ---------------------------------------------------------------------------
// 初期化
// ---------------------------------------------------------------------------

bool is_default_screen_mode() { return program_screen_mode == 0; }
bool is_scrollable_screen_mode() { return program_screen_mode == 0; }

void synchronize_screen()
{
    if (!is_default_screen_mode()) return;
    const int32_t max_offset = 16 * MaxLn - 16 * hard_maxln;
    if (screen_scroll_offset > max_offset) screen_scroll_offset = static_cast<uint16_t>(max_offset);
    virtual_screen__first_row = screen_scroll_offset * SCREEN_RES_x;
}

void TxtScrIO_Init(uint8_t screen_mode)
{
    program_screen_mode = screen_mode;
    centered_frame_vdest = screen_ptr;

    switch (program_screen_mode) {
    case 1:   // フルスクリーン表示
        SCREEN_RES_x = 960;
        SCREEN_RES_y = 800;
        MAX_COLUMNS = 120;
        MAX_ROWS = 50;
        MAX_ORDER_COLS = 13;
        MAX_TRACKS = 7;
        MAX_PATTERN_ROWS = 28;
        INSCTRL_xshift = 15;
        INSCTRL_yshift = 6;
        INSEDIT_yshift = 12;
        PATTORD_xshift = 1;
        hard_maxln = 50;
        work_maxln = 40;
        break;

    case 2:   // ワイドフルスクリーン表示
        SCREEN_RES_x = 1440;
        SCREEN_RES_y = 960;
        MAX_COLUMNS = 180;
        MAX_ROWS = 60;
        MAX_ORDER_COLS = 22;
        MAX_TRACKS = 11;
        MAX_PATTERN_ROWS = 38;
        INSCTRL_xshift = 45;
        INSCTRL_yshift = 12;
        INSEDIT_yshift = 12;
        PATTORD_xshift = 0;
        hard_maxln = 60;
        work_maxln = 50;
        break;

    default:   // 標準 (90x40 のうち 30 行を表示)
        program_screen_mode = 0;
        SCREEN_RES_x = 720;
        SCREEN_RES_y = 480;
        MAX_COLUMNS = 90;
        MAX_ROWS = 40;
        MAX_ORDER_COLS = 9;
        MAX_TRACKS = 5;
        MAX_PATTERN_ROWS = 18;
        INSCTRL_xshift = 0;
        INSCTRL_yshift = 0;
        INSEDIT_yshift = 0;
        PATTORD_xshift = 0;
        hard_maxln = 30;
        work_maxln = 30;
        break;
    }

    MaxCol = MAX_COLUMNS;
    MaxLn = MAX_ROWS;
    hard_maxcol = MAX_COLUMNS;
    work_maxcol = MAX_COLUMNS;
    scr_font_width = 8;
    scr_font_height = 16;

    for (int i = 0; i < MAX_SCREEN_MEM_SIZE; i += 2) {
        screen_ptr[i] = 0x00;
        screen_ptr[i + 1] = 0x07;
    }
    SCREEN_MEM_SIZE = (SCREEN_RES_x / scr_font_width) * MAX_ROWS * 2;
    screen_scroll_offset = 0;
    synchronize_screen();
}

} // namespace at2
