#include "MenuLib.h"

#include "DialogIO.h"
#include "StringIO.h"
#include "../platform/System.h"
#include "../platform/input/Keyboard.h"

namespace at2
{

MenuLib MenuLib1;
MenuLib MenuLib2;

namespace
{

constexpr uint16_t BYTE_NULL = 0xff;
constexpr uint16_t WORD_NULL = 0xffff;

char UpCase(char c) { return (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c; }

char OutKey(const std::string& str)
{
    const size_t pos = str.find('~');
    if (pos == std::string::npos) return '~';
    if (pos + 2 >= str.size() || str[pos + 2] != '~') return '~';
    return str[pos + 1];
}

} // namespace

MenuLib::MenuLib()
{
    // オリジナルの初期値 (const の初期化) は起動時の MenuLib1_Init ですぐ上書きされるため、Init の値で始める
    mn_setting.edit_contents = false;
    mn_setting.reverse_use = false;
    mn_setting.posbar_enabled = true;
    mn_setting.fixed_len = 0;
    for (uint16_t& key : mn_setting.terminate_keys) key = 0;
    mn_setting.terminate_keys[0] = kESC;
    mn_setting.terminate_keys[1] = kENTER;
    mn_environment.edit_pos = 0;
    mn_environment.curr_pos = 0;
    Init();
}

void MenuLib::Init()
{
    // frame_single (別の翻訳単位の変数で初期化順が決まらないため、同じ内容を直接書く)
    mn_setting.frame_type = "\xDA\xC4\xBF\xB3\xB3\xC0\xC4\xD9";
    mn_setting.center_box = false;
    mn_setting.shadow_enabled = false;
    mn_setting.cycle_moves = false;
    mn_setting.title_attr = dialog_background + dialog_title;
    mn_setting.menu_attr = dialog_background + dialog_text;
    mn_setting.text_attr = dialog_background + dialog_item;
    mn_setting.text2_attr = dialog_sel_itm_bck + dialog_sel_itm;
    mn_setting.default_attr = dialog_def_bckg + dialog_def;
    mn_setting.short_attr = dialog_background + dialog_short;
    mn_setting.short2_attr = dialog_sel_itm_bck + dialog_sel_short;
    mn_setting.disbld_attr = dialog_background + dialog_item_dis;
    mn_setting.contxt_attr = dialog_background + dialog_context;
    mn_setting.contxt2_attr = dialog_background + dialog_context_dis;
    mn_setting.terminate_keys[2] = kTAB;
    mn_setting.terminate_keys[3] = kLEFT;
    mn_setting.terminate_keys[4] = kRIGHT;
    mn_setting.terminate_keys[5] = kCtrlO;
    mn_setting.terminate_keys[6] = kF1;
    mn_setting.terminate_keys[7] = kShTAB;

    mn_environment.v_dest = nullptr;   // screen_ptr (Menu で設定する)
    mn_environment.keystroke = 0;
    mn_environment.context.clear();
    mn_environment.unpolite = false;
    mn_environment.winshade = true;
    mn_environment.ext_proc = nullptr;
    mn_environment.do_refresh = false;
    mn_environment.preview = false;
}

std::string MenuLib::pstr(uint16_t item) const
{
    std::string temp;
    if (item >= 1 && item <= mnu_data_->size()) temp = Copy((*mnu_data_)[item - 1], 1, mnu_len_);
    if (!solid_) return ExpStrR(temp, static_cast<uint8_t>(mnu_len_ - 2), ' ');
    return ExpStrR(temp, mnu_len_, ' ');
}

// オリジナルは dialogio.pas の mn_environment の説明 (descr) を見る
std::string MenuLib::pdes(uint16_t item) const
{
    std::string temp;
    const auto* descr = at2::mn_environment.descr;
    if (descr != nullptr && item >= 1 && item - 1u < descr->size())
        temp = Copy((*descr)[item - 1], 1, at2::mn_environment.descr_len);
    return ExpStrR(temp, at2::mn_environment.descr_len, ' ');
}

void MenuLib::refresh()
{
    if (page_ == opage_ && idx2_ == opos_ && !mn_environment.do_refresh) return;
    opage_ = page_;
    opos_ = idx2_;
    mn_environment.do_refresh = false;

    const auto width = [this](const std::string& s, const std::string& d, size_t len_d) {
        return static_cast<uint8_t>(max_ + (s.size() + len_d) - CStrLen(s + d));
    };

    for (int idx = page_; idx <= mnu_len2_ + page_ - 1; ++idx) {
        const uint16_t sel = static_cast<uint16_t>(idx2_ + page_ - 1);
        if (idx == sel) {
            // ShowCStr_clone (fixed_len = 0 なので ShowCStr と同じ)
            const std::string s = pstr(sel), d = pdes(sel);
            ShowCStr(mn_environment.v_dest, static_cast<uint8_t>(mnu_x_ + 1), static_cast<uint8_t>(mnu_y_ + idx2_),
                     ExpStrR(s + d, width(s, d, d.size()), ' '), mn_setting.text2_attr, mn_setting.short2_attr);
        } else {
            const std::string s = pstr(static_cast<uint16_t>(idx)), d = pdes(static_cast<uint16_t>(idx));
            const size_t len_d = pdes(sel).size();
            if (mbuf[idx].use) {
                ShowCStr(mn_environment.v_dest, static_cast<uint8_t>(mnu_x_ + 1), static_cast<uint8_t>(mnu_y_ + idx - page_ + 1),
                         ExpStrR(s + d, width(s, d, len_d), ' '), mn_setting.text_attr, mn_setting.short_attr);
            } else {
                ShowCStr(mn_environment.v_dest, static_cast<uint8_t>(mnu_x_ + 1), static_cast<uint8_t>(mnu_y_ + idx - page_ + 1),
                         ExpStrR(s + d, width(s, d, len_d), ' '), mn_setting.disbld_attr, mn_setting.disbld_attr);
            }
        }
    }

    vscrollbar_pos_ = VScrollBar(mn_environment.v_dest, static_cast<uint8_t>(mnu_x_ + max_ + 1), static_cast<uint8_t>(mnu_y_ + 1),
                                 static_cast<uint8_t>(temp2_), mnu_count_, static_cast<uint16_t>(idx2_ + page_ - 1),
                                 vscrollbar_pos_, mn_setting.menu_attr, mn_setting.menu_attr);
}

void MenuLib::SubPos(uint16_t& p)
{
    uint16_t temp = p;
    if (temp > 1 && mbuf[temp + page_ - 2].use) {
        --temp;
    } else if (temp > 1) {
        --temp;
        SubPos(temp);
    } else if (page_ > first_) {
        --page_;
    }
    if (mbuf[temp + page_ - 1].use) p = temp;
    else if (temp + page_ - 1 > first_) SubPos(temp);
}

void MenuLib::AddPos(uint16_t& p)
{
    uint16_t temp = p;
    if (temp < len2_ && temp < last_ && mbuf[temp + page_].use) {
        ++temp;
    } else if (temp < len2_ && temp < last_) {
        ++temp;
        AddPos(temp);
    } else if (page_ + temp <= last_) {
        ++page_;
    }
    if (mbuf[temp + page_ - 1].use) p = temp;
    else if (temp + page_ - 1 < last_) AddPos(temp);
}

uint16_t MenuLib::Menu(std::vector<std::string>& data, uint8_t x, uint8_t y, uint16_t spos, uint8_t len, uint8_t len2,
                       uint16_t count, const std::string& title)
{
    if (count == 0) return 0;
    if (mn_environment.v_dest == nullptr) mn_environment.v_dest = screen_ptr;

    max_ = static_cast<uint16_t>(title.size());
    mnu_data_ = &data;
    mnu_count_ = count;
    mnu_len_ = len;
    len2_ = len2;

    if (!mn_environment.unpolite) ScreenMemCopy(mn_environment.v_dest, scr_backup);

    vscrollbar_pos_ = BYTE_NULL;
    if (!mn_environment.preview) HideCursor();

    bool any_used = false;
    for (uint16_t idx = 1; idx <= count; ++idx) {
        mbuf[idx].key = OutKey(pstr(idx));
        if (!mn_setting.reverse_use) mbuf[idx].use = mbuf[idx].key != '~';
        else mbuf[idx].use = mbuf[idx].key == '~';
        if (mbuf[idx].use) any_used = true;
    }

    solid_ = false;
    if (!any_used) {
        for (uint16_t idx = 1; idx <= count; ++idx) mbuf[idx].use = true;
        solid_ = true;
    }

    for (uint16_t idx = 1; idx <= count; ++idx) {
        if (max_ < CStrLen(pstr(idx)) + at2::mn_environment.descr_len)
            max_ = static_cast<uint16_t>(CStrLen(pstr(idx)) + at2::mn_environment.descr_len);
    }

    if (mn_setting.center_box) {
        const int w = work_maxcol - max_ - 2;
        const int h = work_maxln - len2 - 1;
        x = static_cast<uint8_t>(w / 2 + w % 2);
        y = static_cast<uint8_t>(h / 2 + h % 2);
    }

    mnu_x_ = x;
    mnu_y_ = y;
    len2b_ = len2;

    if (!mn_environment.unpolite) {
        const bool old_fr_shadow_enabled = fr_setting.shadow_enabled;
        fr_setting.shadow_enabled = mn_setting.shadow_enabled;
        fr_setting.update_area = false;
        Frame(mn_environment.v_dest, x, y, static_cast<uint8_t>(x + max_ + 1), static_cast<uint8_t>(y + len2 + 1),
              mn_setting.menu_attr, title, mn_setting.title_attr, mn_setting.frame_type);
        fr_setting.update_area = true;
        fr_setting.shadow_enabled = old_fr_shadow_enabled;

        const std::string contxt = DietStr(mn_environment.context,
            static_cast<uint8_t>(max_ + (mn_environment.context.size() - CStrLen(mn_environment.context))));
        ShowCStr(mn_environment.v_dest, static_cast<uint8_t>(x + max_ + 1 - CStrLen(contxt)), static_cast<uint8_t>(y + len2 + 1),
                 contxt, mn_setting.contxt_attr, mn_setting.contxt2_attr);

        temp2_ = len2;
        if (len2 > count) len2 = static_cast<uint8_t>(count);
        if (len2 < 1) len2 = 1;
        if (spos < 1) spos = 1;
        if (spos > count) spos = count;
        len2_ = len2;

        mnu_len2_ = len2;

        first_ = 1;
        last_ = count;
        while (!mbuf[first_].use) ++first_;
        while (!mbuf[last_].use) --last_;

        if (spos < first_ || spos > last_) spos = first_;
        idx2_ = 1;
        page_ = 1;
        opage_ = WORD_NULL;
        opos_ = WORD_NULL;
        while (idx2_ + page_ - 1 < spos) AddPos(idx2_);
    }

    const auto RetKey = [&](uint8_t code) -> uint16_t {
        for (uint16_t temp = 1; temp <= count; ++temp) {
            if (UpCase(mbuf[temp].key) == UpCase(static_cast<char>(code))) return temp;
        }
        return 0;
    };

    mn_environment.curr_pos = static_cast<uint16_t>(idx2_ + page_ - 1);
    mn_environment.keystroke = WORD_NULL;
    if (mn_environment.ext_proc) mn_environment.ext_proc();

    bool qflg = false;
    if (mn_environment.preview) {
        mn_environment.preview = false;
        mn_environment.unpolite = true;
        refresh();
    } else {
        do {
            const uint16_t key = getkey();
            // edit_contents は使わない (REMAP では常に FALSE)
            if (LookUpKey(key, mn_setting.terminate_keys, 50)) qflg = true;

            if (!qflg) {
                const uint8_t lo = static_cast<uint8_t>(key & 0xff);
                if (lo >= 0x20) {
                    const uint16_t idx = RetKey(lo);
                    if (idx != 0) {
                        refresh();
                        idx2_ = idx;
                        qflg = true;
                    }
                } else if (!shift_pressed() && !ctrl_pressed() && !alt_pressed()) {
                    switch (key) {
                    case kUP:
                        if (page_ + idx2_ - 1 > first_ || !mn_setting.cycle_moves) {
                            SubPos(idx2_);
                        } else {
                            idx2_ = len2;
                            page_ = static_cast<uint16_t>(count - len2 + 1);
                            if (!mbuf[idx2_ + page_ - 1].use) SubPos(idx2_);
                        }
                        break;

                    case kDOWN:
                        if (page_ + idx2_ - 1 < last_ || !mn_setting.cycle_moves) {
                            AddPos(idx2_);
                        } else {
                            idx2_ = 1;
                            page_ = 1;
                            if (!mbuf[idx2_ + page_ - 1].use) AddPos(idx2_);
                        }
                        break;

                    case kHOME:
                        idx2_ = 1;
                        page_ = 1;
                        if (!mbuf[idx2_ + page_ - 1].use) AddPos(idx2_);
                        break;

                    case kEND:
                        idx2_ = len2;
                        page_ = static_cast<uint16_t>(count - len2 + 1);
                        if (!mbuf[idx2_ + page_ - 1].use) SubPos(idx2_);
                        break;

                    case kPgUP:
                        for (int temp = 1; temp <= len2 - 1; ++temp) SubPos(idx2_);
                        break;

                    case kPgDOWN:
                        for (int temp = 1; temp <= len2 - 1; ++temp) AddPos(idx2_);
                        break;
                    }
                }
            }

            mn_environment.curr_pos = static_cast<uint16_t>(idx2_ + page_ - 1);
            refresh();
            mn_environment.keystroke = key;
            if (mn_environment.ext_proc) mn_environment.ext_proc();
            draw_screen();
        } while (!(qflg || _force_program_quit));
    }

    if (mn_environment.winshade && !mn_environment.unpolite) {
        if (move_to_screen_routine) {
            move_to_screen_data = scr_backup;
            move_to_screen_area[1] = x;
            move_to_screen_area[2] = y;
            move_to_screen_area[3] = static_cast<uint8_t>(x + max_ + 1 + 2);
            move_to_screen_area[4] = static_cast<uint8_t>(y + len2b_ + 1 + 1);
            move_to_screen_routine();
        } else {
            ScreenMemCopy(scr_backup, mn_environment.v_dest);
        }
    }

    return static_cast<uint16_t>(idx2_ + page_ - 1);
}

} // namespace at2
