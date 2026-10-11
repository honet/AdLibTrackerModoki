#include "DialogIO.h"

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

} // namespace at2
