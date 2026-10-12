#include "DialogIO.h"

#include <algorithm>

#include "StringIO.h"
#include "TxtScrIO.h"
#include "../platform/System.h"
#include "../platform/input/Keyboard.h"

namespace at2
{

// DialogIO_Init で設定される値
// (frame_double は別の翻訳単位の変数で初期化順が決まらないため、同じ内容を直接書く)
tDIALOG_SETTING dl_setting = {
    "\xC9\xCD\xBB\xBA\xBA\xC8\xCD\xBC",
    true,
    static_cast<uint8_t>(dialog_background + dialog_title),
    static_cast<uint8_t>(dialog_background + dialog_border),
    static_cast<uint8_t>(dialog_background + dialog_text),
    static_cast<uint8_t>(dialog_background + dialog_hi_text),
    static_cast<uint8_t>(dialog_background + dialog_item),
    static_cast<uint8_t>(dialog_sel_itm_bck + dialog_sel_itm),
    static_cast<uint8_t>(dialog_background + dialog_short),
    static_cast<uint8_t>(dialog_sel_itm_bck + dialog_sel_short),
    static_cast<uint8_t>(dialog_background + dialog_item_dis),
    static_cast<uint8_t>(dialog_background + dialog_context),
    static_cast<uint8_t>(dialog_background + dialog_context_dis),
    1,
    1,
    true,
    true,
    true,
    false,
    { kESC, kENTER },
};

tDIALOG_ENVIRONMENT dl_environment = { 0, "", "", 0, 0, 0, 0, 0, nullptr };

namespace
{

struct DialogItem   // tDBUFFR の要素
{
    std::string str;
    uint8_t pos;
    char key;
    bool use;
};

DialogItem dbuf[101];   // [1..100]
uint16_t idx2, idx3, num, nm2;
uint16_t ystart;

char UpCase(char c) { return (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c; }

// Pascal の str[i] (範囲外は 0)
char at(const std::string& str, size_t i) { return (i >= 1 && i <= str.size()) ? str[i - 1] : '\0'; }

char OutKey(const std::string& str)
{
    const size_t p = str.find('~');
    if (p == std::string::npos) return '~';
    const size_t pos = p + 1;   // 1 始まり
    if (at(str, pos + 2) != '~') return '~';
    return at(str, pos + 1);
}

std::string ReadChunk(const std::string& str, size_t pos)
{
    const std::string s = pos - 1 < str.size() ? str.substr(pos - 1) : std::string();
    const size_t p = s.find('$');
    if (p == std::string::npos) return std::string();
    return s.substr(0, p);
}

void SubPos(uint16_t& p)
{
    uint16_t temp = p;
    if (temp > 1 && dbuf[temp - 1].use) --temp;
    else if (temp > 1) {
        --temp;
        SubPos(temp);
    }
    if (dbuf[temp].use) p = temp;
}

void AddPos(uint16_t& p)
{
    uint16_t temp = p;
    if (temp < nm2 && dbuf[temp + 1].use) ++temp;
    else if (temp < nm2) {
        ++temp;
        AddPos(temp);
    }
    if (dbuf[temp].use) p = temp;
}

void ShowItem()
{
    if (idx2 == 0) return;
    if (idx2 != idx3) {
        ShowCStr(screen_ptr, dbuf[idx3].pos, static_cast<uint8_t>(ystart + num + 1), dbuf[idx3].str,
                 dl_setting.keys_attr, dl_setting.short_attr);
    }
    ShowCStr(screen_ptr, dbuf[idx2].pos, static_cast<uint8_t>(ystart + num + 1), dbuf[idx2].str,
             dl_setting.keys2_attr, dl_setting.short2_attr);
    idx3 = idx2;
}

uint16_t RetKey(uint8_t code)
{
    for (uint16_t temp = 1; temp <= nm2; ++temp) {
        if (UpCase(dbuf[temp].key) == UpCase(static_cast<char>(code))) return temp;
    }
    return 0;
}

} // namespace

uint8_t Dialog(const std::string& text, const std::string& keys, const std::string& title, uint8_t spos)
{
    size_t pos = 1;
    uint16_t max = static_cast<uint16_t>(title.size());
    num = 0;

    ScreenMemCopy(screen_ptr, scr_backup);
    HideCursor();

    std::string str;
    do {
        str = ReadChunk(text, pos);
        pos += str.size() + 1;
        if (CStrLen(str) > max) max = CStrLen(str);
        if (!str.empty()) ++num;
    } while (!(pos >= text.size() || str.empty()));

    pos = 1;
    uint16_t mx2 = 0;
    nm2 = 0;

    // 文字列入力 ('%string_input%') は未対応 (本クローン版ではファイル名の入力にコモンダイアログを使う)
    do {
        str = ReadChunk(keys, pos);
        pos += str.size() + 1;
        if (!str.empty() && nm2 < 100) {
            ++nm2;
            dbuf[nm2].str = " " + str + " ";
            dbuf[nm2].key = OutKey(str);
            if (!dl_setting.all_enabled) dbuf[nm2].use = dbuf[nm2].key != '~';
            else dbuf[nm2].use = true;
            if (nm2 > 1) {
                dbuf[nm2].pos = static_cast<uint8_t>(dbuf[nm2 - 1].pos + CStrLen(dbuf[nm2 - 1].str) + 1);
                mx2 = static_cast<uint16_t>(mx2 + CStrLen(dbuf[nm2].str) + 1);
            } else {
                dbuf[nm2].pos = 1;
                mx2 = static_cast<uint16_t>(mx2 + CStrLen(dbuf[nm2].str));
            }
        }
    } while (!(pos >= keys.size() || str.empty()));

    if (max < mx2) max = mx2;

    uint16_t xstart;
    if (dl_setting.center_box) {
        const int w = work_maxcol - (max + 4);
        const int h = work_maxln - (num + 2);
        xstart = static_cast<uint16_t>(w / 2 + w % 2);
        ystart = static_cast<uint16_t>(h / 2 + h % 2);
    } else {
        xstart = dl_setting.xstart;
        ystart = dl_setting.ystart;
    }

    const bool old_fr_shadow_enabled = fr_setting.shadow_enabled;
    fr_setting.shadow_enabled = dl_setting.shadow_enabled;
    Frame(screen_ptr, static_cast<uint8_t>(xstart), static_cast<uint8_t>(ystart), static_cast<uint8_t>(xstart + max + 3),
          static_cast<uint8_t>(ystart + num + 2), dl_setting.box_attr, title, dl_setting.title_attr, dl_setting.frame_type);
    fr_setting.shadow_enabled = old_fr_shadow_enabled;

    dl_environment.xpos = static_cast<uint8_t>(xstart);
    dl_environment.ypos = static_cast<uint8_t>(ystart);
    dl_environment.xsize = static_cast<uint8_t>(max + 3);
    dl_environment.ysize = static_cast<uint8_t>(num + 2);

    pos = 1;
    const std::string contxt = DietStr(dl_environment.context,
        static_cast<uint8_t>(max + (dl_environment.context.size() - CStrLen(dl_environment.context))));
    ShowCStr(screen_ptr, static_cast<uint8_t>(xstart + max + 3 - CStrLen(contxt)), static_cast<uint8_t>(ystart + num + 2),
             contxt, dl_setting.contxt_attr, dl_setting.contxt2_attr);

    for (uint16_t idx = 1; idx <= num; ++idx) {
        str = ReadChunk(text, pos);
        pos += str.size() + 1;
        if (dl_setting.center_text) {
            ShowCStr(screen_ptr, static_cast<uint8_t>(xstart + 2), static_cast<uint8_t>(ystart + idx),
                     ExpStrL(str, static_cast<uint8_t>(str.size() + (max - CStrLen(str)) / 2), ' '),
                     dl_setting.text_attr, dl_setting.text2_attr);
        } else {
            ShowCStr(screen_ptr, static_cast<uint8_t>(xstart + 2), static_cast<uint8_t>(ystart + idx), str,
                     dl_setting.text_attr, dl_setting.text2_attr);
        }
    }

    uint8_t result = 0;
    for (uint16_t idx = 1; idx <= nm2; ++idx) {
        dbuf[idx].pos = static_cast<uint8_t>(dbuf[idx].pos + xstart + (max - mx2) / 2 + 1);
        if (dbuf[idx].use) {
            ShowCStr(screen_ptr, dbuf[idx].pos, static_cast<uint8_t>(ystart + num + 1), dbuf[idx].str,
                     dl_setting.keys_attr, dl_setting.short_attr);
        } else {
            ShowCStr(screen_ptr, dbuf[idx].pos, static_cast<uint8_t>(ystart + num + 1), dbuf[idx].str,
                     dl_setting.disbld_attr, dl_setting.disbld_attr);
        }
    }

    if (spos < 1) spos = 1;
    if (spos > nm2) spos = static_cast<uint8_t>(nm2);

    idx2 = spos;
    idx3 = 1;

    if (!dbuf[idx2].use) {
        SubPos(idx2);
        if (!dbuf[idx2].use) AddPos(idx2);
    }

    ShowItem();
    ShowItem();
    bool qflg = false;
    // 項目のない ('$' だけの) ダイアログは表示だけして戻る (画面は戻さない)
    if (keys == "$") return 0;

    uint16_t key;
    do {
        dl_environment.cur_item = static_cast<uint8_t>(idx2);
        if (dl_environment.ext_proc) dl_environment.ext_proc();
        key = getkey();
        if (LookUpKey(key, dl_setting.terminate_keys, 50)) qflg = true;

        if (!qflg) {
            const uint8_t lo = static_cast<uint8_t>(key & 0xff);
            if (lo >= 0x20) {
                const uint16_t idx = RetKey(lo);
                if (idx != 0) {
                    qflg = true;
                    idx2 = idx;
                }
            } else if (!shift_pressed() && !ctrl_pressed() && !alt_pressed()) {
                switch (key) {
                case kLEFT:
                    if (idx2 > 1 || !dl_setting.cycle_moves) {
                        SubPos(idx2);
                    } else {
                        idx2 = nm2;
                        if (!dbuf[idx2].use) SubPos(idx2);
                    }
                    break;

                case kRIGHT:
                    if (idx2 < nm2 || !dl_setting.cycle_moves) {
                        const uint16_t temp = idx2;
                        AddPos(idx2);
                        if (idx2 == temp) {
                            idx2 = 1;
                            if (!dbuf[idx2].use) AddPos(idx2);
                        }
                    } else {
                        idx2 = 1;
                        if (!dbuf[idx2].use) AddPos(idx2);
                    }
                    break;

                case kHOME:
                    idx2 = 1;
                    if (!dbuf[idx2].use) AddPos(idx2);
                    break;

                case kEND:
                    idx2 = nm2;
                    if (!dbuf[idx2].use) SubPos(idx2);
                    break;

                default:
                    break;
                }
            }
        }

        ShowItem();
        draw_screen();
    } while (!(qflg || _force_program_quit));

    result = static_cast<uint8_t>(idx2);
    dl_environment.keystroke = key;

    if (move_to_screen_routine) {
        move_to_screen_data = scr_backup;
        move_to_screen_area[1] = static_cast<uint8_t>(xstart);
        move_to_screen_area[2] = static_cast<uint8_t>(ystart);
        move_to_screen_area[3] = static_cast<uint8_t>(xstart + max + 3 + 2);
        move_to_screen_area[4] = static_cast<uint8_t>(ystart + num + 2 + 1);
        move_to_screen_routine();
    } else {
        ScreenMemCopy(scr_backup, screen_ptr);
    }
    return result;
}

// --- InputStr ---

bool use_large_cursor = false;

tINPUT_STR_SETTING is_setting = {
    true,
    true,
    true,
    CharRange(0x20, 0xff),
    CharRange(0x20, 0xff),
    CharRange(0x20, 0xff),
    CharRange('A', 'Z') | CharRange('a', 'z') | CharRange('0', '9') | CharSet("_"),
    { kESC, kENTER },
};

tINPUT_STR_ENVIRONMENT is_environment = { 0, 0, false, 0, 0, "", nullptr };

std::string InputStr(std::string s, uint8_t x, uint8_t y, uint8_t ln, uint8_t ln1, uint8_t atr1, uint8_t atr2)
{
    // Pascal の s[i] (範囲外は 0、s[0] は長さ)
    const auto sc = [&s](int i) -> char {
        if (i == 0) return static_cast<char>(s.size());
        return (i >= 1 && i <= static_cast<int>(s.size())) ? s[static_cast<size_t>(i - 1)] : '\0';
    };
    const auto len = [&s]() { return static_cast<int>(s.size()); };
    const auto del = [&s](int index, int count) {   // Pascal の Delete
        if (index < 1 || index > static_cast<int>(s.size()) || count <= 0) return;
        s.erase(static_cast<size_t>(index - 1), static_cast<size_t>(count));
    };
    const auto set_cursor = [](bool ins) {
        if (ins) {
            if (use_large_cursor) WideCursor();
            else ThinCursor();
        } else {
            if (use_large_cursor) ThinCursor();
            else WideCursor();
        }
    };
    const auto filtered = [](const std::string& str) { return FilterStr2(str, is_setting.char_filter, '_'); };
    const int more_ln = std::max(ln, ln1);

    s = Copy(s, 1, ln);
    if (is_environment.locate_pos > ln1) is_environment.locate_pos = ln1;
    if (is_environment.locate_pos > s.size() + 1) is_environment.locate_pos = static_cast<uint8_t>(s.size());

    int cloc = is_environment.locate_pos;
    int xloc = is_environment.locate_pos;
    const uint8_t xint = x;
    bool qflg = false;
    bool ins = is_setting.insert_mode;
    bool appn = !is_setting.append_enabled;

    --x;
    set_cursor(ins);

    std::string s1 = s;
    if (s1.size() > ln1) s1.resize(ln1);

    ShowStr(screen_ptr, xint, y, ExpStrR("", ln1, ' '), atr1);
    ShowStr(screen_ptr, xint, y, filtered(s1), atr2);
    bool for1st = true;
    uint16_t key = 0;

    do {
        const std::string s2 = s1;
        if (xloc == 1) s1 = Copy(s, cloc, ln1);
        else s1 = Copy(s, cloc - xloc + 1, ln1);

        const uint8_t attr = !appn ? atr2 : atr1;

        if (appn && for1st) {
            ShowStr(screen_ptr, xint, y, ExpStrR(filtered(s1), ln1, ' '), atr1);
            for1st = false;
        }

        if (s2 != s1) ShowStr(screen_ptr, xint, y, ExpStrR(filtered(s1), ln1, ' '), atr1);

        if (ln1 < ln) {
            if (cloc - xloc > 0 && len() > 0)
                ShowStr(screen_ptr, xint, y, "\x11", static_cast<uint8_t>((attr & 0xf0) + 0x0f));
            else if (cloc - xloc == 0 && len() != 0)
                ShowStr(screen_ptr, xint, y, std::string(1, s[0]), attr);
            else
                ShowStr(screen_ptr, xint, y, " ", atr1);
        }

        if (ln1 < ln) {
            if (cloc - xloc + ln1 < len())
                ShowStr(screen_ptr, static_cast<uint8_t>(xint + ln1 - 1), y, "\x10", static_cast<uint8_t>((attr & 0xf0) + 0x0f));
            else if (cloc - xloc + ln1 == len())
                ShowStr(screen_ptr, static_cast<uint8_t>(xint + ln1 - 1), y, filtered(std::string(1, sc(len()))), attr);
            else
                ShowStr(screen_ptr, static_cast<uint8_t>(xint + ln1 - 1), y, " ", atr1);
        }

        GotoXY(static_cast<uint8_t>(x + xloc), y);
        if (keypressed()) {
            key = getkey();
            if (LookUpKey(key, is_setting.terminate_keys, 50)) qflg = true;
            bool chflag = false;

            if (!qflg) {
                switch (key) {
                case kTAB:
                    appn = true;
                    break;

                case kCHplus:
                case kNPplus:
                    chflag = true;
                    if (is_setting.character_set == DEC_NUM_CHARSET) {
                        const int32_t v = Str2num(s, 10) + 1;
                        if (Num2str(v, 10).size() <= ln1 && static_cast<uint32_t>(v) <= is_environment.max_num) {
                            s = Num2str(v, 10);
                            chflag = false;
                        }
                    } else if (is_setting.character_set == HEX_NUM_CHARSET) {
                        const int32_t v = Str2num(s, 16) + 1;
                        if (Num2str(v, 16).size() <= ln1 && static_cast<uint32_t>(v) <= is_environment.max_num) {
                            s = Num2str(v, 16);
                            chflag = false;
                        }
                    }
                    break;

                case kCHmins:
                case kNPmins:
                    chflag = true;
                    if (is_setting.character_set == DEC_NUM_CHARSET) {
                        const int32_t v = Str2num(s, 10);
                        if (v > 0 && static_cast<uint32_t>(v - 1) >= is_environment.min_num) {
                            s = Num2str(v - 1, 10);
                            chflag = false;
                        }
                    } else if (is_setting.character_set == HEX_NUM_CHARSET) {
                        const int32_t v = Str2num(s, 16);
                        if (v > 0 && static_cast<uint32_t>(v - 1) >= is_environment.min_num) {
                            s = Num2str(v - 1, 16);
                            chflag = false;
                        }
                    }
                    break;

                case kCtrlY:
                    appn = true;
                    s.clear();
                    cloc = 1;
                    xloc = 1;
                    break;

                case kCtrlT:
                    appn = true;
                    while (InSet(is_setting.word_characters, sc(cloc)) && cloc <= len()) del(cloc, 1);
                    while (!InSet(is_setting.word_characters, sc(cloc)) && cloc <= len()) del(cloc, 1);
                    break;

                case kCtrlK:
                    appn = true;
                    del(cloc, len());
                    break;

                case kCtBkSp:
                    appn = true;
                    while (InSet(is_setting.word_characters, sc(cloc - 1)) && cloc > 1) {
                        --cloc;
                        del(cloc, 1);
                        if (xloc > 1) --xloc;
                    }
                    while (!InSet(is_setting.word_characters, sc(cloc - 1)) && cloc > 1) {
                        --cloc;
                        del(cloc, 1);
                        if (xloc > 1) --xloc;
                    }
                    break;

                case kBkSPC:
                    appn = true;
                    if (cloc > 1) {
                        if (xloc > 1) --xloc;
                        --cloc;
                        del(cloc, 1);
                    }
                    break;

                case kDELETE:
                    appn = true;
                    if (cloc <= len()) del(cloc, 1);
                    break;

                case kCtLEFT:
                    appn = true;
                    while (InSet(is_setting.word_characters, sc(cloc)) && cloc > 1) {
                        --cloc;
                        if (xloc > 1) --xloc;
                    }
                    while (!InSet(is_setting.word_characters, sc(cloc)) && cloc > 1) {
                        --cloc;
                        if (xloc > 1) --xloc;
                    }
                    break;

                case kCtRGHT:
                    appn = true;
                    while (InSet(is_setting.word_characters, sc(cloc)) && cloc < len()) {
                        ++cloc;
                        if (xloc < ln1) ++xloc;
                    }
                    while (!InSet(is_setting.word_characters, sc(cloc)) && cloc < len()) {
                        ++cloc;
                        if (xloc < ln1) ++xloc;
                    }
                    break;

                case kLEFT:
                    appn = true;
                    if (cloc > 1) --cloc;
                    if (xloc > 1) --xloc;
                    break;

                case kRIGHT:
                    appn = true;
                    if (cloc < len() || (cloc == len() && len() < more_ln)) ++cloc;
                    if (xloc < ln1 && xloc <= len()) ++xloc;
                    break;

                case kINSERT:
                    if (is_setting.replace_enabled) {
                        ins = !ins;
                        set_cursor(ins);
                    }
                    break;

                case kHOME:
                    appn = true;
                    cloc = 1;
                    xloc = 1;
                    break;

                case kEND:
                    appn = true;
                    if (len() < more_ln) cloc = len() + 1;
                    else cloc = len();
                    xloc = cloc < ln1 ? cloc : ln1;
                    break;

                default:
                    chflag = true;
                    break;
                }
            }

            const char ch = static_cast<char>(key & 0xff);
            if (chflag && InSet(is_setting.character_set, ch)) {
                if (!appn) {
                    s.clear();
                    cloc = 1;
                    xloc = 1;
                }
                appn = true;
                if (ins && static_cast<int>(CutStrR(s, static_cast<uint8_t>(cloc)).size()) < ln) {
                    s.insert(static_cast<size_t>(std::min(cloc - 1, len())), 1, ch);
                    if (s.size() > 255) s.resize(255);
                    s = FilterStr2(s, is_setting.valid_chars, '_');
                    if (cloc < ln) ++cloc;
                    if (xloc < ln && xloc < ln1) ++xloc;
                } else if (len() < ln || !ins) {
                    if (cloc > len() && len() < ln) s.push_back('\0');
                    if (cloc >= 1 && cloc <= len()) s[static_cast<size_t>(cloc - 1)] = ch;
                    s = FilterStr2(s, is_setting.valid_chars, '_');
                    if (cloc < ln) ++cloc;
                    if (xloc < ln && xloc < ln1) ++xloc;
                }
            }
        }

        is_environment.cur_str = s;
        if (is_environment.ext_proc) is_environment.ext_proc();
        draw_screen();
    } while (!qflg && !_force_program_quit);

    is_environment.locate_pos = static_cast<uint8_t>(cloc == 0 ? 1 : cloc);
    is_environment.keystroke = key;
    is_environment.insert_mode = ins;
    return s;
}

// --- Menu ---

// DialogIO_Init で設定される値
tMENU_SETTING mn_setting = {
    "\xC9\xCD\xBB\xBA\xBA\xC8\xCD\xBC",
    true,
    true,
    true,
    static_cast<uint8_t>(dialog_background + dialog_title),
    static_cast<uint8_t>(dialog_background + dialog_border),
    static_cast<uint8_t>(dialog_background + dialog_item),
    static_cast<uint8_t>(dialog_sel_itm_bck + dialog_sel_itm),
    static_cast<uint8_t>(dialog_def_bckg + dialog_def),
    static_cast<uint8_t>(dialog_background + dialog_short),
    static_cast<uint8_t>(dialog_sel_itm_bck + dialog_sel_short),
    static_cast<uint8_t>(dialog_background + dialog_item_dis),
    static_cast<uint8_t>(dialog_background + dialog_context),
    static_cast<uint8_t>(dialog_background + dialog_context_dis),
    static_cast<uint8_t>(dialog_background + dialog_topic),
    static_cast<uint8_t>(dialog_background + dialog_hi_topic),
    tCHARSET(),
    true,
    true,
    false,
    false,
    true,
    0,
    0,
    0,
    { kESC, kENTER },
};

// v_dest は screen_ptr (別の翻訳単位の変数で初期化順が決まらないため、Menu で設定する)
tMENU_ENVIRONMENT mn_environment = {
    nullptr, 0, "", false, true, false, 0, 0, 0, "", nullptr, nullptr, nullptr,
    false, false, false, 0, 0, nullptr, false, 0, 0, 0, 0, 0, 0,
};

namespace
{

struct MenuKey   // tMBUFFR の要素
{
    char key;
    bool use;
};

// オリジナルは [1..16384] の静的配列で、項目数より後ろは前回のメニューの値が残る
MenuKey mbuf[16385];

std::vector<std::string>* mnu_data;
size_t mnu_offset;   // 見出し (topic_len) の分だけ先頭をずらす
uint8_t mnu_x, mnu_y, mnu_len, mnu_len2, mnu_topic_len;
uint16_t mnu_count;
uint16_t vscrollbar_pos;

// Menu と refresh が共有する変数 (オリジナルはユニットのグローバル変数)
uint16_t m_max, m_page, m_idx2, m_first, m_last, m_temp2, m_opage, m_opos;
uint16_t m_key;
bool m_solid;

uint16_t _partial(uint16_t max, uint16_t val, uint8_t base)
{
    const double temp1 = static_cast<double>(max) / base;
    double temp2 = temp1 / 2;
    uint16_t temp3 = 0;
    while (temp2 < val) {
        temp2 += temp1;
        ++temp3;
    }
    return temp3;
}

std::string menu_item(uint16_t item)
{
    if (item < 1 || item > mnu_count) return std::string();
    const size_t i = mnu_offset + item - 1;
    if (i >= mnu_data->size()) return std::string();
    return Copy((*mnu_data)[i], 1, mnu_len);
}

std::string pstr(uint16_t item)
{
    if (!m_solid) return ExpStrR(menu_item(item), static_cast<uint8_t>(mnu_len - 2), ' ');
    return ExpStrR(menu_item(item), mnu_len, ' ');
}

std::string pstr2(uint16_t item)
{
    const std::string temp = pstr(item);
    if (mn_setting.fixed_len != 0) return temp;
    std::string result;
    for (char c : temp) {
        if (InSet(mn_setting.topic_mask_chr, c)) result += std::string("`") + c + "`";
        else result += c;
    }
    return result;
}

std::string pdes(uint16_t item)
{
    std::string temp;
    const auto* descr = mn_environment.descr;
    if (descr != nullptr && item >= 1 && item <= mnu_count && item - 1u < descr->size())
        temp = Copy((*descr)[item - 1], 1, mn_environment.descr_len);
    return ExpStrR(temp, mn_environment.descr_len, ' ');
}

void ShowCStr_clone(tSCREEN_MEM_PTR dest, uint8_t x, uint8_t y, const std::string& str,
                    uint8_t atr1, uint8_t atr2, uint8_t atr3, uint8_t atr4)
{
    if (mn_setting.fixed_len == 0) {
        ShowC3Str(dest, x, y, str, atr1, atr2, static_cast<uint8_t>((atr1 & 0xf0) + (mn_setting.topic_attr & 0x0f)));
        return;
    }

    bool highlighted = false;
    uint8_t len = 0;
    uint8_t len2 = 0;
    for (size_t temp = 1; temp <= str.size(); ++temp) {
        const char c = str[temp - 1];
        if (c == '~') {
            highlighted = !highlighted;
        } else if (temp >= mn_environment.fixed_start && len < mn_setting.fixed_len) {
            ShowStr(dest, static_cast<uint8_t>(x + len2), y, std::string(1, c), highlighted ? atr2 : atr1);
            ++len;
            ++len2;
        } else {
            ShowStr(dest, static_cast<uint8_t>(x + len2), y, std::string(1, c), highlighted ? atr4 : atr3);
            if (temp >= mn_environment.fixed_start) ++len;
            ++len2;
        }
    }
}

// 先頭 hlight_chrs 文字を強調する
std::string hlight(const std::string& str)
{
    const uint8_t n = mn_environment.hlight_chrs;
    if (n == 0 || str.empty()) return str;
    return "~" + Copy(str, 1, n) + "~" + Copy(str, n + 1, static_cast<int>(str.size()) - n);
}

void menu_refresh()
{
    if (m_page == m_opage && m_idx2 == m_opos && !mn_environment.do_refresh) return;
    m_opage = m_page;
    m_opos = m_idx2;
    mn_environment.do_refresh = false;

    const auto width = [](int extra) { return static_cast<uint8_t>(m_max + extra); };

    if (!mn_environment.own_refresh) {
        for (int idx = m_page; idx <= mnu_len2 + m_page - 1; ++idx) {
            const std::string item_str = hlight(pstr(static_cast<uint16_t>(idx - m_page + 1)));
            const std::string item_str_alt = hlight(pstr(static_cast<uint16_t>(idx)));
            const std::string item_str2 = hlight(pstr2(static_cast<uint16_t>(m_idx2 + m_page - 1)));
            const std::string item_str2_alt = hlight(pstr2(static_cast<uint16_t>(idx)));
            const std::string desc_str = pdes(static_cast<uint16_t>(idx - m_page + 1));
            const std::string desc_str2 = pdes(static_cast<uint16_t>(idx));
            const std::string desc_str3 = pdes(static_cast<uint16_t>(m_idx2 + m_page - 1));
            const int len_str3 = static_cast<int>(desc_str3.size());

            if (idx == m_idx2 + m_page - 1) {
                ShowCStr_clone(mn_environment.v_dest, static_cast<uint8_t>(mnu_x + 1), static_cast<uint8_t>(mnu_y + m_idx2),
                               ExpStrR(item_str2 + desc_str3,
                                       width(static_cast<int>(item_str2.size()) + len_str3 -
                                                            (C3StrLen(item_str2) + CStrLen(desc_str3))), ' '),
                               mn_setting.text2_attr, mn_setting.short2_attr, mn_setting.text_attr, mn_setting.short_attr);
            } else if (idx - m_page + 1 <= mnu_topic_len) {
                ShowCStr(mn_environment.v_dest, static_cast<uint8_t>(mnu_x + 1), static_cast<uint8_t>(mnu_y + idx - m_page + 1),
                         ExpStrR(item_str + desc_str,
                                 width(static_cast<int>(item_str.size()) + len_str3 - CStrLen(item_str + desc_str)), ' '),
                         mn_setting.topic_attr, mn_setting.hi_topic_attr);
            } else if (mbuf[idx].use) {
                ShowC3Str(mn_environment.v_dest, static_cast<uint8_t>(mnu_x + 1), static_cast<uint8_t>(mnu_y + idx - m_page + 1),
                          ExpStrR(item_str2_alt + desc_str2,
                                  width(static_cast<int>(item_str2_alt.size()) + len_str3 -
                                                           (C3StrLen(item_str2_alt) + CStrLen(desc_str2))), ' '),
                          mn_setting.text_attr, mn_setting.short_attr, mn_setting.topic_attr);
            } else {
                ShowCStr(mn_environment.v_dest, static_cast<uint8_t>(mnu_x + 1), static_cast<uint8_t>(mnu_y + idx - m_page + 1),
                         ExpStrR(item_str_alt + desc_str2,
                                 width(static_cast<int>(item_str_alt.size()) + len_str3 -
                                                         CStrLen(item_str_alt + desc_str2)), ' '),
                         mn_setting.disbld_attr, mn_setting.disbld_attr);
            }
        }
    }

    if (mn_setting.show_scrollbar) {
        vscrollbar_pos = VScrollBar(mn_environment.v_dest, static_cast<uint8_t>(mnu_x + m_max + 1),
                                    static_cast<uint8_t>(mnu_y + 1 - mn_setting.topic_len), static_cast<uint8_t>(m_temp2),
                                    mnu_count, static_cast<uint16_t>(m_idx2 + m_page - 1), vscrollbar_pos,
                                    mn_setting.menu_attr, mn_setting.menu_attr);
    }
}

} // namespace

uint16_t HScrollBar(tSCREEN_MEM_PTR dest, uint8_t x, uint8_t y, uint8_t size, uint16_t len1, uint16_t len2, uint16_t pos,
                    uint8_t atr1, uint8_t atr2)
{
    if (size > work_maxcol - x) size = static_cast<uint8_t>(work_maxcol - x);
    if (size < 5) size = 5;

    uint16_t temp;
    if (size - 2 - 1 < 10) temp = _partial(len1, len2, static_cast<uint8_t>(size - 2 - 1));
    else temp = _partial(len1, len2, static_cast<uint8_t>(size - 2 - 1 - 2));

    if (pos == temp) return temp;

    if (size < len1) {
        pos = temp;
        ShowStr(dest, x, y, "\x11" + ExpStrL("", static_cast<uint8_t>(size - 2), '\xB0') + "\x10", atr1);
        if (size - 2 - 1 < 10) ShowStr(dest, static_cast<uint8_t>(x + 1 + temp), y, "\xB2", atr2);
        else ShowStr(dest, static_cast<uint8_t>(x + 1 + temp), y, "\xB2\xB2\xB2", atr2);
    } else {
        ShowCStr(dest, x, y, "~\x11~" + ExpStrL("", static_cast<uint8_t>(size - 2), '\xB1') + "~\x10~", atr2, atr1);
    }
    return pos;
}

uint16_t VScrollBar(tSCREEN_MEM_PTR dest, uint8_t x, uint8_t y, uint8_t size, uint16_t len1, uint16_t len2, uint16_t pos,
                    uint8_t atr1, uint8_t atr2)
{
    if (size > work_maxln - y) size = static_cast<uint8_t>(work_maxln - y);
    if (size < 5) size = 5;

    uint16_t temp;
    if (size - 2 - 1 < 10) temp = _partial(len1, len2, static_cast<uint8_t>(size - 2 - 1));
    else temp = _partial(len1, len2, static_cast<uint8_t>(size - 2 - 1 - 2));

    if (pos == temp) return temp;

    if (size < len1) {
        pos = temp;
        ShowVStr(dest, x, y, "\x1E" + ExpStrL("", static_cast<uint8_t>(size - 2), '\xB0') + "\x1F", atr1);
        if (size - 2 - 1 < 10) ShowStr(dest, x, static_cast<uint8_t>(y + 1 + temp), "\xB2", atr2);
        else ShowVStr(dest, x, static_cast<uint8_t>(y + 1 + temp), "\xB2\xB2\xB2", atr2);
    } else {
        ShowVCStr(dest, x, y, "~\x1E~" + ExpStrL("", static_cast<uint8_t>(size - 2), '\xB1') + "~\x1F~", atr2, atr1);
    }
    return pos;
}

uint16_t Menu(std::vector<std::string>& data, uint8_t x, uint8_t y, uint16_t spos, uint8_t len, uint8_t len2,
              uint16_t count, const std::string& title)
{
    if (count == 0) return 0;
    if (mn_environment.v_dest == nullptr) mn_environment.v_dest = screen_ptr;

    m_max = static_cast<uint16_t>(title.size());
    mnu_data = &data;
    mnu_offset = 0;
    mnu_count = count;
    mnu_len = len;

    if (!mn_environment.unpolite) ScreenMemCopy(mn_environment.v_dest, scr_backup2);

    vscrollbar_pos = 0xffff;
    if (!mn_environment.preview) HideCursor();

    bool any_used = false;
    for (uint16_t idx = 1; idx <= count; ++idx) {
        mbuf[idx].key = OutKey(pstr(idx));
        if (!mn_setting.reverse_use) mbuf[idx].use = mbuf[idx].key != '~';
        else mbuf[idx].use = mbuf[idx].key == '~';
        if (mbuf[idx].use) any_used = true;
    }

    m_solid = false;
    if (!any_used) {
        for (uint16_t idx = 1; idx <= count; ++idx) mbuf[idx].use = true;
        m_solid = true;
    }

    for (uint16_t idx = 1; idx <= count; ++idx) {
        if (m_max < CStrLen(pstr(idx)) + mn_environment.descr_len)
            m_max = static_cast<uint16_t>(CStrLen(pstr(idx)) + mn_environment.descr_len);
    }

    if (mn_setting.center_box) {
        const int w = work_maxcol - m_max - 2;
        const int h = work_maxln - len2 - 1;
        x = static_cast<uint8_t>(w / 2 + w % 2);
        y = static_cast<uint8_t>(h / 2 + h % 2);
    }

    mnu_x = x;
    mnu_y = y;
    const uint8_t len2b = len2;
    mn_environment.xpos = x;
    mn_environment.ypos = y;
    mn_environment.xsize = static_cast<uint8_t>(m_max + 1);
    mn_environment.ysize = static_cast<uint8_t>(len2 + 1);
    mn_environment.desc_pos = static_cast<uint8_t>(y + len2 + 1);

    // SubPos / AddPos は len2 と count (見出しの分を引いた後の値) を使う
    const auto SubPos = [&](uint16_t& p) {
        const auto impl = [&](auto& self, uint16_t& p) -> void {
            uint16_t temp = p;
            if (temp > 1 && mbuf[temp + m_page - 2].use) {
                --temp;
            } else if (temp > 1) {
                --temp;
                self(self, temp);
            } else if (m_page > m_first) {
                --m_page;
            }
            if (mbuf[temp + m_page - 1].use) p = temp;
            else if (temp + m_page - 1 > m_first) self(self, temp);
        };
        impl(impl, p);
    };
    const auto AddPos = [&](uint16_t& p) {
        const auto impl = [&](auto& self, uint16_t& p) -> void {
            uint16_t temp = p;
            if (temp < len2 && temp < m_last && mbuf[temp + m_page].use) {
                ++temp;
            } else if (temp < len2 && temp < m_last) {
                ++temp;
                self(self, temp);
            } else if (m_page + temp <= m_last) {
                ++m_page;
            }
            if (mbuf[temp + m_page - 1].use) p = temp;
            else if (temp + m_page - 1 < m_last) self(self, temp);
        };
        impl(impl, p);
    };

    if (!mn_environment.unpolite) {
        const bool old_fr_shadow_enabled = fr_setting.shadow_enabled;
        fr_setting.shadow_enabled = mn_setting.shadow_enabled;
        if (mn_environment.intact_area) fr_setting.update_area = false;
        if (mn_setting.frame_enabled) {
            Frame(mn_environment.v_dest, x, y, static_cast<uint8_t>(x + m_max + 1), static_cast<uint8_t>(y + len2 + 1),
                  mn_setting.menu_attr, title, mn_setting.title_attr, mn_setting.frame_type);
        }
        if (mn_environment.intact_area) fr_setting.update_area = true;
        fr_setting.shadow_enabled = old_fr_shadow_enabled;

        const std::string contxt = DietStr(mn_environment.context,
            static_cast<uint8_t>(m_max + (mn_environment.context.size() - CStrLen(mn_environment.context))));
        if (mn_setting.frame_enabled) {
            ShowC3Str(mn_environment.v_dest, static_cast<uint8_t>(x + 1), static_cast<uint8_t>(y + len2 + 1),
                      "`" + ExpStrL("", static_cast<uint8_t>(m_max - CStrLen(contxt)), mn_setting.frame_type[1]) + "`" + contxt,
                      mn_setting.contxt_attr, mn_setting.contxt2_attr, mn_setting.menu_attr);
        }

        m_temp2 = len2;
        mnu_len2 = len2;

        if (len2 > count) len2 = static_cast<uint8_t>(count);
        if (len2 < 1) len2 = 1;
        if (spos < 1) spos = 1;
        if (spos > count) spos = count;

        mn_environment.refresh = menu_refresh;

        m_first = 1;
        m_last = count;
        while (!mbuf[m_first].use) ++m_first;
        while (!mbuf[m_last].use) --m_last;

        if (m_first <= mn_setting.topic_len) m_first = static_cast<uint16_t>(mn_setting.topic_len + 1);
        if (spos < m_first || spos > m_last) spos = m_first;
        m_idx2 = 1;
        m_page = 1;
        m_opage = 0xffff;
        m_opos = 0xffff;
        while (m_idx2 + m_page - 1 < spos) AddPos(m_idx2);
    }

    mnu_topic_len = mn_setting.topic_len;
    if (mnu_topic_len != 0) {
        mn_setting.topic_len = 0;
        menu_refresh();

        mn_setting.topic_len = mnu_topic_len;
        mnu_topic_len = 0;
        mnu_offset += mn_setting.topic_len;
        mnu_y = static_cast<uint8_t>(mnu_y + mn_setting.topic_len);
        len2 = static_cast<uint8_t>(len2 - mn_setting.topic_len);
        mnu_len2 = static_cast<uint8_t>(mnu_len2 - mn_setting.topic_len);
        std::copy(mbuf + mn_setting.topic_len + 1, mbuf + count + 1, mbuf + 1);

        for (int temp = 1; temp <= mn_setting.topic_len; ++temp) SubPos(m_idx2);
        count = static_cast<uint16_t>(count - mn_setting.topic_len);
        mnu_count = static_cast<uint16_t>(mnu_count - mn_setting.topic_len);
        m_first = static_cast<uint16_t>(m_first - mn_setting.topic_len);
        m_last = static_cast<uint16_t>(m_last - mn_setting.topic_len);
        menu_refresh();
    } else {
        menu_refresh();
    }

    // 項目の内容を書き換える (edit_contents)
    const auto edit_contents = [&](uint16_t item) {
        is_setting.append_enabled = true;
        is_setting.character_set = CharRange(' ', '_') | CharRange('a', '}') | CharRange(0x80, 0xff);   // ` と ~ を除く
        is_environment.locate_pos = 1;

        const std::string item_str = pstr(item);
        std::string temp;
        if (mn_environment.edit_pos > 0 && mn_environment.edit_pos < m_max - 2)
            temp = Copy(item_str, mn_environment.edit_pos + 1, static_cast<int>(item_str.size()) - mn_environment.edit_pos + 1);
        else
            temp = CutStr(item_str);

        mn_environment.is_editing = true;
        while (!temp.empty() && temp.back() == ' ') temp.pop_back();
        const uint8_t w = static_cast<uint8_t>(m_max - 2 - mn_environment.edit_pos + 1);
        temp = InputStr(temp, static_cast<uint8_t>(x + 1 + mn_environment.edit_pos), static_cast<uint8_t>(y + m_idx2), w, w,
                        mn_setting.text2_attr, mn_setting.default_attr);
        mn_environment.is_editing = false;

        HideCursor();
        if (is_environment.keystroke == kENTER) {
            if (mn_environment.edit_pos > 0 && mn_environment.edit_pos < m_max - 2)
                temp = Copy(item_str, 1, mn_environment.edit_pos) + temp;
            else
                temp = CutStr(temp);
            const size_t i = mnu_offset + item - 1;
            if (i < data.size()) data[i] = Copy(temp, 1, len);
        }

        mn_environment.do_refresh = true;
        menu_refresh();
    };

    const auto RetKey = [&](uint8_t code) -> uint16_t {
        for (uint16_t temp = 1; temp <= count; ++temp) {
            if (UpCase(mbuf[temp].key) == UpCase(static_cast<char>(code))) return temp;
        }
        return 0;
    };

    mn_environment.curr_page = m_page;
    mn_environment.curr_pos = static_cast<uint16_t>(m_idx2 + m_page - 1);
    mn_environment.curr_item = CutStr(pstr(static_cast<uint16_t>(m_idx2 + m_page - 1)));
    mn_environment.keystroke = 0xffff;
    if (mn_environment.ext_proc) mn_environment.ext_proc();

    bool qflg = false;
    if (mn_environment.preview) {
        mn_environment.preview = false;
        mn_environment.unpolite = true;
    } else {
        do {
            mn_environment.keystroke = m_key;
            m_key = getkey();
            const uint8_t lo = static_cast<uint8_t>(m_key & 0xff);
            if (!qflg) {
                if (lo >= 0x20) {
                    const uint16_t idx = RetKey(lo);
                    if (idx != 0) {
                        menu_refresh();
                        m_idx2 = idx;
                        if (!(m_key == mn_setting.terminate_keys[1] && mn_setting.edit_contents)) qflg = true;
                        else edit_contents(idx);
                    }
                } else if (!shift_pressed() && !ctrl_pressed() && !alt_pressed()) {
                    switch (m_key) {
                    case kUP:
                        if (m_page + m_idx2 - 1 > m_first || !mn_setting.cycle_moves) {
                            SubPos(m_idx2);
                        } else {
                            m_idx2 = len2;
                            m_page = static_cast<uint16_t>(count - len2 + 1);
                            if (!mbuf[m_idx2 + m_page - 1].use) SubPos(m_idx2);
                        }
                        break;

                    case kDOWN:
                        if (m_page + m_idx2 - 1 < m_last || !mn_setting.cycle_moves) {
                            AddPos(m_idx2);
                        } else {
                            m_idx2 = 1;
                            m_page = 1;
                            if (!mbuf[m_idx2 + m_page - 1].use) AddPos(m_idx2);
                        }
                        break;

                    case kHOME:
                        if (mn_setting.homing_pos == 0) {
                            m_idx2 = 1;
                            m_page = 1;
                        } else if (m_idx2 + m_page - 1 > mn_setting.homing_pos && mn_setting.homing_pos < count) {
                            do SubPos(m_idx2); while (!(m_idx2 + m_page - 1 <= mn_setting.homing_pos));
                        } else {
                            m_idx2 = 1;
                            m_page = 1;
                        }
                        if (!mbuf[m_idx2 + m_page - 1].use) AddPos(m_idx2);
                        break;

                    case kEND:
                        if (mn_setting.homing_pos == 0) {
                            m_idx2 = len2;
                            m_page = static_cast<uint16_t>(count - len2 + 1);
                        } else if (m_idx2 + m_page - 1 < mn_setting.homing_pos && mn_setting.homing_pos < count) {
                            do AddPos(m_idx2); while (!(m_idx2 + m_page - 1 >= mn_setting.homing_pos));
                        } else {
                            m_idx2 = len2;
                            m_page = static_cast<uint16_t>(count - len2 + 1);
                        }
                        if (!mbuf[m_idx2 + m_page - 1].use) SubPos(m_idx2);
                        break;

                    case kPgUP:
                        if (m_idx2 + m_page - 1 - (len2 - 1) > mn_setting.homing_pos ||
                            m_idx2 + m_page - 1 <= mn_setting.homing_pos || mn_setting.homing_pos == 0 ||
                            !(mn_setting.homing_pos < count)) {
                            for (int temp = 1; temp <= len2 - 1; ++temp) SubPos(m_idx2);
                        } else {
                            do SubPos(m_idx2); while (!(m_idx2 + m_page - 1 <= mn_setting.homing_pos));
                        }
                        break;

                    case kPgDOWN:
                        if (m_idx2 + m_page - 1 + (len2 - 1) < mn_setting.homing_pos ||
                            m_idx2 + m_page - 1 >= mn_setting.homing_pos || mn_setting.homing_pos == 0 ||
                            !(mn_setting.homing_pos < count)) {
                            for (int temp = 1; temp <= len2 - 1; ++temp) AddPos(m_idx2);
                        } else {
                            do AddPos(m_idx2); while (!(m_idx2 + m_page - 1 >= mn_setting.homing_pos));
                        }
                        break;
                    }
                }
            }

            if (LookUpKey(m_key, mn_setting.terminate_keys, 50)) {
                if (!(m_key == mn_setting.terminate_keys[1] && mn_setting.edit_contents)) qflg = true;
                else edit_contents(static_cast<uint16_t>(m_idx2 + m_page - 1));
            }

            mn_environment.curr_page = m_page;
            mn_environment.curr_pos = static_cast<uint16_t>(m_idx2 + m_page - 1);
            mn_environment.curr_item = CutStr(pstr(static_cast<uint16_t>(m_idx2 + m_page - 1)));
            menu_refresh();
            mn_environment.keystroke = m_key;
            if (mn_environment.ext_proc) mn_environment.ext_proc();
            draw_screen();
        } while (!(qflg || _force_program_quit));
    }

    if (mn_environment.winshade && !mn_environment.unpolite) {
        if (move_to_screen_routine) {
            move_to_screen_data = scr_backup2;
            move_to_screen_area[1] = x;
            move_to_screen_area[2] = y;
            move_to_screen_area[3] = static_cast<uint8_t>(x + m_max + 1 + 2);
            move_to_screen_area[4] = static_cast<uint8_t>(y + len2b + 1 + 1);
            move_to_screen_routine();
        } else {
            ScreenMemCopy(scr_backup2, mn_environment.v_dest);
        }
    }

    return static_cast<uint16_t>(m_idx2 + m_page - 1);
}

} // namespace at2
