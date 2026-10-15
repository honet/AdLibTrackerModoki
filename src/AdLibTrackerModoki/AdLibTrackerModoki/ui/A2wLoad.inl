// ---------------------------------------------------------------------------
// A2W の読み込みとマクロブラウザ (adt2ext5.pas の a2w_file_loader とその補助処理、adt2ext4.pas の MACRO_BROWSER)
// ---------------------------------------------------------------------------
//
// MainScreen.cpp の無名名前空間の中で、BankLoad.inl の後に #include する。
//
// 窓は 2 つある。
//   楽器マクロの窓 (_jmp2): 楽器の一覧と、選んでいる楽器の FM レジスタのマクロ (a2w_lister_external_proc)
//   アルペジオ / ビブラートのマクロの窓 (_jmp1): 表の一覧と、選んでいる表のグラフ (a2w_macro_lister_external_proc)
// A2W ファイルの中身 (temp_songdata) を見るときと、曲のデータを見るとき (MACRO_BROWSER) の両方で使う。

// typcons2.inc
const std::string av_browser_str[6] = {
    "\xC4\xC4\xC4\xC4\xD2\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4"
    "\xC4\xC4\xC4\xC4\xC4\xD2\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4"
    "\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4",
    "    \xBA     ~ARPEGGiO MACRO TABLE~     \xBA        ~ViBRATO MACRO TABLE~",
    "    \xC7\xC4\xC4\xC4\xC4\xC4\xC2\xC4\xC4\xC4\xC4\xC4\xC2\xC4\xC4\xC4\xC4\xC4\xC2\xC4\xC4\xC4\xC4\xC4\xC2\xC4\xC4\xC4\xC4\xC4"
    "\xC4\xD7\xC4\xC4\xC4\xC4\xC4\xC2\xC4\xC4\xC4\xC4\xC4\xC2\xC4\xC4\xC4\xC4\xC4\xC2\xC4\xC4\xC4\xC4\xC4\xC2\xC4\xC4\xC4\xC4"
    "\xC4\xC2\xC4\xC4\xC4\xC4\xC4\xC4",
    " NO.\xBALEN. \xB3SPEED\xB3L.BEG\xB3L.LEN\xB3K.OFF \xBALEN. \xB3SPEED\xB3" "DELAY\xB3L.BEG\xB3L.LEN\xB3K.OFF ",
    "\xCD\xCD\xCD\xCD\xCE\xCD\xCD\xCD\xCD\xCD\xD8\xCD\xCD\xCD\xCD\xCD\xD8\xCD\xCD\xCD\xCD\xCD\xD8\xCD\xCD\xCD\xCD\xCD\xD8\xCD"
    "\xCD\xCD\xCD\xCD\xCD\xCE\xCD\xCD\xCD\xCD\xCD\xD8\xCD\xCD\xCD\xCD\xCD\xD8\xCD\xCD\xCD\xCD\xCD\xD8\xCD\xCD\xCD\xCD\xCD\xD8"
    "\xCD\xCD\xCD\xCD\xCD\xD8\xCD\xCD\xCD\xCD\xCD\xCD",
    "\xCD\xCD\xCD\xCD\xCA\xCD\xCD\xCD\xCD\xCD\xCF\xCD\xCD\xCD\xCD\xCD\xCF\xCD\xCD\xCD\xCD\xCD\xCF\xCD\xCD\xCD\xCD\xCD\xCF\xCD"
    "\xCD\xCD\xCD\xCD\xCD\xCA\xCD\xCD\xCD\xCD\xCD\xCF\xCD\xCD\xCD\xCD\xCD\xCF\xCD\xCD\xCD\xCD\xCD\xCF\xCD\xCD\xCD\xCD\xCD\xCF"
    "\xCD\xCD\xCD\xCD\xCD\xCF\xCD\xCD\xCD\xCD\xCD\xCD",
};

const std::string a2w_header_str[3] = {
    " iNSTRUMENT                                 iNSTRUMENT                  ",
    " NAME    DESCRiPTiON                        VOiCE     MACROS            ",
    std::string(72, '\xCD'),
};

const std::string a2w_header_hires1_str[3] = {
    " iNSTRUMENT                                                               iNSTRUMENT                  ",
    " NAME    DESCRiPTiON                        \xDA" "20\xBF \xDA" "40\xBF \xDA" "60\xBF \xDA" "80\xBF \xDA"
    "E0\xBF C0   VOiCE     MACROS            ",
    std::string(102, '\xCD'),
};

const std::string a2w_header_hires2_str[3] = {
    " iNSTRUMENT                                                               PANNiNG            iNSTRUMENT                  ",
    " NAME    DESCRiPTiON                        \xDA" "20\xBF \xDA" "40\xBF \xDA" "60\xBF \xDA" "80\xBF \xDA"
    "E0\xBF C0   \xAA  c  \xAB   F.TUNE   VOiCE     MACROS            ",
    std::string(121, '\xCD'),
};

namespace a2wload
{
    // typcons1.inc (adtrack2.ini の macro editor window のうち、MacroEdit.inl にないもの)
    constexpr uint8_t macro_title = 0x0b;
    constexpr uint8_t macro_topic2 = 0x0d;
    constexpr uint8_t macro_hi_topic = 0x0e;
    constexpr uint8_t macro_item = 0x0f;
    constexpr uint8_t macro_short = 0x0e;
    constexpr uint8_t macro_item_dis = 0x07;
    constexpr uint8_t macro_sel_itm_bck = 0x70;
    constexpr uint8_t macro_sel_itm = 0x00;
    constexpr uint8_t macro_sel_short = 0x04;

    // typcons1.inc: FM レジスタ表の各列の、カーソルを置く位置。[1..35] は [0..34]
    const uint8_t pos5vw[35] = { 1, 3, 5, 7, 9, 11, 11, 14, 16, 18, 19, 20, 21, 23, 25, 27, 29, 31, 33, 33,
                                 36, 38, 40, 41, 42, 43, 45, 47, 49, 49, 49, 49, 54, 56, 56 };

    // adt2ext5.pas のユニット変数
    uint8_t xstart = 0, ystart = 0;
    uint8_t window_xsize = 0, window_ysize = 0;
    std::string context_str, context_str2, context_str3;
    uint8_t a2w_institle_pos = 0;
    bool update_current_inst = false;
    uint8_t xstart_arp = 0, ystart_arp = 0, xstart_vib = 0, ystart_vib = 0;
    uint8_t scrollbar_xstart = 0, scrollbar_ystart = 0, scrollbar_size = 0;
    uint8_t macro_table_size = 0;
    uint8_t arpeggio_table_idx = 1, vibrato_table_idx = 1;
    uint8_t arpeggio_table_pos = 1, vibrato_table_pos = 1;
    uint8_t _a2w_lister_count = 0;
    uint8_t _fmreg_add_prev_size = 0;
    uint8_t fmreg_cursor_pos = 1, fmreg_left_margin = 1, fmreg_hpos = 1, fmreg_vpos = 1, fmreg_instr = 1, fmreg_page = 1;
    std::string fmreg_str;   // 最後に表示した行の文字列 (カーソルの横移動で区切りの位置を見る)
    uint8_t fmreg_scrlbar_size = 0, fmreg_scrlbar_items = 0;

    // adt2unit.pas の min / max (min は大きい方、max は小さい方を返す)
    int pmin(int value, int minimum) { return value < minimum ? minimum : value; }
    int pmax(int value, int maximum) { return value > maximum ? maximum : value; }

    // 読み込みの前の曲データ (オリジナルの songdata_bak)
    FixedSongData& songdata_bak()
    {
        static const auto data = std::make_unique<FixedSongData>();
        return *data;
    }

    // s.macro_table[idx]。オリジナルは表の番号 0 も範囲外のまま読み書きする (直前の instr_macros[255] の末尾)
    MacroTable& mt_at(FixedSongData& s, uint8_t idx)
    {
        uint8_t* base = reinterpret_cast<uint8_t*>(&s.macro_table[0]);
        return *reinterpret_cast<MacroTable*>(base + (static_cast<ptrdiff_t>(idx) - 1) * static_cast<ptrdiff_t>(sizeof(MacroTable)));
    }

    // FM レジスタ表の幅 (31+window_xsize-82-_fmreg_add_prev_size)
    int fmreg_width() { return 31 + window_xsize - 82 - _fmreg_add_prev_size; }

    // 空でない最後のアルペジオ / ビブラートの表
    uint8_t count_macros()
    {
        FixedSongData& ts = temp_songdata();
        uint8_t result = 255;
        while (result > 0 && is_empty_data(&at1(ts.macro_table, result).arpeggio, sizeof(ArpeggioTable)) &&
               is_empty_data(&at1(ts.macro_table, result).vibrato, sizeof(VibratoTable))) {
            --result;
        }
        return result;
    }

    // data を置く曲のアルペジオの表の番号。空いていなければ 0。
    // 空きか同じ内容の表を探し、なければ後ろから長さ 0 の表を探す。
    // オリジナルは 2 番目の探し方で、曲ではなく temp_songdata (読み込んだバンク) の表の長さを見る
    uint8_t get_free_arpeggio_table_idx(const ArpeggioTable& data)
    {
        const FixedSongData& sd = songdata();
        const FixedSongData& ts = temp_songdata();
        uint8_t result = 0;
        bool free_flag = false;
        do {
            ++result;
            const ArpeggioTable& t = at1(sd.macro_table, result).arpeggio;
            if (is_empty_data(&t, sizeof(t)) || std::memcmp(&t, &data, sizeof(t)) == 0) free_flag = true;
        } while (!(free_flag || result == 255));

        if (!free_flag) {
            do {
                if (at1(ts.macro_table, result).arpeggio.length == 0) free_flag = true;
                else --result;
            } while (!(free_flag || result == 0));
        }
        return result;
    }

    uint8_t get_free_vibrato_table_idx(const VibratoTable& data)
    {
        const FixedSongData& sd = songdata();
        const FixedSongData& ts = temp_songdata();
        uint8_t result = 0;
        bool free_flag = false;
        do {
            ++result;
            const VibratoTable& t = at1(sd.macro_table, result).vibrato;
            if (is_empty_data(&t, sizeof(t)) || std::memcmp(&t, &data, sizeof(t)) == 0) free_flag = true;
        } while (!(free_flag || result == 255));

        if (!free_flag) {
            do {
                if (at1(ts.macro_table, result).vibrato.length == 0) free_flag = true;
                else --result;
            } while (!(free_flag || result == 0));
        }
        return result;
    }

    // 4OP の組の楽器を試聴するときの組 (上位: 1 つ目、下位: 2 つ目) の、窓の右下の表示
    std::string _4op_context_str()
    {
        const uint16_t t = get_4op_to_test();
        if (!(update_current_inst && t != 0 && (current_inst == HI(t) || current_inst == LO(t)))) return std::string();
        if (LO(t) == HI(t)) return " ~[~\xF4" + byte2hex(HI(t)) + "~,~\xF5" + byte2hex(HI(t)) + "~]~";
        if (current_inst == HI(t)) return " ~[~\xF4" + byte2hex(HI(t)) + "~,\xF5" + byte2hex(LO(t)) + "]~";
        return " ~[\xF4" + byte2hex(HI(t)) + ",~\xF5" + byte2hex(LO(t)) + "~]~";
    }

    // 窓の右下の SPEED などの表示
    void show_context()
    {
        const FixedSongData& sd = songdata();
        context_str3 = _4op_context_str();
        const std::string temps = "`" + ExpStrL("`" + context_str3 + context_str2 + context_str + " `[`SPEED:" +
                                                Num2str(sd.tempo * sd.macro_speedup, 10) + "\xAE`]` ",
                                            52, '\xCD');
        ShowC3Str(centered_frame_vdest, B(xstart + window_xsize - C3StrLen(temps)), B(ystart + window_ysize), temps,
                  macro_background + macro_context, macro_background + macro_context_dis, macro_background + macro_border);
    }

    void show_institle()
    {
        if (a2w_institle_pos != 0) {
            ShowStr(centered_frame_vdest, B(mn_environment.xpos + a2w_institle_pos), mn_environment.ypos, byte2hex(current_inst),
                    macro_background + macro_title);
        }
    }

    // Ctrl+[ / ]: 前後の楽器、Shift 付きはマクロの速度 (update_inst = FALSE なら楽器は変えない)
    void change_inst_or_speedup(uint16_t key, bool update_inst)
    {
        FixedSongData& sd = songdata();
        if (shift_pressed()) {
            if (key == kCtLbr) {
                if (sd.macro_speedup > 1) --sd.macro_speedup;
            } else {
                ++sd.macro_speedup;
                if (calc_max_speedup(sd.tempo) < sd.macro_speedup) sd.macro_speedup = calc_max_speedup(sd.tempo);
            }
            P().macro_speedup = sd.macro_speedup;
            keyboard_reset_buffer();
        } else if (update_inst && (key == kCtLbr ? current_inst > 1 : current_inst < 255)) {
            current_inst = B(current_inst + (key == kCtLbr ? -1 : 1));
            if (!(marked_instruments() == 2)) reset_marked_instruments();
            instrum_page = current_inst;
            STATUS_LINE_refresh();
            keyboard_reset_buffer();
        }
    }

    // --- アルペジオ / ビブラートのマクロの窓 ---

    // 選んでいる表のグラフ (_macro_preview_refresh)
    void _macro_preview_refresh()
    {
        FixedSongData& ts = temp_songdata();
        const tSCREEN_MEM_PTR dest = centered_frame_vdest;
        const uint8_t text = macro_background + macro_text;
        const uint8_t topic = macro_background + macro_topic;

        // arpeggio preview
        const ArpeggioTable& a = mt_at(ts, arpeggio_table_idx).arpeggio;
        ShowStr(dest, B(xstart_arp + 15), ystart_arp, "\xFD", macro_background + macro_topic2);
        ShowStr(dest, B(xstart_arp + 15), B(ystart_arp + 7), "\xFC", macro_background + macro_topic2);
        ShowVStr(dest, xstart_arp, B(ystart_arp + 1), "\xB3\xB3\xB3\xB3\xB3\x9E", text);
        ShowVStr(dest, B(xstart_arp + 30), B(ystart_arp + 1), "\xB3\xB3\xB3\xB3\xB3\x9E", text);

        int max_value = 0;
        for (int temp = 1; temp <= 255; ++temp) {
            if (a.data[temp - 1] > max_value && a.data[temp - 1] < 0x80) max_value = a.data[temp - 1];
        }
        ShowStr(dest, B(xstart_arp + 31), B(ystart_arp + 1), ExpStrR(Num2str(max_value, 10), 3, ' '), topic);
        ShowStr(dest, B(xstart_arp + 31), B(ystart_arp + 2), "+", topic);

        double d_factor = 90.0 / pmin(max_value, 1);
        for (int temp = -14; temp <= 14; ++temp) {
            const uint8_t x = B(xstart_arp + 15 + temp);
            const int page = arpeggio_table_pos + temp;
            if (page >= 1 && page <= 255) {
                const uint8_t value = a.data[page - 1];
                const uint8_t attr = LO(macroedit::def_attr(a, B(page)));
                if (value < 0x80) {
                    ShowVStr(dest, x, B(ystart_arp + 1), ExpStrL(macroedit::_gfx_bar_str(B(pas_round(value * d_factor)), false), 6, ' '),
                             attr);
                } else {
                    std::string note = note_layout_at(value - 0x80);
                    for (char& c : note) {
                        if (c == '-') c = '\xF1';
                    }
                    ShowVStr(dest, x, B(ystart_arp + 1), ExpStrL(note, 6, ' '), attr);
                }
            } else {
                ShowVStr(dest, x, B(ystart_arp + 1), ExpStrL("", 6, ' '), text);
            }
        }

        // vibrato preview
        const VibratoTable& v = mt_at(ts, vibrato_table_idx).vibrato;
        ShowStr(dest, B(xstart_vib + 15), ystart_vib, "\xFD", macro_background + macro_topic2);
        ShowStr(dest, B(xstart_vib + 15), B(ystart_vib + 7), "\xFC", macro_background + macro_topic2);
        ShowVStr(dest, xstart_vib, B(ystart_vib + 1), "\xB3\xB3\x9E\xB3\xB3\xB3", text);
        ShowVStr(dest, B(xstart_vib + 30), B(ystart_vib + 1), "\xB3\xB3\x9E\xB3\xB3\xB3", text);

        max_value = 0;
        for (int temp = 1; temp <= 255; ++temp) {
            if (std::abs(v.data[temp - 1]) > max_value) max_value = std::abs(v.data[temp - 1]);
        }
        ShowStr(dest, B(xstart_vib + 31), B(ystart_vib + 1), ExpStrR(Num2str(max_value, 10), 3, ' '), topic);
        ShowStr(dest, B(xstart_vib + 31), B(ystart_vib + 2), "+", topic);
        ShowStr(dest, B(xstart_vib + 31), B(ystart_vib + 5), "-", topic);
        ShowStr(dest, B(xstart_vib + 31), B(ystart_vib + 6), ExpStrR(Num2str(max_value, 10), 3, ' '), topic);

        d_factor = 45.0 / pmin(max_value, 1);
        for (int temp = -14; temp <= 14; ++temp) {
            const uint8_t x = B(xstart_vib + 15 + temp);
            const int page = vibrato_table_pos + temp;
            if (page >= 1 && page <= 255) {
                const int value = v.data[page - 1];
                const uint8_t attr = LO(macroedit::def_attr(v, B(page)));
                if (pas_round(value * d_factor) >= 0) {
                    ShowVStr(dest, x, B(ystart_vib + 1),
                             ExpStrR(ExpStrL(macroedit::_gfx_bar_str(B(pas_round(value * d_factor)), false), 3, ' '), 6, ' '), attr);
                } else {
                    ShowVStr(dest, x, B(ystart_vib + 1),
                             ExpStrL(ExpStrR(macroedit::_gfx_bar_str(B(pas_round(std::abs(value) * d_factor)), true), 3, ' '), 6, ' '),
                             attr);
                }
            } else {
                ShowVStr(dest, x, B(ystart_vib + 1), ExpStrR("", 6, ' '), text);
            }
        }
    }

    // a2w_macro_lister_external_proc_callback: 曲の楽器を、選んでいる表 (バンクのもの) で試聴する
    void a2w_macro_lister_external_proc_callback()
    {
        uint8_t test_ins1, test_ins2;
        const uint16_t t = get_4op_to_test_temp();
        if (t != 0) {
            test_ins1 = LO(t);
            test_ins2 = HI(t);
        } else {
            const uint16_t curr_inst = mn_environment.curr_pos;
            if (!(curr_inst >= 1 && curr_inst <= 255)) return;
            test_ins1 = B(curr_inst);
            test_ins2 = 0;
        }
        if (!shift_pressed() && !alt_pressed() && !ctrl_pressed()) {
            // test current instr, with current FM-data and macro and chosen Arp./Vib. macro tables from bank
            test_instrument_alt(count_channel(pattern_hpos), mn_environment.keystroke, false, false, true, test_ins1, test_ins2);
        }
    }

    // 表の一覧の Menu の ext_proc (a2w_macro_lister_external_proc)
    void a2w_macro_lister_external_proc()
    {
        PlayerLock lock;
        FixedSongData& sd = songdata();
        FixedSongData& ts = temp_songdata();
        const tSCREEN_MEM_PTR dest = centered_frame_vdest;
        static const char _check_chr[2] = { '\xFB', ' ' };   // [FALSE], [TRUE]

        uint8_t idx = B(Str2num(Copy(mn_environment.curr_item, 2, 2), 16));
        if (idx == 0) idx = 1;

        VScrollBar(dest, scrollbar_xstart, scrollbar_ystart, scrollbar_size, macro_table_size, idx, WORD_NULL,
                   macro_background + macro_border, macro_background + macro_border);

        arpeggio_table_idx = idx;
        vibrato_table_idx = idx;

        RegisterTable& tim = at1(ts.instr_macros, current_inst);
        RegisterTable& im = at1(sd.instr_macros, current_inst);
        const MacroTable& m = mt_at(ts, idx);
        const bool sh = shift_pressed();

        switch (mn_environment.keystroke) {
        case kSPACE:
            if (shift_pressed()) {
                arp_tab_selected = !arp_tab_selected;
                if (alt_pressed()) {
                    tim.arpeggio_table = 0;
                    arp_tab_selected = false;
                }
                if (arp_tab_selected) tim.arpeggio_table = arpeggio_table_idx;
                else if (!alt_pressed()) tim.arpeggio_table = im.arpeggio_table;
            }
            if (ctrl_pressed()) {
                vib_tab_selected = !vib_tab_selected;
                if (alt_pressed()) {
                    tim.vibrato_table = 0;
                    vib_tab_selected = false;
                }
                if (vib_tab_selected) tim.vibrato_table = vibrato_table_idx;
                else if (!alt_pressed()) tim.vibrato_table = im.vibrato_table;
            }
            break;

        case kESC:
            tim.arpeggio_table = im.arpeggio_table;
            tim.vibrato_table = im.vibrato_table;
            return;

        case kENTER:
            if (!arp_tab_selected && tim.arpeggio_table == 0) im.arpeggio_table = 0;
            if (!vib_tab_selected && tim.vibrato_table == 0) im.vibrato_table = 0;
            return;

        case kLEFT:
        case kShLEFT:
            if (sh && arpeggio_table_pos > 1) --arpeggio_table_pos;
            break;

        case kCtLEFT:
            if (sh && arpeggio_table_pos > 1) --arpeggio_table_pos;
            if (vibrato_table_pos > 1) --vibrato_table_pos;
            break;

        case kRIGHT:
        case kShRGHT:
            if (sh && arpeggio_table_pos < 255) ++arpeggio_table_pos;
            break;

        case kCtRGHT:
            if (sh && arpeggio_table_pos < 255) ++arpeggio_table_pos;
            if (vibrato_table_pos < 255) ++vibrato_table_pos;
            break;

        case kPgUP:
            if (sh) arpeggio_table_pos = arpeggio_table_pos - 18 > 1 ? B(arpeggio_table_pos - 18) : 1;
            break;

        case kCtPgUP:
            if (sh) arpeggio_table_pos = arpeggio_table_pos - 18 > 1 ? B(arpeggio_table_pos - 18) : 1;
            vibrato_table_pos = vibrato_table_pos - 18 > 1 ? B(vibrato_table_pos - 18) : 1;
            break;

        case kPgDOWN:
            if (sh) arpeggio_table_pos = arpeggio_table_pos + 18 < 255 ? B(arpeggio_table_pos + 18) : 255;
            break;

        case kCtPgDN:
            if (sh) arpeggio_table_pos = arpeggio_table_pos + 18 < 255 ? B(arpeggio_table_pos + 18) : 255;
            vibrato_table_pos = vibrato_table_pos + 18 < 255 ? B(vibrato_table_pos + 18) : 255;
            break;

        case kHOME:
            if (sh) arpeggio_table_pos = arpeggio_table_pos > m.arpeggio.length ? B(pmin(1, m.arpeggio.length)) : 1;
            break;

        case kCtHOME:
            vibrato_table_pos = vibrato_table_pos > m.vibrato.length ? B(pmin(1, m.vibrato.length)) : 1;
            break;

        case kEND:
            if (sh) arpeggio_table_pos = arpeggio_table_pos < m.arpeggio.length ? m.arpeggio.length : 255;
            break;

        case kCtEND:
            vibrato_table_pos = vibrato_table_pos < m.vibrato.length ? m.vibrato.length : 255;
            break;

        case kCtLbr:
        case kCtRbr:
            change_inst_or_speedup(mn_environment.keystroke, update_current_inst);
            break;
        }

        // 選んでいる表の番号 ([√] は表を使わない)
        const auto show_table_selector = [&](bool selected, uint8_t table, uint8_t table_idx, uint8_t x, uint8_t y) {
            uint8_t attr, attr2, attr3, temp;
            if (selected) {
                attr = B((macro_hi_text << 4) + (macro_background >> 4));
                attr2 = macro_background + macro_hi_text;
                attr3 = B((macro_hi_text << 4) + (macro_background >> 4));
                temp = table_idx;
            } else {
                attr = macro_background + macro_hi_text;
                attr2 = table != 0 ? macro_background + macro_hi_text : macro_background + macro_text_dis;
                attr3 = macro_background + macro_text;
                temp = table;
            }
            ShowC3Str(dest, x, y, std::string("`[`") + _check_chr[!selected && table == 0] + "`]`~ " + byte2hex(temp) + "~", attr,
                      attr2, attr3);
        };
        show_table_selector(arp_tab_selected, tim.arpeggio_table, arpeggio_table_idx, B(xstart_arp - 1), B(ystart_arp + 8));
        show_table_selector(vib_tab_selected, tim.vibrato_table, vibrato_table_idx, B(xstart_vib - 1), B(ystart_vib + 8));

        const uint8_t text = macro_background + macro_text;
        const uint8_t dis = macro_background + macro_text_dis;
        ShowStr(dest, B(xstart_arp + 6), B(ystart_arp + 8), "\x11", arpeggio_table_pos > 15 ? text : dis);
        ShowStr(dest, B(xstart_arp + 25), B(ystart_arp + 8), "\x10",
                arpeggio_table_pos < m.arpeggio.length - 15 + 1 ? text : dis);
        ShowStr(dest, B(xstart_vib + 6), B(ystart_vib + 8), "\x11", vibrato_table_pos > 15 ? text : dis);
        ShowStr(dest, B(xstart_vib + 24), B(ystart_vib + 8), "\x10",
                vibrato_table_pos < m.vibrato.length - 15 + 1 ? text : dis);

        ShowCStr(dest, B(xstart_arp + 10), B(ystart_vib + 8), "ARPEGGiO (~" + byte2hex(arpeggio_table_pos) + "~)", text,
                 macro_background + macro_hi_text);
        ShowCStr(dest, B(xstart_vib + 10), B(ystart_vib + 8), "ViBRATO (~" + byte2hex(vibrato_table_pos) + "~)", text,
                 macro_background + macro_hi_text);

        show_context();
        show_institle();

        arpvib_arpeggio_table = arpeggio_table_idx;
        arpvib_vibrato_table = vibrato_table_idx;

        _macro_preview_refresh();
        mn_environment.curr_pos = current_inst;
        a2w_macro_lister_external_proc_callback();
    }

    // --- 楽器マクロの窓 ---

    RegisterTable& fmreg_macro() { return at1(temp_songdata().instr_macros, fmreg_instr); }

    // 列 fmreg_col (1..35) が無効か。オリジナルはパーカッションの判定に、表示している楽器ではなく current_inst を使う
    bool _dis_fmreg_col(uint8_t fmreg_col)
    {
        const FixedSongData& ts = temp_songdata();
        const auto col = [&ts](int idx) { return at1(ts.dis_fmreg_col, fmreg_instr)[idx] != 0; };
        bool result = false;
        if (fmreg_col >= 1 && fmreg_col <= 5) result = col(fmreg_col - 1);
        else if (fmreg_col >= 6 && fmreg_col <= 7) result = col(5);
        else if (fmreg_col >= 8 && fmreg_col <= 18) result = col(fmreg_col - 2);
        else if (fmreg_col >= 19 && fmreg_col <= 20) result = col(17);
        else if (fmreg_col >= 21 && fmreg_col <= 28) result = col(fmreg_col - 3);
        else if (fmreg_col >= 29 && fmreg_col <= 32) result = col(26);
        else if (fmreg_col == 33) result = col(27);

        const uint8_t perc_voice = at1(ts.instr_data, current_inst).perc_voice;
        if (fmreg_col >= 14 && fmreg_col <= 28 && perc_voice >= 2 && perc_voice <= 5) result = true;
        return result;
    }

    int _fmreg_param(uint8_t page, uint8_t hpos)
    {
        return macroedit::fmreg_str_param(macroedit::fmreg_def_str(fmreg_macro().data[page - 1]), hpos);
    }

    void fmreg_page_refresh(uint8_t xpos, uint8_t ypos, uint8_t page)
    {
        const RegisterTable& m = fmreg_macro();
        const uint8_t attr = LO(macroedit::def_attr(m, page));
        fmreg_str = macroedit::fmreg_def_str(m.data[page - 1]);
        std::string fmreg_str2 = fmreg_str;

        int index = 0, index2 = 0;
        for (int fmreg_col = 1; fmreg_col <= 35 - 1; ++fmreg_col) {
            if (_dis_fmreg_col(B(fmreg_col))) {
                fmreg_str2.insert(static_cast<size_t>(pos5[fmreg_col - 1] + index - 1), 1, '`');
                fmreg_str2.insert(static_cast<size_t>(pos5[fmreg_col - 1] + index + 2 - 1), 1, '`');
                if (pos5[fmreg_col - 1] < fmreg_left_margin) index2 += 2;
                index += 2;
            }
        }

        const std::string temps = Copy(fmreg_str2, fmreg_left_margin + index2,
                                       static_cast<int>(fmreg_str2.size()) - fmreg_left_margin - index2 + 1);
        const std::string dummy_str = macro_retrig_str[m.data[page - 1].fm_data.FEEDBACK_FM >> 5];
        ShowC3Str(centered_frame_vdest, B(xpos + 3), ypos,
                  "\xB3~" + dummy_str + "~\xF6~" + macroedit::_str2(temps, B(fmreg_width())) + "~",
                  macro_background + macro_text, attr, macro_background + macro_text_dis);
    }

    // fmreg_str[i] (Pascal の文字列なので 0 は長さ)
    char fmreg_chr(int i)
    {
        if (i == 0) return static_cast<char>(fmreg_str.size());
        return (i >= 1 && i <= static_cast<int>(fmreg_str.size())) ? fmreg_str[i - 1] : '\0';
    }

    void _scroll_cur_left()
    {
        do {
            if (fmreg_cursor_pos > 1) --fmreg_cursor_pos;
            else --fmreg_left_margin;
        } while (!(fmreg_chr(fmreg_left_margin + fmreg_cursor_pos - 1 - 1) == ' ' || fmreg_left_margin + fmreg_cursor_pos - 1 == 1));
        fmreg_cursor_pos = B(pos5[fmreg_hpos - 1] - fmreg_left_margin + 1);
    }

    void _scroll_cur_right()
    {
        do {
            if (fmreg_cursor_pos < fmreg_width()) ++fmreg_cursor_pos;
            else ++fmreg_left_margin;
        } while (!(fmreg_chr(fmreg_left_margin + fmreg_cursor_pos - 1 + 1) == ' ' || fmreg_left_margin + fmreg_cursor_pos - 1 == 57));
        fmreg_cursor_pos = B(pos5[fmreg_hpos - 1] - fmreg_left_margin + 1);
    }

    void _dec_fmreg_hpos()
    {
        const uint8_t old_hpos_idx = pos5vw[fmreg_hpos - 1];
        uint8_t new_hpos_idx;
        do {
            --fmreg_hpos;
            new_hpos_idx = pos5vw[fmreg_hpos - 1];
            _scroll_cur_left();
        } while (!(fmreg_hpos == 1 || old_hpos_idx != new_hpos_idx));
        if (fmreg_hpos > 1) {
            while (pos5vw[fmreg_hpos - 2] == pos5vw[fmreg_hpos - 1]) {
                --fmreg_hpos;
                _scroll_cur_left();
            }
        }
    }

    void _inc_fmreg_hpos()
    {
        const uint8_t old_hpos_idx = pos5vw[fmreg_hpos - 1];
        uint8_t new_hpos_idx;
        do {
            ++fmreg_hpos;
            new_hpos_idx = pos5vw[fmreg_hpos - 1];
            _scroll_cur_right();
        } while (!(fmreg_hpos == 35 - 1 || old_hpos_idx != new_hpos_idx));
    }

    void fmreg_first_col()
    {
        fmreg_hpos = 1;
        fmreg_cursor_pos = 1;
        fmreg_left_margin = 1;
    }

    void fmreg_last_col()
    {
        fmreg_hpos = 35 - 1;
        fmreg_cursor_pos = B(pmax(pos5[fmreg_hpos - 1], fmreg_width()));
        fmreg_left_margin = B(pmin(pos5[35 - 1] - fmreg_width(), 1));
        _dec_fmreg_hpos();
        _inc_fmreg_hpos();
    }

    // 選んでいる列のグラフ (_fmreg_macro_preview_refresh)
    void _fmreg_macro_preview_refresh(uint8_t xs, uint8_t ys, uint8_t page)
    {
        const tSCREEN_MEM_PTR dest = centered_frame_vdest;
        const RegisterTable& m = fmreg_macro();
        const int add = _fmreg_add_prev_size;
        const uint8_t text = macro_background + macro_text;
        const uint8_t topic = macro_background + macro_topic;
        const bool signed_col = fmreg_hpos >= 29 && fmreg_hpos <= 33;

        ShowStr(dest, B(xs + 10 + add / 2), ys, "\xFD", macro_background + macro_topic2);
        ShowStr(dest, B(xs + 10 + add / 2), B(ys + 7), "\xFC", macro_background + macro_topic2);

        const std::string axis = !signed_col ? "\xB3\xB3\xB3\xB3\xB3\x9E" : "\xB3\xB3\x9E\xB3\xB3\xB3";
        ShowVStr(dest, xs, B(ys + 1), axis, text);
        ShowVStr(dest, B(xs + 20 + add), B(ys + 1), axis, text);

        int max_value = 0;
        for (int temp = 1; temp <= 255; ++temp) {
            if (std::abs(_fmreg_param(B(temp), fmreg_hpos)) > max_value) max_value = std::abs(_fmreg_param(B(temp), fmreg_hpos));
        }

        const uint8_t mx = B(xs + 20 + add + 1);
        const std::string max_str = ExpStrR(ExpStrL(Num2str(max_value, 16), 2, '0'), 3, ' ');
        ShowStr(dest, mx, B(ys + 1), max_str, topic);
        ShowStr(dest, mx, B(ys + 2), "+", topic);
        if (!signed_col) {
            ShowStr(dest, mx, B(ys + 5), " ", topic);
            ShowStr(dest, mx, B(ys + 6), ExpStrR("", 3, ' '), topic);
        } else {
            ShowStr(dest, mx, B(ys + 5), "-", topic);
            ShowStr(dest, mx, B(ys + 6), max_str, topic);
        }

        const double d_factor = (!signed_col ? 90.0 : 45.0) / pmin(max_value, 1);
        const bool dis = _dis_fmreg_col(fmreg_hpos);
        for (int temp = -9 - add / 2; temp <= 9 + add / 2; ++temp) {
            const uint8_t x = B(xs + 10 + temp + add / 2);
            const int p = page + temp;
            if (p >= 1 && p <= 255) {
                const uint8_t col_attr = !dis ? LO(macroedit::def_attr(m, B(p))) : macro_background + macro_text_dis;
                const int value = _fmreg_param(B(p), fmreg_hpos);
                if (!signed_col) {
                    ShowVStr(dest, x, B(ys + 1), ExpStrL(macroedit::_gfx_bar_str(B(pas_round(value * d_factor)), false), 6, ' '),
                             col_attr);
                } else if (pas_round(value * d_factor) >= 0) {
                    ShowVStr(dest, x, B(ys + 1),
                             ExpStrR(ExpStrL(macroedit::_gfx_bar_str(B(pas_round(value * d_factor)), false), 3, ' '), 6, ' '),
                             col_attr);
                } else {
                    ShowVStr(dest, x, B(ys + 1),
                             ExpStrL(ExpStrR(macroedit::_gfx_bar_str(B(pas_round(std::abs(value) * d_factor)), true), 3, ' '), 6, ' '),
                             col_attr);
                }
            } else {
                ShowVStr(dest, x, B(ys + 1), ExpStrL("", 6, ' '), text);
            }
        }
    }

    // a2w_lister_external_proc_callback: バンクの楽器を、FM マクロとアルペジオ / ビブラートの表も使って試聴する
    void a2w_lister_external_proc_callback()
    {
        uint8_t test_ins1, test_ins2;
        const uint16_t t = get_4op_to_test_temp();
        if (t != 0) {
            test_ins1 = LO(t);
            test_ins2 = HI(t);
        } else {
            const uint16_t curr_inst = mn_environment.curr_pos;
            if (!(curr_inst >= 1 && curr_inst <= 255)) return;
            test_ins1 = B(curr_inst);
            test_ins2 = 0;
        }
        if (!shift_pressed() && !alt_pressed() && !ctrl_pressed()) {
            // test instrument from bank, with chosen FM-data and macro and current Arp./Vib. macro tables
            test_instrument_alt(count_channel(pattern_hpos), mn_environment.keystroke, true, true, false, test_ins1, test_ins2);
        }
    }

    // 楽器の一覧の Menu の ext_proc (a2w_lister_external_proc)
    void a2w_lister_external_proc()
    {
        PlayerLock lock;
        const tSCREEN_MEM_PTR dest = centered_frame_vdest;
        const bool sh = shift_pressed();

        switch (mn_environment.keystroke) {
        case kUP:
            if (sh) {
                if (fmreg_vpos > 1) --fmreg_vpos;
                else if (fmreg_page > 1) --fmreg_page;
            }
            break;

        case kDOWN:
            if (sh) {
                if (fmreg_vpos < 6) ++fmreg_vpos;
                else if (fmreg_page < 255 - 6 + 1) ++fmreg_page;
            }
            break;

        case kPgUP:
            if (sh) {
                if (fmreg_page > 6) fmreg_page = B(fmreg_page - 6);
                else if (fmreg_page > 1) fmreg_page = 1;
                else fmreg_vpos = 1;
            }
            break;

        case kPgDOWN:
            if (sh) {
                if (fmreg_page + 6 < 255 - 6 + 1) fmreg_page = B(fmreg_page + 6);
                else if (fmreg_page < 255 - 6 + 1) fmreg_page = 255 - 6 + 1;
                else fmreg_vpos = 6;
            }
            break;

        case kHOME:
            if (sh) {
                fmreg_page = 1;
                fmreg_vpos = 1;
            }
            break;

        case kEND:
            if (sh) {
                fmreg_page = 255 - 6 + 1;
                fmreg_vpos = 6;
            }
            break;

        case kCtHOME:
            fmreg_first_col();
            break;

        case kCtEND:
            fmreg_last_col();
            break;

        case kLEFT:
            if (sh) {
                if (fmreg_hpos > 1) _dec_fmreg_hpos();
                else if (cycle_pattern) fmreg_last_col();
            }
            break;

        case kRIGHT:
            if (sh) {
                if (fmreg_hpos < 35 - 1) _inc_fmreg_hpos();
                else if (cycle_pattern) fmreg_first_col();
            }
            break;

        case kCtLbr:
        case kCtRbr:
            change_inst_or_speedup(mn_environment.keystroke, false);
            break;
        }

        for (int temp = 1; temp <= _a2w_lister_count; ++temp) {
            const int i = mn_environment.curr_page + temp - 1;
            const uint8_t attr = mn_environment.curr_pos == i ? mn_setting.text2_attr : mn_setting.text_attr;
            ShowStr(mn_environment.v_dest, B(mn_environment.xpos + 1), B(mn_environment.ypos + 3 + temp),
                    std::string(1, i <= 255 ? _4op_flag_column[i] : '\0'), attr);
        }

        fmreg_instr = B(Str2num(Copy(mn_environment.curr_item, 7, 2), 16));
        const FixedSongData* songdata_ptr = &temp_songdata();
        if (update_current_inst) {
            current_inst = fmreg_instr;
            instrum_page = fmreg_instr;
            STATUS_LINE_refresh();
            songdata_ptr = &songdata();
        }

        show_institle();
        show_context();

        const RegisterTable& m = at1(songdata_ptr->instr_macros, fmreg_instr);
        const uint8_t topic = macro_background + macro_topic;
        const uint8_t text = macro_background + macro_text;
        const int y0 = ystart + window_ysize - 10;
        ShowCStr(dest, B(xstart + 2), B(y0 + 2), "LENGTH:    ~" + byte2hex(m.length) + " ~", topic, text);
        ShowCStr(dest, B(xstart + 2), B(y0 + 3), "LOOP BEG.: ~" + byte2hex(m.loop_begin) + " ~", topic, text);
        ShowCStr(dest, B(xstart + 2), B(y0 + 4), "LOOP LEN.: ~" + byte2hex(m.loop_length) + " ~", topic, text);
        ShowCStr(dest, B(xstart + 2), B(y0 + 5), "KEY-OFF:   ~" + byte2hex(m.keyoff_pos) + " ~", topic, text);
        ShowCStr(dest, B(xstart + 2), B(y0 + 6), "ARP.TABLE: ~" + byte2hex(m.arpeggio_table) + " ~", topic, text);
        ShowCStr(dest, B(xstart + 2), B(y0 + 7), "ViB.TABLE: ~" + byte2hex(m.vibrato_table) + " ~", topic, text);

        VScrollBar(dest, B(xstart + window_xsize), B(ystart + 3), fmreg_scrlbar_size, fmreg_scrlbar_items, mn_environment.curr_pos,
                   WORD_NULL, macro_background + macro_border, macro_background + macro_border);
        VScrollBar(dest, B(xstart + window_xsize), B(y0 + 1), 8, 255 - 6, fmreg_page, WORD_NULL, macro_background + macro_border,
                   macro_background + macro_border);

        const int add = _fmreg_add_prev_size;
        _fmreg_macro_preview_refresh(B(xstart + 17), B(y0 + 1), B(fmreg_page + fmreg_vpos - 1));
        const std::string right = ExpStrL("", B(window_xsize - 49 - add - fmreg_cursor_pos - 1), '\xCD');
        ShowCStr(dest, B(xstart + 49 + add), B(y0 + 1), ExpStrL("", fmreg_cursor_pos, '\xCD') + "~\x1F~" + right,
                 macro_background + macro_topic2, macro_background + macro_hi_text);
        ShowCStr(dest, B(xstart + 49 + add), B(ystart + window_ysize - 2), ExpStrL("", fmreg_cursor_pos, '\xCD') + "~\x1E~" + right,
                 macro_background + macro_topic2, macro_background + macro_hi_text);
        ShowStr(dest, B(xstart + 2), B(ystart + window_ysize - 1),
                ExpStrR(macro_table_hint_str[20 + fmreg_hpos - 1], B(window_xsize - 2), ' '), macro_background + macro_hint);

        for (int idx = 1; idx <= 6; ++idx) {
            const uint8_t y = B(y0 + 1 + idx);
            if (idx == fmreg_vpos) {
                ShowStr(dest, B(xstart + 43 + add), y, "\x10" + byte2hex(B(fmreg_page + idx - 1)) + "\x11",
                        macro_background + macro_hi_text);
            } else {
                ShowStr(dest, B(xstart + 43 + add), y, " " + byte2hex(B(fmreg_page + idx - 1)) + " ", topic);
            }
            fmreg_page_refresh(B(xstart + 44 + add), y, B(fmreg_page + idx - 1));
        }
        a2w_lister_external_proc_callback();
    }

    // Menu の設定 (a2w_file_loader が窓を開く間だけ変える)
    struct MenuSettingsBackup
    {
        uint16_t terminate_keys[50];
        void (*ext_proc)();
        uint8_t topic_len;
        bool cycle_moves;
        tCHARSET topic_mask_chr;
        bool frame_enabled, shadow_enabled, winshade, center_box, show_scrollbar;
        uint8_t text_attr, text2_attr, short_attr, short2_attr, disbld_attr, contxt_attr, contxt2_attr, topic_attr, hi_topic_attr;

        MenuSettingsBackup()
        {
            std::memcpy(terminate_keys, mn_setting.terminate_keys, sizeof(terminate_keys));
            ext_proc = mn_environment.ext_proc;
            topic_len = mn_setting.topic_len;
            cycle_moves = mn_setting.cycle_moves;
            topic_mask_chr = mn_setting.topic_mask_chr;
            frame_enabled = mn_setting.frame_enabled;
            shadow_enabled = mn_setting.shadow_enabled;
            winshade = mn_environment.winshade;
            center_box = mn_setting.center_box;
            show_scrollbar = mn_setting.show_scrollbar;
            text_attr = mn_setting.text_attr;
            text2_attr = mn_setting.text2_attr;
            short_attr = mn_setting.short_attr;
            short2_attr = mn_setting.short2_attr;
            disbld_attr = mn_setting.disbld_attr;
            contxt_attr = mn_setting.contxt_attr;
            contxt2_attr = mn_setting.contxt2_attr;
            topic_attr = mn_setting.topic_attr;
            hi_topic_attr = mn_setting.hi_topic_attr;
        }

        void restore() const
        {
            std::memcpy(mn_setting.terminate_keys, terminate_keys, sizeof(terminate_keys));
            mn_environment.ext_proc = ext_proc;
            mn_setting.topic_len = topic_len;
            mn_setting.cycle_moves = cycle_moves;
            mn_setting.topic_mask_chr = topic_mask_chr;
            mn_setting.frame_enabled = frame_enabled;
            mn_setting.shadow_enabled = shadow_enabled;
            mn_environment.winshade = winshade;
            mn_setting.center_box = center_box;
            mn_setting.show_scrollbar = show_scrollbar;
            mn_setting.text_attr = text_attr;
            mn_setting.text2_attr = text2_attr;
            mn_setting.short_attr = short_attr;
            mn_setting.short2_attr = short2_attr;
            mn_setting.disbld_attr = disbld_attr;
            mn_setting.contxt_attr = contxt_attr;
            mn_setting.contxt2_attr = contxt2_attr;
            mn_setting.topic_attr = topic_attr;
            mn_setting.hi_topic_attr = hi_topic_attr;
        }
    };

    // 窓の Menu の共通の設定 (枠と影は窓の側で描く)
    void set_menu_settings(void (*ext_proc)(), uint8_t topic_len)
    {
        static const uint16_t new_keys[5] = { kESC, kENTER, kF1, kTAB, kCtENTR };
        std::memcpy(mn_setting.terminate_keys, new_keys, sizeof(new_keys));
        mn_environment.ext_proc = ext_proc;
        mn_setting.topic_len = topic_len;
        mn_setting.cycle_moves = false;
        mn_setting.frame_enabled = false;
        mn_setting.shadow_enabled = false;
        mn_environment.winshade = false;
        mn_setting.center_box = false;
        mn_setting.show_scrollbar = false;
        mn_environment.unpolite = false;
        mn_environment.preview = true;
        mn_environment.v_dest = temp_screen;
        mn_setting.text_attr = macro_background + macro_item;
        mn_setting.text2_attr = macro_sel_itm_bck + macro_sel_itm;
        mn_setting.short_attr = macro_background + macro_short;
        mn_setting.short2_attr = macro_sel_itm_bck + macro_sel_short;
        mn_setting.disbld_attr = macro_background + macro_item_dis;
        mn_setting.contxt_attr = macro_background + macro_context;
        mn_setting.contxt2_attr = macro_background + macro_context_dis;
        mn_setting.topic_attr = macro_background + macro_topic2;
        mn_setting.hi_topic_attr = macro_background + macro_hi_topic;
    }

    // 一度 temp_screen に描いてから画面へ出し (preview)、もう一度 Menu を開いて操作する
    uint16_t run_menu(std::vector<std::string>& data, uint16_t spos, uint8_t len, uint8_t len2, uint16_t count,
                      const std::string& title)
    {
        Menu(data, xstart, ystart, spos, len, len2, count, title);

        move_to_screen_data = temp_screen;
        move_to_screen_area[1] = xstart;
        move_to_screen_area[2] = ystart;
        move_to_screen_area[3] = B(xstart + window_xsize + 2 + 1);
        move_to_screen_area[4] = B(ystart + window_ysize + 1);
        move2screen_alt();

        mn_environment.unpolite = false;
        mn_environment.preview = false;
        mn_environment.v_dest = screen_ptr;
        centered_frame_vdest = mn_environment.v_dest;

        keyboard_reset_buffer();
        uint16_t index = 0;
        if (!_force_program_quit) index = Menu(data, xstart, ystart, spos, len, len2, count, title);
        return index;
    }

    void restore_screen_area()
    {
        move_to_screen_data = screen_backup;
        move_to_screen_area[1] = xstart;
        move_to_screen_area[2] = ystart;
        move_to_screen_area[3] = B(xstart + window_xsize + 2 + 1);
        move_to_screen_area[4] = B(ystart + window_ysize + 1);
        move2screen();
    }

    // 楽器 dst の、表の番号 table (temp_songdata の表) を曲の空いている表へ写す。写せなければ temp_str に名前を入れる
    void put_related_tables(uint8_t dst, std::string& temp_str, bool keep_first_error)
    {
        FixedSongData& sd = songdata();
        FixedSongData& ts = temp_songdata();
        RegisterTable& im = at1(sd.instr_macros, dst);
        int idx1 = -1, idx2 = -1;
        if (im.arpeggio_table != 0) idx1 = get_free_arpeggio_table_idx(mt_at(ts, im.arpeggio_table).arpeggio);
        if (im.vibrato_table != 0) idx2 = get_free_vibrato_table_idx(mt_at(ts, im.vibrato_table).vibrato);

        if (!keep_first_error || temp_str.empty()) {
            if (idx1 == 0) temp_str = idx2 == 0 ? "~ARPEGGiO/ViBRATO" : "~ARPEGGiO";
            else if (idx2 == 0) temp_str = "~ViBRATO";
            else if (!keep_first_error) temp_str.clear();
        }
        // 楽器 1 つを写すときは、どちらかの表が写せなければどちらも写さない
        if (!keep_first_error && !temp_str.empty()) return;

        if (idx1 > 0) {
            mt_at(sd, B(idx1)).arpeggio = mt_at(ts, im.arpeggio_table).arpeggio;
            im.arpeggio_table = B(idx1);
        }
        if (idx2 > 0) {
            mt_at(sd, B(idx2)).vibrato = mt_at(ts, im.vibrato_table).vibrato;
            im.vibrato_table = B(idx2);
        }
    }

    void tables_not_loaded_dialog(const std::string& temp_str)
    {
        Dialog("RELATED " + temp_str + " DATA~ WAS NOT LOADED!$FREE SOME SPACE iN MACRO TABLES AND ~REPEAT THiS ACTiON~$",
               "~O~K$", " A2W LOADER ", 1);
    }

    // バンクの楽器 src を曲の楽器 dst へ (FM データ・マクロ・無効な列・名前と、使っている表)
    void put_instrument(uint8_t dst, uint8_t src)
    {
        std::string temp_str;
        {
            PlayerLock lock;
            FixedSongData& sd = songdata();
            FixedSongData& ts = temp_songdata();
            at1(sd.instr_data, dst) = at1(ts.instr_data, src);
            at1(sd.instr_macros, dst) = at1(ts.instr_macros, src);
            std::memcpy(at1(sd.dis_fmreg_col, dst), at1(ts.dis_fmreg_col, src), sizeof(DisFmregCol));
            auto& name = at1(sd.instr_names, dst);
            name.assign(Copy(name.str(), 1, 9) + Copy(at1(ts.instr_names, src).str(), 10, 32));
            put_related_tables(dst, temp_str, false);
        }
        if (!temp_str.empty()) tables_not_loaded_dialog(temp_str);
    }
}   // namespace a2wload

// A2W の読み込み (a2w_file_loader)。取り込んだら 1、取り込まなければ BYTE_NULL を返す (オリジナルの load_flag)。
//   loadFromFile: instdata_source の A2W を読み込む (FALSE なら曲のデータを見る。MACRO_BROWSER)
//   loadMacros: アルペジオ / ビブラートのマクロの窓から始める (FALSE なら楽器マクロの窓)
//   bankSelector: 窓を出す (FALSE なら窓を出さずにバンク全体を読み込む)
//   loadBankPossible: 窓で Ctrl+Enter を押すとバンク全体を読み込む
//   updateCurInstr: 楽器マクロの窓で選んだ楽器を現在の楽器にする (TAB でアルペジオ / ビブラートの窓へ移れる)
uint8_t a2w_file_loader(bool loadFromFile, bool loadMacros, bool bankSelector, bool loadBankPossible, bool updateCurInstr)
{
    using namespace a2wload;
    FixedSongData& ts = temp_songdata();

    uint8_t load_flag = BYTE_NULL;
    uint8_t arpvib_arpeggio_table_bak, arpvib_vibrato_table_bak;
    {
        PlayerLock lock;
        arpvib_arpeggio_table_bak = arpvib_arpeggio_table;
        arpvib_vibrato_table_bak = arpvib_vibrato_table;
        songdata_bak() = songdata();
        ts = songdata_bak();
    }
    update_current_inst = updateCurInstr;
    bool browser_flag = false;

    // A2W ファイル (オリジナルの a2w_instdata_source。曲のデータを見るときは空)
    std::wstring a2w_instdata_source;
    if (!loadFromFile && bankSelector && !loadBankPossible) {
        PlayerLock lock;
        if (loadMacros) {
            const uint8_t mpos = macroedit::_macro_editor__pos[_arp_vib_mode ? 1 : 0];
            arp_tab_selected = mpos >= 8 && mpos <= 13;
            vib_tab_selected = mpos >= 14 && mpos <= 20;
        } else {
            arp_tab_selected = at1(songdata().instr_macros, current_inst).arpeggio_table != 0;
            vib_tab_selected = at1(songdata().instr_macros, current_inst).vibrato_table != 0;
        }
        // init 4OP flags (no file bank)
        ts.ins_4op_flags = songdata().ins_4op_flags;
        init_4op_flag_column();
    } else {
        a2w_instdata_source = instdata_source;
        std::wstring error;
        bool ok;
        {
            PlayerLock lock;
            ok = a2ins::LoadA2wBank(a2w_instdata_source, ts, error);
        }
        if (!ok) {
            MessageBoxW(g_hwnd, error.c_str(), L"AdLibTrackerModoki", MB_OK | MB_ICONWARNING);
            keyboard_init();
            return load_flag;
        }
        // init 4OP flags (file bank)
        PlayerLock lock;
        init_4op_flag_column();
    }

    const bool from_file = !a2w_instdata_source.empty();
    const std::string bank_key = from_file ? to_cp437(a2w_instdata_source) : std::string("?internal_instrument_data");
    const std::string bank_title = from_file ? iCASE(to_cp437(std::filesystem::path(a2w_instdata_source).filename().wstring()))
                                             : std::string();
    const bool narrow_mode =
        program_screen_mode == 0 || program_screen_mode == 3 || program_screen_mode == 4 || program_screen_mode == 5;

    enum class Step { Jmp1, Jmp1e, Jmp2, Jmp2e, End };
    Step step = loadMacros ? Step::Jmp1 : Step::Jmp2;
    uint16_t key = 0;
    uint16_t index = 0;
    uint8_t nm_valid = 0;

    for (;;) {
        if (step == Step::Jmp1) {
            // --- Arpeggio/Vibrato Macro Browser ---
            ScreenMemCopy(screen_ptr, screen_backup);
            {
                PlayerLock lock;
                nm_valid = count_macros();
            }
            if (nm_valid == 0) nm_valid = 1;

            if (!bankSelector) {
                key = kCtENTR;
                arp_tab_selected = true;
                vib_tab_selected = true;
                step = Step::Jmp1e;
                continue;
            }

            window_xsize = 73;
            if (is_default_screen_mode()) window_ysize = B(pmax(nm_valid + 5, 15) + 10);
            else window_ysize = B(pmax(nm_valid + 5, 20) + 10);
            std::string temp_str = from_file ? " " + bank_title + " \x10 A/V MACROS " : " ARPEGGiO/ViBRATO MACRO BROWSER ";
            temp_str += update_current_inst ? "(iNS_  ) " : "[iNS_  ] ";
            a2w_institle_pos = B(window_xsize / 2 + static_cast<int>(temp_str.size()) / 2 - 3);

            ScreenMemCopy(screen_ptr, temp_screen);
            centered_frame_vdest = temp_screen;
            centered_frame(xstart, ystart, window_xsize, window_ysize, temp_str, macro_background + macro_border,
                           macro_background + macro_title, frame_double);

            ShowStr(centered_frame_vdest, B(xstart + 1), B(ystart + window_ysize - 10 + 1), av_browser_str[5],
                    macro_background + macro_topic2);
            ShowStr(centered_frame_vdest, B(xstart + 1), B(ystart + window_ysize - 2), ExpStrR("", 72, '\xCD'),
                    macro_background + macro_topic2);

            context_str = " ~[~" + Num2str(nm_valid, 10) + "~/255]~";
            xstart_arp = B(xstart + 3);
            ystart_arp = B(ystart + window_ysize - 10 + 1);
            xstart_vib = B(xstart + 38);
            ystart_vib = B(ystart + window_ysize - 10 + 1);
            scrollbar_xstart = B(xstart + window_xsize);
            scrollbar_ystart = B(ystart + 1);
            scrollbar_size = B(window_ysize - 10 + 1);
            macro_table_size = nm_valid;

            arpeggio_table_pos = B(pmin(1, get_bank_position(bank_key + "?macro_av?arp_pos", nm_valid)));
            vibrato_table_pos = B(pmin(1, get_bank_position(bank_key + "?macro_av?vib_pos", nm_valid)));

            std::vector<std::string> a2w_queue_m(5 + nm_valid);
            for (int i = 0; i < 5; ++i) a2w_queue_m[i] = av_browser_str[i];
            {
                PlayerLock lock;
                const auto field = [](uint8_t value, const char* tail) {
                    return value > 0 ? " " + byte2hex(value) + tail : std::string(" \xF9\xF9") + tail;
                };
                for (int idx = 1; idx <= nm_valid; ++idx) {
                    const MacroTable& m = at1(ts.macro_table, idx);
                    std::string q = " ~" + byte2hex(B(idx)) + "~ \xBA";
                    if (is_empty_data(&m.arpeggio, sizeof(ArpeggioTable))) {
                        q += " \xFA\xFA  \xB3 \xFA\xFA  \xB3 \xFA\xFA  \xB3 \xFA\xFA  \xB3 \xFA\xFA   \xBA";
                    } else {
                        const ArpeggioTable& a = m.arpeggio;
                        q += field(a.length, "  \xB3") + field(a.speed, "  \xB3") + field(a.loop_begin, "  \xB3") +
                             field(a.loop_length, "  \xB3") + field(a.keyoff_pos, "   \xBA");
                    }
                    if (is_empty_data(&m.vibrato, sizeof(VibratoTable))) {
                        q += " \xFA\xFA  \xB3 \xFA\xFA  \xB3 \xFA\xFA  \xB3 \xFA\xFA  \xB3 \xFA\xFA  \xB3 \xFA\xFA";
                    } else {
                        const VibratoTable& v = m.vibrato;
                        q += field(v.length, "  \xB3") + field(v.speed, "  \xB3") + field(v.delay, "  \xB3") +
                             field(v.loop_begin, "  \xB3") + field(v.loop_length, "  \xB3");
                        // オリジナルはキーオフの位置がないとき、後ろの区切りを付けない
                        q += v.keyoff_pos > 0 ? " " + byte2hex(v.keyoff_pos) + "   \xBA" : std::string(" \xF9\xF9");
                    }
                    a2w_queue_m[4 + idx] = Copy(q, 1, 72);   // String[72]
                }
            }

            const MenuSettingsBackup backup;
            set_menu_settings(a2w_macro_lister_external_proc, 5);
            mn_setting.terminate_keys[3] = 0;                         // TAB possible only in FM-Register bank browser
            if (!loadBankPossible) mn_setting.terminate_keys[4] = 0;   // ^ENTER possible only in Arpeggio/Vibrato Macro Editor
            mn_setting.topic_mask_chr = CharSet("\xB3\xBA");

            const uint8_t ysize = is_default_screen_mode() ? 15 : 20;
            const uint16_t spos = static_cast<uint16_t>(pmin(1, get_bank_position(bank_key + "?macro_av?pos", nm_valid)));
            const uint8_t len2 = B(narrow_mode ? pmax(nm_valid + 5, ysize) : pmax(nm_valid + 5, 20));
            index = run_menu(a2w_queue_m, spos, 72, len2, static_cast<uint16_t>(nm_valid + 5), "");
            key = mn_environment.keystroke;

            add_bank_position(bank_key + "?macro_av?pos", nm_valid, index + 5);
            add_bank_position(bank_key + "?macro_av?arp_pos", nm_valid, arpeggio_table_pos);
            add_bank_position(bank_key + "?macro_av?vib_pos", nm_valid, vibrato_table_pos);

            backup.restore();
            restore_screen_area();

            if (key == kF1) {
                // ヘルプ (HELP) は 6f で実装する。開き直す
                if (!_force_program_quit) continue;
            }

            // 曲のデータを見ているだけ (MACRO_BROWSER) なら、Enter は取り込まずに閉じる。
            // オリジナルは A2W ファイルから読み込むとき (楽器を選ぶ窓ではないとき) も、Enter で閉じてしまう
            if (!update_current_inst && key == kENTER) {
                PlayerLock lock;
                songdata() = songdata_bak();
                ts = songdata_bak();
                key = kESC;
            }

            if (!loadMacros && key == kESC) {
                load_flag = BYTE_NULL;
                step = Step::Jmp2;
                continue;
            }
            step = Step::Jmp1e;
            continue;
        }

        if (step == Step::Jmp1e) {
            step = Step::End;
            if (key == kENTER || (loadBankPossible && key == kCtENTR)) {
                if (key == kENTER) {
                    std::string temp_str;
                    {
                        PlayerLock lock;
                        FixedSongData& sd = songdata();
                        const MacroTable& src = mt_at(ts, B(index));
                        if (loadMacros || _arp_vib_loader) {
                            arpvib_arpeggio_table = arpvib_arpeggio_table_bak;
                            arpvib_vibrato_table = arpvib_vibrato_table_bak;
                            if (arp_tab_selected) mt_at(sd, arpvib_arpeggio_table).arpeggio = src.arpeggio;
                            if (vib_tab_selected) mt_at(sd, arpvib_vibrato_table).vibrato = src.vibrato;
                        } else {
                            int idx1 = -1, idx2 = -1;
                            if (arp_tab_selected) idx1 = get_free_arpeggio_table_idx(src.arpeggio);
                            if (vib_tab_selected) idx2 = get_free_vibrato_table_idx(src.vibrato);

                            if (idx1 == 0) temp_str = idx2 == 0 ? "~ARPEGGiO/ViBRATO" : "~ARPEGGiO";
                            else if (idx2 == 0) temp_str = "~ViBRATO";

                            if (temp_str.empty()) {
                                RegisterTable& im = at1(sd.instr_macros, current_inst);
                                if (idx1 > 0) {
                                    mt_at(sd, B(idx1)).arpeggio = src.arpeggio;
                                    im.arpeggio_table = B(idx1);
                                }
                                if (idx2 > 0) {
                                    mt_at(sd, B(idx2)).vibrato = src.vibrato;
                                    im.vibrato_table = B(idx2);
                                }
                            }
                        }
                    }
                    if (!temp_str.empty()) {
                        tables_not_loaded_dialog(temp_str);
                    } else {
                        load_flag = 1;
                    }
                } else {
                    std::string temp_str;
                    if (arp_tab_selected) temp_str = "ARPEGGiO";
                    if (vib_tab_selected) temp_str = !temp_str.empty() ? temp_str + "/ViBRATO" : "ViBRATO";
                    if (!(!arp_tab_selected && !vib_tab_selected) || nm_valid < 2) {
                        uint8_t answer;
                        if (bankSelector) {
                            answer = Dialog("ALL UNSAVED " + temp_str + " MACRO DATA WiLL BE LOST$DO YOU WiSH TO CONTiNUE?$",
                                            "~Y~UP$~N~OPE$", " A2W LOADER ", 1);
                        } else {
                            answer = 1;
                            dl_environment.keystroke = kENTER;
                        }
                        if (dl_environment.keystroke != kESC && answer == 1) {
                            PlayerLock lock;
                            FixedSongData& sd = songdata();
                            for (int idx = 1; idx <= 255; ++idx) {
                                MacroTable& d = at1(sd.macro_table, idx);
                                if (!(idx > nm_valid)) {
                                    if (arp_tab_selected) d.arpeggio = at1(ts.macro_table, idx).arpeggio;
                                    if (vib_tab_selected) d.vibrato = at1(ts.macro_table, idx).vibrato;
                                } else {
                                    std::memset(&d.arpeggio, 0, sizeof(d.arpeggio));
                                    std::memset(&d.vibrato, 0, sizeof(d.vibrato));
                                }
                            }
                            load_flag = 1;
                        }
                    }
                }
            }
            continue;
        }

        if (step == Step::Jmp2) {
            // --- Instrument Macro Browser ---
            browser_flag = false;
            ScreenMemCopy(screen_ptr, screen_backup);

            {
                PlayerLock lock;
                nm_valid = count_instruments_temp();
            }
            if (nm_valid == 0) nm_valid = 1;

            if (!bankSelector) {
                key = kCtENTR;
                step = Step::Jmp2e;
                continue;
            }

            context_str.clear();
            context_str2.clear();
            if (!from_file) {
                nm_valid = 255;
            } else {
                context_str = " ~[~" + Num2str(nm_valid, 10) + "~/255]~";
                context_str2 = !loadBankPossible ? " ~[BANK]~" : " [BANK]";
            }

            std::vector<std::string> a2w_queue(3 + nm_valid), a2w_queue_more(3 + nm_valid), a2w_queue_more2(3 + nm_valid);
            for (int i = 0; i < 3; ++i) {
                a2w_queue[i] = a2w_header_str[i];
                a2w_queue_more[i] = a2w_header_hires1_str[i];
                a2w_queue_more2[i] = a2w_header_hires2_str[i];
            }

            {
                PlayerLock lock;
                for (int idx = 1; idx <= nm_valid; ++idx) {
                    const std::string name = at1(ts.instr_names, idx).str();
                    const Instrument& ins = at1(ts.instr_data, idx);
                    const RegisterTable& im = at1(ts.instr_macros, idx);
                    std::string q = Copy("~ " + ExpStrR(Copy(name, 2, 8) + "~" + Copy(name, 10, 32), 44, ' '), 1, 72);
                    const FmInstData& fm = ins.fm_data;
                    std::string qm = Copy(q + byte2hex(fm.AM_VIB_EG_carrier) + byte2hex(fm.AM_VIB_EG_modulator) + " " +
                                              byte2hex(fm.KSL_VOLUM_carrier) + byte2hex(fm.KSL_VOLUM_modulator) + " " +
                                              byte2hex(fm.ATTCK_DEC_carrier) + byte2hex(fm.ATTCK_DEC_modulator) + " " +
                                              byte2hex(fm.SUSTN_REL_carrier) + byte2hex(fm.SUSTN_REL_modulator) + " " +
                                              byte2hex(fm.WAVEFORM_carrier) + byte2hex(fm.WAVEFORM_modulator) + " " +
                                              byte2hex(fm.FEEDBACK_FM) + "   ",
                                          1, 102);

                    std::string temp_str = ins.panning <= 2 ? std::string(ins_pan_str2[ins.panning]) : ExpStrL("", 7, '\xFA');
                    std::string qm2 = Copy(qm + temp_str + "   ", 1, 121);
                    if (ins.fine_tune > 0) temp_str = "+" + ExpStrR(Num2str(ins.fine_tune, 16), 5, ' ');
                    else if (ins.fine_tune < 0) temp_str = "-" + ExpStrR(Num2str(0 - ins.fine_tune, 16), 5, ' ');
                    else temp_str = ExpStrR("", 6, ' ');
                    qm2 = Copy(qm2 + temp_str + "   ", 1, 121);

                    temp_str = ins.perc_voice <= 5 ? std::string(perc_voice_str[ins.perc_voice]) : ExpStrL("", 7, ' ');
                    q = Copy(q + temp_str, 1, 72);
                    qm = Copy(qm + temp_str, 1, 102);
                    qm2 = Copy(qm2 + temp_str, 1, 121);

                    temp_str = im.length != 0 ? " MACRO:FM" : " ";
                    if (im.arpeggio_table != 0) temp_str += temp_str != " " ? "+ARP" : "MACRO:ARP";
                    if (im.vibrato_table != 0) temp_str += temp_str != " " ? "+ViB" : "MACRO:ViB";

                    a2w_queue[2 + idx] = Copy(q + "  " + ExpStrR(temp_str, 18, ' '), 1, 72);
                    a2w_queue_more[2 + idx] = Copy(qm + "  " + ExpStrR(temp_str, 18, ' '), 1, 102);
                    a2w_queue_more2[2 + idx] = Copy(qm2 + "  " + ExpStrR(temp_str, 18, ' '), 1, 121);
                }
            }

            uint16_t idx = static_cast<uint16_t>(pmin(1, get_bank_position(bank_key + "?macro?pos", nm_valid)));
            fmreg_page = B(pmin(1, get_bank_position(bank_key + "?macro?fmreg_page", nm_valid)));
            fmreg_hpos = B(pmin(1, get_bank_position(bank_key + "?macro?fmreg_hpos", nm_valid)));
            fmreg_vpos = B(pmin(1, get_bank_position(bank_key + "?macro?fmreg_vpos", nm_valid)));
            fmreg_left_margin = B(pmin(1, get_bank_position(bank_key + "?macro?fmreg_left_margin", nm_valid)));
            fmreg_cursor_pos = B(pmin(1, get_bank_position(bank_key + "?macro?fmreg_cursor_pos", nm_valid)));

            std::string temp_str;
            if (from_file) {
                temp_str = " " + bank_title + " ";
            } else {
                if (updateCurInstr) add_bank_position("?internal_instrument_data?pos", 255, current_inst);
                temp_str = " iNSTRUMENT MACRO BROWSER ";
            }

            switch (program_screen_mode) {
            case 0:
            case 3:
                window_xsize = 73;
                if (is_default_screen_mode()) window_ysize = B(pmax(nm_valid + 3, 15) + 10);
                else window_ysize = B(pmax(nm_valid + 3, 20) + 10);
                break;
            case 4:
            case 5:
                window_xsize = 73;
                window_ysize = B(pmax(nm_valid + 3, 20) + 10);
                break;
            case 1:
                window_xsize = 103;
                window_ysize = B(pmax(nm_valid + 3, 20) + 10);
                break;
            case 2:
                window_xsize = 122;
                window_ysize = B(pmax(nm_valid + 3, 20) + 10);
                break;
            }

            xstart = B((work_maxcol - window_xsize) / 2);
            ystart = B((work_maxln - window_ysize) / 2 + 1);
            fmreg_scrlbar_items = nm_valid;
            fmreg_scrlbar_size = B(window_ysize - 10 - 2);
            _fmreg_add_prev_size = !narrow_mode ? 10 : 0;

            if (update_current_inst) {
                temp_str += "(iNS_  ) ";
                idx = current_inst;
            } else {
                temp_str += "[iNS_  ] ";
            }
            a2w_institle_pos = B(window_xsize / 2 + static_cast<int>(temp_str.size()) / 2 - 3);

            ScreenMemCopy(screen_ptr, temp_screen);
            centered_frame_vdest = temp_screen;
            centered_frame(xstart, ystart, window_xsize, window_ysize, temp_str, macro_background + macro_border,
                           macro_background + macro_title, frame_double);

            ShowStr(centered_frame_vdest, B(xstart + 1), B(ystart + window_ysize - 10 + 1), ExpStrR("", B(window_xsize - 1), '\xCD'),
                    macro_background + macro_topic2);
            ShowStr(centered_frame_vdest, B(xstart + 1), B(ystart + window_ysize - 2), ExpStrR("", B(window_xsize - 1), '\xCD'),
                    macro_background + macro_topic2);
            ShowVStr(centered_frame_vdest, B(xstart + 42 + _fmreg_add_prev_size), B(ystart + window_ysize - 10 + 1),
                     "\xCB\xBA\xBA\xBA\xBA\xBA\xBA\xCA", macro_background + macro_topic2);

            const MenuSettingsBackup backup;
            set_menu_settings(a2w_lister_external_proc, 3);
            if (!update_current_inst) mn_setting.terminate_keys[3] = 0;   // TAB not possible when called from Macro Browser
            if (!loadBankPossible) mn_setting.terminate_keys[4] = 0;      // ^ENTER possible only in Instrument Control

            const uint8_t ysize = is_default_screen_mode() ? 15 : 20;
            _a2w_lister_count = B(pmax(nm_valid + 3, ysize) - 3);

            const uint16_t spos = static_cast<uint16_t>(idx + 3);
            if (narrow_mode) {
                index = run_menu(a2w_queue, spos, 72, B(pmax(nm_valid + 3, ysize)), static_cast<uint16_t>(nm_valid + 3), temp_str);
            } else if (program_screen_mode == 1) {
                index = run_menu(a2w_queue_more, spos, 102, B(pmax(nm_valid + 3, 20)), static_cast<uint16_t>(nm_valid + 3), temp_str);
            } else {
                index = run_menu(a2w_queue_more2, spos, 121, B(pmax(nm_valid + 3, 20)), static_cast<uint16_t>(nm_valid + 3), temp_str);
            }
            key = mn_environment.keystroke;

            add_bank_position(bank_key + "?macro?pos", nm_valid, index);
            add_bank_position(bank_key + "?macro?fmreg_page", nm_valid, fmreg_page);
            add_bank_position(bank_key + "?macro?fmreg_hpos", nm_valid, fmreg_hpos);
            add_bank_position(bank_key + "?macro?fmreg_vpos", nm_valid, fmreg_vpos);
            add_bank_position(bank_key + "?macro?fmreg_left_margin", nm_valid, fmreg_left_margin);
            add_bank_position(bank_key + "?macro?fmreg_cursor_pos", nm_valid, fmreg_cursor_pos);

            backup.restore();
            restore_screen_area();
            centered_frame_vdest = screen_ptr;

            if (key == kESC) {
                step = Step::End;
                continue;
            }

            if (key == kTAB) {
                {
                    PlayerLock lock;
                    arp_tab_selected = at1(songdata().instr_macros, current_inst).arpeggio_table != 0;
                    vib_tab_selected = at1(songdata().instr_macros, current_inst).vibrato_table != 0;
                }
                loadMacros = false;
                browser_flag = true;
                step = Step::Jmp1;
                continue;
            }

            if (key == kF1) {
                // ヘルプ (HELP) は 6f で実装する。開き直す
                if (!_force_program_quit) continue;
            }
            step = Step::Jmp2e;
            continue;
        }

        if (step == Step::Jmp2e) {
            step = Step::End;
            if (key == kENTER || (loadBankPossible && key == kCtENTR)) {
                if (key == kENTER) {
                    const bool alternate = shift_pressed();
                    bool _4op_ins_flag;
                    {
                        PlayerLock lock;
                        _4op_ins_flag = select_4op_destination(B(index), alternate);
                    }
                    if (_4op_ins_flag) {
                        put_instrument(_4op_idx11, _4op_idx21);
                        put_instrument(_4op_idx12, _4op_idx22);
                        if (!alternate) {
                            PlayerLock lock;
                            a2ins::set_4op_flag(songdata(), _4op_idx11);
                        }
                    } else {
                        // put 2op instrument
                        put_instrument(current_inst, B(index));
                    }
                    load_flag = 1;
                } else {
                    uint8_t answer;
                    if (bankSelector) {
                        answer = Dialog("ALL UNSAVED iNSTRUMENT AND MACRO DATA WiLL BE LOST$DO YOU WiSH TO CONTiNUE?$",
                                        "~Y~UP$~N~OPE$", " A2W LOADER ", 1);
                    } else {
                        answer = 1;
                        dl_environment.keystroke = kENTER;
                    }

                    if (dl_environment.keystroke != kESC && answer == 1) {
                        std::string temp_str;
                        {
                            PlayerLock lock;
                            FixedSongData& sd = songdata();
                            for (int i = 1; i <= 255; ++i) {
                                auto& name = at1(sd.instr_names, i);
                                if (!(i > nm_valid)) {
                                    at1(sd.instr_data, i) = at1(ts.instr_data, i);
                                    at1(sd.instr_macros, i) = at1(ts.instr_macros, i);
                                    std::memcpy(at1(sd.dis_fmreg_col, i), at1(ts.dis_fmreg_col, i), sizeof(DisFmregCol));
                                    name.assign(Copy(name.str(), 1, 9) + Copy(at1(ts.instr_names, i).str(), 10, 32));
                                    put_related_tables(B(i), temp_str, true);
                                } else {
                                    std::memset(&at1(sd.instr_data, i), 0, sizeof(Instrument));
                                    std::memset(&at1(sd.instr_macros, i), 0, sizeof(RegisterTable));
                                    std::memset(at1(sd.dis_fmreg_col, i), 0, sizeof(DisFmregCol));
                                    name.assign(Copy(name.str(), 1, 9));
                                }
                            }
                            sd.ins_4op_flags = ts.ins_4op_flags;
                        }
                        if (!temp_str.empty()) tables_not_loaded_dialog(temp_str);
                        load_flag = 1;
                    }
                }
            }
            continue;
        }

        // _end
        if (browser_flag) {
            step = Step::Jmp2;
            continue;
        }
        break;
    }

    PlayerLock lock;
    arpvib_arpeggio_table = arpvib_arpeggio_table_bak;
    arpvib_vibrato_table = arpvib_vibrato_table_bak;
    return load_flag;
}

// Ctrl+M: マクロブラウザ (曲の楽器マクロ・アルペジオ / ビブラートのマクロを見る)
//   instrBrowser: 楽器マクロの窓から始める (FALSE ならアルペジオ / ビブラートのマクロの窓)
//   updateCurInstr: 選んだ楽器を現在の楽器にする
void MACRO_BROWSER(bool instrBrowser, bool updateCurInstr)
{
    {
        PlayerLock lock;
        songdata_crc = Update32(&songdata(), sizeof(FixedSongData), 0);
    }
    a2w_file_loader(false, !instrBrowser, true, false, updateCurInstr);   // browse internal A2W data
    PlayerLock lock;
    if (Update32(&songdata(), sizeof(FixedSongData), 0) != songdata_crc) module_archived = false;
}

// 曲 (A2P を含む) を読み込んだときの初期化 (adt2extn.pas の FILE_open の load_flag = 1 の場合)。ロックを取った状態で呼ぶ。
// オリジナルはまだない画面 (置換・曲の設定・メッセージボードなど) の位置もここで戻すため、同じ名前で戻しておく
void reset_positions_on_load()
{
    using namespace macroedit;
    std::fill(std::begin(P().channel_flag), std::end(P().channel_flag), true);
    track_notes = false;
    track_chan_start = 1;
    nm_track_chan = 1;
    remap_mtype = 1;
    remap_ins1 = 1;
    remap_ins2 = 1;
    remap_selection = 1;
    reset_replace_settings();
    current_inst = 1;
    pattern_list__page = 1;
    _macro_editor__pos[0] = 1;
    _macro_editor__pos[1] = 8;
    for (int av = 0; av <= 1; ++av) {
        _macro_editor__fmreg_hpos[av] = 1;
        _macro_editor__fmreg_page[av] = 1;
        _macro_editor__fmreg_left_margin[av] = 1;
        _macro_editor__fmreg_cursor_pos[av] = 1;
        _macro_editor__arpeggio_page[av] = 1;
        _macro_editor__vibrato_hpos[av] = 1;
        _macro_editor__vibrato_page[av] = 1;
    }
    for (const char* name : { "?macro?pos", "?macro?fmreg_page", "?macro?fmreg_hpos", "?macro?fmreg_vpos",
                              "?macro?fmreg_left_margin", "?macro?fmreg_cursor_pos", "?macro_av?pos", "?macro_av?arp_pos",
                              "?macro_av?vib_pos" }) {
        add_bank_position(std::string("?internal_instrument_data") + name, -1, 1);
    }
    add_bank_position("?song_variables_window?pos", -1, 1);
    add_bank_position("?song_variables_window?pos_4op", -1, 0);
    add_bank_position("?replace_window?pos", -1, 1);
    add_bank_position("?replace_window?posfx", -1, 1);
    add_bank_position("?message_board?hpos", -1, 1);
    add_bank_position("?message_board?vpos", -1, 1);
    for (int index = 1; index <= 255; ++index) {
        const std::string key = "?instrument_editor?" + byte2hex(B(index));
        add_bank_position(key + "?carrier", -1, 1);
        add_bank_position(key + "?carrier?hpos", -1, 1);
        add_bank_position(key + "?carrier?vpos", -1, 1);
        add_bank_position(key + "?modulator?hpos", -1, 1);
        add_bank_position(key + "?modulator?vpos", -1, 1);
    }
}
